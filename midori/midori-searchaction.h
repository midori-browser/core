/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_SEARCH_ACTION_H__
#define __MIDORI_SEARCH_ACTION_H__

#include <katze/katze.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_SEARCH_ACTION \
    (midori_search_action_get_type ())
#define MIDORI_SEARCH_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_SEARCH_ACTION, MidoriSearchAction))
#define MIDORI_SEARCH_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  MIDORI_TYPE_SEARCH_ACTION, MidoriSearchActionClass))
#define MIDORI_IS_SEARCH_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_SEARCH_ACTION))
#define MIDORI_IS_SEARCH_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  MIDORI_TYPE_SEARCH_ACTION))
#define MIDORI_SEARCH_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  MIDORI_TYPE_SEARCH_ACTION, MidoriSearchActionClass))

typedef struct _MidoriSearchAction         MidoriSearchAction;
typedef struct _MidoriSearchActionClass    MidoriSearchActionClass;

GType
midori_search_action_get_type              (void) G_GNUC_CONST;

const gchar*
midori_search_action_get_text              (MidoriSearchAction* action);

void
midori_search_action_set_text              (MidoriSearchAction* search_action,
                                            const gchar*        text);

KatzeArray*
midori_search_action_get_search_engines    (MidoriSearchAction* search_action);

void
midori_search_action_set_search_engines    (MidoriSearchAction* search_action,
                                            KatzeArray*         search_engines);

KatzeItem*
midori_search_action_get_current_item      (MidoriSearchAction* search_action);

void
midori_search_action_set_current_item      (MidoriSearchAction* search_action,
                                            KatzeItem*          item);

KatzeItem*
midori_search_action_get_default_item      (MidoriSearchAction* search_action);

GdkPixbuf*
midori_search_action_get_icon              (KatzeItem*          item,
                                            GtkWidget*          widget,
                                            const gchar** icon_name,
                                            gboolean      in_entry);

void
midori_search_action_set_default_item      (MidoriSearchAction* search_action,
                                            KatzeItem*          item);

GtkWidget*
midori_search_action_get_dialog            (MidoriSearchAction* search_action);

void
midori_search_action_get_editor            (MidoriSearchAction* search_action,
                                            KatzeItem*          item,
                                            gboolean            new_engine);

KatzeItem*
midori_search_action_get_engine_for_form   (WebKitWebView*      web_view,
                                            PangoEllipsizeMode  ellipsize);

G_END_DECLS

#endif /* __MIDORI_SEARCH_ACTION_H__ */
