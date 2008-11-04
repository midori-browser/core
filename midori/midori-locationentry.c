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
#include <string.h>

#define DEFAULT_ICON GTK_STOCK_FILE
#define MAX_ITEMS 25

struct _MidoriLocationEntry
{
    GtkComboBoxEntry parent_instance;

    gdouble progress;
};

struct _MidoriLocationEntryClass
{
    GtkComboBoxEntryClass parent_class;
};

G_DEFINE_TYPE (MidoriLocationEntry, midori_location_entry, GTK_TYPE_COMBO_BOX_ENTRY)

enum
{
    FAVICON_COL,
    URI_COL,
    TITLE_COL,
    VISIBLE_COL,
    N_COLS
};

enum
{
    ACTIVE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean
entry_key_press_event (GtkWidget*           widget,
                       GdkEventKey*         event,
                       MidoriLocationEntry* location_entry);

static void
midori_location_entry_changed (GtkComboBox* combo_box,
                               gpointer     user_data);

static void
midori_location_entry_class_init (MidoriLocationEntryClass* class)
{
    signals[ACTIVE_CHANGED] = g_signal_new ("active-changed",
                                            G_TYPE_FROM_CLASS (class),
                                            (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                            0,
                                            0,
                                            NULL,
                                            g_cclosure_marshal_VOID__INT,
                                            G_TYPE_NONE, 1,
                                            G_TYPE_INT);
}

static GtkTreeModel*
midori_location_entry_get_model (MidoriLocationEntry* location_entry)
{
    GtkTreeModel* model;
    GtkTreeModelFilter* filter_model;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    filter_model = GTK_TREE_MODEL_FILTER (model);
    g_assert (filter_model);

    return gtk_tree_model_filter_get_model (filter_model);
}

#define HAVE_ENTRY_PROGRESS 1

#ifdef HAVE_ENTRY_PROGRESS

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
      gint start_index = g_utf8_offset_to_pointer (text, start_char) - text;
      gint end_index = g_utf8_offset_to_pointer (text, end_char) - text;
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

void
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

  if (location_entry->progress > 0.0/* && location_entry->progress < 1.0*/)
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

gdouble
midori_location_entry_get_progress (MidoriLocationEntry* location_entry)
{
    g_return_val_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry), 0.0);

    return location_entry->progress;
}

void
midori_location_entry_set_progress (MidoriLocationEntry* location_entry,
                                    gdouble              progress)
{
    #ifdef HAVE_ENTRY_PROGRESS
    GtkWidget* child;
    #endif

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    location_entry->progress = CLAMP (progress, 0.0, 1.0);

    #ifdef HAVE_ENTRY_PROGRESS
    child = gtk_bin_get_child (GTK_BIN (location_entry));
    if (GTK_ENTRY (child)->text_area)
        gdk_window_invalidate_rect (GTK_ENTRY (child)->text_area, NULL, FALSE);
    #endif
}

static gboolean
midori_location_entry_completion_selected (GtkEntryCompletion*  completion,
                                           GtkTreeModel*        model,
                                           GtkTreeIter*         iter,
                                           MidoriLocationEntry* location_entry)
{
    gchar* uri;

    gtk_tree_model_get (model, iter, URI_COL, &uri, -1);
    midori_location_entry_set_item_from_uri (location_entry, uri);
    g_free (uri);

    return FALSE;
}

static void
midori_location_entry_render_pixbuf_cb (GtkCellLayout*   layout,
                                        GtkCellRenderer* renderer,
                                        GtkTreeModel*    model,
                                        GtkTreeIter*     iter,
                                        gpointer         data)
{
    GdkPixbuf* pixbuf;

    gtk_tree_model_get (model, iter, FAVICON_COL, &pixbuf, -1);
    if (pixbuf)
    {
        g_object_set (renderer, "pixbuf", pixbuf, NULL);
        g_object_set (renderer, "xpad", 5, "ypad", 5, "yalign", 0.0, NULL);
        g_object_unref (pixbuf);
    }
}

static void
midori_location_entry_render_text_cb (GtkCellLayout*   layout,
                                      GtkCellRenderer* renderer,
                                      GtkTreeModel*    model,
                                      GtkTreeIter*     iter,
                                      gpointer         data)
{
    gchar* uri;
    gchar* title;
    gchar* desc;

    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title, -1);
    if (title)
        desc = g_markup_printf_escaped ("<b>%s</b> - %s", uri, title);
    else
        desc = g_markup_printf_escaped ("<b>%s</b>", uri);

    g_object_set (renderer, "markup", desc, "xpad", 5, "ypad", 5, NULL);
    g_object_set (renderer, "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    katze_assign (uri, NULL);
    katze_assign (title, NULL);
    katze_assign (desc, NULL);
}

