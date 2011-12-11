/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008-2010 Arno Renevier <arno@renevier.net>
 Copyright (C) 2010-2011 Paweł Forysiuk <tuxator@o2.pl>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/* This extensions add support for user addons: userscripts and userstyles */

#include <midori/midori.h>
#include <glib/gstdio.h>

#include "config.h"
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#ifndef X_OK
    #define X_OK 1
#endif

typedef enum
{
    ADDON_NONE,
    ADDONS_USER_SCRIPTS,
    ADDONS_USER_STYLES
} AddonsKind;

#define ADDONS_TYPE \
    (addons_get_type ())
#define ADDONS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), ADDONS_TYPE, Addons))
#define ADDONS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), ADDONS_TYPE, AddonsClass))
#define IS_ADDONS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ADDONS_TYPE))
#define IS_ADDONS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), ADDONS_TYPE))
#define ADDONS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), ADDONS_TYPE, AddonsClass))

typedef struct _Addons                Addons;
typedef struct _AddonsClass           AddonsClass;

struct _AddonsClass
{
    GtkVBoxClass parent_class;
};

struct _Addons
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* treeview;

    AddonsKind kind;
};

static void
addons_iface_init (MidoriViewableIface* iface);

static gchar*
addons_convert_to_simple_regexp (const gchar* pattern);

G_DEFINE_TYPE_WITH_CODE (Addons, addons, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             addons_iface_init));

struct AddonElement
{
    gchar* fullpath;
    gchar* displayname;
    gchar* description;
    gchar* script_content;

    gboolean enabled;
    gboolean broken;

    GSList* includes;
    GSList* excludes;
};

struct AddonsList
{
    GtkListStore* liststore;
    GSList* elements;
};

static void
addons_install_response (GtkWidget*  infobar,
                         gint        response_id,
                         MidoriView* view)
{
    if (response_id == GTK_RESPONSE_ACCEPT)
    {
        const gchar* uri = midori_view_get_display_uri (view);
        if (uri && *uri)
        {
            gchar* hostname, *path;
            gchar* dest_uri, *filename, *dest_path, *temp_uri, *folder_path;
            const gchar* folder;
            WebKitNetworkRequest* request;
            WebKitDownload* download;

            hostname = midori_uri_parse_hostname (uri, &path);
            temp_uri = NULL;
            filename = NULL;
            folder = NULL;

            if (g_str_has_suffix (uri, ".user.js"))
                folder = "scripts";
            else if (g_str_has_suffix (uri, ".user.css"))
                folder = "styles";
            else if (!g_strcmp0 (hostname, "userscripts.org"))
            {
                /* http://userscripts.org/scripts/ACTION/SCRIPT_ID/NAME */
                gchar* subpage = strchr (strchr (path + 1, '/') + 1, '/');
                if (subpage && subpage[0] == '/' && g_ascii_isdigit (subpage[1]))
                {
                    const gchar* js_script;
                    WebKitWebView* web_view;
                    WebKitWebFrame* web_frame;

                    js_script = "document.getElementById('heading').childNodes[3].childNodes[1].textContent";
                    web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (view));
                    web_frame = webkit_web_view_get_main_frame (web_view);

                    if (WEBKIT_IS_WEB_FRAME (web_frame))
                    {
                        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
                        gchar* value = sokoke_js_script_eval (js_context, js_script, NULL);
                        if (value && *value)
                            filename = g_strdup_printf ("%s.user.js", value);
                        g_free (value);
                    }

                    /* rewrite uri to get source js */
                    temp_uri = g_strdup_printf ("http://%s/scripts/source/%s.user.js",
                                                hostname, subpage + 1);
                    uri = temp_uri;
                    folder = "scripts";
                }
            }
            else if (!g_strcmp0 (hostname, "userstyles.org"))
            {
                /* http://userstyles.org/styles/STYLE_ID/NAME */
                gchar* subpage = strchr (path + 1, '/');
                if (subpage && subpage[0] == '/' && g_ascii_isdigit (subpage[1]))
                {
                    const gchar* js_script;
                    WebKitWebView* web_view;
                    WebKitWebFrame* web_frame;
                    gchar** style_id;

                    js_script = "document.getElementById('stylish-description').innerHTML;";
                    web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (view));
                    web_frame = webkit_web_view_get_main_frame (web_view);

                    if (WEBKIT_IS_WEB_FRAME (web_frame))
                    {
                        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
                        gchar* value = sokoke_js_script_eval (js_context, js_script, NULL);
                        if (value && *value)
                            filename = g_strdup_printf ("%s.css", value);
                        g_free (value);
                    }
                    /* rewrite uri to get css */
                    style_id = g_strsplit (subpage + 1, "/", 2);
                    temp_uri = g_strdup_printf ("http://%s/styles/%s.css", hostname, style_id[0]);
                    g_strfreev (style_id);
                    uri = temp_uri;
                    folder = "styles";
                }
            }

            if (!filename)
                filename = g_path_get_basename (uri);
            folder_path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                                 PACKAGE_NAME, folder, NULL);

            if (!g_file_test (folder_path, G_FILE_TEST_EXISTS))
                katze_mkdir_with_parents (folder_path, 0700);
            dest_path = g_build_path (G_DIR_SEPARATOR_S, folder_path, filename, NULL);

            request = webkit_network_request_new (uri);
            download = webkit_download_new (request);
            g_object_unref (request);

            dest_uri = g_filename_to_uri (dest_path, NULL, NULL);
            webkit_download_set_destination_uri (download, dest_uri);
            webkit_download_start (download);

            g_free (filename);
            g_free (dest_uri);
            g_free (temp_uri);
            g_free (dest_path);
            g_free (folder_path);
            g_free (hostname);
        }
    }
    gtk_widget_destroy (GTK_WIDGET (infobar));
}

static void
addons_uri_install (MidoriView*    view,
                    AddonsKind     kind)
{
    const gchar* message;
    const gchar* button_text;

    if (kind == ADDONS_USER_SCRIPTS)
    {
        /* i18n: An infobar shows up when viewing a script on userscripts.org */
        message = _("This page appears to contain a user script. Do you wish to install it?");
        button_text = _("_Install user script");
    }
    else if (kind == ADDONS_USER_STYLES)
    {
        /* i18n: An infobar shows up when viewing a style on userstyles.org */
        message = _("This page appears to contain a user style. Do you wish to install it?");
        button_text = _("_Install user style");
    }
    else
        g_assert_not_reached ();

    midori_view_add_info_bar (view, GTK_MESSAGE_QUESTION, message,
        G_CALLBACK (addons_install_response), view,
        button_text, GTK_RESPONSE_ACCEPT,
        _("Don't install"), GTK_RESPONSE_CANCEL, NULL);
}

