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

#include "gtkiconentry.h"

#if GTK_CHECK_VERSION (2, 16, 0)

void
gtk_icon_entry_set_icon_from_pixbuf (GtkEntry*            entry,
                                     GtkEntryIconPosition position,
                                     GdkPixbuf*           pixbuf)
{
    gboolean activatable;

    /* Without this ugly hack pixbuf icons don't work */
    activatable = gtk_entry_get_icon_activatable (entry, position);
    gtk_entry_set_icon_from_pixbuf (entry, position, pixbuf);
    gtk_entry_set_icon_activatable (entry, position, !activatable);
    gtk_entry_set_icon_activatable (entry, position, activatable);
}

#else

#include <string.h>

#if GTK_CHECK_VERSION (2, 14, 0)
#define _GTK_IMAGE_GICON GTK_IMAGE_GICON
#else
#define _GTK_IMAGE_GICON 8
#endif

#ifndef GTK_PARAM_READABLE
#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#endif

#ifndef GTK_PARAM_WRITABLE
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#endif

#ifndef GTK_PARAM_READWRITE
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#endif

#define P_(s) (s)

#define ICON_MARGIN 2
#define MAX_ICONS 2

#define IS_VALID_ICON_ENTRY_POSITION(pos) \
	((pos) == GTK_ICON_ENTRY_PRIMARY || \
	 (pos) == GTK_ICON_ENTRY_SECONDARY)

typedef struct
{
  GdkPixbuf *pixbuf;
  gboolean highlight;
  gboolean hovered;
  GdkWindow *window;
  gchar *tooltip_text;
  GdkCursorType cursor_type;
  gboolean custom_cursor;
  GtkImageType storage_type;
  GIcon *gicon;
  gchar *icon_name;
  gboolean insensitive;
} EntryIconInfo;

struct _GtkIconEntryPrivate
{
  gdouble fraction;
  EntryIconInfo icons[MAX_ICONS];

  gulong icon_released_id;
};

enum
{
  ICON_PRESSED,
  ICON_RELEASED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PIXBUF_PRIMARY,
  PROP_PIXBUF_SECONDARY,
  PROP_STOCK_PRIMARY,
  PROP_STOCK_SECONDARY,
  PROP_ICON_NAME_PRIMARY,
  PROP_ICON_NAME_SECONDARY,
  PROP_GICON_PRIMARY,
  PROP_GICON_SECONDARY,
  PROP_SENSITIVITY_PRIMARY,
  PROP_SENSITIVITY_SECONDARY
};

static void gtk_icon_entry_editable_init     (GtkEditableClass     *iface);
static void gtk_icon_entry_finalize          (GObject              *obj);
static void gtk_icon_entry_dispose           (GObject              *obj);
static void gtk_icon_entry_map               (GtkWidget            *widget);
static void gtk_icon_entry_unmap             (GtkWidget            *widget);
static void gtk_icon_entry_realize           (GtkWidget            *widget);
static void gtk_icon_entry_unrealize         (GtkWidget            *widget);
static void gtk_icon_entry_size_request      (GtkWidget            *widget,
					      GtkRequisition       *requisition);
static void gtk_icon_entry_size_allocate     (GtkWidget            *widget,
					      GtkAllocation        *allocation);
static gint gtk_icon_entry_expose            (GtkWidget            *widget,
					      GdkEventExpose       *event);
static gint gtk_icon_entry_enter_notify      (GtkWidget            *widget,
					      GdkEventCrossing     *event);
static gint gtk_icon_entry_leave_notify      (GtkWidget            *widget,
					      GdkEventCrossing     *event);
static gint gtk_icon_entry_button_press      (GtkWidget            *widget,
					      GdkEventButton       *event);
static gint gtk_icon_entry_button_release    (GtkWidget            *widget,
					      GdkEventButton       *event);
static void gtk_icon_entry_set_property      (GObject              *object,
					      guint                 prop_id,
					      const GValue         *value,
					      GParamSpec           *pspec);
static void gtk_icon_entry_get_property      (GObject              *object,
					      guint                 prop_id,
					      GValue               *value,
					      GParamSpec           *pspec);
static void gtk_icon_entry_style_set         (GtkWidget            *widget,
					      GtkStyle             *prev_style);
static void gtk_icon_entry_set_icon_internal (GtkIconEntry         *entry,
					      GtkIconEntryPosition  icon_pos,
					      GdkPixbuf            *pixbuf);
static void icon_theme_changed               (GtkIconEntry         *entry);


static GtkEntryClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE_EXTENDED (GtkIconEntry, gtk_icon_entry, GTK_TYPE_ENTRY,
			0,
			G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
					       gtk_icon_entry_editable_init));