static gboolean
midori_location_entry_completion_match_cb (GtkEntryCompletion* completion,
                                           const gchar*        key,
                                           GtkTreeIter*        iter,
                                           gpointer            data)
{
    GtkTreeModel* model;
    gchar* uri;
    gchar* title;
    gboolean match = FALSE;
    gchar *temp;

    model = gtk_entry_completion_get_model (completion);
    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title, -1);

    if (uri)
    {
        temp = g_utf8_casefold (uri, -1);
        match = (strstr (temp, key) != NULL);
        g_free (temp);
        g_free (uri);

        if (!match && title)
        {
            temp = g_utf8_casefold (title, -1);
            match = (strstr (temp, key) != NULL);
            g_free (temp);
            g_free (title);
        }
    }
    return match;
}

static void
midori_location_entry_completion_init (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;
    GtkEntryCompletion* completion;
    GtkCellRenderer* renderer;

    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    completion = gtk_entry_completion_new ();

    gtk_entry_completion_set_model (GTK_ENTRY_COMPLETION (completion),
        midori_location_entry_get_model (location_entry));
    gtk_entry_completion_set_text_column (GTK_ENTRY_COMPLETION (completion),
                                          URI_COL);
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (completion));

    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion), renderer,
                                        midori_location_entry_render_pixbuf_cb,
                                        NULL, NULL);
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, TRUE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion), renderer,
                                        midori_location_entry_render_text_cb,
                                        NULL, NULL);
    gtk_entry_completion_set_match_func (completion,
        midori_location_entry_completion_match_cb, NULL, NULL);

    gtk_entry_set_completion (GTK_ENTRY (entry), completion);
    g_signal_connect (completion, "match-selected",
        G_CALLBACK (midori_location_entry_completion_selected), location_entry);

    g_object_unref (completion);
}

static void
midori_location_entry_init (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;
    GtkListStore* store;
    GtkCellRenderer* renderer;
    GtkTreeModel* filter_model;

    /* we want the widget to have appears-as-list applied */
    gtk_rc_parse_string ("style \"midori-location-entry-style\" {\n"
                         "  GtkComboBox::appears-as-list = 1\n }\n"
                         "widget_class \"*MidoriLocationEntry\" "
                         "style \"midori-location-entry-style\"\n");

    location_entry->progress = 0.0;

    entry = gtk_icon_entry_new ();
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_PRIMARY, DEFAULT_ICON);
    g_signal_connect_after (entry, "key-press-event",
        G_CALLBACK (entry_key_press_event), location_entry);
    #ifdef HAVE_ENTRY_PROGRESS
    g_signal_connect_after (entry, "expose-event",
        G_CALLBACK (entry_expose_event), location_entry);
    #endif

    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (location_entry), entry);

    store = gtk_list_store_new (N_COLS,
        GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
    gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (filter_model), VISIBLE_COL);
    g_object_set (location_entry, "model", filter_model, NULL);
    g_object_unref (store);

    gtk_combo_box_entry_set_text_column (
        GTK_COMBO_BOX_ENTRY (location_entry), URI_COL);
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (location_entry));

    /* setup the renderer for the favicon */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (location_entry),
                                renderer, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (location_entry), renderer,
                                        midori_location_entry_render_pixbuf_cb,
                                        NULL, NULL);
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (location_entry),
                                renderer, TRUE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (location_entry), renderer,
                                        midori_location_entry_render_text_cb,
                                        NULL, NULL);

    gtk_combo_box_set_active (GTK_COMBO_BOX (location_entry), -1);

    midori_location_entry_completion_init (location_entry);

    g_signal_connect (location_entry, "changed",
        G_CALLBACK (midori_location_entry_changed), NULL);
}

static gboolean
entry_key_press_event (GtkWidget*           widget,
                       GdkEventKey*         event,
                       MidoriLocationEntry* location_entry)
{
    switch (event->keyval)
    {
        case GDK_Down:
        case GDK_Up:
        {
            if (!sokoke_object_get_boolean (location_entry, "popup-shown"))
                gtk_combo_box_popup (GTK_COMBO_BOX (location_entry));
            return TRUE;
        }
    }

    return FALSE;
}

