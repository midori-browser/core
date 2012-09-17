/*
 Copyright (C) 2007-2011 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Dale Whittaker <dayul@users.sf.net>
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "sokoke.h"

#include "midori-core.h"
#include "midori-platform.h"
#include "midori-app.h"

#include <config.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>

#ifdef GDK_WINDOWING_X11
    #include <gdk/gdkx.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <webkit/webkit.h>

#ifdef HAVE_HILDON_FM
    #include <hildon/hildon-file-chooser-dialog.h>
#endif

#if HAVE_HILDON
    #include <libosso.h>
    #include <hildon/hildon.h>
    #include <hildon-mime.h>
    #include <hildon-uri.h>
#endif

static gchar*
sokoke_js_string_utf8 (JSStringRef js_string)
{
    size_t size_utf8;
    gchar* string_utf8;

    g_return_val_if_fail (js_string, NULL);

    size_utf8 = JSStringGetMaximumUTF8CStringSize (js_string);
    string_utf8 = g_new (gchar, size_utf8);
    JSStringGetUTF8CString (js_string, string_utf8, size_utf8);
    return string_utf8;
}

gchar*
sokoke_js_script_eval (JSContextRef js_context,
                       const gchar* script,
                       gchar**      exception)
{
    JSGlobalContextRef temporary_context = NULL;
    gchar* value;
    JSStringRef js_value_string;
    JSStringRef js_script;
    JSValueRef js_exception = NULL;
    JSValueRef js_value;

    g_return_val_if_fail (script, FALSE);

    if (!js_context)
        js_context = temporary_context = JSGlobalContextCreateInGroup (NULL, NULL);

    js_script = JSStringCreateWithUTF8CString (script);
    js_value = JSEvaluateScript (js_context, js_script,
        JSContextGetGlobalObject (js_context), NULL, 0, &js_exception);
    JSStringRelease (js_script);

    if (!js_value)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        g_return_val_if_fail (js_message != NULL, NULL);

        value = sokoke_js_string_utf8 (js_message);
        if (exception)
            *exception = value;
        else
        {
            g_warning ("%s", value);
            g_free (value);
        }
        JSStringRelease (js_message);
        if (temporary_context)
            JSGlobalContextRelease (temporary_context);
        return NULL;
    }

    js_value_string = JSValueToStringCopy (js_context, js_value, NULL);
    value = sokoke_js_string_utf8 (js_value_string);
    JSStringRelease (js_value_string);
    if (temporary_context)
        JSGlobalContextRelease (temporary_context);
    return value;
}

static void
sokoke_message_dialog_response_cb (GtkWidget* dialog,
                                   gint       response,
                                   gpointer   data)
{
    gtk_widget_destroy (dialog);
}

void
sokoke_message_dialog (GtkMessageType message_type,
                       const gchar*   short_message,
                       const gchar*   detailed_message,
                       gboolean       modal)
{
    GtkWidget* dialog = gtk_message_dialog_new (
        NULL, 0, message_type,
        #if HAVE_HILDON
        GTK_BUTTONS_NONE,
        #else
        GTK_BUTTONS_OK,
        #endif
        "%s", short_message);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", detailed_message);
    if (modal)
    {
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }
    else
    {
        g_signal_connect (dialog, "response",
                          G_CALLBACK (sokoke_message_dialog_response_cb), NULL);
        gtk_widget_show (dialog);
    }

}

/**
 * sokoke_show_uri_with_mime_type:
 * @screen: a #GdkScreen, or %NULL
 * @uri: the URI to show
 * @mime_type: a MIME type
 * @timestamp: the timestamp of the event
 * @error: the location of a #GError, or %NULL
 *
 * Shows the specified URI with an appropriate application,
 * as though it had the specified MIME type.
 *
 * On Maemo, hildon_mime_open_file_with_mime_type() is used.
 *
 * See also: sokoke_show_uri().
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
sokoke_show_uri_with_mime_type (GdkScreen*   screen,
                                const gchar* uri,
                                const gchar* mime_type,
                                guint32      timestamp,
                                GError**     error)
{
    gboolean success;
    #if HAVE_HILDON
    osso_context_t* osso;
    DBusConnection* dbus;

    osso = osso_initialize (PACKAGE_NAME, PACKAGE_VERSION, FALSE, NULL);
    if (!osso)
    {
        g_print ("Failed to initialize libosso\n");
        return FALSE;
    }

    dbus = (DBusConnection *) osso_get_dbus_connection (osso);
    if (!dbus)
    {
        osso_deinitialize (osso);
        g_print ("Failed to get dbus connection from osso context\n");
        return FALSE;
    }

    success = (hildon_mime_open_file_with_mime_type (dbus,
               uri, mime_type) == 1);
    osso_deinitialize (osso);
    #else
    GFile* file = g_file_new_for_uri (uri);
    gchar* content_type;
    GAppInfo* app_info;
    GList* files;
    gpointer context;

    content_type = g_content_type_from_mime_type (mime_type);
    app_info = g_app_info_get_default_for_type (content_type,
        !g_str_has_prefix (uri, "file://"));
    g_free (content_type);
    files = g_list_prepend (NULL, file);
    #if GTK_CHECK_VERSION (3, 0, 0)
    context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
    #else
    context = gdk_app_launch_context_new ();
    #endif
    gdk_app_launch_context_set_screen (context, screen);
    gdk_app_launch_context_set_timestamp (context, timestamp);

    success = g_app_info_launch (app_info, files, context, error);

    g_object_unref (app_info);
    g_list_free (files);
    g_object_unref (file);
    #endif

    return success;
}

static void
sokoke_open_with_response_cb (GtkWidget* dialog,
                              gint       response,
                              GtkEntry*  entry)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        const gchar* command = gtk_entry_get_text (entry);
        const gchar* uri = g_object_get_data (G_OBJECT (dialog), "uri");
        sokoke_spawn_program (command, FALSE, uri, TRUE);
    }
    gtk_widget_destroy (dialog);
}

static GAppInfo*
sokoke_default_for_uri (const gchar* uri,
                        gchar**      scheme_ptr)
{
    gchar* scheme;
    GAppInfo* info;

    scheme = g_uri_parse_scheme (uri);
    if (scheme_ptr != NULL)
        *scheme_ptr = scheme;
    if (!scheme)
        return NULL;

    info = g_app_info_get_default_for_uri_scheme (scheme);
    #if !GLIB_CHECK_VERSION (2, 28, 0)
    if (!info)
    {
        gchar* type = g_strdup_printf ("x-scheme-handler/%s", scheme);
        info = g_app_info_get_default_for_type (type, FALSE);
        g_free (type);
    }
    #endif
    if (info != NULL && scheme_ptr != NULL)
        g_free (scheme);
    return info;

}

/**
 * sokoke_show_uri:
 * @screen: a #GdkScreen, or %NULL
 * @uri: the URI to show
 * @timestamp: the timestamp of the event
 * @error: the location of a #GError, or %NULL
 *
 * Shows the specified URI with an appropriate application. This
 * supports xdg-open, exo-open and gnome-open as fallbacks if
 * GIO doesn't do the trick.
 * x-scheme-handler is supported for GLib < 2.28 as of 0.3.3.
 *
 * On Maemo, hildon_uri_open() is used.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
sokoke_show_uri (GdkScreen*   screen,
                 const gchar* uri,
                 guint32      timestamp,
                 GError**     error)
{
    #if HAVE_HILDON
    HildonURIAction* action = hildon_uri_get_default_action_by_uri (uri, NULL);
    return hildon_uri_open (uri, action, error);

    #elif defined (G_OS_WIN32)
    CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
    SHELLEXECUTEINFO info = { sizeof (info) };
    info.nShow = SW_SHOWNORMAL;
    info.lpFile = uri;

    return ShellExecuteEx (&info);
    #else

    #if !GLIB_CHECK_VERSION (2, 28, 0)
    GAppInfo* info;
    gchar* scheme;
    #endif
    const gchar* fallbacks [] = { "xdg-open", "exo-open", "gnome-open" };
    gsize i;
    GtkWidget* dialog;
    GtkWidget* box;
    gchar* filename;
    gchar* ms;
    GtkWidget* entry;

    g_return_val_if_fail (GDK_IS_SCREEN (screen) || !screen, FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);
    g_return_val_if_fail (!error || !*error, FALSE);

    sokoke_recursive_fork_protection (uri, TRUE);

    if (gtk_show_uri (screen, uri, timestamp, error))
        return TRUE;

    #if !GLIB_CHECK_VERSION (2, 28, 0)
    info = sokoke_default_for_uri (uri, &scheme);
    if (info)
    {
        gchar* argument = g_strdup (&uri[scheme - uri]);
        GList* uris = g_list_prepend (NULL, argument);
        if (g_app_info_launch_uris (info, uris, NULL, NULL))
        {
            g_list_free (uris);
            g_free (scheme);
            g_object_unref (info);
            return TRUE;
        }
        g_list_free (uris);
        g_free (scheme);
        g_object_unref (info);
    }
    #endif

    for (i = 0; i < G_N_ELEMENTS (fallbacks); i++)
    {
        gchar* command = g_strconcat (fallbacks[i], " ", uri, NULL);
        gboolean result = g_spawn_command_line_async (command, error);
        g_free (command);
        if (result)
            return TRUE;
        if (error)
            *error = NULL;
    }

    dialog = gtk_dialog_new_with_buttons (_("Open with"), NULL, 0,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    if (g_str_has_prefix (uri, "file:///"))
        filename = g_filename_from_uri (uri, NULL, NULL);
    else
        filename = g_strdup (uri);
    ms = g_strdup_printf (_("Choose an application or command to open \"%s\":"),
                          filename);
    gtk_box_pack_start (GTK_BOX (box), gtk_label_new (ms), TRUE, FALSE, 4);
    g_free (ms);
    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_box_pack_start (GTK_BOX (box), entry, TRUE, FALSE, 4);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (sokoke_open_with_response_cb), entry);
    g_object_set_data_full (G_OBJECT (dialog), "uri",
                            filename, (GDestroyNotify)g_free);
    gtk_widget_show_all (dialog);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_widget_grab_focus (entry);

    return TRUE;
    #endif
}

/**
 * sokoke_prepare_command:
 * @command: the command, properly quoted
 * @argument: any arguments, properly quoted
 * @quote_command: if %TRUE, @command will be quoted
 * @quote_argument: if %TRUE, @argument will be quoted, ie. a URI or filename
 *
 * If @command contains %s, @argument will be quoted and inserted into
 * @command, which is left unquoted regardless of @quote_command.
 *
 * Return value: the command prepared for spawning
 **/
