/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-locationaction.h"

#include "gtkiconentry.h"
#include "marshal.h"
#include "sokoke.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#define MAX_ITEMS 25

struct _MidoriLocationAction
{
    GtkAction parent_instance;

    gchar* uri;
    KatzeArray* search_engines;
    gdouble progress;
    gchar* secondary_icon;

    GtkTreeModel* model;
    GtkTreeModel* filter_model;
    GdkPixbuf* default_icon;
    GHashTable* items;
    KatzeNet* net;
    KatzeArray* history;
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
    PROP_SECONDARY_ICON,
    PROP_HISTORY
};

enum
{
    ACTIVE_CHANGED,
    FOCUS_IN,
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
    YALIGN_COL,
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
midori_location_action_get_property (GObject*    object,
                                     guint       prop_id,
                                     GValue*     value,
                                     GParamSpec* pspec);

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
midori_location_action_completion_init (MidoriLocationAction* location_action,
                                        GtkEntry*             entry);

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

    /**
     * MidoriLocationAction:focus-in:
     *
     * The focus-in signal is emitted when the entry obtains the focus.
     *
     * Since 0.1.8
     */
    signals[FOCUS_IN] = g_signal_new ("focus-in",
                                      G_TYPE_FROM_CLASS (class),
                                      (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                      0,
                                      0,
                                      NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE, 0);

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
    gobject_class->get_property = midori_location_action_get_property;

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
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_SECONDARY_ICON,
                                     g_param_spec_string (
                                     "secondary-icon",
                                     "Secondary",
                                     "The stock ID of the secondary icon",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * MidoriLocationAction:history:
     *
     * The list of history items.
     *
     * This is actually a reference to a history instance.
     *
     * Since 0.1.8
     */
    g_object_class_install_property (gobject_class,
                                     PROP_HISTORY,
                                     g_param_spec_object (
                                     "history",
                                     "History",
                                     "The list of history items",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* Allow this to be used in tests, it's otherwise private */
/*static*/ GtkWidget*
midori_location_action_entry_for_proxy (GtkWidget* proxy)
{
    GtkWidget* alignment = gtk_bin_get_child (GTK_BIN (proxy));
    GtkWidget* entry = gtk_bin_get_child (GTK_BIN (alignment));
    return entry;
}

static void
midori_location_action_set_model (MidoriLocationAction* location_action,
                                  GtkTreeModel*         model)
{
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        g_object_set (location_entry, "model", model, NULL);
        midori_location_action_completion_init (location_action, GTK_ENTRY (entry));
    }
}

static gboolean
midori_location_action_is_frozen (MidoriLocationAction* location_action)
{
    return location_action->filter_model == NULL;
}

/**
 * midori_location_action_freeze:
 * @location_action: a #MidoriLocationAction
 *
 * Freezes the action, which essentially means disconnecting models
 * from proxies and skipping updates of the current state.
 *
 * Freezing is recommended if you need to insert a large amount
 * of items at once, which is faster in the frozen state.
 *
 * Use midori_location_action_thaw() to go back to normal operation.
 *
 * Since: 0.1.2
 **/
void
midori_location_action_freeze (MidoriLocationAction* location_action)
{
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!midori_location_action_is_frozen (location_action));

    katze_object_assign (location_action->filter_model, NULL);
    midori_location_action_set_model (location_action, NULL);

    if (location_action->items)
        g_hash_table_remove_all (location_action->items);
}

/**
 * midori_location_action_thaw:
 * @location_action: a #MidoriLocationAction
 *
 * Thaws the action, ie. after freezing it.
 *
 * Since: 0.1.2
 **/
void
midori_location_action_thaw (MidoriLocationAction* location_action)
{
    GtkTreeModel* filter_model;
    GtkTreeIter iter;
    gint i;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (midori_location_action_is_frozen (location_action));

    gtk_tree_sortable_set_sort_column_id (
        GTK_TREE_SORTABLE (location_action->model),
        VISITS_COL, GTK_SORT_DESCENDING);

    filter_model = gtk_tree_model_filter_new (location_action->model, NULL);
    gtk_tree_model_filter_set_visible_column (
        GTK_TREE_MODEL_FILTER (filter_model), VISIBLE_COL);

    location_action->filter_model = filter_model;
    midori_location_action_set_model (location_action, location_action->model);

    i = MAX_ITEMS;
    while (gtk_tree_model_iter_nth_child (location_action->model, &iter, NULL, i++))
    {
        gtk_list_store_set (GTK_LIST_STORE (location_action->model),
                    &iter, VISIBLE_COL, FALSE, -1);
    }
}

static GtkTreeModel*
midori_location_action_create_model (void)
{
    GtkTreeModel* model = (GtkTreeModel*) gtk_list_store_new (N_COLS,
        GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_FLOAT);
    return model;
}

static void
midori_location_action_init (MidoriLocationAction* location_action)
{
    location_action->uri = NULL;
    location_action->search_engines = NULL;
    location_action->progress = 0.0;
    location_action->secondary_icon = NULL;
    location_action->default_icon = NULL;

    location_action->model = midori_location_action_create_model ();

    location_action->filter_model = NULL;
    midori_location_action_thaw (location_action);

    location_action->items = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free, g_free);
    location_action->net = katze_net_new ();
    location_action->history = NULL;
}

static void
midori_location_action_finalize (GObject* object)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (object);

    katze_assign (location_action->uri, NULL);
    katze_assign (location_action->search_engines, NULL);

    katze_object_assign (location_action->model, NULL);
    katze_object_assign (location_action->filter_model, NULL);
    katze_object_assign (location_action->default_icon, NULL);

    g_hash_table_destroy (location_action->items);
    katze_object_assign (location_action->net, NULL);
    katze_object_assign (location_action->history, NULL);

    G_OBJECT_CLASS (midori_location_action_parent_class)->finalize (object);
}

static void
midori_location_action_history_remove_item_cb (KatzeArray*           folder,
                                               KatzeItem*            item,
                                               MidoriLocationAction* action)
{
    midori_location_action_delete_item_from_uri (action, katze_item_get_uri (item));
    if (KATZE_IS_ARRAY (item))
        g_signal_handlers_disconnect_by_func (item,
            midori_location_action_history_remove_item_cb, action);
}

static void
midori_location_action_insert_history_item (MidoriLocationAction* action,
                                            KatzeItem*            item)
{
    KatzeItem* child;
    guint i;
    const gchar* uri;
    GdkPixbuf* pixbuf = NULL;

    if (KATZE_IS_ARRAY (item))
    {
        for (i = katze_array_get_length (KATZE_ARRAY (item)); i > 0; i--)
        {
            child = katze_array_get_nth_item (KATZE_ARRAY (item), i - 1);
            midori_location_action_insert_history_item (action, child);
        }
    }
    else
    {
        uri = katze_item_get_uri (item);
        pixbuf = katze_net_load_icon (action->net, katze_item_get_uri (item),
                                      NULL, NULL, NULL);
        if (!pixbuf)
            pixbuf = action->default_icon;
        midori_location_action_add_item (action, uri,
            pixbuf, katze_item_get_name (item));
        g_object_unref (pixbuf);
        g_signal_connect (katze_item_get_parent (item), "remove-item",
            G_CALLBACK (midori_location_action_history_remove_item_cb), action);
    }
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
    case PROP_HISTORY:
    {
        KatzeArray* history;
        GtkTreeModel* model;

        history = g_value_dup_object (value);
        katze_assign (location_action->history, g_object_ref (history));
        model = g_object_get_data (G_OBJECT (history), "midori-location-model");
        if (model != NULL)
        {
            katze_object_assign (location_action->model, g_object_ref (model));
            location_action->filter_model = NULL;
            midori_location_action_thaw (location_action);
        }
        else
        {
            g_object_unref (location_action->model);
            location_action->model = midori_location_action_create_model ();
            midori_location_action_freeze (location_action);
            /* FIXME: MidoriBrowser is essentially making up for the lack
                      of synchronicity of newly added items. */
            midori_location_action_insert_history_item (location_action,
                KATZE_ITEM (g_value_get_object (value)));
            midori_location_action_thaw (location_action);
            g_object_set_data (G_OBJECT (history),
                "midori-location-model", location_action->model);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_location_action_get_property (GObject*    object,
                                     guint       prop_id,
                                     GValue*     value,
                                     GParamSpec* pspec)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (object);

    switch (prop_id)
    {
    case PROP_PROGRESS:
        g_value_set_double (value, location_action->progress);
        break;
    case PROP_SECONDARY_ICON:
        g_value_set_string (value, location_action->secondary_icon);
        break;
    case PROP_HISTORY:
        g_value_set_object (value, location_action->history);
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
    GtkWidget* entry;

    proxies = gtk_action_get_proxies (action);

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        entry = midori_location_action_entry_for_proxy (proxies->data);

        /* Obviously only one widget can end up with the focus.
        Yet we can't predict which one that is, can we? */
        gtk_widget_grab_focus (entry);
    }

    if (GTK_ACTION_CLASS (midori_location_action_parent_class)->activate)
        GTK_ACTION_CLASS (midori_location_action_parent_class)->activate (action);
}

static GtkWidget*
midori_location_action_create_tool_item (GtkAction* action)
{
    GtkWidget* toolitem;
    GtkWidget* alignment;
    GtkWidget* location_entry;

    toolitem = GTK_WIDGET (gtk_tool_item_new ());
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);

    alignment = gtk_alignment_new (0.0f, 0.5f, 1.0f, 0.1f);
    gtk_widget_show (alignment);
    gtk_container_add (GTK_CONTAINER (toolitem), alignment);
    location_entry = midori_location_entry_new ();
    gtk_widget_show (location_entry);
    gtk_container_add (GTK_CONTAINER (alignment), location_entry);

    return toolitem;
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
        if ((uri = gtk_entry_get_text (GTK_ENTRY (widget))) && *uri)
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
midori_location_action_focus_in_event_cb (GtkWidget*   widget,
                                           GdkEventKey* event,
                                           GtkAction*   action)
{
    g_signal_emit (action, signals[FOCUS_IN], 0);
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
    gchar* start;
    gchar* skey;
    gchar* temp;
    gchar** parts;
    size_t len;

    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title, -1);

    desc_uri = desc_title = key = NULL;
    if (G_LIKELY (data))
    {
        entry = gtk_entry_completion_get_entry (GTK_ENTRY_COMPLETION (data));
        key = title ? g_utf8_strdown (gtk_entry_get_text (GTK_ENTRY (entry)), -1)
            : g_ascii_strdown (gtk_entry_get_text (GTK_ENTRY (entry)), -1);
        len = 0;
    }
    if (G_LIKELY (data && uri))
    {
        temp = g_ascii_strdown (uri, -1);
        if ((start = strstr (temp, key)))
        {
            len = strlen (key);
            skey = g_malloc0 (len + 1);
            strncpy (skey, uri + (start - temp), len);
            if (skey && *skey && (parts = g_strsplit (uri, skey, 2)))
            {
                if (parts[0] && parts[1])
                {
                    desc_uri = g_markup_printf_escaped ("%s<b>%s</b>%s",
                        parts[0], skey, parts[1]);
                    g_strfreev (parts);
                }
            }
            g_free (skey);
        }
        g_free (temp);
    }
    if (uri && !desc_uri)
        desc_uri = g_markup_escape_text (uri, -1);
    if (G_LIKELY (data && title))
    {
        temp = g_utf8_strdown (title, -1);
        if ((start = strstr (temp, key)))
        {
            if (!len)
                len = strlen (key);
            skey = g_malloc0 (len + 1);
            strncpy (skey, title + (start - temp), len);
            parts = g_strsplit (title, skey, 2);
            if (parts && parts[0] && parts[1])
                desc_title = g_markup_printf_escaped ("%s<b>%s</b>%s",
                    parts[0], skey, parts[1]);
            g_strfreev (parts);
            g_free (skey);
        }
        g_free (temp);
    }
    if (title && !desc_title)
        desc_title = g_markup_escape_text (title, -1);

    if (desc_title)
    {
        desc = g_strdup_printf ("%s\n<span color='gray45'>%s</span>",
                                desc_title, desc_uri);
        g_free (desc_uri);
        g_free (desc_title);
    }
    else
        desc = desc_uri;

    g_object_set (renderer, "markup", desc,
        "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    g_free (uri);
    g_free (title);
    g_free (key);
    g_free (desc);
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
    if (G_LIKELY (uri))
    {
        temp = g_utf8_casefold (uri, -1);
        match = (strstr (temp, key) != NULL);
        g_free (temp);
        g_free (uri);

        if (!match && G_LIKELY (title))
        {
            temp = g_utf8_casefold (title, -1);
            match = (strstr (temp, key) != NULL);
            g_free (temp);
            g_free (title);
        }
    }

    return match;
}

/**
 * midori_location_action_iter_lookup:
 * @location_action: a #MidoriLocationAction
 * @uri: a string
 * @iter: a #GtkTreeIter
 *
 * Retrieves the iter of the item matching @uri if it was
 * inserted with midori_location_action_iter_insert().
 *
 * Return value: %TRUE if @uri was found, %FALSE otherwise
 **/
static gboolean
midori_location_action_iter_lookup (MidoriLocationAction* location_action,
                                    const gchar*          uri,
                                    GtkTreeIter*          iter)
{
    GtkTreeModel* model;
    gchar* path;

    model = location_action->model;

    if (midori_location_action_is_frozen (location_action))
    {
        gboolean found = FALSE;
        if ((path = g_hash_table_lookup (location_action->items, uri)))
            if (!(found = gtk_tree_model_get_iter_from_string (model, iter, path)))
                g_hash_table_remove (location_action->items, uri);
        return found;
    }

    if (gtk_tree_model_get_iter_first (model, iter))
    {
        gchar* tmp_uri = NULL;
        do
        {
            gint cmp;
            gtk_tree_model_get (model, iter, URI_COL, &tmp_uri, -1);
            cmp = strcmp (uri, tmp_uri);
            g_free (tmp_uri);
            if (!cmp)
                return TRUE;
        }
        while (gtk_tree_model_iter_next (model, iter));
    }

    return FALSE;
}

/**
 * midori_location_action_iter_insert:
 * @location_action: a #MidoriLocationAction
 * @uri: a string
 * @iter: a #GtkTreeIter
 * @position: position to insert the new row
 *
 * Creates a new row for @uri if it doesn't exist, or sets @iter
 * to the existing iter for @uri.
 *
 * Return value: %TRUE if @uri was found, %FALSE otherwise
 **/
static gboolean
midori_location_action_iter_insert (MidoriLocationAction* location_action,
                                    const gchar*          uri,
                                    GtkTreeIter*          iter,
                                    gint                  position)
{
    if (!midori_location_action_iter_lookup (location_action, uri, iter))
    {
        GtkTreeModel* model;

        model = location_action->model;
        gtk_list_store_insert (GTK_LIST_STORE (model), iter, position);
        if (midori_location_action_is_frozen (location_action))
        {
            gchar* new_uri = g_strdup (uri);
            gchar* path = gtk_tree_model_get_string_from_iter (model,  iter);
            g_hash_table_insert (location_action->items, new_uri, path);
        }
        return FALSE;
    }

    return TRUE;
}

static void
midori_location_action_set_item (MidoriLocationAction*    location_action,
                                 MidoriLocationEntryItem* item,
                                 gboolean                 increment_visits,
                                 gboolean                 filter)
{
    GtkTreeModel* model;
    GtkTreeModel* filter_model;
    GtkTreeIter iter;
    GdkPixbuf* icon;
    GdkPixbuf* new_icon;
    gint visits = 0;

    model = location_action->model;

    if (midori_location_action_iter_insert (location_action,
        item->uri, &iter, G_MAXINT))
        gtk_tree_model_get (model, &iter, VISITS_COL, &visits, -1);

    if (increment_visits)
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            VISITS_COL, ++visits, VISIBLE_COL, TRUE, -1);

    /* Ensure we keep the title if we added the same URI with a title before */
    if (!item->title)
        gtk_tree_model_get (model, &iter, TITLE_COL, &item->title, -1);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
        URI_COL, item->uri, TITLE_COL, item->title, YALIGN_COL, 0.25, -1);

    gtk_tree_model_get (model, &iter, FAVICON_COL, &icon, -1);
    if (item->favicon)
        new_icon = item->favicon;
    else if (!icon)
        new_icon = location_action->default_icon;
    else
        new_icon = NULL;
    if (new_icon)
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            FAVICON_COL, new_icon, -1);

