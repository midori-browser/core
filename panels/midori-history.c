/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-history.h"

#include "midori-app.h"
#include "midori-array.h"
#include "midori-browser.h"
#include "midori-platform.h"
#include "midori-view.h"
#include "midori-core.h"

#include <glib/gi18n.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#define COMPLETION_DELAY 200

void
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeItem*     bookmark,
                                         gboolean       new_bookmark,
                                         gboolean       is_folder,
                                         GtkWidget*     proxy);


struct _MidoriHistory
{
    GtkVBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* bookmark;
    GtkWidget* delete;
    GtkWidget* clear;
    GtkWidget* treeview;
    MidoriApp* app;
    KatzeArray* array;

    gint filter_timeout;
    gchar* filter;
};

struct _MidoriHistoryClass
{
    GtkVBoxClass parent_class;
};

static void
midori_history_viewable_iface_init (MidoriViewableIface* iface);

G_DEFINE_TYPE_WITH_CODE (MidoriHistory, midori_history, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                             midori_history_viewable_iface_init));

enum
{
    PROP_0,

    PROP_APP
};

static void
midori_history_finalize     (GObject*      object);

static void
midori_history_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec);

static void
midori_history_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec);

static void
midori_history_class_init (MidoriHistoryClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_history_finalize;
    gobject_class->set_property = midori_history_set_property;
    gobject_class->get_property = midori_history_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_APP,
                                     g_param_spec_object (
                                     "app",
                                     "App",
                                     "The app",
                                     MIDORI_TYPE_APP,
                                     flags));
}

static const gchar*
midori_history_get_label (MidoriViewable* viewable)
{
    return _("History");
}

static const gchar*
midori_history_get_stock_id (MidoriViewable* viewable)
{
    return STOCK_HISTORY;
}

#if !GLIB_CHECK_VERSION (2, 26, 0)
static gint
sokoke_days_between (const time_t* day1,
                     const time_t* day2)
{
    GDate* date1;
    GDate* date2;
    gint age;

    date1 = g_date_new ();
    date2 = g_date_new ();

    g_date_set_time_t (date1, *day1);
    g_date_set_time_t (date2, *day2);

    age = g_date_days_between (date1, date2);

    g_date_free (date1);
    g_date_free (date2);

    return age;
}
#endif

static gchar*
midori_history_format_date (KatzeItem *item)
{
    gint64 day = katze_item_get_added (item);
    gchar* sdate;
    gint age;
    #if GLIB_CHECK_VERSION (2, 26, 0)
    GDateTime* now = g_date_time_new_now_local ();
    GDateTime* then = g_date_time_new_from_unix_local (day);
    age = g_date_time_get_day_of_year (now) - g_date_time_get_day_of_year (then);
    if (g_date_time_get_year (now) != g_date_time_get_year (then))
        age = 999;

    if (age == 0)
        sdate = g_strdup (_("Today"));
    else if (age == 1)
        sdate = g_strdup (_("Yesterday"));
    else if (age < 7)
        sdate = g_strdup_printf (ngettext ("%d day ago",
            "%d days ago", (gint)age), (gint)age);
    else if (age == 7)
        sdate = g_strdup (_("A week ago"));
    else
        sdate = g_date_time_format (then, "%x");
    #else
    gchar token[50];
    time_t current_time;

    current_time = time (NULL);
    age = sokoke_days_between ((time_t*)&day, &current_time);

    /* A negative age is a date in the future, the clock is probably off */
    if (age < -1)
        sdate = g_strdup ("");
    else if (age > 7 || age < 0)
    {
        strftime (token, sizeof (token), "%x", localtime ((time_t*)&day));
        sdate = g_strdup (token);
    }
    else if (age > 6)
        sdate = g_strdup (_("A week ago"));
    else if (age > 1)
        sdate = g_strdup_printf (ngettext ("%d day ago",
            "%d days ago", (gint)age), (gint)age);
    else if (age == 0)
        sdate = g_strdup (_("Today"));
    else
        sdate = g_strdup (_("Yesterday"));
    #endif
    return sdate;
}

