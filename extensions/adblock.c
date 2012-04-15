/*
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/
#include <midori/midori.h>
#include <glib/gstdio.h>

#include "config.h"
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#define SIGNATURE_SIZE 8
#define USE_PATTERN_MATCHING 1
#define CUSTOM_LIST_NAME "custom.list"
#define ADBLOCK_FILTER_VALID(__filter) \
    (__filter && (g_str_has_prefix (__filter, "http") \
               || g_str_has_prefix (__filter, "file")))
#define ADBLOCK_FILTER_SET(__filter,__active) \
    __filter[4] = __active ? (__filter[5] == ':' ? 's' : ':') : '-'
#define ADBLOCK_FILTER_IS_SET(__filter) \
    (__filter[4] != '-' && __filter[5] != '-')
#ifdef G_ENABLE_DEBUG
    #define adblock_debug(dmsg, darg1, darg2) \
        do { if (debug == 1) g_debug (dmsg, darg1, darg2); } while (0)
#else
    #define adblock_debug(dmsg, darg1, darg2) /* nothing */
#endif

static GHashTable* pattern = NULL;
static GHashTable* keys = NULL;
static GHashTable* optslist = NULL;
static GHashTable* urlcache = NULL;
static GHashTable* blockcssprivate = NULL;
static GHashTable* navigationwhitelist = NULL;
static GString* blockcss = NULL;
#ifdef G_ENABLE_DEBUG
static guint debug;
#endif

static gboolean
adblock_parse_file (gchar* path);

static void
adblock_reload_rules (MidoriExtension* extension,
                      gboolean         custom_only);

static gchar*
adblock_build_js (const gchar* uri)
{
    gchar* domain;
    const gchar* style;
    GString* subdomain;
    GString* code;
    int cnt = 0, blockscnt = 0;
    gchar** subdomains;

    domain = midori_uri_parse_hostname (uri, NULL);
    subdomains = g_strsplit (domain, ".", -1);
    g_free (domain);
    if (!subdomains)
        return NULL;

    code = g_string_new (
        "window.addEventListener ('DOMContentLoaded',"
        "function () {"
        "   if (document.getElementById('madblock'))"
        "       return;"
        "   public = '");

    cnt = g_strv_length (subdomains) - 1;
    subdomain = g_string_new (subdomains [cnt]);
    g_string_prepend_c (subdomain, '.');
    cnt--;
    while (cnt >= 0)
    {
        g_string_prepend (subdomain, subdomains[cnt]);
        if ((style = g_hash_table_lookup (blockcssprivate, subdomain->str)))
        {
            g_string_append (code, style);
            g_string_append_c (code, ',');
            blockscnt++;
        }
        g_string_prepend_c (subdomain, '.');
        cnt--;
    }
    g_string_free (subdomain, TRUE);
    g_strfreev (subdomains);

    if (blockscnt == 0)
        return g_string_free (code, TRUE);

    g_string_append (code,
        "   zz-non-existent {display: none !important}';"
        "   var mystyle = document.createElement('style');"
        "   mystyle.setAttribute('type', 'text/css');"
        "   mystyle.setAttribute('id', 'madblock');"
        "   mystyle.appendChild(document.createTextNode(public));"
        "   var head = document.getElementsByTagName('head')[0];"
        "   if (head) head.appendChild(mystyle);"
        "}, true);");
    return g_string_free (code, FALSE);
}

static GString*
adblock_fixup_regexp (const gchar* prefix,
                      gchar*       src);

static void
adblock_destroy_db ()
{
    if (blockcss)
        g_string_free (blockcss, TRUE);
    blockcss = NULL;

    g_hash_table_destroy (pattern);
    pattern = NULL;
    g_hash_table_destroy (optslist);
    optslist = NULL;
    g_hash_table_destroy (urlcache);
    urlcache = NULL;
    g_hash_table_destroy (blockcssprivate);
    blockcssprivate = NULL;
    g_hash_table_destroy (navigationwhitelist);
    navigationwhitelist = NULL;
}

static void
adblock_init_db ()
{
    pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_regex_unref);
    keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_regex_unref);
    optslist = g_hash_table_new_full (g_str_hash, g_str_equal,
                   NULL,
                   (GDestroyNotify)g_free);
    urlcache = g_hash_table_new_full (g_str_hash, g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_free);
    blockcssprivate = g_hash_table_new_full (g_str_hash, g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_free);
    navigationwhitelist = g_hash_table_new_full (g_direct_hash, g_str_equal,
                   NULL,
                   (GDestroyNotify)g_free);

    if (blockcss && blockcss->len > 0)
        g_string_free (blockcss, TRUE);
    blockcss = g_string_new ("z-non-exist");
}

static void
adblock_download_notify_status_cb (WebKitDownload*  download,
                                   GParamSpec*      pspec,
                                   MidoriExtension* extension)
{
    if (webkit_download_get_status (download) != WEBKIT_DOWNLOAD_STATUS_FINISHED)
        return;
    adblock_reload_rules (extension, FALSE);
}

static gchar*
adblock_get_filename_for_uri (const gchar* uri)
{
    gchar* filename;
    gchar* folder;
    gchar* path;

    if (!ADBLOCK_FILTER_IS_SET (uri))
        return NULL;

    if (!strncmp (uri, "file", 4))
        return g_strndup (uri + 7, strlen (uri) - 7);

    folder = g_build_filename (g_get_user_cache_dir (), PACKAGE_NAME,
                               "adblock", NULL);
    katze_mkdir_with_parents (folder, 0700);

    filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    path = g_build_filename (folder, filename, NULL);

    g_free (filename);
    g_free (folder);
    return path;
}