    if (filter)
    {
        filter_model = location_action->filter_model;

        if (filter_model)
        {
            GtkTreeIter idx;
            gint n;

            n = gtk_tree_model_iter_n_children (filter_model, NULL);
            if (n > MAX_ITEMS)
            {
                gtk_tree_model_iter_nth_child (filter_model, &idx, NULL, n - 1);
                gtk_tree_model_filter_convert_iter_to_child_iter (
                    GTK_TREE_MODEL_FILTER (filter_model), &iter, &idx);
                gtk_list_store_set (GTK_LIST_STORE (model),
                    &iter, VISIBLE_COL, FALSE, -1);
            }
        }
    }
}

static gboolean
midori_location_entry_match_selected_cb (GtkEntryCompletion*   completion,
                                         GtkTreeModel*         model,
                                         GtkTreeIter*          iter,
                                         MidoriLocationAction* location_action)
{
    gchar* uri;
    gtk_tree_model_get (model, iter, URI_COL, &uri, -1);

    midori_location_action_set_uri (location_action, uri);
    g_signal_emit (location_action, signals[SUBMIT_URI], 0, uri, FALSE);
    g_free (uri);

    return FALSE;
}

static void
midori_location_entry_action_activated_cb (GtkEntryCompletion*   completion,
                                           gint                  action,
                                           MidoriLocationAction* location_action)
{
    if (location_action->search_engines)
    {
        KatzeItem* item = katze_array_get_nth_item (
            location_action->search_engines, action);
        GtkWidget* entry = gtk_entry_completion_get_entry (completion);
        const gchar* keywords = gtk_entry_get_text (GTK_ENTRY (entry));
        const gchar* uri = katze_item_get_uri (item);
        gchar* search;
        if (!item)
            return;
        search = sokoke_search_uri (uri, keywords);
        midori_location_action_set_uri (location_action, search);
        g_signal_emit (location_action, signals[SUBMIT_URI], 0, search, FALSE);
        g_free (search);
    }
}

