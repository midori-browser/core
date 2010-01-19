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
#include "midori-browser.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#if HAVE_SQLITE
    #include <sqlite3.h>
#endif

#define COMPLETION_DELAY 150
#define MAX_ITEMS 25

struct _MidoriLocationAction
{
    GtkAction parent_instance;

    gchar* text;
    gchar* uri;
    gdouble progress;
    gchar* secondary_icon;

    GtkTreeModel* model;
    GtkTreeModel* filter_model;
    guint completion_timeout;
    gchar* key;
    GtkWidget* popup;
    GtkWidget* treeview;
    GtkTreeModel* completion_model;
    GtkWidget* entry;
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

#if !HAVE_SQLITE
static gboolean
midori_location_action_filter_match_cb (GtkTreeModel* model,
                                        GtkTreeIter*  iter,
                                        gpointer      data);
#endif

static void
midori_location_entry_render_text_cb (GtkCellLayout*   layout,
                                      GtkCellRenderer* renderer,
                                      GtkTreeModel*    model,
                                      GtkTreeIter*     iter,
                                      gpointer         data);

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

    /**
     * MidoriLocationAction:secondary-icon-released:
     *
     * The secondary-icon-released signal is emitted when the mouse button
     * is released above the secondary icon.
     *
     * Since 0.1.10 a signal handler can return %TRUE to stop signal
     * emission, for instance to suppress default behavior.
     */
    signals[SECONDARY_ICON_RELEASED] = g_signal_new ("secondary-icon-released",
                                       G_TYPE_FROM_CLASS (class),
                                       (GSignalFlags) (G_SIGNAL_RUN_LAST),
                                       0,
                                       g_signal_accumulator_true_handled,
                                       NULL,
                                       midori_cclosure_marshal_BOOLEAN__OBJECT,
                                       G_TYPE_BOOLEAN, 1,
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

    /* We want location entries to have appears-as-list applied */
    gtk_rc_parse_string ("style \"midori-location-entry-style\" {\n"
                         "  GtkComboBox::appears-as-list = 1\n }\n"
                         "widget \"*MidoriLocationEntry\" "
                         "style \"midori-location-entry-style\"\n");
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
midori_location_action_popup_position (GtkWidget* popup,
                                       GtkWidget* widget)
{
    gint wx, wy;
    GtkRequisition menu_req;
    GtkRequisition widget_req;

    if (GTK_WIDGET_NO_WINDOW (widget))
    {
        gdk_window_get_position (widget->window, &wx, &wy);
        wx += widget->allocation.x;
        wy += widget->allocation.y;
    }
    else
        gdk_window_get_origin (widget->window, &wx, &wy);
    gtk_widget_size_request (popup, &menu_req);
    gtk_widget_size_request (widget, &widget_req);

    gtk_window_move (GTK_WINDOW (popup),
        wx, wy + widget_req.height);
    gtk_window_resize (GTK_WINDOW (popup),
        widget->allocation.width, 1);
}

static gboolean
midori_location_action_popup_timeout_cb (gpointer data)
{
    MidoriLocationAction* action = data;
    static GtkTreeModel* model = NULL;
    GtkTreeViewColumn* column;
    #if HAVE_SQLITE
    GtkListStore* store;
    sqlite3* db;
    gchar* query;
    gint result;
    sqlite3_stmt* statement;
    #endif
    gint matches, height, screen_height;

    if (!gtk_widget_has_focus (action->entry))
        return FALSE;

    if (G_UNLIKELY (!action->popup))
    {
        GtkWidget* popup;
        GtkWidget* scrolled;
        GtkWidget* treeview;
        GtkCellRenderer* renderer;

        #if HAVE_SQLITE
        model = midori_location_action_create_model ();
        #else
        model = gtk_tree_model_filter_new (action->model, NULL);
        gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (model),
            midori_location_action_filter_match_cb, action, NULL);
        #endif
        action->completion_model = model;

        popup = gtk_window_new (GTK_WINDOW_POPUP);
        gtk_window_set_type_hint (GTK_WINDOW (popup), GDK_WINDOW_TYPE_HINT_COMBO);
        scrolled = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
            "hscrollbar-policy", GTK_POLICY_NEVER,
            "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
        gtk_container_add (GTK_CONTAINER (popup), scrolled);
        treeview = gtk_tree_view_new_with_model (model);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
        gtk_container_add (GTK_CONTAINER (scrolled), treeview);
        /* FIXME: Handle button presses and hovering rows */
        action->treeview = treeview;

        column = gtk_tree_view_column_new ();
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
            "pixbuf", FAVICON_COL, "yalign", YALIGN_COL, NULL);
        renderer = gtk_cell_renderer_text_new ();
        g_object_set_data (G_OBJECT (renderer), "location-action", action);
        gtk_cell_renderer_set_fixed_size (renderer, 1, -1);
        gtk_cell_renderer_text_set_fixed_height_from_font (
            GTK_CELL_RENDERER_TEXT (renderer), 2);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer,
                                            midori_location_entry_render_text_cb,
                                            action->entry, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        action->popup = popup;
        g_signal_connect (popup, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &action->popup);
    }

