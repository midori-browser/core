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

KatzeArray*
midori_search_engines_new_from_file        (const gchar*        filename,
                                            GError**            error);


KatzeArray*
midori_search_engines_new_from_folder      (GString*            error_messages);

gboolean
midori_search_engines_save_to_file         (KatzeArray*         search_engines,
                                            const gchar*        filename,
                                            GError**            error);

void
midori_search_engines_set_filename         (KatzeArray*         search_engines,
                                            const gchar*        filename);

G_END_DECLS

#endif /* __MIDORI_SEARCH_ACTION_H__ */
