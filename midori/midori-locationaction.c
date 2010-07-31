/*
 Copyright (C) 2008-2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008-2010 Dale Whittaker <dayul@users.sf.net>

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

#include <sqlite3.h>

#define COMPLETION_DELAY 200
#define MAX_ITEMS 25

struct _MidoriLocationAction
{
    GtkAction parent_instance;

    gchar* text;
    gchar* uri;
    KatzeArray* search_engines;
    gdouble progress;
    gchar* secondary_icon;

    guint completion_timeout;
    gchar* key;
    GtkWidget* popup;
    GtkWidget* treeview;
    GtkTreeModel* completion_model;
    gint completion_index;
    GtkWidget* entry;
    GdkPixbuf* default_icon;
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
    BACKGROUND_COL,
    STYLE_COL,
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
midori_location_entry_render_text_cb (GtkCellLayout*   layout,
                                      GtkCellRenderer* renderer,
                                      GtkTreeModel*    model,
                                      GtkTreeIter*     iter,
                                      gpointer         data);

static void
midori_location_action_popdown_completion (MidoriLocationAction* location_action);

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
        G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_FLOAT,
        GDK_TYPE_COLOR, G_TYPE_BOOLEAN);
    return model;
}

static void
midori_location_action_popup_position (GtkWidget* popup,
                                       GtkWidget* widget)
{
    gint wx, wy;
    GtkRequisition menu_req;
    GtkRequisition widget_req;
    GdkScreen* screen;
    gint monitor_num;
    GdkRectangle monitor;

    gdk_window_get_origin (widget->window, &wx, &wy);
    gtk_widget_size_request (popup, &menu_req);
    gtk_widget_size_request (widget, &widget_req);

    screen = gtk_widget_get_screen (widget);
    monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);
    gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

    if (wy + widget_req.height + menu_req.height <= monitor.y + monitor.height
     || wy - monitor.y < (monitor.y + monitor.height) - (wy + widget_req.height))
        wy += widget_req.height;
    else
        wy -= menu_req.height;
    gtk_window_move (GTK_WINDOW (popup),  wx, wy);
    gtk_window_resize (GTK_WINDOW (popup), widget->allocation.width, 1);
}

static gboolean
midori_location_action_treeview_button_press_cb (GtkWidget*            treeview,
                                                 GdkEventButton*       event,
                                                 MidoriLocationAction* action)
{
    GtkTreePath* path;

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
        event->x, event->y, &path, NULL, NULL, NULL))
    {
        GtkTreeIter iter;
        gchar* uri;

        gtk_tree_model_get_iter (action->completion_model, &iter, path);
        gtk_tree_path_free (path);

        midori_location_action_popdown_completion (action);

        gtk_tree_model_get (action->completion_model, &iter, URI_COL, &uri, -1);
        gtk_entry_set_text (GTK_ENTRY (action->entry), uri);
        g_signal_emit (action, signals[SUBMIT_URI], 0, uri,
                       MIDORI_MOD_NEW_TAB (event->state));
        g_free (uri);

        return TRUE;
    }

    return FALSE;
}

static gboolean
midori_location_action_popup_timeout_cb (gpointer data)
{
    MidoriLocationAction* action = data;
    GtkTreeViewColumn* column;
    GtkListStore* store;
    gint result;
    static sqlite3_stmt* stmt;
    const gchar* sqlcmd;
    gint matches, searches, height, screen_height, browser_height, sep;
    MidoriBrowser* browser;
    GtkStyle* style;

    if (!action->entry || !gtk_widget_has_focus (action->entry) || !action->history)
        return FALSE;

    if (!(action->key && *action->key))
    {
        midori_location_action_popdown_completion (action);
        return FALSE;
    }

    if (!stmt)
    {
        sqlite3* db;
        db = g_object_get_data (G_OBJECT (action->history), "db");
        sqlcmd = "SELECT type, uri, title FROM ("
                 "  SELECT 1 AS type, uri, title, count() AS ct FROM history "
                 "      WHERE uri LIKE ?1 OR title LIKE ?1 GROUP BY uri "
                 "  UNION ALL "
                 "  SELECT 2 AS type, replace(uri, '%s', keywords) AS uri, "
                 "      keywords AS title, count() AS ct FROM search "
                 "      WHERE uri LIKE ?1 OR title LIKE ?1 GROUP BY uri "
                 "  UNION ALL "
                 "  SELECT 1 AS type, uri, title, 50 AS ct FROM bookmarks "
                 "      WHERE title LIKE ?1 OR uri LIKE ?1 AND uri !='' "
                 ") GROUP BY uri ORDER BY ct DESC LIMIT ?2";
        sqlite3_prepare_v2 (db, sqlcmd, strlen (sqlcmd) + 1, &stmt, NULL);
    }
    sqlite3_bind_text (stmt, 1, g_strdup_printf ("%%%s%%", action->key), -1, g_free);
    sqlite3_bind_int64 (stmt, 2, MAX_ITEMS);

    result = sqlite3_step (stmt);
    if (result != SQLITE_ROW && !action->search_engines)
    {
        if (result == SQLITE_ERROR)
            g_print (_("Failed to select from history\n"));
        sqlite3_reset (stmt);
        sqlite3_clear_bindings (stmt);
        midori_location_action_popdown_completion (action);
        return FALSE;
    }

    if (G_UNLIKELY (!action->popup))
    {
        GtkTreeModel* model = NULL;
        GtkWidget* popup;
        GtkWidget* popup_frame;
        GtkWidget* scrolled;
        GtkWidget* treeview;
        GtkCellRenderer* renderer;

        model = midori_location_action_create_model ();
        action->completion_model = model;

        popup = gtk_window_new (GTK_WINDOW_POPUP);
        gtk_window_set_type_hint (GTK_WINDOW (popup), GDK_WINDOW_TYPE_HINT_COMBO);
        popup_frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (popup_frame), GTK_SHADOW_ETCHED_IN);
        gtk_container_add (GTK_CONTAINER (popup), popup_frame);
        scrolled = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
            "hscrollbar-policy", GTK_POLICY_NEVER,
            "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
        gtk_container_add (GTK_CONTAINER (popup_frame), scrolled);
        treeview = gtk_tree_view_new_with_model (model);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
        gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (treeview), TRUE);
        gtk_container_add (GTK_CONTAINER (scrolled), treeview);
        g_signal_connect (treeview, "button-press-event",
            G_CALLBACK (midori_location_action_treeview_button_press_cb), action);
        /* a nasty hack to get the completions treeview to size nicely */
        gtk_widget_set_size_request (gtk_scrolled_window_get_vscrollbar (
            GTK_SCROLLED_WINDOW (scrolled)), -1, 0);
        action->treeview = treeview;

        column = gtk_tree_view_column_new ();
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
            "pixbuf", FAVICON_COL, "yalign", YALIGN_COL,
            "cell-background-gdk", BACKGROUND_COL,
            NULL);
        renderer = gtk_cell_renderer_text_new ();
        g_object_set_data (G_OBJECT (renderer), "location-action", action);
        gtk_cell_renderer_set_fixed_size (renderer, 1, -1);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
            "cell-background-gdk", BACKGROUND_COL,
            NULL);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer,
                                            midori_location_entry_render_text_cb,
                                            action, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        action->popup = popup;
        g_signal_connect (popup, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &action->popup);
    }

    store = GTK_LIST_STORE (action->completion_model);
    gtk_list_store_clear (store);

    matches = searches = 0;
    style = gtk_widget_get_style (action->treeview);
    while (result == SQLITE_ROW)
    {
        gchar* unescaped_uri;
        sqlite3_int64 type = sqlite3_column_int64 (stmt, 0);
        const unsigned char* uri = sqlite3_column_text (stmt, 1);
        const unsigned char* title = sqlite3_column_text (stmt, 2);
        GdkPixbuf* icon = katze_load_cached_icon ((gchar*)uri, NULL);
        if (!icon)
            icon = action->default_icon;
        if (type == 1 /* history_view */)
        {
            unescaped_uri = sokoke_uri_unescape_string ((const char*)uri);

            gtk_list_store_insert_with_values (store, NULL, matches,
                URI_COL, unescaped_uri, TITLE_COL, title, YALIGN_COL, 0.25,
                FAVICON_COL, icon, -1);
            g_free (unescaped_uri);
        }
        else if (type == 2 /* search_view */)
        {
            gchar* search_title = g_strdup_printf (_("Search for %s"), title);
            gtk_list_store_insert_with_values (store, NULL, matches,
                URI_COL, uri, TITLE_COL, search_title, YALIGN_COL, 0.25,
                STYLE_COL, 1, FAVICON_COL, icon, -1);
            g_free (search_title);
        }

        matches++;
        result = sqlite3_step (stmt);
    }
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);

    if (action->search_engines)
    {
        gint i = 0;
        KatzeItem* item;
        while ((item = katze_array_get_nth_item (action->search_engines, i)))
        {
            gchar* uri;
            gchar* title;

            uri = sokoke_search_uri (katze_item_get_uri (item), action->key);
            title = g_strdup_printf (_("Search with %s"), katze_item_get_name (item));
            gtk_list_store_insert_with_values (store, NULL, matches + i,
                URI_COL, uri, TITLE_COL, title, YALIGN_COL, 0.25,
                BACKGROUND_COL, style ? &style->bg[GTK_STATE_NORMAL] : NULL,
                STYLE_COL, 1, FAVICON_COL, NULL, -1);
            g_free (uri);
            g_free (title);
            i++;
        }
        searches += i;
    }

    if (!gtk_widget_get_visible (action->popup))
    {
        GtkWidget* toplevel = gtk_widget_get_toplevel (action->entry);
        gtk_window_set_screen (GTK_WINDOW (action->popup),
                               gtk_widget_get_screen (action->entry));
        gtk_window_set_transient_for (GTK_WINDOW (action->popup), GTK_WINDOW (toplevel));
        gtk_tree_view_columns_autosize (GTK_TREE_VIEW (action->treeview));
    }

    browser = midori_browser_get_for_widget (action->entry);
    column = gtk_tree_view_get_column (GTK_TREE_VIEW (action->treeview), 0);
    gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &height);
    screen_height = gdk_screen_get_height (gtk_widget_get_screen (action->popup));
    gtk_window_get_size (GTK_WINDOW (browser), NULL, &browser_height);
    screen_height = MIN (MIN (browser_height, screen_height / 1.5), screen_height / 1.5);
    gtk_widget_style_get (action->treeview, "vertical-separator", &sep, NULL);
    /* FIXME: Instead of 1.5 we should relate to the height of one line */
    height = MIN (matches * height + (matches + searches) * sep
                                   + searches * height / 1.5, screen_height);
    gtk_widget_set_size_request (action->treeview, -1, height);
    midori_location_action_popup_position (action->popup, action->entry);
    gtk_widget_show_all (action->popup);

    return FALSE;
}

