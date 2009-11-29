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

#define HAVE_WEBKIT_RESOURCE_REQUEST WEBKIT_CHECK_VERSION (1, 1, 14)

#define ADBLOCK_FILTER_VALID(__filter) \
    (__filter && (g_str_has_prefix (__filter, "http") \
               || g_str_has_prefix (__filter, "file")))

typedef struct
{
    const gchar* page_uri;
    const gchar* uri;
    const gchar* query;
} Matcher;

static GHashTable* pattern;
static gchar* blockcss = NULL;
static gchar* blockcssprivate = NULL;
static gchar* blockscript = NULL;

static void
adblock_parse_file (gchar* path);

static gchar*
adblock_build_js (const gchar* style,
                  const gchar* private)
{
    return g_strdup_printf (
        "window.addEventListener ('DOMContentLoaded',"
        "function () {"
        "   var URL = location.href;"
        "   var sites = new Array(); %s;"
        "   var public = '%s';"
        "   for (var i in sites) {"
        "       if (URL.indexOf(i) != -1) {"
        "           public += sites[i];"
        "           break;"
        "   }}"
        "   public += ' {display: none !important;}';"
        "   var mystyle = document.createElement('style');"
        "   mystyle.setAttribute('type', 'text/css');"
        "   mystyle.appendChild(document.createTextNode(public));"
        "   var head = document.getElementsByTagName('head')[0];"
        "   if (head) head.appendChild(mystyle);"
        "}, true);",
        private,
        style);
}

static gchar *
adblock_fixup_regexp (gchar* src)
{
    gchar* dst;
    gchar* s;

    if (!(src && *src))
        return g_strdup ("");

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
        /* FIXME: We actually need to match :[0-9]+ or '/'. Sign means
           "here could be port number or nothing". So bla.com^ will match
           bla.com/ or bla.com:8080/ but not bla.com.au/ */
        case '^':
            *src = '?';
            break;
        }
        *s++ = *src;
        src++;
    }
    *s = 0;
    return dst;
}

static void
adblock_download_notify_status_cb (WebKitDownload* download,
                                   GParamSpec*     pspec,
                                   gchar*          path)
{
    if (!g_file_test (path, G_FILE_TEST_EXISTS))
       return;
    adblock_parse_file (path);
    g_free (path);
    /* g_object_unref (download); */
}

static void
adblock_reload_rules (MidoriExtension* extension)
{
    gchar** filters;
    gchar* folder;
    guint i = 0;
    filters = midori_extension_get_string_list (extension, "filters", NULL);
    folder = g_build_filename (g_get_user_cache_dir (), PACKAGE_NAME,
                               "adblock", NULL);
    g_mkdir_with_parents (folder, 0700);

    if (!filters)
        return;

    pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_regex_unref);
    katze_assign (blockcss, g_strdup ("z-non-exist"));
    katze_assign (blockcssprivate, g_strdup (""));

    while (filters[i++] != NULL)
    {
        gchar* filename = g_compute_checksum_for_string (G_CHECKSUM_MD5,
                                                         filters[i - 1], -1);
        gchar* path = g_build_filename (folder, filename, NULL);
        if (!g_file_test (path, G_FILE_TEST_EXISTS))
        {
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
        }
        else
        {
            adblock_parse_file (path);
            g_free (path);
        }
        g_free (filename);
    }
    katze_assign (blockscript, adblock_build_js (blockcss, blockcssprivate));
    g_strfreev (filters);
    g_free (folder);
}

static void
adblock_browser_populate_tool_menu_cb (MidoriBrowser*   browser,
                                       GtkWidget*       menu,
                                       MidoriExtension* extension);
static void
adblock_preferences_render_tick_cb (GtkTreeViewColumn* column,
                                    GtkCellRenderer*   renderer,
                                    GtkTreeModel*      model,
                                    GtkTreeIter*       iter,
                                    MidoriExtension*   extension)
{
    gchar* filter;

    gtk_tree_model_get (model, iter, 0, &filter, -1);

    g_object_set (renderer,
        "activatable", ADBLOCK_FILTER_VALID (filter),
        "active", ADBLOCK_FILTER_VALID (filter) && filter[4] != '-',
        NULL);

    g_free (filter);
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
adblock_preferences_renderer_toggle_toggled_cb (GtkTreeViewColumn* column,
                                                const gchar*       path,
                                                GtkTreeModel*      model)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        gchar* filter;

        gtk_tree_model_get (model, &iter, 0, &filter, -1);

        if (ADBLOCK_FILTER_VALID (filter))
        {
            filter[4] = filter[4] != '-' ? '-' : ':';

            gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, filter, -1);

            g_free (filter);
        }
    }
}