static void
adblock_reload_rules (MidoriExtension* extension,
                      gboolean         custom_only)
{
    gchar* path;
    gchar* custom_list;
    gchar** filters;
    guint i = 0;
    MidoriApp* app = midori_extension_get_app (extension);
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");

    if (pattern)
        adblock_destroy_db ();
    adblock_init_db ();

    custom_list = g_build_filename (midori_extension_get_config_dir (extension),
                                    CUSTOM_LIST_NAME, NULL);
    adblock_parse_file (custom_list);
    g_free (custom_list);

    filters = midori_extension_get_string_list (extension, "filters", NULL);
    if (!custom_only && filters && *filters)
    {
        while (filters[i] != NULL)
        {
            path = adblock_get_filename_for_uri (filters[i]);
            if (!path)
            {
                i++;
                continue;
            }

            if (!adblock_parse_file (path))
            {
                WebKitNetworkRequest* request;
                WebKitDownload* download;
                gchar* destination = g_filename_to_uri (path, NULL, NULL);

                request = webkit_network_request_new (filters[i]);
                download = webkit_download_new (request);
                g_object_unref (request);
                webkit_download_set_destination_uri (download, destination);
                g_free (destination);
                g_signal_connect (download, "notify::status",
                    G_CALLBACK (adblock_download_notify_status_cb), extension);
                webkit_download_start (download);
            }
            g_free (path);
            i++;
        }
    }
    g_strfreev (filters);
    g_string_append (blockcss, " {display: none !important}\n");

    midori_web_settings_add_style (settings, "adblock-blockcss", blockcss->str);
    g_object_unref (settings);
}

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
        "active", ADBLOCK_FILTER_VALID (filter) && ADBLOCK_FILTER_IS_SET (filter),
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
adblock_preferences_renderer_toggle_toggled_cb (GtkCellRendererToggle* renderer,
                                                const gchar*           path,
                                                GtkTreeModel*          model)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        gchar* filter;

        gtk_tree_model_get (model, &iter, 0, &filter, -1);

        if (ADBLOCK_FILTER_VALID (filter))
        {
            ADBLOCK_FILTER_SET (filter, TRUE);
            if (gtk_cell_renderer_toggle_get_active (renderer))
            {
                if (midori_uri_is_http (filter))
                {
                    gchar* filename = adblock_get_filename_for_uri (filter);
                    g_unlink (filename);
                    g_free (filename);
                }
                ADBLOCK_FILTER_SET (filter, FALSE);
            }

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
        ADBLOCK_FILTER_SET (filter, TRUE);

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
        adblock_reload_rules (extension, FALSE);
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
    GtkEntry* entry = GTK_IS_ENTRY (button)
        ? button : g_object_get_data (G_OBJECT (button), "entry");
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
    MidoriBrowser* browser = midori_browser_get_for_widget (label);
    gint n = midori_browser_add_uri (browser, uri);
    if (n > -1)
        midori_browser_set_current_page (browser, n);
    return n > -1;
}
#endif

static void
adblock_preferences_response_cb (GtkWidget* dialog,
                                 gint       response,
                                 gpointer   data)
{
    gtk_widget_destroy (dialog);
}

static GtkWidget*
adblock_get_preferences_dialog (MidoriExtension* extension)
{
    MidoriApp* app;
    GtkWidget* browser;
    const gchar* dialog_title;
    GtkWidget* dialog;
    GtkWidget* content_area;
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
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &dialog);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_PROPERTIES);
    /* TODO: Implement some kind of help function */
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_HELP, FALSE);
    sokoke_widget_get_text_size (dialog, "M", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width * 52, -1);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (adblock_preferences_response_cb), NULL);
    /* TODO: We need mnemonics */
    if ((xfce_heading = sokoke_xfce_header_new (
        gtk_window_get_icon_name (GTK_WINDOW (dialog)), dialog_title)))
        gtk_box_pack_start (GTK_BOX (content_area), xfce_heading, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 12);
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
    entry = katze_uri_entry_new (NULL);
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 4);
    liststore = gtk_list_store_new (1, G_TYPE_STRING);
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
    g_object_connect (liststore,
        "signal::row-inserted",
        adblock_preferences_model_row_changed_cb, extension,
        "signal::row-changed",
        adblock_preferences_model_row_changed_cb, extension,
        "signal::row-deleted",
        adblock_preferences_model_row_deleted_cb, extension,
        NULL);

    g_object_unref (liststore);
    vbox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 4);
    button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_object_set_data (G_OBJECT (button), "entry", entry);
    g_signal_connect (button, "clicked",
        G_CALLBACK (adblock_preferences_add_clicked_cb), liststore);
    g_signal_connect (entry, "activate",
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
    gtk_box_pack_end (GTK_BOX (content_area),
        hbox, FALSE, FALSE, 0);
    #endif
    gtk_widget_show_all (content_area);

    g_object_unref (browser);

    return dialog;
}

static void
adblock_open_preferences_cb (MidoriExtension* extension)
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