static void
addons_notify_load_status_cb (MidoriView*      view,
                              GParamSpec*      pspec,
                              MidoriExtension* extension)
{
    const gchar* uri = midori_view_get_display_uri (view);
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (view));

    if (webkit_web_view_get_view_source_mode (web_view))
        return;

    if (uri && *uri)
    {
       if (midori_view_get_load_status (view) == MIDORI_LOAD_COMMITTED)
       {
           /* casual sites goes by uri suffix */
           if (g_str_has_suffix (uri, ".user.js"))
               addons_uri_install (view, ADDONS_USER_SCRIPTS);
           else if (g_str_has_suffix (uri, ".user.css"))
               addons_uri_install (view, ADDONS_USER_STYLES);
           else
           {
               gchar* path;
               gchar* hostname = midori_uri_parse_hostname (uri, &path);
               if (!g_strcmp0 (hostname, "userscripts.org")
                && (g_str_has_prefix (path, "/scripts/show/")
                 || g_str_has_prefix (path, "/scripts/review/")))
               {
                   /* Main (with desc) and "source view" pages */
                   addons_uri_install (view, ADDONS_USER_SCRIPTS);
               }
               else if (!g_strcmp0 (hostname, "userstyles.org")
                && g_str_has_prefix (path, "/styles/"))
               {
                   gchar* subpage = strchr (path + 1, '/');
                   /* Main page with style description */
                   if (subpage && subpage[0] == '/' && g_ascii_isdigit (subpage[1]))
                       addons_uri_install (view, ADDONS_USER_STYLES);
               }
               g_free (hostname);
           }
       }
    }
}

static void
addons_button_add_clicked_cb (GtkToolItem* toolitem,
                              Addons*      addons)
{
    gchar* addons_type;
    gchar* path;
    GtkWidget* dialog;
    GtkFileFilter* filter;

    if (addons->kind == ADDONS_USER_SCRIPTS)
    {
        addons_type = g_strdup ("userscripts");
        path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                             PACKAGE_NAME, "scripts", NULL);
    }
    else if (addons->kind == ADDONS_USER_STYLES)
    {
        addons_type = g_strdup ("userstyles");
        path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                             PACKAGE_NAME, "styles", NULL);
    }
    else
        g_assert_not_reached ();

    dialog = gtk_file_chooser_dialog_new (_("Choose file"),
        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
        GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
        GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);

    filter = gtk_file_filter_new ();

    if (addons->kind == ADDONS_USER_SCRIPTS)
    {
        gtk_file_filter_set_name (filter, _("Userscripts"));
        gtk_file_filter_add_pattern (filter, "*.js");
    }
    else if (addons->kind == ADDONS_USER_STYLES)
    {
        gtk_file_filter_set_name (filter, _("Userstyles"));
        gtk_file_filter_add_pattern (filter, "*.css");
    }
    else
        g_assert_not_reached ();

    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        GSList* files;

        if (!g_file_test (path, G_FILE_TEST_EXISTS))
            katze_mkdir_with_parents (path, 0700);

        #if !GTK_CHECK_VERSION (2, 14, 0)
        files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (dialog));
        #else
        files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));
        #endif

        while (files)
        {
            GFile* src_file;
            GError* error = NULL;

            #if !GTK_CHECK_VERSION (2, 14, 0)
            src_file = g_file_new_for_path (files);
            #else
            src_file = files->data;
            #endif

            if (G_IS_FILE (src_file))
            {
                GFile* dest_file;
                gchar* dest_file_path;

                dest_file_path = g_build_path (G_DIR_SEPARATOR_S, path,
                    g_file_get_basename (src_file), NULL);

                dest_file = g_file_new_for_path (dest_file_path);

                g_file_copy (src_file, dest_file,
                    G_FILE_COPY_OVERWRITE | G_FILE_COPY_BACKUP,
                    NULL, NULL, NULL, &error);

                if (error)
                {
                    GtkWidget* msg_box;
                    msg_box = gtk_message_dialog_new (
                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_OK,
                        "%s", error->message);

                    gtk_window_set_title (GTK_WINDOW (msg_box), _("Error"));
                    gtk_dialog_run (GTK_DIALOG (msg_box));
                    gtk_widget_destroy (msg_box);
                    g_error_free (error);
                }

                g_object_unref (src_file);
                g_object_unref (dest_file);
                g_free (dest_file_path);
            }
            files = g_slist_next (files);
        }
        g_slist_free (files);
    }

    g_free (addons_type);
    g_free (path);
    gtk_widget_destroy (dialog);
}

static void
addons_button_delete_clicked_cb (GtkWidget* toolitem,
                                 Addons*    addons)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (addons->treeview),
                                                          &model, &iter))
    {
        struct AddonElement* element;
        gint delete_response;
        GtkWidget* dialog;
        gchar* markup;

        gtk_tree_model_get (model, &iter, 0, &element, -1);
        dialog = gtk_message_dialog_new (
            GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_CANCEL,
            _("Do you want to delete '%s'?"),
            element->displayname);
        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_DELETE, GTK_RESPONSE_YES);

        gtk_window_set_title (GTK_WINDOW (dialog),
            addons->kind == ADDONS_USER_SCRIPTS
            ? _("Delete user script")
            : _("Delete user style"));

        markup = g_markup_printf_escaped (
            _("The file <b>%s</b> will be permanently deleted."),
            element->fullpath);
        gtk_message_dialog_format_secondary_markup (
            GTK_MESSAGE_DIALOG (dialog), "%s", markup);
        g_free (markup);

        delete_response = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (GTK_WIDGET (dialog));

        if (delete_response == GTK_RESPONSE_YES)
        {
            GError* error = NULL;
            GFile* file;
            gboolean result;

            file = g_file_new_for_path (element->fullpath);
            result = g_file_delete (file, NULL, &error);

            if (!result && error)
            {
                GtkWidget* msg_box;
                msg_box = gtk_message_dialog_new (
                    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "%s", error->message);

                    gtk_window_set_title (GTK_WINDOW (msg_box), _("Error"));
                    gtk_dialog_run (GTK_DIALOG (msg_box));
                    gtk_widget_destroy (msg_box);
                    g_error_free (error);
            }

            if (result)
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

            g_object_unref (file);
        }
    }
}
static void
addons_open_in_editor_clicked_cb (GtkWidget* toolitem,
                                  Addons*    addons)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (addons->treeview),
                                                          &model, &iter))
    {
        struct AddonElement* element;
        MidoriWebSettings* settings;
        MidoriBrowser* browser;
        gchar* text_editor;

        browser = midori_browser_get_for_widget (GTK_WIDGET (addons->treeview));
        settings = midori_browser_get_settings (browser);

        gtk_tree_model_get (model, &iter, 0, &element, -1);

        g_object_get (settings, "text-editor", &text_editor, NULL);
        if (text_editor && *text_editor)
            sokoke_spawn_program (text_editor, element->fullpath);
        else
        {
            gchar* element_uri = g_filename_to_uri (element->fullpath, NULL, NULL);
            sokoke_show_uri (NULL, element_uri,
                             gtk_get_current_event_time (), NULL);
            g_free (element_uri);
        }

        g_free (text_editor);
    }
}

