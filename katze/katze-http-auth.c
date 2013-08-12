/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "katze-http-auth.h"
#include "gtk3-compat.h"

#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

struct _KatzeHttpAuth
{
    GObject parent_instance;
    gchar* filename;
    GHashTable* logins;
};

struct _KatzeHttpAuthClass
{
    GObjectClass parent_class;
};

typedef struct
{
    KatzeHttpAuth* http_auth;
    SoupAuth* auth;
    gchar* username;
    gchar* password;
} KatzeHttpAuthSave;

typedef struct
{
    gchar* username;
    gchar* password;
} KatzeHttpAuthLogin;

static void
katze_http_auth_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                            gpointer                     data);

G_DEFINE_TYPE_WITH_CODE (KatzeHttpAuth, katze_http_auth, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
                         katze_http_auth_session_feature_iface_init));

enum
{
    PROP_0,

    PROP_FILENAME
};

static void
katze_http_auth_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec);

static void
katze_http_auth_get_property (GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec);

static void
katze_http_auth_finalize (GObject* object);

static gchar*
katze_http_auth_soup_auth_get_hash (SoupAuth* auth)
{
    return g_strdup_printf ("%s:%s:%s",
        soup_auth_get_host (auth),
        soup_auth_get_scheme_name (auth),
        soup_auth_get_realm (auth));
}

static void
authentication_message_got_headers_cb (SoupMessage*       msg,
                                       KatzeHttpAuthSave* save)
{
    /* Anything but 401 and 5xx means the password was accepted */
    if (msg->status_code != 401 && msg->status_code < 500)
    {
        gchar* opaque_info;
        FILE* file;

        opaque_info = katze_http_auth_soup_auth_get_hash (save->auth);

        if (!g_hash_table_lookup (save->http_auth->logins, opaque_info))
        {
            KatzeHttpAuthLogin* login = g_slice_new (KatzeHttpAuthLogin);
            login->username = save->username;
            login->password = save->password;
            g_hash_table_insert (save->http_auth->logins, opaque_info, login);

            if ((file = g_fopen (save->http_auth->filename, "a")))
            {
                fprintf (file, "%s\t%s\t%s\n", opaque_info,
                         login->username, login->password);
                fclose (file);
                g_chmod (save->http_auth->filename, 0600);
            }
        }
        else
        {
            /* FIXME g_free (save->username);
            g_free (save->password); */
        }
    }
    else
    {
        /* FIXME g_free (save->username);
        g_free (save->password); */
    }

    /* FIXME g_object_unref (save->auth); */
    /* FIXME g_slice_free (KatzeHttpAuthSave, save); */
    g_signal_handlers_disconnect_by_func (msg,
        authentication_message_got_headers_cb, save);
}

static void
authentication_dialog_response_cb (GtkWidget*         dialog,
                                   gint               response,
                                   KatzeHttpAuthSave* save)
{
    SoupSession* session;
    SoupMessage* msg;

    msg = g_object_get_data (G_OBJECT (dialog), "msg");

    if (response == GTK_RESPONSE_OK)
    {
        GtkEntry* username = g_object_get_data (G_OBJECT (dialog), "username");
        GtkEntry* password = g_object_get_data (G_OBJECT (dialog), "password");
        GtkToggleButton* remember = g_object_get_data (G_OBJECT (dialog), "remember");

        soup_auth_authenticate (save->auth,
            gtk_entry_get_text (username), gtk_entry_get_text (password));

        if (gtk_toggle_button_get_active (remember) && save->http_auth->filename)
        {
            save->username = g_strdup (gtk_entry_get_text (username));
            save->password = g_strdup (gtk_entry_get_text (password));
            g_signal_connect (msg, "got-headers",
                G_CALLBACK (authentication_message_got_headers_cb), save);
        }
        else
        {
            g_object_unref (save->auth);
            g_slice_free (KatzeHttpAuthSave, save);
        }
    }

    session = g_object_get_data (G_OBJECT (dialog), "session");
    if (g_object_get_data (G_OBJECT (msg), "paused"))
        soup_session_unpause_message (session, msg);
    gtk_widget_destroy (dialog);
    g_object_unref (msg);
}