static void
midori_history_toolbar_update (MidoriHistory *history)
{
    gboolean selected;

    selected = katze_tree_view_get_selected_iter (
        GTK_TREE_VIEW (history->treeview), NULL, NULL);
    gtk_widget_set_sensitive (GTK_WIDGET (history->delete), selected);
}

static void
midori_history_remove_item_from_db (MidoriHistory* history,
                                    KatzeItem*     item)
{
    gchar* sqlcmd;
    sqlite3* db;
    char* errmsg = NULL;

    db = g_object_get_data (G_OBJECT (history->array), "db");

    if (!db)
        return;

    if (KATZE_ITEM_IS_BOOKMARK (item))
        sqlcmd = sqlite3_mprintf (
            "DELETE FROM history WHERE uri = '%q' AND"
            " title = '%q' AND date = %llu",
            katze_item_get_uri (item),
            katze_item_get_name (item),
            katze_item_get_added (item));
    else
       sqlcmd = sqlite3_mprintf ("DELETE FROM history WHERE day = %d",
                katze_item_get_meta_integer (item, "day"));

    if (sqlite3_exec (db, sqlcmd, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        g_printerr (_("Failed to remove history item: %s\n"), errmsg);
        sqlite3_free (errmsg);
    }

    sqlite3_free (sqlcmd);
}

/**
 * midori_history_read_from_db:
 * @history: a #MidoriHistory
 * @req_day: the timestamp of one day, or 0
 * @filter: a filter string to search for
 *
 * Populates the model according to parameters:
 * 1. If @req_day is 0, all dates are added as folders.
 * 2. If @req_day is given, all pages for that day are added.
 * 3. If @filter is given, all pages matching the filter are added.
 **/
static KatzeArray*
midori_history_read_from_db (MidoriHistory* history,
                             int            req_day,
                             const gchar*   filter)
{
    sqlite3* db;
    sqlite3_stmt* statement;
    gint result;
    const gchar* sqlcmd;

    db = g_object_get_data (G_OBJECT (history->array), "db");

    if (!db)
        return katze_array_new (KATZE_TYPE_ITEM);

    if (filter && *filter)
    {
        gchar* filterstr;

        sqlcmd = "SELECT * FROM ("
                 "    SELECT uri, title, day, date FROM history"
                 "    WHERE uri LIKE ?1 OR title LIKE ?1 GROUP BY uri "
                 "UNION ALL "
                 "    SELECT replace (uri, '%s', keywords) AS uri, "
                 "    keywords AS title, day, 0 AS date FROM search "
                 "    WHERE uri LIKE ?1 OR keywords LIKE ?1 GROUP BY uri "
                 ") ORDER BY day ASC";
        result = sqlite3_prepare_v2 (db, sqlcmd, -1, &statement, NULL);
        filterstr = g_strdup_printf ("%%%s%%", filter);
        sqlite3_bind_text (statement, 1, filterstr, -1, g_free);
        req_day = -1;
    }
    else if (req_day == 0)
    {
        sqlcmd = "SELECT day, date FROM history GROUP BY day ORDER BY day ASC";
        result = sqlite3_prepare_v2 (db, sqlcmd, -1, &statement, NULL);
    }
    else
    {
        sqlcmd = "SELECT uri, title, date, day "
                 "FROM history WHERE day = ? "
                 "GROUP BY uri ORDER BY date ASC";
        result = sqlite3_prepare_v2 (db, sqlcmd, -1, &statement, NULL);
        sqlite3_bind_int64 (statement, 1, req_day);
    }

    if (result != SQLITE_OK)
        return katze_array_new (KATZE_TYPE_ITEM);

    return katze_array_from_statement (statement);
}

static void
midori_history_read_from_db_to_model (MidoriHistory* history,
                                      GtkTreeStore*  model,
                                      GtkTreeIter*   parent,
                                      int            req_day,
                                      const gchar*   filter)
{
    KatzeArray* array;
    gint last;
    KatzeItem* item;
    GtkTreeIter child;

    array = midori_history_read_from_db (history, req_day, filter);
    katze_bookmark_populate_tree_view (array, model, parent);

    /* Remove invisible dummy row */
    last = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), parent);
    if (!last)
        return;
    gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &child, parent, last - 1);
    gtk_tree_model_get (GTK_TREE_MODEL (model), &child, 0, &item, -1);
    if (KATZE_ITEM_IS_SEPARATOR (item))
        gtk_tree_store_remove (model, &child);
    else
        g_object_unref (item);
}

