/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-locationaction.h"

#include "gtkiconentry.h"
#include "sokoke.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#define MAX_ITEMS 25

struct _MidoriLocationAction
{
    GtkAction parent_instance;

    gchar* uri;
    gdouble progress;

    GtkTreeModel* model;
    GtkTreeModel* filter_model;
    GtkTreeModel* sort_model;
    GdkPixbuf* default_icon;
};

struct _MidoriLocationActionClass
{
    GtkActionClass parent_class;
};

G_DEFINE_TYPE (MidoriLocationAction, midori_location_action, GTK_TYPE_ACTION)

enum
{
    PROP_0,

    PROP_PROGRESS,
    PROP_SECONDARY_ICON
};

enum
{
    ACTIVE_CHANGED,
    FOCUS_OUT,
    SECONDARY_ICON_RELEASED,
    RESET_URI,
    SUBMIT_URI,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
    FAVICON_COL,
    URI_COL,
    TITLE_COL,
    VISITS_COL,
    VISIBLE_COL,
    N_COLS
};

static void
midori_location_action_finalize (GObject* object);

static void
midori_location_action_set_property (GObject*      object,
                                     guint         prop_id,
                                     const GValue* value,
                                     GParamSpec*   pspec);

static void
midori_location_action_activate (GtkAction* object);

static GtkWidget*
midori_location_action_create_tool_item (GtkAction* action);

static void
midori_location_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy);

static void
midori_location_action_disconnect_proxy (GtkAction* action,
                                         GtkWidget* proxy);

static void
midori_cclosure_marshal_VOID__STRING_BOOLEAN (GClosure*     closure,
                                              GValue*       return_value,
                                              guint         n_param_values,
                                              const GValue* param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data)
{
    typedef void(*GMarshalFunc_VOID__STRING_BOOLEAN) (gpointer      data1,
                                                      const gchar*  arg_1,
                                                      gboolean      arg_2,
                                                      gpointer      data2);
    register GMarshalFunc_VOID__STRING_BOOLEAN callback;
    register GCClosure* cc = (GCClosure*) closure;
    register gpointer data1, data2;

    g_return_if_fail (n_param_values == 3);

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__STRING_BOOLEAN) (marshal_data
        ? marshal_data : cc->callback);
    callback (data1,
              g_value_get_string (param_values + 1),
              g_value_get_boolean (param_values + 2),
              data2);
}

