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

#include "marshal.h"
#include "midori-browser.h"
#include "midori-searchaction.h"
#include "midori-platform.h"
#include <midori/midori-core.h>

#include "config.h"
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

    /**
     * MidoriLocationAction:active-changed:
     * @row: the active row
     *
     * The active-changed signal is emitted when the active row changes.
     *
     * Deprecated: 0.3.4
     */
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
midori_location_action_popup_position (MidoriLocationAction* action,
                                       gint                  matches)
{
    GtkWidget* popup = action->popup;
    GtkWidget* widget = action->entry;
    GdkWindow* window = gtk_widget_get_window (widget);
    gint wx, wy, x_border, y_border, items;
    GtkRequisition menu_req;
    GtkRequisition widget_req;
    GdkScreen* screen;
    gint monitor_num;
    GdkRectangle monitor;
    GtkAllocation alloc;
    gint height, sep, width, toplevel_height;
    gboolean above;
    GtkWidget* scrolled = gtk_widget_get_parent (action->treeview);
    GtkWidget* toplevel;
    GtkTreePath* path;

    if (!window)
        return;

    gtk_widget_get_allocation (widget, &alloc);
    #if GTK_CHECK_VERSION (3, 0, 0)
    gtk_widget_get_preferred_size (widget, &widget_req, NULL);
    #else
    gtk_widget_size_request (widget, &widget_req);
    #endif
    gdk_window_get_origin (window, &wx, &wy);

    #if GTK_CHECK_VERSION (3, 0, 0)
    wx += alloc.x;
    wy += alloc.y + (alloc.height - widget_req.height) / 2;
    #endif
    /* _gtk_entry_get_borders (GTK_ENTRY (widget), &x_border, &y_border); */
    x_border = y_border = 0;

    gtk_tree_view_column_cell_get_size (
        gtk_tree_view_get_column (GTK_TREE_VIEW (action->treeview), 0),
        NULL, NULL, NULL, NULL, &height);
    gtk_widget_style_get (action->treeview, "vertical-separator", &sep, NULL);
    height += sep;

    /* Constrain to screen/ window size */
    screen = gtk_widget_get_screen (widget);
    monitor_num = gdk_screen_get_monitor_at_window (screen, window);
    gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
    toplevel = gtk_widget_get_toplevel (widget);
    gtk_window_get_size (GTK_WINDOW (toplevel), NULL, &toplevel_height);
    toplevel_height = MIN (toplevel_height, monitor.height);
    if (wy > toplevel_height / 2)
        items = MIN (matches, ((monitor.y + wy) / height) - 1);
    else
        items = MIN (matches, ((toplevel_height - wy) / height) - 1);
    width = MIN (alloc.width, monitor.width) - 2 * x_border;

    gtk_tree_view_columns_autosize (GTK_TREE_VIEW (action->treeview));
    #if GTK_CHECK_VERSION (3, 0, 0)
    gtk_widget_set_size_request (scrolled, width, -1);
    gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolled), width);
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled), items * height);
    gtk_widget_get_preferred_size (popup, &menu_req, NULL);
    #else
    gtk_widget_set_size_request (scrolled, width, items * height);
    gtk_widget_size_request (popup, &menu_req);
    #endif

    if (wx < monitor.x)
        wx = monitor.x;
    else if (wx + menu_req.width > monitor.x + monitor.width)
        wx = monitor.x + monitor.width - menu_req.width;

    if (wy + widget_req.height + menu_req.height <= monitor.y + monitor.height ||
        wy - monitor.y < (monitor.y + monitor.height) - (wy + widget_req.height))
    {
        wy += widget_req.height;
        above = FALSE;
    }
    else
    {
        wy -= menu_req.height;
        above = TRUE;
    }

    path = gtk_tree_path_new_from_indices (above ? matches - 1 : 0, -1);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (action->treeview), path,
                                  NULL, FALSE, 0.0, 0.0);
    gtk_tree_path_free (path);

    gtk_window_move (GTK_WINDOW (popup), wx, wy);
}

