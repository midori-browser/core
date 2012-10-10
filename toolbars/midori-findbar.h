/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_FINDBAR_H__
#define __MIDORI_FINDBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_FINDBAR \
    (midori_findbar_get_type ())
#define MIDORI_FINDBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_FINDBAR, MidoriFindbar))
#define MIDORI_FINDBAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_FINDBAR, MidoriFindbarClass))
#define MIDORI_IS_FINDBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_FINDBAR))
#define MIDORI_IS_FINDBAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_FINDBAR))
#define MIDORI_FINDBAR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_FINDBAR, MidoriFindbarClass))

typedef struct _MidoriFindbar                MidoriFindbar;
typedef struct _MidoriFindbarClass           MidoriFindbarClass;

GType
midori_findbar_get_type               (void);

void
midori_findbar_invoke                 (MidoriFindbar* findbar,
                                       const gchar*   selected_text);

void
midori_findbar_continue               (MidoriFindbar* findbar,
                                       gboolean       forward);

const gchar*
midori_findbar_get_text                (MidoriFindbar* findbar);

void
midori_findbar_set_can_find           (MidoriFindbar* findbar,
                                       gboolean       can_find);

void
midori_findbar_search_text            (MidoriFindbar* findbar,
                                       GtkWidget*     view,
                                       gboolean       found,
                                       const gchar*   typing);

void
midori_findbar_set_close_button_left  (MidoriFindbar* findbar,
                                       gboolean       close_button_left);

G_END_DECLS

#endif /* __MIDORI_FINDBAR_H__ */
