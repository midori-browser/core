/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "gjs.h"

#include <glib/gi18n.h>

#define G_OBJECT_NAME(object) G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object))

// FIXME: Return a GValue
JSValueRef
gjs_script_eval (JSContextRef js_context,
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
    gboolean result = FALSE;
    gchar* script;
    GError* error = NULL;
    if (g_file_get_contents (filename, &script, NULL, &error))
    {
        if (gjs_script_eval (js_context, script, exception))
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
    gchar* property = gjs_string_utf8 (js_property);
    GObject* object = JSObjectGetPrivate (js_object);
    if (object)
    {
        if (g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                          property))
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
            return gjs_object_new (js_context, g_object_new (type, NULL));
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
        gchar* property = gjs_string_utf8 (js_property);
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
        gchar* property = gjs_string_utf8 (js_property);
        GParamSpec* pspec = g_object_class_find_property (
            G_OBJECT_GET_CLASS (object), property);
        if (!pspec)
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
                js_result = gjs_object_new (js_context, value);
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
        gchar* property = gjs_string_utf8 (js_property);
        GParamSpec* pspec = g_object_class_find_property (
            G_OBJECT_GET_CLASS (object), property);
        if (!pspec)
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
    }
    return result;
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
_js_object_add_function (JSContextRef js_context,
                         JSObjectRef  js_object,
                         const gchar* func)
{
    JSStringRef js_func = JSStringCreateWithUTF8CString (func);
    JSObjectRef js_function = JSObjectMakeFunctionWithCallback (
        js_context, js_func, _js_object_call_as_function_cb);
    JSStringRelease (js_func);
    _js_object_set_property (js_context, js_object, func, js_function);
}

JSObjectRef
gjs_object_new (JSContextRef js_context,
                gpointer     instance)
{
    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    js_class_def.className = g_strdup (G_OBJECT_NAME (instance));
    js_class_def.getPropertyNames = _js_class_get_property_names_cb;
    js_class_def.hasProperty = _js_class_has_property_cb;
    js_class_def.getProperty = _js_class_get_property_cb;
    js_class_def.setProperty = _js_class_set_property_cb;
    JSClassRef js_class = JSClassCreate (&js_class_def);
    JSObjectRef js_object = JSObjectMake (js_context, js_class, instance);
    if (instance && G_IS_OBJECT (instance))
    {
        // TODO: Add functions dynamically
        /*if (GTK_IS_WIDGET (instance))
        {
            _js_object_add_function (js_context, js_object, "show");
        }*/
    }
    return js_object;
}
