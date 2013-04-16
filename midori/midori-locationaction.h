/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_LOCATION_ACTION_H__
#define __MIDORI_LOCATION_ACTION_H__

#include <katze/katze.h>
#include "midori-view.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_LOCATION_ACTION \
    (midori_location_action_get_type ())
#define MIDORI_LOCATION_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_LOCATION_ACTION, MidoriLocationAction))
#define MIDORI_LOCATION_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  MIDORI_TYPE_LOCATION_ACTION, MidoriLocationActionClass))
#define MIDORI_IS_LOCATION_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_LOCATION_ACTION))
#define MIDORI_IS_LOCATION_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  MIDORI_TYPE_LOCATION_ACTION))
#define MIDORI_LOCATION_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  MIDORI_TYPE_LOCATION_ACTION, MidoriLocationActionClass))

typedef struct _MidoriLocationAction         MidoriLocationAction;
typedef struct _MidoriLocationActionClass    MidoriLocationActionClass;

GType
midori_location_action_get_type             (void) G_GNUC_CONST;

const gchar*
midori_location_action_get_text             (MidoriLocationAction* location_action);

void
midori_location_action_set_text             (MidoriLocationAction* location_action,
                                             const gchar*          text);

void
midori_location_action_set_title_for_uri    (MidoriLocationAction* location_action,
                                             const gchar*          title,
                                             const gchar*          text);

void
midori_location_action_set_search_engines   (MidoriLocationAction* location_action,
                                             KatzeArray*           search_engines);

gdouble
midori_location_action_get_progress         (MidoriLocationAction* location_action);

void
midori_location_action_set_progress         (MidoriLocationAction* location_action,
                                             gdouble               progress);

void
midori_location_action_set_secondary_icon   (MidoriLocationAction* location_action,
                                             const gchar*          stock_id);

void
midori_location_action_delete_item_from_uri (MidoriLocationAction* location_action,
                                             const gchar*          uri);

void
midori_location_action_clear                (MidoriLocationAction* location_action);

void
midori_location_action_set_primary_icon     (MidoriLocationAction* location_action,
                                             GIcon*                icon,
                                             const gchar*          tooltip);

void
midori_location_action_set_security_hint    (MidoriLocationAction* location_action,
                                             MidoriSecurity        hint);

gchar*
midori_location_action_render_uri           (gchar**      keys,
                                             const gchar* uri_escaped);

gchar*
midori_location_action_render_title         (gchar**      keys,
                                             const gchar* title);

G_END_DECLS

#endif /* __MIDORI_LOCATION_ACTION_H__ */