static void
midori_location_action_add_actions (GtkEntryCompletion* completion,
                                    KatzeArray*         search_engines)
{
    guint i;
    KatzeItem* item;

    if (!search_engines)
        return;

    i = 0;
    while ((item = katze_array_get_nth_item (search_engines, i)))
    {
        gchar* text = g_strdup_printf (_("Search with %s"),
            katze_item_get_name (item));
        gtk_entry_completion_insert_action_text (completion, i++, text);
        g_free (text);
    }
}

static void
midori_location_action_completion_init (MidoriLocationAction* location_action,
                                        GtkEntry*             entry)
{
    GtkEntryCompletion* completion;
    GtkCellRenderer* renderer;

    if ((completion = gtk_entry_get_completion (entry)))
    {
        gtk_entry_completion_set_model (completion,
            midori_location_action_is_frozen (location_action)
            ? NULL : location_action->filter_model);
        return;
    }

    completion = gtk_entry_completion_new ();
    gtk_entry_set_completion (entry, completion);
    g_object_unref (completion);
    gtk_entry_completion_set_model (completion,
        midori_location_action_is_frozen (location_action)
        ? NULL : location_action->filter_model);

    gtk_entry_completion_set_text_column (completion, URI_COL);
    #if GTK_CHECK_VERSION (2, 12, 0)
    gtk_entry_completion_set_inline_selection (completion, TRUE);
    #endif
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (completion));

    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (completion), renderer,
        "pixbuf", FAVICON_COL, "yalign", YALIGN_COL, NULL);
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_renderer_set_fixed_size (renderer, 1, -1);
    gtk_cell_renderer_text_set_fixed_height_from_font (
        GTK_CELL_RENDERER_TEXT (renderer), 2);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, TRUE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion), renderer,
                                        midori_location_entry_render_text_cb,
                                        completion, NULL);
    gtk_entry_completion_set_match_func (completion,
        midori_location_entry_completion_match_cb, NULL, NULL);


    g_signal_connect (completion, "match-selected",
        G_CALLBACK (midori_location_entry_match_selected_cb), location_action);

    midori_location_action_add_actions (completion,
                                        location_action->search_engines);
    g_signal_connect (completion, "action-activated",
        G_CALLBACK (midori_location_entry_action_activated_cb), location_action);
}