gchar*
sokoke_prepare_command (const gchar* command,
                        gboolean     quote_command,
                        const gchar* argument,
                        gboolean     quote_argument)
{
    g_return_val_if_fail (command != NULL, FALSE);
    g_return_val_if_fail (argument != NULL, FALSE);

    if (midori_debug ("paths"))
        g_print ("Preparing command: %s %d %s %d\n",
                 command, quote_command, argument, quote_argument);

    {
        gchar* uri_format;
        gchar* real_command;
        gchar* command_ready;

        /* .desktop files accept %u, %U, %f, %F as URI/ filename, we treat it like %s */
        real_command = g_strdup (command);
        if ((uri_format = strstr (real_command, "%u"))
         || (uri_format = strstr (real_command, "%U"))
         || (uri_format = strstr (real_command, "%f"))
         || (uri_format = strstr (real_command, "%F")))
            uri_format[1] = 's';


        if (strstr (real_command, "%s"))
        {
            gchar* argument_quoted = quote_argument ? g_shell_quote (argument) : g_strdup (argument);
            command_ready = g_strdup_printf (real_command, argument_quoted);
            g_free (argument_quoted);
        }
        else if (quote_argument)
        {
            gchar* quoted_command = quote_command ? g_shell_quote (real_command) : g_strdup (real_command);
            gchar* argument_quoted = g_shell_quote (argument);
            command_ready = g_strconcat (quoted_command, " ", argument_quoted, NULL);
            g_free (argument_quoted);
            g_free (quoted_command);
        }
        else
        {
            gchar* quoted_command = quote_command ? g_shell_quote (real_command) : g_strdup (real_command);
            command_ready = g_strconcat (quoted_command, " ", argument, NULL);
            g_free (quoted_command);
        }
        g_free (real_command);
        return command_ready;
    }
}