static void
katze_http_auth_session_authenticate_cb (SoupSession*   session,
                                         SoupMessage*   msg,
                                         SoupAuth*      auth,
                                         gboolean       retrying,
                                         KatzeHttpAuth* http_auth)
{
    gchar* opaque_info;
    KatzeHttpAuthLogin* login;
    GtkWidget* dialog;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* image;
    GtkWidget* label;
    GtkWidget* align;
    GtkWidget* entry;
    KatzeHttpAuthSave* save;

    /* We want to ask for authentication exactly once, so we
       enforce this with a tag. There might be a better way. */
    if (!retrying && g_object_get_data (G_OBJECT (msg), "katze-session-tag"))
        return;

    if (1)
    {
        /* We use another tag to indicate whether a message is paused.
           There doesn't seem to be API in libSoup to find that out. */
        soup_session_pause_message (session, g_object_ref (msg));
        g_object_set_data (G_OBJECT (msg), "paused", (void*)1);
    }
    g_object_set_data (G_OBJECT (msg), "katze-session-tag", (void*)1);

    opaque_info = katze_http_auth_soup_auth_get_hash (auth);
    login = g_hash_table_lookup (http_auth->logins, opaque_info);
    g_free (opaque_info);

    dialog = gtk_dialog_new_with_buttons (_("Authentication Required"),
        NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OK, GTK_RESPONSE_OK,
        NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog),
        GTK_STOCK_DIALOG_AUTHENTICATION);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 5);

    gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 5);
    hbox = gtk_hbox_new (FALSE, 6);
    image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
                                      GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    label = gtk_label_new (_("A username and a password are required\n"
                             "to open this location:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, TRUE, 0);
    label = gtk_label_new (soup_auth_get_host (auth));
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), label, FALSE, TRUE, 0);
    /* If the realm is merely the host, omit the realm label */
    if (g_strcmp0 (soup_auth_get_host (auth), soup_auth_get_realm (auth)))
    {
        label = gtk_label_new (soup_auth_get_realm (auth));
        gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), label, FALSE, TRUE, 0);
    }
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    hbox = gtk_hbox_new (FALSE, 6);
    label = gtk_label_new (_("Username"));
    align = gtk_alignment_new (0, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER (align), label);
    gtk_size_group_add_widget (sizegroup, align);
    gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
    entry = gtk_entry_new ();
    if (login)
        gtk_entry_set_text (GTK_ENTRY (entry), login->username);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    g_object_set_data (G_OBJECT (dialog), "username", entry);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, TRUE, 0);
    hbox = gtk_hbox_new (FALSE, 6);
    label = gtk_label_new (_("Password"));
    align = gtk_alignment_new (0, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER (align), label);
    gtk_size_group_add_widget (sizegroup, align);
    gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
    entry = gtk_entry_new ();
    if (login)
        gtk_entry_set_text (GTK_ENTRY (entry), login->password);
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    g_object_set_data (G_OBJECT (dialog), "password", entry);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, TRUE, 0);
    hbox = gtk_hbox_new (FALSE, 6);
    label = gtk_check_button_new_with_mnemonic (_("_Remember password"));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    g_object_set_data (G_OBJECT (dialog), "remember", label);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label), (login != NULL));
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, TRUE, 0);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));

    g_object_set_data (G_OBJECT (dialog), "session", session);
    g_object_set_data (G_OBJECT (dialog), "msg", msg);

    save = g_slice_new0 (KatzeHttpAuthSave);
    save->http_auth = http_auth;
    save->auth = g_object_ref (auth);
    g_signal_connect (dialog, "response",
        G_CALLBACK (authentication_dialog_response_cb), save);
    gtk_widget_show (dialog);
}

static void
katze_http_auth_session_request_queued_cb (SoupSession*   session,
                                           SoupMessage*   msg,
                                           KatzeHttpAuth* http_auth)
{
    /* WebKit has its own authentication dialog in recent versions.
       We want only one, and we choose our own to have localization. */
    GType type = g_type_from_name ("WebKitSoupAuthDialog");
    if (type)
        soup_session_remove_feature_by_type (session, type);

    g_signal_connect (session, "authenticate",
        G_CALLBACK (katze_http_auth_session_authenticate_cb), http_auth);
    g_signal_handlers_disconnect_by_func (session,
        katze_http_auth_session_request_queued_cb, http_auth);
}