static void
midori_location_action_class_init (MidoriLocationActionClass* class)
{
    GObjectClass* gobject_class;
    GtkActionClass* action_class;

    signals[ACTIVE_CHANGED] = g_signal_new ("active-changed",
                                            G_TYPE_FROM_CLASS (class),
                                            (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                            0,
                                            0,
                                            NULL,
                                            g_cclosure_marshal_VOID__INT,
                                            G_TYPE_NONE, 1,
                                            G_TYPE_INT);

    signals[FOCUS_OUT] = g_signal_new ("focus-out",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE, 0);

    signals[SECONDARY_ICON_RELEASED] = g_signal_new ("secondary-icon-released",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       g_cclosure_marshal_VOID__OBJECT,
                                       G_TYPE_NONE, 1,
                                       GTK_TYPE_WIDGET);

    signals[RESET_URI] = g_signal_new ("reset-uri",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       0,
                                       NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE, 0);

    signals[SUBMIT_URI] = g_signal_new ("submit-uri",
                                        G_TYPE_FROM_CLASS (class),
                                        (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                        0,
                                        0,
                                        NULL,
                                        midori_cclosure_marshal_VOID__STRING_BOOLEAN,
                                        G_TYPE_NONE, 2,
                                        G_TYPE_STRING,
                                        G_TYPE_BOOLEAN);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_location_action_finalize;
    gobject_class->set_property = midori_location_action_set_property;

    action_class = GTK_ACTION_CLASS (class);
    action_class->activate = midori_location_action_activate;
    action_class->create_tool_item = midori_location_action_create_tool_item;
    action_class->connect_proxy = midori_location_action_connect_proxy;
    action_class->disconnect_proxy = midori_location_action_disconnect_proxy;

    g_object_class_install_property (gobject_class,
                                     PROP_PROGRESS,
                                     g_param_spec_double (
                                     "progress",
                                     "Progress",
                                     "The current progress of the action",
                                     0.0, 1.0, 0.0,
                                     G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_SECONDARY_ICON,
                                     g_param_spec_string (
                                     "secondary-icon",
                                     "Secondary",
                                     "The stock ID of the secondary icon",
                                     NULL,
                                     G_PARAM_WRITABLE));
}

static void
midori_location_action_init (MidoriLocationAction* location_action)
{
    GtkTreeModel* liststore;
    GtkTreeModel* sort_model;
    GtkTreeModel* filter_model;

    location_action->uri = NULL;
    location_action->progress = 0.0;

    liststore = (GtkTreeModel*)gtk_list_store_new (N_COLS,
        GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_INT, G_TYPE_BOOLEAN);
    sort_model = (GtkTreeModel*)gtk_tree_model_sort_new_with_model (liststore);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
        VISITS_COL, GTK_SORT_DESCENDING);
    filter_model = gtk_tree_model_filter_new (sort_model, NULL);
    gtk_tree_model_filter_set_visible_column (
        GTK_TREE_MODEL_FILTER (filter_model), VISIBLE_COL);

    location_action->model = liststore;
    location_action->filter_model = filter_model;
    location_action->sort_model = sort_model;
    location_action->default_icon = NULL;
}

static void
midori_location_action_finalize (GObject* object)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (object);

    katze_assign (location_action->uri, NULL);

    katze_object_assign (location_action->model, NULL);
    katze_object_assign (location_action->sort_model, NULL);
    katze_object_assign (location_action->filter_model, NULL);
    katze_object_assign (location_action->default_icon, NULL);

    G_OBJECT_CLASS (midori_location_action_parent_class)->finalize (object);
}

static void
midori_location_action_set_property (GObject*      object,
                                     guint         prop_id,
                                     const GValue* value,
                                     GParamSpec*   pspec)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (object);

    switch (prop_id)
    {
    case PROP_PROGRESS:
        midori_location_action_set_progress (location_action,
            g_value_get_double (value));
        break;
    case PROP_SECONDARY_ICON:
        midori_location_action_set_secondary_icon (location_action,
            g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_location_action_activate (GtkAction* action)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;

    proxies = gtk_action_get_proxies (action);
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        /* Obviously only one widget can end up with the focus.
        Yet we can't predict which one that is, can we? */
        gtk_widget_grab_focus (entry);
    }
    while ((proxies = g_slist_next (proxies)));

    if (GTK_ACTION_CLASS (midori_location_action_parent_class)->activate)
        GTK_ACTION_CLASS (midori_location_action_parent_class)->activate (action);
}

static GtkWidget*
midori_location_action_create_tool_item (GtkAction* action)
{
    GtkWidget* toolitem;
    GtkWidget* location_entry;
    GtkWidget* alignment;

    toolitem = GTK_WIDGET (gtk_tool_item_new ());
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    location_entry = midori_location_entry_new ();

    alignment = gtk_alignment_new (0, 0.5, 1, 0.1);
    gtk_container_add (GTK_CONTAINER (alignment), location_entry);
    gtk_widget_show (location_entry);
    gtk_container_add (GTK_CONTAINER (toolitem), alignment);
    gtk_widget_show (alignment);

    return toolitem;
}

static void
midori_location_action_active_changed_cb (GtkWidget* location_entry,
                                          gint       active,
                                          GtkAction* action)
{
    MidoriLocationAction* location_action;
    GtkWidget* entry;
    const gchar* text;

    location_action = MIDORI_LOCATION_ACTION (action);
    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    text = gtk_entry_get_text (GTK_ENTRY (entry));
    katze_assign (location_action->uri, g_strdup (text));

    g_signal_emit (action, signals[ACTIVE_CHANGED], 0, active);
}

static gboolean
midori_location_action_key_press_event_cb (GtkWidget*   widget,
                                           GdkEventKey* event,
                                           GtkAction*   action)
{
    const gchar* uri;

    switch (event->keyval)
    {
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_Return:
    {
        if ((uri = gtk_entry_get_text (GTK_ENTRY (widget))))
        {
            g_signal_emit (action, signals[SUBMIT_URI], 0, uri,
                (event->state & GDK_MOD1_MASK) ? TRUE : FALSE);
            return TRUE;
        }
    }
    case GDK_Escape:
    {
        g_signal_emit (action, signals[RESET_URI], 0);
        return TRUE;
    }
    }
    return FALSE;
}

static gboolean
midori_location_action_focus_out_event_cb (GtkWidget*   widget,
                                           GdkEventKey* event,
                                           GtkAction*   action)
{
    g_signal_emit (action, signals[FOCUS_OUT], 0);
    return FALSE;
}

static void
midori_location_action_icon_released_cb (GtkWidget*           widget,
                                         GtkIconEntryPosition icon_pos,
                                         gint                 button,
                                         GtkAction*           action)
{
    if (icon_pos == GTK_ICON_ENTRY_SECONDARY)
        g_signal_emit (action, signals[SECONDARY_ICON_RELEASED], 0, widget);
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
        g_object_set (renderer, "pixbuf", pixbuf, "yalign", 0.25, NULL);
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
    gchar* desc_uri;
    gchar* desc_title;
    GtkWidget* entry;
    gchar* key;
    gchar* temp;
    gchar** parts;

    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title, -1);

    desc_uri = desc_title = key = NULL;
    if (data)
    {
        entry = gtk_entry_completion_get_entry (GTK_ENTRY_COMPLETION (data));
        key = g_utf8_strdown (gtk_entry_get_text (GTK_ENTRY (entry)), -1);
    }
    if (data && uri)
    {
        temp = g_utf8_strdown (uri, -1);
        parts = g_strsplit (temp, key, 2);
        g_free (temp);
        if (parts && parts[0] && parts[1])
            desc_uri = g_markup_printf_escaped ("%s<b>%s</b>%s",
                parts[0], key, parts[1]);
        g_strfreev (parts);
    }
    if (uri && !desc_uri)
        desc_uri = g_markup_escape_text (uri, -1);
    if (data && title)
    {
        temp = g_utf8_strdown (title, -1);
        parts = g_strsplit (temp, key, 2);
        g_free (temp);
        if (parts && parts[0] && parts[1])
            desc_title = g_markup_printf_escaped ("%s<b>%s</b>%s",
                parts[0], key, parts[1]);
        g_strfreev (parts);
    }
    if (title && !desc_title)
        desc_title = g_markup_escape_text (title, -1);

    if (desc_title)
        desc = g_strdup_printf ("%s\n<span color='gray45'>%s</span>",
                                desc_title, desc_uri);
    else
        desc = g_strdup_printf ("%s", desc_uri);

    g_object_set (renderer, "markup", desc,
        "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    g_free (uri);
    g_free (title);
    g_free (key);
    g_free (desc);
    g_free (desc_uri);
    g_free (desc_title);
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
    gboolean match;
    gchar* temp;

    model = gtk_entry_completion_get_model (completion);
    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title, -1);

    match = FALSE;
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
midori_location_action_set_item (MidoriLocationAction*    location_action,
                                 GtkTreeIter*             iter,
                                 MidoriLocationEntryItem* item)
{
    GtkTreeModel* model;
    GdkPixbuf* icon;
    GdkPixbuf* new_icon;

    model = location_action->model;
    gtk_list_store_set (GTK_LIST_STORE (model), iter,
        URI_COL, item->uri, TITLE_COL, item->title, -1);

    gtk_tree_model_get (model, iter, FAVICON_COL, &icon, -1);
    if (item->favicon)
        new_icon = item->favicon;
    else if (!icon && !item->favicon)
        new_icon = location_action->default_icon;
    else
        new_icon = NULL;
    if (new_icon)
        gtk_list_store_set (GTK_LIST_STORE (model), iter,
            FAVICON_COL, new_icon, -1);
}

static gboolean
midori_location_action_child_iter_to_iter (MidoriLocationAction* location_action,
                                           GtkTreeIter*          iter,
                                           GtkTreeIter*          child_iter)
{
    GtkTreeModel* filter_model;
    GtkTreeModel* sort_model;
    GtkTreeIter sort_iter;
    GtkTreeIter* temp_iter;

    temp_iter = child_iter;

    filter_model = location_action->filter_model;
    sort_model = location_action->sort_model;
    gtk_tree_model_sort_convert_child_iter_to_iter
        (GTK_TREE_MODEL_SORT (sort_model), &sort_iter, child_iter);
    temp_iter = &sort_iter;

    return gtk_tree_model_filter_convert_child_iter_to_iter
        (GTK_TREE_MODEL_FILTER (filter_model), iter, temp_iter);
}

static void
midori_location_action_set_active_iter (MidoriLocationAction* location_action,
                                        GtkTreeIter*          iter)
{
    GdkPixbuf* pixbuf;
    GtkTreeModel* model;
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkTreeIter parent_iter;

    model = location_action->filter_model;

    /* The filter iter must be set, not the child iter,
     * but the row must first be set as visible to
     * convert to a filter iter without error.
     */
    gtk_list_store_set (GTK_LIST_STORE (model), iter, VISIBLE_COL, TRUE, -1);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        location_entry = gtk_bin_get_child (GTK_BIN (alignment));
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        if (midori_location_action_child_iter_to_iter (
            location_action, &parent_iter, iter))
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (location_entry),
                                           &parent_iter);

        /* When setting the active iter (when adding or setting an item)
         * the icon may have changed, so we must update the entry icon.
         */
        gtk_tree_model_get (model, iter, FAVICON_COL, &pixbuf, -1);

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, pixbuf);
        g_object_unref (pixbuf);
    }
    while ((proxies = g_slist_next (proxies)));
}