/**
 * sokoke_spawn_program:
 * @command: the command, properly quoted
 * @argument: any arguments, properly quoted
 * @quote_command: if %TRUE, @command will be quoted
 * @quote_argument: if %TRUE, @argument will be quoted, ie. a URI or filename
 *
 * If @command contains %s, @argument will be quoted and inserted into
 * @command, which is left unquoted regardless of @quote_command.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
sokoke_spawn_program (const gchar* command,
                      gboolean     quote_command,
                      const gchar* argument,
                      gboolean     quote_argument)
{
    GError* error;
    gchar* command_ready;
    gchar** argv;

    g_return_val_if_fail (command != NULL, FALSE);
    g_return_val_if_fail (argument != NULL, FALSE);

    command_ready = sokoke_prepare_command (command, quote_command, argument, quote_argument);
    g_print ("Launching command: %s\n", command_ready);

    error = NULL;
    if (!g_shell_parse_argv (command_ready, NULL, &argv, &error))
    {
        sokoke_message_dialog (GTK_MESSAGE_ERROR,
                               _("Could not run external program."),
                               error->message, FALSE);
        g_error_free (error);
        g_free (command_ready);
        return FALSE;
    }
    g_free (command_ready);

    error = NULL;
    if (!g_spawn_async (NULL, argv, NULL,
        (GSpawnFlags)G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
        NULL, NULL, NULL, &error))
    {
        sokoke_message_dialog (GTK_MESSAGE_ERROR,
                               _("Could not run external program."),
                               error->message, FALSE);
        g_error_free (error);
    }

    g_strfreev (argv);
    return TRUE;
}

void
sokoke_spawn_app (const gchar* uri,
                  gboolean     private)
{
    const gchar* executable = midori_paths_get_command_line (NULL)[0];
    gchar* uri_quoted = g_shell_quote (uri);
    gchar* argument;
    if (private)
    {
        gchar* config_quoted = g_shell_quote (midori_paths_get_config_dir ());
        argument = g_strconcat ("-c ", config_quoted,
                                " -p ", uri_quoted, NULL);
    }
    else
        argument = g_strconcat ("-a ", uri_quoted, NULL);
    g_free (uri_quoted);
    sokoke_spawn_program (executable, TRUE, argument, FALSE);
    g_free (argument);
}

static void
sokoke_resolve_hostname_cb (SoupAddress *address,
                            guint        status,
                            gpointer     data)
{
    if (status == SOUP_STATUS_OK)
        *(gint *)data = 1;
    else
        *(gint *)data = 2;
}

/**
 * sokoke_resolve_hostname
 * @hostname: a string typed by a user
 *
 * Takes a string that was typed by a user,
 * resolves the hostname, and returns the status.
 *
 * Return value: %TRUE if is a valid host, else %FALSE
 **/