static int
midori_location_action_add_search_engines (MidoriLocationAction* action,
                                           GtkListStore*         store,
                                           gint                  matches)
{
    KatzeItem* item;
    gint i = 0;
    #ifndef G_OS_WIN32
    GtkStyle* style;
    #endif

    gtk_widget_realize (action->treeview);
    #ifndef G_OS_WIN32
    style = gtk_widget_get_style (action->treeview);
    #endif

    /* FIXME: choose 3 most frequently except for default */
    KATZE_ARRAY_FOREACH_ITEM (item, action->search_engines)
    {
        gchar* uri;
        gchar* title;
        const gchar* text;
        gchar* desc;
        GdkPixbuf* icon;

        uri = midori_uri_for_search (katze_item_get_uri (item), action->key);
        title = g_strdup_printf (_("Search with %s"), katze_item_get_name (item));
        text = katze_item_get_text (item);
        desc = g_strdup_printf ("%s\n%s", title, text ? text : uri);
        icon = midori_search_action_get_icon (item, action->treeview, NULL, FALSE);
        gtk_list_store_insert_with_values (store, NULL, matches + i,
            URI_COL, uri, TITLE_COL, desc, YALIGN_COL, 0.25,
            #ifndef G_OS_WIN32
            BACKGROUND_COL, style ? &style->bg[GTK_STATE_NORMAL] : NULL,
            #endif
            STYLE_COL, 1, FAVICON_COL, icon, -1);
        g_free (uri);
        g_free (title);
        g_free (desc);
        if (icon != NULL)
            g_object_unref (icon);
        i++;

        if (i > 2 && matches > 0)
        {
            gtk_list_store_insert_with_values (store, NULL, matches + i,
                URI_COL, "about:search", TITLE_COL, _("Search with…"),
                YALIGN_COL, 0.25,
                #ifndef G_OS_WIN32
                BACKGROUND_COL, style ? &style->bg[GTK_STATE_NORMAL] : NULL,
                #endif
                STYLE_COL, 1, FAVICON_COL, NULL, -1);
            i++;
            break;
        }
    }
    return i;
}