static void
midori_location_action_entry_changed_cb (GtkComboBox*          combo_box,
                                         MidoriLocationAction* location_action)
{
    GtkTreeIter iter;
    GtkIconEntry* entry;
    GtkTreeModel* model;
    GdkPixbuf* pixbuf;
    gchar* uri;

    if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
        if ((entry = GTK_ICON_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box)))))
        {
            pixbuf = NULL;

            model = location_action->filter_model;
            gtk_tree_model_get (model, &iter, FAVICON_COL, &pixbuf,
                URI_COL, &uri, -1);

            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                                                 GTK_ICON_ENTRY_PRIMARY, pixbuf);
            g_object_unref (pixbuf);
            katze_assign (location_action->uri, uri);

            g_signal_emit (location_action, signals[ACTIVE_CHANGED], 0,
                           gtk_combo_box_get_active (combo_box));
        }
    }
}

static void
midori_location_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy)
{
    GtkWidget* entry;
    MidoriLocationAction* location_action;
    GtkCellRenderer* renderer;

    GTK_ACTION_CLASS (midori_location_action_parent_class)->connect_proxy (
        action, proxy);

    location_action = MIDORI_LOCATION_ACTION (action);
    katze_object_assign (location_action->default_icon,
                         gtk_widget_render_icon (proxy, GTK_STOCK_FILE,
                                                 GTK_ICON_SIZE_MENU, NULL));

    if (GTK_IS_TOOL_ITEM (proxy))
    {
        entry = midori_location_action_entry_for_proxy (proxy);

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
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (entry), renderer,
            "pixbuf", FAVICON_COL, "yalign", YALIGN_COL, NULL);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (entry),
            renderer, midori_location_entry_render_text_cb, NULL, NULL);

        gtk_combo_box_set_active (GTK_COMBO_BOX (entry), -1);
        midori_location_action_completion_init (location_action,
            GTK_ENTRY (gtk_bin_get_child (GTK_BIN (entry))));
        g_signal_connect (entry, "changed",
            G_CALLBACK (midori_location_action_entry_changed_cb), action);

        g_object_connect (gtk_bin_get_child (GTK_BIN (entry)),
                      "signal::key-press-event",
                      midori_location_action_key_press_event_cb, action,
                      "signal::focus-in-event",
                      midori_location_action_focus_in_event_cb, action,
                      "signal::focus-out-event",
                      midori_location_action_focus_out_event_cb, action,
                      "signal::icon-release",
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
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkTreeIter iter;
    GdkPixbuf* icon;

    if (!(proxies = gtk_action_get_proxies (GTK_ACTION (location_action))))
        return;

    if (midori_location_action_iter_lookup (location_action, text, &iter))
        gtk_tree_model_get (location_action->model,
                            &iter, FAVICON_COL, &icon, -1);
    else
        icon = g_object_ref (location_action->default_icon);

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_entry_set_text (GTK_ENTRY (entry), text);
        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }

    if (icon)
        g_object_unref (icon);
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
 * midori_location_action_set_icon:
 * @location_action: a #MidoriLocationAction
 * @icon: a #GdkPixbuf or %NULL
 *
 * Sets the icon shown on the left hand side.
 *
 * Note: Since 0.1.8 %NULL can be passed to indicate that the
 *     visible URI refers to a target, not the current location.
 **/