static void
gtk_icon_entry_class_init (GtkIconEntryClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkEntryClass *entry_class;

  parent_class = g_type_class_peek_parent(klass);

  gobject_class = G_OBJECT_CLASS(klass);
  object_class  = GTK_OBJECT_CLASS(klass);
  widget_class  = GTK_WIDGET_CLASS(klass);
  entry_class   = GTK_ENTRY_CLASS(klass);

  gobject_class->finalize = gtk_icon_entry_finalize;
  gobject_class->dispose = gtk_icon_entry_dispose;
  gobject_class->set_property = gtk_icon_entry_set_property;
  gobject_class->get_property = gtk_icon_entry_get_property;

  widget_class->map = gtk_icon_entry_map;
  widget_class->unmap = gtk_icon_entry_unmap;
  widget_class->realize = gtk_icon_entry_realize;
  widget_class->unrealize = gtk_icon_entry_unrealize;
  widget_class->size_request = gtk_icon_entry_size_request;
  widget_class->size_allocate = gtk_icon_entry_size_allocate;
  widget_class->expose_event = gtk_icon_entry_expose;
  widget_class->enter_notify_event = gtk_icon_entry_enter_notify;
  widget_class->leave_notify_event = gtk_icon_entry_leave_notify;
  widget_class->button_press_event = gtk_icon_entry_button_press;
  widget_class->button_release_event = gtk_icon_entry_button_release;
  widget_class->style_set = gtk_icon_entry_style_set;

  /**
   * GtkIconEntry::icon-pressed:
   * @entry: The entry on which the signal is emitted.
   * @icon_pos: The position of the clicked icon.
   * @button: The mouse button clicked.
   *
   * The ::icon-pressed signal is emitted when an icon is clicked.
   */
  if (!(signals[ICON_PRESSED] = g_signal_lookup ("icon-pressed", GTK_TYPE_ENTRY)))
  signals[ICON_PRESSED] =
    g_signal_new ("icon-pressed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconEntryClass, icon_pressed),
		  NULL, NULL,
		  gtk_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 2,
		  G_TYPE_INT,
		  G_TYPE_INT);

  /**
   * GtkIconEntry::icon-release:
   * @entry: The entry on which the signal is emitted.
   * @icon_pos: The position of the clicked icon.
   * @button: The mouse button clicked.
   *
   * The ::icon-release signal is emitted on the button release from a
   * mouse click.
   */
  if (!(signals[ICON_RELEASED] = g_signal_lookup ("icon-release", GTK_TYPE_ENTRY)))
  signals[ICON_RELEASED] =
    g_signal_new ("icon-release",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkIconEntryClass, icon_released),
		  NULL, NULL,
		  gtk_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 2,
		  G_TYPE_INT,
		  G_TYPE_INT);

  /**
   * GtkIconEntry:pixbuf-primary:
   *
   * An image to use as the primary icon for the entry.
   */
  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF_PRIMARY,
				   g_param_spec_object ("pixbuf-primary",
							P_("Primary pixbuf"),
							P_("Primary pixbuf for the entry"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  /**
   * GtkIconEntry:pixbuf-secondary:
   *
   * An image to use as the secondary icon for the entry.
   */
  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF_SECONDARY,
				   g_param_spec_object ("pixbuf-secondary",
							P_("Secondary pixbuf"),
							P_("Secondary pixbuf for the entry"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_STOCK_PRIMARY,
				   g_param_spec_string ("stock-primary",
							P_("Primary stock ID"),
							P_("Stock ID for primary icon"),
							NULL,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
				   PROP_STOCK_SECONDARY,
				   g_param_spec_string ("stock-secondary",
							P_("Secondary stock ID"),
							P_("Stock ID for secondary icon"),
							NULL,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
				   PROP_ICON_NAME_PRIMARY,
				   g_param_spec_string ("icon-name-primary",
							P_("Primary icon name"),
							P_("Icon name for primary icon"),
							NULL,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
				   PROP_ICON_NAME_SECONDARY,
				   g_param_spec_string ("icon-name-secondary",
							P_("Secondary icon name"),
							P_("Icon name for secondary icon"),
							NULL,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
				   PROP_GICON_PRIMARY,
				   g_param_spec_object ("gicon-primary",
							P_("Primary GIcon"),
							P_("GIcon for primary icon"),
							G_TYPE_ICON,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_GICON_SECONDARY,
				   g_param_spec_object ("gicon-secondary",
							P_("Secondary GIcon"),
							P_("GIcon for secondary icon"),
							G_TYPE_ICON,
							GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkIconEntryPrivate));
}

static void
gtk_icon_entry_editable_init (GtkEditableClass *iface)
{
};

/* GTK+/ GtkEntry internal helper function
   Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
   Modified by the GTK+ Team and others 1997-2000
   Copied from Gtk+ 2.13, whitespace adjusted */
static void
gtk_entry_get_pixel_ranges (GtkEntry  *entry,
                            gint     **ranges,
                            gint      *n_ranges)
{
  gint start_char, end_char;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry),
                                         &start_char, &end_char))
    {
      PangoLayout *layout = gtk_entry_get_layout (entry);
      PangoLayoutLine *line = pango_layout_get_lines (layout)->data;
      const char *text = pango_layout_get_text (layout);
      gsize start_index = g_utf8_offset_to_pointer (text, start_char) - text;
      gsize end_index = g_utf8_offset_to_pointer (text, end_char) - text;
      gint real_n_ranges, i;

      pango_layout_line_get_x_ranges (line,
          start_index, end_index, ranges, &real_n_ranges);

      if (ranges)
        {
          gint *r = *ranges;

          for (i = 0; i < real_n_ranges; ++i)
            {
              r[2 * i + 1] = (r[2 * i + 1] - r[2 * i]) / PANGO_SCALE;
              r[2 * i] = r[2 * i] / PANGO_SCALE;
            }
        }

      if (n_ranges)
        *n_ranges = real_n_ranges;
    }
  else
    {
      if (n_ranges)
        *n_ranges = 0;
      if (ranges)
        *ranges = NULL;
    }
}

/* GTK+/ GtkEntry internal helper function
   Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
   Modified by the GTK+ Team and others 1997-2000
   Copied from Gtk+ 2.13, whitespace adjusted
   Code adjusted to not rely on internal qdata */
static void
_gtk_entry_effective_inner_border (GtkEntry  *entry,
                                   GtkBorder *border)
{
  static const GtkBorder default_inner_border = { 2, 2, 2, 2 };
  GtkBorder *tmp_border;

  tmp_border = (GtkBorder*) gtk_entry_get_inner_border (entry);

  if (tmp_border)
    {
      *border = *tmp_border;
      return;
    }

  gtk_widget_style_get (GTK_WIDGET (entry), "inner-border", &tmp_border, NULL);

  if (tmp_border)
    {
      *border = *tmp_border;
      gtk_border_free (tmp_border);
      return;
    }

  *border = default_inner_border;
}

static void
gtk_entry_borders (GtkEntry* entry,
                   gint*     xborder,
                   gint*     yborder,
                   gboolean* interior_focus,
                   gint*     focus_width)
{
  GtkWidget *widget = GTK_WIDGET (entry);

  if (entry->has_frame)
    {
      *xborder = widget->style->xthickness;
      *yborder = widget->style->ythickness;
    }
  else
    {
      *xborder = 0;
      *yborder = 0;
    }

  gtk_widget_style_get (widget, "interior-focus", interior_focus,
                        "focus-line-width", focus_width, NULL);

  if (interior_focus)
    {
      *xborder += *focus_width;
      *yborder += *focus_width;
    }
}

/* GTK+/ GtkEntry internal helper function
   Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
   Modified by the GTK+ Team and others 1997-2000
   Copied from Gtk+ 2.13, whitespace adjusted */
static void
gtk_entry_get_text_area_size (GtkEntry *entry,
                              gint     *x,
                              gint     *y,
                              gint     *width,
                              gint     *height)
{
  gint frame_height;
  gint xborder, yborder;
  gboolean interior_focus;
  gint focus_width;
  GtkRequisition requisition;
  GtkWidget *widget = GTK_WIDGET (entry);

  gtk_widget_get_child_requisition (widget, &requisition);
  gtk_entry_borders (entry, &xborder, &yborder, &interior_focus, &focus_width);

  if (GTK_WIDGET_REALIZED (widget))
    gdk_drawable_get_size (widget->window, NULL, &frame_height);
  else
    frame_height = requisition.height;

  if (GTK_WIDGET_HAS_FOCUS (widget) && interior_focus)
      frame_height -= 2 * focus_width;

  if (x)
    *x = xborder;

  if (y)
    *y = frame_height / 2 - (requisition.height - yborder * 2) / 2;

  if (width)
    *width = GTK_WIDGET (entry)->allocation.width - xborder * 2;

  if (height)
    *height = requisition.height - yborder * 2;
}

/* GTK+/ GtkEntry internal helper function
   Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
   Modified by the GTK+ Team and others 1997-2000
   Copied from Gtk+ 2.13, whitespace adjusted */
static void
get_layout_position (GtkEntry *entry,
                     gint     *x,
                     gint     *y)
{
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint area_width, area_height;
  GtkBorder inner_border;
  gint y_pos;
  PangoLayoutLine *line;

  layout = gtk_entry_get_layout (entry);

  gtk_entry_get_text_area_size (entry, NULL, NULL, &area_width, &area_height);
  _gtk_entry_effective_inner_border (entry, &inner_border);

  area_height = PANGO_SCALE *
                (area_height - inner_border.top - inner_border.bottom);

  line = pango_layout_get_lines (layout)->data;
  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Align primarily for locale's ascent/descent */
  y_pos = ((area_height - entry->ascent - entry->descent) / 2 +
           entry->ascent + logical_rect.y);

  /* Now see if we need to adjust to fit in actual drawn string */
  if (logical_rect.height > area_height)
    y_pos = (area_height - logical_rect.height) / 2;
  else if (y_pos < 0)
    y_pos = 0;
  else if (y_pos + logical_rect.height > area_height)
    y_pos = area_height - logical_rect.height;

  y_pos = inner_border.top + y_pos / PANGO_SCALE;

  if (x)
    *x = inner_border.left - entry->scroll_offset;

  if (y)
    *y = y_pos;
}

/* GTK+/ GtkEntry internal helper function
   Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
   Modified by the GTK+ Team and others 1997-2000
   Copied from Gtk+ 2.13, whitespace adjusted
   Code adjusted to not rely on internal _gtk_entry_ensure_layout */
static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget;

  if (!entry->visible && entry->invisible_char == 0)
    return;

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      PangoLayout *layout = gtk_entry_get_layout (entry);
      cairo_t *cr;
      gint x, y;
      gint start_pos, end_pos;

      widget = GTK_WIDGET (entry);

      get_layout_position (entry, &x, &y);

      cr = gdk_cairo_create (entry->text_area);

      cairo_move_to (cr, x, y);
      gdk_cairo_set_source_color (cr, &widget->style->text [widget->state]);
      pango_cairo_show_layout (cr, layout);

      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry),
                                             &start_pos, &end_pos))
        {
          gint *ranges;
          gint n_ranges, i;
          PangoRectangle logical_rect;
          GdkColor *selection_color, *text_color;
          GtkBorder inner_border;

          pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
          gtk_entry_get_pixel_ranges (entry, &ranges, &n_ranges);

          if (GTK_WIDGET_HAS_FOCUS (entry))
            {
              selection_color = &widget->style->base [GTK_STATE_SELECTED];
              text_color = &widget->style->text [GTK_STATE_SELECTED];
            }
          else
            {
              selection_color = &widget->style->base [GTK_STATE_ACTIVE];
              text_color = &widget->style->text [GTK_STATE_ACTIVE];
            }

          _gtk_entry_effective_inner_border (entry, &inner_border);

          for (i = 0; i < n_ranges; ++i)
            cairo_rectangle (cr,
                             inner_border.left -
                             entry->scroll_offset + ranges[2 * i],
                             y,
                             ranges[2 * i + 1],
                             logical_rect.height);

          cairo_clip (cr);

          gdk_cairo_set_source_color (cr, selection_color);
          cairo_paint (cr);

          cairo_move_to (cr, x, y);
          gdk_cairo_set_source_color (cr, text_color);
          pango_cairo_show_layout (cr, layout);

          g_free (ranges);
        }

      cairo_destroy (cr);
    }
}