static void
midori_location_action_complete (MidoriLocationAction* action,
                                 gboolean              new_tab,
                                 const gchar*          uri)
{
    if (!strcmp (uri, "about:search"))
    {
        GtkListStore* store = GTK_LIST_STORE (action->completion_model);
        gtk_list_store_clear (store);
        midori_location_action_popup_position (action,
            midori_location_action_add_search_engines (action, store, 0));
    }
    else
    {
        midori_location_action_popdown_completion (action);
        gtk_entry_set_text (GTK_ENTRY (action->entry), uri);
        g_signal_emit (action, signals[SUBMIT_URI], 0, uri, new_tab);
    }
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
        gtk_tree_model_get (action->completion_model, &iter, URI_COL, &uri, -1);
        midori_location_action_complete (action,
                MIDORI_MOD_NEW_TAB (event->state), uri);
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
    gchar* effective_key;
    gint i;
    gint result;
    static sqlite3_stmt* stmt;
    const gchar* sqlcmd;
    gint matches, searches;

    if (!action->entry || !gtk_widget_has_focus (action->entry) || !action->history)
        return FALSE;

    /* No completion when typing a search token */
    if (action->search_engines
     && katze_array_find_token (action->search_engines, action->key))
    {
        midori_location_action_popdown_completion (action);
        return FALSE;
    }

    /* Empty string or starting with a space means: no completion */
    if (!(action->key && *action->key && *action->key != ' '))
    {
        midori_location_action_popdown_completion (action);
        return FALSE;
    }

    if (!stmt)
    {
        sqlite3* db;
        db = g_object_get_data (G_OBJECT (action->history), "db");

        if (!db)
            return FALSE;

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
    effective_key = g_strdup_printf ("%%%s%%", action->key);
    i = 0;
    do
    {
        if (effective_key[i] == ' ')
            effective_key[i] = '%';
        i++;
    }
    while (effective_key[i] != '\0');
    sqlite3_bind_text (stmt, 1, effective_key, -1, g_free);
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
        /* Window managers may ignore programmatic resize without this */
        gtk_window_set_resizable (GTK_WINDOW (popup), FALSE);
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
        gtk_cell_renderer_text_set_fixed_height_from_font (
            GTK_CELL_RENDERER_TEXT (renderer), 2);
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
    while (result == SQLITE_ROW)
    {
        sqlite3_int64 type = sqlite3_column_int64 (stmt, 0);
        const unsigned char* uri = sqlite3_column_text (stmt, 1);
        const unsigned char* title = sqlite3_column_text (stmt, 2);
        GdkPixbuf* icon = katze_load_cached_icon ((gchar*)uri, NULL);
        if (!icon)
            icon = g_object_ref (action->default_icon);
        if (type == 1 /* history_view */)
        {
            gtk_list_store_insert_with_values (store, NULL, matches,
                URI_COL, uri, TITLE_COL, title, YALIGN_COL, 0.25,
                FAVICON_COL, icon, -1);
        }
        else if (type == 2 /* search_view */)
        {
            gchar* search_title = g_strdup_printf (_("Search for %s"), title);
            gchar* search_desc = g_strdup_printf ("%s\n%s", search_title, uri);
            gtk_list_store_insert_with_values (store, NULL, matches,
                URI_COL, uri, TITLE_COL, search_desc, YALIGN_COL, 0.25,
                STYLE_COL, 1, FAVICON_COL, icon, -1);
            g_free (search_desc);
            g_free (search_title);
        }
        if (icon != NULL)
            g_object_unref (icon);

        matches++;
        result = sqlite3_step (stmt);
    }

    if (stmt)
    {
        sqlite3_reset (stmt);
        sqlite3_clear_bindings (stmt);
    }

    if (action->search_engines)
        searches += midori_location_action_add_search_engines (action, store, matches);

    if (!gtk_widget_get_visible (action->popup))
    {
        GtkWidget* toplevel = gtk_widget_get_toplevel (action->entry);
        gtk_window_set_screen (GTK_WINDOW (action->popup),
                               gtk_widget_get_screen (action->entry));
        gtk_window_set_transient_for (GTK_WINDOW (action->popup), GTK_WINDOW (toplevel));
        gtk_widget_show_all (action->popup);
    }

    midori_location_action_popup_position (action, matches + searches);

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

static void
midori_location_action_entry_drag_data_get_cb (GtkWidget*        entry,
                                               GdkDragContext*   context,
                                               GtkSelectionData* data,
                                               guint             info,
                                               guint32           time,
                                               GtkAction*        action)
{
    if (gtk_entry_get_current_icon_drag_source (GTK_ENTRY (entry)) == GTK_ENTRY_ICON_PRIMARY)
    {
        const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
        gchar** uris = g_strsplit (uri, uri, 1);
        gtk_selection_data_set_uris (data, uris);
        g_strfreev (uris);
    }
}

static GtkWidget*
midori_location_action_create_tool_item (GtkAction* action)
{
    GtkWidget* toolitem;
    GtkWidget* alignment;
    GtkWidget* entry;

    GtkTargetList *targetlist;

    toolitem = GTK_WIDGET (gtk_tool_item_new ());
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);

    alignment = gtk_alignment_new (0.0f, 0.5f, 1.0f, 0.1f);
    gtk_widget_show (alignment);
    gtk_container_add (GTK_CONTAINER (toolitem), alignment);

    entry = gtk_icon_entry_new ();
    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_PRIMARY, TRUE);
    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_SECONDARY, TRUE);

    targetlist = gtk_target_list_new (NULL, 0);
    gtk_target_list_add_uri_targets (targetlist, 0);
    gtk_entry_set_icon_drag_source (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY, targetlist, GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK);
    gtk_target_list_unref (targetlist);
    g_signal_connect (entry, "drag-data-get",
        G_CALLBACK (midori_location_action_entry_drag_data_get_cb), action);
    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (alignment), entry);

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
    case GDK_KEY_ISO_Enter:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
        is_enter = TRUE;
    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
        if (location_action->popup && gtk_widget_get_visible (location_action->popup))
        {
            GtkTreeModel* model = location_action->completion_model;
            GtkTreeIter iter;
            gint selected = location_action->completion_index;
            if (selected > -1 &&
                gtk_tree_model_iter_nth_child (model, &iter, NULL, selected))
            {
                gchar* uri;
                gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);

                if (is_enter)
                    midori_location_action_complete (location_action,
                            MIDORI_MOD_NEW_TAB (event->state), uri);
                else
                {
                    midori_location_action_popdown_completion (location_action);
                    gtk_entry_set_text (entry, uri);
                }

                g_free (uri);
                return TRUE;
            }
            midori_location_action_popdown_completion (location_action);
        }

        if (is_enter && (text = gtk_entry_get_text (entry)) && *text)
            g_signal_emit (action, signals[SUBMIT_URI], 0, text,
                           MIDORI_MOD_NEW_TAB (event->state));
        break;
    case GDK_KEY_Escape:
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
    case GDK_KEY_Delete:
    case GDK_KEY_KP_Delete:
    {
        gint selected = location_action->completion_index;
        GtkTreeModel* model = location_action->completion_model;
        GtkTreeIter iter;

        if (selected > -1 &&
            gtk_tree_model_iter_nth_child (model, &iter, NULL, selected))
            {
                gchar* uri;
                gchar* sqlcmd;
                sqlite3* db;
                gchar* errmsg;
                gint result;

                gtk_tree_model_get (model, &iter, URI_COL, &uri, -1);
                sqlcmd = sqlite3_mprintf ("DELETE FROM history "
                                          "WHERE uri = '%q'", uri);
                g_free (uri);
                db = g_object_get_data (G_OBJECT (location_action->history), "db");
                result = sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg);
                sqlite3_free (sqlcmd);
                if (result == SQLITE_ERROR)
                {
                    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                        URI_COL, errmsg, -1);
                    sqlite3_free (errmsg);
                    break;
                }
                if (result != SQLITE_OK || sqlite3_changes (db) == 0)
                    break;
                if (!gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
                {
                    midori_location_action_popdown_completion (location_action);
                    break;
                }
                /* Fall through to advance the selection */
            }
        else
            break;
    }
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
    case GDK_KEY_Tab:
    case GDK_KEY_ISO_Left_Tab:
    case GDK_KEY_Page_Down:
    case GDK_KEY_Page_Up:
    {
        if ((event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_Page_Down) &&
           !(location_action->popup && gtk_widget_get_visible (location_action->popup)))
            return TRUE;
        if (location_action->popup && gtk_widget_get_visible (location_action->popup))
        {
            GtkTreeModel* model = location_action->completion_model;
            gint matches = gtk_tree_model_iter_n_children (model, NULL);
            GtkTreePath* path;
            GtkTreeIter iter;
            gint selected = location_action->completion_index;

            if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down
             || ((event->keyval == GDK_KEY_Tab  || event->keyval == GDK_KEY_ISO_Left_Tab)
             && !(event->state & GDK_SHIFT_MASK)))
            {
                selected = selected + 1;
                if (selected == matches)
                    selected = -1;
            }
            else if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up
                  || ((event->keyval == GDK_KEY_Tab  || event->keyval == GDK_KEY_ISO_Left_Tab)
                  && (event->state & GDK_SHIFT_MASK)))
            {
                if (selected == -1)
                    selected = matches - 1;
                else
                    selected = selected - 1;
            }
            else if (event->keyval == GDK_KEY_Page_Down)
            {
                if (selected == -1)
                    selected = 0;
                else if (selected < matches - 1)
                    selected = MIN (selected + 14, matches -1);
                else
                    selected = -1;
            }
            else if (event->keyval == GDK_KEY_Page_Up)
            {
                if (selected == -1)
                    selected = matches - 1;
                else if (selected > 0)
                    selected = MAX (selected - 14, 0);
                else
                    selected = -1;
            }
            else if (event->keyval != GDK_KEY_KP_Delete && event->keyval != GDK_KEY_Delete)
                g_assert_not_reached ();

            if (selected != -1)
            {
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
            }
            else
                gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (location_action->treeview)));

            location_action->completion_index = selected;
            return TRUE;
        }

        /* Allow Tab to handle focus if the popup is closed */
        if (event->keyval == GDK_KEY_Tab  || event->keyval == GDK_KEY_ISO_Left_Tab)
            return FALSE;
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