static void
adblock_preferences_render_text_cb (GtkTreeViewColumn* column,
                                    GtkCellRenderer*   renderer,
                                    GtkTreeModel*      model,
                                    GtkTreeIter*       iter,
                                    MidoriExtension*   extension)
{
    gchar* filter;

    gtk_tree_model_get (model, iter, 0, &filter, -1);

    if (ADBLOCK_FILTER_VALID (filter))
        filter[4] = ':';

    g_object_set (renderer,
        "text", filter,
        NULL);

    g_free (filter);
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
    gboolean need_reload = FALSE;

    if (gtk_tree_model_iter_children (model, iter, NULL))
        do
        {
            gchar* filter;
            gtk_tree_model_get (model, iter, 0, &filter, -1);
            if (filter && *filter)
            {
                filters[i++] = filter;
                need_reload = TRUE;
            }
            else
                g_free (filter);
        }
        while (gtk_tree_model_iter_next (model, iter));
    filters[i] = NULL;
    midori_extension_set_string_list (extension, "filters", filters, i);
    g_free (filters);
    if (need_reload)
        adblock_reload_rules (extension);
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
    GtkEntry* entry = g_object_get_data (G_OBJECT (button), "entry");
    gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
        NULL, 0, 0, gtk_entry_get_text (entry), -1);
    gtk_entry_set_text (entry, "");
}

static void
adblock_preferences_edit_clicked_cb (GtkWidget*         button,
                                     GtkTreeViewColumn* column)
{
    GdkEvent* event = gtk_get_current_event ();
    GtkTreeView* treeview = g_object_get_data (G_OBJECT (button), "treeview");
    GtkTreeModel* model;
    GtkTreeIter iter;
    if (katze_tree_view_get_selected_iter (treeview, &model, &iter))
    {
        gchar* path = gtk_tree_model_get_string_from_iter (model, &iter);
        GtkTreePath* tree_path = gtk_tree_path_new_from_string (path);
        /* gtk_cell_renderer_start_editing */
        gtk_tree_view_set_cursor (treeview, tree_path, column, TRUE);
        gtk_tree_path_free (tree_path);
        g_free (path);
    }
    gdk_event_free (event);
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

#if GTK_CHECK_VERSION (2, 18, 0)
static gboolean
adblock_activate_link_cb (GtkWidget*   label,
                          const gchar* uri)
{
    return sokoke_show_uri (gtk_widget_get_screen (label), uri, GDK_CURRENT_TIME, NULL);
}
#endif

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
    GtkCellRenderer* renderer_toggle;
    GtkWidget* scrolled;
    gchar** filters;
    GtkWidget* vbox;
    GtkWidget* button;
    gchar* description;
    GtkWidget* entry;
    #if HAVE_OSX
    GtkWidget* icon;
    #endif

    app = midori_extension_get_app (extension);
    browser = katze_object_get_object (app, "browser");

    dialog_title = _("Configure Advertisement filters");
    dialog = gtk_dialog_new_with_buttons (dialog_title, GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        #if !HAVE_OSX
        #if !HAVE_HILDON
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
        #endif
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
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 4);
    button = gtk_label_new (NULL);
    description = g_strdup_printf (_(
        "Type the address of a preconfigured filter list in the text entry "
        "and click \"Add\" to add it to the list. "
        "You can find more lists at %s."),
        #if GTK_CHECK_VERSION (2, 18, 0)
        "<a href=\"http://easylist.adblockplus.org/\">easylist.adblockplus.org</a>");
        #else
        "<u>http://easylist.adblockplus.org/</u>");
        #endif
    #if GTK_CHECK_VERSION (2, 18, 0)
    g_signal_connect (button, "activate-link",
        G_CALLBACK (adblock_activate_link_cb), NULL);
    #endif
    gtk_label_set_markup (GTK_LABEL (button), description);
    g_free (description);
    gtk_label_set_line_wrap (GTK_LABEL (button), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 4);
    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 4);
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
    renderer_toggle = gtk_cell_renderer_toggle_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer_toggle, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer_toggle,
        (GtkCellLayoutDataFunc)adblock_preferences_render_tick_cb,
        extension, NULL);
    g_signal_connect (renderer_toggle, "toggled",
        G_CALLBACK (adblock_preferences_renderer_toggle_toggled_cb), liststore);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    column = gtk_tree_view_column_new ();
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    g_object_set (renderer_text, "editable", TRUE, NULL);
    g_signal_connect (renderer_text, "edited",
        G_CALLBACK (adblock_preferences_renderer_text_edited_cb), liststore);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer_text,
        (GtkCellLayoutDataFunc)adblock_preferences_render_text_cb,
        extension, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 5);

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
    g_object_set_data (G_OBJECT (button), "entry", entry);
    g_signal_connect (button, "clicked",
        G_CALLBACK (adblock_preferences_add_clicked_cb), liststore);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    g_object_set_data (G_OBJECT (button), "treeview", treeview);
    g_signal_connect (button, "clicked",
        G_CALLBACK (adblock_preferences_edit_clicked_cb), column);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
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

