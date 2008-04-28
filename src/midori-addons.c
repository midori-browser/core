/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "config.h"

#include "midori-addons.h"

#include "sokoke.h"
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (MidoriAddons, midori_addons, GTK_TYPE_VBOX)

struct _MidoriAddonsPrivate
{
    MidoriAddonKind kind;
    GtkWidget* toolbar;
    GtkWidget* treeview;
};

#define MIDORI_ADDONS_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_ADDONS, MidoriAddonsPrivate))

GType
midori_addon_kind_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_ADDON_EXTENSIONS, "MIDORI_ADDON_EXTENSIONS", N_("Extensions") },
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
    g_type_class_add_private (class, sizeof (MidoriAddonsPrivate));
}

static void
midori_addons_button_add_clicked_cb (GtkToolItem*  toolitem,
                                     MidoriAddons* addons)
{
    GtkWidget* dialog = gtk_message_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "Put scripts in the folder ~/.local/share/midori/scripts");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
midori_addons_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    // gchar* source_id;
    // gtk_tree_model_get (model, iter, 2, &source_id, -1);

    g_object_set (renderer, "stock-id", GTK_STOCK_FILE, NULL);

    // g_free (source_id);
}

static void
midori_addons_treeview_render_text_cb (GtkTreeViewColumn* column,
                                       GtkCellRenderer*   renderer,
                                       GtkTreeModel*      model,
                                       GtkTreeIter*       iter,
                                       GtkWidget*         treeview)
{
    gchar* filename;
    gint   a;
    gchar* b;
    gtk_tree_model_get (model, iter, 0, &filename, 1, &a, 2, &b, -1);

    // FIXME: Convert filename to UTF8
    gchar* text = g_strdup_printf ("%s", filename);
    g_object_set (renderer, "text", text, NULL);
    g_free (text);

    g_free (filename);
    g_free (b);
}

static void
midori_addons_treeview_row_activated_cb (GtkTreeView*       treeview,
                                         GtkTreePath*       path,
                                         GtkTreeViewColumn* column,
                                         MidoriAddons*     addons)
{
    /*GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gchar* b;
        gtk_tree_model_get (model, &iter, 2, &b, -1);
        g_free (b);
    }*/
}

static void
midori_addons_init (MidoriAddons* addons)
{
    addons->priv = MIDORI_ADDONS_GET_PRIVATE (addons);

    MidoriAddonsPrivate* priv = addons->priv;

    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    priv->treeview = gtk_tree_view_new ();
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_icon_cb,
        priv->treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_addons_treeview_render_text_cb,
        priv->treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column);
    g_signal_connect (priv->treeview, "row-activated",
                      G_CALLBACK (midori_addons_treeview_row_activated_cb),
                      addons);
    gtk_widget_show (priv->treeview);
    gtk_box_pack_start (GTK_BOX (addons), priv->treeview, TRUE, TRUE, 0);
}

static gchar*
_js_string_utf8 (JSStringRef js_string)
{
    size_t size_utf8 = JSStringGetMaximumUTF8CStringSize (js_string);
    gchar* string_utf8 = (gchar*)g_malloc (size_utf8);
    JSStringGetUTF8CString (js_string, string_utf8, size_utf8);
    return string_utf8;
}

static void
_js_class_get_property_names_cb (JSContextRef                 js_context,
                                 JSObjectRef                  js_object,
                                 JSPropertyNameAccumulatorRef js_properties)
{
    GObject* object = JSObjectGetPrivate (js_object);
    if (object)
    {
        guint n_properties;
        GParamSpec** pspecs = g_object_class_list_properties (
            G_OBJECT_GET_CLASS (object), &n_properties);
        gint i;
        for (i = 0; i < n_properties; i++)
        {
            GType type = G_PARAM_SPEC_TYPE (pspecs[i]);
            const gchar* property = g_param_spec_get_name (pspecs[i]);
            JSStringRef js_property = JSStringCreateWithUTF8CString (property);
            JSPropertyNameAccumulatorAddName (js_properties, js_property);
            JSStringRelease (js_property);
        }
    }
}

static bool
_js_class_has_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property)
{
    GObject* object = JSObjectGetPrivate (js_object);
    bool result = false;
    if (object)
    {
        gchar* property = _js_string_utf8 (js_property);
        if (g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                          property))
            result = true;
        g_free (property);
    }
    return result;
}