static void
midori_history_delete_clicked_cb (GtkWidget*     toolitem,
                                  MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (history->treeview),
                                           &model, &iter))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);
        midori_history_remove_item_from_db (history, item);
        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
        g_object_unref (item);
    }
}

static void
midori_history_clear_clicked_cb (GtkWidget*     toolitem,
                                 MidoriHistory* history)
{
    MidoriBrowser* browser;
    GtkWidget* dialog;
    gint result;

    browser = midori_browser_get_for_widget (GTK_WIDGET (history));
    dialog = gtk_message_dialog_new (GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        _("Are you sure you want to remove all history items?"));
    result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (result != GTK_RESPONSE_YES)
        return;

    katze_array_clear (history->array);
}

static void
midori_history_bookmark_add_cb (GtkWidget*     menuitem,
                                MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item = NULL;

    GtkWidget* proxy = GTK_IS_TOOL_ITEM (menuitem) ? menuitem : NULL;
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (history));
    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (history->treeview),
                                           &model, &iter))
        gtk_tree_model_get (model, &iter, 0, &item, -1);

    if (KATZE_IS_ITEM (item) && katze_item_get_uri (item))
    {
        midori_browser_edit_bookmark_dialog_new (browser, item, TRUE, FALSE, proxy);
        g_object_unref (item);
    }
    else
        midori_browser_edit_bookmark_dialog_new (browser, NULL, TRUE, FALSE, proxy);
}

static GtkWidget*
midori_history_get_toolbar (MidoriViewable* viewable)
{
    MidoriHistory* history = MIDORI_HISTORY (viewable);

    if (!history->toolbar)
    {
        GtkWidget* toolbar;
        GtkToolItem* toolitem;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
        history->toolbar = toolbar;
        toolitem = gtk_tool_button_new_from_stock (STOCK_BOOKMARK_ADD);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Bookmark the selected history item"));
        gtk_tool_item_set_is_important (toolitem, TRUE);
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_history_bookmark_add_cb), history);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        history->bookmark = GTK_WIDGET (toolitem);
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DELETE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Delete the selected history item"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_history_delete_clicked_cb), history);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        history->delete = GTK_WIDGET (toolitem);
        toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem),
                                     _("Clear the entire history"));
        g_signal_connect (toolitem, "clicked",
            G_CALLBACK (midori_history_clear_clicked_cb), history);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        gtk_widget_show (GTK_WIDGET (toolitem));
        history->clear = GTK_WIDGET (toolitem);
        midori_history_toolbar_update (history);
        g_signal_connect (history->bookmark, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &history->bookmark);
        g_signal_connect (history->delete, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &history->delete);
        g_signal_connect (history->clear, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &history->clear);
    }

    return history->toolbar;
}

static void
midori_history_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_history_get_stock_id;
    iface->get_label = midori_history_get_label;
    iface->get_toolbar = midori_history_get_toolbar;
}

