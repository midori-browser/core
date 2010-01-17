/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_SEPARATOR_ACTION_H__
#define __KATZE_SEPARATOR_ACTION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define KATZE_TYPE_SEPARATOR_ACTION \
    (katze_separator_action_get_type ())
#define KATZE_SEPARATOR_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_SEPARATOR_ACTION, \
    KatzeSeparatorAction))
#define KATZE_SEPARATOR_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  KATZE_TYPE_SEPARATOR_ACTION, \
    KatzeSeparatorActionClass))
#define KATZE_IS_SEPARATOR_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_SEPARATOR_ACTION))
#define KATZE_IS_SEPARATOR_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  KATZE_TYPE_SEPARATOR_ACTION))
#define KATZE_SEPARATOR_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  KATZE_TYPE_SEPARATOR_ACTION, \
    KatzeSeparatorActionClass))

typedef struct _KatzeSeparatorAction         KatzeSeparatorAction;
typedef struct _KatzeSeparatorActionClass    KatzeSeparatorActionClass;

GType
katze_separator_action_get_type              (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __KATZE_SEPARATOR_ACTION_H__ */
