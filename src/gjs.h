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
