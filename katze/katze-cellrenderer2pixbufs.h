/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_CELL_RENDERER_2PIXBUFS_H__
#define __KATZE_CELL_RENDERER_2PIXBUFS_H__


#include <gtk/gtk.h>

#ifndef GSEAL
#define GSEAL(String) String
#endif


G_BEGIN_DECLS


#define KATZE_TYPE_CELL_RENDERER_2PIXBUFS			(katze_cell_renderer_2pixbufs_get_type ())
#define KATZE_CELL_RENDERER_2PIXBUFS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_CELL_RENDERER_2PIXBUFS, KatzeCellRenderer2Pixbufs))
#define KATZE_CELL_RENDERER_2PIXBUFS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_CELL_RENDERER_2PIXBUFS, KatzeCellRenderer2PixbufsClass))
#define KATZE_IS_CELL_RENDERER_2PIXBUFS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_CELL_RENDERER_2PIXBUFS))
#define KATZE_IS_CELL_RENDERER_2PIXBUFS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_CELL_RENDERER_2PIXBUFS))
#define KATZE_CELL_RENDERER_2PIXBUFS_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_CELL_RENDERER_2PIXBUFS, GtkCellRenderer2PixbufsClass))

typedef struct _KatzeCellRenderer2Pixbufs KatzeCellRenderer2Pixbufs;
typedef struct _KatzeCellRenderer2PixbufsClass KatzeCellRenderer2PixbufsClass;

struct _KatzeCellRenderer2Pixbufs
{
  GtkCellRenderer parent;
};

struct _KatzeCellRenderer2PixbufsClass
{
  GtkCellRendererClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType            katze_cell_renderer_2pixbufs_get_type (void) G_GNUC_CONST;
GtkCellRenderer *katze_cell_renderer_2pixbufs_new      (void);


G_END_DECLS


#endif /* __KATZE_CELL_RENDERER_2PIXBUFS_H__ */