static void
addons_open_target_folder_clicked_cb (GtkWidget* toolitem,
                                      Addons*    addons)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    gchar* folder;
    gchar* folder_uri;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (addons->treeview),
                                            &model, &iter))
    {
        struct AddonElement* element;

        gtk_tree_model_get (model, &iter, 0, &element, -1);
        folder = g_path_get_dirname (element->fullpath);
    }
    else
        folder = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                               PACKAGE_NAME,
                               addons->kind == ADDONS_USER_SCRIPTS
                               ? "scripts" : "styles", NULL);
    folder_uri = g_filename_to_uri (folder, NULL, NULL);
    g_free (folder);

    sokoke_show_uri (gtk_widget_get_screen (GTK_WIDGET (addons->treeview)),
                     folder_uri, gtk_get_current_event_time (), NULL);
    g_free (folder_uri);
}

static void
addons_popup_item (GtkMenu*             menu,
                   const gchar*         stock_id,
                   const gchar*         label,
                   struct AddonElement* element,
                   gpointer             callback,
                   Addons*              addons)
{
    GtkWidget* menuitem;

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
            GTK_BIN (menuitem))), label);
    if (!strcmp (stock_id, GTK_STOCK_EDIT))
        gtk_widget_set_sensitive (menuitem, element->fullpath !=NULL);
    else if (strcmp (stock_id, GTK_STOCK_DELETE))
        gtk_widget_set_sensitive (menuitem, element->fullpath !=NULL);
    g_object_set_data (G_OBJECT (menuitem), "AddonElement", &element);
    g_signal_connect (menuitem, "activate", G_CALLBACK(callback), addons);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static void
addons_popup (GtkWidget*           widget,
              GdkEventButton*      event,
              struct AddonElement* element,
              Addons*              addons)
{
    GtkWidget* menu;

    menu = gtk_menu_new ();
    addons_popup_item (GTK_MENU (menu), GTK_STOCK_EDIT, _("Open in Text Editor"),
        element, addons_open_in_editor_clicked_cb, addons);
    addons_popup_item (GTK_MENU (menu), GTK_STOCK_OPEN, _("Open Target Folder"),
        element, addons_open_target_folder_clicked_cb, addons);
    addons_popup_item (GTK_MENU (menu), GTK_STOCK_DELETE, NULL,
         element, addons_button_delete_clicked_cb, addons);
   katze_widget_popup (widget, GTK_MENU (menu), event, KATZE_MENU_POSITION_CURSOR);
}

static gboolean
addons_popup_menu_cb (GtkWidget *widget,
                      Addons*    addons)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        struct AddonElement* element;
        gtk_tree_model_get (model, &iter, 0, &element, -1);
        addons_popup (widget, NULL, element, addons);
        return TRUE;
    }
    return FALSE;
}

static gboolean
addons_button_release_event_cb (GtkWidget*       widget,
                                GdkEventButton*  event,
                                Addons*          addons)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button != 3)
        return FALSE;
    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        struct AddonElement* element;
        gtk_tree_model_get (model, &iter, 0, &element, -1);
        addons_popup (widget, NULL, element, addons);
        return TRUE;
    }
    return FALSE;
}

GtkWidget*
addons_get_toolbar (MidoriViewable* viewable)
{
    GtkWidget* toolbar;
    GtkToolItem* toolitem;

    g_return_val_if_fail (IS_ADDONS (viewable), NULL);

    if (!ADDONS (viewable)->toolbar)
    {
        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* add button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (addons_button_add_clicked_cb), viewable);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem), _("Add new addon"));
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* Text editor button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_EDIT);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (addons_open_in_editor_clicked_cb), viewable);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                    _("Open in Text Editor"));
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* Target folder button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DIRECTORY);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (addons_open_target_folder_clicked_cb), viewable);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                    _("Open Target Folder"));
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* Delete button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DELETE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (addons_button_delete_clicked_cb), viewable);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem), _("Remove selected addon"));
        gtk_widget_show (GTK_WIDGET (toolitem));
        ADDONS (viewable)->toolbar = toolbar;

        g_signal_connect (toolbar, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &ADDONS (viewable)->toolbar);
    }

    return ADDONS (viewable)->toolbar;
}

static const gchar*
addons_get_label (MidoriViewable* viewable)
{
    Addons* addons = ADDONS (viewable);
    if (addons->kind == ADDONS_USER_SCRIPTS)
        return _("Userscripts");
    else if (addons->kind == ADDONS_USER_STYLES)
        return _("Userstyles");
    return NULL;
}

static const gchar*
addons_get_stock_id (MidoriViewable* viewable)
{
    Addons* addons = ADDONS (viewable);
    if (addons->kind == ADDONS_USER_SCRIPTS)
        return STOCK_SCRIPT;
    else if (addons->kind == ADDONS_USER_STYLES)
        return STOCK_STYLE;
    return NULL;
}

static void
addons_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = addons_get_stock_id;
    iface->get_label = addons_get_label;
    iface->get_toolbar = addons_get_toolbar;
}

static void
addons_free_elements (GSList* elements)
{
    struct AddonElement* element;

    while (elements)
    {
        element = elements->data;
        g_free (element->fullpath);
        g_free (element->displayname);
        g_free (element->description);
        g_free (element->script_content);
        g_slist_free (element->includes);
        g_slist_free (element->excludes);
        g_slice_free (struct AddonElement, element);

        elements = g_slist_next (elements);
    }
}

static void
addons_class_init (AddonsClass* class)
{
}

static void
addons_treeview_render_tick_cb (GtkTreeViewColumn* column,
                                GtkCellRenderer*   renderer,
                                GtkTreeModel*      model,
                                GtkTreeIter*       iter,
                                GtkWidget*         treeview)
{
    struct AddonElement *element;

    gtk_tree_model_get (model, iter, 0, &element, -1);

    g_object_set (renderer,
                  "active", element->enabled,
                  "sensitive", !element->broken,
                  NULL);
}

static void
addons_cell_renderer_toggled_cb (GtkCellRendererToggle* renderer,
                                 const gchar*           path,
                                 Addons*                addons)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (addons->treeview));
    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
        struct AddonElement *element;
        GtkTreePath* tree_path;

        gtk_tree_model_get (model, &iter, 0, &element, -1);

        element->enabled = !element->enabled;

        /* After enabling or disabling an element, the tree view
           is not updated automatically; we need to notify tree model
           in order to take the modification into account */
        tree_path = gtk_tree_path_new_from_string (path);
        gtk_tree_model_row_changed (model, tree_path, &iter);
        gtk_tree_path_free (tree_path);
    }
}