gboolean
sokoke_resolve_hostname (const gchar* hostname)
{
    gchar* uri;
    gint host_resolved = 0;

    uri = g_strconcat ("http://", hostname, NULL);
    if (sokoke_prefetch_uri (NULL, uri, sokoke_resolve_hostname_cb,
                             &host_resolved))
    {
        GTimer* timer = g_timer_new ();
        while (!host_resolved && g_timer_elapsed (timer, NULL) < 10)
            g_main_context_iteration (NULL, FALSE);
        g_timer_destroy (timer);
    }
    g_free (uri);
    return host_resolved == 1 ? TRUE : FALSE;
}

gboolean
sokoke_external_uri (const gchar* uri)
{
    GAppInfo* info;

    if (!uri || !strncmp (uri, "http", 4)
             || !strncmp (uri, "file", 4)
             || !strncmp (uri, "geo", 3)
             || !strncmp (uri, "about:", 6))
        return FALSE;

    info = sokoke_default_for_uri (uri, NULL);
    if (info)
        g_object_unref (info);
    return info != NULL;
}

/**
 * sokoke_magic_uri:
 * @uri: a string typed by a user
 *
 * Takes a string that was typed by a user,
 * guesses what it is, and returns an URI.
 *
 * If it was a search, %NULL will be returned.
 *
 * Return value: a newly allocated URI, or %NULL
 **/
gchar*
sokoke_magic_uri (const gchar* uri)
{
    gchar** parts;
    gchar* search;

    g_return_val_if_fail (uri, NULL);

    /* Add file:// if we have a local path */
    if (g_path_is_absolute (uri))
        return g_filename_to_uri (uri, NULL, NULL);
    /* Parse geo URI geo:48.202778,16.368472;crs=wgs84;u=40 as a location */
    if (!strncmp (uri, "geo:", 4))
    {
        gchar* comma;
        gchar* semicolon;
        gchar* latitude;
        gchar* longitude;
        gchar* geo;

        comma = strchr (&uri[4], ',');
        /* geo:latitude,longitude[,altitude][;u=u][;crs=crs] */
        if (!(comma && *comma))
            return g_strdup (uri);
        semicolon = strchr (comma + 1, ';');
        if (!semicolon)
            semicolon = strchr (comma + 1, ',');
        latitude = g_strndup (&uri[4], comma - &uri[4]);
        if (semicolon)
            longitude = g_strndup (comma + 1, semicolon - comma - 1);
        else
            longitude = g_strdup (comma + 1);
        geo = g_strdup_printf ("http://www.openstreetmap.org/?mlat=%s&mlon=%s",
            latitude, longitude);
        g_free (latitude);
        g_free (longitude);
        return geo;
    }
    if (midori_uri_is_location (uri) || sokoke_external_uri (uri))
        return g_strdup (uri);
    if (midori_uri_is_ip_address (uri))
        return g_strconcat ("http://", uri, NULL);
    search = NULL;
    if (!strchr (uri, ' ') &&
        ((search = strchr (uri, ':')) || (search = strchr (uri, '@'))) &&
        search[0] && g_ascii_isdigit (search[1]))
        return g_strconcat ("http://", uri, NULL);
    if ((!strcmp (uri, "localhost") || strchr (uri, '/'))
      && sokoke_resolve_hostname (uri))
        return g_strconcat ("http://", uri, NULL);
    if (!search)
    {
        parts = g_strsplit (uri, ".", 0);
        if (parts[0] && parts[1])
        {
            if (!(parts[1][1] == '\0' && !g_ascii_isalpha (parts[1][0])))
                if (!strchr (parts[0], ' ') && !strchr (parts[1], ' '))
                {
                    search = g_strconcat ("http://", uri, NULL);
                    g_strfreev (parts);
                   return search;
                }
        }
        g_strfreev (parts);
    }
    return NULL;
}

