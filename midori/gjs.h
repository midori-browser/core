/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __GJS_H__
#define __GJS_H__

#include <JavaScriptCore/JavaScript.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GJS_TYPE_VALUE \
    (gjs_value_get_type ())
#define GJS_VALUE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GJS_TYPE_VALUE, GjsValue))
#define GJS_VALUE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GJS_TYPE_VALUE, GjsValueClass))
#define GJS_IS_VALUE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GJS_TYPE_VALUE))
#define GJS_IS_VALUE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GJS_TYPE_VALUE))
#define GJS_VALUE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GJS_TYPE_VALUE, GjsValueClass))

typedef struct _GjsValue GjsValue;
typedef struct _GjsValueClass GjsValueClass;

struct _GjsValueClass
{
    GObjectClass parent_class;
};

GType
gjs_value_get_type (void);

GjsValue*
gjs_value_new (JSContextRef js_context,
               JSValueRef   js_value);

gboolean
gjs_value_is_valid (GjsValue* value);

gboolean
gjs_value_is_object (GjsValue* value);

gboolean
gjs_value_has_attribute (GjsValue*    value,
                         const gchar* name);

GjsValue*
gjs_value_get_attribute (GjsValue*    value,
                         const gchar* name);

const gchar*
gjs_value_get_string (GjsValue* value);

const gchar*
gjs_value_get_attribute_string (GjsValue*    value,
                                const gchar* name);

GjsValue*
gjs_value_get_nth_attribute (GjsValue* value,
                             guint     n);

typedef void
(*GjsCallback) (GjsValue* value,
                gpointer  user_data);

void
gjs_value_foreach (GjsValue*   value,
                   GjsCallback callback,
                   gpointer    user_data);

void
gjs_value_forall (GjsValue*   value,
                  GjsCallback callback,
                  gpointer    user_data);

GjsValue*
gjs_value_get_by_name (GjsValue*    value,
                       const gchar* name);

GjsValue*
gjs_value_get_elements_by_tag_name (GjsValue*    value,
                                    const gchar* name);

GjsValue*
gjs_value_execute (GjsValue*    value,
                   const gchar* script,
                   gchar**      exception);

JSValueRef
gjs_script_eval (JSContextRef js_context,
                 const gchar* script,
                 gchar**      exception);

gboolean
gjs_script_check_syntax (JSContextRef js_context,
                         const gchar* script,
                         gchar**      exception);

gboolean
gjs_script_from_file (JSContextRef js_context,
                      const gchar* filename,
                      gchar**      exception);

gchar*
gjs_string_utf8 (JSStringRef js_string);

JSObjectRef
gjs_object_new (JSContextRef context,
                const gchar* name,
                gpointer     instance);

JSGlobalContextRef
gjs_global_context_new (void);

G_END_DECLS

#endif /* __GJS_H__ */