    if (!*action->key)
    {
        const gchar* uri = gtk_entry_get_text (GTK_ENTRY (action->entry));
        #if HAVE_SQLITE
        katze_assign (action->key, g_strdup (uri));
        #else
        katze_assign (action->key, katze_collfold (uri));
        #endif
    }

    #if HAVE_SQLITE
    store = GTK_LIST_STORE (model);
    gtk_list_store_clear (store);

    db = g_object_get_data (G_OBJECT (action->history), "db");
    /* FIXME: Consider keeping the prepared statement with '...LIKE ?...'
        and prepending/ appending % to the key. */
    query = sqlite3_mprintf ("SELECT DISTINCT uri, title FROM history WHERE "
                             "uri LIKE '%%%q%%' OR title LIKE '%%%q%%'"
                             "ORDER BY day LIMIT %d",
                             action->key, action->key, MAX_ITEMS);
    result = sqlite3_prepare_v2 (db, query, -1, &statement, NULL);
    sqlite3_free (query);
    matches = 0;
    if (result == SQLITE_OK)
    {
        while ((result = sqlite3_step (statement)) == SQLITE_ROW)
        {
            const unsigned char* uri = sqlite3_column_text (statement, 0);
            const unsigned char* title = sqlite3_column_text (statement, 1);
            GdkPixbuf* icon = katze_load_cached_icon ((gchar*)uri, NULL);
            if (!icon)
                icon = action->default_icon;
            gtk_list_store_insert_with_values (store, NULL, 0,
                URI_COL, uri, TITLE_COL, title, YALIGN_COL, 0.25,
                FAVICON_COL, icon, -1);
            matches++;
        }
        if (result != SQLITE_DONE)
            g_print (_("Failed to execute database statement: %s\n"),
                     sqlite3_errmsg (db));
        sqlite3_finalize (statement);
    }
    else
        g_print (_("Failed to execute database statement: %s\n"),
                 sqlite3_errmsg (db));
    #else
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));
    matches = gtk_tree_model_iter_n_children (model, NULL);
    #endif
    /* TODO: Suggest _("Search with %s") or opening hostname as actions */

    if (!GTK_WIDGET_VISIBLE (action->popup))
    {
        GtkWidget* toplevel = gtk_widget_get_toplevel (action->entry);
        gtk_window_set_screen (GTK_WINDOW (action->popup),
                               gtk_widget_get_screen (action->entry));
        gtk_window_set_transient_for (GTK_WINDOW (action->popup), GTK_WINDOW (toplevel));
        gtk_tree_view_columns_autosize (GTK_TREE_VIEW (action->treeview));
        gtk_widget_show_all (action->popup);
    }

    column = gtk_tree_view_get_column (GTK_TREE_VIEW (action->treeview), 0);
    gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &height);
    /* FIXME: This really should consider monitor geometry */
    /* FIXME: Consider y position versus height */
    screen_height = gdk_screen_get_height (gtk_widget_get_screen (action->popup));
    height = MIN (matches * height, screen_height / 1.5);
    gtk_widget_set_size_request (action->treeview, -1, height);
    midori_location_action_popup_position (action->popup, action->entry);

    return FALSE;
}

static void
midori_location_action_popup_completion (MidoriLocationAction* action,
                                         GtkWidget*            entry,
                                         const gchar*          key)
{
    if (action->completion_timeout)
        g_source_remove (action->completion_timeout);
    katze_assign (action->key, g_strdup (key));
    action->entry = entry;
    g_signal_connect (entry, "destroy",
        G_CALLBACK (gtk_widget_destroyed), &action->entry);
    action->completion_timeout = g_timeout_add (COMPLETION_DELAY,
        midori_location_action_popup_timeout_cb, action);
    /* TODO: Inline completion */
}

