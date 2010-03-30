/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/*
midori.tabs
midori.windows
midori.currentWindow
midori.createAction({name:'Example',label:'_Example',icon:'gtk-info',tooltip:'Examples',activate:function()})
action = midori.currentWindow.getAction(name)
foreach (window as midori.windows) window.getAction('myaction').activate()
midori.window[i].tabs
midori.addWindow
midori.addTab
midori.removeWindow
midori.removeTab
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include <midori/midori.h>
#include <midori/sokoke.h>
#include <JavaScriptCore/JavaScript.h>

gchar*
sokoke_js_string_utf8 (JSStringRef js_string)
{
    size_t size_utf8;
    gchar* string_utf8;

    g_return_val_if_fail (js_string, NULL);

    size_utf8 = JSStringGetMaximumUTF8CStringSize (js_string);
    string_utf8 = g_new (gchar, size_utf8);
    JSStringGetUTF8CString (js_string, string_utf8, size_utf8);
    return string_utf8;
}

static void
javascript_deactivate_cb (MidoriExtension* extension,
                          MidoriBrowser*   browser)
{
    /* FIXME: Unload all javascript extensions */
}

static JSValueRef
midori_javascript_midori_action_cb (JSContextRef     js_context,
                                    JSObjectRef      js_function,
                                    JSObjectRef      js_this,
                                    size_t           n_arguments,
                                    const JSValueRef js_arguments[],
                                    JSValueRef*      js_exception)
{
    GtkAction* action;
    JSObjectRef js_meta;
    JSPropertyNameArrayRef js_properties;
    size_t n_properties;
    guint i;
    MidoriApp* app;

    if (n_arguments != 1)
    {
        *js_exception = JSValueMakeString (js_context,
                                           JSStringCreateWithUTF8CString (
            "MidoriError: Wrong number of arguments; 'midori.createAction ({property:value, ...})'"));
        return JSValueMakeNull (js_context);
    }

    if (!JSValueIsObject (js_context, js_arguments[0]))
    {
        *js_exception = JSValueMakeString (js_context,
                                           JSStringCreateWithUTF8CString (
            "MidoriError: Argument is not an object; 'midori.createAction ({property:value, ...})'"));
        return JSValueMakeNull (js_context);
    }

    action = g_object_new (GTK_TYPE_ACTION, NULL);
    js_meta = JSValueToObject (js_context, js_arguments[0], NULL);
    js_properties = JSObjectCopyPropertyNames (js_context, js_meta);
    n_properties = JSPropertyNameArrayGetCount (js_properties);
    for (i = 0; i < n_properties; i++)
    {
        JSStringRef js_name;
        gchar* name;
        JSValueRef js_property;
        JSStringRef js_string;
        gchar* string;

        js_name = JSPropertyNameArrayGetNameAtIndex (js_properties, i);
        name = sokoke_js_string_utf8 (js_name);
        if (g_str_equal (name, "label"))
        {
            js_property = JSObjectGetProperty (js_context, js_meta, js_name, NULL);
            js_string = JSValueToStringCopy (js_context, js_property, NULL);
            string = sokoke_js_string_utf8 (js_string);
            JSStringRelease (js_string);
            g_object_set (action, "label", string, NULL);
            g_free (string);
        }
        else if (g_str_equal (name, "icon"))
        {
            js_property = JSObjectGetProperty (js_context, js_meta, js_name, NULL);
            js_string = JSValueToStringCopy (js_context, js_property, NULL);
            string = sokoke_js_string_utf8 (js_string);
            JSStringRelease (js_string);
            /* FIXME: stock-id, or icon-name, or URI */
            g_object_set (action, "stock-id", string, NULL);
            g_free (string);
        }
        else if (g_str_equal (name, "tooltip"))
        {
            js_property = JSObjectGetProperty (js_context, js_meta, js_name, NULL);
            js_string = JSValueToStringCopy (js_context, js_property, NULL);
            string = sokoke_js_string_utf8 (js_string);
            JSStringRelease (js_string);
            g_object_set (action, "tooltip", string, NULL);
            g_free (string);
        }
        else if (g_str_equal (name, "activate"))
        {
            /* FIXME */
        }
        else
        {
            *js_exception = JSValueMakeString (js_context,
                                               JSStringCreateWithUTF8CString (
                "MidoriError: Unknown property; 'midori.createAction ({property:value, ...})'"));
            return JSValueMakeNull (js_context);
        }
    }

    if ((app = JSObjectGetPrivate (js_this)))
    {
        /* TODO: Offer the user to add a toolbar button */
    }
    /* TODO: add action to all existing and future browsers */
    /* gtk_action_connect_accelerator */
    return JSValueMakeNull (js_context);
}

static JSContextRef
midori_javascript_context (MidoriApp* app)
{
    JSContextRef js_context = JSGlobalContextCreateInGroup (NULL, NULL);
    JSClassDefinition js_class_def = kJSClassDefinitionEmpty;
    JSClassRef js_class;
    JSObjectRef js_object;
    JSStringRef js_name;
    JSStaticFunction functions[] = {
        { "createAction", midori_javascript_midori_action_cb, kJSPropertyAttributeNone },
        { NULL, NULL, 0 }
    };

    js_class_def.className = "midori";
    js_class_def.staticFunctions = functions;
    js_class = JSClassCreate (&js_class_def);
    js_object = JSObjectMake (js_context, js_class, app);

    js_name = JSStringCreateWithUTF8CString ("midori");
    JSObjectSetProperty (js_context, JSContextGetGlobalObject (js_context),
                         js_name, js_object, kJSPropertyAttributeNone, NULL);
    JSStringRelease (js_name);

    return js_context;
}