void sokoke_widget_set_visible (GtkWidget* widget, gboolean visible)
{
    /* Show or hide the widget */
    if (visible)
        gtk_widget_show (widget);
    else
        gtk_widget_hide (widget);
}

typedef enum
{
    SOKOKE_DESKTOP_UNTESTED,
    SOKOKE_DESKTOP_XFCE,
    SOKOKE_DESKTOP_OSX,
    SOKOKE_DESKTOP_UNKNOWN
} SokokeDesktop;

static SokokeDesktop
sokoke_get_desktop (void)
{
    #if HAVE_OSX
    return SOKOKE_DESKTOP_OSX;
    #elif defined (GDK_WINDOWING_X11)
    static SokokeDesktop desktop = SOKOKE_DESKTOP_UNTESTED;
    if (G_UNLIKELY (desktop == SOKOKE_DESKTOP_UNTESTED))
    {
        desktop = SOKOKE_DESKTOP_UNKNOWN;

        /* Are we running in Xfce >= 4.8? */
        if (!g_strcmp0 (g_getenv ("DESKTOP_SESSION"), "xfce"))
        {
            desktop = SOKOKE_DESKTOP_XFCE;
        }
        else
        {
        /* Are we running in Xfce <= 4.6? */
        GdkDisplay* display = gdk_display_get_default ();
        Display* xdisplay = GDK_DISPLAY_XDISPLAY (display);
        Window root_window = RootWindow (xdisplay, 0);
        Atom save_mode_atom = gdk_x11_get_xatom_by_name_for_display (
            display, "_DT_SAVE_MODE");
        Atom actual_type;
        int actual_format;
        unsigned long n_items, bytes;
        gchar* value;
        int status = XGetWindowProperty (xdisplay, root_window,
            save_mode_atom, 0, (~0L),
            False, AnyPropertyType, &actual_type, &actual_format,
            &n_items, &bytes, (unsigned char**)&value);
        if (status == Success)
        {
            if (n_items == 6 && !strncmp (value, "xfce4", 6))
                desktop = SOKOKE_DESKTOP_XFCE;
            XFree (value);
        }
        }
    }

    return desktop;
    #else
    return SOKOKE_DESKTOP_UNKNOWN;
    #endif
}

/**
 * sokoke_xfce_header_new:
 * @icon: an icon name
 * @title: the title of the header
 *
 * Creates an Xfce style header *if* Xfce is running.
 *
 * Return value: A #GtkWidget or %NULL
 *
 * Since 0.1.2 @icon may be NULL, and a default is used.
 **/
GtkWidget*
sokoke_xfce_header_new (const gchar* icon,
                        const gchar* title)
{
    g_return_val_if_fail (title, NULL);

    /* Create an xfce header with icon and title
       This returns NULL if the desktop is not Xfce */
    if (sokoke_get_desktop () == SOKOKE_DESKTOP_XFCE)
    {
        GtkWidget* entry;
        gchar* markup;
        GtkWidget* xfce_heading;
        GtkWidget* hbox;
        GtkWidget* image;
        GtkWidget* label;
        GtkWidget* vbox;
        GtkWidget* separator;

        xfce_heading = gtk_event_box_new ();
        entry = gtk_entry_new ();

        hbox = gtk_hbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
        if (icon)
            image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_DIALOG);
        else
            image = gtk_image_new_from_stock (GTK_STOCK_PREFERENCES,
                GTK_ICON_SIZE_DIALOG);
        gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
        label = gtk_label_new (NULL);
        markup = g_strdup_printf ("<span size='large' weight='bold'>%s</span>",
                                  title);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (xfce_heading), hbox);
        g_free (markup);
        gtk_widget_destroy (entry);

        #if !GTK_CHECK_VERSION (3, 0, 0)
        {
        GtkStyle* style = gtk_widget_get_style (entry);
        gtk_widget_modify_bg (xfce_heading, GTK_STATE_NORMAL,
            &style->base[GTK_STATE_NORMAL]);
        gtk_widget_modify_fg (label, GTK_STATE_NORMAL
         , &style->text[GTK_STATE_NORMAL]);
        }
        #endif

        vbox = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), xfce_heading, FALSE, FALSE, 0);

        separator = gtk_hseparator_new ();
        gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

        return vbox;
    }
    return NULL;
}

gboolean
sokoke_key_file_save_to_file (GKeyFile*    key_file,
                              const gchar* filename,
                              GError**     error)
{
    gchar* data;
    gboolean success = FALSE;

    data = g_key_file_to_data (key_file, NULL, error);
    if (!data)
        return FALSE;

    success = g_file_set_contents (filename, data, -1, error);
    g_free (data);
    return success;
}