static void
midori_location_action_popdown_completion (MidoriLocationAction* location_action)
{
    if (G_LIKELY (location_action->popup))
    {
        gtk_widget_hide (location_action->popup);
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (
            GTK_TREE_VIEW (location_action->treeview)));
    }
    location_action->completion_timeout = 0;
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

static gboolean
midori_location_action_filter_recent_cb (GtkTreeModel* model,
                                         GtkTreeIter*  iter,
                                         gpointer      data)
{
    GtkTreePath* path = gtk_tree_model_get_path (model, iter);
    gint* indices = gtk_tree_path_get_indices (path);
    gboolean visible = *indices < MAX_ITEMS;
    gtk_tree_path_free (path);
    return visible;
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

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (midori_location_action_is_frozen (location_action));

    gtk_tree_sortable_set_sort_column_id (
        GTK_TREE_SORTABLE (location_action->model),
        VISITS_COL, GTK_SORT_DESCENDING);

    filter_model = gtk_tree_model_filter_new (location_action->model, NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter_model),
        midori_location_action_filter_recent_cb, location_action, NULL);
    location_action->filter_model = filter_model;
    midori_location_action_set_model (location_action, location_action->model);
}

static void
midori_location_action_init (MidoriLocationAction* location_action)
{
    location_action->text = location_action->uri = NULL;
    location_action->progress = 0.0;
    location_action->secondary_icon = NULL;
    location_action->default_icon = NULL;
    location_action->completion_timeout = 0;
    location_action->key = NULL;
    location_action->popup = NULL;
    location_action->entry = NULL;

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

    katze_assign (location_action->text, NULL);
    katze_assign (location_action->uri, NULL);

    katze_object_assign (location_action->model, NULL);
    katze_object_assign (location_action->filter_model, NULL);
    katze_assign (location_action->key, NULL);
    if (location_action->popup)
    {
        gtk_widget_destroy (location_action->popup);
        location_action->popup = NULL;
    }
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
        midori_location_action_add_item (action, uri,
            pixbuf, katze_item_get_name (item));
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
    GtkWidget* entry;
    #if HAVE_HILDON
    HildonGtkInputMode mode;
    #endif

    toolitem = GTK_WIDGET (gtk_tool_item_new ());
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);

    alignment = gtk_alignment_new (0.0f, 0.5f, 1.0f, 0.1f);
    gtk_widget_show (alignment);
    gtk_container_add (GTK_CONTAINER (toolitem), alignment);
    location_entry = gtk_combo_box_entry_new ();
    gtk_widget_set_name (location_entry, "MidoriLocationEntry");
    gtk_widget_show (location_entry);
    gtk_container_add (GTK_CONTAINER (alignment), location_entry);

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
    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (location_entry), entry);

    return toolitem;
}

static void
midori_location_action_changed_cb (GtkEntry*             entry,
                                   MidoriLocationAction* location_action)
{
    katze_assign (location_action->text, g_strdup (gtk_entry_get_text (entry)));
}

