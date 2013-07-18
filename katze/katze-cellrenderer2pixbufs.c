/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-cellrenderer2pixbufs.h"

#include "marshal.h"

#include <gdk/gdk.h>

#define P_(String) (String)
#define I_(String) (String)
#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB


static void
katze_cell_renderer_2pixbufs_finalize (GObject* object);
static void
katze_cell_renderer_2pixbufs_get_property (GObject*    object,
    guint       param_id,
    GValue*     value,
    GParamSpec* pspec);
static void
katze_cell_renderer_2pixbufs_set_property (GObject*      object,
    guint         param_id,
    const GValue* value,
    GParamSpec*   pspec);
static void
katze_cell_renderer_2pixbufs_get_size (GtkCellRenderer* cell,
    GtkWidget*       widget,
    GdkRectangle*    cell_area,
    gint*            x_offset,
    gint*            y_offset,
    gint*            width,
    gint*            height);
static void
#if GTK_CHECK_VERSION(3,0,0)
katze_cell_renderer_2pixbufs_render (GtkCellRenderer      *cell,
    cairo_t*             cr,
    GtkWidget            *widget,
    GdkRectangle         *background_area,
    GdkRectangle         *cell_area,
    GtkCellRendererState  flags);
#else
katze_cell_renderer_2pixbufs_render (GtkCellRenderer      *cell,
    GdkDrawable          *window,
    GtkWidget            *widget,
    GdkRectangle         *background_area,
    GdkRectangle         *cell_area,
    GdkRectangle         *expose_area,
    GtkCellRendererState  flags);
#endif

enum {
  PROP_0,
  PROP_PIXBUF_EXPANDER_OPEN,
  PROP_PIXBUF_EXPANDER_CLOSED,
  PROP_FOLLOW_STATE,
};

#define KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), KATZE_TYPE_CELL_RENDERER_2PIXBUFS, KatzeCellRenderer2PixbufsPrivate))

typedef struct _KatzeCellRenderer2PixbufsPrivate KatzeCellRenderer2PixbufsPrivate;
struct _KatzeCellRenderer2PixbufsPrivate
{
  GtkCellRendererPixbuf* cellpixbuf;

  guint markup_set : 1;
  guint alternate_markup_set : 1;
};

G_DEFINE_TYPE (KatzeCellRenderer2Pixbufs, katze_cell_renderer_2pixbufs, GTK_TYPE_CELL_RENDERER)

static void
katze_cell_renderer_2pixbufs_notify (GObject    *gobject,
                                   GParamSpec *pspec,
                                   KatzeCellRenderer2Pixbufs *cellpixbuf)
{
    if (!g_strcmp0(P_("text"), pspec->name)
        || !g_strcmp0(P_("attributes"), pspec->name))
        return;

    g_object_notify (G_OBJECT (cellpixbuf), pspec->name);
}

static void
katze_cell_renderer_2pixbufs_init (KatzeCellRenderer2Pixbufs *cellpixbuf)
{
  GValue true_value = {0};
  KatzeCellRenderer2PixbufsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE (cellpixbuf);

  priv->cellpixbuf = GTK_CELL_RENDERER_PIXBUF (gtk_cell_renderer_pixbuf_new());
  g_object_ref (priv->cellpixbuf);

  g_value_init (&true_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&true_value, TRUE);
  g_object_set_property (G_OBJECT (priv->cellpixbuf), "is-expander", &true_value);
  g_value_reset (&true_value);

  g_signal_connect (priv->cellpixbuf, "notify",
      G_CALLBACK (katze_cell_renderer_2pixbufs_notify),
      cellpixbuf);
}