static inline gint
adblock_check_rule (GRegex*      regex,
                    const gchar* patt,
                    const gchar* req_uri,
                    const gchar* page_uri)
{
    gchar* opts;

    if (!g_regex_match_full (regex, req_uri, -1, 0, 0, NULL, NULL))
        return FALSE;

    opts = g_hash_table_lookup (optslist, patt);
    if (opts && g_regex_match_simple (",third-party", opts,
                              G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
        if (page_uri && g_regex_match_full (regex, page_uri, -1, 0, 0, NULL, NULL))
            return FALSE;
    }
    /* TODO: Domain opt check */
    #ifdef G_ENABLE_DEBUG
    adblock_debug ("blocked by pattern regexp=%s -- %s", g_regex_get_pattern (regex), req_uri);
    #endif
    return TRUE;
}

static inline gboolean
adblock_is_matched_by_pattern (const gchar* req_uri,
                               const gchar* page_uri)
{
    GHashTableIter iter;
    gpointer patt, regex;

    if (USE_PATTERN_MATCHING == 0)
        return FALSE;

    g_hash_table_iter_init (&iter, pattern);
    while (g_hash_table_iter_next (&iter, &patt, &regex))
    {
        if (adblock_check_rule (regex, patt, req_uri, page_uri))
            return TRUE;
    }
    return FALSE;
}

static inline gboolean
adblock_is_matched_by_key (const gchar* req_uri,
                           const gchar* page_uri)
{
    gchar* uri;
    gint len;
    int pos = 0;
    GList* regex_bl = NULL;
    GString* guri;
    gboolean ret = FALSE;
    gchar sig[SIGNATURE_SIZE + 1];

    memset (&sig[0], 0, sizeof (sig));
    /* Signatures are made on pattern, so we need to convert url to a pattern as well */
    guri = adblock_fixup_regexp ("", (gchar*)req_uri);
    uri = guri->str;
    len = guri->len;

    for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--)
    {
        GRegex* regex;
        strncpy (sig, uri + pos, SIGNATURE_SIZE);
        regex = g_hash_table_lookup (keys, sig);

        /* Dont check if regex is already blacklisted */
        if (!regex || g_list_find (regex_bl, regex))
            continue;
        ret = adblock_check_rule (regex, sig, req_uri, page_uri);
        if (ret)
            break;
        regex_bl = g_list_prepend (regex_bl, regex);
    }
    g_string_free (guri, TRUE);
    g_list_free (regex_bl);
    return ret;
}

static gboolean
adblock_is_matched (const gchar*  req_uri,
                    const gchar*  page_uri)
{
    gchar* value;

    if ((value = g_hash_table_lookup (urlcache, req_uri)))
    {
        if (value[0] == '0')
            return FALSE;
        else
            return TRUE;
    }

    if (adblock_is_matched_by_key (req_uri, page_uri))
    {
        g_hash_table_insert (urlcache, g_strdup (req_uri), g_strdup("1"));
        return TRUE;
    }

    if (adblock_is_matched_by_pattern (req_uri, page_uri))
    {
        g_hash_table_insert (urlcache, g_strdup (req_uri), g_strdup("1"));
        return TRUE;
    }
    g_hash_table_insert (urlcache, g_strdup (req_uri), g_strdup("0"));
    return FALSE;
}

static gchar*
adblock_prepare_urihider_js (GList* uris)
{
    GList* li = NULL;
    GString* js = g_string_new (
        "(function() {"
        "function getElementsByAttribute (strTagName, strAttributeName, arrAttributeValue) {"
        "    var arrElements = document.getElementsByTagName (strTagName);"
        "    var arrReturnElements = new Array();"
        "    for (var j=0; j<arrAttributeValue.length; j++) {"
        "        var strAttributeValue = arrAttributeValue[j];"
        "        for (var i=0; i<arrElements.length; i++) {"
        "             var oCurrent = arrElements[i];"
        "             var oAttribute = oCurrent.getAttribute && oCurrent.getAttribute (strAttributeName);"
        "             if (oAttribute && oAttribute.length > 0 && strAttributeValue.indexOf (oAttribute) != -1)"
        "                 arrReturnElements.push (oCurrent);"
        "        }"
        "    }"
        "    return arrReturnElements;"
        "};"
        "function hideElementBySrc (uris) {"
        "    var oElements = getElementsByAttribute('img', 'src', uris);"
        "    if (oElements.length == 0)"
        "        oElements = getElementsByAttribute ('iframe', 'src', uris);"
        "    for (var i=0; i<oElements.length; i++) {"
        "        oElements[i].style.visibility = 'hidden !important';"
        "        oElements[i].style.width = '0';"
        "        oElements[i].style.height = '0';"
        "    }"
        "};"
        "var uris=new Array ();");

    for (li = uris; li != NULL; li = g_list_next (li))
        g_string_append_printf (js, "uris.push ('%s');", (gchar*)li->data);

    g_string_append (js, "hideElementBySrc (uris);})();");

    return g_string_free (js, FALSE);
}

static gboolean
adblock_navigation_policy_decision_requested_cb (WebKitWebView*             web_view,
                                                 WebKitWebFrame*            web_frame,
                                                 WebKitNetworkRequest*      request,
                                                 WebKitWebNavigationAction* action,
                                                 WebKitWebPolicyDecision*   decision,
                                                 MidoriView*                view)
{
    if (web_frame == webkit_web_view_get_main_frame (web_view))
    {
        const gchar* req_uri = webkit_network_request_get_uri (request);
        g_hash_table_replace (navigationwhitelist, web_view, g_strdup (req_uri));
    }
    return false;
}


