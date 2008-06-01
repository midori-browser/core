/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_TRASH_H__
#define __MIDORI_TRASH_H__

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_TRASH \
    (midori_trash_get_type ())
#define MIDORI_TRASH(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_TRASH, MidoriTrash))
#define MIDORI_TRASH_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_TRASH, MidoriTrashClass))
#define MIDORI_IS_TRASH(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_TRASH))
#define MIDORI_IS_TRASH_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_TRASH))
#define MIDORI_TRASH_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_TRASH, MidoriTrashClass))

typedef struct _MidoriTrash                MidoriTrash;
typedef struct _MidoriTrashPrivate         MidoriTrashPrivate;
typedef struct _MidoriTrashClass           MidoriTrashClass;

struct _MidoriTrash
{
    GObject parent_instance;

    MidoriTrashPrivate* priv;
};

struct _MidoriTrashClass
{
    GObjectClass parent_class;

    /* Signals */
    void
    (*inserted)               (MidoriTrash* trash,
                               guint        n);
    void
    (*removed)                (MidoriTrash* trash,
                               guint        n);
};

GType
midori_trash_get_type               (void);

MidoriTrash*
midori_trash_new                    (guint limit);

gboolean
midori_trash_is_empty               (MidoriTrash*   trash);

guint
midori_trash_get_n_items            (MidoriTrash* trash);

KatzeXbelItem*
midori_trash_get_nth_xbel_item      (MidoriTrash* trash,
                                     guint        n);

void
midori_trash_prepend_xbel_item      (MidoriTrash*   trash,
                                     KatzeXbelItem* xbel_item);

void
midori_trash_remove_nth_item        (MidoriTrash*   trash,
                                     guint          n);

void
midori_trash_empty                  (MidoriTrash*   trash);

G_END_DECLS

#endif /* __MIDORI_TRASH_H__ */
