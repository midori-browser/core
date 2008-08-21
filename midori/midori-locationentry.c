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

struct _MidoriLocationEntry
{
    GtkComboBoxEntry parent_instance;
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

    g_signal_connect (location_entry, "changed", G_CALLBACK (midori_location_entry_changed), NULL);
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
    gchar* title;
    gchar* desc;
    GdkPixbuf* icon;
    GdkPixbuf* new_icon;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));
    gtk_tree_model_get (model, iter, TITLE_COL, &title, -1);
    if (item->title)
        desc = g_markup_printf_escaped ("<b>%s</b> - %s", item->uri, item->title);
    else if (!title && !item->title)
        desc = g_markup_printf_escaped ("<b>%s</b>", item->uri);
    else
        desc = NULL;
    if (desc)
    {
        gtk_list_store_set (GTK_LIST_STORE (model), iter,
            TITLE_COL, desc, -1);
        g_free (desc);
    }

    gtk_list_store_set (GTK_LIST_STORE (model), iter,
        URI_COL, item->uri, -1);

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

    entry = gtk_bin_get_child (GTK_BIN (location_entry));

    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (location_entry), iter);

    /* When setting the active iter (when adding or setting an item)
     * The favicon may have changed, so we must update the entry favicon.
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
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));
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
 * midori_location_entry_add_item:
 * @location_entry: a #MidoriLocationEntry
 * @item: a MidoriLocationItem
 *
 * Adds @item if it is not already in the list.
 **/
void
midori_location_entry_add_item (MidoriLocationEntry*     location_entry,
                                MidoriLocationEntryItem* item)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    g_return_if_fail (MIDORI_IS_LOCATION_ENTRY (location_entry));
    g_return_if_fail (item->uri != NULL);

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (location_entry));

    if (!midori_location_entry_item_iter (location_entry, item->uri, &iter))
        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

    midori_location_entry_set_item (location_entry, &iter, item);
}