#ifdef HAVE_GCR
    #define GCR_API_SUBJECT_TO_CHANGE
    #include <gcr/gcr.h>
#endif

#if defined (HAVE_LIBSOUP_2_34_0)
static GHashTable* message_map = NULL;
void
midori_map_add_message (SoupMessage* message)
{
    SoupURI* uri = soup_message_get_uri (message);
    if (message_map == NULL)
        message_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    g_return_if_fail (uri && uri->host);
    g_hash_table_insert (message_map, g_strdup (uri->host), g_object_ref (message));
}

SoupMessage*
midori_map_get_message (SoupMessage* message)
{
    SoupURI* uri = soup_message_get_uri (message);
    SoupMessage* full;
    g_return_val_if_fail (uri && uri->host, message);
    full = g_hash_table_lookup (message_map, uri->host);
    g_return_val_if_fail (full, message);
    return full;
}

#ifdef HAVE_GCR
typedef enum {
    MIDORI_CERT_TRUST,
    MIDORI_CERT_REVOKE,
    MIDORI_CERT_EXPORT,
} MidoriCertTrust;

static void
midori_location_action_cert_response_cb (GtkWidget*      dialog,
                                         gint            response,
                                         GcrCertificate* gcr_cert)
{
    gchar* peer = g_object_get_data (G_OBJECT (gcr_cert), "peer");
    GError* error = NULL;
    if (response == MIDORI_CERT_TRUST)
        gcr_trust_add_pinned_certificate (gcr_cert, GCR_PURPOSE_SERVER_AUTH, peer, NULL, &error);
    else if (response == MIDORI_CERT_REVOKE)
        gcr_trust_remove_pinned_certificate (gcr_cert, GCR_PURPOSE_SERVER_AUTH, peer, NULL, &error);
    else if (response == MIDORI_CERT_EXPORT)
    {
        /* FIXME: Would be nice if GcrCertificateExporter became public */
        gchar* filename = g_strconcat (peer, ".crt", NULL);
        GtkWidget* dialog = sokoke_file_chooser_dialog_new (_("Export certificate"),
            NULL, GTK_FILE_CHOOSER_ACTION_SAVE);
        gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
        g_free (filename);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
        {
            gsize n_data;
            gconstpointer data = gcr_certificate_get_der_data (gcr_cert, &n_data);
            g_return_if_fail (data);
            filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
            g_file_set_contents (filename, data, n_data, NULL);
            g_free (filename);
        }
        gtk_widget_destroy (dialog);
    }
    if (error != NULL)
    {
        g_warning ("Error %s trust: %s", response == MIDORI_CERT_TRUST ?
                   "granting" : "revoking", error->message);
        g_error_free (error);
    }
    gtk_widget_destroy (dialog);
}
#endif