/**
 * midori_location_action_item_iter:
 * @location_action: a #MidoriLocationAction
 * @uri: a string
 * @iter: a GtkTreeIter
 *
 * Retrieves the iter of the item matching @uri.
 *
 * Return value: %TRUE if @uri was found, %FALSE otherwise
 **/
static gboolean
midori_location_action_item_iter (MidoriLocationAction* location_action,
                                  const gchar*          uri,
                                  GtkTreeIter*          iter)
{
    GtkTreeModel* model;
    gchar* tmpuri;
    gboolean found;

    g_return_val_if_fail (MIDORI_IS_LOCATION_ACTION (location_action), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    found = FALSE;
    model = location_action->model; // filter_model
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
 * midori_location_action_reset:
 * @location_action: a #MidoriLocationAction
 *
 * Clears the text in the entry and resets the icon.
 **/
static void
midori_location_action_reset (MidoriLocationAction* location_action)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        location_entry = gtk_bin_get_child (GTK_BIN (alignment));
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_entry_set_text (GTK_ENTRY (entry), "");
        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, GTK_STOCK_FILE);
    }
    while ((proxies = g_slist_next (proxies)));
}

/**
 * midori_location_action_set_item_from_uri:
 * @location_action: a #MidoriLocationAction
 * @uri: a string
 *
 * Finds the item from the list matching @uri
 * and sets it as the active item.
 * If @uri is not found it clears the active item.
 **/