static void
adblock_resource_request_starting_cb (WebKitWebView*         web_view,
                                      WebKitWebFrame*        web_frame,
                                      WebKitWebResource*     web_resource,
                                      WebKitNetworkRequest*  request,
                                      WebKitNetworkResponse* response,
                                      MidoriView*            view)
{
    SoupMessage* msg;
    GList* blocked_uris;
    const gchar* req_uri;
    const char *page_uri;

    page_uri = webkit_web_view_get_uri (web_view);
    /* Skip checks on about: pages */
    if (midori_uri_is_blank (page_uri))
        return;

    req_uri = webkit_network_request_get_uri (request);

    if (!g_strcmp0 (req_uri, g_hash_table_lookup (navigationwhitelist, web_view)))
        return;

    if (!midori_uri_is_http (req_uri)
     || g_str_has_suffix (req_uri, "favicon.ico"))
        return;

    msg = webkit_network_request_get_message (request);
    if (!(msg && !g_strcmp0 (msg->method, "GET")))
        return;

    if (response != NULL) /* request is caused by redirect */
    {
        if (web_frame == webkit_web_view_get_main_frame (web_view))
        {
            g_hash_table_replace (navigationwhitelist, web_view, g_strdup (req_uri));
            return;
        }
    }

    #ifdef G_ENABLE_DEBUG
    if (debug == 2)
        g_test_timer_start ();
    #endif
    if (adblock_is_matched (req_uri, page_uri))
    {
        blocked_uris = g_object_get_data (G_OBJECT (web_view), "blocked-uris");
        blocked_uris = g_list_prepend (blocked_uris, g_strdup (req_uri));
        webkit_network_request_set_uri (request, "about:blank");
        g_object_set_data (G_OBJECT (web_view), "blocked-uris", blocked_uris);
    }
    #ifdef G_ENABLE_DEBUG
    if (debug == 2)
        g_debug ("match: %f%s", g_test_timer_elapsed (), "seconds");
    #endif

}

static void
adblock_custom_block_image_cb (GtkWidget*       widget,
                               MidoriExtension* extension)
{
    gchar* custom_list;
    FILE* list;
    MidoriApp* app;
    GtkWidget* browser;
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* label;
    GtkWidget* entry;
    gchar* title;

    app = midori_extension_get_app (extension);
    browser = katze_object_get_object (app, "browser");

    title = _("Edit rule");
    dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (browser),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
            NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_ADD);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Rule:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_entry_set_text (GTK_ENTRY (entry),
                        g_object_get_data (G_OBJECT (widget), "uri"));
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (content_area), hbox);
    gtk_widget_show_all (hbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
    {
        gtk_widget_destroy (dialog);
        return;
    }

    custom_list = g_build_filename (midori_extension_get_config_dir (extension),
                                    CUSTOM_LIST_NAME, NULL);
    katze_mkdir_with_parents (midori_extension_get_config_dir (extension), 0700);
    if ((list = g_fopen (custom_list, "a+")))
    {
        g_fprintf (list, "%s\n", gtk_entry_get_text (GTK_ENTRY (entry)));
        fclose (list);
        adblock_reload_rules (extension, TRUE);
        g_debug ("%s: Updated custom list\n", G_STRFUNC);
    }
    else
        g_debug ("%s: Failed to open custom list %s\n", G_STRFUNC, custom_list);
    g_free (custom_list);
    gtk_widget_destroy (dialog);
}

static void
adblock_populate_popup_cb (WebKitWebView*   web_view,
                           GtkWidget*       menu,
                           MidoriExtension* extension)
{
    GtkWidget* menuitem;
    gchar *uri;
    gint x, y;
    GdkEventButton event;
    WebKitHitTestResultContext context;
    WebKitHitTestResult* hit_test;

    gdk_window_get_pointer (gtk_widget_get_window(GTK_WIDGET (web_view)), &x, &y, NULL);
    event.x = x;
    event.y = y;
    hit_test = webkit_web_view_get_hit_test_result (web_view, &event);
    context = katze_object_get_int (hit_test, "context");
    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE)
    {
        uri = katze_object_get_string (hit_test, "image-uri");
        menuitem = gtk_menu_item_new_with_mnemonic (_("Bl_ock image"));
    }
    else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
    {
        uri = katze_object_get_string (hit_test, "link-uri");
        menuitem = gtk_menu_item_new_with_mnemonic (_("Bl_ock link"));
    }
    else
        return;
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_object_set_data_full (G_OBJECT (menuitem), "uri", uri, (GDestroyNotify)g_free);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (adblock_custom_block_image_cb), extension);
}

static void
adblock_load_finished_cb (WebKitWebView  *web_view,
                          WebKitWebFrame *web_frame,
                          gpointer        user_data)
{
    GList* uris = g_object_get_data (G_OBJECT (web_view), "blocked-uris");
    gchar* script;
    GList* li;

    if (g_list_nth_data (uris, 0) == NULL)
        return;

    script = adblock_prepare_urihider_js (uris);
    webkit_web_view_execute_script (web_view, script);
    li = NULL;
    for (li = uris; li != NULL; li = g_list_next (li))
        uris = g_list_remove (uris, li->data);
    g_free (script);
    g_object_set_data (G_OBJECT (web_view), "blocked-uris", uris);
}

static void
adblock_window_object_cleared_cb (WebKitWebView*  web_view,
                                  WebKitWebFrame* web_frame,
                                  JSContextRef    js_context,
                                  JSObjectRef     js_window)
{
    const char *page_uri;
    gchar* script;

    page_uri = webkit_web_frame_get_uri (web_frame);
    /* Don't add adblock css into speeddial and about: pages */
    if (!midori_uri_is_http (page_uri))
        return;

    script = adblock_build_js (page_uri);
    if (!script)
        return;

    g_free (sokoke_js_script_eval (js_context, script, NULL));
    g_free (script);
}