static void
midori_location_entry_changed (GtkComboBox* combo_box,
                               gpointer     user_data)
{
    GtkTreeIter iter;
    GtkIconEntry* entry;
    GtkTreeModel* model;
    GdkPixbuf* pixbuf;

    if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
        entry = GTK_ICON_ENTRY (GTK_BIN (combo_box)->child);

        if (entry)
        {
            pixbuf = NULL;

            model = gtk_combo_box_get_model (combo_box);
            gtk_tree_model_get (model, &iter, FAVICON_COL, &pixbuf, -1);

            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                                                 GTK_ICON_ENTRY_PRIMARY, pixbuf);
            g_object_unref (pixbuf);

            g_signal_emit (MIDORI_LOCATION_ENTRY (combo_box),
                signals[ACTIVE_CHANGED], 0, gtk_combo_box_get_active (combo_box));
        }
    }
}

static void
midori_location_entry_set_item (MidoriLocationEntry*     entry,
                                GtkTreeIter*             iter,
                                MidoriLocationEntryItem* item)
{
    GtkTreeModel* model;
    GdkPixbuf* icon;
    GdkPixbuf* new_icon;

    model = midori_location_entry_get_model (entry);
    gtk_list_store_set (GTK_LIST_STORE (model), iter,
        URI_COL, item->uri, TITLE_COL, item->title, -1);

    gtk_tree_model_get (model, iter, FAVICON_COL, &icon, -1);
    if (item->favicon)
        new_icon = g_object_ref (item->favicon);
    else if (!icon && !item->favicon)
        new_icon = gtk_widget_render_icon (GTK_WIDGET (entry), DEFAULT_ICON,
                                           GTK_ICON_SIZE_MENU, NULL);
    else
        new_icon = NULL;
    if (new_icon)
    {
        gtk_list_store_set (GTK_LIST_STORE (model), iter,
            FAVICON_COL, new_icon, -1);
        g_object_unref (new_icon);
    }
}

static void
midori_location_entry_set_active_iter (MidoriLocationEntry* location_entry,
                                       GtkTreeIter*         iter)
{
    GdkPixbuf* pixbuf;
    GtkTreeModel* model;
    GtkWidget* entry;
    GtkTreeModel* filter_model;
    GtkTreeIter filter_iter;

    entry = gtk_bin_get_child (GTK_BIN (location_entry));

    model = midori_location_entry_get_model (location_entry);
    filter_model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));

    /* The filter iter must be set, not the child iter,
     * but the row must first be set as visible to
     * convert to a filter iter without error.
     */
    gtk_list_store_set (GTK_LIST_STORE (model), iter, VISIBLE_COL, TRUE, -1);
    gtk_tree_model_filter_convert_child_iter_to_iter (
        GTK_TREE_MODEL_FILTER (filter_model), &filter_iter, iter);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (location_entry), &filter_iter);

    /* When setting the active iter (when adding or setting an item)
     * the favicon may have changed, so we must update the entry favicon.
     */
    if (entry)
    {
        gtk_tree_model_get (model, iter, FAVICON_COL, &pixbuf, -1);

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, pixbuf);

        g_object_unref (pixbuf);
    }
}

/**
 * midori_location_entry_new:
 *
 * Creates a new #MidoriLocationEntry.
 *
 * Return value: a new #MidoriLocationEntry
 **/
GtkWidget*
midori_location_entry_new (void)
{
    return (g_object_new (MIDORI_TYPE_LOCATION_ENTRY, NULL));
}

/**
 * midori_location_entry_item_iter:
 * @location_entry: a #MidoriLocationEntry
 * @uri: a string
 * @iter: a GtkTreeIter
 *
 * Retrieves the iter of the item matching @uri.
 *
 * Return value: %TRUE if @uri was found, %FALSE otherwise
 **/
gboolean
midori_location_entry_item_iter (MidoriLocationEntry* location_entry,
                                 const gchar*         uri,
                                 GtkTreeIter*         iter)
{
    GtkTreeModel* model;
    gchar* tmpuri;
    gboolean found;

    g_return_val_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    found = FALSE;
    model = midori_location_entry_get_model (location_entry);
    if (gtk_tree_model_get_iter_first (model, iter))
    {
        tmpuri = NULL;
        do
        {
            gtk_tree_model_get (model, iter, URI_COL, &tmpuri, -1);
            found = !g_ascii_strcasecmp (uri, tmpuri);
            katze_assign (tmpuri, NULL);

            if (found)
                break;
        }
        while (gtk_tree_model_iter_next (model, iter));
    }
    return found;
}

