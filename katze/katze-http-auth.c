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

#if HAVE_LIBSOUP
    #include <libsoup/soup.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct _KatzeHttpAuth
{
    GObject parent_instance;
};

struct _KatzeHttpAuthClass
{
    GObjectClass parent_class;
};

#if HAVE_LIBSOUP
static void
katze_http_auth_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                            gpointer                     data);

G_DEFINE_TYPE_WITH_CODE (KatzeHttpAuth, katze_http_auth, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
                         katze_http_auth_session_feature_iface_init));

static void
authentication_dialog_response_cb (GtkWidget* dialog,
                                   gint       response,
                                   SoupAuth*  auth)
{
    GtkWidget* username;
    GtkWidget* password;
    SoupSession* session;
    SoupMessage* msg;

    if (response == GTK_RESPONSE_OK)
    {

        username = g_object_get_data (G_OBJECT (dialog), "username");
        password = g_object_get_data (G_OBJECT (dialog), "password");

        soup_auth_authenticate (auth,
            gtk_entry_get_text (GTK_ENTRY (username)),
            gtk_entry_get_text (GTK_ENTRY (password)));
    }

    session = g_object_get_data (G_OBJECT (dialog), "session");
    msg = g_object_get_data (G_OBJECT (dialog), "msg");
    gtk_widget_destroy (dialog);
    if (g_object_get_data (G_OBJECT (msg), "paused"))
        soup_session_unpause_message (session, msg);
    g_object_unref (auth);
}

static void
katze_http_auth_session_authenticate_cb (SoupSession* session,
                                         SoupMessage* msg,
                                         SoupAuth*    auth,
                                         gboolean     retrying)
{
    GtkWidget* dialog;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* image;
    GtkWidget* label;
    GtkWidget* align;
    GtkWidget* entry;

    /* We want to ask for authentication exactly once, so we
       enforce this with a tag. There might be a better way. */
    if (!retrying && g_object_get_data (G_OBJECT (msg), "katze-session-tag"))
        return;

    if (soup_message_is_keepalive (msg))
    {
        /* We use another tag to indicate whether a message is paused.
           There doesn't seem to be API in libSoup to find that out. */
        soup_session_pause_message (session, msg);
        g_object_set_data (G_OBJECT (msg), "paused", (void*)1);
    }
    g_object_set_data (G_OBJECT (msg), "katze-session-tag", (void*)1);

    dialog = gtk_dialog_new_with_buttons (_("Authentication Required"),
        NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OK, GTK_RESPONSE_OK,
        NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog),
        GTK_STOCK_DIALOG_AUTHENTICATION);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 5);

    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 5);
    hbox = gtk_hbox_new (FALSE, 6);
    image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
                                      GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    label = gtk_label_new (_("A username and a password are required\n"
                             "to open this location:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    label = gtk_label_new (soup_auth_get_host (auth));
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);
    /* If the realm is merely the host, omit the realm label */
    if (g_strcmp0 (soup_auth_get_host (auth), soup_auth_get_realm (auth)))
    {
        label = gtk_label_new (soup_auth_get_realm (auth));
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);
    }
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    hbox = gtk_hbox_new (FALSE, 6);
    label = gtk_label_new (_("Username"));
    align = gtk_alignment_new (0, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER (align), label);
    gtk_size_group_add_widget (sizegroup, align);
    gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    g_object_set_data (G_OBJECT (dialog), "username", entry);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    hbox = gtk_hbox_new (FALSE, 6);
    label = gtk_label_new (_("Password"));
    align = gtk_alignment_new (0, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER (align), label);
    gtk_size_group_add_widget (sizegroup, align);
    gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
    entry = gtk_entry_new_with_max_length (32);
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    g_object_set_data (G_OBJECT (dialog), "password", entry);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    g_object_set_data (G_OBJECT (dialog), "session", session);
    g_object_set_data (G_OBJECT (dialog), "msg", msg);
    g_signal_connect (dialog, "response",
        G_CALLBACK (authentication_dialog_response_cb), g_object_ref (auth));
    gtk_widget_show (dialog);
}

static void
katze_http_auth_attach (SoupSessionFeature* feature,
                        SoupSession*        session)
{
    g_signal_connect (session, "authenticate",
        G_CALLBACK (katze_http_auth_session_authenticate_cb), NULL);
}

static void
katze_http_auth_detach (SoupSessionFeature* feature,
                        SoupSession*        session)
{
    g_signal_handlers_disconnect_by_func (session,
        katze_http_auth_session_authenticate_cb, NULL);
}

static void
katze_http_auth_session_feature_iface_init (SoupSessionFeatureInterface *iface,
                                            gpointer                     data)
{
    iface->attach = katze_http_auth_attach;
    iface->detach = katze_http_auth_detach;
}
#else
G_DEFINE_TYPE (KatzeHttpAuth, katze_http_auth, G_TYPE_OBJECT)
#endif

static void
katze_http_auth_class_init (KatzeHttpAuthClass* class)
{
    /* Nothing to do. */
}

static void
katze_http_auth_init (KatzeHttpAuth* http_auth)
{
    /* Nothing to do. */
}