static gboolean
midori_location_action_key_press_event_cb (GtkEntry*    entry,
                                           GdkEventKey* event,
                                           GtkAction*   action)
{
    GtkWidget* widget = GTK_WIDGET (entry);
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (action);
    const gchar* text;
    static gint selected = -1;
    gboolean is_enter = FALSE;

    switch (event->keyval)
    {
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_Return:
        is_enter = TRUE;
    case GDK_Left:
    case GDK_KP_Left:
    case GDK_Right:
    case GDK_KP_Right:

        if (location_action->popup && GTK_WIDGET_VISIBLE (location_action->popup))
        {
            GtkTreeModel* model = location_action->completion_model;
            GtkTreeIter iter;
            midori_location_action_popdown_completion (location_action);
            if (selected > -1 &&
                gtk_tree_model_iter_nth_child (model, &iter, NULL, selected))
            {
                gchar* uri;
                gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);
                gtk_entry_set_text (entry, uri);

                if (is_enter)
                    g_signal_emit (action, signals[SUBMIT_URI], 0, uri,
                        (event->state & GDK_CONTROL_MASK) ? TRUE : FALSE);

                g_free (uri);
                selected = -1;
                return TRUE;
            }
            selected = -1;
        }

        if (is_enter)
            if ((text = gtk_entry_get_text (entry)) && *text)
                g_signal_emit (action, signals[SUBMIT_URI], 0, text,
                    (event->state & GDK_CONTROL_MASK) ? TRUE : FALSE);
        break;
    case GDK_Escape:
    {
        if (location_action->popup && GTK_WIDGET_VISIBLE (location_action->popup))
        {
            midori_location_action_popdown_completion (location_action);
            text = gtk_entry_get_text (entry);
            pango_layout_set_text (gtk_entry_get_layout (entry), text, -1);
            selected = -1;
            return TRUE;
        }

        g_signal_emit (action, signals[RESET_URI], 0);
        /* Return FALSE to allow Escape to stop loading */
        return FALSE;
    }
    case GDK_Page_Up:
    case GDK_Page_Down:
        if (!(location_action->popup && GTK_WIDGET_VISIBLE (location_action->popup)))
            return TRUE;
    case GDK_Down:
    case GDK_KP_Down:
    case GDK_Up:
    case GDK_KP_Up:
    {
        GtkWidget* parent;

        if (location_action->popup && GTK_WIDGET_VISIBLE (location_action->popup))
        {
            GtkTreeModel* model = location_action->completion_model;
            gint matches = gtk_tree_model_iter_n_children (model, NULL);
            GtkTreePath* path;
            GtkTreeIter iter;

            if (event->keyval == GDK_Down || event->keyval == GDK_KP_Down)
                selected = MIN (selected + 1, matches -1);
            else if (event->keyval == GDK_Up || event->keyval == GDK_KP_Up)
                selected = MAX (selected - 1, 0);
            else if (event->keyval == GDK_Page_Down)
                selected = MIN (selected + 14, matches -1);
            else if (event->keyval == GDK_Page_Up)
                selected = MAX (selected - 14, 0);

            path = gtk_tree_path_new_from_indices (selected, -1);
            gtk_tree_view_set_cursor (GTK_TREE_VIEW (location_action->treeview),
                                      path, NULL, FALSE);
            gtk_tree_path_free (path);

            if (gtk_tree_model_iter_nth_child (model, &iter, NULL, selected))
            {
                gchar* uri;
                gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);
                /* Update the layout without actually changing the text */
                pango_layout_set_text (gtk_entry_get_layout (entry), uri, -1);
                g_free (uri);
            }
            return TRUE;
        }

        parent = gtk_widget_get_parent (widget);
        if (!katze_object_get_boolean (parent, "popup-shown"))
            gtk_combo_box_popup (GTK_COMBO_BOX (parent));
        return TRUE;
    }
    default:
        if ((text = gtk_entry_get_text (entry)) && *text)
        {
            midori_location_action_popup_completion (location_action, widget, "");
            selected = -1;
            return FALSE;
        }
    }
    return FALSE;
}