void
sokoke_widget_get_text_size (GtkWidget*   widget,
                             const gchar* text,
                             gint*        width,
                             gint*        height)
{
    PangoLayout* layout = gtk_widget_create_pango_layout (widget, text);
    pango_layout_get_pixel_size (layout, width, height);
    g_object_unref (layout);
}

/**
 * sokoke_action_create_popup_menu_item:
 * @action: a #GtkAction
 *
 * Creates a menu item from an action, just like
 * gtk_action_create_menu_item(), but it won't
 * display an accelerator.
 *
 * Note: This menu item is not a proxy and will
 *       not reflect any changes to the action.
 *
 * Return value: a new #GtkMenuItem
 **/
GtkWidget*
sokoke_action_create_popup_menu_item (GtkAction* action)
{
    GtkWidget* menuitem;
    GtkWidget* icon;
    gchar* label;
    gchar* stock_id;
    gchar* icon_name;
    gboolean sensitive;
    gboolean visible;

    g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

    if (KATZE_IS_ARRAY_ACTION (action))
        return gtk_action_create_menu_item (action);

    g_object_get (action,
                  "label", &label,
                  "stock-id", &stock_id,
                  "icon-name", &icon_name,
                  "sensitive", &sensitive,
                  "visible", &visible,
                  NULL);
    if (GTK_IS_TOGGLE_ACTION (action))
    {
        menuitem = gtk_check_menu_item_new_with_mnemonic (label);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
            gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
        if (GTK_IS_RADIO_ACTION (action))
            gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (menuitem),
                                                   TRUE);
    }
    else if (stock_id)
    {
        if (label)
        {
            menuitem = gtk_image_menu_item_new_with_mnemonic (label);
            icon = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
        else
            menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    }
    else
    {
        menuitem = gtk_image_menu_item_new_with_mnemonic (label);
        if (icon_name)
        {
            icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
    }
    gtk_widget_set_sensitive (menuitem, sensitive);
    sokoke_widget_set_visible (menuitem, visible);
    gtk_widget_set_no_show_all (menuitem, TRUE);
    g_signal_connect_swapped (menuitem, "activate",
                              G_CALLBACK (gtk_action_activate), action);

    return menuitem;
}

/**
 * sokoke_time_t_to_julian:
 * @timestamp: a time_t timestamp value
 *
 * Calculates a unix timestamp to a julian day value.
 *
 * Return value: an integer.
 **/
gint64
sokoke_time_t_to_julian (const time_t* timestamp)
{
    GDate* date;
    gint64 julian;

    date = g_date_new ();

    g_date_set_time_t (date, *timestamp);
    julian = (gint64)g_date_get_julian (date);

    g_date_free (date);

    return julian;
}

/**
 * sokoke_remove_path:
 * @path: an absolute path
 * @ignore_errors: keep removing even if an error occurred
 *
 * Removes the file at @path or the folder including any
 * child folders and files if @path is a folder.
 *
 * If @ignore_errors is %TRUE and @path is a folder with
 * children, one of which can't be removed, remaining
 * children will be deleted nevertheless
 * If @ignore_errors is %FALSE and @path is a folder, the
 * removal process will cancel immediately.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
sokoke_remove_path (const gchar* path,
                    gboolean     ignore_errors)
{
    GDir* dir = g_dir_open (path, 0, NULL);
    const gchar* name;

    if (!dir)
        return g_remove (path) == 0;

    while ((name = g_dir_read_name (dir)))
    {
        gchar* sub_path = g_build_filename (path, name, NULL);
        if (!sokoke_remove_path (sub_path, ignore_errors) && !ignore_errors)
            return FALSE;
        g_free (sub_path);
    }

    g_dir_close (dir);
    g_rmdir (path);
    return TRUE;
}

gchar*
sokoke_replace_variables (const gchar* template,
                          const gchar* variable_first, ...)
{
    gchar* result = g_strdup (template);
    const gchar* variable;

    va_list args;
    va_start (args, variable_first);

    for (variable = variable_first; variable; variable = va_arg (args, const gchar*))
    {
        const gchar* value = va_arg (args, const gchar*);
        GRegex* regex = g_regex_new (variable, 0, 0, NULL);
        gchar* replaced = result;
        result = g_regex_replace_literal (regex, replaced, -1, 0, value, 0, NULL);
        g_free (replaced);
        g_regex_unref (regex);
    }

    va_end (args);

    return result;
}

/**
 * sokoke_window_activate_key:
 * @window: a #GtkWindow
 * @event: a #GdkEventKey
 *
 * Attempts to activate they key from the event, much
 * like gtk_window_activate_key(), including keys
 * that gtk_accelerator_valid() considers invalid.
 *
 * Return value: %TRUE on success
 **/
gboolean
sokoke_window_activate_key (GtkWindow*   window,
                            GdkEventKey* event)
{
    gchar *accel_name;
    GQuark accel_quark;
    GObject* object;
    GSList *slist;

    if (gtk_window_activate_key (window, event))
        return TRUE;

    /* Hack to allow Ctrl + Shift + Tab */
    if (event->keyval == 65056)
        event->keyval = GDK_KEY_Tab;

    /* We don't use gtk_accel_groups_activate because it refuses to
        activate anything that gtk_accelerator_valid doesn't like. */
    accel_name = gtk_accelerator_name (event->keyval, (event->state & gtk_accelerator_get_default_mod_mask ()));
    accel_quark = g_quark_from_string (accel_name);
    g_free (accel_name);
    object = G_OBJECT (window);

    for (slist = gtk_accel_groups_from_object (object); slist; slist = slist->next)
        if (gtk_accel_group_activate (slist->data, accel_quark,
                                      object, event->keyval, event->state))
            return TRUE;

    return FALSE;
}

/**
 * sokoke_gtk_action_count_modifiers:
 * @action: a #GtkAction
 *
 * Counts the number of modifiers in the accelerator
 * belonging to the action.
 *
 * Return value: the number of modifiers
 **/
guint
sokoke_gtk_action_count_modifiers (GtkAction* action)
{
    GtkAccelKey key;
    gint mods, cmods = 0;
    const gchar* accel_path;

    g_return_val_if_fail (GTK_IS_ACTION (action), 0);

    accel_path = gtk_action_get_accel_path (action);
    if (accel_path)
        if (gtk_accel_map_lookup_entry (accel_path, &key))
        {
            mods = key.accel_mods;
            while (mods)
            {
                if (1 & mods >> 0)
                    cmods++;
                mods = mods >> 1;
            }
        }
    return cmods;
}

/**
 * sokoke_file_chooser_dialog_new:
 * @title: a window title, or %NULL
 * @window: a parent #GtkWindow, or %NULL
 * @action: a #GtkFileChooserAction
 *
 * Creates a new file chooser dialog, as appropriate for
 * the platform, with buttons according to the @action.
 *
 * The positive response is %GTK_RESPONSE_OK.
 *
 * Return value: a new #GtkFileChooser
 **/
GtkWidget*
sokoke_file_chooser_dialog_new (const gchar*         title,
                                GtkWindow*           window,
                                GtkFileChooserAction action)
{
    const gchar* stock_id = GTK_STOCK_OPEN;
    GtkWidget* dialog;

    if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
        stock_id = GTK_STOCK_SAVE;
    #ifdef HAVE_HILDON_FM
    dialog = hildon_file_chooser_dialog_new (window, action);
    #else
    dialog = gtk_file_chooser_dialog_new (title, window, action,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        stock_id, GTK_RESPONSE_OK, NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), stock_id);
    #endif
    return dialog;
}