const gchar*
midori_location_action_tls_flags_to_string (GTlsCertificateFlags tls_flags)
{
    const gchar* tls_error;
    if (tls_flags & G_TLS_CERTIFICATE_UNKNOWN_CA)
        tls_error = _("The signing certificate authority is not known.");
    else if (tls_flags & G_TLS_CERTIFICATE_BAD_IDENTITY)
        tls_error = _("The certificate does not match the expected identity of the site that it was retrieved from.");
    else if(tls_flags & G_TLS_CERTIFICATE_NOT_ACTIVATED)
        tls_error = _("The certificate's activation time is still in the future.");
    else if (tls_flags & G_TLS_CERTIFICATE_EXPIRED)
        tls_error = _("The certificate has expired");
    else if (tls_flags & G_TLS_CERTIFICATE_REVOKED)
        tls_error = _("The certificate has been revoked according to the GTlsConnection's certificate revocation list.");
    else if (tls_flags & G_TLS_CERTIFICATE_INSECURE)
        tls_error = _("The certificate's algorithm is considered insecure.");
    else if (tls_flags & G_TLS_CERTIFICATE_GENERIC_ERROR)
        tls_error = _("Some other error occurred validating the certificate.");
    else
        tls_error = "Unknown GTLSCertificateFlags value";
    return tls_error;
}

void
midori_location_action_show_page_info (GtkWidget* widget,
                                       GtkBox*    box,
                                       GtkWidget* dialog)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (widget);
    MidoriView* view = MIDORI_VIEW (midori_browser_get_current_tab (browser));
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (view));
    WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (web_view);
    WebKitWebDataSource* source = webkit_web_frame_get_data_source (web_frame);
    WebKitNetworkRequest* request = webkit_web_data_source_get_request (source);
    SoupMessage* message = midori_map_get_message (webkit_network_request_get_message (request));
    GTlsCertificate* tls_cert;
    GTlsCertificateFlags tls_flags;

    g_return_if_fail (message);
    g_object_get (message, "tls-certificate", &tls_cert, "tls-errors", &tls_flags, NULL);

    if (tls_cert == NULL)
        return;

    #ifdef HAVE_GCR
    GByteArray* der_cert;
    GcrCertificate* gcr_cert;
    GtkWidget* details;
    SoupURI* uri = soup_message_get_uri (message);

    g_object_get (tls_cert, "certificate", &der_cert, NULL);
    gcr_cert = gcr_simple_certificate_new (
        der_cert->data, der_cert->len);
    g_byte_array_unref (der_cert);
    details = (GtkWidget*)gcr_certificate_details_widget_new (gcr_cert);
    gtk_widget_show (details);
    gtk_container_add (GTK_CONTAINER (box), details);
    if (gcr_trust_is_certificate_pinned (gcr_cert, GCR_PURPOSE_SERVER_AUTH, uri->host, NULL, NULL))
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
            ("_Don't trust this website"), MIDORI_CERT_REVOKE, NULL);
    else if (tls_flags > 0)
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
            ("_Trust this website"), MIDORI_CERT_TRUST, NULL);
    gtk_container_child_set (GTK_CONTAINER (gtk_dialog_get_action_area (GTK_DIALOG (dialog))),
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Export certificate"), MIDORI_CERT_EXPORT),
        "secondary", TRUE, NULL);

    g_object_set_data_full (G_OBJECT (gcr_cert), "peer", g_strdup (uri->host), (GDestroyNotify)g_free);
    g_object_set_data_full (G_OBJECT (dialog), "gcr-cert", gcr_cert, (GDestroyNotify)g_object_unref);
    g_signal_connect (dialog, "response",
        G_CALLBACK (midori_location_action_cert_response_cb), gcr_cert);
    /* With GTK+2 the scrolled contents can't communicate a natural size to the window */
    #if !GTK_CHECK_VERSION (3, 0, 0)
    gtk_window_set_default_size (GTK_WINDOW (dialog), 250, 200);
    #endif
    #else
    const gchar* tls_error = midori_location_action_tls_flags_to_string (tls_flags);

    if (!g_tls_certificate_get_issuer (tls_cert))
        gtk_box_pack_start (box, gtk_label_new (_("Self-signed")), FALSE, FALSE, 0);

    gtk_box_pack_start (box, gtk_label_new (tls_error), FALSE, FALSE, 0);
    #endif

    g_object_unref (tls_cert);
}
#endif

#ifndef HAVE_GRANITE
static gboolean
midori_location_action_dialog_focus_out_cb (GtkWidget* dialog,
                                            GdkEvent*  event,
                                            gpointer   user_data)
{
    gtk_widget_destroy (dialog);
    return TRUE;
}
#endif

