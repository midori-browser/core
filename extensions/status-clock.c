/*
 Copyright (C) 2010 Arno Renevier <arno@renevier.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/*
 * This extension adds time and/or date in midori statusbar. Format for time
 * display can be configured by creating a desktop entry file named
 * ~/.config/midori/extensions/libclock.so/config
 *
 * That file must contain a section "settings", and a key "format". That format
 * will be used as format parameter to strftime. For example, If you want to
 * display full date and time according to your locale, "config" must contain:
 *
 * [settings]
 * format=%c
 *
 * If that file does not exist, or format specification cannot be read, format
 * fallback to %R which means time will be display with a 24-hour notation. For
 * example, 13:53
 */

#include <midori/midori.h>

#include "config.h"

#define DEFAULT_FORMAT "%R"

static void
clock_deactivate_cb (MidoriExtension* extension,
                     MidoriApp*   app);

static void
clock_set_timeout (MidoriBrowser* browser,
                   guint interval);

static gboolean
clock_set_current_time (MidoriBrowser* browser)
{
    guint interval;
    MidoriExtension* extension = g_object_get_data (G_OBJECT (browser), "clock-extension");
    GtkWidget* label = g_object_get_data (G_OBJECT (browser), "clock-label");
    const gchar* format = midori_extension_get_string (extension, "format");

    #if GLIB_CHECK_VERSION (2, 26, 0)
    GDateTime* date = g_date_time_new_now_local ();
    gint seconds = g_date_time_get_seconds (date);
    gchar* pretty = g_date_time_format (date, format);
    gtk_label_set_label (GTK_LABEL (label), pretty);
    g_free (pretty);
    g_date_time_unref (date);
    #else
    time_t rawtime = time (NULL);
    struct tm *tm = localtime (&rawtime);
    gint seconds = tm->tm_sec;
    char date_fmt[512];
    strftime (date_fmt, sizeof (date_fmt), format, tm);
    gtk_label_set_label (GTK_LABEL (label), date_fmt);
    #endif

    if (g_strstr_len (format, -1, "%c")
     || g_strstr_len (format, -1, "%N")
     || g_strstr_len (format, -1, "%s")
     || g_strstr_len (format, -1, "%S")
     || g_strstr_len (format, -1, "%T")
     || g_strstr_len (format, -1, "%X")
    )
        interval = 1;
    else
        /* FIXME: Occasionally there are more than 60 seconds in a minute. */
        interval = MAX (60 - seconds, 1);

    clock_set_timeout (browser, interval);

    return FALSE;
}

static void
clock_set_timeout (MidoriBrowser* browser,
                   guint interval)
{
    GSource* source;
    source = g_timeout_source_new_seconds (interval);
    g_source_set_callback (source, (GSourceFunc)clock_set_current_time, browser, NULL);
    g_source_attach (source, NULL);
    g_object_set_data (G_OBJECT (browser), "clock-timer", source);
    g_source_unref (source);
}

static void
clock_browser_destroy_cb (MidoriBrowser* browser,
                          gpointer data)
{
    GSource* source;
    source = g_object_get_data (G_OBJECT (browser), "clock-timer");
    g_source_destroy (source);
    g_signal_handlers_disconnect_by_func (browser, clock_browser_destroy_cb, NULL);
}

static void
clock_app_add_browser_cb (MidoriApp*       app,
                            MidoriBrowser*   browser,
                            MidoriExtension* extension)
{
    GtkWidget* statusbar;
    GtkWidget* label;

    label = gtk_label_new (NULL);

    statusbar = katze_object_get_object (browser, "statusbar");
    gtk_box_pack_end  (GTK_BOX (statusbar), label, FALSE, FALSE, 0);

    g_object_set_data (G_OBJECT (browser), "clock-label", label);
    g_object_set_data (G_OBJECT (browser), "clock-extension", extension);

    clock_set_current_time (browser);
    gtk_widget_show (label);

    g_object_unref (statusbar);

    g_signal_connect (browser, "destroy", G_CALLBACK (clock_browser_destroy_cb), NULL);
}

static void
clock_deactivate_cb (MidoriExtension* extension,
                     MidoriApp*   app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    GtkWidget* label;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
    {
        clock_browser_destroy_cb (browser, NULL);
        label = g_object_get_data (G_OBJECT (browser), "clock-label");
        gtk_widget_destroy (label);
        g_object_set_data (G_OBJECT (browser), "clock-label", NULL);
    }

    g_signal_handlers_disconnect_by_func (
          app, clock_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
          extension, clock_deactivate_cb, app);
    g_object_unref (browsers);
}

static void
clock_activate_cb (MidoriExtension* extension,
                   MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        clock_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (clock_app_add_browser_cb), extension);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (clock_deactivate_cb), app);

    g_object_unref (browsers);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Statusbar Clock"),
        "description", _("Display date and time in the statusbar"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Arno Renevier <arno@renevier.net>",
        NULL);
    midori_extension_install_string (extension, "format", DEFAULT_FORMAT);

    g_signal_connect (extension, "activate",
        G_CALLBACK (clock_activate_cb), NULL);

    return extension;
}