/**
 * sokoke_prefetch_uri:
 * @settings: a #MidoriWebSettings instance, or %NULL
 * @uri: an URI string
 *
 * Attempts to prefetch the specified URI, that is
 * it tries to resolve the hostname in advance.
 *
 * Return value: %TRUE on success
 **/
gboolean
sokoke_prefetch_uri (MidoriWebSettings*  settings,
                     const char*         uri,
                     SoupAddressCallback callback,
                     gpointer            user_data)
{
    #define MAXHOSTS 50
    static gchar* hosts = NULL;
    static gint host_count = G_MAXINT;
    gchar* hostname;

    if (settings && !katze_object_get_boolean (settings, "enable-dns-prefetching"))
        return FALSE;

    if (!(hostname = midori_uri_parse_hostname (uri, NULL))
     || g_hostname_is_ip_address (hostname)
     || !midori_uri_is_http (uri))
    {
        g_free (hostname);
        return FALSE;
    }

    if (!hosts ||
        !g_regex_match_simple (hostname, hosts,
                               G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
        SoupAddress* address;
        gchar* new_hosts;

        address = soup_address_new (hostname, SOUP_ADDRESS_ANY_PORT);
        soup_address_resolve_async (address, 0, 0, callback, user_data);
        g_object_unref (address);

        if (host_count > MAXHOSTS)
        {
            katze_assign (hosts, g_strdup (""));
            host_count = 0;
        }
        host_count++;
        new_hosts = g_strdup_printf ("%s|%s", hosts, hostname);
        katze_assign (hosts, new_hosts);
    }
    else if (callback)
        callback (NULL, SOUP_STATUS_OK, user_data);
    g_free (hostname);
    return TRUE;
}

/**
 * sokoke_recursive_fork_protection
 * @uri: the URI to check
 * @set_uri: if TRUE the URI will be saved
 *
 * Protects against recursive invokations of the Midori executable
 * with the same URI.
 *
 * As an example, consider having an URI starting with 'tel://'. You
 * could attempt to open it with sokoke_show_uri. In turn, 'exo-open'
 * might be called. Now quite possibly 'exo-open' is unable to handle
 * 'tel://' and might well fall back to 'midori' as default browser.
 *
 * To protect against this scenario, call this function with the
 * URI and %TRUE before calling any external tool.
 * #MidoriApp calls sokoke_recursive_fork_protection() with %FALSE
 * and bails out if %FALSE is returned.
 *
 * Return value: %TRUE if @uri is new, %FALSE on recursion
 **/
gboolean
sokoke_recursive_fork_protection (const gchar* uri,
                                  gboolean     set_uri)
{
    static gchar* fork_uri = NULL;
    if (set_uri)
        katze_assign (fork_uri, g_strdup (uri));
    return g_strcmp0 (fork_uri, uri) == 0 ? FALSE : TRUE;
}

/**
 * sokoke_register_privacy_item:
 * @name: the name of the privacy item
 * @label: a user visible, localized label
 * @clear: a callback clearing data
 *
 * Registers an item to clear data, either via the
 * Clear Private Data dialogue or when Midori quits.
 *
 * Return value: a #GList if all arguments are %NULL
 **/
GList*
sokoke_register_privacy_item (const gchar* name,
                              const gchar* label,
                              GCallback    clear)
{
    static GList* items = NULL;
    SokokePrivacyItem* item;

    if (name == NULL && label == NULL && clear == NULL)
        return items;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (clear != NULL, NULL);

    item = g_new (SokokePrivacyItem, 1);
    item->name = g_strdup (name);
    item->label = g_strdup (label);
    item->clear = clear;
    items = g_list_append (items, item);
    return NULL;
}

void
sokoke_widget_copy_clipboard (GtkWidget*   widget,
                              const gchar* text)
{
    GdkDisplay* display = gtk_widget_get_display (widget);
    GtkClipboard* clipboard;

    clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (clipboard, text ? text : "", -1);
    clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text (clipboard, text ? text : "", -1);
}

static gboolean
sokoke_entry_has_placeholder_text (GtkEntry* entry)
{
    const gchar* text = gtk_entry_get_text (entry);
    const gchar* hint = gtk_entry_get_placeholder_text (entry);
    if (!gtk_widget_has_focus (GTK_WIDGET (entry))
     && hint != NULL
     && (text == NULL || !strcmp (text, hint)))
        return TRUE;
    return FALSE;
}

static void
sokoke_entry_changed_cb (GtkEditable* editable,
                         GtkEntry*    entry)
{
    const gchar* text = gtk_entry_get_text (entry);
    gboolean visible = text && *text
      && ! sokoke_entry_has_placeholder_text (entry);
    gtk_icon_entry_set_icon_from_stock (
        GTK_ICON_ENTRY (entry),
        GTK_ICON_ENTRY_SECONDARY,
        visible ? GTK_STOCK_CLEAR : NULL);
}

static gboolean
sokoke_entry_focus_out_event_cb (GtkEditable*   editable,
                                 GdkEventFocus* event,
                                 GtkEntry*      entry)
{
    sokoke_entry_changed_cb (editable, entry);
    return FALSE;
}

static void
sokoke_entry_icon_released_cb (GtkEntry*            entry,
                               GtkIconEntryPosition icon_pos,
                               GdkEvent*            event,
                               gpointer             user_data)
{
    if (icon_pos != GTK_ICON_ENTRY_SECONDARY)
        return;

    gtk_entry_set_text (entry, "");
    gtk_widget_grab_focus (GTK_WIDGET (entry));
}

GtkWidget*
sokoke_search_entry_new (const gchar* placeholder_text)
{
    GtkWidget* entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (entry), placeholder_text);
    gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
    gtk_icon_entry_set_icon_highlight (GTK_ENTRY (entry),
        GTK_ENTRY_ICON_SECONDARY, TRUE);
    {
        g_object_connect (entry,
            "signal::icon-release",
            G_CALLBACK (sokoke_entry_icon_released_cb), NULL,
            "signal::focus-in-event",
            G_CALLBACK (sokoke_entry_focus_out_event_cb), entry,
            "signal::focus-out-event",
            G_CALLBACK (sokoke_entry_focus_out_event_cb), entry,
            "signal::changed",
            G_CALLBACK (sokoke_entry_changed_cb), entry, NULL);
        sokoke_entry_changed_cb ((GtkEditable*)entry, GTK_ENTRY (entry));
    }
    return entry;
}