#if GTK_CHECK_VERSION (2, 19, 3)
static void
midori_location_action_preedit_changed_cb (GtkWidget*   widget,
                                           const gchar* preedit,
                                           GtkAction*   action)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (action);
    #if HAVE_SQLITE
    midori_location_action_popup_completion (location_action,
                                             GTK_WIDGET (widget), preedit);
    #else
    gchar* key = katze_collfold (preedit);
    midori_location_action_popup_completion (location_action,
                                             GTK_WIDGET (widget), key);
    g_free (key);
    #endif
}
#endif

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
    midori_location_action_popdown_completion (MIDORI_LOCATION_ACTION (action));
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
    {
        gboolean result;
        g_signal_emit (action, signals[SECONDARY_ICON_RELEASED], 0,
                       widget, &result);
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
    GdkPixbuf* icon;
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

    entry = data;

    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title,
                        FAVICON_COL, &icon, -1);

    if (G_UNLIKELY (!icon))
    {
        if (uri)
        {
            #if !HAVE_HILDON
            MidoriLocationAction* action;

            action = g_object_get_data (G_OBJECT (renderer), "location-action");
            if ((icon = katze_load_cached_icon (uri, NULL)))
            {
                midori_location_action_set_icon_for_uri (action, icon, uri);
                g_object_unref (icon);
            }
            else
                midori_location_action_set_icon_for_uri (action, action->default_icon, uri);
            #endif
        }
    }
    else
        g_object_unref (icon);

    desc = desc_uri = desc_title = key = NULL;
    key = g_utf8_strdown (gtk_entry_get_text (GTK_ENTRY (entry)), -1);
    len = 0;

    if (G_LIKELY (uri))
    {
        /* g_uri_unescape_segment () sometimes produces garbage */
        if (!g_utf8_validate (uri, -1, (const gchar **)&temp))
            temp[0] = '\0';

        temp = g_utf8_strdown (uri, -1);
        if ((start = strstr (temp, key)) && (start - temp))
        {
            len = strlen (key);
            skey = g_strndup (uri + (start - temp), len);
            parts = g_strsplit (uri, skey, 2);
            if (parts[0] && parts[1])
                desc_uri = g_markup_printf_escaped ("%s<b>%s</b>%s",
                    parts[0], skey, parts[1]);
            else
                desc_uri = g_markup_escape_text (uri, -1);
            g_strfreev (parts);
            g_free (skey);
        }
        else
            desc_uri = g_markup_escape_text (uri, -1);
        g_free (temp);
    }

    if (G_LIKELY (title))
    {
        /* g_uri_unescape_segment () sometimes produces garbage */
        if (!g_utf8_validate (title, -1, (const gchar **)&temp))
            temp[0] = '\0';

        temp = g_utf8_strdown (title, -1);
        if ((start = strstr (temp, key)) && (start - temp))
        {
            if (!len)
                len = strlen (key);
            skey = g_strndup (title + (start - temp), len);
            parts = g_strsplit (title, skey, 2);
            if (parts && parts[0] && parts[1])
                desc_title = g_markup_printf_escaped ("%s<b>%s</b>%s",
                    parts[0], skey, parts[1]);
            else
                desc_title = g_markup_escape_text (title, -1);
            g_strfreev (parts);
            g_free (skey);
        }
        else
            desc_title = g_markup_escape_text (title, -1);
        g_free (temp);
    }

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

#if !HAVE_SQLITE
static gboolean
midori_location_action_match (GtkTreeModel* model,
                              const gchar*  key,
                              GtkTreeIter*  iter,
                              gpointer      data)
{
    gchar* uri;
    gchar* title;
    gboolean match;

    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title, -1);

    match = FALSE;
    if (G_LIKELY (uri))
    {
        match = katze_utf8_stristr (uri, key);
        g_free (uri);

        if (!match && G_LIKELY (title))
            match = katze_utf8_stristr (title, key);
    }

    g_free (title);

    return match;
}

static gboolean
midori_location_action_filter_match_cb (GtkTreeModel* model,
                                        GtkTreeIter*  iter,
                                        gpointer      data)
{
    MidoriLocationAction* action = data;
    return midori_location_action_match (model, action->key, iter, data);
}
#endif

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
midori_location_action_set_item (MidoriLocationAction* location_action,
                                 GdkPixbuf*            icon,
                                 const gchar*          uri,
                                 const gchar*          title,
                                 gboolean              increment_visits,
                                 gboolean              filter)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    GdkPixbuf* new_icon;
    gint visits = 0;
    gchar* _title = NULL;
    GdkPixbuf* original_icon = NULL;

    model = location_action->model;

    if (midori_location_action_iter_insert (location_action, uri, &iter, G_MAXINT))
        gtk_tree_model_get (model, &iter, VISITS_COL, &visits, -1);

    gtk_tree_model_get (model, &iter, FAVICON_COL, &original_icon, -1);

    if (increment_visits)
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            VISITS_COL, ++visits, VISIBLE_COL, TRUE, -1);

    /* Ensure we keep the title if we added the same URI with a title before */
    if (!title)
    {
        gtk_tree_model_get (model, &iter, TITLE_COL, &_title, -1);
        title = _title;
    }

    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
        URI_COL, uri, TITLE_COL, title, YALIGN_COL, 0.25, -1);
    g_free (_title);

    if (icon)
        new_icon = icon;
    else if (original_icon)
    {
        new_icon = NULL;
        g_object_unref (original_icon);
    }
    else
        new_icon = NULL;
    if (new_icon)
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            FAVICON_COL, new_icon, -1);
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

            #if !HAVE_HILDON
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                                                 GTK_ICON_ENTRY_PRIMARY, pixbuf);
            #endif
            g_object_unref (pixbuf);
            katze_assign (location_action->text, uri);
            katze_assign (location_action->uri, g_strdup (uri));

            g_signal_emit (location_action, signals[ACTIVE_CHANGED], 0,
                           gtk_combo_box_get_active (combo_box));
        }
    }
}