/**
 * midori_location_entry_get_text:
 * @location_entry: a #MidoriLocationEntry
 *
 * Retrieves the text of the embedded entry.
 *
 * Return value: a string
 **/
const gchar*
midori_location_entry_get_text (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;

    g_return_val_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry), NULL);

    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

    return gtk_entry_get_text (GTK_ENTRY (entry));
}

/**
 * midori_location_entry_set_text:
 * @location_entry: a #MidoriLocationEntry
 * @text: a string
 *
 * Sets the entry text to @text and, if applicable, updates the icon.
 **/
void
midori_location_entry_set_text (MidoriLocationEntry* location_entry,
                                const gchar*         text)
{
    GtkWidget* entry;
    GtkTreeIter iter;
    GtkTreeModel* model;
    GdkPixbuf* icon;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    g_return_if_fail (GTK_IS_ENTRY (entry));

    gtk_entry_set_text (GTK_ENTRY (entry), text);
    if (midori_location_entry_item_iter (location_entry, text, &iter))
    {
        model = midori_location_entry_get_model (location_entry);
        gtk_tree_model_get (model, &iter, FAVICON_COL, &icon, -1);
        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
    /* FIXME: Due to a bug in GtkIconEntry we can't reset the icon
    else
        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, DEFAULT_ICON);*/
}

/**
 * midori_location_entry_clear:
 * @location_entry: a #MidoriLocationEntry
 *
 * Clears the entry text and resets the entry favicon.
 **/
void
midori_location_entry_clear (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    g_return_if_fail (GTK_IS_ICON_ENTRY (entry));

    gtk_entry_set_text (GTK_ENTRY (entry), "");
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
        GTK_ICON_ENTRY_PRIMARY, DEFAULT_ICON);
}

/**
 * midori_location_entry_set_item_from_uri:
 * @location_entry: a #MidoriLocationEntry
 * @uri: a string
 *
 * Finds the item from the list matching @uri and sets it as the active item.
 * If @uri is not found it clears the active item.
 **/
void
midori_location_entry_set_item_from_uri (MidoriLocationEntry* location_entry,
                                         const gchar*         uri)
{
    GtkTreeIter iter;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    if (midori_location_entry_item_iter (location_entry, uri, &iter))
        midori_location_entry_set_active_iter (location_entry, &iter);
    else
        midori_location_entry_clear (location_entry);

}

/**
 * midori_location_entry_prepend_item:
 * @location_entry: a #MidoriLocationEntry
 * @item: a MidoriLocationItem
 *
 * Prepends @item if it is not already in the list.
 * If the item already exists, it is moved before the first item.
 * If the maximum is reached, the last item is removed.
 **/
void
midori_location_entry_prepend_item (MidoriLocationEntry*     location_entry,
                                    MidoriLocationEntryItem* item)
{
    GtkTreeModel* filter_model;
    GtkTreeModel* model;
    GtkTreeIter iter;
    GtkTreeIter index;
    gint n;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));
    g_return_if_fail (item->uri != NULL);

    filter_model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    model = midori_location_entry_get_model (location_entry);

    if (!midori_location_entry_item_iter (location_entry, item->uri, &iter))
    {
        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
    }
    else
    {
        gtk_tree_model_get_iter_first (model, &index);
        gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &index);
    }
    n = gtk_tree_model_iter_n_children (filter_model, NULL);
    if (n > MAX_ITEMS)
    {
        gtk_tree_model_iter_nth_child (model, &index, NULL, n - 1);
        gtk_list_store_set (GTK_LIST_STORE (model), &index, VISIBLE_COL, FALSE, -1);
    }
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, VISIBLE_COL, TRUE, -1);
    midori_location_entry_set_item (location_entry, &iter, item);
}

/**
 * midori_location_entry_append_item:
 * @location_entry: a #MidoriLocationEntry
 * @item: a MidoriLocationItem
 *
 * Appends @item if it is not already in the list.
 * @item is not added if the maximum is reached.
 **/
void
midori_location_entry_append_item (MidoriLocationEntry*     location_entry,
                                   MidoriLocationEntryItem* item)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    gint n;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));
    g_return_if_fail (item->uri != NULL);

    model = midori_location_entry_get_model (location_entry);

    if (!midori_location_entry_item_iter (location_entry, item->uri, &iter))
    {
          gtk_list_store_append (GTK_LIST_STORE (model), &iter);

          n = gtk_tree_model_iter_n_children (model, NULL);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, VISIBLE_COL,
                              (n <= MAX_ITEMS), -1);
    }
    midori_location_entry_set_item (location_entry, &iter, item);
}

