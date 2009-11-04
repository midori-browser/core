/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Arnaud Renevier <arenevier@fdn.fr>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-addons.h"
#include "midori-stock.h"

#include "sokoke.h"

#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <glib/gi18n.h>
#include <string.h>
#include <gio/gio.h>

struct _MidoriAddons
{
    GtkVBox parent_instance;

    GtkWidget* web_widget;
    GtkWidget* toolbar;
    GtkWidget* treeview;

    GSList* elements;
};

struct _MidoriAddonsClass
{
    GtkVBoxClass parent_class;
};

struct AddonElement
{
    gchar *fullpath;
    gchar *name;
    gchar *description;
    gboolean enabled;
    gboolean broken;

    GSList* includes;
    GSList* excludes;
};

static void
midori_addons_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriAddons, midori_addons, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_addons_viewable_iface_init));

enum
{
    PROP_0,

    PROP_KIND,
    PROP_WEB_WIDGET
};

static void
midori_addons_finalize (GObject* object);

static void
midori_addons_set_property (GObject*      object,
                            guint         prop_id,
                            const GValue* value,
                            GParamSpec*   pspec);

static void
midori_addons_get_property (GObject*    object,
                            guint       prop_id,
                            GValue*     value,
                            GParamSpec* pspec);

GType
midori_addon_kind_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ADDON_NONE, "MIDORI_ADDON_NONE", N_("None") },
         { MIDORI_ADDON_USER_SCRIPTS, "MIDORI_USER_SCRIPTS", N_("Userscripts") },
         { MIDORI_ADDON_USER_STYLES, "MIDORI_USER_STYLES", N_("Userstyles") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriAddonKind", values);
    }
    return type;
}

static void
midori_addons_class_init (MidoriAddonsClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_addons_finalize;
    gobject_class->set_property = midori_addons_set_property;
    gobject_class->get_property = midori_addons_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_KIND,
                                     g_param_spec_enum (
                                     "kind",
                                     "Kind",
                                     "The kind of addons",
                                     MIDORI_TYPE_ADDON_KIND,
                                     MIDORI_ADDON_NONE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_WEB_WIDGET,
                                     g_param_spec_object (
                                     "web-widget",
                                     "Web Widget",
                                     "The assigned web widget",
                                     GTK_TYPE_WIDGET,
                                     G_PARAM_READWRITE));
}

static const gchar*
midori_addons_get_label (MidoriViewable* viewable)
{
    return _("Userscripts");
}

static const gchar*
midori_addons_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_SCRIPTS;
}

static void
midori_addons_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_addons_get_stock_id;
    iface->get_label = midori_addons_get_label;
    iface->get_toolbar = midori_addons_get_toolbar;
}

