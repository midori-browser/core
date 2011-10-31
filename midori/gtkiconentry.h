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

#if GTK_CHECK_VERSION (2, 16, 0)
    #define GtkIconEntry GtkEntry
    #define GtkIconEntryPosition GtkEntryIconPosition
    #define GTK_ICON_ENTRY_PRIMARY GTK_ENTRY_ICON_PRIMARY
    #define GTK_ICON_ENTRY_SECONDARY GTK_ENTRY_ICON_SECONDARY
    #define GTK_ICON_ENTRY GTK_ENTRY
    #define GTK_TYPE_ICON_ENTRY GTK_TYPE_ENTRY
    #define gtk_icon_entry_new gtk_entry_new
    #define gtk_icon_entry_set_icon_from_stock gtk_entry_set_icon_from_stock
    #define gtk_icon_entry_set_icon_from_icon_name gtk_entry_set_icon_from_icon_name

    void
    gtk_icon_entry_set_icon_from_pixbuf (GtkEntry*            entry,
                                         GtkEntryIconPosition position,
                                         GdkPixbuf*           pixbuf);
    #define gtk_icon_entry_set_tooltip gtk_entry_set_icon_tooltip_text
    #define gtk_icon_entry_set_icon_highlight gtk_entry_set_icon_activatable
    #define gtk_icon_entry_set_progress_fraction gtk_entry_set_progress_fraction
#else

#define GTK_TYPE_ICON_ENTRY (gtk_icon_entry_get_type())
#define GTK_ICON_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_ICON_ENTRY, GtkIconEntry))
#define GTK_ICON_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_ICON_ENTRY, GtkIconEntryClass))
#define GTK_IS_ICON_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_ICON_ENTRY))
#define GTK_IS_ICON_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_ICON_ENTRY))
#define GTK_ICON_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ICON_ENTRY, GtkIconEntryClass))

typedef enum
{
  GTK_ICON_ENTRY_PRIMARY,
  GTK_ICON_ENTRY_SECONDARY
} GtkIconEntryPosition;

typedef struct _GtkIconEntry        GtkIconEntry;
typedef struct _GtkIconEntryClass   GtkIconEntryClass;
typedef struct _GtkIconEntryPrivate GtkIconEntryPrivate;

struct _GtkIconEntry
{
  GtkEntry parent_object;

  GtkIconEntryPrivate* priv;
};

struct _GtkIconEntryClass
{
  GtkEntryClass parent_class;

  /* Signals */
  void (*icon_pressed) (GtkIconEntry *entry,
			GtkIconEntryPosition icon_pos,
			int button);
  void (*icon_released) (GtkIconEntry *entry,
			 GtkIconEntryPosition icon_pos,
			 int button);

  void (*gtk_reserved1) (void);
  void (*gtk_reserved2) (void);
  void (*gtk_reserved3) (void);
  void (*gtk_reserved4) (void);
};

GType      gtk_icon_entry_get_type                (void) G_GNUC_CONST;

GtkWidget* gtk_icon_entry_new                     (void);

void       gtk_icon_entry_set_icon_from_pixbuf    (GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos,
						   GdkPixbuf *pixbuf);
void       gtk_icon_entry_set_icon_from_stock     (GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos,
						   const gchar *stock_id);
void       gtk_icon_entry_set_icon_from_icon_name (GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos,
						   const gchar *icon_name);

void       gtk_icon_entry_set_icon_from_gicon     (const GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos,
						   GIcon *icon);

GdkPixbuf* gtk_icon_entry_get_pixbuf              (const GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos);

GIcon*     gtk_icon_entry_get_gicon               (const GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos);

void       gtk_icon_entry_set_icon_highlight      (const GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos,
						   gboolean highlight);

gboolean   gtk_icon_entry_get_icon_highlight      (const GtkIconEntry *entry,
						   GtkIconEntryPosition icon_pos);

void       gtk_icon_entry_set_cursor              (const GtkIconEntry *icon_entry,
						   GtkIconEntryPosition icon_pos,
						   GdkCursorType cursor_type);

void       gtk_icon_entry_set_tooltip             (const GtkIconEntry *icon_entry,
						   GtkIconEntryPosition icon_pos,
						   const gchar *text);

void       gtk_icon_entry_set_icon_sensitive      (const GtkIconEntry *icon_entry,
						   GtkIconEntryPosition icon_pos,
						   gboolean sensitive);

void       gtk_icon_entry_set_progress_fraction    (GtkIconEntry *icon_entry,
                                                    gdouble       fraction);

#endif

G_END_DECLS

#endif /* __GTK_ICON_ENTRY_H__ */