static void
addons_treeview_render_text_cb (GtkTreeViewColumn* column,
                                GtkCellRenderer*   renderer,
                                GtkTreeModel*      model,
                                GtkTreeIter*       iter,
                                GtkWidget*         treeview)
{
    struct AddonElement *element;

    gtk_tree_model_get (model, iter, 0, &element, -1);

    g_object_set (renderer, "text", element->displayname, NULL);
    if (!element->enabled)
        g_object_set (renderer, "sensitive", false, NULL);
    else
        g_object_set (renderer, "sensitive", true, NULL);
}

static void
addons_treeview_row_activated_cb (GtkTreeView*       treeview,
                                  GtkTreePath*       path,
                                  GtkTreeViewColumn* column,
                                  Addons*            addons)
{
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        struct AddonElement *element;

        gtk_tree_model_get (model, &iter, 0, &element, -1);

        element->enabled = !element->enabled;

        /* After enabling or disabling an element, the tree view
           is not updated automatically; we need to notify tree model
           in order to take the modification into account */
        gtk_tree_model_row_changed (model, path, &iter);
    }
}

static GSList*
addons_get_directories (AddonsKind kind)
{
    gchar* folder_name;
    GSList* directories;
    const char* const* datadirs;
    gchar* path;

    g_assert (kind == ADDONS_USER_SCRIPTS || kind == ADDONS_USER_STYLES);

    directories = NULL;
    if (kind == ADDONS_USER_SCRIPTS)
        folder_name = g_strdup ("scripts");
    else if (kind == ADDONS_USER_STYLES)
        folder_name = g_strdup ("styles");
    else
        g_assert_not_reached ();

    path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                         PACKAGE_NAME, folder_name, NULL);
    if (g_access (path, X_OK) == 0)
        directories = g_slist_prepend (directories, path);
    else
        g_free (path);

    datadirs = g_get_system_data_dirs ();
    while (*datadirs)
    {
        path = g_build_path (G_DIR_SEPARATOR_S, *datadirs,
                             PACKAGE_NAME, folder_name, NULL);
        if (g_slist_find (directories, path) == NULL && g_access (path, X_OK) == 0)
            directories = g_slist_prepend (directories, path);
        else
            g_free (path);
        datadirs++;
    }

    g_free (folder_name);

    return directories;
}

static GSList*
addons_get_files (AddonsKind kind)
{
    GSList* files;
    GDir* addon_dir;
    GSList* directories;
    const gchar* filename;
    gchar* dirname;
    gchar* fullname;
    gchar* file_extension;

    g_assert (kind == ADDONS_USER_SCRIPTS || kind == ADDONS_USER_STYLES);

    if (kind == ADDONS_USER_SCRIPTS)
        file_extension = ".js";
    else if (kind == ADDONS_USER_STYLES)
        file_extension = ".css";
    else
        g_assert_not_reached ();

    files = NULL;

    directories = addons_get_directories (kind);
    while (directories)
    {
        dirname = directories->data;
        if ((addon_dir = g_dir_open (dirname, 0, NULL)))
        {
            while ((filename = g_dir_read_name (addon_dir)))
            {
                if (g_str_has_suffix (filename, file_extension))
                {
                    fullname = g_build_filename (dirname, filename, NULL);
                    if (g_slist_find (files, fullname) == NULL)
                        files = g_slist_prepend (files, fullname);
                }
            }
            g_dir_close (addon_dir);
        }
        g_free (dirname);
        directories = g_slist_next (directories);
    }

    g_slist_free (directories);

    return files;
}

static gboolean
js_metadata_from_file (const gchar* filename,
                       GSList**     includes,
                       GSList**     excludes,
                       gchar**      name,
                       gchar**      description)
{
    GIOChannel* channel;
    gboolean found_meta;
    gchar* line;
    gchar* rest_of_line;

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return FALSE;

    channel = g_io_channel_new_file (filename, "r", 0);
    if (!channel)
        return FALSE;

    found_meta = FALSE;

    while (g_io_channel_read_line (channel, &line, NULL, NULL, NULL)
           == G_IO_STATUS_NORMAL)
    {
        g_strstrip (line);
        if (g_str_has_prefix (line, "// ==UserScript=="))
            found_meta = TRUE;
        else if (found_meta)
        {
            if (g_str_has_prefix (line, "// ==/UserScript=="))
                found_meta = FALSE;
            else if (g_str_has_prefix (line, "// @require")
                  || g_str_has_prefix (line, "// @resource"))
            {
                /* We don't support these, so abort here */
                g_free (line);
                g_io_channel_shutdown (channel, false, 0);
                g_slist_free (*includes);
                g_slist_free (*excludes);
                *includes = NULL;
                *excludes = NULL;
                return FALSE;
            }
             else if (includes && g_str_has_prefix (line, "// @include"))
            {
                 gchar* re = NULL;
                 rest_of_line = g_strdup (line + strlen ("// @include"));
                 rest_of_line =  g_strstrip (rest_of_line);
                 re = addons_convert_to_simple_regexp (rest_of_line);
                 *includes = g_slist_prepend (*includes, re);
            }
             else if (excludes && g_str_has_prefix (line, "// @exclude"))
            {
                 gchar* re = NULL;
                 rest_of_line = g_strdup (line + strlen ("// @exclude"));
                 rest_of_line =  g_strstrip (rest_of_line);
                 re = addons_convert_to_simple_regexp (rest_of_line);
                 *excludes = g_slist_prepend (*excludes, re);
            }
             else if (name && g_str_has_prefix (line, "// @name"))
            {
                 if (!strncmp (line, "// @namespace", 13))
                     continue;
                 rest_of_line = g_strdup (line + strlen ("// @name"));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *name = rest_of_line;
            }
             else if (description && g_str_has_prefix (line, "// @description"))
            {
                 rest_of_line = g_strdup (line + strlen ("// @description"));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *description = rest_of_line;
            }
        }
        g_free (line);
    }
    g_io_channel_shutdown (channel, false, 0);
    g_io_channel_unref (channel);

    return TRUE;
}