static gboolean
entry_expose_event (GtkWidget*      entry,
                    GdkEventExpose* event,
                    GtkIconEntry*   icon_entry)
{
  GtkIconEntryPrivate *priv;
  GdkWindow* text_area;
  gint width, height;

  priv = icon_entry->priv;
  text_area = GTK_ENTRY (entry)->text_area;
  gdk_drawable_get_size (text_area, &width, &height);

  if (priv->fraction > 0.0)
  {
      gtk_paint_box (entry->style, text_area,
                     GTK_STATE_SELECTED, GTK_SHADOW_OUT,
                     &event->area, entry, "entry-progress",
                     0, 0, priv->fraction * width, height);
      gtk_entry_draw_text (GTK_ENTRY (entry));
  }
  return FALSE;
}

static void
gtk_icon_entry_init (GtkIconEntry *entry)
{
  entry->priv = G_TYPE_INSTANCE_GET_PRIVATE (entry, GTK_TYPE_ICON_ENTRY,
                                             GtkIconEntryPrivate);

  g_signal_connect_after (entry, "expose-event",
    G_CALLBACK (entry_expose_event), entry);
}

static void
gtk_icon_entry_finalize (GObject *obj)
{
  GtkIconEntry *entry;

  g_return_if_fail (obj != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY(obj));

  entry = GTK_ICON_ENTRY (obj);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gtk_icon_entry_dispose (GObject *obj)
{
  GtkIconEntry *entry;

  entry = GTK_ICON_ENTRY (obj);

  gtk_icon_entry_set_icon_from_pixbuf (entry, GTK_ICON_ENTRY_PRIMARY, NULL);
  gtk_icon_entry_set_icon_from_pixbuf (entry, GTK_ICON_ENTRY_SECONDARY, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gtk_icon_entry_map (GtkWidget *widget)
{
  GtkIconEntryPrivate *priv;
  GdkCursor *cursor;

  if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_MAPPED (widget))
    {
      int i;

      GTK_WIDGET_CLASS (parent_class)->map (widget);

      priv = GTK_ICON_ENTRY (widget)->priv;

      for (i = 0; i < MAX_ICONS; i++)
	{
	  if (priv->icons[i].pixbuf != NULL)
	    gdk_window_show (priv->icons[i].window);

	  if (priv->icons[i].custom_cursor == TRUE && !priv->icons[i].insensitive)
	    {
	      cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						   priv->icons[i].cursor_type);

	      gdk_window_set_cursor (priv->icons[i].window, cursor);
	      gdk_cursor_unref (cursor);
	    }
	}

      GTK_WIDGET_CLASS (parent_class)->map (widget);
    }
}

static void
gtk_icon_entry_unmap (GtkWidget *widget)
{
  GtkIconEntryPrivate *priv;

  if (GTK_WIDGET_MAPPED (widget))
    {
      int i;

      priv = GTK_ICON_ENTRY (widget)->priv;

      for (i = 0; i < MAX_ICONS; i++)
	{
	  if (priv->icons[i].pixbuf != NULL)
	    {
	      gdk_window_hide (priv->icons[i].window);
	    }
	}

      GTK_WIDGET_CLASS (parent_class)->unmap (widget);
    }
}

static void
gtk_icon_entry_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkIconEntry *entry = GTK_ICON_ENTRY (object);

  switch (prop_id)
    {
    case PROP_PIXBUF_PRIMARY:
      gtk_icon_entry_set_icon_from_pixbuf (entry,
					   GTK_ICON_ENTRY_PRIMARY,
					   g_value_get_object (value));
      break;

    case PROP_PIXBUF_SECONDARY:
      gtk_icon_entry_set_icon_from_pixbuf (entry,
					   GTK_ICON_ENTRY_SECONDARY,
					   g_value_get_object (value));
      break;

    case PROP_STOCK_PRIMARY:
      gtk_icon_entry_set_icon_from_stock (entry,
					  GTK_ICON_ENTRY_PRIMARY,
					  g_value_get_string (value));
      break;

    case PROP_STOCK_SECONDARY:
      gtk_icon_entry_set_icon_from_stock (entry,
					  GTK_ICON_ENTRY_SECONDARY,
					  g_value_get_string (value));
      break;

    case PROP_ICON_NAME_PRIMARY:
      gtk_icon_entry_set_icon_from_icon_name (entry,
					      GTK_ICON_ENTRY_PRIMARY,
					      g_value_get_string (value));
      break;

    case PROP_ICON_NAME_SECONDARY:
      gtk_icon_entry_set_icon_from_icon_name (entry,
					      GTK_ICON_ENTRY_SECONDARY,
					      g_value_get_string (value));
      break;

    case PROP_GICON_PRIMARY:
      gtk_icon_entry_set_icon_from_gicon (entry,
					  GTK_ICON_ENTRY_PRIMARY,
					  g_value_get_object (value));
      break;

    case PROP_GICON_SECONDARY:
      gtk_icon_entry_set_icon_from_gicon (entry,
					  GTK_ICON_ENTRY_SECONDARY,
					  g_value_get_object (value));
      break;
    }
}