static JSValueRef
_js_class_get_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property,
                           JSValueRef*  js_exception)
{
    GObject* object = JSObjectGetPrivate (js_object);
    JSValueRef js_result = NULL;
    if (object)
    {
        gchar* property = _js_string_utf8 (js_property);
        GParamSpec* pspec = g_object_class_find_property (
            G_OBJECT_GET_CLASS (object), property);
        if (!pspec)
        {
            gchar* message = g_strdup_printf (_("%s has no property '%s'"),
                KATZE_OBJECT_NAME (object), property);
            JSStringRef js_message = JSStringCreateWithUTF8CString (message);
            *js_exception = JSValueMakeString (js_context, js_message);
            JSStringRelease (js_message);
            g_free (message);
        }
        GType type = G_PARAM_SPEC_TYPE (pspec);
        if (type == G_TYPE_PARAM_STRING)
        {
            gchar* value;
            g_object_get (object, property, &value, NULL);
            JSStringRef js_string = JSStringCreateWithUTF8CString (value);
            js_result = JSValueMakeString (js_context, js_string);
        }
        else
        {
            gchar* message = g_strdup_printf (_("%s.%s cannot be accessed"),
                KATZE_OBJECT_NAME (object), property);
            JSStringRef js_message = JSStringCreateWithUTF8CString (message);
            *js_exception = JSValueMakeString (js_context, js_message);
            JSStringRelease (js_message);
            g_free (message);
        }
        g_free (property);
    }
    return js_result;
}

static bool
_js_class_set_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property,
                           JSValueRef   js_value,
                           JSValueRef*  js_exception)
{
    GObject* object = JSObjectGetPrivate (js_object);
    bool result = false;
    if (object)
    {
        gchar* property = _js_string_utf8 (js_property);
        GParamSpec* pspec = g_object_class_find_property (
            G_OBJECT_GET_CLASS (object), property);
        if (!pspec)
        {
            gchar* message = g_strdup_printf (_("%s has no property '%s'"),
                KATZE_OBJECT_NAME (object), property);
            JSStringRef js_message = JSStringCreateWithUTF8CString (message);
            *js_exception = JSValueMakeString (js_context, js_message);
            JSStringRelease (js_message);
            g_free (message);
        }
        if (!(pspec->flags & G_PARAM_WRITABLE))
        {
            g_free (property);
            return false;
        }
        GType type = G_PARAM_SPEC_TYPE (pspec);
        if (type == G_TYPE_PARAM_STRING)
        {
            JSStringRef js_string = JSValueToStringCopy (js_context, js_value,
                                                         js_exception);
            if (js_string)
            {
                gchar* string = _js_string_utf8 (js_string);
                g_object_set (object, property, string, NULL);
                g_free (string);
            }
        }
        else
        {
            gchar* message = g_strdup_printf (_("%s.%s cannot be accessed"),
                KATZE_OBJECT_NAME (object), property);
            JSStringRef js_message = JSStringCreateWithUTF8CString (message);
            *js_exception = JSValueMakeString (js_context, js_message);
            JSStringRelease (js_message);
            g_free (message);
        }
        g_free (property);
    }
    return result;
}

static JSObjectRef
_js_object_new (JSContextRef js_context,
                GObject*     object)
{
    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    js_class_def.className = g_strdup (KATZE_OBJECT_NAME (object));
    js_class_def.getPropertyNames = _js_class_get_property_names_cb;
    js_class_def.hasProperty = _js_class_has_property_cb;
    js_class_def.getProperty = _js_class_get_property_cb;
    js_class_def.setProperty = _js_class_set_property_cb;
    // js_class_def.staticFunctions = JSStaticFunction*;
    JSClassRef js_class = JSClassCreate (&js_class_def);
    return JSObjectMake (js_context, js_class, object);
}

static void
_js_object_set_property (JSContextRef js_context,
                         JSObjectRef  js_object,
                         const gchar* name,
                         JSValueRef   js_value)
{
    JSStringRef js_name = JSStringCreateWithUTF8CString (name);
    JSObjectSetProperty(js_context, js_object, js_name, js_value,
                        kJSPropertyAttributeNone, NULL);
    JSStringRelease (js_name);
}

static JSValueRef
_js_eval (JSContextRef js_context,
          const gchar* script,
          gchar**      exception)
{
    JSStringRef js_script = JSStringCreateWithUTF8CString (script);
    JSValueRef js_exception;
    JSValueRef js_value = JSEvaluateScript (js_context, js_script,
        JSContextGetGlobalObject (js_context),
        NULL, 0, &js_exception);
    if (!js_value && exception)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        *exception = _js_string_utf8 (js_message);
        JSStringRelease (js_message);
    }
    JSStringRelease (js_script);
    return js_value;
}

static gboolean
_js_document_load_script_file (JSContextRef js_context,
                               const gchar* filename,
                               gchar**      exception)
{
    gboolean result = FALSE;
    gchar* script;
    GError* error = NULL;
    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        if (_js_eval (js_context, script, exception))
            result = TRUE;
        g_free (script);
    }
    else
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    return result;
}