void
midori_location_action_set_icon (MidoriLocationAction* location_action,
                                 GdkPixbuf*            icon)
{
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        if (icon)
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, icon);
        else
            gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, GTK_STOCK_JUMP_TO);
    }
}

void
midori_location_action_add_uri (MidoriLocationAction* location_action,
                                const gchar*          uri)
{
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);

    if (midori_location_action_is_frozen (location_action))
        return;

    item.favicon = NULL;
    item.uri = uri;
    item.title = NULL;
    midori_location_action_set_item (location_action, &item, TRUE, TRUE);

    katze_assign (location_action->uri, g_strdup (uri));
}

void
midori_location_action_add_item (MidoriLocationAction* location_action,
                                 const gchar*          uri,
                                 GdkPixbuf*            icon,
                                 const gchar*          title)
{
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);
    g_return_if_fail (title != NULL);
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));

    item.favicon = icon;
    item.uri = uri;
    item.title = title;
    midori_location_action_set_item (location_action, &item, TRUE, FALSE);

    if (midori_location_action_is_frozen (location_action))
        return;

    katze_assign (location_action->uri, g_strdup (uri));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data) &&
        !strcmp (location_action->uri, uri))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
}

void
midori_location_action_set_icon_for_uri (MidoriLocationAction* location_action,
                                         GdkPixbuf*            icon,
                                         const gchar*          uri)
{
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;
    MidoriLocationEntryItem item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));
    g_return_if_fail (uri != NULL);

    item.favicon = icon;
    item.uri = uri;
    item.title = NULL;
    midori_location_action_set_item (location_action, &item, FALSE, TRUE);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data) &&
        !g_strcmp0 (location_action->uri, uri))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
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
    midori_location_action_set_item (location_action, &item, FALSE, TRUE);
}

