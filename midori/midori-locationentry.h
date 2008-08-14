/*
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_LOCATION_ENTRY_H__
#define __MIDORI_LOCATION_ENTRY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_LOCATION_ENTRY            (midori_location_entry_get_type ())
#define MIDORI_LOCATION_ENTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_LOCATION_ENTRY, MidoriLocationEntry))
#define MIDORI_LOCATION_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MIDORI_TYPE_LOCATION_ENTRY, MidoriLocationEntryClass))
#define MIDORI_IS_LOCATION_ENTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_LOCATION_ENTRY))
#define MIDORI_IS_LOCATION_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MIDORI_TYPE_LOCATION_ENTRY))
#define MIDORI_LOCATION_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MIDORI_TYPE_LOCATION_ENTRY, MidoriLocationEntryClass))

typedef struct _MidoriLocationEntry         MidoriLocationEntry;
typedef struct _MidoriLocationEntryClass    MidoriLocationEntryClass;
typedef struct _MidoriLocationEntryItem     MidoriLocationEntryItem;

struct _MidoriLocationEntryItem
{
    GdkPixbuf* favicon;
    const gchar* uri;
    const gchar* title;
};

GType
midori_location_entry_get_type (void);

GtkWidget*
midori_location_entry_new (void);

gboolean
midori_location_entry_item_iter         (MidoriLocationEntry* location_entry,
                                         const gchar*         uri,
                                         GtkTreeIter*         iter);

const gchar*
midori_location_entry_get_text          (MidoriLocationEntry* location_entry);

void
midori_location_entry_set_text          (MidoriLocationEntry* location_entry,
                                         const gchar*         text);

void
midori_location_entry_clear             (MidoriLocationEntry* location_entry);

void
midori_location_entry_set_item_from_uri (MidoriLocationEntry* location_entry,
                                         const gchar*         uri);

void
midori_location_entry_add_item          (MidoriLocationEntry* location_entry,
                                         MidoriLocationEntryItem* item);

G_END_DECLS

#endif /* __MIDORI_LOCATION_ENTRY_H__ */