static void
adblock_add_tab_cb (MidoriBrowser*   browser,
                    MidoriView*      view,
                    MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);

    g_signal_connect (web_view, "window-object-cleared",
        G_CALLBACK (adblock_window_object_cleared_cb), 0);

    g_signal_connect_after (web_view, "populate-popup",
        G_CALLBACK (adblock_populate_popup_cb), extension);
    g_signal_connect (web_view, "navigation-policy-decision-requested",
        G_CALLBACK (adblock_navigation_policy_decision_requested_cb), view);
    g_signal_connect (web_view, "resource-request-starting",
        G_CALLBACK (adblock_resource_request_starting_cb), view);
    g_signal_connect (web_view, "load-finished",
        G_CALLBACK (adblock_load_finished_cb), view);
}

static void
adblock_remove_tab_cb (MidoriBrowser*   browser,
                       MidoriView*      view,
                       MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);
    g_hash_table_remove (navigationwhitelist, web_view);
}

static void
adblock_deactivate_cb (MidoriExtension* extension,
                       MidoriBrowser*   browser);

static void
adblock_app_add_browser_cb (MidoriApp*       app,
                            MidoriBrowser*   browser,
                            MidoriExtension* extension)
{
    GtkWidget* statusbar;
    GtkWidget* image;
    GtkWidget* view;
    gint i;

    statusbar = katze_object_get_object (browser, "statusbar");
    image = NULL;
    /* image = gtk_image_new_from_stock (STOCK_IMAGE, GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);
    gtk_box_pack_start (GTK_BOX (statusbar), image, FALSE, FALSE, 3); */
    g_object_set_data_full (G_OBJECT (browser), "status-image", image,
                            (GDestroyNotify)gtk_widget_destroy);

    i = 0;
    while((view = midori_browser_get_nth_tab(browser, i++)))
        adblock_add_tab_cb (browser, MIDORI_VIEW (view), extension);

    g_signal_connect (browser, "add-tab",
        G_CALLBACK (adblock_add_tab_cb), extension);
    g_signal_connect (browser, "remove-tab",
        G_CALLBACK (adblock_remove_tab_cb), extension);
    g_signal_connect (extension, "open-preferences",
        G_CALLBACK (adblock_open_preferences_cb), extension);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (adblock_deactivate_cb), browser);
    g_object_unref (statusbar);
}

static GString*
adblock_fixup_regexp (const gchar* prefix,
                      gchar*       src)
{
    GString* str;
    int len = 0;

    if (!src)
        return NULL;

    str = g_string_new (prefix);

    /* lets strip first .* */
    if (src[0] == '*')
    {
        (void)*src++;
    }

    do
    {
        switch (*src)
        {
        case '*':
            g_string_append (str, ".*");
            break;
        /*case '.':
            g_string_append (str, "\\.");
            break;*/
        case '?':
            g_string_append (str, "\\?");
            break;
        case '|':
        /* FIXME: We actually need to match :[0-9]+ or '/'. Sign means
           "here could be port number or nothing". So bla.com^ will match
           bla.com/ or bla.com:8080/ but not bla.com.au/ */
        case '^':
        case '+':
            break;
        default:
            g_string_append_printf (str,"%c", *src);
            break;
        }
        src++;
    }
    while (*src);

    len = str->len;
    /* We dont need .* in the end of url. Thats stupid */
    if (str->str && str->str[len-1] == '*' && str->str[len-2] == '.')
        g_string_erase (str, len-2, 2);

    return str;
}

static gboolean
adblock_compile_regexp (GString* gpatt,
                        gchar*   opts)
{
    GRegex* regex;
    GError* error = NULL;
    int pos = 0;
    gchar *sig;
    gchar *patt;
    int len;

    if (!gpatt)
        return FALSE;

    patt = gpatt->str;
    len = gpatt->len;

    /* TODO: Play with optimization flags */
    regex = g_regex_new (patt, G_REGEX_OPTIMIZE,
                         G_REGEX_MATCH_NOTEMPTY, &error);
    if (error)
    {
        g_warning ("%s: %s", G_STRFUNC, error->message);
        g_error_free (error);
        return TRUE;
    }

    if (!g_regex_match_simple ("^/.*[\\^\\$\\*].*/$", patt, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY))
    {
        int signature_count = 0;

        for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--) {
            sig = g_strndup (patt + pos, SIGNATURE_SIZE);
            if (!g_regex_match_simple ("[\\*]", sig, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY) &&
                !g_hash_table_lookup (keys, sig))
            {
                #ifdef G_ENABLE_DEBUG
                adblock_debug ("sig: %s %s", sig, patt);
                #endif
                g_hash_table_insert (keys, sig, regex);
                g_hash_table_insert (optslist, sig, g_strdup (opts));
                signature_count++;
            }
            else
            {
                if (g_regex_match_simple ("^\\*", sig, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY) &&
                    !g_hash_table_lookup (pattern, patt))
                {
                    #ifdef G_ENABLE_DEBUG
                    adblock_debug ("patt2: %s %s", sig, patt);
                    #endif
                    g_hash_table_insert (pattern, patt, regex);
                    g_hash_table_insert (optslist, patt, g_strdup (opts));
                }
                g_free (sig);
            }
        }
        if (signature_count > 1 && g_hash_table_lookup (pattern, patt))
        {
            g_hash_table_steal (pattern, patt);
            return TRUE;
        }
        return FALSE;
    }
    else
    {
        #ifdef G_ENABLE_DEBUG
        adblock_debug ("patt: %s%s", patt, "");
        #endif
        /* Pattern is a regexp chars */
        g_hash_table_insert (pattern, patt, regex);
        g_hash_table_insert (optslist, patt, g_strdup (opts));
        return FALSE;
    }
}