static void
gtk_icon_entry_get_property (GObject      *object,
			     guint         prop_id,
			     GValue       *value,
			     GParamSpec   *pspec)
{
  GtkIconEntry *entry = GTK_ICON_ENTRY (object);

  switch (prop_id)
    {
    case PROP_PIXBUF_PRIMARY:
      g_value_set_object (value,
			  gtk_icon_entry_get_pixbuf (entry,
						     GTK_ICON_ENTRY_PRIMARY));
      break;

    case PROP_PIXBUF_SECONDARY:
      g_value_set_object (value,
			  gtk_icon_entry_get_pixbuf (entry,
						     GTK_ICON_ENTRY_SECONDARY));
      break;

    case PROP_GICON_PRIMARY:
      g_value_set_object (value,
			  gtk_icon_entry_get_gicon (entry,
						    GTK_ICON_ENTRY_PRIMARY));
      break;

    case PROP_GICON_SECONDARY:
      g_value_set_object (value,
			  gtk_icon_entry_get_gicon (entry,
						    GTK_ICON_ENTRY_SECONDARY));
    }
}

static gint
get_icon_width (GtkIconEntry *entry, GtkIconEntryPosition icon_pos)
{
  gint menu_icon_width;
  gint width;
  GtkIconEntryPrivate *priv;
  EntryIconInfo *icon_info;

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  if (icon_info->pixbuf == NULL)
    return 0;

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &menu_icon_width, NULL);

  width = MAX (gdk_pixbuf_get_width (icon_info->pixbuf), menu_icon_width);

  return width;
}

static void
get_borders (GtkIconEntry *entry, gint *xborder, gint *yborder)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  gint focus_width;
  gboolean interior_focus;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			NULL);

  if (gtk_entry_get_has_frame (GTK_ENTRY (entry)))
    {
      *xborder = widget->style->xthickness;
      *yborder = widget->style->ythickness;
    }
  else
    {
      *xborder = 0;
      *yborder = 0;
    }

  if (!interior_focus)
    {
      *xborder += focus_width;
      *yborder += focus_width;
    }
}

static void
get_text_area_size (GtkIconEntry *entry, GtkAllocation *alloc)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkRequisition requisition;
  gint xborder, yborder;

  gtk_widget_get_child_requisition (widget, &requisition);
  get_borders (entry, &xborder, &yborder);

  alloc->x      = xborder;
  alloc->y      = yborder;
  alloc->width  = widget->allocation.width - xborder * 2;
  alloc->height = requisition.height       - yborder * 2;
}

static void
get_icon_allocation (GtkIconEntry *icon_entry,
		     gboolean left,
		     GtkAllocation *widget_alloc,
		     GtkAllocation *text_area_alloc,
		     GtkAllocation *allocation,
		     GtkIconEntryPosition *icon_pos)
{
  gboolean rtl;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (icon_entry)) ==
	 GTK_TEXT_DIR_RTL);

  if (left)
    *icon_pos = (rtl ? GTK_ICON_ENTRY_SECONDARY : GTK_ICON_ENTRY_PRIMARY);
  else
    *icon_pos = (rtl ? GTK_ICON_ENTRY_PRIMARY : GTK_ICON_ENTRY_SECONDARY);

  allocation->y = text_area_alloc->y;
  allocation->width = get_icon_width(icon_entry, *icon_pos);
  allocation->height = text_area_alloc->height;

  if (left)
    {
      allocation->x = text_area_alloc->x + ICON_MARGIN;
    }
  else
    {
      allocation->x = text_area_alloc->x + text_area_alloc->width -
	allocation->width - ICON_MARGIN;
    }
}