static void
midori_location_action_set_item_from_uri (MidoriLocationAction* location_action,
                                          const gchar*          uri)
{
    GtkTreeIter iter;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    if (midori_location_action_item_iter (location_action, uri, &iter))
        midori_location_action_set_active_iter (location_action, &iter);
    else
        midori_location_action_reset (location_action);

}

static gboolean
midori_location_entry_completion_selected (GtkEntryCompletion*   completion,
                                           GtkTreeModel*         model,
                                           GtkTreeIter*          iter,
                                           MidoriLocationAction* location_action)
{
    gchar* uri;

    gtk_tree_model_get (model, iter, URI_COL, &uri, -1);
    midori_location_action_set_item_from_uri (location_action, uri);
    g_free (uri);

    return FALSE;
}

static void
midori_location_action_completion_init (MidoriLocationAction* location_action,
                                        GtkWidget*            location_entry)
{
    GtkWidget* entry;
    GtkEntryCompletion* completion;
    GtkCellRenderer* renderer;

    entry = gtk_bin_get_child (GTK_BIN (location_entry));
    completion = gtk_entry_completion_new ();

    gtk_entry_completion_set_model (completion, location_action->sort_model);
    gtk_entry_completion_set_text_column (completion, URI_COL);
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
                                        completion, NULL);
    gtk_entry_completion_set_match_func (completion,
        midori_location_entry_completion_match_cb, NULL, NULL);

    gtk_entry_set_completion (GTK_ENTRY (entry), completion);
    g_signal_connect (completion, "match-selected",
        G_CALLBACK (midori_location_entry_completion_selected), location_entry);

    g_object_unref (completion);
}