static inline gchar*
adblock_add_url_pattern (gchar* prefix,
                         gchar* type,
                         gchar* line)
{
    gchar** data;
    gchar* patt;
    GString* format_patt;
    gchar* opts;
    gboolean should_free;

    data = g_strsplit (line, "$", -1);
    if (!data || !data[0])
    {
        g_strfreev (data);
        return NULL;
    }

    if (data[1] && data[2])
    {
        patt = g_strconcat (data[0], data[1], NULL);
        opts = g_strconcat (type, ",", data[2], NULL);
    }
    else if (data[1])
    {
        patt = data[0];
        opts = g_strconcat (type, ",", data[1], NULL);
    }
    else
    {
        patt = data[0];
        opts = type;
    }

    if (g_regex_match_simple ("subdocument", opts,
                              G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
        if (data[1] && data[2])
            g_free (patt);
        if (data[1])
            g_free (opts);
        g_strfreev (data);
        return NULL;
    }

    format_patt = adblock_fixup_regexp (prefix, patt);

    #ifdef G_ENABLE_DEBUG
    adblock_debug ("got: %s opts %s", format_patt->str, opts);
    #endif
    should_free = adblock_compile_regexp (format_patt, opts);

    if (data[1] && data[2])
        g_free (patt);
    if (data[1])
        g_free (opts);
    g_strfreev (data);

    return g_string_free (format_patt, should_free);
}

static inline void
adblock_frame_add (gchar* line)
{
    const gchar* separator = " , ";

    (void)*line++;
    (void)*line++;
    if (strchr (line, '\'')
    || (strchr (line, ':')
    && !g_regex_match_simple (".*\\[.*:.*\\].*", line,
                              G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)))
    {
        return;
    }
    g_string_append (blockcss, separator);
    g_string_append (blockcss, line);
}

static inline void
adblock_update_css_hash (gchar* domain,
                         gchar* value)
{
    const gchar* olddata;
    gchar* newdata;

    if ((olddata = g_hash_table_lookup (blockcssprivate, domain)))
    {
        newdata = g_strconcat (olddata, " , ", value, NULL);
        g_hash_table_replace (blockcssprivate, g_strdup (domain), newdata);
    }
    else
        g_hash_table_insert (blockcssprivate, g_strdup (domain), g_strdup (value));
}

static inline void
adblock_frame_add_private (const gchar* line,
                           const gchar* sep)
{
    gchar** data;
    data = g_strsplit (line, sep, 2);

    if (!(data[1] && *data[1])
     ||  strchr (data[1], '\'')
     || (strchr (data[1], ':')
     && !g_regex_match_simple (".*\\[.*:.*\\].*", data[1],
                               G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)))
    {
        g_strfreev (data);
        return;
    }

    if (strchr (data[0], ','))
    {
        gchar** domains;
        gint i;

        domains = g_strsplit (data[0], ",", -1);
        for (i = 0; domains[i]; i++)
        {
            gchar* domain;

            domain = domains[i];
            /* Ignore Firefox-specific option */
            if (!g_strcmp0 (domain, "~pregecko2"))
                continue;
            /* FIXME: ~ should negate match */
            if (domain[0] == '~')
                domain++;
            adblock_update_css_hash (g_strstrip (domain), data[1]);
        }
        g_strfreev (domains);
    }
    else
    {
        adblock_update_css_hash (data[0], data[1]);
    }
    g_strfreev (data);
}

static gchar*
adblock_parse_line (gchar* line)
{
    /*
     * AdblockPlus rule reference based on http://adblockplus.org/en/filters
     * Block URL:
     *   http://example.com/ads/banner123.gif
     *   http://example.com/ads/banner*.gif
     *   http://example.com/ads/*
     * Partial match for "ad":
     *   *ad*
     *   ad
     * Block example.com/annoyingflash.swf but not example.com/swf/:
     *   swf|
     * Block bad.example/banner.gif but not good.example/analyze?http://bad.example:
     *   |http://baddomain.example/
     * Block http(s) example.com but not badexample.com or good.example/analyze?http://bad.example:
     *   ||example.com/banner.gif
     * Block example.com/ and example.com:8000/ but not example.com.ar/:
     *   http://example.com^
     * A ^ matches anything that isn't A-Za-z0-0_-.%
     * Block example.com:8000/foo.bar?a=12&b=%D1%82%D0%B5:
     *   ^example.com^
     *   ^%D1%82%D0%B5^
     *   ^foo.bar^
     * TODO: ^ is partially supported by Midori
     * Block banner123 and banner321 with a regex:
     *   /banner\d+/
     * Never block URIs with "advice":
     *   @@advice
     * No blocking at all:
     *   @@http://example.com
     *   @@|http://example.com
     * TODO: @@ is currently ignored by Midori.
     * Element hiding by class:
     *   ##textad
     *   ##div.textad
     * Element hiding by id:
     *   ##div#sponsorad
     *   ##*#sponsorad
     * Match example.com/ and something.example.com/ but not example.org/
     *   example.com##*.sponsor
     * Match multiple domains:
     *   domain1.example,domain2.example,domain3.example##*.sponsor
     * Match on any domain but "example.com":
     *  ~example.com##*.sponsor
     * Match on "example.com" except "foo.example.com":
     *   example.com,~foo.example.com##*.sponsor
     * By design rules only apply to full domain names:
     *   "domain" is NOT equal to "domain.example,domain.test."
     * In Firefox rules can apply to browser UI:
     *   browser##menuitem#javascriptConsole will hide the Console menuitem
     * Hide tables with width attribute 80%:
     *   ##table[width="80%"]
     * Hide all div with title attribute containing "adv":
     *   ##div[title*="adv"]
     * Hide div with title starting with "adv" and ending with "ert":
     *   ##div[title^="adv"][title$="ert"]
     * Match tables with width attribute 80% and bgcolor attribute white:
     *   table[width="80%"][bgcolor="white"]
     * TODO: [] is currently ignored by Midori
     * Hide anything following div with class "adheader":
     *   ##div.adheader + *
     * Old CSS element hiding syntax, officially deprecated:
     *   #div(id=foo)
     * Match anything but "example.com"
     *   ~example.com##*.sponsor
     * TODO: ~ is currently ignored by Midori
     * Match "example.com" domain except "foo.example.com":
     *   example.com,~foo.example.com##*.sponsor
     * ! Comment
     * Supported options after a trailing $:
     *   domain,third-party,~pregecko2
     * Official options (not all supported by Midori):
     *   script,image,stylesheet,object,xmlhttprequest,object-subrequest,
     *   subdocument,document,elemhide,popup,third-party,sitekey,match-case
     *   collapse,donottrack,pregecko2
     * Deprecated:
     *   background,xbl,ping,dtd
     * Inverse options:
     *   ~script,~image,~stylesheet,~object,~xmlhttprequest,~collapse,
     *   ~object-subrequest,~subdocument,~document,~elemhide,~third-party,
     *   ~pregecko2
     **/

    /* Skip invalid, empty and comment lines */
    if (!(line && line[0] != ' ' && line[0] != '!' && line[0]))
        return NULL;

    /* FIXME: No support for whitelisting */
    if (line[0] == '@' && line[1] == '@')
        return NULL;
    /* FIXME: No support for [include] and [exclude] tags */
    if (line[0] == '[')
        return NULL;

    g_strchomp (line);

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
    if (strstr (line, "##"))
    {
        adblock_frame_add_private (line, "##");
        return NULL;
    }
    /* Got per domain CSS hider rule. Workaround */
    if (strchr (line, '#'))
    {
        adblock_frame_add_private (line, "#");
        return NULL;
    }

    /* Got URL blocker rule */
    if (line[0] == '|' && line[1] == '|' )
    {
        (void)*line++;
        (void)*line++;
        return adblock_add_url_pattern ("", "fulluri", line);
    }
    if (line[0] == '|')
    {
        (void)*line++;
        return adblock_add_url_pattern ("^", "fulluri", line);
    }
    return adblock_add_url_pattern ("", "uri", line);
}

static gboolean
adblock_parse_file (gchar* path)
{
    FILE* file;
    gchar line[2000];

    if ((file = g_fopen (path, "r")))
    {
        while (fgets (line, 2000, file))
            adblock_parse_line (line);
        fclose (file);
        return TRUE;
    }
    return FALSE;
}

static void
adblock_deactivate_tabs (MidoriView*      view,
                         MidoriBrowser*   browser,
                         MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);

    g_signal_handlers_disconnect_by_func (
       web_view, adblock_window_object_cleared_cb, 0);
    g_signal_handlers_disconnect_by_func (
       web_view, adblock_populate_popup_cb, extension);
    g_signal_handlers_disconnect_by_func (
       web_view, adblock_resource_request_starting_cb, view);
    g_signal_handlers_disconnect_by_func (
       web_view, adblock_load_finished_cb, view);
    g_signal_handlers_disconnect_by_func (
       web_view, adblock_navigation_policy_decision_requested_cb, view);
}