static gboolean
css_metadata_from_file (const gchar* filename,
                        GSList**     includes,
                        GSList**     excludes)
{
    GIOChannel* channel;
    gchar* line;
    gchar* rest_of_line;
    gboolean line_has_meta;

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return FALSE;

    channel = g_io_channel_new_file (filename, "r", 0);
    if (!channel)
        return FALSE;

    line_has_meta = FALSE;
    while (g_io_channel_read_line (channel, &line, NULL, NULL, NULL)
           == G_IO_STATUS_NORMAL)
    {
        g_strstrip (line);
        if (g_str_has_prefix (line, "@-moz-document") || line_has_meta)
        { /* FIXME: We merely look for includes. We should honor blocks. */
             if (includes)
             {
                 gchar** parts;
                 guint i;

                 if (g_str_has_prefix (line, "@-moz-document"))
                     rest_of_line = g_strdup (line + strlen ("@-moz-document"));
                 else
                     rest_of_line = g_strdup (line);

                 rest_of_line = g_strstrip (rest_of_line);
                 line_has_meta  = !g_str_has_suffix (rest_of_line, "{");

                 parts = g_strsplit_set (rest_of_line, " ,", 0);
                 i = 0;
                 while (parts[i] && *parts[i] != '{')
                 {
                     gchar* value = NULL;
                     if (g_str_has_prefix (parts[i], "url-prefix("))
                        value = &parts[i][strlen ("url-prefix(")];
                     else if (g_str_has_prefix (parts[i], "domain("))
                        value = &parts[i][strlen ("domain(")];
                     else if (g_str_has_prefix (parts[i], "url("))
                        value = &parts[i][strlen ("url(")];
                    if (value)
                    {
                         guint begin, end;
                         gchar* domain;
                         gchar* tmp_domain;
                         gchar* re = NULL;

                         line_has_meta = TRUE;
                         begin = value[0] == '"' || value[0] == '\'' ? 1 : 0;
                         end = 1;
                         while (value[end] != '\0' && value[end] != ')')
                             ++end;

                         domain = g_strndup (value + begin, end - begin * 2);
                         if (!midori_uri_is_location (domain)
                          && !g_str_has_prefix (domain, "file://"))
                             tmp_domain = g_strdup_printf ("http://*%s/*", domain);
                         else
                             tmp_domain = domain;

                         re = addons_convert_to_simple_regexp (tmp_domain);
                         *includes = g_slist_prepend (*includes, re);
                         g_free (domain);
                    }
                    i++;
                 }
                 g_strfreev (parts);
                 g_free (rest_of_line);
             }
             else
                 line_has_meta = FALSE;
        }
        g_free (line);
    }
    g_io_channel_shutdown (channel, false, 0);
    g_io_channel_unref (channel);

    return TRUE;
}

static gboolean
addons_get_element_content (gchar*     file_path,
                            AddonsKind kind,
                            gboolean   has_metadata,
                            gchar**    content)
{
    gchar* file_content;
    GString* content_chunks;
    guint meta, comment;
    guint i, n;

    g_assert (kind == ADDONS_USER_SCRIPTS || kind == ADDONS_USER_STYLES);

    if (g_file_get_contents (file_path, &file_content, NULL, NULL))
    {
        if (kind == ADDONS_USER_SCRIPTS)
        {
            *content = g_strdup_printf (
                "window.addEventListener ('DOMContentLoaded',"
                "function () { %s }, true);", file_content);
        }
        else if (kind == ADDONS_USER_STYLES)
        {
            meta = 0;
            comment = 0;
            n = strlen (file_content);
            content_chunks = g_string_new_len (NULL, n);
            for (i = 0; i < n; i++)
            {
                /* Replace line breaks with spaces */
                if (file_content[i] == '\n' || file_content[i] == '\r')
                    file_content[i] = ' ';
                /* Change all single quotes to double quotes */
                if (file_content[i] == '\'')
                    file_content[i] = '\"';
                if (!meta && file_content[i] == '@')
                {
                    meta++;
                }
                else if (meta == 1
                    && (file_content[i] == '-' || file_content[i] == 'n'))
                {
                    meta++;
                }
                else if (meta == 2 && file_content[i] == '{')
                {
                    meta++;
                    continue;
                }
                else if (meta == 3 && file_content[i] == '{')
                    meta++;
                else if (meta == 4 && file_content[i] == '}')
                    meta--;
                else if (meta == 3 && file_content[i] == '}')
                {
                    meta = 0;
                    continue;
                }

                if (file_content[i] == '/' && file_content[i+1] == '*')
                    comment++;
                /* Check whether we have comment ending, merely '*' or '/' */
                else if (comment == 2
                    && file_content[i] == '*' && file_content[i+1] == '/')
                {
                    comment--;
                }
                else if (comment == 1
                    && file_content[i-1] == '*' && file_content[i] == '/')
                {
                    comment--;
                    continue;
                }

                /* Skip consecutive spaces */
                if (file_content[i] == ' '
                 && i > 0 && file_content[i - 1] == ' ')
                    continue;

                /* Append actual data to string */
                if ((meta == 0 || meta >= 3) && !comment)
                    g_string_append_c (content_chunks, file_content[i]);
            }

            if (has_metadata)
            {
            *content = g_strdup_printf (
                "window.addEventListener ('DOMContentLoaded',"
                "function () {"
                "var mystyle = document.createElement(\"style\");"
                "mystyle.setAttribute(\"type\", \"text/css\");"
                "mystyle.appendChild(document.createTextNode('%s'));"
                "var head = document.getElementsByTagName(\"head\")[0];"
                "if (head) head.appendChild(mystyle);"
                "else document.documentElement.insertBefore"
                "(mystyle, document.documentElement.firstChild);"
                "}, true);",
                content_chunks->str);
            g_string_free (content_chunks, TRUE);
            }
            else
            {
                *content = content_chunks->str;
                g_string_free (content_chunks, FALSE);
            }
        }
        g_free (file_content);
        if (*content)
            return TRUE;
    }
    return FALSE;
}