static void
midori_history_add_item_cb (KatzeArray*    array,
                            KatzeItem*     item,
                            MidoriHistory* history)
{
    GtkTreeView* treeview = GTK_TREE_VIEW (history->treeview);
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    KatzeItem* today;
    time_t current_time = time (NULL);

    if (gtk_tree_model_iter_children (model, &iter, NULL))
    {
        gint64 day;
        gboolean has_today;

        gtk_tree_model_get (model, &iter, 0, &today, -1);

        day = katze_item_get_added (today);
        #if GLIB_CHECK_VERSION (2, 26, 0)
        has_today = g_date_time_get_day_of_month (
            g_date_time_new_from_unix_local (day))
         == g_date_time_get_day_of_month (
            g_date_time_new_from_unix_local (current_time))
        && g_date_time_get_day_of_year (
            g_date_time_new_from_unix_local (day))
         == g_date_time_get_day_of_year (
            g_date_time_new_from_unix_local (current_time));
        #else
        has_today = sokoke_days_between ((time_t*)&day, &current_time) == 0;
        #endif
        g_object_unref (today);
        if (has_today)
        {
            gchar* tooltip = g_markup_escape_text (katze_item_get_uri (item), -1);
            KatzeItem* copy = katze_item_copy (item);
            gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), NULL, &iter,
                                               0, 0, copy, 1, tooltip, -1);
            g_object_unref (copy);
            g_free (tooltip);
            return;
        }
    }

    today = (KatzeItem*)katze_array_new (KATZE_TYPE_ITEM);
    katze_item_set_added (today, current_time);
    katze_item_set_meta_integer (today, "day",
                                 sokoke_time_t_to_julian (&current_time));
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter, NULL,
                                       0, 0, today, -1);
    /* That's an invisible dummy, so we always have an expander */
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), NULL, &iter,
                                       0, 0, NULL, -1);
}

static void
midori_history_clear_cb (KatzeArray*    array,
                         MidoriHistory* history)
{
    GtkTreeView* treeview = GTK_TREE_VIEW (history->treeview);
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    gtk_tree_store_clear (GTK_TREE_STORE (model));
}
static void
midori_history_set_app (MidoriHistory* history,
                        MidoriApp*     app)
{
    GtkTreeModel* model;

    if (history->array)
    {
        g_signal_handlers_disconnect_by_func (history->array,
            midori_history_add_item_cb, history);
        g_signal_handlers_disconnect_by_func (history->array,
            midori_history_clear_cb, history);
        katze_object_assign (history->array, NULL);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));
        gtk_tree_store_clear (GTK_TREE_STORE (model));
    }

    katze_object_assign (history->app, app);
    if (!app)
        return;
    g_object_ref (app);

    history->array = katze_object_get_object (app, "history");
    g_signal_connect (history->array, "add-item",
                      G_CALLBACK (midori_history_add_item_cb), history);
    g_signal_connect (history->array, "clear",
                      G_CALLBACK (midori_history_clear_cb), history);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));
    if (history->array)
        midori_history_read_from_db_to_model (history, GTK_TREE_STORE (model), NULL, 0, NULL);
}