static void
adblock_deactivate_cb (MidoriExtension* extension,
                       MidoriBrowser*   browser)
{
    gint i;
    GtkWidget* view;
    MidoriApp* app = midori_extension_get_app (extension);
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");

    g_signal_handlers_disconnect_by_func (
        browser, adblock_open_preferences_cb, extension);
    g_signal_handlers_disconnect_by_func (
        extension, adblock_deactivate_cb, browser);
    g_signal_handlers_disconnect_by_func (
        app, adblock_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, adblock_add_tab_cb, extension);
    g_signal_handlers_disconnect_by_func (
        browser, adblock_remove_tab_cb, extension);

    i = 0;
    while((view = midori_browser_get_nth_tab(browser, i++)))
        adblock_deactivate_tabs (MIDORI_VIEW (view), browser, extension);

    adblock_destroy_db ();
    midori_web_settings_remove_style (settings, "adblock-blockcss");
    g_object_unref (settings);
}

static void
adblock_activate_cb (MidoriExtension* extension,
                     MidoriApp*       app)
{
    #ifdef G_ENABLE_DEBUG
    const gchar* debug_mode;
    #endif
    KatzeArray* browsers;
    MidoriBrowser* browser;

    #ifdef G_ENABLE_DEBUG
    debug_mode = g_getenv ("MIDORI_ADBLOCK");
    if (debug_mode)
    {
        if (*debug_mode == '1')
            debug = 1;
        else if (*debug_mode == '2')
            debug = 2;
        else
            debug = 0;
    }
    #endif

    adblock_reload_rules (extension, FALSE);

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        adblock_app_add_browser_cb (app, browser, extension);
    g_signal_connect (app, "add-browser",
        G_CALLBACK (adblock_app_add_browser_cb), extension);

    g_object_unref (browsers);
}