static void
gtk_icon_entry_realize (GtkWidget *widget)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  GdkWindowAttr attributes;
  gint attributes_mask;
  int i;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = 1;
  attributes.height = 1;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |=
    (GDK_EXPOSURE_MASK
     | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
     | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info;

      icon_info = &priv->icons[i];
      icon_info->window = gdk_window_new (widget->window, &attributes,
					  attributes_mask);
      gdk_window_set_user_data (icon_info->window, widget);

      gdk_window_set_background (icon_info->window,
				 &widget->style->base[GTK_WIDGET_STATE(widget)]);
    }

  gtk_widget_queue_resize (widget);
}

static void
gtk_icon_entry_unrealize (GtkWidget *widget)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  int i;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = &priv->icons[i];

      gdk_window_destroy (icon_info->window);
      icon_info->window = NULL;
    }
}

static void
gtk_icon_entry_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  GtkEntry *gtkentry;
  GtkIconEntry *entry;
  gint icon_widths = 0;
  int i;

  gtkentry = GTK_ENTRY(widget);
  entry    = GTK_ICON_ENTRY(widget);

  for (i = 0; i < MAX_ICONS; i++)
    {
      int icon_width = get_icon_width (entry, i);

      if (icon_width > 0)
	{
	  icon_widths += icon_width + ICON_MARGIN;
	}
    }

  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  if (icon_widths > requisition->width)
    requisition->width += icon_widths;
}

static void
place_windows (GtkIconEntry *icon_entry, GtkAllocation *widget_alloc)
{
  GtkIconEntryPosition left_icon_pos;
  GtkIconEntryPosition right_icon_pos;
  GtkAllocation left_icon_alloc;
  GtkAllocation right_icon_alloc;
  GtkAllocation text_area_alloc;
  GtkIconEntryPrivate *priv;
  gint y;

  priv = icon_entry->priv;

  get_text_area_size (icon_entry, &text_area_alloc);
  
  /* DJW center text/icon
   * TODO flicker needs to be eliminated
   */
  gdk_window_get_geometry (GTK_ENTRY (icon_entry)->text_area, NULL, &y, NULL, NULL, NULL);
  text_area_alloc.y = y;

  get_icon_allocation (icon_entry, TRUE, widget_alloc, &text_area_alloc,
		       &left_icon_alloc, &left_icon_pos);
  get_icon_allocation (icon_entry, FALSE, widget_alloc, &text_area_alloc,
		       &right_icon_alloc, &right_icon_pos);

  if (left_icon_alloc.width > 0)
    {
      text_area_alloc.x = left_icon_alloc.x + left_icon_alloc.width + ICON_MARGIN;
    }

  if (right_icon_alloc.width > 0)
    {
      text_area_alloc.width -= right_icon_alloc.width + ICON_MARGIN;
    }

  text_area_alloc.width -= text_area_alloc.x;

  gdk_window_move_resize (priv->icons[left_icon_pos].window,
			  left_icon_alloc.x, left_icon_alloc.y,
			  left_icon_alloc.width, left_icon_alloc.height);

  gdk_window_move_resize (priv->icons[right_icon_pos].window,
			  right_icon_alloc.x, right_icon_alloc.y,
			  right_icon_alloc.width, right_icon_alloc.height);

  gdk_window_move_resize (GTK_ENTRY (icon_entry)->text_area,
			  text_area_alloc.x, text_area_alloc.y,
			  text_area_alloc.width, text_area_alloc.height);
}

static void
gtk_icon_entry_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  g_return_if_fail (GTK_IS_ICON_ENTRY(widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  if (GTK_WIDGET_REALIZED (widget))
    place_windows (GTK_ICON_ENTRY (widget), allocation);
}

static GdkPixbuf *
get_pixbuf_from_icon (GtkIconEntry *entry, GtkIconEntryPosition icon_pos)
{
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  g_object_ref (icon_info->pixbuf);

  return icon_info->pixbuf;
}

/* Kudos to the gnome-panel guys. */
static void
colorshift_pixbuf (GdkPixbuf *dest, GdkPixbuf *src, int shift)
{
  gint i, j;
  gint width, height, has_alpha, src_rowstride, dest_rowstride;
  guchar *target_pixels;
  guchar *original_pixels;
  guchar *pix_src;
  guchar *pix_dest;
  int val;
  guchar r, g, b;

  has_alpha       = gdk_pixbuf_get_has_alpha (src);
  width           = gdk_pixbuf_get_width (src);
  height          = gdk_pixbuf_get_height (src);
  src_rowstride   = gdk_pixbuf_get_rowstride (src);
  dest_rowstride  = gdk_pixbuf_get_rowstride (dest);
  original_pixels = gdk_pixbuf_get_pixels (src);
  target_pixels   = gdk_pixbuf_get_pixels (dest);

  for (i = 0; i < height; i++)
    {
      pix_dest = target_pixels   + i * dest_rowstride;
      pix_src  = original_pixels + i * src_rowstride;

      for (j = 0; j < width; j++)
	{
	  r = *(pix_src++);
	  g = *(pix_src++);
	  b = *(pix_src++);

	  val = r + shift;
	  *(pix_dest++) = CLAMP(val, 0, 255);

	  val = g + shift;
	  *(pix_dest++) = CLAMP(val, 0, 255);

	  val = b + shift;
	  *(pix_dest++) = CLAMP(val, 0, 255);

	  if (has_alpha)
	    *(pix_dest++) = *(pix_src++);
	}
    }
}

static void
draw_icon (GtkWidget *widget, GtkIconEntryPosition icon_pos)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  EntryIconInfo *icon_info;
  GdkPixbuf *pixbuf;
  gint x, y, width, height;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  if (icon_info->pixbuf == NULL || !GTK_WIDGET_REALIZED (widget))
    return;

  if ((pixbuf = get_pixbuf_from_icon (entry, icon_pos)) == NULL)
    return;

  gdk_drawable_get_size (icon_info->window, &width, &height);

  if (width == 1 || height == 1)
    {
      /*
       * size_allocate hasn't been called yet. These are the default values.
       */
      return;
    }

  if (gdk_pixbuf_get_height (pixbuf) > height)
    {
      GdkPixbuf *temp_pixbuf;
      int scale;

      scale = height - (2 * ICON_MARGIN);

      temp_pixbuf = gdk_pixbuf_scale_simple (pixbuf, scale, scale,
					     GDK_INTERP_BILINEAR);

      g_object_unref (pixbuf);

      pixbuf = temp_pixbuf;
    }

  x = (width  - gdk_pixbuf_get_width(pixbuf)) / 2;
  y = (height - gdk_pixbuf_get_height(pixbuf)) / 2;

  if (icon_info->insensitive)
    {
      GdkPixbuf *temp_pixbuf;

      temp_pixbuf = gdk_pixbuf_copy (pixbuf);

      gdk_pixbuf_saturate_and_pixelate (pixbuf,
					temp_pixbuf,
					0.8f,
					TRUE);
      g_object_unref (pixbuf);
      pixbuf = temp_pixbuf;
    }
  else if (icon_info->hovered)
    {
      GdkPixbuf *temp_pixbuf;

      temp_pixbuf = gdk_pixbuf_copy (pixbuf);

      colorshift_pixbuf (temp_pixbuf, pixbuf, 30);

      g_object_unref (pixbuf);

      pixbuf = temp_pixbuf;
    }

  gdk_draw_pixbuf (icon_info->window, widget->style->black_gc, pixbuf,
		   0, 0, x, y, -1, -1,
		   GDK_RGB_DITHER_NORMAL, 0, 0);

  g_object_unref (pixbuf);
}