static void
midori_location_action_popup_completion (MidoriLocationAction* action,
                                         GtkWidget*            entry,
                                         gchar*                key)
{
    if (action->completion_timeout)
        g_source_remove (action->completion_timeout);
    katze_assign (action->key, key);
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
        katze_assign (location_action->key, NULL);
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (
            GTK_TREE_VIEW (location_action->treeview)));
    }
    if (location_action->completion_timeout)
    {
        g_source_remove (location_action->completion_timeout);
        location_action->completion_timeout = 0;
    }
    location_action->completion_index = -1;
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
midori_location_action_init (MidoriLocationAction* location_action)
{
    location_action->text = location_action->uri = NULL;
    location_action->search_engines = NULL;
    location_action->progress = 0.0;
    location_action->secondary_icon = NULL;
    location_action->default_icon = NULL;
    location_action->completion_timeout = 0;
    location_action->completion_index = -1;
    location_action->key = NULL;
    location_action->popup = NULL;
    location_action->entry = NULL;
    location_action->history = NULL;
}

static void
midori_location_action_finalize (GObject* object)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (object);

    katze_assign (location_action->text, NULL);
    katze_assign (location_action->uri, NULL);
    katze_assign (location_action->search_engines, NULL);

    katze_assign (location_action->key, NULL);
    if (location_action->popup)
    {
        gtk_widget_destroy (location_action->popup);
        location_action->popup = NULL;
    }
    katze_object_assign (location_action->default_icon, NULL);
    katze_object_assign (location_action->history, NULL);

    G_OBJECT_CLASS (midori_location_action_parent_class)->finalize (object);
}