static void
midori_location_action_icon_released_cb (GtkWidget*           widget,
                                         GtkIconEntryPosition icon_pos,
                                         gint                 button,
                                         GtkAction*           action)
{
    /* The dialog should "toggle" like a menu, as far as users go
       FIXME: Half-working: the dialog closes but re-opens */
    static GtkWidget* dialog = NULL;
    if (icon_pos == GTK_ICON_ENTRY_PRIMARY && dialog != NULL)
        gtk_widget_destroy (dialog);

    if (icon_pos == GTK_ICON_ENTRY_PRIMARY)
    {
        const gchar* title = _("Security details");
        GtkWidget* content_area;
        GtkWidget* hbox;
        #ifdef HAVE_GRANITE
        gint root_x, root_y;
        GdkRectangle icon_rect;
        /* FIXME: granite: should return GtkWidget* like GTK+ */
        dialog = (GtkWidget*)granite_widgets_pop_over_new ();
        gchar* markup = g_strdup_printf ("<b>%s</b>", title);
        GtkWidget* label = gtk_label_new (markup);
        content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        g_free (markup);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 0);
        gtk_entry_get_icon_area (GTK_ENTRY (widget), icon_pos, &icon_rect);
        gdk_window_get_root_coords (gtk_widget_get_window (widget),
            icon_rect.x + icon_rect.width / 2, icon_rect.y + icon_rect.height,
            &root_x, &root_y);
        granite_widgets_pop_over_move_to_coords (GRANITE_WIDGETS_POP_OVER (dialog),
            root_x, root_y, TRUE);
        #else
        dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (gtk_widget_get_toplevel (widget)),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR, NULL, NULL);
        /* FIXME: check focus-in on the transient-for window instead of
                  focus-out-event */
        g_signal_connect (dialog, "focus-out-event",
            G_CALLBACK (midori_location_action_dialog_focus_out_cb), NULL);
        content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        #endif
        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_image_new_from_gicon (
            gtk_entry_get_icon_gicon (GTK_ENTRY (widget), icon_pos), GTK_ICON_SIZE_DIALOG), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox),
            gtk_label_new (gtk_icon_entry_get_tooltip (GTK_ICON_ENTRY (widget), icon_pos)), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 0);
        #if defined (HAVE_LIBSOUP_2_34_0)
        midori_location_action_show_page_info (widget, GTK_BOX (content_area), dialog);
        #endif
        g_signal_connect (dialog, "destroy", G_CALLBACK (gtk_widget_destroyed), &dialog);
        gtk_widget_show_all (dialog);
    }
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
    gchar* uri_escaped;
    gchar* uri_temp;
    gchar* uri;
    gchar* title;
    gboolean style;
    gchar* desc;
    gchar* desc_uri;
    gchar* desc_iter;
    gchar* temp_iter;
    gchar* desc_title;
    const gchar* str;
    gchar* key;
    gchar** keys;
    gint key_idx;
    gchar* start;
    gchar* skey;
    gchar* temp;
    gchar* temp_concat;
    gchar* temp_markup;
    gchar** parts;
    size_t offset;

    gtk_tree_model_get (model, iter, URI_COL, &uri_escaped, TITLE_COL, &title,
        STYLE_COL, &style, -1);

    if (style) /* A search engine action */
    {
        g_object_set (renderer, "text", title,
            "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        g_free (uri_escaped);
        g_free (title);
        return;
    }

    desc = desc_uri = desc_title = key = NULL;
    if (action->key)
        str = action->key;
    else
        str = "";

    key = g_utf8_strdown (str, -1);
    keys = g_strsplit_set (key, " %", -1);
    g_free (key);

    uri_temp = midori_uri_unescape (uri_escaped);
    g_free (uri_escaped);
    uri = g_strescape (uri_temp, NULL);
    g_free (uri_temp);

    if (G_LIKELY (uri))
    {
        /* Strip URI scheme and www. for display to reduce visual noise */
        gchar* stripped_uri = uri;
        if (g_str_has_prefix (uri, "http://"))
            stripped_uri = &uri[7];
        else if (g_str_has_prefix (uri, "https://"))
            stripped_uri = &uri[8];
        else if (g_str_has_prefix (uri, "file://"))
            stripped_uri = &uri[7];
        if (g_str_has_prefix (stripped_uri, "www."))
            stripped_uri = &stripped_uri[4];

        temp_iter = temp = g_utf8_strdown (stripped_uri, -1);
        desc_iter = stripped_uri;
        key_idx = 0;
        key = keys[key_idx];
        offset = 0;
        while (key && (start = strstr (temp_iter, key)) && start)
        {
            gsize len = strlen (key);
            if (len)
            {
                offset = (start - temp_iter);
                skey = g_strndup (desc_iter + offset, len);
                parts = g_strsplit (desc_iter, skey, 2);
                if (parts[0] && parts[1])
                {
                    if (desc_uri)
                    {
                        temp_markup = g_markup_printf_escaped ("%s<b>%s</b>",
                            parts[0], skey);
                        temp_concat = g_strconcat (desc_uri, temp_markup, NULL);
                        g_free (temp_markup);
                        katze_assign (desc_uri, temp_concat);
                    }
                    else
                    {
                        desc_uri = g_markup_printf_escaped ("%s<b>%s</b>",
                            parts[0], skey);
                    }
                }
                g_strfreev (parts);
                g_free (skey);

                offset += len;
                temp_iter += offset;
                desc_iter += offset;
            }
            key_idx++;
            key = keys[key_idx];
            if (key == NULL)
                break;
        }
        if (key)
            katze_assign (desc_uri, NULL);
        if (desc_uri)
        {
            temp_markup = g_markup_escape_text (desc_iter, -1);
            temp_concat = g_strconcat (desc_uri, temp_markup, NULL);
            g_free (temp_markup);
            katze_assign (desc_uri, temp_concat);
        }
        else
            desc_uri = g_markup_escape_text (stripped_uri, -1);
        g_free (temp);
    }

    if (G_LIKELY (title))
    {
        temp_iter = temp = g_utf8_strdown (title, -1);
        desc_iter = title;
        key_idx = 0;
        key = keys[key_idx];
        offset = 0;
        while (key && (start = strstr (temp_iter, key)) && start)
        {
            gsize len = strlen (key);
            if (len)
            {
                offset = (start - temp_iter);
                skey = g_strndup (desc_iter + offset, len);
                parts = g_strsplit (desc_iter, skey, 2);
                if (parts[0] && parts[1])
                {
                    if (desc_title)
                    {
                        temp_markup = g_markup_printf_escaped ("%s<b>%s</b>",
                            parts[0], skey);
                        temp_concat = g_strconcat (desc_title, temp_markup, NULL);
                        g_free (temp_markup);
                        katze_assign (desc_title, temp_concat);
                    }
                    else
                    {
                        desc_title = g_markup_printf_escaped ("%s<b>%s</b>",
                            parts[0], skey);
                    }
                }
                g_strfreev (parts);
                g_free (skey);

                offset += len;
                temp_iter += offset;
                desc_iter += offset;
            }
            key_idx++;
            key = keys[key_idx];
            if (key == NULL)
                break;
        }
        if (key)
            katze_assign (desc_title, NULL);
        if (desc_title)
        {
            temp_markup = g_markup_escape_text (desc_iter, -1);
            temp_concat = g_strconcat (desc_title, temp_markup, NULL);
            g_free (temp_markup);
            katze_assign (desc_title, temp_concat);
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
    g_strfreev (keys);
    g_free (desc);
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
    GtkClipboard* clipboard = gtk_clipboard_get_for_display (
        gtk_widget_get_display (entry),GDK_SELECTION_CLIPBOARD);

    menuitem = gtk_separator_menu_item_new ();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (menu, menuitem);
    menuitem = sokoke_action_create_popup_menu_item (
        gtk_action_group_get_action (actions, "ManageSearchEngines"));
    gtk_menu_shell_append (menu, menuitem);
    /* i18n: Right-click on Location, Open an URL from the clipboard */
    menuitem = gtk_menu_item_new_with_mnemonic (_("Paste and p_roceed"));
    gtk_widget_show (menuitem);
    /* Insert menu item after default Paste menu item */
    gtk_menu_shell_insert (menu, menuitem, 3);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_location_action_paste_proceed_cb), location_action);
    if (!gtk_clipboard_wait_is_text_available (clipboard))
        gtk_widget_set_sensitive (menuitem, FALSE);
}

