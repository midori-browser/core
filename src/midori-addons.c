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

static const
gchar* _folder_for_kind (MidoriAddonKind kind)
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
midori_addons_button_add_clicked_cb (GtkToolItem*  toolitem,
                                     MidoriAddons* addons)
{
    MidoriAddonsPrivate* priv = addons->priv;

    GtkWidget* dialog = gtk_message_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (addons))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "Put scripts in the folder ~/.local/share/midori/%s",
        _folder_for_kind (priv->kind));
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
    bool result = false;
    gchar* property = _js_string_utf8 (js_property);
    GObject* object = JSObjectGetPrivate (js_object);
    if (object)
    {
        if (g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                          property))
            result = true;
    }
    else if (js_object == JSContextGetGlobalObject (js_context))
    {
        gchar* property = _js_string_utf8 (js_property);
        GType type = g_type_from_name (property);
        result = type ? type : false;
    }
    g_free (property);
    return result;
}

static JSObjectRef
_js_object_new (JSContextRef js_context,
                gpointer     object);

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

static JSObjectRef
_js_object_call_as_constructor_cb (JSContextRef     js_context,
                                   JSObjectRef      js_object,
                                   size_t           n_arguments,
                                   const JSValueRef js_arguments[],
                                   JSValueRef*      js_exception)
{
    gchar* type_name = JSObjectGetPrivate (js_object);
    if (type_name)
    {
        GType type = g_type_from_name (type_name);
        if (type)
        {
            GObject* object = g_object_new (type, NULL);
            JSObjectRef js_object = _js_object_new (js_context, object);
            return js_object;
        }
    }
    return JSValueMakeNull (js_context);
}