#if G_ENABLE_DEBUG
static void
test_adblock_parse (void)
{
    adblock_init_db ();
    g_assert (!adblock_parse_line (NULL));
    g_assert (!adblock_parse_line ("!"));
    g_assert (!adblock_parse_line ("@@"));
    g_assert (!adblock_parse_line ("##"));
    g_assert (!adblock_parse_line ("["));

    g_assert_cmpstr (adblock_parse_line ("+advert/"), ==, "advert/");
    g_assert_cmpstr (adblock_parse_line ("*foo"), ==, "foo");
    g_assert_cmpstr (adblock_parse_line ("f*oo"), ==, "f.*oo");
    g_assert_cmpstr (adblock_parse_line ("?foo"), ==, "\\?foo");
    g_assert_cmpstr (adblock_parse_line ("foo?"), ==, "foo\\?");

    g_assert_cmpstr (adblock_parse_line (".*foo/bar"), ==, "..*foo/bar");
    g_assert_cmpstr (adblock_parse_line ("http://bla.blub/*"), ==, "http://bla.blub/");
}

static void
test_adblock_pattern (void)
{
    gint temp;
    gchar* filename;

    temp = g_file_open_tmp ("midori_adblock_match_test_XXXXXX", &filename, NULL);

    /* TODO: Update some tests and add new ones. */
    g_file_set_contents (filename,
        "*ads.foo.bar*\n"
        "*ads.bogus.name*\n"
        "||^http://ads.bla.blub/*\n"
        "|http://ads.blub.boing/*$domain=xxx.com\n"
        "engine.adct.ru/*?\n"
        "/addyn|*|adtech;\n"
        "doubleclick.net/pfadx/*.mtvi\n"
        "objects.tremormedia.com/embed/xml/*.xml?r=\n"
        "videostrip.com^*/admatcherclient.\n"
        "test.dom/test?var\n"
        "/adpage.\n"
        "br.gcl.ru/cgi-bin/br/",
        -1, NULL);

    adblock_parse_file (filename);

    g_test_timer_start ();
    g_assert (adblock_is_matched ("http://www.engadget.com/_uac/adpage.html", ""));
    g_assert (adblock_is_matched ("http://test.dom/test?var=1", ""));
    g_assert (adblock_is_matched ("http://ads.foo.bar/teddy", ""));
    g_assert (!adblock_is_matched ("http://ads.fuu.bar/teddy", ""));
    g_assert (adblock_is_matched ("https://ads.bogus.name/blub", ""));
    g_assert (adblock_is_matched ("http://ads.bla.blub/kitty", ""));
    g_assert (adblock_is_matched ("http://ads.blub.boing/soda", ""));
    g_assert (!adblock_is_matched ("http://ads.foo.boing/beer", ""));
    g_assert (adblock_is_matched ("https://testsub.engine.adct.ru/test?id=1", ""));
    if (USE_PATTERN_MATCHING)
        g_assert (adblock_is_matched ("http://test.ltd/addyn/test/test?var=adtech;&var2=1", ""));
    g_assert (adblock_is_matched ("http://add.doubleclick.net/pfadx/aaaa.mtvi", ""));
    g_assert (!adblock_is_matched ("http://add.doubleclick.net/pfadx/aaaa.mtv", ""));
    g_assert (adblock_is_matched ("http://objects.tremormedia.com/embed/xml/list.xml?r=", ""));
    g_assert (!adblock_is_matched ("http://qq.videostrip.c/sub/admatcherclient.php", ""));
    g_assert (adblock_is_matched ("http://qq.videostrip.com/sub/admatcherclient.php", ""));
    g_assert (adblock_is_matched ("http://qq.videostrip.com/sub/admatcherclient.php", ""));
    g_assert (adblock_is_matched ("http://br.gcl.ru/cgi-bin/br/test", ""));
    g_assert (!adblock_is_matched ("https://bugs.webkit.org/buglist.cgi?query_format=advanced&short_desc_type=allwordssubstr&short_desc=&long_desc_type=substring&long_desc=&bug_file_loc_type=allwordssubstr&bug_file_loc=&keywords_type=allwords&keywords=&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&emailassigned_to1=1&emailtype1=substring&email1=&emailassigned_to2=1&emailreporter2=1&emailcc2=1&emailtype2=substring&email2=&bugidtype=include&bug_id=&votes=&chfieldfrom=&chfieldto=Now&chfieldvalue=&query_based_on=gtkport&field0-0-0=keywords&type0-0-0=anywordssubstr&value0-0-0=Gtk%20Cairo%20soup&field0-0-1=short_desc&type0-0-1=anywordssubstr&value0-0-1=Gtk%20Cairo%20soup%20autoconf%20automake%20autotool&field0-0-2=component&type0-0-2=equals&value0-0-2=WebKit%20Gtk", ""));
    g_assert (!adblock_is_matched ("http://www.engadget.com/2009/09/24/google-hits-android-rom-modder-with-a-cease-and-desist-letter/", ""));
    g_assert (!adblock_is_matched ("http://karibik-invest.com/es/bienes_raices/search.php?sqT=19&sqN=&sqMp=&sqL=0&qR=1&sqMb=&searchMode=1&action=B%FAsqueda", ""));
    g_assert (!adblock_is_matched ("http://google.com", ""));

    g_print ("Search took %f seconds\n", g_test_timer_elapsed ());

    close (temp);
    g_unlink (filename);

    g_hash_table_destroy (pattern);
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
        "version", "0.6" MIDORI_VERSION_SUFFIX,
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);
    midori_extension_install_string_list (extension, "filters", NULL, G_MAXSIZE);

    g_signal_connect (extension, "activate",
        G_CALLBACK (adblock_activate_cb), NULL);

    return extension;
}
