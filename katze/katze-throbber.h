/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_THROBBER_H__
#define __KATZE_THROBBER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define KATZE_TYPE_THROBBER \
    (katze_throbber_get_type ())
#define KATZE_THROBBER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_THROBBER, KatzeThrobber))
#define KATZE_THROBBER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_THROBBER, KatzeThrobberClass))
#define KATZE_IS_THROBBER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_THROBBER))
#define KATZE_IS_THROBBER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_THROBBER))
#define KATZE_THROBBER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_THROBBER, KatzeThrobberClass))

typedef struct _KatzeThrobber                KatzeThrobber;
typedef struct _KatzeThrobberPrivate         KatzeThrobberPrivate;
typedef struct _KatzeThrobberClass           KatzeThrobberClass;

struct _KatzeThrobberClass
{
    GtkMiscClass parent_class;

    /* Padding for future expansion */
    void (*_katze_reserved1) (void);
    void (*_katze_reserved2) (void);
    void (*_katze_reserved3) (void);
    void (*_katze_reserved4) (void);
};

GType
katze_throbber_get_type             (void);

GtkWidget*
katze_throbber_new                  (void);

void
katze_throbber_set_icon_size        (KatzeThrobber*   throbber,
                                     GtkIconSize      icon_size);

void
katze_throbber_set_icon_name        (KatzeThrobber*   throbber,
                                     const gchar*     icon_size);

void
katze_throbber_set_pixbuf           (KatzeThrobber*   throbber,
                                     GdkPixbuf*       pixbuf);

void
katze_throbber_set_animated         (KatzeThrobber*   throbber,
                                     gboolean         animated);

void
katze_throbber_set_static_icon_name (KatzeThrobber*   throbber,
                                     const gchar*     icon_name);

void
katze_throbber_set_static_pixbuf    (KatzeThrobber*   throbber,
                                     GdkPixbuf*       pixbuf);

void
katze_throbber_set_static_stock_id  (KatzeThrobber*   throbber,
                                     const gchar*     stock_id);

GtkIconSize
katze_throbber_get_icon_size        (KatzeThrobber*   throbber);

const gchar*
katze_throbber_get_icon_name        (KatzeThrobber*   throbber);

GdkPixbuf*
katze_throbber_get_pixbuf           (KatzeThrobber*   throbber);

gboolean
katze_throbber_get_animated         (KatzeThrobber*   throbber);

const gchar*
katze_throbber_get_static_icon_name (KatzeThrobber    *throbber);

GdkPixbuf*
katze_throbber_get_static_pixbuf    (KatzeThrobber*   throbber);

const gchar*
katze_throbber_get_static_stock_id  (KatzeThrobber*   throbber);

G_END_DECLS

#endif /* __KATZE_THROBBER_H__ */