static void
midori_location_action_toggle_arrow_cb (GtkWidget*            widget,
                                        MidoriLocationAction* location_action)
{    gboolean show = FALSE;

    sqlite3* db;
    const gchar* sqlcmd;
    sqlite3_stmt* statement;
    gint result;

    if (!GTK_IS_BUTTON (widget))
        return;

    db = g_object_get_data (G_OBJECT (location_action->history), "db");
    sqlcmd = "SELECT uri FROM history LIMIT 1";
    sqlite3_prepare_v2 (db, sqlcmd, -1, &statement, NULL);
    result = sqlite3_step (statement);
    if (result == SQLITE_ROW)
        show = TRUE;
    sqlite3_finalize (statement);
    sokoke_widget_set_visible (widget, show);
    gtk_widget_set_size_request (widget, show ? -1 : 1, show ? -1 : 1);
}

static void
midori_location_action_toggle_arrow (MidoriLocationAction* location_action)
{
    GSList* proxies;

    if (!location_action->history)
        return;

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));
    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxies->data);
        gtk_container_forall (GTK_CONTAINER (entry),
            (GtkCallback)midori_location_action_toggle_arrow_cb, location_action);
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
        katze_assign (location_action->history, g_value_dup_object (value));

        midori_location_action_toggle_arrow (location_action);
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

