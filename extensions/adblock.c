/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <midori/midori.h>

#include <midori/sokoke.h>
#include "config.h"

#include <glib/gstdio.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

static gchar *
adblock_fixup_regexp (gchar* src)
{
    gchar* dst;
    gchar* s;
    /* FIXME: Avoid always allocating twice the string */
    s = dst = g_malloc (strlen (src) * 2);

    while (*src)
    {
        switch (*src)
        {
        case '*':
            *s++ = '.';
            break;
        case '.':
            *s++ = '\\';
            break;
        case '?':
            *s++ = '\\';
            break;
        case '|':
            *s++ = '\\';
            break;
        }
        *s++ = *src;
        src++;
    }
    *s = 0;
    return dst;
}

static void
adblock_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                       GtkWidget*       menu,
                                       MidoriExtension* extension);

static void
adblock_app_add_browser_cb (MidoriApp*       app,
                            MidoriBrowser*   browser,
                            MidoriExtension* extension);

static void
adblock_deactivate_cb (MidoriExtension* extension,
                       MidoriBrowser*   browser)
{
    MidoriApp* app = midori_extension_get_app (extension);

    g_signal_handlers_disconnect_by_func (
        browser, adblock_browser_populate_tool_menu_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, adblock_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, adblock_app_add_browser_cb, extension);
    /* FIXME: Disconnect session callbacks */
}

static void
adblock_preferences_renderer_text_edited_cb (GtkCellRenderer* renderer,
                                             const gchar*     tree_path,
                                             const gchar*     new_text,
                                             GtkTreeModel*    model)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_from_string (model, &iter, tree_path))
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, new_text, -1);
}

static void
adblock_preferences_model_row_changed_cb (GtkTreeModel*    model,
                                          GtkTreePath*     path,
                                          GtkTreeIter*     iter,
                                          MidoriExtension* extension)
{
    gsize length = gtk_tree_model_iter_n_children (model, NULL);
    gchar** filters = g_new (gchar*, length + 1);
    guint i = 0;

    if (gtk_tree_model_iter_children (model, iter, NULL))
        do
        {
            gchar* filter;
            gtk_tree_model_get (model, iter, 0, &filter, -1);
            filters[i++] = filter;
        }
        while (gtk_tree_model_iter_next (model, iter));
    filters[length] = NULL;
    midori_extension_set_string_list (extension, "filters", filters, length);
}

static void
adblock_preferences_model_row_deleted_cb (GtkTreeModel*    model,
                                          GtkTreePath*     path,
                                          MidoriExtension* extension)
{
    GtkTreeIter iter;
    adblock_preferences_model_row_changed_cb (model, path, &iter, extension);
}

static void
adblock_preferences_add_clicked_cb (GtkWidget*    button,
                                    GtkTreeModel* model)
{
    gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
        NULL, 0, 0, "", -1);
}

static void
adblock_preferences_remove_clicked_cb (GtkWidget*   button,
                                       GtkTreeView* treeview)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    if (katze_tree_view_get_selected_iter (treeview, &model, &iter))
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static GtkWidget*
adblock_get_preferences_dialog (MidoriExtension* extension)
{
    MidoriApp* app;
    GtkWidget* browser;
    const gchar* dialog_title;
    GtkWidget* dialog;
    gint width, height;
    GtkWidget* xfce_heading;
    GtkWidget* hbox;
    GtkListStore* liststore;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkWidget* scrolled;
    gchar** filters;
    GtkWidget* vbox;
    GtkWidget* button;
    #if HAVE_OSX
    GtkWidget* icon;
    #endif

    app = midori_extension_get_app (extension);
    browser = katze_object_get_object (app, "browser");

    dialog_title = _("Configure Advertisement filters");
    dialog = gtk_dialog_new_with_buttons (dialog_title, GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        #if !HAVE_OSX
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        #endif
        NULL);
    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &dialog);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_PROPERTIES);
    /* TODO: Implement some kind of help function */
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_HELP, FALSE);
    sokoke_widget_get_text_size (dialog, "M", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width * 52, -1);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (gtk_widget_destroy), dialog);
    /* TODO: We need mnemonics */
    if ((xfce_heading = sokoke_xfce_header_new (
        gtk_window_get_icon_name (GTK_WINDOW (dialog)), dialog_title)))
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                            xfce_heading, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                                 TRUE, TRUE, 12);
    liststore = gtk_list_store_new (1, G_TYPE_STRING);
    g_object_connect (liststore,
        "signal::row-inserted",
        adblock_preferences_model_row_changed_cb, extension,
        "signal::row-changed",
        adblock_preferences_model_row_changed_cb, extension,
        "signal::row-deleted",
        adblock_preferences_model_row_deleted_cb, extension,
        NULL);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (liststore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer_text,
        "text", 0, NULL);
    g_object_set (renderer_text, "editable", TRUE, NULL);
    g_signal_connect (renderer_text, "edited",
        G_CALLBACK (adblock_preferences_renderer_text_edited_cb), liststore);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), scrolled, TRUE, TRUE, 5);

    filters = midori_extension_get_string_list (extension, "filters", NULL);
    if (filters != NULL)
    {
        gsize i = 0;
        while (filters[i++] != NULL)
            gtk_list_store_insert_with_values (GTK_LIST_STORE (liststore),
                                               NULL, i - 1, 0, filters[i -1], -1);
    }
    g_strfreev (filters);

    g_object_unref (liststore);
    vbox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 4);
    button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect (button, "clicked",
        G_CALLBACK (adblock_preferences_add_clicked_cb), liststore);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_widget_set_sensitive (button, FALSE);
    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    g_signal_connect (button, "clicked",
        G_CALLBACK (adblock_preferences_remove_clicked_cb), treeview);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_label_new (""); /* This is an invisible separator */
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 8);
    gtk_widget_set_sensitive (button, FALSE);
    button = gtk_label_new (""); /* This is an invisible separator */
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 12);
    button = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

    #if HAVE_OSX
    hbox = gtk_hbox_new (FALSE, 0);
    button = gtk_button_new ();
    icon = gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    /* TODO: Implement some kind of help function */
    gtk_widget_set_sensitive (button, FALSE);
    /* g_signal_connect (button, "clicked",
        G_CALLBACK (adblock_preferences_help_clicked_cb), dialog); */
    gtk_box_pack_end (GTK_BOX (hbox),
        button, FALSE, FALSE, 4);
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
        hbox, FALSE, FALSE, 0);
    #endif
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    g_object_unref (browser);

    return dialog;
}