static const gchar* _folder_for_kind (MidoriAddonKind kind)
{
    switch (kind)
    {
    case MIDORI_ADDON_EXTENSIONS:
        return "extensions";
    case MIDORI_ADDON_USER_SCRIPTS:
        return "scripts";
    case MIDORI_ADDON_USER_STYLES:
        return "styles";
    default:
        return NULL;
    }
}

static void
midori_web_widget_window_object_cleared_cb (GtkWidget*         web_widget,
                                            WebKitWebFrame*    web_frame,
                                            JSGlobalContextRef js_context,
                                            JSObjectRef        js_window,
                                            MidoriAddons*      addons)
{
    MidoriAddonsPrivate* priv = addons->priv;

    GObject* settings;
    g_object_get (web_widget, "settings", &settings, NULL);
    JSObjectRef js_settings = _js_object_new (js_context, settings);
    _js_object_set_property (js_context,
                             JSContextGetGlobalObject (js_context),
                             KATZE_OBJECT_NAME (settings), js_settings);

    // FIXME: We want to honor system installed addons as well
    gchar* addon_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME,
                                          _folder_for_kind (priv->kind), NULL);
    GDir* addon_dir = g_dir_open (addon_path, 0, NULL);
    if (addon_dir)
    {
        const gchar* filename;
        while ((filename = g_dir_read_name (addon_dir)))
        {
            gchar* fullname = g_build_filename (addon_path, filename, NULL);
            gchar* exception;
            if (!_js_document_load_script_file (js_context, fullname,
                                                &exception))
            {
                gchar* message = g_strdup_printf ("console.error ('%s');",
                                                  exception);
                _js_eval (js_context, message, NULL);
                g_free (message);
                g_free (exception);
            }
            g_free (fullname);
        }
        g_dir_close (addon_dir);
    }
}

/**
 * midori_addons_new:
 * @web_widget: a web widget
 * @kind: the kind of addon
 * @extension: a file extension mask
 *
 * Creates a new addons widget.
 *
 * @web_widget can be one of the following:
 *     %MidoriBrowser, %MidoriWebView, %WebKitWebView
 *
 * Note: Currently @extension has no effect.
 *
 * Return value: a new #MidoriAddons
 **/
GtkWidget*
midori_addons_new (GtkWidget*      web_widget,
                   MidoriAddonKind kind)
{
    g_return_val_if_fail (GTK_IS_WIDGET (web_widget), NULL);

    MidoriAddons* addons = g_object_new (MIDORI_TYPE_ADDONS,
                                         // "kind", kind,
                                         NULL);

    MidoriAddonsPrivate* priv = addons->priv;
    priv->kind = kind;

    if (kind == MIDORI_ADDON_USER_SCRIPTS)
        g_signal_connect (web_widget, "window-object-cleared",
            G_CALLBACK (midori_web_widget_window_object_cleared_cb), addons);

    GtkListStore* liststore = gtk_list_store_new (3, G_TYPE_STRING,
                                                     G_TYPE_INT,
                                                     G_TYPE_STRING);
    // FIXME: We want to honor system installed addons as well
    gchar* addon_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME,
                                          _folder_for_kind (priv->kind), NULL);
    GDir* addon_dir = g_dir_open (addon_path, 0, NULL);
    if (addon_dir)
    {
        const gchar* filename;
        while ((filename = g_dir_read_name (addon_dir)))
        {
            GtkTreeIter iter;
            gtk_list_store_append (liststore, &iter);
            gtk_list_store_set (liststore, &iter,
                0, filename, 1, 0, 2, "", -1);
        }
        g_dir_close (addon_dir);
    }
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview),
                             GTK_TREE_MODEL (liststore));

    return GTK_WIDGET (addons);
}

/**
 * midori_addons_get_toolbar:
 *
 * Retrieves the toolbar of the addons. A new widget is created on
 * the first call of this function.
 *
 * Return value: a new #MidoriAddons
 **/
GtkWidget*
midori_addons_get_toolbar (MidoriAddons* addons)
{
    MidoriAddonsPrivate* priv = addons->priv;

    g_return_val_if_fail (MIDORI_IS_ADDONS (addons), NULL);

    if (!priv->toolbar)
    {
        GtkWidget* toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        GtkToolItem* toolitem = gtk_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_separator_tool_item_new ();
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem),
                                          FALSE);
        gtk_tool_item_set_expand (toolitem, TRUE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_ADD);
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_addons_button_add_clicked_cb), addons);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        priv->toolbar = toolbar;
    }

    return priv->toolbar;
}