static void
addons_update_elements (MidoriExtension* extension,
                        AddonsKind       kind)
{
    GSList* addon_files;
    gchar* name;
    gchar* fullpath;
    struct AddonElement* element;
    struct AddonsList* addons_list;
    GSList* elements = NULL;
    GtkListStore* liststore = NULL;
    GtkTreeIter iter;
    gchar* config_file;
    GKeyFile* keyfile;

    if (kind == ADDONS_USER_SCRIPTS)
        addons_list = g_object_get_data (G_OBJECT (extension), "scripts-list");
    else if (kind == ADDONS_USER_STYLES)
        addons_list = g_object_get_data (G_OBJECT (extension), "styles-list");
    else
        g_assert_not_reached ();

    if (addons_list)
    {
        liststore = addons_list->liststore;
        elements = addons_list->elements;
    }

    if (elements)
        addons_free_elements (elements);

    if (liststore)
        gtk_list_store_clear (liststore);
    else
        liststore = gtk_list_store_new (4, G_TYPE_POINTER,
                                        G_TYPE_INT,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING);

    keyfile = g_key_file_new ();
    config_file = g_build_filename (midori_extension_get_config_dir (extension),
                                    "addons", NULL);
    g_key_file_load_from_file (keyfile, config_file, G_KEY_FILE_NONE, NULL);

    addon_files = addons_get_files (kind);

    elements = NULL;
    while (addon_files)
    {
        gchar* filename;
        gchar* tooltip;

        fullpath = addon_files->data;
        element = g_slice_new (struct AddonElement);
        element->displayname = g_filename_display_basename (fullpath);
        element->fullpath = fullpath;
        element->enabled = TRUE;
        element->broken = FALSE;
        element->includes = NULL;
        element->excludes = NULL;
        element->description = NULL;
        element->script_content = NULL;

        if (kind == ADDONS_USER_SCRIPTS)
        {
            name = NULL;
            if (!js_metadata_from_file (fullpath,
                                        &element->includes, &element->excludes,
                                        &name, &element->description))
                element->broken = TRUE;

            if (name)
                katze_assign (element->displayname, name);

            if (!element->broken)
                if (!addons_get_element_content (fullpath, kind, FALSE,
                                                 &(element->script_content)))
                    element->broken = TRUE;

            if (g_key_file_get_integer (keyfile, "scripts", fullpath, NULL) & 1)
                element->enabled = FALSE;
        }
        else if (kind == ADDONS_USER_STYLES)
        {
            if (!css_metadata_from_file (fullpath,
                                         &element->includes,
                                         &element->excludes))
                element->broken = TRUE;

            if (!element->broken)
                if (!addons_get_element_content (fullpath, kind,
                    element->includes || element->excludes,
                                                 &(element->script_content)))
                    element->broken = TRUE;

            if (g_key_file_get_integer (keyfile, "styles", fullpath, NULL) & 1)
                element->enabled = FALSE;
        }

        filename = g_path_get_basename (element->fullpath);
        if (element->description)
            tooltip = g_markup_printf_escaped ("%s\n\n%s",
                                       filename, element->description);
        else
            tooltip = g_markup_escape_text (filename, -1);
        g_free (filename);

        gtk_list_store_append (liststore, &iter);
        gtk_list_store_set (liststore, &iter,
                0, element, 1, 0, 2, element->fullpath,
                3, tooltip, -1);

        g_free (tooltip);
        addon_files = g_slist_next (addon_files);
        elements = g_slist_prepend (elements, element);
    }
    g_free (config_file);
    g_key_file_free (keyfile);

    g_slice_free (struct AddonsList, addons_list);
    addons_list = g_slice_new (struct AddonsList);
    addons_list->elements = elements;
    addons_list->liststore = liststore;

    if (kind == ADDONS_USER_SCRIPTS)
        g_object_set_data (G_OBJECT (extension), "scripts-list", addons_list);
    else if (kind == ADDONS_USER_STYLES)
        g_object_set_data (G_OBJECT (extension), "styles-list", addons_list);
}

static void
addons_init (Addons* addons)
{
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_toggle;

    addons->treeview = gtk_tree_view_new ();
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (addons->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_toggle = gtk_cell_renderer_toggle_new ();
    gtk_tree_view_column_pack_start (column, renderer_toggle, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_toggle,
        (GtkTreeCellDataFunc)addons_treeview_render_tick_cb,
        addons->treeview, NULL);
    g_signal_connect (renderer_toggle, "toggled",
        G_CALLBACK (addons_cell_renderer_toggled_cb), addons);
    gtk_tree_view_append_column (GTK_TREE_VIEW (addons->treeview), column);
    column = gtk_tree_view_column_new ();
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)addons_treeview_render_text_cb,
        addons->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (addons->treeview), column);
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (addons->treeview), 3);
    g_signal_connect (addons->treeview, "row-activated",
                      G_CALLBACK (addons_treeview_row_activated_cb),
                      addons);
    g_signal_connect (addons->treeview, "button-release-event",
                      G_CALLBACK (addons_button_release_event_cb),
                      addons);
    g_signal_connect (addons->treeview, "popup-menu",
                      G_CALLBACK (addons_popup_menu_cb),
                      addons);
    gtk_widget_show (addons->treeview);
    gtk_box_pack_start (GTK_BOX (addons), addons->treeview, TRUE, TRUE, 0);
}

static gchar*
addons_convert_to_simple_regexp (const gchar* pattern)
{
    guint len;
    gchar* dest;
    guint pos;
    guint i;
    gchar c;

    len = strlen (pattern);
    dest = g_malloc0 (len * 2 + 2);
    dest[0] = '^';
    pos = 1;

    for (i = 0; i < len; i++)
    {
        c = pattern[i];
        switch (c)
        {
            case '*':
                dest[pos] = '.';
                dest[pos + 1] = c;
                pos++;
                pos++;
                break;
            case '.' :
            case '?' :
            case '^' :
            case '$' :
            case '+' :
            case '{' :
            case '[' :
            case '|' :
            case '(' :
            case ')' :
            case ']' :
            case '\\' :
                dest[pos] = '\\';
                dest[pos + 1] = c;
                pos++;
                pos++;
                break;
            case ' ' :
                break;
            default:
                dest[pos] = pattern[i];
                pos ++;
        }
    }
    return dest;
}

static gboolean
addons_may_run (const gchar* uri,
                GSList**     includes,
                GSList**     excludes)
{
    gboolean match;
    GSList* list;

    if (*includes)
        match = FALSE;
    else
        match = TRUE;

    list = *includes;
    while (list)
    {
        gboolean matched = g_regex_match_simple (list->data, uri, 0, 0);
        if (matched)
        {
            match = TRUE;
            break;
        }
        list = g_slist_next (list);
    }
    if (!match)
        return FALSE;

    list = *excludes;
    while (list)
    {
        gboolean matched = g_regex_match_simple (list->data, uri, 0, 0);
        if (matched)
        {
            match = FALSE;
            break;
        }
        list = g_slist_next (list);
    }
    return match;
}

static gboolean
addons_skip_element (struct AddonElement* element,
                     gchar* uri)
{
    if (!element->enabled || element->broken)
        return TRUE;
    if (element->includes || element->excludes)
        if (!addons_may_run (uri, &element->includes, &element->excludes))
            return TRUE;
    return FALSE;
}