static gint
gtk_icon_entry_expose (GtkWidget *widget, GdkEventExpose *event)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;

  g_return_val_if_fail (GTK_IS_ICON_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gboolean found = FALSE;
      int i;

      for (i = 0; i < MAX_ICONS && !found; i++)
	{
	  EntryIconInfo *icon_info = &priv->icons[i];

	  if (event->window == icon_info->window)
	    {
	      gint width;
	      GtkAllocation text_area_alloc;

	      get_text_area_size (entry, &text_area_alloc);
	      gdk_drawable_get_size (icon_info->window, &width, NULL);

	      gtk_paint_flat_box (widget->style, icon_info->window,
				  GTK_WIDGET_STATE (widget), GTK_SHADOW_NONE,
				  NULL, widget, "entry_bg",
				  0, 0, width, text_area_alloc.height);

	      draw_icon (widget, i);

	      found = TRUE;
	    }
	}

      if (!found)
	GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static gint
gtk_icon_entry_enter_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  int i;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if (event->window == priv->icons[i].window)
	{
	  if (gtk_icon_entry_get_icon_highlight (entry, i))
	    {
	      priv->icons[i].hovered = TRUE;

	      if (priv->icons[i].tooltip_text != NULL)
		{
		  gtk_widget_set_tooltip_text (widget,
					       priv->icons[i].tooltip_text);
		  gtk_widget_set_has_tooltip (widget, TRUE);
		} else {
		  gtk_widget_set_has_tooltip (widget, FALSE);
	        }

	      gtk_widget_queue_draw (widget);

	      break;
	    }
	}
    }

  return FALSE;
}

static gint
gtk_icon_entry_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  int i;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if (event->window == priv->icons[i].window)
	{
	  if (gtk_icon_entry_get_icon_highlight (entry, i))
	    {
	      priv->icons[i].hovered = FALSE;

	      gtk_widget_set_has_tooltip (widget, FALSE);
	      gtk_widget_queue_draw (widget);

	      break;
	    }
	}
    }

  return FALSE;
}

static gint
gtk_icon_entry_button_press (GtkWidget *widget, GdkEventButton *event)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  int i;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if (event->window == priv->icons[i].window)
	{
	  if (event->button == 1 && gtk_icon_entry_get_icon_highlight (entry, i))
	    {
	      priv->icons[i].hovered = FALSE;

	      gtk_widget_queue_draw (widget);
	    }

	  g_signal_emit (entry, signals[ICON_PRESSED], 0, i, event->button);

	  return TRUE;
	}
    }

  if (GTK_WIDGET_CLASS (parent_class)->button_press_event)
    return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);

  return FALSE;
}

static gint
gtk_icon_entry_button_release (GtkWidget *widget, GdkEventButton *event)
{
  GtkIconEntry *entry;
  GtkIconEntryPrivate *priv;
  int i;

  entry = GTK_ICON_ENTRY (widget);
  priv = entry->priv;

  for (i = 0; i < MAX_ICONS; i++)
    {
      GdkWindow *icon_window = priv->icons[i].window;

      if (event->window == icon_window)
	{
	  int width, height;
	  gdk_drawable_get_size (icon_window, &width, &height);

	  if (event->button == 1 &&
	      gtk_icon_entry_get_icon_highlight (entry, i) &&
	      event->x >= 0     && event->y >= 0 &&
	      event->x <= width && event->y <= height)
	    {
	      priv->icons[i].hovered = TRUE;

	      gtk_widget_queue_draw (widget);
	    }

	  g_signal_emit (entry, signals[ICON_RELEASED], 0, i, event->button);

	  return TRUE;
	}
    }

  if (GTK_WIDGET_CLASS (parent_class)->button_release_event)
    return GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);

  return FALSE;
}

static void
gtk_icon_entry_style_set (GtkWidget *widget, GtkStyle *prev_style)
{
  GtkIconEntry *icon_entry;

  icon_entry = GTK_ICON_ENTRY (widget);

  if (GTK_WIDGET_CLASS (gtk_icon_entry_parent_class)->style_set)
    GTK_WIDGET_CLASS (gtk_icon_entry_parent_class)->style_set (widget, prev_style);

  icon_theme_changed (icon_entry);
}

static void
icon_theme_changed (GtkIconEntry *entry)
{
  GtkIconEntryPrivate *priv;
  int i;

  priv = entry->priv;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if (priv->icons[i].storage_type == GTK_IMAGE_ICON_NAME)
	{
	  g_object_unref (priv->icons[i].pixbuf);
	  priv->icons[i].pixbuf = NULL;

	  gtk_icon_entry_set_icon_from_icon_name (entry, i, priv->icons[i].icon_name);
	}
      else if (priv->icons[i].storage_type == _GTK_IMAGE_GICON)
	{
	  g_object_unref (priv->icons[i].pixbuf);
	  priv->icons[i].pixbuf = NULL;

	  gtk_icon_entry_set_icon_from_gicon (entry, i, priv->icons[i].gicon);
	}
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_icon_entry_set_icon_internal (GtkIconEntry *entry,
                                  GtkIconEntryPosition icon_pos,
                                  GdkPixbuf *pixbuf)
{                                 
  EntryIconInfo *icon_info;         
  GtkIconEntryPrivate *priv;      
  
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos));
  
  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];
  
  if (pixbuf == icon_info->pixbuf)
    return; 
    
  if (icon_pos == GTK_ICON_ENTRY_SECONDARY &&
      priv->icon_released_id != 0)
    { 
      g_signal_handler_disconnect (entry, priv->icon_released_id);
      priv->icon_released_id = 0; 
    } 
    
  if (pixbuf == NULL)
    {
      if (icon_info->pixbuf != NULL)
	{
	  g_object_unref (icon_info->pixbuf);
	  icon_info->pixbuf = NULL;
          
	  /* Explicitly check, as the pointer may become invalidated
	   * during destruction.
	   */
	  if (icon_info->window != NULL && GDK_IS_WINDOW (icon_info->window))
	    gdk_window_hide (icon_info->window);
	} 
    }   
  else
    {
      if (icon_info->window != NULL && icon_info->pixbuf == NULL)
	gdk_window_show (icon_info->window);

      icon_info->pixbuf = pixbuf;
      g_object_ref (pixbuf);
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

/**
 * gtk_icon_entry_new
 *
 * Creates a new GtkIconEntry widget.
 *
 * Returns a new #GtkIconEntry.
 */
GtkWidget *
gtk_icon_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_ICON_ENTRY, NULL));
}

