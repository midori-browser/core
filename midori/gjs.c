/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "gjs.h"

#include <gmodule.h>
#include <glib/gi18n.h>

#define G_OBJECT_NAME(object) G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object))

JSValueRef
gjs_script_eval (JSContextRef js_context,
                 const gchar* script,
                 gchar**      exception)
{
    g_return_val_if_fail (js_context, FALSE);
    g_return_val_if_fail (script, FALSE);

    JSStringRef js_script = JSStringCreateWithUTF8CString (script);
    JSValueRef js_exception = NULL;
    JSValueRef js_value = JSEvaluateScript (js_context, js_script,
        JSContextGetGlobalObject (js_context), NULL, 0, &js_exception);
    if (!js_value && exception)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        *exception = gjs_string_utf8 (js_message);
        JSStringRelease (js_message);
        js_value = JSValueMakeNull (js_context);
    }
    JSStringRelease (js_script);
    return js_value;
}

gboolean
gjs_script_check_syntax (JSContextRef js_context,
                         const gchar* script,
                         gchar**      exception)
{
    g_return_val_if_fail (js_context, FALSE);
    g_return_val_if_fail (script, FALSE);

    JSStringRef js_script = JSStringCreateWithUTF8CString (script);
    JSValueRef js_exception = NULL;
    bool result = JSCheckScriptSyntax (js_context, js_script, NULL,
                                       0, &js_exception);
    if (!result && exception)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        *exception = gjs_string_utf8 (js_message);
        JSStringRelease (js_message);
    }
    JSStringRelease (js_script);
    return result ? TRUE : FALSE;
}

gboolean
gjs_script_from_file (JSContextRef js_context,
                      const gchar* filename,
                      gchar**      exception)
{
    g_return_val_if_fail (js_context, FALSE);
    g_return_val_if_fail (filename, FALSE);

    gboolean result = FALSE;
    gchar* script;
    GError* error = NULL;
    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        if (gjs_script_eval (js_context, script, exception))
            result = TRUE;
        g_free (script);
    }
    else if (error)
    {
        *exception = g_strdup (error->message);
        g_error_free (error);
    }
    else
        *exception = g_strdup (_("An unknown error occured."));
    return result;
}

gchar*
gjs_string_utf8 (JSStringRef js_string)
{
    size_t size_utf8 = JSStringGetMaximumUTF8CStringSize (js_string);
    gchar* string_utf8 = g_new (gchar, size_utf8);
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
        guint n;
        GParamSpec** pspecs = g_object_class_list_properties (
            G_OBJECT_GET_CLASS (object), &n);
        gint i;
        for (i = 0; i < n; i++)
        {
            const gchar* property = g_param_spec_get_name (pspecs[i]);
            JSStringRef js_property = JSStringCreateWithUTF8CString (property);
            JSPropertyNameAccumulatorAddName (js_properties, js_property);
            JSStringRelease (js_property);
        }
        GType type = G_OBJECT_TYPE (object);
        do
        {
            guint* signals = g_signal_list_ids (type, &n);
            for (i = 0; i < n; i++)
            {
                const gchar* signal = g_signal_name (signals[i]);
                JSStringRef js_signal = JSStringCreateWithUTF8CString (signal);
                JSPropertyNameAccumulatorAddName (js_properties, js_signal);
                JSStringRelease (js_signal);
            }
            type = g_type_parent (type);
        }
        while (type);
    }
}

