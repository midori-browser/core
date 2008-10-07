/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_LIST_H__
#define __KATZE_LIST_H__

#include "katze-item.h"

G_BEGIN_DECLS

#define KATZE_TYPE_LIST \
    (katze_list_get_type ())
#define KATZE_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_LIST, KatzeList))
#define KATZE_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_LIST, KatzeListClass))
#define KATZE_IS_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_LIST))
#define KATZE_IS_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_LIST))
#define KATZE_LIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_LIST, KatzeListClass))

typedef struct _KatzeList                       KatzeList;
typedef struct _KatzeListClass                  KatzeListClass;

struct _KatzeList
{
    KatzeItem parent_instance;

    GList* items;
};

struct _KatzeListClass
{
    KatzeItemClass parent_class;

    /* Signals */
    void
    (*add_item)               (KatzeList* list,
                               gpointer   item);
    void
    (*remove_item)            (KatzeList* list,
                               gpointer   item);
    void
    (*clear)                  (KatzeList* list);

};

GType
katze_list_get_type               (void);

KatzeList*
katze_list_new                    (void);

void
katze_list_add_item               (KatzeList*   list,
                                   gpointer     item);

void
katze_list_remove_item            (KatzeList*   list,
                                   gpointer     item);

gpointer
katze_list_get_nth_item           (KatzeList*   list,
                                   guint        n);

gboolean
katze_list_is_empty               (KatzeList*   list);

gint
katze_list_get_item_index         (KatzeList*   list,
                                   gpointer     item);

guint
katze_list_get_length             (KatzeList*   list);

void
katze_list_clear                  (KatzeList*   list);

G_END_DECLS

#endif /* __KATZE_LIST_H__ */
