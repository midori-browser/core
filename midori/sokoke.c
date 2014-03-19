/*
 Copyright (C) 2007-2013 Christian Dywan <christian@twotoasts.de>
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
#include "katze/katze.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <shlobj.h>
#include <gdk/gdkwin32.h>
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

void
sokoke_message_dialog (GtkMessageType message_type,
                       const gchar*   short_message,
                       const gchar*   detailed_message,
                       gboolean       modal)
{
    midori_show_message_dialog (message_type, short_message, detailed_message, modal);
}

GAppInfo*
sokoke_default_for_uri (const gchar* uri,
                        gchar**      scheme_ptr)
{
    gchar* scheme;
    GAppInfo* info;

    scheme = g_uri_parse_scheme (uri);
    if (!scheme)
        return NULL;

    info = g_app_info_get_default_for_uri_scheme (scheme);
    if (scheme_ptr != NULL)
        *scheme_ptr = scheme;
    else
        g_free (scheme);
    return info;

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
 * @sync: spawn synchronously and wait for command to exit
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
                      gboolean     quote_argument,
                      gboolean     sync)
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
    if (sync)
        g_spawn_sync (NULL, argv, NULL,
            (GSpawnFlags)G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, NULL, NULL, &error);
    else
        g_spawn_async (NULL, argv, NULL,
            (GSpawnFlags)G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
            NULL, NULL, NULL, &error);
    if (error != NULL)
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
sokoke_spawn_gdb (const gchar* gdb,
                  gboolean     sync)
{
    gchar* args = midori_paths_get_command_line_str (FALSE);
    const gchar* runtime_dir = midori_paths_get_runtime_dir ();
    gchar* cmd = g_strdup_printf (
        "--batch -ex 'set print thread-events off' -ex run "
        "-ex 'set logging on %s/%s' -ex 'bt' --return-child-result "
        "--args %s",
        runtime_dir, "gdb.bt", args);
    sokoke_spawn_program (gdb, TRUE, cmd, FALSE, sync);
    g_free (cmd);
    g_free (args);
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
        gchar* config_quoted = g_shell_quote (midori_paths_get_config_dir_for_reading ());
        argument = g_strconcat ("-c ", config_quoted,
                                " -p ", uri_quoted, NULL);
    }
    else
        argument = g_strconcat ("-a ", uri_quoted, NULL);
    g_free (uri_quoted);
    sokoke_spawn_program (executable, TRUE, argument, FALSE, FALSE);
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
    if (sokoke_prefetch_uri (NULL, uri, G_CALLBACK (sokoke_resolve_hostname_cb),
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

    /* URI schemes are case-insensitive, followed by ':' - rfc3986 */
    if (!uri || !strncasecmp (uri, "http:", 5)
             || !strncasecmp (uri, "https:", 6)
             || !strncasecmp (uri, "file:", 5)
             || !strncasecmp (uri, "geo:", 4)
             || !strncasecmp (uri, "about:", 6))
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
sokoke_magic_uri (const gchar* uri,
                  gboolean     allow_search,
                  gboolean     allow_relative)
{
    gchar** parts;
    gchar* search;

    g_return_val_if_fail (uri, NULL);

    /* Add file:// if we have a local path */
    if (g_path_is_absolute (uri))
        return g_filename_to_uri (uri, NULL, NULL);
    if (allow_relative
     && g_file_test (uri, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    {
        GFile* file = g_file_new_for_commandline_arg (uri);
        gchar* uri_ready = g_file_get_uri (file);
        g_object_unref (file);
        return uri_ready;
    }
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
    if (!allow_search)
        midori_error (_("Invalid URI"));
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
            if (GDK_IS_X11_DISPLAY (display))
            {
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
                     GCallback           callback,
                     gpointer            user_data)
{
    gchar* hostname;
#ifndef HAVE_WEBKIT2
    SoupURI* soup_uri;
    SoupSession* session = webkit_get_default_session ();

    g_object_get (G_OBJECT (session), "proxy-uri", &soup_uri, NULL);
    if (soup_uri)
        return FALSE;
#endif

    if (settings && !katze_object_get_boolean (settings, "enable-dns-prefetching"))
        return FALSE;

    if (!(hostname = midori_uri_parse_hostname (uri, NULL))
     || g_hostname_is_ip_address (hostname)
     || !midori_uri_is_http (uri))
    {
        g_free (hostname);
        return FALSE;
    }

#ifdef HAVE_WEBKIT2
    WebKitWebContext* context = webkit_web_context_get_default ();
    webkit_web_context_prefetch_dns (context, hostname);
    g_free (hostname);
    return FALSE;
#else
    #define MAXHOSTS 50
    static gchar* hosts = NULL;
    static gint host_count = G_MAXINT;

    if (!hosts ||
        !g_regex_match_simple (hostname, hosts,
                               G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
        SoupAddress* address;
        gchar* new_hosts;

        address = soup_address_new (hostname, SOUP_ADDRESS_ANY_PORT);
        soup_address_resolve_async (address, 0, 0, (SoupAddressCallback)callback, user_data);
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
        ((SoupAddressCallback)callback) (NULL, SOUP_STATUS_OK, user_data);
    g_free (hostname);
    return TRUE;
#endif
}

static void
sokoke_widget_clipboard_owner_clear_func (GtkClipboard* clipboard,
                                          gpointer      user_data)
{
    g_object_unref (user_data);
}

void
sokoke_widget_copy_clipboard (GtkWidget*          widget,
                              const gchar*        text,
                              GtkClipboardGetFunc get_cb,
                              gpointer            owner)
{
    GdkDisplay* display = gtk_widget_get_display (widget);
    GtkClipboard* clipboard;

    g_return_if_fail (text != NULL);

    clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text (clipboard, text, -1);

    clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);
    if (get_cb == NULL)
        gtk_clipboard_set_text (clipboard, text, -1);
    else
    {
        GtkTargetList* target_list = gtk_target_list_new (NULL, 0);
        GtkTargetEntry* targets;
        gint n_targets;
        gtk_target_list_add_text_targets (target_list, 0);
        gtk_target_list_add_image_targets (target_list, 0, TRUE);
        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_clipboard_set_with_owner (clipboard, targets, n_targets, get_cb,
            sokoke_widget_clipboard_owner_clear_func, owner);
        gtk_target_table_free (targets, n_targets);
        gtk_target_list_unref (target_list);
    }
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
    gtk_entry_set_icon_from_stock (
        entry, GTK_ENTRY_ICON_SECONDARY,
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
                               GtkEntryIconPosition icon_pos,
                               GdkEvent*            event,
                               gpointer             user_data)
{
    if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
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
    gtk_entry_set_icon_activatable (GTK_ENTRY (entry),
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

#ifdef G_OS_WIN32
gchar*
sokoke_get_win32_desktop_lnk_path_for_filename (gchar* filename)
{
    const gchar* desktop_dir;
    gchar* lnk_path, *lnk_file;

    /* CSIDL_PROGRAMS for "start menu -> programs" instead - needs saner/shorter filename */
    desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

    lnk_file = g_strconcat (filename, ".lnk", NULL);
    lnk_path = g_build_filename (desktop_dir, lnk_file, NULL);

    g_free (lnk_file);

    return lnk_path;
}

void
sokoke_create_win32_desktop_lnk (gchar* prefix, gchar* filename, gchar* uri)
{
    WCHAR w[MAX_PATH];

    gchar* exec_dir, *exec_path, *argument;
    gchar* lnk_path, *launcher_type;

    IShellLink* pShellLink;
    IPersistFile* pPersistFile;

    exec_dir = g_win32_get_package_installation_directory_of_module (NULL);
    exec_path = g_build_filename (exec_dir, "bin", "midori.exe", NULL);

    if (g_str_has_suffix (prefix, " -a "))
        launcher_type = "-a";
    else if (g_str_has_suffix (prefix, " -c "))
        launcher_type = "-c";
    else
        g_assert_not_reached ();

    argument = g_strdup_printf ("%s \"%s\"", launcher_type, uri);

    /* Create link */
    CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *)&pShellLink);
    pShellLink->lpVtbl->SetPath (pShellLink, exec_path);
    pShellLink->lpVtbl->SetArguments (pShellLink, argument);
    /* TODO: support adding site favicon as webapp icon */
    /* pShellLink->lpVtbl->SetIconLocation (pShellLink, icon_path, icon_index); */

    /* Save link */
    lnk_path = sokoke_get_win32_desktop_lnk_path_for_filename (filename);
    pShellLink->lpVtbl->QueryInterface (pShellLink, &IID_IPersistFile, (LPVOID *)&pPersistFile);
    MultiByteToWideChar (CP_UTF8, 0, lnk_path, -1, w, MAX_PATH);
    pPersistFile->lpVtbl->Save (pPersistFile, w, TRUE);

    pPersistFile->lpVtbl->Release (pPersistFile);
    pShellLink->lpVtbl->Release (pShellLink);

    g_free (exec_dir);
    g_free (exec_path);
    g_free (argument);
    g_free (lnk_path);
    g_free (launcher_type);
}

GdkPixbuf*
sokoke_get_gdk_pixbuf_from_win32_executable (gchar* path)
{
    if (path == NULL)
        return NULL;

    GdkPixbuf* pixbuf = NULL;
    HICON hIcon = NULL;
    HINSTANCE hInstance = NULL;
    hIcon = ExtractIcon (hInstance, (LPCSTR)path, 0);
    if (hIcon == NULL)
        return NULL;

#if GTK_CHECK_VERSION (3, 9, 12)
    pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (hIcon, NULL, NULL);
#else
    pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (hIcon);
#endif
    DestroyIcon (hIcon);

    return pixbuf;
}
#endif
