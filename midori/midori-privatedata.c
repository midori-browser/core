/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-privatedata.h"

#include "marshal.h"
#include "midori-platform.h"
#include "midori-core.h"

#include "config.h"
#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <sqlite3.h>

    #define LIBSOUP_USE_UNSTABLE_REQUEST_API
    #include <libsoup/soup-cache.h>

static void
midori_private_data_dialog_response_cb (GtkWidget*     dialog,
                                        gint           response_id,
                                        MidoriBrowser* browser)
{
    if (response_id == GTK_RESPONSE_ACCEPT)
    {
        GtkToggleButton* button;
        gint clear_prefs = MIDORI_CLEAR_NONE;
        gint saved_prefs = MIDORI_CLEAR_NONE;
        GList* data_items = midori_private_data_register_item (NULL, NULL, NULL);
        GString* clear_data = g_string_new (NULL);
        MidoriWebSettings* settings = midori_browser_get_settings (browser);
        g_object_get (settings, "clear-private-data", &saved_prefs, NULL);

        button = g_object_get_data (G_OBJECT (dialog), "session");
        if (gtk_toggle_button_get_active (button))
        {
            GList* tabs = midori_browser_get_tabs (browser);
            for (; tabs != NULL; tabs = g_list_next (tabs))
                midori_browser_close_tab (browser, tabs->data);
            g_list_free (tabs);
            clear_prefs |= MIDORI_CLEAR_SESSION;
        }
        button = g_object_get_data (G_OBJECT (dialog), "history");
        if (gtk_toggle_button_get_active (button))
        {
            KatzeArray* history = katze_object_get_object (browser, "history");
            KatzeArray* trash = katze_object_get_object (browser, "trash");
            katze_array_clear (history);
            katze_array_clear (trash);
            g_object_ref (history);
            g_object_ref (trash);
            clear_prefs |= MIDORI_CLEAR_HISTORY;
            clear_prefs |= MIDORI_CLEAR_TRASH; /* For backward-compatibility */
        }
        if (clear_prefs != saved_prefs)
        {
            clear_prefs |= (saved_prefs & MIDORI_CLEAR_ON_QUIT);
            g_object_set (settings, "clear-private-data", clear_prefs, NULL);
        }
        for (; data_items != NULL; data_items = g_list_next (data_items))
        {
            MidoriPrivateDataItem* privacy = data_items->data;
            button = g_object_get_data (G_OBJECT (dialog), privacy->name);
            g_return_if_fail (button != NULL && GTK_IS_TOGGLE_BUTTON (button));
            if (gtk_toggle_button_get_active (button))
            {
                privacy->clear ();
                g_string_append (clear_data, privacy->name);
                g_string_append_c (clear_data, ',');
            }
        }
        g_object_set (settings, "clear-data", clear_data->str, NULL);
        g_string_free (clear_data, TRUE);
    }
    if (response_id != GTK_RESPONSE_DELETE_EVENT)
        gtk_widget_destroy (dialog);
}

static void
midori_private_data_clear_on_quit_toggled_cb (GtkToggleButton*   button,
                                              MidoriWebSettings* settings)
{
    gint clear_prefs = MIDORI_CLEAR_NONE;
    g_object_get (settings, "clear-private-data", &clear_prefs, NULL);
    clear_prefs ^= MIDORI_CLEAR_ON_QUIT;
    g_object_set (settings, "clear-private-data", clear_prefs, NULL);
}

/**
 * midori_private_data_dialog_is_empty:
 * @dialog: the dialog
 *
 * The dialog is "empty" when none of the relevant checkboxes are activated.
 *
 * This function returns true if the dialog is empty.
 **/
static bool
midori_private_data_dialog_is_empty (GtkDialog* dialog)
{
    GtkToggleButton* button;
    gint count = 0; // Counts the total number of checked boxes
    GList* data_items = midori_private_data_register_item (NULL, NULL, NULL);

    // Count these first two special ones
    button = g_object_get_data (G_OBJECT (dialog), "session");
    if (gtk_toggle_button_get_active (button))
        count++;
    button = g_object_get_data (G_OBJECT (dialog), "history");
    if (gtk_toggle_button_get_active (button))
        count++;

    // Count each other one
    for (; data_items != NULL; data_items = g_list_next (data_items))
    {
        MidoriPrivateDataItem* privacy = data_items->data;
        button = g_object_get_data (G_OBJECT (dialog), privacy->name);
        g_return_val_if_fail (button != NULL && GTK_IS_TOGGLE_BUTTON (button), false);
        if (gtk_toggle_button_get_active (button))
            count++;
    }

    // No checked boxes means the dialog is empty
    if (count == 0)
        return true;
    return false;
}