static bool
_js_class_has_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property)
{
    bool result = false;
    gchar* property = gjs_string_utf8 (js_property);
    GObject* object = JSObjectGetPrivate (js_object);
    if (object)
    {
        if (g_signal_lookup (property, G_OBJECT_TYPE (object)) ||
            g_object_class_find_property (G_OBJECT_GET_CLASS (object), property))
            result = true;
    }
    else if (js_object == JSContextGetGlobalObject (js_context))
    {
        GType type = g_type_from_name (property);
        result = type ? type : false;
    }
    g_free (property);
    return result;
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
_js_object_call_as_function_cb (JSContextRef     js_context,
                                JSObjectRef      js_function,
                                JSObjectRef      js_this,
                                size_t           n_arguments,
                                const JSValueRef js_arguments[],
                                JSValueRef*      js_exception)
{
    GObject* object = JSObjectGetPrivate (js_this);
    const gchar* function = JSObjectGetPrivate (js_function);

    g_return_val_if_fail (G_IS_OBJECT (object), JSValueMakeNull (js_context));
    g_return_val_if_fail (function, JSValueMakeNull (js_context));

    guint signal_id = g_signal_lookup (function, G_OBJECT_TYPE (object));
    GSignalQuery query;
    g_signal_query (signal_id, &query);
    GValue* values = g_new0 (GValue, n_arguments + 1);
    g_value_init (&values[0], G_OBJECT_TYPE (object));
    g_value_set_instance (&values[0], object);
    gint i;
    for (i = 0; i < n_arguments; i++)
    {
        GValue value = {0, };
        GType gtype;
        switch (JSValueGetType (js_context, js_arguments[i]))
        {
            case kJSTypeBoolean:
                gtype = G_TYPE_BOOLEAN;
                g_value_init (&value, gtype);
                g_value_set_boolean (&value,
                    JSValueToBoolean (js_context, js_arguments[i]) ? TRUE : FALSE);
                break;
            case kJSTypeNumber:
                gtype = G_TYPE_DOUBLE;
                g_value_init (&value, gtype);
                g_value_set_double (&value,
                    JSValueToNumber (js_context, js_arguments[i], NULL));
                break;
            case kJSTypeString:
                gtype = G_TYPE_STRING;
                g_value_init (&value, gtype);
                JSStringRef js_string = JSValueToStringCopy (js_context,
                    js_arguments[i], NULL);
                gchar* string = gjs_string_utf8 (js_string);
                g_value_set_string (&value, string);
                g_free (string);
                JSStringRelease (js_string);
                break;
            case kJSTypeObject:
                gtype = G_TYPE_OBJECT;
                g_value_init (&value, gtype);
                JSObjectRef js_object = JSValueToObject (js_context,
                    js_arguments[i], NULL);
                GObject* object_value = JSObjectGetPrivate (js_object);
                g_value_set_object (&value, object_value);
                break;
            case kJSTypeUndefined:
            case kJSTypeNull:
            default:
                gtype = G_TYPE_NONE;
                g_value_init (&value, gtype);
        }
        g_value_init (&values[i + 1], gtype);
        if (query.n_params >= i
            && g_value_type_compatible (gtype, query.param_types[i]))
            /* && g_value_type_transformable (gtype, query.param_types[i]) */
            g_value_copy (&value, &values[i + 1]);
            /* g_value_transform (&value, &values[i + 1]); */
        else
        {
            gchar* value_type = g_strdup_value_contents (&value);
            /* FIXME: exception */
            g_print ("wrong value, expected %s\n", value_type);
            g_free (value_type);
        }
        g_value_unset (&value);
    }
    GValue return_value = {0, };
    if (query.return_type != G_TYPE_NONE)
        g_value_init (&return_value, query.return_type);
    g_signal_emitv (values, signal_id, 0, &return_value);

    for (i = 0; i < n_arguments; i++)
        g_value_unset (&values[i]);
    /* FIXME: return value */
    return JSValueMakeUndefined (js_context);
}

/*static void
_js_object_add_function (JSContextRef js_context,
                         JSObjectRef  js_object,
                         const gchar* func)
{
    JSStringRef js_func = JSStringCreateWithUTF8CString (func);
    JSObjectRef js_function = JSObjectMakeFunctionWithCallback (
        js_context, js_func, _js_object_call_as_function_cb);
    JSStringRelease (js_func);
    _js_object_set_property (js_context, js_object, func, js_function);
}*/

static JSValueRef
_js_class_get_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property,
                           JSValueRef*  js_exception)
{
    GObject* object = JSObjectGetPrivate (js_object);

    g_return_val_if_fail (G_IS_OBJECT (object), JSValueMakeNull (js_context));

    JSValueRef js_result = NULL;
    gchar* property = gjs_string_utf8 (js_property);
    guint signal_id;
    GParamSpec* pspec;
    if ((signal_id = g_signal_lookup (property, G_OBJECT_TYPE (object))))
    {
        GSignalQuery query;
        g_signal_query (signal_id, &query);
        if (query.signal_flags & G_SIGNAL_ACTION)
        {
            /* We can't use JSObjectMakeFunctionWithCallback
               because it doesn't allocate private data */
            JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
            js_class_def.className = g_strdup (property);
            js_class_def.callAsFunction = _js_object_call_as_function_cb;
            JSClassRef js_class = JSClassCreate (&js_class_def);
            JSObjectRef js_function = JSObjectMake (js_context, js_class, property);
            return js_function;
        }
        g_free (property);
        return JSValueMakeNull (js_context);
    }
    else if (!(pspec = g_object_class_find_property (
        G_OBJECT_GET_CLASS (object), property)))
    {
        gchar* message = g_strdup_printf (_("%s has no property '%s'"),
            G_OBJECT_NAME (object), property);
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
    else if (type == G_TYPE_PARAM_INT || type == G_TYPE_PARAM_UINT)
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
            js_result = gjs_object_new (js_context,
                G_OBJECT_NAME (value), value);
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
    return js_result ? js_result : JSValueMakeNull (js_context);
}

static void
_js_class_signal_callback (GObject* object, ...)
{
    /* FIXME: Support arbitrary signatures */

    /* FIXME: Support arbitrary return values */
}

static bool
_js_class_set_property_cb (JSContextRef js_context,
                           JSObjectRef  js_object,
                           JSStringRef  js_property,
                           JSValueRef   js_value,
                           JSValueRef*  js_exception)
{
    GObject* object = JSObjectGetPrivate (js_object);

    g_return_val_if_fail (G_IS_OBJECT (object), false);

    bool result = false;
    gchar* property = gjs_string_utf8 (js_property);
    guint signal_id;
    GParamSpec* pspec;
    if ((signal_id = g_signal_lookup (property, G_OBJECT_TYPE (object))))
    {
        g_signal_connect (object, property,
            G_CALLBACK (_js_class_signal_callback), (gpointer)js_value);
        g_free (property);
        return true;
    }
    else if (!(pspec = g_object_class_find_property (
        G_OBJECT_GET_CLASS (object), property)))
    {
        gchar* message = g_strdup_printf (_("%s has no property '%s'"),
            G_OBJECT_NAME (object), property);
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
            gchar* string_value = gjs_string_utf8 (js_string_value);
            g_object_set (object, property, string_value, NULL);
            g_free (string_value);
        }
    }
    else if (type == G_TYPE_PARAM_INT || type == G_TYPE_PARAM_UINT)
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
        if (G_IS_OBJECT (object_value))
            g_object_set (object, property, object_value, NULL);
        else
        {
            gchar* message = g_strdup_printf (_("%s cannot be assigned to %s.%s"),
            "[object]", G_OBJECT_NAME (object), property);
            JSStringRef js_message = JSStringCreateWithUTF8CString (message);
            *js_exception = JSValueMakeString (js_context, js_message);
            JSStringRelease (js_message);
            g_free (message);
        }
    }
    else
    {
        gchar* message = g_strdup_printf (_("%s.%s cannot be accessed"),
            G_OBJECT_NAME (object), property);
        JSStringRef js_message = JSStringCreateWithUTF8CString (message);
        *js_exception = JSValueMakeString (js_context, js_message);
        JSStringRelease (js_message);
        g_free (message);
    }
    g_free (property);
    return result;
}