static void
midori_location_action_entry_changed_cb (GtkComboBox*          combo_box,
                                         MidoriLocationAction* location_action)
{
    GtkTreeIter iter;
    GtkIconEntry* entry;
    GtkTreeModel* model;
    GdkPixbuf* pixbuf;

    if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
        if ((entry = GTK_ICON_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box)))))
        {
            pixbuf = NULL;

            model = location_action->filter_model;
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
midori_location_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy)
{
    GtkWidget* alignment;
    GtkWidget* entry;
    MidoriLocationAction* location_action;
    GtkCellRenderer* renderer;

    GTK_ACTION_CLASS (midori_location_action_parent_class)->connect_proxy (
        action, proxy);

    if (GTK_IS_TOOL_ITEM (proxy))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxy));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        location_action = MIDORI_LOCATION_ACTION (action);
        midori_location_entry_set_progress (MIDORI_LOCATION_ENTRY (entry),
            MIDORI_LOCATION_ACTION (action)->progress);
        gtk_combo_box_set_model (GTK_COMBO_BOX (entry),
            MIDORI_LOCATION_ACTION (action)->filter_model);
        gtk_combo_box_entry_set_text_column (
            GTK_COMBO_BOX_ENTRY (entry), URI_COL);
        gtk_cell_layout_clear (GTK_CELL_LAYOUT (entry));

        /* Setup the renderer for the favicon */
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), renderer, FALSE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (entry),
            renderer, midori_location_entry_render_pixbuf_cb, NULL, NULL);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (entry),
            renderer, midori_location_entry_render_text_cb, NULL, NULL);

        gtk_combo_box_set_active (GTK_COMBO_BOX (entry), -1);
        midori_location_action_completion_init (location_action, entry);
        g_signal_connect (entry, "changed",
            G_CALLBACK (midori_location_action_entry_changed_cb), action);

        g_signal_connect (location_action, "active-changed",
            G_CALLBACK (midori_location_action_active_changed_cb), action);
        g_object_connect (gtk_bin_get_child (GTK_BIN (entry)),
                      "signal::key-press-event",
                      midori_location_action_key_press_event_cb, action,
                      "signal::focus-out-event",
                      midori_location_action_focus_out_event_cb, action,
                      "signal::icon-released",
                      midori_location_action_icon_released_cb, action,
                      NULL);
    }
}

static void
midori_location_action_disconnect_proxy (GtkAction* action,
                                         GtkWidget* proxy)
{
    /* FIXME: This is wrong */
    g_signal_handlers_disconnect_by_func (proxy,
        G_CALLBACK (gtk_action_activate), action);

    GTK_ACTION_CLASS (midori_location_action_parent_class)->disconnect_proxy
        (action, proxy);
}

