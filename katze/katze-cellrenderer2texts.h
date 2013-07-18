/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_CELL_RENDERER_2TEXTS_H__
#define __KATZE_CELL_RENDERER_2TEXTS_H__


#include <gtk/gtk.h>

#ifndef GSEAL
#define GSEAL(String) String
#endif

G_BEGIN_DECLS


#define KATZE_TYPE_CELL_RENDERER_2TEXTS		(katze_cell_renderer_2texts_get_type ())
#define KATZE_CELL_RENDERER_2TEXTS(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_CELL_RENDERER_2TEXTS, KatzeCellRenderer2texts))
#define KATZE_CELL_RENDERER_2TEXTS_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_CELL_RENDERER_2TEXTS, KatzeCellRenderer2textsClass))
#define KATZE_IS_CELL_RENDERER_2TEXTS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_CELL_RENDERER_2TEXTS))
#define KATZE_IS_CELL_RENDERER_2TEXTS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_CELL_RENDERER_2TEXTS))
#define KATZE_CELL_RENDERER_2TEXTS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_CELL_RENDERER_2TEXTS, KatzeCellRenderer2textsClass))

typedef struct _KatzeCellRenderer2texts      KatzeCellRenderer2texts;
typedef struct _KatzeCellRenderer2textsClass KatzeCellRenderer2textsClass;

struct _KatzeCellRenderer2texts
{
  GtkCellRenderer parent;

  /*< private >*/
  gchar *GSEAL (text);
  PangoAttrList *GSEAL (extra_attrs);

  gchar *GSEAL (alternate_text);
  PangoAttrList *GSEAL (alternate_extra_attrs);
};

struct _KatzeCellRenderer2textsClass
{
  GtkCellRendererClass parent_class;

  void (* edited) (KatzeCellRenderer2texts *cell_renderer_2texts,
		   const gchar         *path,
		   const gchar         *new_text);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType            katze_cell_renderer_2texts_get_type (void) G_GNUC_CONST;
GtkCellRenderer *katze_cell_renderer_2texts_new      (void);

void             katze_cell_renderer_2texts_set_fixed_height_from_font (KatzeCellRenderer2texts *renderer,
								    gint                 number_of_rows);


G_END_DECLS

#endif /* __KATZE_CELL_RENDERER_2TEXTS_H__ */