/**
 * gtk_icon_entry_set_icon_from_pixbuf
 * @entry: A #GtkIconEntry.
 * @position: Icon position.
 * @pixbuf: A #GdkPixbuf.
 *
 * Sets the icon shown in the specified position using a pixbuf.
 */
void
gtk_icon_entry_set_icon_from_pixbuf (GtkIconEntry *entry,
				     GtkIconEntryPosition icon_pos,
				     GdkPixbuf *pixbuf)
{
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos));

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  if (pixbuf == icon_info->pixbuf)
    return;

  if (icon_pos == GTK_ICON_ENTRY_SECONDARY &&
      priv->icon_released_id != 0)
    {
      g_signal_handler_disconnect (entry, priv->icon_released_id);
      priv->icon_released_id = 0;
    }

  if (pixbuf == NULL)
    {
      if (icon_info->pixbuf != NULL)
	{
	  g_object_unref (icon_info->pixbuf);
	  icon_info->pixbuf = NULL;

	  /* Explicitly check, as the pointer may become invalidated
	   * during destruction.
	   */
	  if (icon_info->window != NULL && GDK_IS_WINDOW (icon_info->window))
	    gdk_window_hide (icon_info->window);
	}
    }
  else
    {
      if (icon_info->window != NULL && icon_info->pixbuf == NULL)
	gdk_window_show (icon_info->window);

      icon_info->pixbuf = pixbuf;
      g_object_ref (pixbuf);
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

/**
 * gtk_icon_entry_set_icon_from_stock
 * @entry: A #GtkIconEntry.
 * @position: Icon position.
 * @stock_id: The name of the stock item.
 *
 * Sets the icon shown in the entry at the specified position from a stock image.
 */
void
gtk_icon_entry_set_icon_from_stock (GtkIconEntry *entry,
				    GtkIconEntryPosition icon_pos,
				    const gchar *stock_id)
{
  GdkPixbuf *pixbuf;

  /* FIXME: Due to a bug in GtkIconEntry we need to set a non-NULL icon */
  if (! stock_id)
    stock_id = GTK_STOCK_INFO;

  pixbuf = gtk_widget_render_icon (GTK_WIDGET (entry),
				   stock_id,
				   GTK_ICON_SIZE_MENU,
				   NULL);

  gtk_icon_entry_set_icon_internal (entry,
                                    icon_pos,
                                    pixbuf);
}

/**
 * gtk_icon_entry_set_icon_from_icon_name
 * @entry: A #GtkIconEntry;
 * @icon_pos: The position at which to set the icon
 * @icon_name: An icon name
 *
 * Sets the icon shown in the entry at the specified position from the current
 * icon theme.  If the icon name isn't known, a "broken image" icon will be
 * displayed instead.  If the current icon theme is changed, the icon will be
 * updated appropriately.
 */
void
gtk_icon_entry_set_icon_from_icon_name (GtkIconEntry *entry,
					GtkIconEntryPosition icon_pos,
					const gchar *icon_name)
{
  GdkPixbuf *pixbuf = NULL;
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  gint width, height;
  GError *error = NULL;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos));

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  screen = gtk_widget_get_screen (GTK_WIDGET (entry));
  icon_theme = gtk_icon_theme_get_for_screen (screen);
  settings = gtk_settings_get_for_screen (screen);

  if (icon_name != NULL)
    {
      gtk_icon_size_lookup_for_settings (settings,
					 GTK_ICON_SIZE_MENU,
					 &width, &height);

      pixbuf = gtk_icon_theme_load_icon (icon_theme,
					 icon_name,
					 MIN (width, height), 0, &error);

      if (pixbuf == NULL)
	{
	  g_error_free (error);
	  pixbuf = gtk_widget_render_icon (GTK_WIDGET (entry),
					   GTK_STOCK_MISSING_IMAGE,
					   GTK_ICON_SIZE_MENU,
					   NULL);
	}
    }

  gtk_icon_entry_set_icon_internal (entry,
				    icon_pos,
				    pixbuf);
}

/**
 * gtk_icon_entry_set_icon_from_gicon
 * @entry: A #GtkIconEntry;
 * @icon_pos: The position at which to set the icon
 * @icon: The icon to set
 *
 * Sets the icon shown in the entry at the specified position from the current
 * icon theme.  If the icon isn't known, a "broken image" icon will be displayed
 * instead.  If the current icon theme is changed, the icon will be updated
 * appropriately.
 */
void
gtk_icon_entry_set_icon_from_gicon (const GtkIconEntry *entry,
				    GtkIconEntryPosition icon_pos,
				    GIcon *icon)
{
  GdkPixbuf *pixbuf = NULL;
  GtkIconEntryPrivate *priv;
  EntryIconInfo *icon_info;
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  gint width, height;
  GError *error = NULL;
  GtkIconInfo *info;

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  screen = gtk_widget_get_screen (GTK_WIDGET (entry));
  icon_theme = gtk_icon_theme_get_for_screen (screen);
  settings = gtk_settings_get_for_screen (screen);

  if (icon != NULL)
    {
      gtk_icon_size_lookup_for_settings (settings,
					 GTK_ICON_SIZE_MENU,
					 &width, &height);

      #if GTK_CHECK_VERSION (2, 14, 0)
      info = gtk_icon_theme_lookup_by_gicon (icon_theme,
					     icon,
					     MIN (width, height), 0);
      #else
      info = NULL;
      #endif
      pixbuf = gtk_icon_info_load_icon (info, &error);
      if (pixbuf == NULL)
	{
	  g_error_free (error);
	  pixbuf = gtk_widget_render_icon (GTK_WIDGET (entry),
					   GTK_STOCK_MISSING_IMAGE,
					   GTK_ICON_SIZE_MENU,
					   NULL);
	}
    }

  gtk_icon_entry_set_icon_internal ((GtkIconEntry*)entry,
				    icon_pos,
				    pixbuf);
}