const gchar*
midori_location_action_get_uri (MidoriLocationAction* location_action)
{
    g_return_val_if_fail (MIDORI_IS_LOCATION_ACTION (location_action), NULL);

    return location_action->uri;
}

/**
 * midori_location_action_set_text:
 * @location_action: a #MidoriLocationAction
 * @text: a string
 *
 * Sets the entry text to @text and, if applicable, updates the icon.
 **/
static void
midori_location_action_set_text (MidoriLocationAction* location_action,
                                 const gchar*          text)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkTreeIter iter;
    GtkTreeModel* model;
    GdkPixbuf* icon;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        location_entry = gtk_bin_get_child (GTK_BIN (alignment));
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_entry_set_text (GTK_ENTRY (entry), text);
        if (midori_location_action_item_iter (location_action, text, &iter))
        {
            model = location_action->model; // filter_model
            gtk_tree_model_get (model, &iter, FAVICON_COL, &icon, -1);
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, icon);
        }
        /* FIXME: Due to a bug in GtkIconEntry we can't reset the icon
        else
            gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, location_action->default_icon);*/
    }
    while ((proxies = g_slist_next (proxies)));
}

void
midori_location_action_set_uri (MidoriLocationAction* location_action,
                                const gchar*          uri)
{
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);

    katze_assign (location_action->uri, g_strdup (uri));

    midori_location_action_set_text (location_action, uri);
}

/**
 * midori_location_action_prepend_item:
 * @location_action: a #MidoriLocationAction
 * @item: a MidoriLocationItem
 *
 * Prepends @item if it is not already in the list.
 * If the item already exists, it is moved before the first item.
 * If the maximum is reached, the last item is removed.
 **/
static void
midori_location_action_prepend_item (MidoriLocationAction*    location_action,
                                     MidoriLocationEntryItem* item)
{
    GtkTreeModel* filter_model;
    GtkTreeModel* model;
    GtkTreeIter iter;
    GtkTreeIter index;
    gint n;
    gint visits = 0;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (item->uri != NULL);

    filter_model = location_action->filter_model;
    model = location_action->model;

    if (midori_location_action_item_iter (location_action, item->uri, &iter))
    {
        gtk_tree_model_get_iter_first (model, &index);
        gtk_tree_model_get (model, &iter, VISITS_COL, &visits, -1);
        gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &index);
    }
    else
        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

    n = gtk_tree_model_iter_n_children (filter_model, NULL);
    if (n > MAX_ITEMS)
    {
        gtk_tree_model_iter_nth_child (model, &index, NULL, n - 1);
        gtk_list_store_set (GTK_LIST_STORE (model),
                            &index, VISIBLE_COL, FALSE, -1);
    }

    /* Only increment the visits when we add the uri */
    if (!item->title && !item->favicon)
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, VISITS_COL, ++visits,
                            VISIBLE_COL, TRUE, -1);
    midori_location_action_set_item (location_action, &iter, item);
}

/**
 * midori_location_entry_append_item:
 * @location_entry: a #MidoriLocationEntry
 * @item: a MidoriLocationItem
 *
 * Appends @item if it is not already in the list.
 * @item is not added if the maximum is reached.
 **/
static void
midori_location_action_append_item (MidoriLocationAction*    location_action,
                                    MidoriLocationEntryItem* item)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    gint n;
    gint visits = 0;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (item->uri != NULL);

    model = location_action->model;

    if (!midori_location_action_item_iter (location_action, item->uri, &iter))
    {
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);

        n = gtk_tree_model_iter_n_children (model, NULL);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, VISIBLE_COL,
                            (n <= MAX_ITEMS), -1);
    }
    else
        gtk_tree_model_get (model, &iter, VISITS_COL, &visits, -1);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, VISITS_COL, ++visits, -1);
    midori_location_action_set_item (location_action, &iter, item);
}

void
midori_location_action_add_uri (MidoriLocationAction* location_action,
                                const gchar*          uri)
{
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);

    katze_assign (location_action->uri, g_strdup (uri));

    item.favicon = NULL;
    item.uri = uri;
    item.title = NULL;
    midori_location_action_prepend_item (location_action, &item);
}