static void
adblock_menu_configure_filters_activate_cb (GtkWidget*       menuitem,
                                            MidoriExtension* extension)
{
    static GtkWidget* dialog = NULL;

    if (!dialog)
    {
        dialog = adblock_get_preferences_dialog (extension);
        g_signal_connect (dialog, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &dialog);
        gtk_widget_show (dialog);
    }
    else
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
adblock_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                       GtkWidget*       menu,
                                       MidoriExtension* extension)
{
    GtkWidget* menuitem;

    menuitem = gtk_menu_item_new_with_mnemonic (_("Configure _Advertisement filters..."));
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (adblock_menu_configure_filters_activate_cb), extension);
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
adblock_app_add_browser_cb (MidoriApp*       app,
                            MidoriBrowser*   browser,
                            MidoriExtension* extension)
{
    g_signal_connect (browser, "populate-tool-menu",
        G_CALLBACK (adblock_browser_populate_tool_menu_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (adblock_deactivate_cb), browser);
}

static gboolean
adblock_is_matched (const gchar*  pattern,
                    const GRegex* regex,
                    const gchar*  uri)
{
    return g_regex_match_full (regex, uri, -1, 0, 0, NULL, NULL);
}

static void
adblock_session_request_queued_cb (SoupSession* session,
                                   SoupMessage* msg,
                                   GHashTable*  pattern)
{
    SoupURI* soup_uri = soup_message_get_uri (msg);
    gchar* uri = soup_uri ? soup_uri_to_string (soup_uri, FALSE) : g_strdup ("");
    if (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched, uri))
    {
        /* g_debug ("match! '%s'", uri); */
        /* FIXME: This leads to funny error pages if frames are blocked */
        soup_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, "adblock", 7);
    }
    g_free (uri);
}

static gchar*
adblock_parse_line (gchar* line)
{
    if (!line)
        return NULL;

    /* Ignore comments and new lines */
    if (line[0] == '!')
        return NULL;
    /* FIXME: No support for whitelisting */
    if (line[0] == '@' && line[1] == '@')
        return NULL;
    /* FIXME: Differentiate # comments from element hiding */
    /* FIXME: No support for element hiding */
    if (line[0] == '#' && line[1] == '#')
        return NULL;
    /* FIXME: No support for [include] and [exclude] tags */
    if (line[0] == '[')
        return NULL;
    g_strchomp (line);
    return adblock_fixup_regexp (line);
}

static GHashTable*
adblock_parse_file (gchar* path)
{
    FILE* file;

    if ((file = g_fopen (path, "r")))
    {
        GHashTable* pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                              (GDestroyNotify)g_free,
                              (GDestroyNotify)g_regex_unref);

        gboolean have_pattern = FALSE;
        gchar line[255];
        GRegex* regex;

        while (fgets (line, 255, file))
        {
            GError* error = NULL;
            gchar* parsed;

            parsed = adblock_parse_line (line);
            regex = g_regex_new (parsed, G_REGEX_OPTIMIZE,
                                 G_REGEX_MATCH_NOTEMPTY, &error);
            if (error)
            {
                g_warning ("%s: %s", G_STRFUNC, error->message);
                g_error_free (error);
                g_free (parsed);
            }
            else
            {
                have_pattern = TRUE;
                g_hash_table_insert (pattern, parsed, regex);
            }
        }
        fclose (file);

        if (have_pattern)
            return pattern;
    }
    /* FIXME: This should presumably be freed, but there's a possible crash
       g_free (path); */
    return NULL;
}