static gboolean
adblock_is_matched (const gchar*  opts,
                    const GRegex* regex,
                    Matcher*      data)
{
    gchar* patt;

    patt = g_strdup (data->uri);
    /* TODO: To figure out
    if (g_regex_match_simple ("type=fulluri,", opts, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY))
        patt = g_strdup (data->uri);
    else
        patt = g_strdup (data->query);
    */
    if (g_regex_match_full (regex, patt, -1, 0, 0, NULL, NULL))
    {
        if (g_regex_match_simple (",third-party", opts,
                                G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
        {
            if (data->page_uri && g_regex_match_full (regex, data->page_uri, -1, 0, 0, NULL, NULL))
            {
                g_free (patt);
                return FALSE;
            }
        }
        /* TODO: Domain opt check */
        g_free (patt);
        return TRUE;
    }
    else
    {
        g_free (patt);
        return FALSE;
    }
}

#if HAVE_WEBKIT_RESOURCE_REQUEST
static void
adblock_resource_request_starting_cb (WebKitWebView*         web_view,
                                      WebKitWebFrame*        web_frame,
                                      WebKitWebResource*     web_resource,
                                      WebKitNetworkRequest*  request,
                                      WebKitNetworkResponse* response,
                                      GtkWidget*             image)
{
    Matcher data;
    const char *page_uri;
    const gchar* uri;
    SoupMessage* msg;
    SoupURI* soup_uri;

    uri = webkit_network_request_get_uri (request);
    if (!strncmp (uri, "data", 4) || !strncmp (uri, "file", 4))
        return;

    msg = webkit_network_request_get_message (request);
    if (!msg)
        return;

    if (msg->method && !strncmp (msg->method, "POST", 4))
        return;

    soup_uri = soup_uri_new (uri);
    if (soup_uri->query)
        data.query = g_strdup_printf ("%s?%s", soup_uri->path, soup_uri->query);
    else
        data.query = g_strdup (soup_uri->path);
    soup_uri_free (soup_uri);

    data.uri = uri;
    page_uri = webkit_web_view_get_uri (web_view);

    if (!page_uri || !strcmp (page_uri, "about:blank"))
        page_uri = uri;
    data.page_uri = page_uri;

    if (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched, &data))
    {
        #if 0
        gchar* text;

        if (gtk_widget_get_has_tooltip (image))
           text = g_strdup_printf ("%s\n%s", gtk_widget_get_tooltip_text (image), uri);
        else
           text = g_strdup_printf ("Blocked content: \n%s", uri);
        gtk_widget_set_tooltip_text (image, text);
        g_free (text);
        #endif

        webkit_network_request_set_uri (request, "about:blank");
    }
}
#else
static void
adblock_session_request_queued_cb (SoupSession* session,
                                   SoupMessage* msg)
{
    Matcher data;
    SoupURI* soup_uri;
    gchar* uri;
    gchar* page_uri;

    if (msg->method && !strncmp (msg->method, "POST", 4))
        return;

    /* FIXME: There is a crasher somewhere introduced with the refactoring */

    soup_uri = soup_message_get_uri (msg);
    uri = soup_uri_to_string (soup_uri, FALSE);
    if (soup_uri->query)
        data.query = g_strdup_printf ("%s?%s", soup_uri->path, soup_uri->query);
    else
        data.query = g_strdup (soup_uri->path);
    soup_uri_free (soup_uri);

    data.uri = uri;
    page_uri = NULL; /* FIXME */

    if (!page_uri || !strcmp (page_uri, "about:blank"))
        page_uri = uri;
    data.page_uri = page_uri;

    if (g_hash_table_find (pattern, (GHRFunc) adblock_is_matched, &data))
    {
        /* FIXME: Update image tooltip */

        soup_uri = soup_uri_new ("http://.invalid");
        soup_message_set_uri (msg, soup_uri);
        soup_uri_free (soup_uri);
    }
    g_free (uri);
}
#endif

static void
adblock_window_object_cleared_cb (GtkWidget*      web_view,
                                  WebKitWebFrame* web_frame,
                                  JSContextRef    js_context,
                                  JSObjectRef     js_window)
{
    webkit_web_view_execute_script (WEBKIT_WEB_VIEW (web_view), blockscript);
}

static void
adblock_add_tab_cb (MidoriBrowser* browser,
                    MidoriView*    view,
                    GtkWidget*     image)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    g_signal_connect (web_view, "window-object-cleared",
        G_CALLBACK (adblock_window_object_cleared_cb), 0);
    #if HAVE_WEBKIT_RESOURCE_REQUEST
    g_signal_connect (web_view, "resource-request-starting",
        G_CALLBACK (adblock_resource_request_starting_cb), image);
    #endif
}