static JSValueRef
_js_class_get_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property,
                           JSValueRef*  js_exception)
{
    if (js_object == JSContextGetGlobalObject (js_context))
    {
        gchar* property = _js_string_utf8 (js_property);
        GType type = g_type_from_name (property);
        if (type)
        {
            JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
            js_class_def.className = g_strdup (property);
            js_class_def.callAsConstructor = _js_object_call_as_constructor_cb;
            JSClassRef js_class = JSClassCreate (&js_class_def);
            return JSObjectMake (js_context, js_class, property);
        }
        g_free (property);
        return JSValueMakeNull (js_context);
    }
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
            g_free (property);
            return JSValueMakeNull (js_context);
        }
        if (!(pspec->flags & G_PARAM_READABLE))
        {
            g_free (property);
            return JSValueMakeUndefined (js_context);
        }
        GType type = G_PARAM_SPEC_TYPE (pspec);
        if (type == G_TYPE_PARAM_STRING)
        {
            gchar* value;
            g_object_get (object, property, &value, NULL);
            if (value)
            {
                JSStringRef js_string = JSStringCreateWithUTF8CString (value);
                js_result = JSValueMakeString (js_context, js_string);
            }
        }
        else if (type == G_TYPE_PARAM_INT
            || type == G_TYPE_PARAM_UINT)
        {
            gint value;
            g_object_get (object, property, &value, NULL);
            js_result = JSValueMakeNumber (js_context, value);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            gboolean value;
            g_object_get (object, property, &value, NULL);
            js_result = JSValueMakeBoolean (js_context, value ? true : false);
        }
        else if (type == G_TYPE_PARAM_OBJECT)
        {
            GObject* value;
            g_object_get (object, property, &value, NULL);
            if (value)
                js_result = _js_object_new (js_context, value);
        }
        else if (type == G_TYPE_PARAM_ENUM)
        {
            gint value;
            g_object_get (object, property, &value, NULL);
            js_result = JSValueMakeNumber (js_context, value);
        }
        else
            js_result = JSValueMakeUndefined (js_context);
        g_free (property);
    }
    return js_result ? js_result : JSValueMakeNull (js_context);
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
            g_free (property);
            return false;
        }
        if (!(pspec->flags & G_PARAM_WRITABLE))
        {
            g_free (property);
            return false;
        }
        GType type = G_PARAM_SPEC_TYPE (pspec);
        if (type == G_TYPE_PARAM_STRING)
        {
            JSStringRef js_string_value = JSValueToStringCopy (js_context,
                js_value, js_exception);
            if (js_string_value)
            {
                gchar* string_value = _js_string_utf8 (js_string_value);
                g_object_set (object, property, string_value, NULL);
                g_free (string_value);
            }
        }
        else if (type == G_TYPE_PARAM_INT
            || type == G_TYPE_PARAM_UINT)
        {
            int value = JSValueToNumber (js_context, js_value,
                                          js_exception);
            g_object_set (object, property, value, NULL);
        }
        else if (type == G_TYPE_PARAM_BOOLEAN)
        {
            bool value = JSValueToBoolean (js_context, js_value);
            g_object_set (object, property, value ? TRUE : FALSE, NULL);
        }
        else if (type == G_TYPE_PARAM_OBJECT)
        {
            JSObjectRef js_object_value = JSValueToObject (
                js_context, js_value, NULL);
            GObject* object_value = JSObjectGetPrivate (js_object_value);
            if (object_value)
                g_object_set (object, property, object_value, NULL);
            else
            {
                gchar* message = g_strdup_printf (_("%s cannot be assigned to %s.%s"),
                "[object]", KATZE_OBJECT_NAME (object), property);
                JSStringRef js_message = JSStringCreateWithUTF8CString (message);
                *js_exception = JSValueMakeString (js_context, js_message);
                JSStringRelease (js_message);
                g_free (message);
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

static JSValueRef
_js_foo_call_as_function_cb (JSContextRef     js_context,
                             JSObjectRef      js_function,
                             JSObjectRef      js_this,
                             size_t           n_arguments,
                             const JSValueRef js_arguments[],
                             JSValueRef*      js_exception)
{
    GObject* object = JSObjectGetPrivate (js_this);
    if (object)
    {
        if (!n_arguments)
        {
            gtk_widget_show (GTK_WIDGET (object));
        }
        else if (n_arguments == 1)
        {
            JSObjectRef js_arg1 = JSValueToObject (
                js_context, js_arguments[0], NULL);
            GObject* arg1 = JSObjectGetPrivate (js_arg1);
            if (arg1)
                gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (arg1));
        }
    }

    return JSValueMakeUndefined (js_context);
}

static void
_js_foo_add_function (JSContextRef js_context,
                      JSObjectRef  js_object,
                      const gchar* func)
{
    JSStringRef js_func = JSStringCreateWithUTF8CString (func);
    JSObjectRef js_function = JSObjectMakeFunctionWithCallback (
        js_context, js_func, _js_foo_call_as_function_cb);
    JSStringRelease (js_func);
    _js_object_set_property (js_context, js_object, func, js_function);
}

static JSObjectRef
_js_object_new (JSContextRef js_context,
                gpointer     object)
{
    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    js_class_def.className = g_strdup (KATZE_OBJECT_NAME (object));
    js_class_def.getPropertyNames = _js_class_get_property_names_cb;
    js_class_def.hasProperty = _js_class_has_property_cb;
    js_class_def.getProperty = _js_class_get_property_cb;
    js_class_def.setProperty = _js_class_set_property_cb;
    JSClassRef js_class = JSClassCreate (&js_class_def);
    JSObjectRef js_object = JSObjectMake (js_context, js_class, object);
    if (GTK_IS_WIDGET (object))
    {
        _js_foo_add_function (js_context, js_object, "show");
    }
    if (GTK_IS_CONTAINER (object))
    {
        _js_foo_add_function (js_context, js_object, "add");
    }
    return js_object;
}

static JSValueRef
_js_eval (JSContextRef js_context,
          const gchar* script,
          gchar**      exception)
{
    JSStringRef js_script = JSStringCreateWithUTF8CString (script);
    JSValueRef js_exception = NULL;
    JSValueRef js_value = JSEvaluateScript (js_context, js_script,
        JSContextGetGlobalObject (js_context), NULL, 0, &js_exception);
    if (!js_value && exception)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        *exception = _js_string_utf8 (js_message);
        JSStringRelease (js_message);
        js_value = JSValueMakeNull (js_context);
    }
    JSStringRelease (js_script);
    return js_value;
}