static void
midori_location_action_paste_proceed_cb (GtkWidget* menuitem,
                                         GtkWidget* location_action)
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display (
        gtk_widget_get_display (GTK_WIDGET (menuitem)),GDK_SELECTION_CLIPBOARD);
    gchar* uri;

    if ((uri = gtk_clipboard_wait_for_text (clipboard)))
    {
        g_signal_emit (location_action, signals[SUBMIT_URI], 0, uri, FALSE);
        g_free (uri);
    }
}

static void
midori_location_action_populate_popup_cb (GtkWidget*            entry,
                                          GtkMenuShell*         menu,
                                          MidoriLocationAction* location_action)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (entry);
    GtkActionGroup* actions = midori_browser_get_action_group (browser);
    GtkWidget* menuitem;

    menuitem = gtk_separator_menu_item_new ();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (menu, menuitem);
    menuitem = sokoke_action_create_popup_menu_item (
        gtk_action_group_get_action (actions, "ManageSearchEngines"));
    gtk_menu_shell_append (menu, menuitem);
    /* i18n: Right-click on Location, Open an URL from the clipboard */
    menuitem = gtk_menu_item_new_with_mnemonic (_("Paste and p_roceed"));
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (menu, menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_location_action_paste_proceed_cb), location_action);
}

static void
midori_location_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy)
{
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
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxy);
        GtkWidget* child = gtk_bin_get_child (GTK_BIN (entry));

        gtk_icon_entry_set_progress_fraction (GTK_ICON_ENTRY (child),
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
        g_object_set_data (G_OBJECT (renderer), "location-action", action);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (entry),
            renderer, midori_location_entry_render_text_cb, child, NULL);

        gtk_combo_box_set_active (GTK_COMBO_BOX (entry), -1);
        g_signal_connect (entry, "changed",
            G_CALLBACK (midori_location_action_entry_changed_cb), action);

        g_object_connect (child,
                      "signal::changed",
                      midori_location_action_changed_cb, action,
                      "signal::key-press-event",
                      midori_location_action_key_press_event_cb, action,
                      #if GTK_CHECK_VERSION (2, 19, 3)
                      "signal::preedit-changed",
                      midori_location_action_preedit_changed_cb, action,
                      #endif
                      "signal::focus-in-event",
                      midori_location_action_focus_in_event_cb, action,
                      "signal::focus-out-event",
                      midori_location_action_focus_out_event_cb, action,
                      "signal::icon-release",
                      midori_location_action_icon_released_cb, action,
                      "signal::populate-popup",
                      midori_location_action_populate_popup_cb, action,
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

/**
 * midori_location_action_get_uri:
 * @location_action: a #MidoriLocationAction
 *
 * Retrieves the current URI. See also midori_location_action_get_text().
 *
 * Return value: the current URI
 **/
const gchar*
midori_location_action_get_uri (MidoriLocationAction* location_action)
{
    g_return_val_if_fail (MIDORI_IS_LOCATION_ACTION (location_action), NULL);

    return location_action->uri;
}

/**
 * midori_location_action_get_text:
 * @location_action: a #MidoriLocationAction
 *
 * Retrieves the current text, which may be the current URI or
 * anything typed in the entry.
 *
 * Return value: the current text
 *
 * Since: 0.2.0
 **/
const gchar*
midori_location_action_get_text (MidoriLocationAction* location_action)
{
    g_return_val_if_fail (MIDORI_IS_LOCATION_ACTION (location_action), NULL);

    return location_action->text;
}

/**
 * midori_location_action_set_text:
 * @location_action: a #MidoriLocationAction
 * @text: a string
 *
 * Sets the entry text to @text and, if applicable, updates the icon.
 *
 * Since: 0.2.0
 **/
void
midori_location_action_set_text (MidoriLocationAction* location_action,
                                 const gchar*          text)
{
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;
    GtkTreeIter iter;
    GdkPixbuf* icon;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (text != NULL);

    katze_assign (location_action->text, g_strdup (text));

    if (!(proxies = gtk_action_get_proxies (GTK_ACTION (location_action))))
        return;

    if (midori_location_action_iter_lookup (location_action, text, &iter))
    {
        gtk_tree_model_get (location_action->model,
                            &iter, FAVICON_COL, &icon, -1);
        katze_assign (location_action->uri, g_strdup (text));
    }
    else
        icon = g_object_ref (location_action->default_icon);

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_entry_set_text (GTK_ENTRY (entry), text);
        #if !HAVE_HILDON
        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
        #endif
    }

    if (icon)
        g_object_unref (icon);
}