static void
adblock_deactivate_cb (MidoriExtension* extension,
                       GtkWidget*       image);

static void
adblock_add_tab_foreach_cb (MidoriView*      view,
                            MidoriBrowser*   browser,
                            GtkWidget*       image)
{
    adblock_add_tab_cb (browser, view, image);
}

static void
adblock_app_add_browser_cb (MidoriApp*       app,
                            MidoriBrowser*   browser,
                            MidoriExtension* extension)
{
    GtkWidget* statusbar;
    GtkWidget* image;

    statusbar = katze_object_get_object (browser, "statusbar");
    #if 0
    image = gtk_image_new_from_stock (STOCK_IMAGE, GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);
    gtk_box_pack_start (GTK_BOX (statusbar), image, FALSE, FALSE, 3);
    #else
    image = GTK_WIDGET (browser);
    #endif

    midori_browser_foreach (browser,
          (GtkCallback)adblock_add_tab_foreach_cb, image);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (adblock_add_tab_cb), image);
    g_signal_connect (browser, "populate-tool-menu",
        G_CALLBACK (adblock_browser_populate_tool_menu_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (adblock_deactivate_cb), image);
    g_object_unref (statusbar);
}

static void
adblock_compile_regexp (GHashTable* tbl,
                       gchar*      patt,
                       gchar*      opts)
{
    GRegex* regex;
    GError* error = NULL;

    /* TODO: Play with optimization flags */
    regex = g_regex_new (patt, G_REGEX_OPTIMIZE,
                         G_REGEX_MATCH_NOTEMPTY, &error);

    if (!error)
        g_hash_table_insert (tbl, opts, regex);
    else
    {
        g_warning ("%s: %s", G_STRFUNC, error->message);
        g_error_free (error);
    }
}

static void
adblock_add_url_pattern (gchar* format,
                         gchar* type,
                         gchar* line)
{
    gchar** data;
    gchar* patt;
    gchar* fixed_patt;
    gchar* format_patt;
    gchar* opts;

    data = g_strsplit (line, "$", -1);
    if (data && data[0] && data[1] && data[2])
    {
        patt = g_strdup_printf ("%s%s", data[0], data[1]);
        opts = g_strdup_printf ("type=%s,regexp=%s,%s", type, patt, data[2]);
    }
    else if (data && data[0] && data[1])
    {
        patt = g_strdup (data[0]);
        opts = g_strdup_printf ("type=%s,regexp=%s,%s", type, patt, data[1]);
    }
    else
    {
        patt = g_strdup (data[0]);
        opts = g_strdup_printf ("type=%s,regexp=%s", type, patt);
    }

    fixed_patt = adblock_fixup_regexp (patt);
    format_patt =  g_strdup_printf (format, fixed_patt);

    /* g_debug ("got: %s opts %s", format_patt, opts); */
    adblock_compile_regexp (pattern, format_patt, opts);

    g_strfreev (data);
    g_free (patt);
    g_free (fixed_patt);
    g_free (format_patt);
    g_free (opts);
}