static void
midori_history_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriHistory* history = MIDORI_HISTORY (object);

    switch (prop_id)
    {
    case PROP_APP:
        midori_history_set_app (history, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_history_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    MidoriHistory* history = MIDORI_HISTORY (object);

    switch (prop_id)
    {
    case PROP_APP:
        g_value_set_object (value, history->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_history_treeview_render_icon_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    KatzeItem* item;
    GdkPixbuf* pixbuf;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (!item)
        pixbuf = NULL;
    else if ((pixbuf = katze_item_get_pixbuf (item, treeview)))
        ;
    else if (katze_item_get_uri (item))
        pixbuf = katze_load_cached_icon (katze_item_get_uri (item), treeview);
    else
        pixbuf = gtk_widget_render_icon (treeview, GTK_STOCK_DIRECTORY,
                                         GTK_ICON_SIZE_MENU, NULL);

    g_object_set (renderer, "pixbuf", pixbuf, NULL);

    if (pixbuf)
    {
        g_object_unref (pixbuf);
        g_object_unref (item);
    }
}

static void
midori_history_treeview_render_text_cb (GtkTreeViewColumn* column,
                                        GtkCellRenderer*   renderer,
                                        GtkTreeModel*      model,
                                        GtkTreeIter*       iter,
                                        GtkWidget*         treeview)
{
    KatzeItem* item;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (KATZE_ITEM_IS_BOOKMARK (item))
        g_object_set (renderer, "markup", NULL,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      "text", katze_item_get_name (item), NULL);
    else if (KATZE_ITEM_IS_FOLDER (item))
    {
        gchar* formatted = midori_history_format_date (item);
        g_object_set (renderer, "markup", NULL, "text", formatted,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);
        g_free (formatted);
    }
    else
        g_object_set (renderer, "markup", _("<i>Separator</i>"), NULL);

    if (item)
        g_object_unref (item);
}

static void
midori_history_row_activated_cb (GtkTreeView*       treeview,
                                 GtkTreePath*       path,
                                 GtkTreeViewColumn* column,
                                 MidoriHistory*     history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    model = gtk_tree_view_get_model (treeview);

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        if (KATZE_ITEM_IS_BOOKMARK (item))
        {
            MidoriBrowser* browser;
            const gchar* uri;

            uri = katze_item_get_uri (item);
            browser = midori_browser_get_for_widget (GTK_WIDGET (history));
            midori_browser_set_current_uri (browser, uri);
            g_object_unref (item);
            return;
        }
        if (gtk_tree_view_row_expanded (treeview, path))
            gtk_tree_view_collapse_row (treeview, path);
        else
            gtk_tree_view_expand_row (treeview, path, FALSE);
        g_object_unref (item);
    }
}

static void
midori_history_popup_item (GtkWidget*     menu,
                           const gchar*   stock_id,
                           const gchar*   label,
                           KatzeItem*     item,
                           gpointer       callback,
                           MidoriHistory* history)
{
    const gchar* uri;
    GtkWidget* menuitem;

    uri = katze_item_get_uri (item);

    menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    if (label)
        gtk_label_set_text_with_mnemonic (GTK_LABEL (gtk_bin_get_child (
        GTK_BIN (menuitem))), label);
    if (!strcmp (stock_id, GTK_STOCK_EDIT))
        gtk_widget_set_sensitive (menuitem, uri != NULL);
    else if (katze_item_get_uri (item) && strcmp (stock_id, GTK_STOCK_DELETE))
        gtk_widget_set_sensitive (menuitem, uri != NULL);
    g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
    g_signal_connect (menuitem, "activate", G_CALLBACK (callback), history);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static void
midori_history_open_activate_cb (GtkWidget*     menuitem,
                                 MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (history));
        midori_browser_set_current_uri (browser, uri);
    }
}

static void
midori_history_open_in_tab_activate_cb (GtkWidget*     menuitem,
                                        MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;
    guint n;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    if (KATZE_ITEM_IS_FOLDER (item))
    {
        sqlite3* db;
        gchar* sqlcmd;
        KatzeItem* child;
        KatzeArray* array;

        db = g_object_get_data (G_OBJECT (history->array), "db");

        if (!db)
            return;

        sqlcmd = g_strdup_printf ("SELECT uri, title, date, day "
                 "FROM history WHERE day = %d "
                 "GROUP BY uri ORDER BY date ASC",
                 (int)katze_item_get_added (item));
        array = katze_array_from_sqlite (db, sqlcmd);
        g_free (sqlcmd);
        KATZE_ARRAY_FOREACH_ITEM (child, KATZE_ARRAY (array))
        {
            if ((uri = katze_item_get_uri (child)) && *uri)
            {
                MidoriBrowser* browser;

                browser = midori_browser_get_for_widget (GTK_WIDGET (history));
                n = midori_browser_add_item (browser, child);
                midori_browser_set_current_page_smartly (browser, n);
            }
        }
    }
    else
    {
        if ((uri = katze_item_get_uri (item)) && *uri)
        {
            MidoriBrowser* browser;

            browser = midori_browser_get_for_widget (GTK_WIDGET (history));
            n = midori_browser_add_item (browser, item);
            midori_browser_set_current_page_smartly (browser, n);
        }
    }
}

static void
midori_history_open_in_window_activate_cb (GtkWidget*     menuitem,
                                           MidoriHistory* history)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);

    if (uri && *uri)
    {
        MidoriBrowser* new_browser = midori_app_create_browser (history->app);
        midori_app_add_browser (history->app, new_browser);
        gtk_widget_show (GTK_WIDGET (new_browser));
        midori_browser_add_uri (new_browser, uri);
    }
}


