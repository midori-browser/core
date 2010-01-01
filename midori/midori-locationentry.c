/*
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-locationentry.h"

#include "gtkiconentry.h"
#include "sokoke.h"
#include <gdk/gdkkeysyms.h>

struct _MidoriLocationEntry
{
    GtkComboBoxEntry parent_instance;

    gdouble progress;
};

struct _MidoriLocationEntryClass
{
    GtkComboBoxEntryClass parent_class;
};

G_DEFINE_TYPE (MidoriLocationEntry,
    midori_location_entry, GTK_TYPE_COMBO_BOX_ENTRY)

static void
midori_location_entry_class_init (MidoriLocationEntryClass* class)
{

}

#if !GTK_CHECK_VERSION (2, 16, 0)

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
entry_expose_event (GtkWidget*           entry,
                    GdkEventExpose*      event,
                    MidoriLocationEntry* location_entry)
{
  GdkWindow* text_area;
  gint width, height;

  text_area = GTK_ENTRY (entry)->text_area;

  gdk_drawable_get_size (text_area, &width, &height);

  if (location_entry->progress > 0.0)
  {
      gtk_paint_box (entry->style, text_area,
                     GTK_STATE_SELECTED, GTK_SHADOW_OUT,
                     &event->area, entry, "bar",
                     0, 0, location_entry->progress * width, height);
      gtk_entry_draw_text (GTK_ENTRY (entry));
  }
  return FALSE;
}

#endif

void
midori_location_entry_set_progress (MidoriLocationEntry* location_entry,
                                    gdouble              progress)
{
    GtkWidget* child;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    location_entry->progress = CLAMP (progress, 0.0, 1.0);

    child = gtk_bin_get_child (GTK_BIN (location_entry));
    #if !GTK_CHECK_VERSION (2, 16, 0)
    if (GTK_ENTRY (child)->text_area)
        gdk_window_invalidate_rect (GTK_ENTRY (child)->text_area, NULL, FALSE);
    #else
    gtk_entry_set_progress_fraction (GTK_ENTRY (child), progress);
    #endif
}

static void
midori_location_entry_init (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;
    #if HAVE_HILDON
    HildonGtkInputMode mode;
    #endif

    /* We want the widget to have appears-as-list applied */
    gtk_rc_parse_string ("style \"midori-location-entry-style\" {\n"
                         "  GtkComboBox::appears-as-list = 1\n }\n"
                         "widget_class \"*MidoriLocationEntry\" "
                         "style \"midori-location-entry-style\"\n");

    location_entry->progress = 0.0;

    #if HAVE_HILDON
    entry = gtk_entry_new ();
    mode = hildon_gtk_entry_get_input_mode (GTK_ENTRY (entry));
    mode &= ~HILDON_GTK_INPUT_MODE_AUTOCAP;
    hildon_gtk_entry_set_input_mode (GTK_ENTRY (entry), mode);
    #else
    entry = gtk_icon_entry_new ();
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_PRIMARY, GTK_STOCK_FILE);
    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_SECONDARY, TRUE);
    #endif
    #if !GTK_CHECK_VERSION (2, 16, 0)
    g_signal_connect_after (entry, "expose-event",
        G_CALLBACK (entry_expose_event), location_entry);
    #endif
    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (location_entry), entry);
}