static void
adblock_frame_add (gchar* line)
{
    gchar* new_blockcss;

    (void)*line++;
    (void)*line++;
    new_blockcss = g_strdup_printf ("%s, %s", blockcss, line);
    katze_assign (blockcss, new_blockcss);
}

static void
adblock_frame_add_private (const gchar* line,
                           const gchar* sep)
{
    gchar* new_blockcss;
    gchar** data;
    data = g_strsplit (line, sep, 2);

    if (strstr (data[0],","))
    {
        gchar** domains;
        gint max, i;

        domains = g_strsplit (data[0], ",", -1);
        for (max = i = 0; domains[i]; i++)
        {
            new_blockcss = g_strdup_printf ("%s;\nsites['%s']+=',%s'",
                blockcssprivate, g_strstrip (domains[i]), data[1]);
            katze_assign (blockcssprivate, new_blockcss);
        }
        g_strfreev (domains);
    }
    else
    {
        new_blockcss = g_strdup_printf ("%s;\nsites['%s']+=',%s'",
            blockcssprivate, data[0], data[1]);
        katze_assign (blockcssprivate, new_blockcss);
    }
    g_strfreev (data);
}

static gchar*
adblock_parse_line (gchar* line)
{
    if (!line)
        return NULL;
    g_strchomp (line);
    /* Ignore comments and new lines */
    if (line[0] == '!')
        return NULL;
    /* FIXME: No support for whitelisting */
    if (line[0] == '@' && line[1] == '@')
        return NULL;
    /* FIXME: No support for [include] and [exclude] tags */
    if (line[0] == '[')
        return NULL;

    /* Got CSS block hider */
    if (line[0] == '#' && line[1] == '#' )
    {
        adblock_frame_add (line);
        return NULL;
    }
    /* Got CSS block hider. Workaround */
    if (line[0] == '#')
        return NULL;

    /* Got per domain CSS hider rule */
    if (strstr (line,"##"))
    {
        adblock_frame_add_private (line,"##");
        return NULL;
    }

    /* Got per domain CSS hider rule. Workaround */
    if (strstr (line,"#"))
    {
        adblock_frame_add_private (line,"#");
        return NULL;
    }
    /* Got URL blocker rule */
    if (line[0] == '|' && line[1] == '|' )
    {
        (void)*line++;
        (void)*line++;
        adblock_add_url_pattern ("^https?://([a-z0-9\\.]+)?%s", "fulluri", line);
        return NULL;
    }
    if (line[0] == '|')
    {
        (void)*line++;
        adblock_add_url_pattern ("^%s", "fulluri", line);
        return NULL;
    }
    adblock_add_url_pattern ("%s", "uri", line);
    return line;
}

static void
adblock_parse_file (gchar* path)
{
    FILE* file;
    gchar line[500];

    if ((file = g_fopen (path, "r")))
    {
        while (fgets (line, 500, file))
            adblock_parse_line (line);
        fclose (file);
    }
}

static void
adblock_deactivate_tabs (MidoriView* view,
                         GtkWidget*  image)
{
    GtkWidget* web_view = gtk_bin_get_child (GTK_BIN (view));
    MidoriBrowser* browser = midori_browser_get_for_widget (image);

    g_signal_handlers_disconnect_by_func (
       browser, adblock_add_tab_cb, 0);
    g_signal_handlers_disconnect_by_func (
       web_view, adblock_window_object_cleared_cb, 0);
    #if HAVE_WEBKIT_RESOURCE_REQUEST
    g_signal_handlers_disconnect_by_func (
       web_view, adblock_resource_request_starting_cb, image);
    #endif
    #if 0
    gtk_widget_destroy (image);
    #endif
}