static void
midori_location_action_move_cursor_cb (GtkEntry*             entry,
                                       GtkMovementStep       step,
                                       gint                  count,
                                       gboolean              extend_selection,
                                       MidoriLocationAction* action)
{
    gchar* text = g_strdup (pango_layout_get_text (gtk_entry_get_layout (entry)));
    /* Update entry with the completed text */
    gtk_entry_set_text (entry, text);
    g_free (text);
    midori_location_action_popdown_completion (action);
}

static void
midori_location_action_backspace_cb (GtkWidget*            entry,
                                     MidoriLocationAction* action)
{
    gchar* key = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    midori_location_action_popup_completion (action, entry, key);
    action->completion_index = -1;
}

static void
midori_location_action_paste_clipboard_cb (GtkWidget*            entry,
                                           MidoriLocationAction* action)
{
    gchar* key = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    midori_location_action_popup_completion (action, entry, key);
    action->completion_index = -1;
}

static gboolean
midori_location_action_button_press_event_cb (GtkEntry*             entry,
                                              GdkEventKey*          event,
                                              MidoriLocationAction* action)
{
    if (action->popup && gtk_widget_get_visible (action->popup))
    {
        midori_location_action_popdown_completion (action);

        /* Allow button handling, for context menu and selection */
        return FALSE;
    }

    return FALSE;
}

