/*
 Copyright (C) 2007 Henrik Hedberg <hhedberg@innologies.fi>
 Copyright (C) 2009 Nadav Wiener <nadavwr@yahoo.com>
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
 */

#ifndef KATZE_SCROLLED_H
#define KATZE_SCROLLED_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define KATZE_TYPE_SCROLLED (katze_scrolled_get_type())
#define KATZE_SCROLLED(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), KATZE_TYPE_SCROLLED, KatzeScrolled))
#define KATZE_SCROLLED_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), KATZE_TYPE_SCROLLED, KatzeScrolledClass))
#define KATZE_IS_SCROLLED(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), KATZE_TYPE_SCROLLED))
#define KATZE_IS_SCROLLED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), KATZE_TYPE_SCROLLED))
#define KATZE_SCROLLED_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), KATZE_TYPE_SCROLLED, KatzeScrolledClass))

typedef struct _KatzeScrolled KatzeScrolled;
typedef struct _KatzeScrolledClass KatzeScrolledClass;
typedef struct _KatzeScrolledPrivate KatzeScrolledPrivate;

struct _KatzeScrolled
{
    GtkScrolledWindow parent;

    KatzeScrolledPrivate* priv;
};

struct _KatzeScrolledClass
{
    GtkScrolledWindowClass parent;

    /* Padding for future expansion */
    void (*_katze_reserved1) (void);
    void (*_katze_reserved2) (void);
    void (*_katze_reserved3) (void);
    void (*_katze_reserved4) (void);
};

GType
katze_scrolled_get_type         (void) G_GNUC_CONST;

GtkWidget*
katze_scrolled_new              (GtkAdjustment* hadjustment,
                                 GtkAdjustment* vadjustment);

G_END_DECLS

#endif /* __KATZE_SCROLLED_H__ */