JSObjectRef
gjs_object_new (JSContextRef js_context,
                const gchar* name,
                gpointer     instance)
{
    g_return_val_if_fail (js_context, NULL);
    g_return_val_if_fail (name, NULL);

    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    js_class_def.className = g_strdup (name);
    js_class_def.getPropertyNames = _js_class_get_property_names_cb;
    js_class_def.hasProperty = _js_class_has_property_cb;
    js_class_def.getProperty = _js_class_get_property_cb;
    js_class_def.setProperty = _js_class_set_property_cb;
    JSClassRef js_class = JSClassCreate (&js_class_def);
    JSObjectRef js_object = JSObjectMake (js_context, js_class, instance);
    return js_object;
}

static JSObjectRef
_js_object_call_as_constructor_cb (JSContextRef     js_context,
                                   JSObjectRef      js_object,
                                   size_t           n_arguments,
                                   const JSValueRef js_arguments[],
                                   JSValueRef*      js_exception)
{
    const gchar* type_name = JSObjectGetPrivate (js_object);

    g_return_val_if_fail (type_name, NULL);

    GType type = g_type_from_name (type_name);
    if (type)
        return gjs_object_new (js_context, type_name, g_object_new (type, NULL));
    return NULL;
}

static bool
_js_module_has_property_cb (JSContextRef js_context,
                            JSObjectRef  js_object,
                            JSStringRef  js_property)
{
    const gchar* namespace = JSObjectGetPrivate (js_object);

    g_return_val_if_fail (namespace, false);

    gchar* property = gjs_string_utf8 (js_property);
    gchar* type_name = g_strdup_printf ("%s%s", namespace, property);

    GType type = g_type_from_name (type_name);
    if (!type)
    {
        GModule* module = g_module_open (NULL,
            G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
        typedef GType (*gjs_get_type_func)(void);
        /* FIXME: Insert a space between each capital letter */
        gchar* type_func_name = g_strdup_printf ("%s_%s_get_type",
                                                 namespace, property);
        gchar* type_func_name_small = g_utf8_strdown (type_func_name, -1);
        gjs_get_type_func type_func;
        if (g_module_symbol (module,
            (const gchar*)type_func_name_small, (gpointer*)&type_func))
        {
            type = type_func ();
            g_type_class_peek (type);
        }
        g_free (type_func_name_small);
        g_free (type_func_name);
        g_module_close (module);
    }
    bool result = type ? true : false;
    g_free (type_name);
    g_free (property);
    return result;
}

static JSValueRef
_js_module_get_property_cb (JSContextRef js_context,
                            JSObjectRef  js_object,
                            JSStringRef  js_property,
                            JSValueRef*  js_exception)
{
    const gchar* namespace = JSObjectGetPrivate (js_object);

    g_return_val_if_fail (namespace, JSValueMakeNull (js_context));

    gchar* property = gjs_string_utf8 (js_property);
    gchar* type_name = g_strdup_printf ("%s%s", namespace, property);
    GType type = g_type_from_name (type_name);
    JSValueRef result;
    if (type)
    {
        JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
        js_class_def.className = g_strdup (type_name);
        js_class_def.callAsConstructor = _js_object_call_as_constructor_cb;
        JSClassRef js_class = JSClassCreate (&js_class_def);
        result = JSObjectMake (js_context, js_class, type_name);
    }
    else
    {
        result = JSValueMakeNull (js_context);
        g_free (type_name);
    }
    g_free (property);
    return result;
}

JSObjectRef
gjs_module_new (JSContextRef js_context,
                const gchar* namespace)
{
    g_return_val_if_fail (js_context, NULL);
    g_return_val_if_fail (namespace, NULL);

    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    js_class_def.className = g_strdup (namespace);
    js_class_def.hasProperty = _js_module_has_property_cb;
    js_class_def.getProperty = _js_module_get_property_cb;
    JSClassRef js_class = JSClassCreate (&js_class_def);
    JSObjectRef js_module = JSObjectMake (js_context, js_class, (gpointer)namespace);
    return js_module;
}

JSGlobalContextRef
gjs_global_context_new (void)
{
    JSGlobalContextRef js_context = JSGlobalContextCreate (NULL);
    JSObjectRef js_object = gjs_object_new (js_context, "GJS", NULL);
    _js_object_set_property (js_context, JSContextGetGlobalObject (js_context),
                             "gjs", js_object);

    js_object = gjs_module_new (js_context, "Gtk");
    _js_object_set_property (js_context, JSContextGetGlobalObject (js_context),
                             "Gtk", js_object);
    js_object = gjs_module_new (js_context, "WebKit");
    _js_object_set_property (js_context, JSContextGetGlobalObject (js_context),
                             "WebKit", js_object);
    js_object = gjs_module_new (js_context, "Midori");
    _js_object_set_property (js_context, JSContextGetGlobalObject (js_context),
                             "Midori", js_object);
    return js_context;
}