static gboolean
midori_location_action_key_press_event_cb (GtkEntry*    entry,
                                           GdkEventKey* event,
                                           GtkAction*   action)
{
    GtkWidget* widget = GTK_WIDGET (entry);
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (action);
    const gchar* text;
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

        if (location_action->popup && gtk_widget_get_visible (location_action->popup))
        {
            GtkTreeModel* model = location_action->completion_model;
            GtkTreeIter iter;
            gint selected = location_action->completion_index;
            midori_location_action_popdown_completion (location_action);
            if (selected > -1 &&
                gtk_tree_model_iter_nth_child (model, &iter, NULL, selected))
            {
                gchar* uri;
                gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);
                gtk_entry_set_text (entry, uri);

                if (is_enter)
                    g_signal_emit (action, signals[SUBMIT_URI], 0, uri,
                                   MIDORI_MOD_NEW_TAB (event->state));

                g_free (uri);
                return TRUE;
            }
        }

        if (is_enter)
            if ((text = gtk_entry_get_text (entry)) && *text)
                g_signal_emit (action, signals[SUBMIT_URI], 0, text,
                               MIDORI_MOD_NEW_TAB (event->state));
        break;
    case GDK_Escape:
    {
        if (location_action->popup && gtk_widget_get_visible (location_action->popup))
        {
            midori_location_action_popdown_completion (location_action);
            text = gtk_entry_get_text (entry);
            pango_layout_set_text (gtk_entry_get_layout (entry), text, -1);
            return TRUE;
        }

        g_signal_emit (action, signals[RESET_URI], 0);
        /* Return FALSE to allow Escape to stop loading */
        return FALSE;
    }
    case GDK_Page_Up:
    case GDK_Page_Down:
        if (!(location_action->popup && gtk_widget_get_visible (location_action->popup)))
            return TRUE;
    case GDK_Down:
    case GDK_KP_Down:
    case GDK_Up:
    case GDK_KP_Up:
    {
        GtkWidget* parent;

        if (location_action->popup && gtk_widget_get_visible (location_action->popup))
        {
            GtkTreeModel* model = location_action->completion_model;
            gint matches = gtk_tree_model_iter_n_children (model, NULL);
            GtkTreePath* path;
            GtkTreeIter iter;
            gint selected = location_action->completion_index;

            if (event->keyval == GDK_Down || event->keyval == GDK_KP_Down)
                selected = MIN (selected + 1, matches -1);
            else if (event->keyval == GDK_Up || event->keyval == GDK_KP_Up)
            {
                if (selected == -1)
                    selected = matches - 1;
                else
                    selected = MAX (selected - 1, 0);
            }
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
            location_action->completion_index = selected;
            return TRUE;
        }

        parent = gtk_widget_get_parent (widget);
        if (!katze_object_get_boolean (parent, "popup-shown"))
            gtk_combo_box_popup (GTK_COMBO_BOX (parent));
        return TRUE;
    }
    default:
    {
        gunichar character;
        gchar buffer[7];
        gint length;
        gchar* key;

        character = gdk_keyval_to_unicode (event->keyval);
        /* Don't trigger completion on control characters */
        if (!character || event->is_modifier)
            return FALSE;

        length = g_unichar_to_utf8 (character, buffer);
        buffer[length] = '\0';
        key = g_strconcat (gtk_entry_get_text (entry), buffer, NULL);
        midori_location_action_popup_completion (location_action, widget, key);
        location_action->completion_index = -1;
        return FALSE;
    }
    }
    return FALSE;
}