static void
midori_location_action_connect_proxy (GtkAction* action,
                                      GtkWidget* proxy)
{
    MidoriLocationAction* location_action;

    GTK_ACTION_CLASS (midori_location_action_parent_class)->connect_proxy (
        action, proxy);

    location_action = MIDORI_LOCATION_ACTION (action);
    katze_object_assign (location_action->default_icon,
                         gtk_widget_render_icon (proxy, GTK_STOCK_FILE,
                                                 GTK_ICON_SIZE_MENU, NULL));

    if (GTK_IS_TOOL_ITEM (proxy))
    {
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxy);
        gtk_icon_entry_set_progress_fraction (GTK_ICON_ENTRY (entry),
            MIDORI_LOCATION_ACTION (action)->progress);

        g_object_connect (entry,
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
 * Sets the entry text to @text.
 *
 * Since: 0.2.0
 **/
void
midori_location_action_set_text (MidoriLocationAction* location_action,
                                 const gchar*          text)
{
    GSList* proxies;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (text != NULL);

    midori_location_action_popdown_completion (location_action);

    katze_assign (location_action->text, g_strdup (text));
    katze_assign (location_action->uri, g_strdup (text));

    if (!(proxies = gtk_action_get_proxies (GTK_ACTION (location_action))))
        return;

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxies->data);
        gtk_entry_set_text (GTK_ENTRY (entry), text);
    }
}