/**
 * midori_location_action_set_uri:
 * @location_action: a #MidoriLocationAction
 * @uri: an URI string
 *
 * Sets the entry URI to @uri and, if applicable, updates the icon.
 *
 * Deprecated: 0.2.0
 **/
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
    #if !HAVE_HILDON
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
    #endif
}

void
midori_location_action_add_uri (MidoriLocationAction* location_action,
                                const gchar*          uri)
{
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);

    if (midori_location_action_is_frozen (location_action))
        return;

    midori_location_action_set_item (location_action, NULL, uri, NULL, TRUE, TRUE);

    katze_assign (location_action->uri, g_strdup (uri));
}

void
midori_location_action_add_item (MidoriLocationAction* location_action,
                                 const gchar*          uri,
                                 GdkPixbuf*            icon,
                                 const gchar*          title)
{
    #if !HAVE_HILDON
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;
    #endif

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);
    g_return_if_fail (title != NULL);
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));

    midori_location_action_set_item (location_action, icon, uri, title, TRUE, FALSE);

    #if !HAVE_HILDON
    if (midori_location_action_is_frozen (location_action))
        return;

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    if (!g_strcmp0 (location_action->uri, uri))
    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
    #endif
}

void
midori_location_action_set_icon_for_uri (MidoriLocationAction* location_action,
                                         GdkPixbuf*            icon,
                                         const gchar*          uri)
{
    #if !HAVE_HILDON
    GSList* proxies;
    GtkWidget* location_entry;
    GtkWidget* entry;
    #endif

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!icon || GDK_IS_PIXBUF (icon));
    g_return_if_fail (uri != NULL);

    midori_location_action_set_item (location_action, icon, uri, NULL, FALSE, TRUE);

    #if !HAVE_HILDON
    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    if (!g_strcmp0 (location_action->uri, uri))
    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        location_entry = midori_location_action_entry_for_proxy (proxies->data);
        entry = gtk_bin_get_child (GTK_BIN (location_entry));

        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
            GTK_ICON_ENTRY_PRIMARY, icon);
    }
    #endif
}

void
midori_location_action_set_title_for_uri (MidoriLocationAction* location_action,
                                          const gchar*          title,
                                          const gchar*          uri)
{
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (title != NULL);
    g_return_if_fail (uri != NULL);

    midori_location_action_set_item (location_action, NULL, uri, title, FALSE, TRUE);
}

/**
 * midori_location_action_set_search_engines:
 * @location_action: a #MidoriLocationAction
 * @search_engines: a #KatzeArray
 *
 * This function is obsolete and has no effect.
 *
 * Deprecated: 0.2.3
 **/
void
midori_location_action_set_search_engines (MidoriLocationAction* location_action,
                                           KatzeArray*           search_engines)
{
    /* Do nothing */
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
    GtkWidget* child;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    location_action->progress = CLAMP (progress, 0.0, 1.0);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        entry = midori_location_action_entry_for_proxy (proxies->data);
        child = gtk_bin_get_child (GTK_BIN (entry));

        gtk_icon_entry_set_progress_fraction (GTK_ICON_ENTRY (child),
                                              location_action->progress);
    }
}

void
midori_location_action_set_secondary_icon (MidoriLocationAction* location_action,
                                           const gchar*          stock_id)
{
    #if !HAVE_HILDON
    GSList* proxies;
    GtkWidget* entry;
    GtkWidget* child;
    #endif
    GtkStockItem stock_item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (!stock_id || gtk_stock_lookup (stock_id, &stock_item));

    katze_assign (location_action->secondary_icon, g_strdup (stock_id));

    #if !HAVE_HILDON
    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        entry = midori_location_action_entry_for_proxy (proxies->data);
        child = gtk_bin_get_child (GTK_BIN (entry));

        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (child),
            GTK_ICON_ENTRY_SECONDARY, stock_id);
    }
    #endif
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
