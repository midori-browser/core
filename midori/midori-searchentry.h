/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_SEARCH_ENTRY_H__
#define __MIDORI_SEARCH_ENTRY_H__

#include "gtkiconentry.h"

#include <katze/katze.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_SEARCH_ENTRY \
    (midori_search_entry_get_type ())
#define MIDORI_SEARCH_ENTRY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_SEARCH_ENTRY, MidoriSearchEntry))
#define MIDORI_SEARCH_ENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_SEARCH_ENTRY, MidoriSearchEntryClass))
#define MIDORI_IS_SEARCH_ENTRY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_SEARCH_ENTRY))
#define MIDORI_IS_SEARCH_ENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_SEARCH_ENTRY))
#define MIDORI_SEARCH_ENTRY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_SEARCH_ENTRY, MidoriSearchEntryClass))

typedef struct _MidoriSearchEntry                MidoriSearchEntry;
typedef struct _MidoriSearchEntryClass           MidoriSearchEntryClass;

struct _MidoriSearchEntryClass
{
    GtkIconEntryClass parent_class;
};

GType
midori_search_entry_get_type               (void);

GtkWidget*
midori_search_entry_new                    (void);

MidoriWebList*
midori_search_entry_get_search_engines     (MidoriSearchEntry*  search_entry);

void
midori_search_entry_set_search_engines     (MidoriSearchEntry*  search_entry,
                                            MidoriWebList*      name);

KatzeItem*
midori_search_entry_get_current_item       (MidoriSearchEntry*  search_entry);

void
midori_search_entry_set_current_item       (MidoriSearchEntry*  search_entry,
                                            KatzeItem*      name);

GtkWidget*
midori_search_entry_get_dialog             (MidoriSearchEntry*  search_entry);

G_END_DECLS

#endif /* __MIDORI_SEARCH_ENTRY_H__ */