static void
midori_addons_set_property (GObject*      object,
                            guint         prop_id,
                            const GValue* value,
                            GParamSpec*   pspec)
{
    MidoriAddons* addons = MIDORI_ADDONS (object);

    switch (prop_id)
    {
    case PROP_KIND:
        /* Ignored */
        break;
    case PROP_WEB_WIDGET:
        katze_object_assign (addons->web_widget, g_value_dup_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_addons_get_property (GObject*    object,
                            guint       prop_id,
                            GValue*     value,
                            GParamSpec* pspec)
{
    MidoriAddons* addons = MIDORI_ADDONS (object);

    switch (prop_id)
    {
    case PROP_KIND:
        g_value_set_enum (value, MIDORI_ADDON_USER_SCRIPTS);
        break;
    case PROP_WEB_WIDGET:
        g_value_set_object (value, addons->web_widget);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static GSList*
_addons_get_directories (MidoriAddons* addons)
{
    const gchar* folders[] = { "scripts", "styles" };
    GSList *directories;
    guint i;
    const char* const* datadirs;
    gchar* path;

    directories = NULL;

    for (i = 0; i < G_N_ELEMENTS (folders); i++)
    {
        path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                             PACKAGE_NAME, folders[i], NULL);
        directories = g_slist_prepend (directories, path);

        datadirs = g_get_system_data_dirs ();
        while (*datadirs)
        {
            path = g_build_path (G_DIR_SEPARATOR_S, *datadirs,
                                 PACKAGE_NAME, folders[i], NULL);
            directories = g_slist_prepend (directories, path);
            datadirs++;
        }
    }

    return directories;
}

static GSList*
_addons_get_files (MidoriAddons* addons)
{
    GSList* files;
    GDir* addon_dir;
    GSList* list;
    GSList* directories;
    const gchar* filename;
    gchar* dirname;
    gchar* fullname;

    files = NULL;

    directories = _addons_get_directories (addons);
    list = directories;
    while (directories)
    {
        dirname = directories->data;
        if ((addon_dir = g_dir_open (dirname, 0, NULL)))
        {
            while ((filename = g_dir_read_name (addon_dir)))
            {
                if (g_str_has_suffix (filename, ".js")
                    || g_str_has_suffix (filename, ".css"))
                {
                    fullname = g_build_filename (dirname, filename, NULL);
                    files = g_slist_prepend (files, fullname);
                }
            }
            g_dir_close (addon_dir);
        }
        g_free (dirname);
        directories = g_slist_next (directories);
    }
    g_slist_free (list);

    return files;
}

static void
midori_addons_directory_monitor_changed (GFileMonitor*     monitor,
                                         GFile*            child,
                                         GFile*            other_file,
                                         GFileMonitorEvent flags,
                                         MidoriAddons*     addons)
{
    midori_addons_update_elements (addons);
}

static void
midori_addons_button_add_clicked_cb (GtkToolItem*  toolitem,
                                     MidoriAddons* addons)
{
    gchar* path_scripts, *path_styles;
    GtkWidget* dialog;

    path_scripts = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                                 PACKAGE_NAME, "scripts", NULL);
    path_styles = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                                 PACKAGE_NAME, "styles", NULL);
    dialog = gtk_message_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        _("Copy userscripts to the folder %s and "
        "copy userstyles to the folder %s."), path_scripts, path_styles);
    g_free (path_scripts);
    g_free (path_styles);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
midori_addons_treeview_render_tick_cb (GtkTreeViewColumn* column,
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
midori_addons_treeview_render_text_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    struct AddonElement *element;

    gtk_tree_model_get (model, iter, 0, &element, -1);

    g_object_set (renderer, "text", element->name, NULL);
    if (!element->enabled)
        g_object_set (renderer, "sensitive", false, NULL);
    else
        g_object_set (renderer, "sensitive", true, NULL);
}

static void
midori_addons_treeview_row_activated_cb (GtkTreeView*       treeview,
                                         GtkTreePath*       path,
                                         GtkTreeViewColumn* column,
                                         MidoriAddons*     addons)
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

static void
midori_addons_cell_renderer_toggled_cb (GtkCellRendererToggle* renderer,
                                        const gchar*           path,
                                        MidoriAddons*          addons)
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
midori_addons_init (MidoriAddons* addons)
{
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_toggle;

    addons->web_widget = NULL;
    addons->elements = NULL;

    addons->treeview = gtk_tree_view_new ();
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (addons->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_toggle = gtk_cell_renderer_toggle_new ();
    gtk_tree_view_column_pack_start (column, renderer_toggle, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_toggle,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_tick_cb,
        addons->treeview, NULL);
    g_signal_connect (renderer_toggle, "toggled",
        G_CALLBACK (midori_addons_cell_renderer_toggled_cb), addons);
    gtk_tree_view_append_column (GTK_TREE_VIEW (addons->treeview), column);
    column = gtk_tree_view_column_new ();
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_text_cb,
        addons->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (addons->treeview), column);
    g_signal_connect (addons->treeview, "row-activated",
                      G_CALLBACK (midori_addons_treeview_row_activated_cb),
                      addons);
    gtk_widget_show (addons->treeview);
    gtk_box_pack_start (GTK_BOX (addons), addons->treeview, TRUE, TRUE, 0);
}

static void
midori_addons_finalize (GObject* object)
{
    MidoriAddons* addons = MIDORI_ADDONS (object);

    katze_object_assign (addons->web_widget, NULL);

    g_slist_free (addons->elements);
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
        if (g_str_has_prefix (line, "// ==UserScript=="))
            found_meta = TRUE;
        else if (found_meta)
        {
            if (g_str_has_prefix (line, "// ==/UserScript=="))
                found_meta = FALSE;
            else if (g_str_has_prefix (line, "// @require ") ||
                g_str_has_prefix (line, "// @resource "))
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
             else if (includes && g_str_has_prefix (line, "// @include "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @include "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *includes = g_slist_prepend (*includes, rest_of_line);
             }
             else if (excludes && g_str_has_prefix (line, "// @exclude "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @exclude "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *excludes = g_slist_prepend (*excludes, rest_of_line);
             }
             else if (name && g_str_has_prefix (line, "// @name "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @name "));
                 rest_of_line =  g_strstrip (rest_of_line);
                 *name = rest_of_line;
             }
             else if (description && g_str_has_prefix (line, "// @description "))
             {
                 rest_of_line = g_strdup (line + strlen ("// @description "));
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

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return FALSE;

    channel = g_io_channel_new_file (filename, "r", 0);
    if (!channel)
        return FALSE;

    while (g_io_channel_read_line (channel, &line, NULL, NULL, NULL)
           == G_IO_STATUS_NORMAL)
    {
        if (g_str_has_prefix (line, "@namespace"))
            ; /* FIXME: Check "http://www.w3.org/1999/xhtml", skip otherwise */
        else if (g_str_has_prefix (line, "@-moz-document"))
        { /* FIXME: We merely look for includes. We should honor blocks. */
             if (includes)
             {
                 gchar** parts;
                 guint i;

                 rest_of_line = g_strdup (line + strlen ("@-moz-document"));
                 rest_of_line = g_strstrip (rest_of_line);
                 parts = g_strsplit (rest_of_line, " ", 0);
                 i = 0;
                 while (parts[i])
                 {
                     if (g_str_has_prefix (parts[i], "url-prefix("))
                     {
                         gchar* value = g_strdup (parts[i] + strlen ("url-prefix("));
                         guint j;

                         if (value[0] != '\'' && value[0] != '"')
                         {
                             /* Wrong syntax, abort */
                             g_free (value);
                             g_strfreev (parts);
                             g_free (line);
                             g_io_channel_shutdown (channel, false, 0);
                             g_slist_free (*includes);
                             g_slist_free (*excludes);
                             *includes = NULL;
                             *excludes = NULL;
                             return FALSE;
                         }
                         j = 1;
                         while (value[j] != '\0')
                         {
                             if (value[j] == value[0])
                                 break;
                             j++;
                         }
                         *includes = g_slist_prepend (*includes, g_strndup (value + 1, j - 1));
                         g_free (value);
                     }
                     /* FIXME: Recognize "domain" */
                     i++;
                 }
                 g_strfreev (parts);
             }
        }
        g_free (line);
    }
    g_io_channel_shutdown (channel, false, 0);
    g_io_channel_unref (channel);

    return TRUE;
}

static gchar*
_convert_to_simple_regexp (const gchar* pattern)
{
    guint len;
    gchar* dest;
    guint pos;
    guint i;
    gchar c;

    len = strlen (pattern);
    dest = g_malloc0 (len * 2 + 1);
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
_may_load_script (const gchar* uri,
                  GSList**     includes,
                  GSList**     excludes)
{
    gboolean match;
    GSList* list;
    gchar* re;

    if (*includes)
        match = FALSE;
    else
        match = TRUE;

    list = *includes;
    while (list)
    {
        re = _convert_to_simple_regexp (list->data);
        if (g_regex_match_simple (re, uri, 0, 0))
        {
            match = TRUE;
            break;
        }
        g_free (re);
        list = g_slist_next (list);
    }
    if (!match)
    {
        return FALSE;
    }
    list = *excludes;
    while (list)
    {
        re = _convert_to_simple_regexp (list->data);
        if (g_regex_match_simple (re, uri, 0, 0))
        {
            match = FALSE;
            break;
        }
        g_free (re);
        list = g_slist_next (list);
    }
    return match;
}

static gboolean
_js_script_from_file (JSContextRef js_context,
                      const gchar* filename,
                      gchar**      exception)
{
    gboolean result = FALSE;
    gchar* script;
    GError* error = NULL;
    gchar* wrapped_script;

    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        /* Wrap the script to prevent global variables */
        wrapped_script = g_strdup_printf (
            "window.addEventListener ('DOMContentLoaded',"
            "function () { %s }, true);", script);
        if (sokoke_js_script_eval (js_context, wrapped_script, exception))
            result = TRUE;
        g_free (wrapped_script);
        g_free (script);
    }
    else
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    return result;
}

static gboolean
_js_style_from_file (JSContextRef js_context,
                     const gchar* filename,
                     gchar**      exception)
{
    gboolean result;
    gchar* style;
    GError* error;
    guint i, n;
    gchar* style_script;

    result = FALSE;
    error = NULL;
    if (g_file_get_contents (filename, &style, NULL, &error))
    {
        guint meta = 0;
        n = strlen (style);
        for (i = 0; i < n; i++)
        {
            /* Replace line breaks with spaces */
            if (style[i] == '\n' || style[i] == '\r')
                style[i] = ' ';
            /* Change all single quotes to double quotes */
            if (style[i] == '\'')
                style[i] = '\"';
            /* Turn metadata we inspected earlier into comments */
            if (!meta && style[i] == '@')
            {
                style[i] = '/';
                meta++;
            }
            else if (meta == 1 && (style[i] == '-' || style[i] == 'n'))
            {
                style[i] = '*';
                meta++;
            }
            else if (meta == 2 && style[i] == '{')
            {
                style[i - 1] = '*';
                style[i] = '/';
                meta++;
            }
            else if (meta == 3 && style[i] == '{')
                meta++;
            else if (meta == 4 && style[i] == '}')
                meta--;
            else if (meta == 3 && style[i] == '}')
            {
                style[i] = ' ';
                meta = 0;
            }
        }

        style_script = g_strdup_printf (
            "window.addEventListener ('DOMContentLoaded',"
            "function () {"
            "var mystyle = document.createElement(\"style\");"
            "mystyle.setAttribute(\"type\", \"text/css\");"
            "mystyle.appendChild(document.createTextNode('%s'));"
            "var head = document.getElementsByTagName(\"head\")[0];"
            "if (head) head.appendChild(mystyle);"
            "else document.documentElement.insertBefore(mystyle, document.documentElement.firstChild);"
            "}, true);",
            style);
        if (sokoke_js_script_eval (js_context, style_script, exception))
            result = TRUE;
        g_free (style_script);
        g_free (style);
    }
    else
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    return result;
}

static void
midori_web_widget_context_ready_cb (GtkWidget*         web_widget,
                                    JSGlobalContextRef js_context,
                                    MidoriAddons*      addons)
{
    gchar* uri;
    GSList* elements;
    struct AddonElement* element;
    gchar* fullname;
    gchar* exception;
    gchar* message;

    uri = katze_object_get_string (web_widget, "uri");
    if (!uri)
        return;

    elements = addons->elements;
    while (elements)
    {
        element = elements->data;
        if (!element->enabled || element->broken)
        {
            elements = g_slist_next (elements);
            continue;
        }

        fullname = element->fullpath;

        if (element->includes || element->excludes)
            if (!_may_load_script (uri, &element->includes, &element->excludes))
            {
                elements = g_slist_next (elements);
                continue;
            }

        exception = NULL;
        if (g_str_has_suffix (fullname, ".js") &&
            !_js_script_from_file (js_context, fullname, &exception))
        {
            message = g_strdup_printf ("console.error ('%s');", exception);
            sokoke_js_script_eval (js_context, message, NULL);
            g_free (message);
            g_free (exception);
        }
        else if (g_str_has_suffix (fullname, ".css") &&
            !_js_style_from_file (js_context, fullname, &exception))
        {
            message = g_strdup_printf ("console.error ('%s');", exception);
            sokoke_js_script_eval (js_context, message, NULL);
            g_free (message);
            g_free (exception);
        }

        elements = g_slist_next (elements);
    }

    g_free (uri);
}

/**
 * midori_addons_new:
 * @kind: the kind of addon
 * @web_widget: a web widget
 *
 * Creates a new addons widget.
 *
 * @web_widget can be one of the following:
 *     %MidoriBrowser, %MidoriWebView, %WebKitWebView
 *
 * Return value: a new #MidoriAddons
 **/
GtkWidget*
midori_addons_new (MidoriAddonKind kind,
                   GtkWidget*      web_widget)
{
    MidoriAddons* addons;
    GSList* directories;
    GSList* list;
    GFile* directory;
    GError* error;
    GFileMonitor* monitor;

    g_return_val_if_fail (GTK_IS_WIDGET (web_widget), NULL);

    addons = g_object_new (MIDORI_TYPE_ADDONS,
                           "kind", kind,
                           "web-widget", web_widget,
                           NULL);

    if (kind == MIDORI_ADDON_USER_SCRIPTS || kind == MIDORI_ADDON_USER_STYLES)
        g_signal_connect (addons->web_widget, "context-ready",
            G_CALLBACK (midori_web_widget_context_ready_cb), addons);

    midori_addons_update_elements (addons);

    directories = _addons_get_directories (addons);
    list = directories;
    while (directories)
    {
        directory = g_file_new_for_path (directories->data);
        directories = g_slist_next (directories);
        error = NULL;
        monitor = g_file_monitor_directory (directory,
                                            G_FILE_MONITOR_NONE,
                                            NULL, &error);
        if (monitor)
            g_signal_connect (monitor, "changed",
                G_CALLBACK (midori_addons_directory_monitor_changed), addons);
        else
        {
            g_warning (_("Can't monitor folder '%s': %s"),
                       g_file_get_parse_name (directory), error->message);
            g_error_free (error);
        }
        g_object_unref (directory);
    }
    g_slist_free (list);

    return GTK_WIDGET (addons);
}

/**
 * midori_addons_get_toolbar:
 * @addons: a #MidoriAddons
 *
 * Retrieves the toolbar of the addons. A new widget
 * is created on the first call of this function.
 *
 * Return value: a toolbar widget
 *
 * Deprecated: 0.1.2: Use midori_viewable_get_toolbar() instead.
 **/
GtkWidget*
midori_addons_get_toolbar (MidoriViewable* addons)
{
    GtkWidget* toolbar;
    GtkToolItem* toolitem;

    g_return_val_if_fail (MIDORI_IS_ADDONS (addons), NULL);

    if (!MIDORI_ADDONS (addons)->toolbar)
    {
        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* separator */
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem),
                                          FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));

        /* add button */
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_addons_button_add_clicked_cb), addons);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        MIDORI_ADDONS (addons)->toolbar = toolbar;

        g_signal_connect (toolbar, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &MIDORI_ADDONS (addons)->toolbar);
    }

    return MIDORI_ADDONS (addons)->toolbar;
}

/**
 * midori_addons_update_elements:
 * @addons: a #MidoriAddons
 *
 * Updates all addons elements (file paths and metadata).
 *
 **/
void
midori_addons_update_elements (MidoriAddons* addons)
{
    GTree* disabled;
    GSList* elements;
    gboolean broken;
    gchar* fullname;
    gchar* displayname;
    gchar* name;
    gchar* description;
    GSList* includes;
    GSList* excludes;
    GtkListStore* liststore;
    GtkTreeIter iter;
    GSList* addon_files;
    GSList* list;
    struct AddonElement* element;

    g_return_if_fail (MIDORI_IS_ADDONS (addons));

    /* FIXME: would GHashTable be better? */
    disabled = g_tree_new ((GCompareFunc)strcmp);
    elements = addons->elements;
    while (elements)
    {
        element = elements->data;
        if (!element->enabled)
            g_tree_insert (disabled, element->fullpath, NULL);
        elements = g_slist_next (elements);
    }

    g_slist_free (addons->elements);
    addons->elements = NULL;

    liststore = gtk_list_store_new (3, G_TYPE_POINTER,
                                    G_TYPE_INT,
                                    G_TYPE_STRING);

    addon_files = _addons_get_files (addons);
    list = addon_files;
    while (addon_files)
    {
        fullname = addon_files->data;
        displayname =  g_filename_display_basename (fullname);
        description = NULL;
        includes = NULL;
        excludes = NULL;
        broken = FALSE;

        if (g_str_has_suffix (fullname, ".js"))
        {
            name = NULL;
            if (!js_metadata_from_file (fullname, &includes, &excludes,
                                        &name, &description))
                broken = TRUE;

            if (name)
            {
                g_free (displayname);
                displayname = name;
            }
        }
        else if (g_str_has_suffix (fullname, ".css"))
        {
            if (!css_metadata_from_file (fullname, &includes, &excludes))
                broken = TRUE;
        }

        element = g_new (struct AddonElement, 1);
        element->name = displayname;
        element->description = description;
        element->fullpath = fullname;

        if (g_tree_lookup_extended (disabled, fullname, NULL, NULL))
            element->enabled = FALSE;
        else
            element->enabled = TRUE;
        element->broken = broken;
        element->includes = includes;
        element->excludes = excludes;
        addons->elements = g_slist_prepend (addons->elements, element);

        gtk_list_store_append (liststore, &iter);
        gtk_list_store_set (liststore, &iter,
                            0, element, 1, 0, 2, "", -1);

        addon_files = g_slist_next (addon_files);
    }
    addons->elements = g_slist_reverse (addons->elements);

    g_tree_destroy (disabled);
    g_slist_free (list);

    gtk_tree_view_set_model (GTK_TREE_VIEW (addons->treeview),
                             GTK_TREE_MODEL (liststore));

    gtk_widget_queue_draw (GTK_WIDGET (addons->treeview));
}