static void
addons_context_ready_cb (WebKitWebView*   web_view,
                         WebKitWebFrame*  web_frame,
                         JSContextRef     js_context,
                         JSObjectRef      js_window,
                         MidoriExtension* extension)
{
    gchar* uri;
    GSList* scripts, *styles;
    struct AddonElement* script, *style;
    struct AddonsList* scripts_list, *styles_list;
    const gchar* page_uri;

    page_uri = webkit_web_frame_get_uri (web_frame);
    if (!midori_uri_is_http (page_uri) && !midori_uri_is_blank (page_uri))
        return;

    /* Not a main frame! Abort */
    if (web_frame != webkit_web_view_get_main_frame (web_view))
        return;

    uri = katze_object_get_string (web_view, "uri");
    scripts_list = g_object_get_data (G_OBJECT (extension), "scripts-list");
    scripts = scripts_list->elements;
    while (scripts)
    {
        script = scripts->data;
        if (addons_skip_element (script, uri))
        {
            scripts = g_slist_next (scripts);
            continue;
        }
        if (script->script_content)
            webkit_web_view_execute_script (web_view, script->script_content);
        scripts = g_slist_next (scripts);
    }

    styles_list = g_object_get_data (G_OBJECT (extension), "styles-list");
    styles = styles_list->elements;
    while (styles)
    {
        style = styles->data;
        if (addons_skip_element (style, uri))
        {
            styles = g_slist_next (styles);
            continue;
        }
        if (style->script_content)
            webkit_web_view_execute_script (web_view, style->script_content);
        styles = g_slist_next (styles);
    }
    g_free (uri);
}

static void
addons_add_tab_cb (MidoriBrowser* browser,
                   MidoriView*  view,
                   MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);
    g_signal_connect (web_view, "window-object-cleared",
        G_CALLBACK (addons_context_ready_cb), extension);
    g_signal_connect (view, "notify::load-status",
        G_CALLBACK (addons_notify_load_status_cb), extension);
}

static void
addons_add_tab_foreach_cb (MidoriView*      view,
                           MidoriBrowser*   browser,
                           MidoriExtension* extension)
{
    addons_add_tab_cb (browser, view, extension);
}

static void
addons_deactivate_tabs (MidoriView*      view,
                        MidoriExtension* extension)
{
    GtkWidget* web_view = midori_view_get_web_view (view);
    g_signal_handlers_disconnect_by_func (
        web_view, addons_context_ready_cb, extension);
}

static void
addons_browser_destroy (MidoriBrowser*   browser,
                        MidoriExtension* extension)
{
    GtkWidget* scripts, *styles;

    midori_browser_foreach (browser, (GtkCallback)addons_deactivate_tabs, extension);
    g_signal_handlers_disconnect_by_func (browser, addons_add_tab_cb, extension);

    scripts = (GtkWidget*)g_object_get_data (G_OBJECT (browser), "scripts-addons");
    gtk_widget_destroy (scripts);
    styles = (GtkWidget*)g_object_get_data (G_OBJECT (browser), "styles-addons");
    gtk_widget_destroy (styles);
}

static char*
addons_generate_global_stylesheet (MidoriExtension* extension)
{
    GSList* styles;
    struct AddonElement* style;
    struct AddonsList* styles_list;
    GString* style_string = g_string_new ("");

    styles_list = g_object_get_data (G_OBJECT (extension), "styles-list");
    styles = styles_list->elements;
    while (styles != NULL)
    {
        style = styles->data;
        if (style->enabled &&
           !(style->includes || style->excludes || style->broken))
        {
            style_string = g_string_append (style_string, style->script_content);
        }
        styles = g_slist_next (styles);
    }

    return g_string_free (style_string, FALSE);
}

static void
addons_apply_global_stylesheet (MidoriExtension* extension)
{
    MidoriApp* app = midori_extension_get_app (extension);
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    gchar* data = addons_generate_global_stylesheet (extension);
    midori_web_settings_add_style (settings, "addons", data);
    g_free (data);
    g_object_unref (settings);
}

GtkWidget*
addons_new (AddonsKind kind, MidoriExtension* extension)
{
    GtkWidget* addons;
    GtkListStore* liststore;
    struct AddonsList* list;

    addons = g_object_new (ADDONS_TYPE, NULL);
    ADDONS (addons)->kind = kind;

    if (kind == ADDONS_USER_SCRIPTS)
        list = g_object_get_data (G_OBJECT (extension), "scripts-list");
    else if (kind == ADDONS_USER_STYLES)
        list = g_object_get_data (G_OBJECT (extension), "styles-list");
    else
        g_assert_not_reached ();

    liststore = list->liststore;
    gtk_tree_view_set_model (GTK_TREE_VIEW (ADDONS(addons)->treeview),
                             GTK_TREE_MODEL (liststore));
    gtk_widget_queue_draw (GTK_WIDGET (ADDONS(addons)->treeview));

    if (kind == ADDONS_USER_STYLES)
        g_signal_connect_swapped (liststore, "row-changed",
            G_CALLBACK (addons_apply_global_stylesheet), extension);

    return addons;
}

static void
addons_app_add_browser_cb (MidoriApp*       app,
                           MidoriBrowser*   browser,
                           MidoriExtension* extension)
{
    GtkWidget* panel;
    GtkWidget* scripts, *styles;

    midori_browser_foreach (browser,
          (GtkCallback)addons_add_tab_foreach_cb, extension);
    g_signal_connect (browser, "add-tab",
        G_CALLBACK (addons_add_tab_cb), extension);
    panel = katze_object_get_object (browser, "panel");

    scripts = addons_new (ADDONS_USER_SCRIPTS, extension);
    gtk_widget_show (scripts);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (scripts));
    g_object_set_data (G_OBJECT (browser), "scripts-addons", scripts);

    styles = addons_new (ADDONS_USER_STYLES, extension);
    gtk_widget_show (styles);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (styles));
    g_object_set_data (G_OBJECT (browser), "styles-addons", styles);

    g_object_unref (panel);
}

static void
addons_save_settings (MidoriApp*       app,
                      MidoriExtension* extension)
{
    struct AddonsList* scripts_list, *styles_list;
    struct AddonElement* script, *style;
    GSList* scripts, *styles;
    GKeyFile* keyfile;
    const gchar* config_dir;
    gchar* config_file;
    GError* error = NULL;

    keyfile = g_key_file_new ();

    /* scripts */
    scripts_list = g_object_get_data (G_OBJECT (extension), "scripts-list");
    scripts = scripts_list->elements;
    while (scripts)
    {
        script = scripts->data;
        if (!script->enabled)
            g_key_file_set_integer (keyfile, "scripts", script->fullpath, 1);
        scripts = g_slist_next (scripts);
    }

    /* styles */
    styles_list = g_object_get_data (G_OBJECT (extension), "styles-list");
    styles = styles_list->elements;
    while (styles)
    {
        style = styles->data;
        if (!style->enabled)
            g_key_file_set_integer (keyfile, "styles", style->fullpath, 1);
        styles = g_slist_next (styles);
    }

    config_dir = midori_extension_get_config_dir (extension);
    config_file = g_build_filename (config_dir, "addons", NULL);
    katze_mkdir_with_parents (config_dir, 0700);
    sokoke_key_file_save_to_file (keyfile, config_file, &error);
    /* If the folder is /, this is a test run, thus no error */
    if (error && !g_str_equal (config_dir, "/"))
    {
        g_warning (_("The configuration of the extension '%s' couldn't be saved: %s\n"),
                    _("User addons"), error->message);
        g_error_free (error);
    }

    g_free (config_file);
    g_key_file_free (keyfile);
}