/**
 * midori_private_data_clear_button_check_sensitive:
 * @dialog: the dialog to clear
 * 
 * When called, sets the sensitivity of the clear private data button depending
 * on whether the dialog is empty (see: midori_private_data_dialog_is_empty())
 **/
static void
midori_private_data_clear_button_check_sensitive (GtkDialog* dialog)
{
    GtkWidget* clear_button;
    clear_button = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_ACCEPT);
    if (midori_private_data_dialog_is_empty (dialog))
        gtk_widget_set_sensitive (clear_button, FALSE);
    else
        gtk_widget_set_sensitive (clear_button, TRUE);
}

static void
midori_private_data_checkbox_toggled_cb (GtkToggleButton*   button,
                                              GtkWidget* dialog)
{
    // This is a separate function so I can invoke it on start too
    midori_private_data_clear_button_check_sensitive (GTK_DIALOG (dialog));
}

/**
 * midori_private_data_get_dialog:
 * @browser: the browser for which to create a dialog
 *
 * Shows a dialog for the user to configure private data settings 
 * and clear some items.
 *
 * Return value: (transfer full): the dialog
 **/
GtkWidget*
midori_private_data_get_dialog (MidoriBrowser* browser)
{
    GtkWidget* dialog;
    GtkWidget* content_area;
    GdkScreen* screen;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* alignment;
    GtkWidget* vbox;
    GtkWidget* icon;
    GtkWidget* label;
    GtkWidget* button;
    GList* data_items;
    MidoriWebSettings* settings = midori_browser_get_settings (browser);
    gchar* clear_data = katze_object_get_string (settings, "clear-data");
    gint clear_prefs = MIDORI_CLEAR_NONE;
    g_object_get (settings, "clear-private-data", &clear_prefs, NULL);

    /* i18n: Dialog: Clear Private Data, in the Tools menu */
    dialog = gtk_dialog_new_with_buttons (_("Clear Private Data"),
        GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        _("_Clear private data"), GTK_RESPONSE_ACCEPT, NULL);
    button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    g_signal_connect (dialog, "response",
        G_CALLBACK (midori_private_data_dialog_response_cb), browser);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    katze_widget_add_class (button, "destructive-action");
    screen = gtk_widget_get_screen (GTK_WIDGET (browser));
    if (screen)
        gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_CLEAR);
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    hbox = gtk_hbox_new (FALSE, 4);
    icon = gtk_image_new_from_icon_name ("edit-clear", GTK_ICON_SIZE_DIALOG);
    gtk_size_group_add_widget (sizegroup, icon);
    gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
    label = gtk_label_new (_("Clear the following data:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 4);
    icon = gtk_image_new ();
    gtk_size_group_add_widget (sizegroup, icon);
    gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
    vbox = gtk_vbox_new (TRUE, 4);
    alignment = gtk_alignment_new (0, 0, 1, 1);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 6, 12, 0);
    button = gtk_check_button_new_with_mnemonic (_("Last open _tabs"));
    if ((clear_prefs & MIDORI_CLEAR_SESSION) == MIDORI_CLEAR_SESSION)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    g_object_set_data (G_OBJECT (dialog), "session", button);
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
    g_signal_connect (button, "toggled",
            G_CALLBACK (midori_private_data_checkbox_toggled_cb), dialog);
    /* i18n: Browsing history, visited web pages, closed tabs */
    button = gtk_check_button_new_with_mnemonic (_("_History"));
    if ((clear_prefs & MIDORI_CLEAR_HISTORY) == MIDORI_CLEAR_HISTORY)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    g_object_set_data (G_OBJECT (dialog), "history", button);
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
    g_signal_connect (button, "toggled",
            G_CALLBACK (midori_private_data_checkbox_toggled_cb), dialog);

    data_items = midori_private_data_register_item (NULL, NULL, NULL);
    for (; data_items != NULL; data_items = g_list_next (data_items))
    {
        MidoriPrivateDataItem* privacy = data_items->data;
        button = gtk_check_button_new_with_mnemonic (privacy->label);
        gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
        g_object_set_data (G_OBJECT (dialog), privacy->name, button);
        if (clear_data && strstr (clear_data, privacy->name))
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
        g_signal_connect (button, "toggled",
            G_CALLBACK (midori_private_data_checkbox_toggled_cb), dialog);
    }
    midori_private_data_clear_button_check_sensitive (GTK_DIALOG (dialog));
    g_free (clear_data);
    gtk_container_add (GTK_CONTAINER (alignment), vbox);
    gtk_box_pack_start (GTK_BOX (hbox), alignment, TRUE, TRUE, 4);
    gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 8);
    button = gtk_check_button_new_with_mnemonic (_("Clear private data when _quitting Midori"));
    if ((clear_prefs & MIDORI_CLEAR_ON_QUIT) == MIDORI_CLEAR_ON_QUIT)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    g_signal_connect (button, "toggled",
        G_CALLBACK (midori_private_data_clear_on_quit_toggled_cb), settings);
    alignment = gtk_alignment_new (0, 0, 1, 1);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 2, 0);
    gtk_container_add (GTK_CONTAINER (alignment), button);
    gtk_box_pack_start (GTK_BOX (content_area), alignment, FALSE, FALSE, 0);
    gtk_widget_show_all (content_area);
    return dialog;
}