void
midori_location_action_add_item (MidoriLocationAction* location_action,
                                 const gchar*          uri,
                                 GdkPixbuf*            icon,
                                 const gchar*          title)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);
    g_return_if_fail (title != NULL);
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));

    katze_assign (location_action->uri, g_strdup (uri));

    item.favicon = icon;
    item.uri = uri;
    item.title = title;
    midori_location_action_append_item (location_action, &item);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data) &&
        !g_strcmp0 (location_action->uri, uri))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        location_entry = gtk_bin_get_child (GTK_BIN (alignment));
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
    while ((proxies = g_slist_next (proxies)));
}

void
midori_location_action_set_icon_for_uri (MidoriLocationAction* location_action,
                                         GdkPixbuf*            icon,
                                         const gchar*          uri)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* location_entry;
    GtkWidget* entry;
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));
    g_return_if_fail (uri != NULL);

    item.favicon = icon;
    item.uri = uri;
    item.title = NULL;
    midori_location_action_prepend_item (location_action, &item);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data) &&
        !g_strcmp0 (location_action->uri, uri))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        location_entry = gtk_bin_get_child (GTK_BIN (alignment));
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
    while ((proxies = g_slist_next (proxies)));
}

void
midori_location_action_set_title_for_uri (MidoriLocationAction* location_action,
                                          const gchar*          title,
                                          const gchar*          uri)
{
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (title != NULL);
    g_return_if_fail (uri != NULL);

    item.favicon = NULL;
    item.uri = uri;
    item.title = title;
    midori_location_action_prepend_item (location_action, &item);
}

gdouble
midori_location_action_get_progress (MidoriLocationAction* location_action)
{
    g_return_val_if_fail (MIDORI_IS_LOCATION_ACTION (location_action), 0.0);

    return location_action->progress;
}

void
midori_location_action_set_progress (MidoriLocationAction* location_action,
                                     gdouble               progress)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    location_action->progress = CLAMP (progress, 0.0, 1.0);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        midori_location_entry_set_progress (MIDORI_LOCATION_ENTRY (entry),
                                            location_action->progress);
    }
    while ((proxies = g_slist_next (proxies)));
}

void
midori_location_action_set_secondary_icon (MidoriLocationAction* location_action,
                                           const gchar*          stock_id)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;
    GtkWidget* child;
    GtkStockItem stock_item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!stock_id || gtk_stock_lookup (stock_id, &stock_item));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));
        child = gtk_bin_get_child (GTK_BIN (entry));

        if (stock_id)
            gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (child),
                GTK_ICON_ENTRY_SECONDARY, stock_id);
        else
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (child),
                GTK_ICON_ENTRY_SECONDARY, NULL);
    }
    while ((proxies = g_slist_next (proxies)));
}

/**
 * midori_location_entry_set_item_from_uri:
 * @location_entry: a #MidoriLocationEntry
 * @uri: a string
 *
 * Finds the item from the list matching @uri
 * and removes it if it is the last instance.
 **/
void
midori_location_action_delete_item_from_uri (MidoriLocationAction* location_action,
                                             const gchar*          uri)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    gint visits;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);

    model = location_action->model;
    if (midori_location_action_item_iter (location_action, uri, &iter))
    {
        gtk_tree_model_get (model, &iter, VISITS_COL, &visits, -1);
        if (visits > 1)
            gtk_list_store_set (GTK_LIST_STORE (model),
                &iter, VISITS_COL, --visits, -1);
        else
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}

void
midori_location_action_clear (MidoriLocationAction* location_action)
{
    GSList* proxies;
    GtkWidget* alignment;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    if (!proxies)
        return;

    do
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        alignment = gtk_bin_get_child (GTK_BIN (proxies->data));
        entry = gtk_bin_get_child (GTK_BIN (alignment));

        gtk_list_store_clear (GTK_LIST_STORE (location_action->filter_model));
    }
    while ((proxies = g_slist_next (proxies)));
}
