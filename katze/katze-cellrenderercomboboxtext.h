/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_CELL_RENDERER_COMBOBOX_TEXT_H__
#define __KATZE_CELL_RENDERER_COMBOBOX_TEXT_H__


#include <gtk/gtk.h>

#ifndef GSEAL
#define GSEAL(String) String
#endif

G_BEGIN_DECLS


#define KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT		(katze_cell_renderer_combobox_text_get_type ())
#define KATZE_CELL_RENDERER_COMBOBOX_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT, KatzeCellRendererComboBoxText))
#define KATZE_CELL_RENDERER_COMBOBOX_TEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT, KatzeCellRendererComboBoxTextClass))
#define KATZE_IS_CELL_RENDERER_COMBOBOX_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT))
#define KATZE_IS_CELL_RENDERER_COMBOBOX_TEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT))
#define KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT, KatzeCellRendererComboBoxTextClass))

typedef struct _KatzeCellRendererComboBoxText      KatzeCellRendererComboBoxText;
typedef struct _KatzeCellRendererComboBoxTextClass KatzeCellRendererComboBoxTextClass;

struct _KatzeCellRendererComboBoxText
{
  GtkCellRendererText parent;

  /*< private >*/
};

struct _KatzeCellRendererComboBoxTextClass
{
  GtkCellRendererTextClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType            katze_cell_renderer_combobox_text_get_type (void) G_GNUC_CONST;
GtkCellRenderer *katze_cell_renderer_combobox_text_new      (void);

G_END_DECLS

#endif /* __KATZE_CELL_RENDERER_COMBOBOX_TEXT_H__ */