/**
 * midori_location_action_set_search_engines:
 * @location_action: a #MidoriLocationAction
 * @search_engines: a #KatzeArray
 *
 * Assigns the specified search engines to the location action.
 * Search engines will appear as actions in the completion.
 *
 * Since: 0.1.6
 **/
void
midori_location_action_set_search_engines (MidoriLocationAction* location_action,
                                           KatzeArray*           search_engines)
{
    GSList* proxies;
    GtkWidget* entry;
    GtkWidget* child;
    GtkEntryCompletion* completion;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    if (search_engines)
        g_object_ref (search_engines);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        KatzeItem* item;
        guint i;

        entry = midori_location_action_entry_for_proxy (proxies->data);
        child = gtk_bin_get_child (GTK_BIN (entry));

        midori_location_action_completion_init (location_action, GTK_ENTRY (child));
        completion = gtk_entry_get_completion (GTK_ENTRY (child));
        i = 0;
        if (location_action->search_engines)
        while ((item = katze_array_get_nth_item (location_action->search_engines, i++)))
            gtk_entry_completion_delete_action (completion, 0);
        midori_location_action_add_actions (completion, search_engines);
    }

    katze_object_assign (location_action->search_engines, search_engines);
    /* FIXME: Take care of adding and removing search engines as needed */
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
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    location_action->progress = CLAMP (progress, 0.0, 1.0);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        entry = midori_location_action_entry_for_proxy (proxies->data);

        midori_location_entry_set_progress (MIDORI_LOCATION_ENTRY (entry),
                                            location_action->progress);
    }
}

void
midori_location_action_set_secondary_icon (MidoriLocationAction* location_action,
                                           const gchar*          stock_id)
{
    GSList* proxies;
    GtkWidget* entry;
    GtkWidget* child;
    GtkStockItem stock_item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!stock_id || gtk_stock_lookup (stock_id, &stock_item));

    katze_assign (location_action->secondary_icon, g_strdup (stock_id));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        entry = midori_location_action_entry_for_proxy (proxies->data);
        child = gtk_bin_get_child (GTK_BIN (entry));

        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (child),
            GTK_ICON_ENTRY_SECONDARY, stock_id);
    }
}

/**
 * midori_location_action_delete_item_from_uri:
 * @location_action: a #MidoriLocationAction
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
    if (midori_location_action_iter_lookup (location_action, uri, &iter))
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
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    gtk_list_store_clear (GTK_LIST_STORE (location_action->model));
}