static void
katze_http_auth_attach (SoupSessionFeature* feature,
                        SoupSession*        session)
{
    g_signal_connect (session, "request-queued",
        G_CALLBACK (katze_http_auth_session_request_queued_cb), feature);
}

static void
katze_http_auth_detach (SoupSessionFeature* feature,
                        SoupSession*        session)
{
    g_signal_handlers_disconnect_by_func (session,
        katze_http_auth_session_authenticate_cb, NULL);
    g_signal_handlers_disconnect_by_func (session,
        katze_http_auth_session_request_queued_cb, NULL);
}

static void
katze_http_auth_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                            gpointer                     data)
{
    iface->attach = katze_http_auth_attach;
    iface->detach = katze_http_auth_detach;
}

static void
katze_http_auth_class_init (KatzeHttpAuthClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_http_auth_finalize;
    gobject_class->set_property = katze_http_auth_set_property;
    gobject_class->get_property = katze_http_auth_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    /**
     * KatzeHttpAuth:filename:
     *
     * An absolute path and name of a file for storing logins.
     *
     * Since: 0.1.10
     */
    g_object_class_install_property (gobject_class,
                                     PROP_FILENAME,
                                     g_param_spec_string (
                                     "filename",
                                     "Filename",
                                     "An absolute path and name of a file for storing logins",
                                     NULL,
                                     flags));
}

static void
katze_http_auth_login_free (KatzeHttpAuthLogin* login)
{
    g_free (login->username);
    g_free (login->password);
    g_slice_free (KatzeHttpAuthLogin, login);
}

static void
katze_http_auth_set_filename (KatzeHttpAuth* http_auth,
                              const gchar*   filename)
{
    FILE* file;

    katze_assign (http_auth->filename, g_strdup (filename));

    g_hash_table_remove_all (http_auth->logins);

    if ((file = g_fopen (filename, "r")))
    {
        gchar line[255];
        guint number = 0;

        while (fgets (line, 255, file))
        {
            gchar** parts = g_strsplit (line, "\t", 3);
            if (parts && parts[0] && parts[1] && parts[2])
            {
                gint length = strlen (parts[2]);
                KatzeHttpAuthLogin* login = g_slice_new (KatzeHttpAuthLogin);
                login->username = parts[1];
                if (parts[2][length - 1] == '\n')
                    length--;
                login->password = g_strndup (parts[2], length);
                g_hash_table_insert (http_auth->logins, parts[0], login);
                g_free (parts);
            }
            else
            {
                g_strfreev (parts);
                g_warning ("Error in line %d in HTTP Auth file", number);
            }
            number++;
        }
        fclose (file);
    }
}

static void
katze_http_auth_init (KatzeHttpAuth* http_auth)
{
    http_auth->filename = NULL;

    http_auth->logins = g_hash_table_new_full (g_str_hash, g_str_equal,
        (GDestroyNotify)g_free, (GDestroyNotify)katze_http_auth_login_free);
}

static void
katze_http_auth_finalize (GObject* object)
{
    KatzeHttpAuth* http_auth = KATZE_HTTP_AUTH (object);

    g_free (http_auth->filename);

    g_hash_table_unref (http_auth->logins);

    G_OBJECT_CLASS (katze_http_auth_parent_class)->finalize (object);
}

static void
katze_http_auth_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec)
{
    KatzeHttpAuth* http_auth = KATZE_HTTP_AUTH (object);

    switch (prop_id)
    {
    case PROP_FILENAME:
        katze_http_auth_set_filename (http_auth, g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
katze_http_auth_get_property (GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec)
{
    KatzeHttpAuth* http_auth = KATZE_HTTP_AUTH (object);

    switch (prop_id)
    {
    case PROP_FILENAME:
        g_value_set_string (value, http_auth->filename);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}