static void
midori_history_popup (GtkWidget*      widget,
                      GdkEventButton* event,
                      KatzeItem*      item,
                      MidoriHistory*  history)
{
    GtkWidget* menu;
    GtkWidget* menuitem;

    menu = gtk_menu_new ();
    if (!katze_item_get_uri (item))
        midori_history_popup_item (menu,
            STOCK_TAB_NEW, _("Open all in _Tabs"),
            item, midori_history_open_in_tab_activate_cb, history);
    else
    {
        midori_history_popup_item (menu, GTK_STOCK_OPEN, NULL,
            item, midori_history_open_activate_cb, history);
        midori_history_popup_item (menu, STOCK_TAB_NEW, _("Open in New _Tab"),
            item, midori_history_open_in_tab_activate_cb, history);
        midori_history_popup_item (menu, STOCK_WINDOW_NEW, _("Open in New _Window"),
            item, midori_history_open_in_window_activate_cb, history);
        midori_history_popup_item (menu, STOCK_BOOKMARK_ADD, NULL,
            item, midori_history_bookmark_add_cb, history);
    }
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    midori_history_popup_item (menu, GTK_STOCK_DELETE, NULL,
        item, midori_history_delete_clicked_cb, history);

    katze_widget_popup (widget, GTK_MENU (menu), event, KATZE_MENU_POSITION_CURSOR);
}

static gboolean
midori_history_button_release_event_cb (GtkWidget*      widget,
                                        GdkEventButton* event,
                                        MidoriHistory*  history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->button != 2 && event->button != 3)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);

        if (!item)
            return FALSE;

        if (event->button == 2)
        {
            const gchar* uri = katze_item_get_uri (item);

            if (uri && *uri)
            {
                MidoriBrowser* browser;
                gint n;

                browser = midori_browser_get_for_widget (widget);
                n = midori_browser_add_uri (browser, uri);
                midori_browser_set_current_page (browser, n);
            }
        }
        else
            midori_history_popup (widget, event, item, history);

        g_object_unref (item);
        return TRUE;
    }
    return FALSE;
}

static gboolean
midori_history_key_release_event_cb (GtkWidget*     widget,
                                     GdkEventKey*   event,
                                     MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (event->keyval != GDK_KEY_Delete)
        return FALSE;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        KatzeItem* item;

        gtk_tree_model_get (model, &iter, 0, &item, -1);
        midori_history_remove_item_from_db (history, item);
        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
        g_object_unref (item);
    }

    return FALSE;
}

static void
midori_history_popup_menu_cb (GtkWidget*     widget,
                              MidoriHistory* history)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (katze_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        midori_history_popup (widget, NULL, item, history);
        g_object_unref (item);
    }
}

static void
midori_history_row_expanded_cb (GtkTreeView*   treeview,
                                GtkTreeIter*   iter,
                                GtkTreePath*   path,
                                MidoriHistory* history)
{
    GtkTreeModel* model;
    KatzeItem* item;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    gtk_tree_model_get (model, iter, 0, &item, -1);
    midori_history_read_from_db_to_model (history, GTK_TREE_STORE (model),
                                         iter, katze_item_get_meta_integer (item, "day"), NULL);
    g_object_unref (item);
}

