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

#define DEFAULT_ICON GTK_STOCK_FILE

G_DEFINE_TYPE (MidoriLocationEntry, midori_location_entry, GTK_TYPE_COMBO_BOX_ENTRY)

enum
{
    FAVICON_COL,
    URI_COL,
    TITLE_COL,
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
midori_location_entry_active_changed (GtkComboBox* combo_box,
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

static void
midori_location_entry_init (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;
    GtkListStore* store;
    GtkCellRenderer* renderer;

    /* we want the widget to have appears-as-list applied */
    gtk_rc_parse_string ("style \"midori-location-entry-style\" {\n"
                         "  GtkComboBox::appears-as-list = 1\n }\n"
                         "widget_class \"*MidoriLocationEntry\" "
                         "style \"midori-location-entry-style\"\n");

    entry = gtk_icon_entry_new ();
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry), GTK_ICON_ENTRY_PRIMARY, DEFAULT_ICON);
    g_signal_connect (entry, "key-press-event", G_CALLBACK (entry_key_press_event), location_entry);

    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (location_entry), entry);

    store = gtk_list_store_new (N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    g_object_set (G_OBJECT (location_entry), "model", store, NULL);
    g_object_unref(store);

    gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (location_entry), URI_COL);
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (location_entry));

    /* setup the renderer for the favicon */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (location_entry), renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (location_entry), renderer, "pixbuf", FAVICON_COL, NULL);
    g_object_set (G_OBJECT (renderer), "xpad", 5, "ypad", 5, "yalign", 0.0, NULL);

    /* setup the renderer for the uri/title */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (location_entry), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (location_entry), renderer, "markup", TITLE_COL, NULL);
    g_object_set (G_OBJECT (renderer), "xpad", 5, "ypad", 5, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    gtk_combo_box_set_active (GTK_COMBO_BOX (location_entry), -1);

    g_signal_connect (location_entry, "changed", G_CALLBACK (midori_location_entry_active_changed), NULL);
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
midori_location_entry_active_changed (GtkComboBox* combo_box,
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
midori_location_entry_set_item (GtkTreeModel*            model,
                                GtkTreeIter*             iter,
                                MidoriLocationEntryItem* item)
{
    gchar* desc = NULL;

    if (item->title)
        desc = g_markup_printf_escaped ("<b>%s</b> - %s", item->uri, item->title);
    else
        desc = g_markup_printf_escaped ("<b>%s</b>", item->uri);

    gtk_list_store_set (GTK_LIST_STORE (model), iter,
        FAVICON_COL, item->favicon, URI_COL, item->uri, TITLE_COL, desc, -1);

    g_free (desc);
}

static void
midori_location_entry_set_active_iter (MidoriLocationEntry* location_entry,
                                       GtkTreeIter*         iter)
{
    GdkPixbuf* pixbuf;
    GtkTreeModel* model;
    GtkWidget* entry;

    entry = gtk_bin_get_child (GTK_BIN (location_entry));

    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (location_entry), iter);

    /* When setting the active iter (when adding or setting an item)
     * The favicon may have change, so we must update the entry favicon.
     */
    if (entry)
    {
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
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
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    if (gtk_tree_model_get_iter_first (model, iter))
    {
        tmpuri = NULL;
        do
        {
            gtk_tree_model_get (model, iter, URI_COL, &tmpuri, -1);
            found = !g_ascii_strcasecmp (uri, tmpuri);
            g_free (tmpuri);

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
    g_return_val_if_fail (GTK_IS_ICON_ENTRY (entry), NULL);

    return gtk_entry_get_text (GTK_ENTRY (entry));
}

/**
 * midori_location_entry_set_text:
 * @location_entry: a #MidoriLocationEntry
 * @text: a string
 *
 * Sets the entry text to @text.
 **/
void
midori_location_entry_set_text (MidoriLocationEntry* location_entry,
                                const gchar*         text)
{
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    g_return_if_fail (GTK_IS_ICON_ENTRY (entry));

    gtk_entry_set_text (GTK_ENTRY (entry), text);
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
    gboolean found;
    GtkTreeIter iter;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));

    found = midori_location_entry_item_iter (MIDORI_LOCATION_ENTRY (location_entry),
                                             uri,
                                             &iter);
    if(found)
        midori_location_entry_set_active_iter (location_entry, &iter);
    else
        midori_location_entry_clear (location_entry);

}

/**
 * midori_location_entry_add_item:
 * @location_entry: a #MidoriLocationEntry
 * @item: a MidoriLocationItem
 *
 * Adds @item if it is not already in the list.
 * Sets @item to be active.
 **/
void
midori_location_entry_add_item (MidoriLocationEntry*     location_entry,
                                MidoriLocationEntryItem* item)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    gboolean item_exists = FALSE;
    gchar* uri;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));
    g_return_if_fail (item->uri != NULL);
    g_return_if_fail (item->favicon != NULL);

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        uri = NULL;
        do
        {
            gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);
            if (g_ascii_strcasecmp (item->uri, uri) == 0)
            {
                item_exists = TRUE;
                g_free (uri);
                break;
            }
            g_free (uri);
        }
        while (gtk_tree_model_iter_next (model, &iter));
    }

    if (!item_exists)
        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

    midori_location_entry_set_item (model, &iter, item);
    midori_location_entry_set_active_iter (location_entry, &iter);
}