/**
 * midori_location_action_set_icon:
 * @location_action: a #MidoriLocationAction
 * @icon: a #GdkPixbuf or %NULL
 *
 * Sets the icon shown on the left hand side.
 *
 * Deprecated: 0.4.6: The left hand icon only shows security status.
 **/
void
midori_location_action_set_icon (MidoriLocationAction* location_action,
                                 GdkPixbuf*            icon)
{
}

void
midori_location_action_add_uri (MidoriLocationAction* location_action,
                                const gchar*          uri)
{
    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));
    g_return_if_fail (uri != NULL);

    katze_assign (location_action->uri, g_strdup (uri));
}

/**
 * midori_location_action_add_item:
 * @location_action: a #MidoriLocationAction
 * @icon: a #GdkPixbuf or %NULL
 * @title: a string
 *
 * Adds the item to the location, historically this added it to
 * completion suggestions and updated the left side icon.
 *
 * Deprecated: 0.4.6: The left hand icon only shows security status.
 **/

void
midori_location_action_add_item (MidoriLocationAction* location_action,
                                 const gchar*          uri,
                                 GdkPixbuf*            icon,
                                 const gchar*          title)
{
}

/**
 * midori_location_action_set_icon_for_uri:
 * @location_action: a #MidoriLocationAction
 * @icon: a #GdkPixbuf
 * @uri: an URI string
 *
 * Sets the icon for the specified URI.
 *
 * Deprecated: 0.4.4
 **/
void
midori_location_action_set_icon_for_uri (MidoriLocationAction* location_action,
                                         GdkPixbuf*            icon,
                                         const gchar*          uri)
{
    midori_location_action_set_icon (location_action, icon);
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

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    location_action->progress = CLAMP (progress, 0.0, 1.0);

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxies->data);
        gtk_icon_entry_set_progress_fraction (GTK_ICON_ENTRY (entry),
                                              location_action->progress);
    }
}

/**
 * midori_location_action_set_security_hint:
 * @location_action: a #MidoriLocationAction
 * @icon: a stock ID, or an icon name
 *
 * Sets the secondary, ie right hand side icon.
 *
 * Since 0.4.6 @icon can be a stock ID or an icon name.
 **/
void
midori_location_action_set_secondary_icon (MidoriLocationAction* location_action,
                                           const gchar*          stock_id)
{
    #if !HAVE_HILDON
    GSList* proxies;
    #endif
    GtkStockItem stock_item;

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    katze_assign (location_action->secondary_icon, g_strdup (stock_id));

    #if !HAVE_HILDON
    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxies->data);
        if (stock_id && gtk_stock_lookup (stock_id, &stock_item))
            gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_SECONDARY, stock_id);
        else
            gtk_icon_entry_set_icon_from_icon_name (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_SECONDARY, stock_id);
    }
    #endif
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

    g_return_if_fail (MIDORI_IS_LOCATION_ACTION (location_action));

    proxies = gtk_action_get_proxies (GTK_ACTION (location_action));

    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        GtkWidget* entry = midori_location_action_entry_for_proxy (proxies->data);

        if (hint == MIDORI_SECURITY_UNKNOWN)
        {
            gchar* icon_names[] = { "channel-insecure-symbolic", "lock-insecure", "dialog-information", NULL };
            gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry), GTK_ICON_ENTRY_PRIMARY,
                g_themed_icon_new_from_names (icon_names, -1));
            gtk_icon_entry_set_tooltip (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, _("Not verified"));
        }
        else if (hint == MIDORI_SECURITY_TRUSTED)
        {
            gchar* icon_names[] = { "channel-secure-symbolic", "lock-secure", "locked", NULL };
            gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry), GTK_ICON_ENTRY_PRIMARY,
                g_themed_icon_new_from_names (icon_names, -1));
            gtk_icon_entry_set_tooltip (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, _("Verified and encrypted connection"));
        }
        else if (hint == MIDORI_SECURITY_NONE)
        {
            gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry), GTK_ICON_ENTRY_PRIMARY,
                g_themed_icon_new_with_default_fallbacks ("text-html-symbolic"));
            gtk_icon_entry_set_tooltip (GTK_ICON_ENTRY (entry),
                GTK_ICON_ENTRY_PRIMARY, _("Open, unencrypted connection"));
        }
    }
}