#if GTK_CHECK_VERSION (2, 19, 3)
static void
midori_location_action_preedit_changed_cb (GtkWidget*   entry,
                                           const gchar* preedit,
                                           GtkAction*   action)
{
    MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (action);
    gchar* key = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    midori_location_action_popup_completion (location_action, entry, key);
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
    MidoriLocationAction* action = data;
    gchar* uri;
    gchar* title;
    GdkColor* background;
    gboolean style;
    gchar* desc;
    gchar* desc_uri;
    gchar* desc_title;
    const gchar* str;
    gchar* key;
    gchar* start;
    gchar* skey;
    gchar* temp;
    gchar** parts;
    size_t len;

    gtk_tree_model_get (model, iter, URI_COL, &uri, TITLE_COL, &title,
        BACKGROUND_COL, &background, STYLE_COL, &style, -1);

    if (style) /* A search engine action */
    {
        g_object_set (renderer, "text", title,
            "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        g_free (uri);
        g_free (title);
        return;
    }

    desc = desc_uri = desc_title = key = NULL;
    if (action->key)
        str = action->key;
    else
        str = "";

    key = g_utf8_strdown (str, -1);
    len = strlen (key);

    if (G_LIKELY (uri))
    {
        temp = g_utf8_strdown (uri, -1);
        if ((start = strstr (temp, key)) && (start - temp))
        {
            skey = g_strndup (uri + (start - temp), len);
            parts = g_strsplit (uri, skey, 2);
            if (parts[0] && parts[1])
                desc_uri = g_markup_printf_escaped ("%s<b>%s</b>%s",
                    parts[0], skey, parts[1]);
            g_strfreev (parts);
            g_free (skey);
        }
        if (!desc_uri)
            desc_uri = g_markup_escape_text (uri, -1);
        g_free (temp);
    }

    if (G_LIKELY (title))
    {
        temp = g_utf8_strdown (title, -1);
        if ((start = strstr (temp, key)) && (start - temp))
        {
            skey = g_strndup (title + (start - temp), len);
            parts = g_strsplit (title, skey, 2);
            if (parts[0] && parts[1])
                desc_title = g_markup_printf_escaped ("%s<b>%s</b>%s",
                    parts[0], skey, parts[1]);
            g_strfreev (parts);
            g_free (skey);
        }
        if (!desc_title)
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

static void
midori_location_action_entry_changed_cb (GtkComboBox*          combo_box,
                                         MidoriLocationAction* location_action)
{
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
        GtkIconEntry* entry;

        if ((entry = GTK_ICON_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box)))))
        {
            GtkTreeModel* model;
            gchar* uri;
            #if !HAVE_HILDON
            GdkPixbuf* pixbuf;
            #endif

            model = gtk_combo_box_get_model (combo_box);
            gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);

            #if !HAVE_HILDON
            gtk_tree_model_get (model, &iter, FAVICON_COL, &pixbuf, -1);
            gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (entry),
                                                 GTK_ICON_ENTRY_PRIMARY, pixbuf);
            g_object_unref (pixbuf);
            #endif
            katze_assign (location_action->text, uri);
            katze_assign (location_action->uri, g_strdup (uri));

            g_signal_emit (location_action, signals[ACTIVE_CHANGED], 0,
                           gtk_combo_box_get_active (combo_box));
        }
    }
}