static bool
_js_check_syntax (JSContextRef js_context,
                  const gchar* script,
                  const gchar* source_id,
                  int          line,
                  gchar**      exception)
{
    JSStringRef js_script = JSStringCreateWithUTF8CString (script);
    JSStringRef js_source_id = JSStringCreateWithUTF8CString (source_id);
    JSValueRef js_exception = NULL;
    bool result = JSCheckScriptSyntax (js_context, js_script, js_source_id,
                                       line, &js_exception);
    if (!result && exception)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        *exception = _js_string_utf8 (js_message);
        JSStringRelease (js_message);
    }
    JSStringRelease (js_source_id);
    JSStringRelease (js_script);
    return result;
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

static JSValueRef
_js_info_call_as_function_cb (JSContextRef     js_context,
                              JSObjectRef      js_function,
                              JSObjectRef      js_this,
                              size_t           n_arguments,
                              const JSValueRef js_arguments[],
                              JSValueRef*      js_exception)
{
    if (n_arguments > 0) {
        JSStringRef js_string = JSValueToStringCopy (
            js_context, js_arguments[0], NULL);
        gchar* string = _js_string_utf8 (js_string);
        // FIXME: Do we want to print this somewhere else?
        printf ("console.info: %s\n", string);
        g_free (string);
        JSStringRelease (js_string);
    }

    return JSValueMakeUndefined (js_context);
}

static void
_midori_addons_extensions_main (MidoriAddons* addons,
                                GtkWidget*    web_widget)
{
    MidoriAddonsPrivate* priv = addons->priv;

    JSClassDefinition js_global_def = kJSClassDefinitionEmpty;
    js_global_def.getPropertyNames = _js_class_get_property_names_cb;
    js_global_def.hasProperty = _js_class_has_property_cb;
    js_global_def.getProperty = _js_class_get_property_cb;
    JSClassRef js_global_class = JSClassCreate (&js_global_def);
    JSGlobalContextRef js_context = JSGlobalContextCreate (js_global_class);
    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    js_class_def.className = g_strdup ("console");
    JSClassRef js_class = JSClassCreate (&js_class_def);
    JSObjectRef js_console = JSObjectMake (js_context, js_class, NULL);
    JSStringRef js_info = JSStringCreateWithUTF8CString ("info");
    JSObjectRef js_info_function = JSObjectMakeFunctionWithCallback (
        js_context, js_info, _js_info_call_as_function_cb);
    JSObjectSetProperty (js_context, js_console, js_info, js_info_function,
                         kJSPropertyAttributeNone, NULL);
    JSStringRelease (js_info);
    _js_object_set_property (js_context,
                             JSContextGetGlobalObject (js_context),
                             "console", js_console);

    GtkWidget* browser = gtk_widget_get_toplevel (GTK_WIDGET (web_widget));
    if (GTK_WIDGET_TOPLEVEL (browser))
    {
        // FIXME: Midori should be backed up by a real GObject
        JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
        js_class_def.className = g_strdup ("Midori");
        JSClassRef js_class = JSClassCreate (&js_class_def);
        JSObjectRef js_midori = JSObjectMake (js_context, js_class, NULL);
        _js_object_set_property (js_context,
                                 JSContextGetGlobalObject (js_context),
                                 "midori", js_midori);
        JSObjectRef js_browser = _js_object_new (js_context, browser);
        _js_object_set_property (js_context,
                                 js_midori,
                                 "browser", js_browser);
    }

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
            gchar* exception = NULL;
            _js_document_load_script_file (js_context, fullname, &exception);
            if (exception)
            // FIXME: Do we want to print this somewhere else?
            // FIXME Convert the filename to UTF8
                printf ("%s - Exception: %s\n", filename, exception);
            g_free (fullname);
        }
        g_dir_close (addon_dir);
    }
    JSGlobalContextRelease (js_context);
}

static void
midori_web_widget_window_object_cleared_cb (GtkWidget*         web_widget,
                                            WebKitWebFrame*    web_frame,
                                            JSGlobalContextRef js_context,
                                            JSObjectRef        js_window,
                                            MidoriAddons*      addons)
{
    MidoriAddonsPrivate* priv = addons->priv;

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

    if (kind == MIDORI_ADDON_EXTENSIONS)
        _midori_addons_extensions_main (addons, web_widget);
    else if (kind == MIDORI_ADDON_USER_SCRIPTS)
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
