/*
 * Copyright (C) 2004-2006 Christian Hammond.
 * Copyright (C) 2008 Cody Russell  <bratsche@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#ifndef __GTK_ICON_ENTRY_H__
#define __GTK_ICON_ENTRY_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

    #define GtkIconEntryPosition GtkEntryIconPosition
    #define GTK_ICON_ENTRY_PRIMARY GTK_ENTRY_ICON_PRIMARY
    #define GTK_ICON_ENTRY_SECONDARY GTK_ENTRY_ICON_SECONDARY
    #define GTK_ICON_ENTRY GTK_ENTRY
    #define gtk_icon_entry_set_icon_from_stock gtk_entry_set_icon_from_stock
    #define gtk_icon_entry_set_icon_from_icon_name gtk_entry_set_icon_from_icon_name

    #define gtk_icon_entry_set_tooltip gtk_entry_set_icon_tooltip_text
    #define gtk_icon_entry_set_icon_highlight gtk_entry_set_icon_activatable

G_END_DECLS

#endif /* __GTK_ICON_ENTRY_H__ */