static void
midori_remove_config_file (gint         clear_prefs,
                           gint         flag,
                           const gchar* filename)
{
    if ((clear_prefs & flag) == flag)
    {
        gchar* config_file = midori_paths_get_config_filename_for_writing (filename);
        g_unlink (config_file);
        g_free (config_file);
    }
}

static void
midori_clear_web_cookies_cb (void)
{
#ifdef HAVE_WEBKIT2
    WebKitWebContext* context = webkit_web_context_get_default ();
    WebKitCookieManager* cookie_manager = webkit_web_context_get_cookie_manager (context);
    webkit_cookie_manager_delete_all_cookies (cookie_manager);
    /* FIXME: site data policy */
#else
    SoupSession* session = webkit_get_default_session ();
    MidoriWebSettings* settings = g_object_get_data (G_OBJECT (session), "midori-settings");
    SoupSessionFeature* jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    GSList* cookies = soup_cookie_jar_all_cookies (SOUP_COOKIE_JAR (jar));

    /* HTTP Cookies/ Web Cookies */
    for (; cookies != NULL; cookies = g_slist_next (cookies))
    {
        const gchar* domain = ((SoupCookie*)cookies->data)->domain;
        if (midori_web_settings_get_site_data_policy (settings, domain)
         == MIDORI_SITE_DATA_PRESERVE)
            continue;
        soup_cookie_jar_delete_cookie ((SoupCookieJar*)jar, cookies->data);
    }
    soup_cookies_free (cookies);
#endif

    /* Local shared objects/ Flash cookies */
    if (midori_web_settings_has_plugin_support ())
    {
    gchar* cache;
    #ifdef GDK_WINDOWING_X11
    cache = g_build_filename (g_get_home_dir (), ".macromedia", "Flash_Player", NULL);
    midori_paths_remove_path (cache);
    g_free (cache);
    #elif defined(GDK_WINDOWING_WIN32)
    cache = g_build_filename (g_get_user_data_dir (), "Macromedia", "Flash Player", NULL);
    midori_paths_remove_path (cache);
    g_free (cache);
    #elif defined(GDK_WINDOWING_QUARTZ)
    cache = g_build_filename (g_get_home_dir (), "Library", "Preferences",
                              "Macromedia", "Flash Player", NULL);
    midori_paths_remove_path (cache);
    g_free (cache);
    #endif
    }

#ifdef HAVE_WEBKIT2
    /* TODO: clear databases and offline app caches */
#else
    /* HTML5 databases */
    webkit_remove_all_web_databases ();

    /* HTML5 offline application caches */
    /* Changing the size implies clearing the cache */
    webkit_application_cache_set_maximum_size (
        webkit_application_cache_get_maximum_size () - 1);
#endif
}

static void
midori_clear_saved_logins_cb (void)
{
    sqlite3* db;
    gchar* filename = midori_paths_get_config_filename_for_writing ("logins");
    g_unlink (filename);
    /* Form History database, written by the extension */
    gchar* path = midori_paths_get_extension_config_dir ("formhistory");
    katze_assign (filename, g_build_filename (path, "forms.db", NULL));
    g_free (path);
    if (sqlite3_open (filename, &db) == SQLITE_OK)
    {
        sqlite3_exec (db, "DELETE FROM forms", NULL, NULL, NULL);
        sqlite3_close (db);
    }
    g_free (filename);
}