/**
 * gtk_icon_entry_set_cursor
 * @entry: A #GtkIconEntry;
 * @icon_pos: The position at which to set the cursor
 * @cursor_type: A #GdkCursorType; describing the cursor to set
 *
 * Sets an alternate mouse cursor used for the specified icon.
 */
void
gtk_icon_entry_set_cursor (const GtkIconEntry *entry,
			   GtkIconEntryPosition icon_pos,
			   GdkCursorType cursor_type)
{
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;
  GdkCursor *cursor;

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  icon_info->cursor_type = cursor_type;
  icon_info->custom_cursor = TRUE;

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (entry)))
    {
      cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (entry)),
					   cursor_type);

      gdk_window_set_cursor (icon_info->window, cursor);
      gdk_cursor_unref (cursor);
    }
}

/**
 * gtk_icon_entry_set_icon_highlight
 * @entry: A #GtkIconEntry;
 * @position: Icon position.
 * @highlight: TRUE if the icon should highlight on mouse-over
 *
 * Determines whether the icon will highlight on mouse-over.
 */
void
gtk_icon_entry_set_icon_highlight (const GtkIconEntry *entry,
				   GtkIconEntryPosition icon_pos,
				   gboolean highlight)
{
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;

  priv = entry->priv;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos));

  icon_info = &priv->icons[icon_pos];

  if (icon_info->highlight == highlight)
    return;

  icon_info->highlight = highlight;
}

/**
 * gtk_icon_entry_get_pixbuf
 * @entry: A #GtkIconEntry.
 * @position: Icon position.
 *
 * Retrieves the image used for the icon.  Unlike the other methods of setting
 * and getting icon data, this method will work regardless of whether the icon
 * was set using a #GdkPixbuf, a #GIcon, a stock item, or an icon name.
 *
 * Returns: A #GdkPixbuf, or NULL if no icon is set for this position.
 */
GdkPixbuf *
gtk_icon_entry_get_pixbuf (const GtkIconEntry *entry,
			   GtkIconEntryPosition icon_pos)
{
  GtkIconEntryPrivate *priv;

  g_return_val_if_fail (entry != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ICON_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos), NULL);

  priv = entry->priv;

  return priv->icons[icon_pos].pixbuf;
}

/**
 * gtk_icon_entry_get_gicon
 * @entry: A #GtkIconEntry
 * @position: Icon position.
 *
 * Retrieves the GIcon used for the icon, or NULL if there is no icon or if
 * the icon was set by some other method (e.g., by stock, pixbuf, or icon name).
 *
 * Returns: A #GIcon, or NULL if no icon is set or if the icon is not a GIcon.
 */
GIcon *
gtk_icon_entry_get_gicon (const GtkIconEntry *entry,
			  GtkIconEntryPosition icon_pos)
{
  GtkIconEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (entry != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ICON_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos), NULL);

  priv = entry->priv;
  icon_info = &priv->icons[icon_pos];

  return icon_info->storage_type == _GTK_IMAGE_GICON ? icon_info->gicon : NULL;
}

/**
 * gtk_icon_entry_get_icon_highlight
 * @entry: A #GtkIconEntry.
 * @position: Icon position.
 *
 * Retrieves whether entry will highlight the icon on mouseover.
 *
 * Returns: TRUE if icon highlights.
 */
gboolean
gtk_icon_entry_get_icon_highlight (const GtkIconEntry *entry,
				   GtkIconEntryPosition icon_pos)
{
  GtkIconEntryPrivate *priv;

  g_return_val_if_fail (entry != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ICON_ENTRY (entry), FALSE);
  g_return_val_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos), FALSE);

  priv = entry->priv;

  return priv->icons[icon_pos].highlight;
}

/**
 * gtk_icon_entry_set_tooltip
 * @entry: A #GtkIconEntry.
 * @position: Icon position.
 * @text: The text to be used for the tooltip.
 *
 * Sets the tooltip text used for the specified icon.
 */
void
gtk_icon_entry_set_tooltip (const GtkIconEntry *entry,
			    GtkIconEntryPosition icon_pos,
			    const gchar *text)
{
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;
  gchar *new_tooltip;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos));

  priv = entry->priv;

  icon_info = &priv->icons[icon_pos];

  new_tooltip = g_strdup (text);
  if (icon_info->tooltip_text != NULL)
    g_free (icon_info->tooltip_text);
  icon_info->tooltip_text = new_tooltip;
}

/**
 * gtk_icon_entry_set_icon_sensitive
 * @entry: A #GtkIconEntry.
 * @position: Icon position.
 * @sensitive: Specifies whether the icon should appear sensitive or insensitive.
 *
 * Sets the sensitivity for the specified icon.
 */
void
gtk_icon_entry_set_icon_sensitive (const GtkIconEntry *icon_entry,
				   GtkIconEntryPosition icon_pos,
				   gboolean sensitive)
{
  EntryIconInfo *icon_info;
  GtkIconEntryPrivate *priv;

  g_return_if_fail (icon_entry != NULL);
  g_return_if_fail (GTK_IS_ICON_ENTRY (icon_entry));
  g_return_if_fail (IS_VALID_ICON_ENTRY_POSITION (icon_pos));

  priv = icon_entry->priv;

  icon_info = &priv->icons[icon_pos];

  icon_info->insensitive = !sensitive;

  if (icon_info->custom_cursor == TRUE && GTK_WIDGET_REALIZED (GTK_WIDGET (icon_entry)))
    {
      GdkCursor *cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (icon_entry)),
						      sensitive ? icon_info->cursor_type : GDK_ARROW);
      gdk_window_set_cursor (icon_info->window, cursor);
      gdk_cursor_unref (cursor);
    }
}

void
gtk_icon_entry_set_progress_fraction (GtkIconEntry *icon_entry,
                                      gdouble       fraction)
{
  GtkIconEntryPrivate *priv;

  g_return_if_fail (GTK_IS_ICON_ENTRY (icon_entry));

  priv = icon_entry->priv;
  priv->fraction = CLAMP (fraction, 0.0, 1.0);

  if (GTK_ENTRY (icon_entry)->text_area)
    gdk_window_invalidate_rect (GTK_ENTRY (icon_entry)->text_area, NULL, FALSE);
}

#endif