static void
midori_history_row_collapsed_cb (GtkTreeView *treeview,
                                 GtkTreeIter *parent,
                                 GtkTreePath *path,
                                 gpointer     user_data)
{
    GtkTreeModel* model;
    GtkTreeStore* treestore;
    GtkTreeIter child;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    treestore = GTK_TREE_STORE (model);
    while (gtk_tree_model_iter_nth_child (model, &child, parent, 0))
        gtk_tree_store_remove (treestore, &child);
    /* That's an invisible dummy, so we always have an expander */
    gtk_tree_store_insert_with_values (treestore, &child, parent,
        0, 0, NULL, -1);
}

static gboolean
midori_history_filter_timeout_cb (gpointer data)
{
    MidoriHistory* history = data;
    GtkTreeModel* model;
    GtkTreeStore* treestore;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (history->treeview));
    treestore = GTK_TREE_STORE (model);

    gtk_tree_store_clear (treestore);
    midori_history_read_from_db_to_model (history, treestore, NULL, 0, history->filter);

    return FALSE;
}

static void
midori_history_filter_entry_changed_cb (GtkEntry*      entry,
                                        MidoriHistory* history)
{
    if (history->filter_timeout)
        g_source_remove (history->filter_timeout);
    history->filter_timeout = g_timeout_add (COMPLETION_DELAY,
        midori_history_filter_timeout_cb, history);

    if (!g_object_get_data (G_OBJECT (entry), "sokoke_has_default"))
        katze_assign (history->filter, g_strdup (gtk_entry_get_text (entry)));
    else
        katze_assign (history->filter, NULL);
}

static void
midori_history_selection_changed_cb (GtkTreeView*   treeview,
                                     MidoriHistory* history)
{
    midori_history_toolbar_update (history);
}

static void
midori_history_init (MidoriHistory* history)
{
    GtkWidget* entry;
    GtkWidget* box;
    GtkTreeStore* model;
    GtkWidget* treeview;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_pixbuf;
    GtkCellRenderer* renderer_text;
    GtkTreeSelection* selection;

    /* Create the filter entry */
    entry = sokoke_search_entry_new (_("Search History"));
    g_signal_connect_after (entry, "changed",
        G_CALLBACK (midori_history_filter_entry_changed_cb), history);
    box = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 3);
    gtk_widget_show_all (box);
    gtk_box_pack_start (GTK_BOX (history), box, FALSE, FALSE, 5);

    /* Create the treeview */
    model = gtk_tree_store_new (2, KATZE_TYPE_ITEM, G_TYPE_STRING);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), 1);
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_expand (column, TRUE);
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_history_treeview_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_history_treeview_render_text_cb,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_object_unref (model);
    g_object_connect (treeview,
                      "signal::row-activated",
                      midori_history_row_activated_cb, history,
                      "signal::button-release-event",
                      midori_history_button_release_event_cb, history,
                      "signal::key-release-event",
                      midori_history_key_release_event_cb, history,
                      "signal::row-expanded",
                      midori_history_row_expanded_cb, history,
                      "signal::row-collapsed",
                      midori_history_row_collapsed_cb, history,
                      "signal::popup-menu",
                      midori_history_popup_menu_cb, history,
                      NULL);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    g_signal_connect_after (selection, "changed",
                            G_CALLBACK (midori_history_selection_changed_cb),
                            history);
    gtk_widget_show (treeview);
    gtk_box_pack_start (GTK_BOX (history), treeview, TRUE, TRUE, 0);
    history->treeview = treeview;
    /* FIXME: We need to connect a signal here, to add new pages into history */

    history->filter = NULL;
}

static void
midori_history_finalize (GObject* object)
{
    MidoriHistory* history = MIDORI_HISTORY (object);

    if (history->app)
        g_object_unref (history->app);

    g_signal_handlers_disconnect_by_func (history->array,
        midori_history_add_item_cb, history);
    g_signal_handlers_disconnect_by_func (history->array,
        midori_history_clear_cb, history);
    g_object_unref (history->array);
    katze_assign (history->filter, NULL);
}