static void
midori_javascript_extension_activate_cb (MidoriExtension* extension,
                                         MidoriApp*       app)
{
    gchar* filename = g_object_get_data (G_OBJECT (extension), "filename");
    gchar* fullname = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
        PACKAGE_NAME, "extensions", filename, NULL);
    gchar* script;
    GError* error = NULL;
    if (g_file_get_contents (fullname, &script, NULL, &error))
    {
        JSContextRef js_context = midori_javascript_context (app);
        gchar* exception = NULL;
        g_free (sokoke_js_script_eval (js_context, script, &exception));
        if (exception)
        {
            g_object_set (extension, "description",
                          exception, "version", NULL, NULL);
            g_warning ("%s", exception);
            g_free (exception);
        }
    }
    else
    {
        g_object_set (extension, "description",
                      error->message, "version", NULL, NULL);
        g_warning ("%s", error->message);
        g_error_free (error);
    }
    g_free (fullname);
    g_free (script);
}

static void
javascript_load_extensions (gchar**      active,
                            MidoriApp*   app,
                            const gchar* path)
{
    GDir* dir;

    /* TODO: Monitor folder for new files or modifications at runtime */
    if ((dir = g_dir_open (path, 0, NULL)))
    {
        KatzeArray* extensions = katze_object_get_object (app, "extensions");
        JSContextRef js_context = midori_javascript_context (app);
        const gchar* filename;

        while ((filename = g_dir_read_name (dir)))
        {
            gchar* fullname;
            GError* error;
            gchar* script;
            MidoriExtension* extension;

            /* Ignore files which don't have the correct suffix */
            if (!g_str_has_suffix (filename, ".js"))
                continue;

            fullname = g_build_filename (path, filename, NULL);
            error = NULL;
            extension = g_object_new (MIDORI_TYPE_EXTENSION, "name", filename, NULL);
            if (g_file_get_contents (fullname, &script, NULL, &error))
            {
                JSStringRef js_script;
                JSValueRef js_exception;

                js_script = JSStringCreateWithUTF8CString (script);
                if (JSCheckScriptSyntax (js_context, js_script, NULL,
                                         0, &js_exception))
                {
                    /* FIXME: Read meta data from .js file */
                    g_object_set (extension, "description", "",
                                  "version", "0.1", "authors", "", NULL);
                    /* Signal that we want the extension to load and save */
                    g_object_set_data_full (G_OBJECT (extension), "filename",
                                            g_strdup (filename), g_free);
                    if (midori_extension_is_prepared (extension))
                        midori_extension_get_config_dir (extension);
                    g_signal_connect (extension, "activate",
                        G_CALLBACK (midori_javascript_extension_activate_cb), NULL);
                }
                else
                {
                    JSStringRef js_string = JSValueToStringCopy (js_context,
                        js_exception, NULL);
                    gchar* string = sokoke_js_string_utf8 (js_string);
                    JSStringRelease (js_string);
                    g_object_set (extension, "description", string, NULL);
                    g_warning ("%s", string);
                    g_free (string);
                }
            }
            else
            {
                g_object_set (extension, "description", error->message, NULL);
                g_warning ("%s", error->message);
                g_error_free (error);
            }
            g_free (fullname);
            katze_array_add_item (extensions, extension);
            if (active)
            {
                guint i = 0;
                gchar* name;
                while ((name = active[i++]))
                    if (!g_strcmp0 (filename, name))
                        g_signal_emit_by_name (extension, "activate", app);
            }
            /* FIXME main.c needs to monitor extensions
            g_signal_connect_after (extension, "activate",
                G_CALLBACK (extension_activate_cb), app);
            g_signal_connect_after (extension, "deactivate",
                G_CALLBACK (extension_activate_cb), app); */
            g_object_unref (extension);
        }
        g_object_unref (extensions);
        g_dir_close (dir);
    }
}

static void
javascript_activate_cb (MidoriExtension* extension,
                        MidoriApp*       app)
{
    gchar** active = midori_extension_get_string_list (extension, "extensions", NULL);
    /* FIXME Scan system data dirs */
    gchar* path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
                                PACKAGE_NAME, "extensions", NULL);
    javascript_load_extensions (active, app, path);
    g_free (path);
    g_strfreev (active);

    g_signal_connect (extension, "deactivate",
        G_CALLBACK (javascript_deactivate_cb), NULL);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Javascript extensions"),
        "description", _("Enable extensions written in Javascript"),
        "version", "0.1",
        "authors", "Christian Dywan <christian@twotoasts.de>",
        NULL);
    midori_extension_install_string_list (extension, "extensions", NULL, G_MAXSIZE);

    g_signal_connect (extension, "activate",
        G_CALLBACK (javascript_activate_cb), NULL);

    return extension;
}