static void
adblock_deactivate_cb (MidoriExtension* extension,
                       GtkWidget*       image)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (image);
    MidoriApp* app = midori_extension_get_app (extension);

    #if !HAVE_WEBKIT_RESOURCE_REQUEST
    g_signal_handlers_disconnect_matched (webkit_get_default_session (),
        G_SIGNAL_MATCH_FUNC,
        g_signal_lookup ("request-queued", SOUP_TYPE_SESSION), 0,
        NULL, adblock_session_request_queued_cb, NULL);
    #endif
    g_signal_handlers_disconnect_by_func (
        browser, adblock_browser_populate_tool_menu_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, adblock_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, adblock_app_add_browser_cb, extension);
    midori_browser_foreach (browser, (GtkCallback)adblock_deactivate_tabs, image);

    katze_assign (blockcss, NULL);
    katze_assign (blockcssprivate, NULL);
    g_hash_table_destroy (pattern);
}

static void
adblock_activate_cb (MidoriExtension* extension,
                     MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;
    guint i;
    #if !HAVE_WEBKIT_RESOURCE_REQUEST
    SoupSession* session = webkit_get_default_session ();

    g_signal_connect (session, "request-queued",
                      G_CALLBACK (adblock_session_request_queued_cb), NULL);
    #endif

    adblock_reload_rules (extension);

    browsers = katze_object_get_object (app, "browsers");
    i = 0;
    while ((browser = katze_array_get_nth_item (browsers, i++)))
        adblock_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (adblock_app_add_browser_cb), extension);

    g_object_unref (browsers);
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

    pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
              (GDestroyNotify)g_free,
              (GDestroyNotify)g_regex_unref);

    temp = g_file_open_tmp ("midori_adblock_match_test_XXXXXX", &filename, NULL);

    /* TODO: Update some tests and add new ones. */
    g_file_set_contents (filename,
        "*ads.foo.bar*\n"
        "*ads.bogus.name*\n"
        "http://ads.bla.blub/*\n"
        "http://ads.blub.boing/*",
        -1, NULL);
    adblock_parse_file (filename);

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

    close (temp);
    g_unlink (filename);

    g_hash_table_destroy (pattern);
}

static void
test_adblock_count (void)
{
    pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
              (GDestroyNotify)g_free,
              (GDestroyNotify)g_regex_unref);

        gchar* urls[6] = {
            "https://bugs.webkit.org/buglist.cgi?query_format=advanced&short_desc_type=allwordssubstr&short_desc=&long_desc_type=substring&long_desc=&bug_file_loc_type=allwordssubstr&bug_file_loc=&keywords_type=allwords&keywords=&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&emailassigned_to1=1&emailtype1=substring&email1=&emailassigned_to2=1&emailreporter2=1&emailcc2=1&emailtype2=substring&email2=&bugidtype=include&bug_id=&votes=&chfieldfrom=&chfieldto=Now&chfieldvalue=&query_based_on=gtkport&field0-0-0=keywords&type0-0-0=anywordssubstr&value0-0-0=Gtk%20Cairo%20soup&field0-0-1=short_desc&type0-0-1=anywordssubstr&value0-0-1=Gtk%20Cairo%20soup%20autoconf%20automake%20autotool&field0-0-2=component&type0-0-2=equals&value0-0-2=WebKit%20Gtk",
            "http://www.engadget.com/2009/09/24/google-hits-android-rom-modder-with-a-cease-and-desist-letter/",
            "http://karibik-invest.com/es/bienes_raices/search.php?sqT=19&sqN=&sqMp=&sqL=0&qR=1&sqMb=&searchMode=1&action=B%FAsqueda",
            "http://google.com",
            "http://ya.ru",
            "http://google.com"
        };
        /* FIXME */
        gchar* filename = "/home/avb/.cache/midori/adblock/bb6cd38a4579b3605946b1228fa65297";
        gdouble elapsed = 0.0;
        gchar* str;
        int i;
        adblock_parse_file (filename);
        for (i = 0; i < 6; i++)
        {
            str = urls[i];
            g_test_timer_start ();
            g_hash_table_find (pattern, (GHRFunc) adblock_is_matched,str);
            elapsed += g_test_timer_elapsed ();
        }
        g_print ("Search took %f seconds\n", elapsed);

    g_hash_table_destroy (pattern);
}

void
extension_test (void)
{
    g_test_add_func ("/extensions/adblock/parse", test_adblock_parse);
    g_test_add_func ("/extensions/adblock/pattern", test_adblock_pattern);
    g_test_add_func ("/extensions/adblock/count", test_adblock_count);
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