static void
addons_disable_monitors (MidoriExtension* extension)
{
    GSList* monitors;

    monitors = g_object_get_data (G_OBJECT (extension), "monitors");
    if (!monitors)
        return;

    g_slist_foreach (monitors, (GFunc)g_file_monitor_cancel, NULL);
    g_slist_free (monitors);
    g_object_set_data (G_OBJECT (extension), "monitors", NULL);
}

static void
addons_deactivate_cb (MidoriExtension* extension,
                      MidoriApp*   app)
{
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    KatzeArray* browsers;
    MidoriBrowser* browser;
    GSource* source;

    addons_disable_monitors (extension);
    addons_save_settings (NULL, extension);
    midori_web_settings_remove_style (settings, "addons");

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        addons_browser_destroy (browser, extension);

    source = g_object_get_data (G_OBJECT (extension), "monitor-timer");
    if (source && !g_source_is_destroyed (source))
        g_source_destroy (source);

    g_signal_handlers_disconnect_by_func (
          app, addons_app_add_browser_cb, extension);
    g_signal_handlers_disconnect_by_func (
          app, addons_save_settings, extension);
    g_signal_handlers_disconnect_by_func (
          extension, addons_deactivate_cb, app);

    g_object_unref (browsers);
    g_object_unref (settings);
}

static gboolean
addons_reset_all_elements_cb (MidoriExtension* extension)
{
    addons_save_settings (NULL, extension);
    addons_update_elements (extension, ADDONS_USER_STYLES);
    addons_update_elements (extension, ADDONS_USER_SCRIPTS);
    g_object_set_data (G_OBJECT (extension), "monitor-timer", NULL);
    return FALSE;
}

static void
addons_directory_monitor_changed (GFileMonitor*     monitor,
                                  GFile*            child,
                                  GFile*            other_file,
                                  GFileMonitorEvent flags,
                                  MidoriExtension*  extension)
{
    GFileInfo* info;
    GSource* source;

    info = g_file_query_info (child,
        "standard::is-hidden,standard::is-backup", 0, NULL, NULL);
    if (info != NULL)
    {
        gboolean hidden = g_file_info_get_is_hidden (info)
                       || g_file_info_get_is_backup (info);
        g_object_unref (info);
        if (hidden)
            return;
    }

    /* We receive a lot of change events, so we use a timeout to trigger
       elements update only once */
    source = g_object_get_data (G_OBJECT (extension), "monitor-timer");
    if (source && !g_source_is_destroyed (source))
        g_source_destroy (source);

    source = g_timeout_source_new_seconds (1);
    g_source_set_callback (source, (GSourceFunc)addons_reset_all_elements_cb,
                           extension, NULL);
    g_source_attach (source, NULL);
    g_object_set_data (G_OBJECT (extension), "monitor-timer", source);
    g_source_unref (source);
}

static void
addons_monitor_directories (MidoriExtension* extension,
                            AddonsKind kind)
{
    GSList* directories;
    GError* error;
    GSList* monitors;
    GFileMonitor* monitor;
    GFile* directory;

    g_assert (kind == ADDONS_USER_SCRIPTS || kind == ADDONS_USER_STYLES);

    monitors = g_object_get_data (G_OBJECT (extension), "monitors");

    directories = addons_get_directories (kind);
    while (directories)
    {
        directory = g_file_new_for_path (directories->data);
        directories = g_slist_next (directories);
        error = NULL;
        monitor = g_file_monitor_directory (directory,
                                            G_FILE_MONITOR_NONE,
                                            NULL, &error);
        if (monitor)
        {
            g_signal_connect (monitor, "changed",
                G_CALLBACK (addons_directory_monitor_changed), extension);
            monitors = g_slist_prepend (monitors, monitor);
        }
        else
        {
            g_warning (_("Can't monitor folder '%s': %s"),
                       g_file_get_parse_name (directory), error->message);
            g_error_free (error);
        }
        g_object_unref (directory);
    }
    g_object_set_data (G_OBJECT (extension), "monitors", monitors);
    g_slist_free (directories);
}

static void
addons_activate_cb (MidoriExtension* extension,
                    MidoriApp*       app)
{
    MidoriWebSettings* settings = katze_object_get_object (app, "settings");
    KatzeArray* browsers;
    MidoriBrowser* browser;
    gchar* data;

    browsers = katze_object_get_object (app, "browsers");
    addons_update_elements (extension, ADDONS_USER_STYLES);
    addons_monitor_directories (extension, ADDONS_USER_STYLES);
    addons_update_elements (extension, ADDONS_USER_SCRIPTS);
    addons_monitor_directories (extension, ADDONS_USER_SCRIPTS);
    data = addons_generate_global_stylesheet (extension);
    midori_web_settings_add_style (settings, "addons", data);

    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        addons_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);
    g_object_unref (settings);
    g_free (data);

    g_signal_connect (app, "add-browser",
        G_CALLBACK (addons_app_add_browser_cb), extension);

    g_signal_connect (app, "quit",
        G_CALLBACK (addons_save_settings), extension);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (addons_deactivate_cb), app);
}

#ifdef G_ENABLE_DEBUG
static void
test_addons_simple_regexp (void)
{
    typedef struct
    {
        const gchar* before;
        const gchar* after;
    } RegexItem;
    guint i;

    static const RegexItem items[] = {
    { "*", "^.*" },
    { "http://", "^http://" },
    { "https://", "^https://" },
    { "about:blank", "^about:blank" },
    { "file://", "^file://" },
    { "ftp://", "^ftp://" },
    { "https://bugzilla.mozilla.org/", "^https://bugzilla\\.mozilla\\.org/" },
    { "http://92.48.103.52/fantasy3/*", "^http://92\\.48\\.103\\.52/fantasy3/.*" },
    { "http://www.rpg.co.uk/fantasy/*", "^http://www\\.rpg\\.co\\.uk/fantasy/.*" },
    { "http://cookpad.com/recipe/*", "^http://cookpad\\.com/recipe/.*" },
    { "https://*/*post_bug.cgi", "^https://.*/.*post_bug\\.cgi" },
    };

    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        gchar* result = addons_convert_to_simple_regexp (items[i].before);
        const gchar* after = items[i].after ? items[i].after : items[i].before;
        katze_assert_str_equal (items[i].before, result, after);
        g_free (result);
    }
}

void
extension_test (void)
{
    g_test_add_func ("/extensions/addons/simple_regexp", test_addons_simple_regexp);
}
#endif

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("User addons"),
        "description", _("Support for userscripts and userstyles"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Arno Renevier <arno@renevier.net>",
        NULL);
    g_signal_connect (extension, "activate",
        G_CALLBACK (addons_activate_cb), NULL);

    return extension;
}