#if WEBKIT_CHECK_VERSION (1, 1, 3)
static void
adblock_download_notify_status_cb (WebKitDownload* download,
                                   GParamSpec*     pspec,
                                   gchar*          path)
{
    SoupSession* session = webkit_get_default_session ();
    GHashTable* pattern = adblock_parse_file (path);
    if (pattern)
        g_signal_connect_data (session, "request-queued",
                               G_CALLBACK (adblock_session_request_queued_cb),
                               pattern, (GClosureNotify)g_hash_table_destroy, 0);
    /* g_object_unref (download); */
}
#endif

static void
adblock_activate_cb (MidoriExtension* extension,
                     MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;
    gchar* folder;
    gchar** filters;
    SoupSession* session;

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        adblock_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (adblock_app_add_browser_cb), extension);
    g_object_unref (browsers);

    session = webkit_get_default_session ();
    folder = g_build_filename (g_get_user_cache_dir (), PACKAGE_NAME,
                               "adblock", NULL);
    g_mkdir_with_parents (folder, 0700);
    filters = midori_extension_get_string_list (extension, "filters", NULL);
    if (filters != NULL)
    {
        i = 0;
        while (filters[i++] != NULL)
        {
            gchar* filename = g_compute_checksum_for_string (G_CHECKSUM_MD5,
                                                             filters[i - 1], -1);
            gchar* path = g_build_filename (folder, filename, NULL);
            if (!g_file_test (path, G_FILE_TEST_EXISTS))
            {
                #if WEBKIT_CHECK_VERSION (1, 1, 3)
                WebKitNetworkRequest* request;
                WebKitDownload* download;
                gchar* destination = g_filename_to_uri (path, NULL, NULL);

                request = webkit_network_request_new (filters[i -1]);
                download = webkit_download_new (request);
                g_object_unref (request);
                webkit_download_set_destination_uri (download, destination);
                g_free (destination);
                g_signal_connect (download, "notify::status",
                    G_CALLBACK (adblock_download_notify_status_cb), path);
                webkit_download_start (download);
                #else
                /* FIXME: Is it worth to rewrite this without WebKitDownload? */
                #endif
            }
            else
            {
                GHashTable* pattern = adblock_parse_file (path);
                if (pattern)
                    g_signal_connect_data (session, "request-queued",
                        G_CALLBACK (adblock_session_request_queued_cb),
                        pattern, (GClosureNotify)g_hash_table_destroy, 0);
            }
            g_free (filename);
        }
    }
    g_strfreev (filters);
    g_free (folder);
}

#if G_ENABLE_DEBUG
static void
test_adblock_parse (void)
{
    g_assert (!adblock_parse_line (NULL));
    g_assert (!adblock_parse_line ("!"));
    g_assert (!adblock_parse_line ("@@"));
    g_assert (!adblock_parse_line ("##"));
    g_assert (!adblock_parse_line ("["));

    g_assert_cmpstr (adblock_parse_line ("*foo"), ==, ".*foo");
    g_assert_cmpstr (adblock_parse_line ("?foo"), ==, "\\?foo");
    g_assert_cmpstr (adblock_parse_line ("foo*"), ==, "foo.*");
    g_assert_cmpstr (adblock_parse_line ("foo?"), ==, "foo\\?");

    g_assert_cmpstr (adblock_parse_line (".*foo/bar"), ==, "\\..*foo/bar");
    g_assert_cmpstr (adblock_parse_line ("http://bla.blub/*"), ==, "http://bla\\.blub/.*");
}

static void
test_adblock_pattern (void)
{
    gint temp;
    gchar* filename;
    GHashTable* pattern;

    temp = g_file_open_tmp ("midori_adblock_match_test_XXXXXX", &filename, NULL);

    g_file_set_contents (filename,
        "*ads.foo.bar*\n"
        "*ads.bogus.name*\n"
        "http://ads.bla.blub/*\n"
        "http://ads.blub.boing/*",
        -1, NULL);
    pattern = adblock_parse_file (filename);

    g_assert (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,
              "http://ads.foo.bar/teddy"));
    g_assert (!g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,
              "http://ads.fuu.bar/teddy"));
    g_assert (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,
              "https://ads.bogus.name/blub"));
    g_assert (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,
              "http://ads.bla.blub/kitty"));
    g_assert (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,
              "http://ads.blub.boing/soda"));
    g_assert (!g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,
              "http://ads.foo.boing/beer"));

    g_hash_table_destroy (pattern);
    close (temp);
    g_unlink (filename);
}

void
extension_test (void)
{
    g_test_add_func ("/extensions/adblock/parse", test_adblock_parse);
    g_test_add_func ("/extensions/adblock/pattern", test_adblock_pattern);
}
#endif

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Advertisement blocker"),
        "description", _("Block advertisements according to a filter list"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);
    midori_extension_install_string_list (extension, "filters", NULL, G_MAXSIZE);

    g_signal_connect (extension, "activate",
        G_CALLBACK (adblock_activate_cb), NULL);

    return extension;
}