static void
katze_cell_renderer_2pixbufs_class_init (KatzeCellRenderer2PixbufsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->finalize = katze_cell_renderer_2pixbufs_finalize;

  object_class->get_property = katze_cell_renderer_2pixbufs_get_property;
  object_class->set_property = katze_cell_renderer_2pixbufs_set_property;

  cell_class->get_size = katze_cell_renderer_2pixbufs_get_size;
  cell_class->render = katze_cell_renderer_2pixbufs_render;

  g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_OPEN,
				   g_param_spec_object ("pixbuf-expander-open",
							P_("Pixbuf Expander Open"),
							P_("Pixbuf for open expander"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_CLOSED,
				   g_param_spec_object ("pixbuf-expander-closed",
							P_("Pixbuf Expander Closed"),
							P_("Pixbuf for closed expander"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererPixbuf:follow-state:
   *
   * Specifies whether the rendered pixbuf should be colorized
   * according to the #GtkCellRendererState.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
				   PROP_FOLLOW_STATE,
				   g_param_spec_boolean ("follow-state",
 							 P_("Follow State"),
 							 P_("Whether the rendered pixbuf should be "
							    "colorized according to the state"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (KatzeCellRenderer2PixbufsPrivate));
}

static void
katze_cell_renderer_2pixbufs_finalize (GObject *object)
{
  KatzeCellRenderer2Pixbufs *cellpixbuf = KATZE_CELL_RENDERER_2PIXBUFS (object);
  KatzeCellRenderer2PixbufsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE (object);

  g_object_unref (priv->cellpixbuf);

  G_OBJECT_CLASS (katze_cell_renderer_2pixbufs_parent_class)->finalize (object);
}

static const gchar* const cell_pixbuf_renderer_property_names[] =
{
  /* GtkCellRendererPixbuf args */
  "pixbuf-expander-open",
  "pixbuf-expander-closed",
  "follow-state"
};

static void
katze_cell_renderer_2pixbufs_get_property (GObject*    object,
					 guint       param_id,
					 GValue*     value,
					 GParamSpec* pspec)
{
  KatzeCellRenderer2Pixbufs *cellpixbuf = KATZE_CELL_RENDERER_2PIXBUFS (object);
  KatzeCellRenderer2PixbufsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_PIXBUF_EXPANDER_OPEN:
    case PROP_PIXBUF_EXPANDER_CLOSED:
    case PROP_FOLLOW_STATE:
      g_object_get_property (G_OBJECT (priv->cellpixbuf), cell_pixbuf_renderer_property_names[param_id-PROP_PIXBUF_EXPANDER_OPEN], value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
katze_cell_renderer_2pixbufs_set_property (GObject*      object,
					 guint         param_id,
					 const GValue* value,
					 GParamSpec*   pspec)
{
  KatzeCellRenderer2Pixbufs *cellpixbuf = KATZE_CELL_RENDERER_2PIXBUFS (object);
  KatzeCellRenderer2PixbufsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_PIXBUF_EXPANDER_OPEN:
    case PROP_PIXBUF_EXPANDER_CLOSED:
    case PROP_FOLLOW_STATE:
      g_object_set_property (G_OBJECT (priv->cellpixbuf), cell_pixbuf_renderer_property_names[param_id-PROP_PIXBUF_EXPANDER_OPEN], value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * katze_cell_renderer_2pixbufs_new:
 *
 * Creates a new #KatzeCellRenderer2Pixbufs. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 *
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
katze_cell_renderer_2pixbufs_new (void)
{
  return g_object_new (KATZE_TYPE_CELL_RENDERER_2PIXBUFS, NULL);
}

static GtkCellRendererState
set_pixbuf(KatzeCellRenderer2Pixbufs*        cellpixbuf,
    GtkWidget*                      widget,
    KatzeCellRenderer2PixbufsPrivate* priv,
    GtkCellRendererState flags)
{
  GtkWidget* pwidget = gtk_widget_get_parent (widget);
  gboolean alternate = FALSE;
  GValue false_value = {0};
  GValue true_value = {0};
  g_value_init (&false_value, G_TYPE_BOOLEAN);
  g_value_init (&true_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&false_value, FALSE);
  g_value_set_boolean (&true_value, TRUE);

  if (GTK_IS_MENU_ITEM (pwidget))
    {
      GtkWidget* menu = gtk_widget_get_parent (pwidget);
      GList* items;

      if (menu
          && (GTK_IS_MENU (menu))
          && (items = gtk_container_get_children (GTK_CONTAINER (menu)))
          && (GTK_WIDGET (items->data) == pwidget)
          && (g_list_length (items) > 1)
          && (GTK_IS_SEPARATOR_MENU_ITEM (g_list_next (items)->data)))
      {
          alternate = TRUE;
      }
    }

  g_object_set_property (G_OBJECT (priv->cellpixbuf), "is-expander", &true_value);
  if (alternate)
    {
        flags |= (1<<6) /* GTK_CELL_RENDERER_EXPANDED */ ;
        g_object_set_property (G_OBJECT (priv->cellpixbuf), "is-expanded", &true_value);
    }
  else
    {
        flags |= (1<<5) /* GTK_CELL_RENDERER_EXPANDABLE*/ ;
        g_object_set_property (G_OBJECT (priv->cellpixbuf), "is-expanded", &false_value);
    }
  return flags;
}

static void
katze_cell_renderer_2pixbufs_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
				 GdkRectangle    *cell_area,
				 gint            *x_offset,
				 gint            *y_offset,
				 gint            *width,
				 gint            *height)
{
  KatzeCellRenderer2Pixbufs *cellpixbuf = (KatzeCellRenderer2Pixbufs *) cell;
  KatzeCellRenderer2PixbufsPrivate *priv;
  priv = KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE (cell);

  gtk_cell_renderer_get_size (GTK_CELL_RENDERER (priv->cellpixbuf),
			      widget, cell_area,
			      x_offset, y_offset, width, height);
}

static void
#if GTK_CHECK_VERSION(3,0,0)
katze_cell_renderer_2pixbufs_render (GtkCellRenderer      *cell,
			       cairo_t*             cr,
			       GtkWidget            *widget,
			       GdkRectangle         *background_area,
			       GdkRectangle         *cell_area,
			       GtkCellRendererState  flags)
#else
katze_cell_renderer_2pixbufs_render (GtkCellRenderer      *cell,
			       GdkDrawable          *window,
			       GtkWidget            *widget,
			       GdkRectangle         *background_area,
			       GdkRectangle         *cell_area,
			       GdkRectangle         *expose_area,
			       GtkCellRendererState  flags)
#endif
{
  KatzeCellRenderer2Pixbufs *cellpixbuf = (KatzeCellRenderer2Pixbufs *) cell;
  KatzeCellRenderer2PixbufsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2PIXBUFS_GET_PRIVATE (cell);


#if GTK_CHECK_VERSION(3,0,0)
  gtk_cell_renderer_render (GTK_CELL_RENDERER (priv->cellpixbuf),
      cr,
      widget,
      background_area,
      cell_area,
      set_pixbuf (cellpixbuf, widget, priv, flags));
#else
  gtk_cell_renderer_render (GTK_CELL_RENDERER (priv->cellpixbuf),
      window,
      widget,
      background_area,
      cell_area,
      expose_area,
      set_pixbuf (cellpixbuf, widget, priv, flags));
#endif
}