static void
midori_clear_web_cache_cb (void)
{
#ifdef HAVE_WEBKIT2
    webkit_web_context_clear_cache (webkit_web_context_get_default ());
#else
    SoupSession* session = webkit_get_default_session ();
    SoupSessionFeature* feature = soup_session_get_feature (session, SOUP_TYPE_CACHE);
    gchar* cache = g_build_filename (midori_paths_get_cache_dir (), "web", NULL);
    soup_cache_clear (SOUP_CACHE (feature));
    soup_cache_flush (SOUP_CACHE (feature));
    midori_paths_remove_path (cache);
    g_free (cache);
#endif
}

void
midori_private_data_register_built_ins ()
{
    /* i18n: Logins and passwords in websites and web forms */
    midori_private_data_register_item ("formhistory", _("Saved logins and _passwords"),
        G_CALLBACK (midori_clear_saved_logins_cb));
    midori_private_data_register_item ("web-cookies", _("Cookies and Website data"),
        G_CALLBACK (midori_clear_web_cookies_cb));
    /* TODO: Preserve page icons of search engines and merge privacy items */
    midori_private_data_register_item ("web-cache", _("Web Cache"),
        G_CALLBACK (midori_clear_web_cache_cb));
    midori_private_data_register_item ("page-icons", _("Website icons"),
        G_CALLBACK (midori_paths_clear_icons));
}

void
midori_private_data_clear_all (MidoriBrowser* browser)
{
    KatzeArray* history = katze_object_get_object (browser, "history");
    KatzeArray* trash = katze_object_get_object (browser, "trash");
    GList* data_items = midori_private_data_register_item (NULL, NULL, NULL);
    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
        midori_browser_close_tab (browser, tabs->data);
    g_list_free (tabs);
    if (history != NULL)
        katze_array_clear (history);
    if (trash != NULL)
        katze_array_clear (trash);
    g_object_unref (history);
    g_object_unref (trash);

    for (; data_items != NULL; data_items = g_list_next (data_items))
        ((MidoriPrivateDataItem*)(data_items->data))->clear ();
}

void
midori_private_data_on_quit (MidoriWebSettings* settings)
{
    gint clear_prefs = MIDORI_CLEAR_NONE;
    g_object_get (settings, "clear-private-data", &clear_prefs, NULL);
    if (clear_prefs & MIDORI_CLEAR_ON_QUIT)
    {
        GList* data_items = midori_private_data_register_item (NULL, NULL, NULL);
        gchar* clear_data = katze_object_get_string (settings, "clear-data");

        midori_remove_config_file (clear_prefs, MIDORI_CLEAR_SESSION, "session.xbel");
        midori_remove_config_file (clear_prefs, MIDORI_CLEAR_HISTORY, "history.db");
        midori_remove_config_file (clear_prefs, MIDORI_CLEAR_HISTORY, "tabtrash.xbel");

        for (; data_items != NULL; data_items = g_list_next (data_items))
        {
            MidoriPrivateDataItem* privacy = data_items->data;
            if (clear_data && strstr (clear_data, privacy->name))
                privacy->clear ();
        }
        g_free (clear_data);
    }
}

/**
 * midori_private_data_register_item:
 * @name: the name of the privacy item
 * @label: a user visible, localized label
 * @clear: (scope async): a callback clearing data
 *
 * Registers an item to clear data, either via the
 * Clear Private Data dialogue or when Midori quits.
 *
 * Return value: (element-type MidoriPrivateDataItem) (transfer none):
 *     a #GList of all previously-registered items if all arguments are
 *     given as %NULL, %NULL otherwise
 **/
GList*
midori_private_data_register_item (const gchar* name,
                                   const gchar* label,
                                   GCallback    clear)
{
    static GList* items = NULL;
    MidoriPrivateDataItem* item;

    if (name == NULL && label == NULL && clear == NULL)
        return items;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (clear != NULL, NULL);

    item = g_new (MidoriPrivateDataItem, 1);
    item->name = g_strdup (name);
    item->label = g_strdup (label);
    item->clear = clear;
    items = g_list_append (items, item);
    return NULL;
}