static void
midori_location_action_entry_popup_cb (GtkComboBox*          combo_box,
                                       MidoriLocationAction* location_action)
{
    GtkListStore* store;
    gint result;
    const gchar* sqlcmd;
    static sqlite3_stmt* stmt;
    gint matches;

    store = GTK_LIST_STORE (gtk_combo_box_get_model (combo_box));
    gtk_list_store_clear (store);

    if (!stmt)
    {
        sqlite3* db;
        db = g_object_get_data (G_OBJECT (location_action->history), "db");
        sqlcmd = "SELECT uri, title FROM history"
                 " GROUP BY uri ORDER BY count() DESC LIMIT ?";
        sqlite3_prepare_v2 (db, sqlcmd, -1, &stmt, NULL);
    }

    sqlite3_bind_int64 (stmt, 1, MAX_ITEMS);
    result = sqlite3_step (stmt);
    if (result != SQLITE_ROW)
    {
        g_print (_("Failed to execute database statement\n"));
        sqlite3_reset (stmt);
        sqlite3_clear_bindings (stmt);
        return;
    }

    matches = 0;
    do
    {
        const unsigned char* uri = sqlite3_column_text (stmt, 0);
        const unsigned char* title = sqlite3_column_text (stmt, 1);
        GdkPixbuf* icon = katze_load_cached_icon ((gchar*)uri, NULL);
        if (!icon)
            icon = location_action->default_icon;
        gtk_list_store_insert_with_values (store, NULL, matches,
            URI_COL, uri, TITLE_COL, title, YALIGN_COL, 0.25,
            FAVICON_COL, icon, -1);
        matches++;
        result = sqlite3_step (stmt);
    }
    while (result == SQLITE_ROW);
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
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
        GtkTreeModel* model = midori_location_action_create_model ();

        gtk_icon_entry_set_progress_fraction (GTK_ICON_ENTRY (child),
            MIDORI_LOCATION_ACTION (action)->progress);
        gtk_combo_box_set_model (GTK_COMBO_BOX (entry), model);
        #if GTK_CHECK_VERSION (2, 14, 0)
        gtk_combo_box_set_button_sensitivity (GTK_COMBO_BOX (entry),
            GTK_SENSITIVITY_ON);
        #endif
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
            renderer, midori_location_entry_render_text_cb, action, NULL);

        gtk_combo_box_set_active (GTK_COMBO_BOX (entry), -1);
        if (location_action->history)
            gtk_container_forall (GTK_CONTAINER (entry),
                (GtkCallback)midori_location_action_toggle_arrow_cb, action);
        g_signal_connect (entry, "changed",
            G_CALLBACK (midori_location_action_entry_changed_cb), action);
        g_signal_connect (entry, "popup",
            G_CALLBACK (midori_location_action_entry_popup_cb), action);

        g_object_connect (child,
                      "signal::changed",
                      midori_location_action_changed_cb, action,
                      "signal::move-cursor",
                      midori_location_action_move_cursor_cb, action,
                      "signal-after::backspace",
                      midori_location_action_backspace_cb, action,
                      "signal-after::paste-clipboard",
                      midori_location_action_paste_clipboard_cb, action,
                      "signal::button-press-event",
                      midori_location_action_button_press_event_cb, action,
                      "signal::key-press-event",
                      midori_location_action_key_press_event_cb, action,
                      #if GTK_CHECK_VERSION (2, 19, 3)
                      "signal-after::preedit-changed",
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
    GdkPixbuf* icon;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (text != NULL);

    midori_location_action_popdown_completion (location_action);

    katze_assign (location_action->text, g_strdup (text));
    katze_assign (location_action->uri, g_strdup (text));

    if (!(proxies = gtk_action_get_proxies (GTK_ACTION (location_action))))
        return;

    if (!(icon = katze_load_cached_icon (location_action->uri, NULL)))
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

    g_object_unref (icon);
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

    katze_assign (location_action->uri, g_strdup (uri));

    midori_location_action_toggle_arrow (location_action);
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

    #if !HAVE_HILDON
    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

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

    #if !HAVE_HILDON
    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

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
    /* Nothing to do */
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
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    if (search_engines)
        g_object_ref (search_engines);

    katze_object_assign (location_action->search_engines, search_engines);
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
    /* Nothing to do */
}

void
midori_location_action_clear (MidoriLocationAction* location_action)
{
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    midori_location_action_toggle_arrow (location_action);
}

/**
 * midori_location_action_set_security_hint:
 * @location_action: a #MidoriLocationAction
 * @hint: a security hint
 *
 * Sets a security hint on the action, so that the security status
 * can be reflected visually.
 *
 * Since: 0.2.5
 **/
void
midori_location_action_set_security_hint (MidoriLocationAction* location_action,
                                          MidoriSecurity        hint)
{
    GSList* proxies;
    GtkWidget* entry;
    GtkWidget* child;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        GdkColor bg_color = { 0, 1 };
        GdkColor fg_color = { 0, 1 };

        entry = midori_location_action_entry_for_proxy (proxies->data);
        child = gtk_bin_get_child (GTK_BIN (entry));

        if (hint == MIDORI_SECURITY_UNKNOWN)
        {
            gdk_color_parse ("#ef7070", &bg_color);
            gdk_color_parse ("#000", &fg_color);
            #if !HAVE_HILDON
            gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (child),
                GTK_ICON_ENTRY_SECONDARY, GTK_STOCK_INFO);
            #endif
        }
        else if (hint == MIDORI_SECURITY_TRUSTED)
        {
            gdk_color_parse ("#fcf19a", &bg_color);
            gdk_color_parse ("#000", &fg_color);
            #if !HAVE_HILDON
            gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (child),
                GTK_ICON_ENTRY_SECONDARY, GTK_STOCK_DIALOG_AUTHENTICATION);
            #endif
        }

        gtk_widget_modify_base (child, GTK_STATE_NORMAL,
            bg_color.red == 1 ? NULL : &bg_color);
        gtk_widget_modify_text (child, GTK_STATE_NORMAL,
            bg_color.red == 1 ? NULL : &fg_color);
    }
}
