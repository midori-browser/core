/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori-browser.h"

#include "midori-view.h"
#include "midori-source.h"
#include "midori-preferences.h"
#include "midori-panel.h"
#include "midori-addons.h"
#include "midori-console.h"
#include "midori-searchentry.h"
#include "midori-locationaction.h"
#include "midori-stock.h"

#include "gtkiconentry.h"
#include "compat.h"
#include "sokoke.h"
#include "gjs.h"

#if HAVE_GIO
#include <gio/gio.h>
#endif
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

struct _MidoriBrowser
{
    GtkWindow parent_instance;

    GtkActionGroup* action_group;
    GtkWidget* menubar;
    GtkWidget* menu_bookmarks;
    GtkWidget* menu_tools;
    GtkWidget* menu_window;
    GtkWidget* popup_bookmark;
    GtkWidget* throbber;
    GtkWidget* navigationbar;
    GtkWidget* button_tab_new;
    GtkWidget* button_homepage;
    GtkWidget* search;
    GtkWidget* button_trash;
    GtkWidget* button_fullscreen;
    GtkWidget* bookmarkbar;

    GtkWidget* panel;
    GtkWidget* panel_bookmarks;
    GtkWidget* panel_console;
    GtkWidget* panel_pageholder;
    GtkWidget* notebook;

    GtkWidget* find;
    GtkWidget* find_text;
    GtkToolItem* find_case;
    GtkToolItem* find_highlight;

    GtkWidget* statusbar;
    GtkWidget* progressbar;

    gchar* statusbar_text;
    MidoriWebSettings* settings;
    KatzeArray* bookmarks;

    KatzeArray* proxy_array;
    KatzeArray* trash;
    KatzeArray* search_engines;
};

G_DEFINE_TYPE (MidoriBrowser, midori_browser, GTK_TYPE_WINDOW)

enum
{
    PROP_0,

    PROP_MENUBAR,
    PROP_NAVIGATIONBAR,
    PROP_URI,
    PROP_TAB,
    PROP_STATUSBAR,
    PROP_STATUSBAR_TEXT,
    PROP_SETTINGS,
    PROP_BOOKMARKS,
    PROP_TRASH,
    PROP_SEARCH_ENGINES
};

enum
{
    WINDOW_OBJECT_CLEARED,
    NEW_WINDOW,

    ADD_TAB,
    REMOVE_TAB,
    ACTIVATE_ACTION,
    QUIT,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_browser_dispose (GObject* object);

static void
midori_browser_finalize (GObject* object);

static void
midori_browser_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec);

static void
midori_browser_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec);



static GtkAction*
_action_by_name (MidoriBrowser* browser,
                 const gchar*   name)
{
    return gtk_action_group_get_action (browser->action_group, name);
}

static void
_action_set_sensitive (MidoriBrowser* browser,
                       const gchar*   name,
                       gboolean       sensitive)
{
    gtk_action_set_sensitive (_action_by_name (browser, name), sensitive);
}

static void
_action_set_active (MidoriBrowser* browser,
                    const gchar*   name,
                    gboolean       active)
{
    GtkAction* action = _action_by_name (browser, name);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

static void
_midori_browser_open_uri (MidoriBrowser* browser,
                          const gchar*   uri)
{
    GtkWidget* view;

    view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_set_uri (MIDORI_VIEW (view), uri);
}

static void
_midori_browser_update_actions (MidoriBrowser* browser)
{
    guint n;
    gboolean trash_empty;

    n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (browser->notebook));
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (browser->notebook), n > 1);
    _action_set_sensitive (browser, "TabClose", n > 1);
    _action_set_sensitive (browser, "TabPrevious", n > 1);
    _action_set_sensitive (browser, "TabNext", n > 1);

    if (browser->trash)
    {
        trash_empty = katze_array_is_empty (browser->trash);
        _action_set_sensitive (browser, "UndoTabClose", !trash_empty);
        _action_set_sensitive (browser, "Trash", !trash_empty);
    }
}

static void
_midori_browser_update_interface (MidoriBrowser* browser)
{
    GtkWidget* view;
    gboolean loading;
    gboolean can_reload;
    GtkAction* action;

    view = midori_browser_get_current_tab (browser);
    loading = midori_view_get_load_status (MIDORI_VIEW (view))
        != MIDORI_LOAD_FINISHED;
    can_reload = midori_view_can_reload (MIDORI_VIEW (view));

    _action_set_sensitive (browser, "Reload", can_reload && !loading);
    _action_set_sensitive (browser, "Stop", can_reload && loading);
    _action_set_sensitive (browser, "Back",
        midori_view_can_go_back (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "Forward",
        midori_view_can_go_forward (MIDORI_VIEW (view)));

    _action_set_sensitive (browser, "Print",
        midori_view_can_print (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "ZoomIn",
        midori_view_can_zoom_in (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "ZoomOut",
        midori_view_can_zoom_out (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "ZoomNormal",
        midori_view_get_zoom_level (MIDORI_VIEW (view)) != 1.0);
    _action_set_sensitive (browser, "SourceView",
        midori_view_can_view_source (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "Find",
        midori_view_can_find (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "FindNext",
        midori_view_can_find (MIDORI_VIEW (view)));
    _action_set_sensitive (browser, "FindPrevious",
        midori_view_can_find (MIDORI_VIEW (view)));
    /* _action_set_sensitive (browser, "FindQuick",
        midori_view_can_find (MIDORI_VIEW (view))); */
    gtk_widget_set_sensitive (GTK_WIDGET (browser->find_highlight),
        midori_view_can_find (MIDORI_VIEW (view)));

    action = gtk_action_group_get_action (browser->action_group, "ReloadStop");
    if (!loading)
    {
        gtk_widget_set_sensitive (browser->throbber, FALSE);
        g_object_set (action,
                      "stock-id", GTK_STOCK_REFRESH,
                      "tooltip", _("Reload the current page"),
                      "sensitive", can_reload, NULL);
        gtk_widget_hide (browser->progressbar);
        if (!GTK_WIDGET_VISIBLE (browser->statusbar))
            if (!sokoke_object_get_boolean (browser->settings,
                "show-navigationbar"))
                gtk_widget_hide (browser->navigationbar);
    }
    else
    {
        gtk_widget_set_sensitive (browser->throbber, TRUE);
        g_object_set (action,
                      "stock-id", GTK_STOCK_STOP,
                      "tooltip", _("Stop loading the current page"), NULL);
        gtk_widget_show (browser->progressbar);
        if (!GTK_WIDGET_VISIBLE (browser->statusbar))
        {
            if (!GTK_WIDGET_VISIBLE (browser->navigationbar))
                gtk_widget_show (browser->navigationbar);
            g_object_set (_action_by_name (browser, "Location"), "progress",
                midori_view_get_progress (MIDORI_VIEW (view)), NULL);
        }
    }
    katze_throbber_set_animated (KATZE_THROBBER (browser->throbber), loading);

    /* FIXME: This won't work due to a bug in GtkIconEntry */
    /* if (view && midori_view_get_news_feeds (MIDORI_VIEW (view)))
        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (
            gtk_bin_get_child (GTK_BIN (browser->location))),
            GTK_ICON_ENTRY_SECONDARY, STOCK_NEWS_FEED);
    else
        gtk_icon_entry_set_icon_from_pixbuf (GTK_ICON_ENTRY (
            gtk_bin_get_child (GTK_BIN (browser->location))),
            GTK_ICON_ENTRY_SECONDARY, NULL);*/
}

static void
_midori_browser_set_statusbar_text (MidoriBrowser* browser,
                                    const gchar*   text)
{
    katze_assign (browser->statusbar_text, g_strdup (text));
    gtk_statusbar_pop (GTK_STATUSBAR (browser->statusbar), 1);
    gtk_statusbar_push (GTK_STATUSBAR (browser->statusbar), 1,
                        browser->statusbar_text ? browser->statusbar_text : "");
}

static void
_midori_browser_set_current_page_smartly (MidoriBrowser* browser,
                                          gint           n)
{
    if (!sokoke_object_get_boolean (browser->settings,
        "open-tabs-in-the-background"))
        midori_browser_set_current_page (browser, n);
}

static void
_midori_browser_update_progress (MidoriBrowser* browser,
                                 MidoriView*    view)
{
    MidoriLocationAction* action;
    gdouble progress;
    gchar* message;

    action = MIDORI_LOCATION_ACTION (_action_by_name (browser, "Location"));
    progress = midori_view_get_progress (view);
    /* When we are finished, we don't want to *see* progress anymore */
    if (midori_view_get_load_status (view) == MIDORI_LOAD_FINISHED)
        progress = 0.0;
    if (progress > 0.0)
    {
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (browser->progressbar),
                                       progress);
        message = g_strdup_printf (_("%d%% loaded"), (gint)(progress * 100));
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (browser->progressbar),
                                   message);
        g_free (message);
        if (!GTK_WIDGET_VISIBLE (browser->statusbar))
            midori_location_action_set_progress (action, progress);
    }
    else
    {
        gtk_progress_bar_pulse (GTK_PROGRESS_BAR (browser->progressbar));
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (browser->progressbar),
                                   NULL);
        midori_location_action_set_progress (action, 0.0);
    }
}

static void
_midori_browser_activate_action (MidoriBrowser* browser,
                                 const gchar*   name)
{
    GtkAction* action = _action_by_name (browser, name);
    if (action)
        gtk_action_activate (action);
    else
        g_warning (_("Unexpected action '%s'."), name);
}

static void
midori_view_notify_icon_cb (MidoriView*    view,
                            GParamSpec*    pspec,
                            MidoriBrowser* browser)
{
    const gchar* uri;
    GtkAction* action;

    uri = midori_view_get_display_uri (MIDORI_VIEW (view));
    action = _action_by_name (browser, "Location");
    midori_location_action_set_icon_for_uri (
        MIDORI_LOCATION_ACTION (action), midori_view_get_icon (view), uri);
}

static void
midori_view_notify_load_status_cb (GtkWidget*      view,
                                   GParamSpec*     pspec,
                                   MidoriBrowser*  browser)
{
    const gchar* uri;
    GtkAction* action;

    uri = midori_view_get_display_uri (MIDORI_VIEW (view));
    action = _action_by_name (browser, "Location");

    if (midori_view_get_load_status (MIDORI_VIEW (view))
        == MIDORI_LOAD_COMMITTED)
        midori_location_action_add_uri (
            MIDORI_LOCATION_ACTION (action), uri);
    else if (midori_view_get_load_status (MIDORI_VIEW (view))
        == MIDORI_LOAD_FINISHED)
    {
        /* g_signal_emit (browser, signals[WINDOW_OBJECT_CLEARED], 0,
                       web_frame, js_context, js_window); */
    }

    if (view == midori_browser_get_current_tab (browser))
    {
        if (midori_view_get_load_status (MIDORI_VIEW (view))
            == MIDORI_LOAD_COMMITTED)
        {
            midori_location_action_set_uri (
                MIDORI_LOCATION_ACTION (action), uri);
            midori_location_action_set_secondary_icon (
                MIDORI_LOCATION_ACTION (action), NULL);
            g_object_notify (G_OBJECT (browser), "uri");
        }

        _midori_browser_update_interface (browser);
        _midori_browser_set_statusbar_text (browser, NULL);
    }
}

static void
midori_view_notify_progress_cb (GtkWidget*     view,
                                GParamSpec*    pspec,
                                MidoriBrowser* browser)
{
    if (view == midori_browser_get_current_tab (browser))
        _midori_browser_update_progress (browser, MIDORI_VIEW (view));
}

/*
static void
midori_web_view_news_feed_ready_cb (MidoriWebView* web_view,
                                    const gchar*   href,
                                    const gchar*   type,
                                    const gchar*   title,
                                    MidoriBrowser* browser)
{
    if (web_view == (MidoriWebView*)midori_browser_get_current_web_view (browser))
        midori_location_action_set_secondary_icon (MIDORI_LOCATION_ACTION (
            _action_by_name (browser, "Location")), STOCK_NEWS_FEED);
}
*/

static void
midori_view_notify_title_cb (GtkWidget*     view,
                             GParamSpec*    pspec,
                             MidoriBrowser* browser)
{
    const gchar* uri;
    const gchar* title;
    GtkAction* action;
    gchar* window_title;

    uri = midori_view_get_display_uri (MIDORI_VIEW (view));
    title = midori_view_get_display_title (MIDORI_VIEW (view));
    action = _action_by_name (browser, "Location");
    midori_location_action_set_title_for_uri (
        MIDORI_LOCATION_ACTION (action), title, uri);

    if (view == midori_browser_get_current_tab (browser))
    {
        window_title = g_strconcat (title, " - ",
            g_get_application_name (), NULL);
        gtk_window_set_title (GTK_WINDOW (browser), window_title);
        g_free (window_title);
    }
}

static void
midori_view_notify_zoom_level_cb (GtkWidget*     view,
                                  GParamSpec*    pspec,
                                  MidoriBrowser* browser)
{
    if (view == midori_browser_get_current_tab (browser))
        _action_set_sensitive (browser, "ZoomNormal",
            midori_view_get_zoom_level (MIDORI_VIEW (view)) != 1.0);
}

static void
midori_view_notify_statusbar_text_cb (MidoriView*    view,
                                      GParamSpec*    pspec,
                                      MidoriBrowser* browser)
{
    gchar* text;

    g_object_get (view, "statusbar-text", &text, NULL);
    _midori_browser_set_statusbar_text (browser, text);
    g_free (text);
}

static void
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeItem*     bookmark)
{
    gboolean new_bookmark;
    GtkWidget* dialog;
    GtkSizeGroup* sizegroup;
    GtkWidget* view;
    GtkWidget* hbox;
    GtkWidget* label;
    const gchar* value;
    GtkWidget* entry_title;
    GtkWidget* entry_desc;
    GtkWidget* entry_uri;
    GtkWidget* combo_folder;
    GtkTreeView* treeview;
    GtkTreeModel* treemodel;
    GtkTreeIter iter;

    new_bookmark = bookmark == NULL;
    dialog = gtk_dialog_new_with_buttons (
        new_bookmark ? _("New bookmark") : _("Edit bookmark"),
        GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        new_bookmark ? GTK_STOCK_ADD : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog),
        new_bookmark ? GTK_STOCK_ADD : GTK_STOCK_REMOVE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 5);
    sizegroup =  gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    if (new_bookmark)
    {
        view = midori_browser_get_current_tab (browser);
        bookmark = g_object_new (KATZE_TYPE_ITEM,
            "uri", midori_view_get_display_uri (MIDORI_VIEW (view)),
            "name", midori_view_get_display_title (MIDORI_VIEW (view)), NULL);
    }

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Title:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_title = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_title), TRUE);
    value = katze_item_get_name (bookmark);
    gtk_entry_set_text (GTK_ENTRY (entry_title), value ? value : "");
    gtk_box_pack_start (GTK_BOX (hbox), entry_title, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Description:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry_desc = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_desc), TRUE);
    if (!new_bookmark)
    {
        value = katze_item_get_text (bookmark);
        gtk_entry_set_text (GTK_ENTRY (entry_desc), value ? value : "");
    }
    gtk_box_pack_start (GTK_BOX (hbox), entry_desc, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    entry_uri = NULL;
    if (!KATZE_IS_ARRAY (bookmark))
    {
        hbox = gtk_hbox_new (FALSE, 8);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
        label = gtk_label_new_with_mnemonic (_("_URL:"));
        gtk_size_group_add_widget (sizegroup, label);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        entry_uri = gtk_entry_new ();
        gtk_entry_set_activates_default (GTK_ENTRY (entry_uri), TRUE);
        gtk_entry_set_text (GTK_ENTRY (entry_uri), katze_item_get_uri (bookmark));
        gtk_box_pack_start (GTK_BOX (hbox), entry_uri, TRUE, TRUE, 0);
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
        gtk_widget_show_all (hbox);
    }

    combo_folder = NULL;
    if (new_bookmark)
    {
        hbox = gtk_hbox_new (FALSE, 8);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
        label = gtk_label_new_with_mnemonic (_("_Folder:"));
        gtk_size_group_add_widget (sizegroup, label);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        combo_folder = gtk_combo_box_new_text ();
        gtk_combo_box_append_text (GTK_COMBO_BOX (combo_folder), _("Root"));
        gtk_widget_set_sensitive (combo_folder, FALSE);
        gtk_box_pack_start (GTK_BOX (hbox), combo_folder, TRUE, TRUE, 0);
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
        gtk_widget_show_all (hbox);
    }

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        katze_item_set_name (bookmark,
            gtk_entry_get_text (GTK_ENTRY (entry_title)));
        katze_item_set_text (bookmark,
            gtk_entry_get_text (GTK_ENTRY (entry_desc)));
        if (!KATZE_IS_ARRAY (bookmark))
            katze_item_set_uri (bookmark,
                gtk_entry_get_text (GTK_ENTRY (entry_uri)));

        /* FIXME: We want to choose a folder */
        if (new_bookmark)
        {
            katze_array_add_item (browser->bookmarks, bookmark);
            treeview = GTK_TREE_VIEW (browser->panel_bookmarks);
            treemodel = gtk_tree_view_get_model (treeview);
            gtk_tree_store_insert_with_values (GTK_TREE_STORE (treemodel),
                &iter, NULL, G_MAXINT, 0, bookmark, -1);
            g_object_ref (bookmark);
        }

        /* FIXME: Update navigationbar */
        /* FIXME: Update panel in other windows */
    }
    gtk_widget_destroy (dialog);
}

static void
midori_view_add_bookmark_cb (GtkWidget*   menuitem,
                             const gchar* uri,
                             GtkWidget*   view)
{
    KatzeItem* item;
    MidoriBrowser* browser;

    item = katze_item_new ();
    katze_item_set_uri (item, uri);
    browser = (MidoriBrowser*)gtk_widget_get_toplevel (menuitem);
    midori_browser_edit_bookmark_dialog_new (browser, item);
}

static gboolean
midori_browser_tab_leave_notify_event_cb (GtkWidget*        widget,
                                          GdkEventCrossing* event,
                                          MidoriBrowser*    browser)
{
    _midori_browser_set_statusbar_text (browser, NULL);
    return TRUE;
}

static void
midori_view_activate_action_cb (GtkWidget*     view,
                                const gchar*   action,
                                MidoriBrowser* browser)
{
    _midori_browser_activate_action (browser, action);
}

static void
midori_view_console_message_cb (GtkWidget*     view,
                                const gchar*   message,
                                gint           line,
                                const gchar*   source_id,
                                MidoriBrowser* browser)
{
    midori_console_add (MIDORI_CONSOLE (browser->panel_console),
                        message, line, source_id);
}

static void
midori_view_new_tab_cb (GtkWidget*     view,
                        const gchar*   uri,
                        gboolean       background,
                        MidoriBrowser* browser)
{
    gint n = midori_browser_add_uri (browser, uri);
    if (!background)
        midori_browser_set_current_page (browser, n);
}

static void
midori_view_new_window_cb (GtkWidget*     view,
                           const gchar*   uri,
                           MidoriBrowser* browser)
{
    g_signal_emit (browser, signals[NEW_WINDOW], 0, uri);
}

static void
midori_view_search_text_cb (GtkWidget*     view,
                            gboolean       found,
                            MidoriBrowser* browser)
{
    const gchar* text;
    gboolean case_sensitive;
    gboolean highlight;

    if (GTK_WIDGET_VISIBLE (browser->find))
    {
        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (browser->find_text),
            GTK_ICON_ENTRY_PRIMARY, (found) ? GTK_STOCK_FIND : GTK_STOCK_STOP);
        text = gtk_entry_get_text (GTK_ENTRY (browser->find_text));
        case_sensitive = gtk_toggle_tool_button_get_active (
            GTK_TOGGLE_TOOL_BUTTON (browser->find_case));
        midori_view_mark_text_matches (MIDORI_VIEW (view), text, case_sensitive);
        highlight = gtk_toggle_tool_button_get_active (
            GTK_TOGGLE_TOOL_BUTTON (browser->find_highlight));
        midori_view_set_highlight_text_matches (MIDORI_VIEW (view), highlight);
    }
}

static gboolean
midori_browser_tab_destroy_cb (GtkWidget*     widget,
                               MidoriBrowser* browser)
{
    KatzeItem* item;
    const gchar* uri;

    if (browser->proxy_array && MIDORI_IS_VIEW (widget))
    {
        item = midori_view_get_proxy_item (MIDORI_VIEW (widget));
        uri = katze_item_get_uri (item);
        if (browser->trash && uri && *uri)
            katze_array_add_item (browser->trash, item);
        katze_array_remove_item (browser->proxy_array, item);
        g_object_unref (item);
    }

    _midori_browser_update_actions (browser);

    /* This callback must only be called once, but we need to ensure
       that "remove-tab" is emitted in any case */
    g_signal_handlers_disconnect_by_func (widget,
        midori_browser_tab_destroy_cb, browser);

    g_signal_emit (browser, signals[REMOVE_TAB], 0, widget);
    return FALSE;
}

static void
midori_browser_window_menu_item_activate_cb (GtkWidget* menuitem,
                                             GtkWidget* widget)
{
    MidoriBrowser* browser;

    g_return_if_fail (GTK_IS_WIDGET (widget));

    browser = MIDORI_BROWSER (gtk_widget_get_toplevel (widget));
    g_return_if_fail (browser);

    midori_browser_set_current_tab (browser, widget);
}

static void
_midori_browser_add_tab (MidoriBrowser* browser,
                         GtkWidget*     view)
{
    GtkWidget* tab_label;
    GtkWidget* menuitem;
    KatzeItem* item;
    guint n;

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    GTK_WIDGET_SET_FLAGS (view, GTK_CAN_FOCUS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view),
                                         GTK_SHADOW_ETCHED_IN);

    tab_label = midori_view_get_proxy_tab_label (MIDORI_VIEW (view));
    menuitem = midori_view_get_proxy_menu_item (MIDORI_VIEW (view));

    if (browser->proxy_array)
    {
        item = midori_view_get_proxy_item (MIDORI_VIEW (view));
        g_object_ref (item);
        katze_array_add_item (browser->proxy_array, item);
    }

    g_object_connect (view,
                      "signal::notify::icon",
                      midori_view_notify_icon_cb, browser,
                      "signal::notify::load-status",
                      midori_view_notify_load_status_cb, browser,
                      "signal::notify::progress",
                      midori_view_notify_progress_cb, browser,
                      /* "signal::news-feed-ready",
                      midori_view_news_feed_ready_cb, browser, */
                      "signal::notify::title",
                      midori_view_notify_title_cb, browser,
                      "signal::notify::zoom-level",
                      midori_view_notify_zoom_level_cb, browser,
                      "signal::notify::statusbar-text",
                      midori_view_notify_statusbar_text_cb, browser,
                      "signal::activate-action",
                      midori_view_activate_action_cb, browser,
                      "signal::console-message",
                      midori_view_console_message_cb, browser,
                      "signal::new-tab",
                      midori_view_new_tab_cb, browser,
                      "signal::new-window",
                      midori_view_new_window_cb, browser,
                      "signal::search-text",
                      midori_view_search_text_cb, browser,
                      "signal::add-bookmark",
                      midori_view_add_bookmark_cb, browser,
                      NULL);

    g_signal_connect (view, "leave-notify-event",
        G_CALLBACK (midori_browser_tab_leave_notify_event_cb), browser);

    if (sokoke_object_get_boolean (browser->settings, "open-tabs-next-to-current"))
    {
        n = gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
        gtk_notebook_insert_page (GTK_NOTEBOOK (browser->notebook), view,
                                  tab_label, n + 1);
    }
    else
        gtk_notebook_append_page (GTK_NOTEBOOK (browser->notebook), view,
                                  tab_label);

    #if GTK_CHECK_VERSION(2, 10, 0)
    gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (browser->notebook),
                                      view, TRUE);
    gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (browser->notebook),
                                     view, TRUE);
    #endif

    gtk_widget_show (menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_browser_window_menu_item_activate_cb), view);
    gtk_menu_shell_append (GTK_MENU_SHELL (browser->menu_window), menuitem);

    /* We want the tab to be removed if the widget is destroyed */
    g_signal_connect_swapped (view, "destroy",
        G_CALLBACK (gtk_widget_destroy), menuitem);
    g_signal_connect (view, "destroy",
        G_CALLBACK (midori_browser_tab_destroy_cb), browser);

    _midori_browser_update_actions (browser);
}

static void
_midori_browser_remove_tab (MidoriBrowser* browser,
                            GtkWidget*     view)
{
    gtk_widget_destroy (view);
}

static void
_midori_browser_quit (MidoriBrowser* browser)
{
    /* Nothing to do */
}

static void
midori_cclosure_marshal_VOID__OBJECT_POINTER_POINTER (GClosure*     closure,
                                                      GValue*       return_value,
                                                      guint         n_param_values,
                                                      const GValue* param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data)
{
    typedef gboolean(*GMarshalFunc_VOID__OBJECT_POINTER_POINTER) (gpointer  data1,
                                                                  gpointer  arg_1,
                                                                  gpointer  arg_2,
                                                                  gpointer  arg_3,
                                                                  gpointer  data2);
    register GMarshalFunc_VOID__OBJECT_POINTER_POINTER callback;
    register GCClosure* cc = (GCClosure*) closure;
    register gpointer data1, data2;

    g_return_if_fail (n_param_values == 4);

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
    callback = (GMarshalFunc_VOID__OBJECT_POINTER_POINTER) (marshal_data
        ? marshal_data : cc->callback);

    callback (data1,
              g_value_get_object (param_values + 1),
              g_value_get_pointer (param_values + 2),
              g_value_get_pointer (param_values + 3),
              data2);
}

static void
midori_browser_class_init (MidoriBrowserClass* class)
{
    signals[WINDOW_OBJECT_CLEARED] = g_signal_new (
        "window-object-cleared",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriBrowserClass, window_object_cleared),
        0,
        NULL,
        midori_cclosure_marshal_VOID__OBJECT_POINTER_POINTER,
        G_TYPE_NONE, 3,
        WEBKIT_TYPE_WEB_FRAME,
        G_TYPE_POINTER,
        G_TYPE_POINTER);

    signals[NEW_WINDOW] = g_signal_new (
        "new-window",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriBrowserClass, new_window),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[ADD_TAB] = g_signal_new (
        "add-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, add_tab),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_WIDGET);

    signals[REMOVE_TAB] = g_signal_new (
        "remove-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, remove_tab),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_WIDGET);

    signals[ACTIVATE_ACTION] = g_signal_new (
        "activate-action",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, activate_action),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[QUIT] = g_signal_new (
        "quit",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, quit),
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    class->add_tab = _midori_browser_add_tab;
    class->remove_tab = _midori_browser_remove_tab;
    class->activate_action = _midori_browser_activate_action;
    class->quit = _midori_browser_quit;

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->dispose = midori_browser_dispose;
    gobject_class->finalize = midori_browser_finalize;
    gobject_class->set_property = midori_browser_set_property;
    gobject_class->get_property = midori_browser_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_MENUBAR,
                                     g_param_spec_object (
                                     "menubar",
                                     _("Menubar"),
                                     _("The menubar"),
                                     GTK_TYPE_MENU_BAR,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NAVIGATIONBAR,
                                     g_param_spec_object (
                                     "navigationbar",
                                     _("Navigationbar"),
                                     _("The navigationbar"),
                                     GTK_TYPE_TOOLBAR,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     _("URI"),
                                     _("The current URI"),
                                     "about:blank",
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TAB,
                                     g_param_spec_object (
                                     "tab",
                                     _("Tab"),
                                     _("The current tab"),
                                     GTK_TYPE_WIDGET,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR,
                                     g_param_spec_object (
                                     "statusbar",
                                     _("Statusbar"),
                                     _("The statusbar"),
                                     GTK_TYPE_STATUSBAR,
                                     G_PARAM_READABLE));

    /**
    * MidoriBrowser:statusbar-text:
    *
    * The text that is displayed in the statusbar.
    *
    * This value reflects changes to the text visible in the statusbar, such
    * as the uri of a hyperlink the mouse hovers over or the description of
    * a menuitem.
    *
    * Setting this value changes the displayed text until the next change.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR_TEXT,
                                     g_param_spec_string (
                                     "statusbar-text",
                                     _("Statusbar Text"),
                                     _("The text that is displayed in the statusbar"),
                                     "",
                                     flags));

    /**
    * MidoriBrowser:settings:
    *
    * An associated settings instance that is shared among all web views.
    *
    * Setting this value is propagated to every present web view. Also
    * every newly created web view will use this instance automatically.
    *
    * If no settings are specified a default will be used.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     _("Settings"),
                                     _("The associated settings"),
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE));

    /**
    * MidoriBrowser:bookmarks:
    *
    * The bookmarks folder, containing all bookmarks.
    *
    * This is actually a reference to a bookmarks instance,
    * so if bookmarks should be used it must be initially set.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_BOOKMARKS,
                                     g_param_spec_object (
                                     "bookmarks",
                                     _("Bookmarks"),
                                     _("The bookmarks folder, containing all bookmarks"),
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    /**
    * MidoriBrowser:trash:
    *
    * The trash, that collects all closed tabs and windows.
    *
    * This is actually a reference to a trash instance, so if a trash should
    * be used it must be initially set.
    *
    * Note: In the future the trash might collect other types of items.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_TRASH,
                                     g_param_spec_object (
                                     "trash",
                                     _("Trash"),
                                     _("The trash, collecting recently closed tabs and windows"),
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));

    /**
    * MidoriBrowser:search-engines:
    *
    * The list of search engines to be used for web search.
    *
    * This is actually a reference to a search engines instance,
    * so if search engines should be used it must be initially set.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES,
                                     g_param_spec_object (
                                     "search-engines",
                                     _("Search Engines"),
                                     _("The list of search engines to be used for web search"),
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE));
}

static void
_action_window_new_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    g_signal_emit (browser, signals[NEW_WINDOW], 0, "");
}

static void
_action_tab_new_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    gint n = midori_browser_add_uri (browser, "");
    midori_browser_set_current_page (browser, n);
    gtk_action_activate (_action_by_name (browser, "Location"));
}

static void
_action_open_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    static gchar* last_dir = NULL;
    gchar* uri = NULL;
    gboolean folder_set = FALSE;
    GtkWidget* dialog;

    dialog = gtk_file_chooser_dialog_new (
        ("Open file"), GTK_WINDOW (browser),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
        NULL);
     gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_OPEN);
     gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (browser));

     /* base the start folder on the current view's uri if it is local */
     GtkWidget* view = midori_browser_get_current_tab (browser);
     if ((uri = (gchar*)midori_view_get_display_uri (MIDORI_VIEW (view))))
     {
         gchar* filename = g_filename_from_uri (uri, NULL, NULL);
         if (filename)
         {
             gchar* dirname = g_path_get_dirname (filename);
             if (dirname && g_file_test (dirname, G_FILE_TEST_IS_DIR))
             {
                 gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), dirname);
                 folder_set = TRUE;
             }

             g_free (dirname);
             g_free (filename);
         }
     }

     if (!folder_set && last_dir && *last_dir)
         gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), last_dir);

     if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
     {
         uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
         gchar* folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
         _midori_browser_open_uri (browser, uri);

         g_free (last_dir);
         last_dir = folder;
         g_free (uri);
     }
    gtk_widget_destroy (dialog);
}

static void
_action_tab_close_activate (GtkAction*     action,
                            MidoriBrowser* browser)
{
    GtkWidget* widget = midori_browser_get_current_tab (browser);
    gtk_widget_destroy (widget);
}

static void
_action_window_close_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    gtk_widget_destroy (GTK_WIDGET (browser));
}

static void
_action_print_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_print (MIDORI_VIEW (view));
}

static void
_action_quit_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    g_signal_emit (browser, signals[QUIT], 0);
}

static void
_action_edit_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    gboolean can_cut = FALSE, can_copy = FALSE, can_paste = FALSE;
    gboolean has_selection, can_select_all = FALSE;

    if (MIDORI_IS_VIEW (widget))
    {
        MidoriView* view = MIDORI_VIEW (widget);
        can_cut = midori_view_can_cut_clipboard (view);
        can_copy = midori_view_can_copy_clipboard (view);
        can_paste = midori_view_can_paste_clipboard (view);
        can_select_all = TRUE;
    }
    else if (GTK_IS_EDITABLE (widget))
    {
        GtkEditable* editable = GTK_EDITABLE (widget);
        has_selection = gtk_editable_get_selection_bounds (editable, NULL, NULL);
        can_cut = has_selection && gtk_editable_get_editable (editable);
        can_copy = has_selection;
        can_paste = gtk_editable_get_editable (editable);
        can_select_all = TRUE;
    }
    else if (GTK_IS_TEXT_VIEW (widget))
    {
        GtkTextView* text_view = GTK_TEXT_VIEW (widget);
        GtkTextBuffer* buffer = gtk_text_view_get_buffer (text_view);
        has_selection = gtk_text_buffer_get_has_selection (buffer);
        can_cut = gtk_text_view_get_editable (text_view);
        can_copy = has_selection;
        can_paste = gtk_text_view_get_editable (text_view) && has_selection;
        can_select_all = TRUE;
    }

    _action_set_sensitive (browser, "Cut", can_cut);
    _action_set_sensitive (browser, "Copy", can_copy);
    _action_set_sensitive (browser, "Paste", can_paste);
    _action_set_sensitive (browser, "Delete", can_cut);
    _action_set_sensitive (browser, "SelectAll", can_select_all);
}

static void
_action_cut_activate (GtkAction*     action,
                      MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
        g_signal_emit_by_name (widget, "cut-clipboard");
}

static void
_action_copy_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
        g_signal_emit_by_name (widget, "copy-clipboard");
}

static void
_action_paste_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
        g_signal_emit_by_name (widget, "paste-clipboard");
}

static void
_action_delete_activate (GtkAction*     action,
                         MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
    {
        if (WEBKIT_IS_WEB_VIEW (widget))
            webkit_web_view_delete_selection (WEBKIT_WEB_VIEW (widget));
        else if (GTK_IS_EDITABLE (widget))
            gtk_editable_delete_selection (GTK_EDITABLE (widget));
        else if (GTK_IS_TEXT_VIEW (widget))
            gtk_text_buffer_delete_selection (
                gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)), TRUE, FALSE);
    }
}

static void
_action_select_all_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
    {
        if (GTK_IS_EDITABLE (widget))
            gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
        else
            g_signal_emit_by_name (widget, "select-all", TRUE);
    }
}

static void
_action_find_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* view;

    if (GTK_WIDGET_VISIBLE (browser->find))
    {
        view = midori_browser_get_current_tab (browser);
        midori_view_unmark_text_matches (MIDORI_VIEW (view));
        gtk_toggle_tool_button_set_active (
            GTK_TOGGLE_TOOL_BUTTON (browser->find_highlight), FALSE);
        gtk_widget_hide (browser->find);
    }
    else
    {
        gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (browser->find_text),
                                            GTK_ICON_ENTRY_PRIMARY, GTK_STOCK_FIND);
        gtk_entry_set_text (GTK_ENTRY (browser->find_text), "");
        gtk_widget_show (browser->find);
        gtk_widget_grab_focus (GTK_WIDGET (browser->find_text));
    }
}

static void
_midori_browser_find (MidoriBrowser* browser,
                      gboolean       forward)
{
    const gchar* text;
    gboolean case_sensitive;
    GtkWidget* view;

    text = gtk_entry_get_text (GTK_ENTRY (browser->find_text));
    case_sensitive = gtk_toggle_tool_button_get_active (
        GTK_TOGGLE_TOOL_BUTTON (browser->find_case));
    view = midori_browser_get_current_tab (browser);

    if (GTK_WIDGET_VISIBLE (browser->find))
        midori_view_unmark_text_matches (MIDORI_VIEW (view));
    midori_view_search_text (MIDORI_VIEW (view), text, case_sensitive, forward);
}

static void
_action_find_next_activate (GtkAction*     action,
                            MidoriBrowser* browser)
{
    _midori_browser_find (browser, TRUE);
}

static void
_action_find_previous_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    _midori_browser_find (browser, FALSE);
}

static void
_find_highlight_toggled (GtkToggleToolButton* toolitem,
                         MidoriBrowser*       browser)
{
    GtkWidget* view;
    gboolean highlight;

    view = midori_browser_get_current_tab (browser);
    highlight = gtk_toggle_tool_button_get_active (toolitem);
    midori_view_set_highlight_text_matches (MIDORI_VIEW (view), highlight);
}

static void
midori_browser_find_button_close_clicked_cb (GtkWidget*     widget,
                                             MidoriBrowser* browser)
{
    gtk_widget_hide (browser->find);
}

static void
midori_browser_navigationbar_notify_style_cb (GObject*       object,
                                              GParamSpec*    arg1,
                                              MidoriBrowser* browser)
{
    MidoriToolbarStyle toolbar_style;
    GtkToolbarStyle gtk_toolbar_style;

    g_object_get (browser->settings, "toolbar-style", &toolbar_style, NULL);
    if (toolbar_style == MIDORI_TOOLBAR_DEFAULT)
    {
        g_object_get (browser->settings,
                      "gtk-toolbar-style", &gtk_toolbar_style, NULL);
        gtk_toolbar_set_style (GTK_TOOLBAR (browser->navigationbar),
                               gtk_toolbar_style);
    }
}

static void
midori_browser_menu_trash_item_activate_cb (GtkWidget*     menuitem,
                                            MidoriBrowser* browser)
{
    KatzeItem* item;
    gint n;

    /* Create a new web view with an uri which has been closed before */
    item = g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    n = midori_browser_add_item (browser, item);
    midori_browser_set_current_page (browser, n);
    katze_array_remove_item (browser->trash, item);
    _midori_browser_update_actions (browser);
}

static void
midori_browser_menu_trash_activate_cb (GtkWidget*     widget,
                                       MidoriBrowser* browser)
{
    GtkWidget* menu;
    guint i, n;
    KatzeItem* item;
    const gchar* title;
    const gchar* uri;
    GtkWidget* menuitem;
    GtkWidget* icon;
    GtkAction* action;

    menu = gtk_menu_new ();
    n = katze_array_get_length (browser->trash);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (browser->trash, i);
        title = katze_item_get_name (item);
        uri = katze_item_get_uri (item);
        menuitem = sokoke_image_menu_item_new_ellipsized (title ? title : uri);
        /* FIXME: Get the real icon */
        icon = gtk_image_new_from_stock (GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_browser_menu_trash_item_activate_cb), browser);
        gtk_widget_show (menuitem);
    }

    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    action = gtk_action_group_get_action (browser->action_group, "TrashEmpty");
    menuitem = gtk_action_create_menu_item (action);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    if (GTK_IS_MENU_ITEM (widget))
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), menu);
    else
        sokoke_widget_popup (widget, GTK_MENU (menu), NULL,
                             SOKOKE_MENU_POSITION_RIGHT);
}

static void
midori_preferences_response_help_cb (GtkWidget*     preferences,
                                     gint           response,
                                     MidoriBrowser* browser)
{
    if (response == GTK_RESPONSE_HELP)
        gtk_action_activate (_action_by_name (browser, "HelpContents"));
}

static void
_action_preferences_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    /* Show the preferences dialog. Create it if necessary. */
    static GtkWidget* dialog = NULL;
    if (GTK_IS_DIALOG (dialog))
        gtk_window_present (GTK_WINDOW (dialog));
    else
    {
        dialog = midori_preferences_new (GTK_WINDOW (browser),
                                         browser->settings);
        g_signal_connect (dialog, "response",
            G_CALLBACK (midori_preferences_response_help_cb), browser);
        gtk_widget_show (dialog);
    }
}

static void
_action_navigationbar_activate (GtkToggleAction* action,
                                MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-navigationbar", active, NULL);
    sokoke_widget_set_visible (browser->navigationbar, active);
}

static void
_action_bookmarkbar_activate (GtkToggleAction* action,
                              MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-bookmarkbar", active, NULL);
    sokoke_widget_set_visible (browser->bookmarkbar, active);
}

static void
_action_statusbar_activate (GtkToggleAction* action,
                            MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-statusbar", active, NULL);
    sokoke_widget_set_visible (browser->statusbar, active);
    if (active)
        g_object_set (_action_by_name (browser, "Location"),
                      "progress", 0.0, NULL);
}

static void
_action_reload_stop_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    gchar* stock_id;
    GtkWidget* view;
    GdkModifierType state = (GdkModifierType)0;
    gint x, y;
    gboolean from_cache;

    g_object_get (action, "stock-id", &stock_id, NULL);
    view = midori_browser_get_current_tab (browser);

    /* Refresh or stop, depending on the stock id */
    if (!strcmp (stock_id, GTK_STOCK_REFRESH))
    {
        gdk_window_get_pointer (NULL, &x, &y, &state);
        from_cache = state & GDK_SHIFT_MASK;
        midori_view_reload (MIDORI_VIEW (view), !from_cache);
    }
    else
        midori_view_stop_loading (MIDORI_VIEW (view));
    g_free (stock_id);
}

static void
_action_zoom_in_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_set_zoom_level (MIDORI_VIEW (view),
            midori_view_get_zoom_level (MIDORI_VIEW (view)) + 0.25f);
}

static void
_action_zoom_out_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_set_zoom_level (MIDORI_VIEW (view),
            midori_view_get_zoom_level (MIDORI_VIEW (view)) - 0.25f);
}

static void
_action_zoom_normal_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_set_zoom_level (MIDORI_VIEW (view), 1.0f);
}

static void
_action_source_view_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* view;
    GtkWidget* source_view;
    gchar* uri;
    gint n;

    if (!(view = midori_browser_get_current_tab (browser)))
        return;

    uri = g_strdup_printf ("view-source:%s",
        midori_view_get_display_uri (MIDORI_VIEW (view)));
    source_view = midori_view_new_with_uri (uri);
    g_free (uri);
    gtk_widget_show (source_view);
    n = midori_browser_add_tab (browser, source_view);
    midori_browser_set_current_page (browser, n);
}

static void
_action_fullscreen_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    GdkWindowState state = gdk_window_get_state (GTK_WIDGET (browser)->window);
    if (state & GDK_WINDOW_STATE_FULLSCREEN)
        gtk_window_unfullscreen (GTK_WINDOW (browser));
    else
        gtk_window_fullscreen (GTK_WINDOW (browser));
}

static void
_action_back_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_go_back (MIDORI_VIEW (view));
}

static void
_action_forward_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    if (view)
        midori_view_go_forward (MIDORI_VIEW (view));
}

static void
_action_homepage_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    gchar* homepage;

    g_object_get (browser->settings, "homepage", &homepage, NULL);
    _midori_browser_open_uri (browser, homepage);
    g_free (homepage);
}

static void
_action_location_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    if (!GTK_WIDGET_VISIBLE (browser->navigationbar))
        gtk_widget_show (browser->navigationbar);
}

static void
_action_location_active_changed (GtkAction*     action,
                                 gint           index,
                                 MidoriBrowser* browser)
{
    const gchar* uri;

    if (index > -1)
    {
        uri = midori_location_action_get_uri (MIDORI_LOCATION_ACTION (action));
        _midori_browser_open_uri (browser, uri);
    }
}

static void
_action_location_focus_out (GtkAction*     action,
                            MidoriBrowser* browser)
{
    if (GTK_WIDGET_VISIBLE (browser->statusbar) &&
        !sokoke_object_get_boolean (browser->settings, "show-navigationbar"))
        gtk_widget_hide (browser->navigationbar);
}

static void
_action_location_reset_uri (GtkAction*     action,
                            MidoriBrowser* browser)
{
    const gchar* uri;

    uri = midori_browser_get_current_uri (browser);
    midori_location_action_set_uri (MIDORI_LOCATION_ACTION (action), uri);
}

static void
_action_location_submit_uri (GtkAction*     action,
                             const gchar*   uri,
                             gboolean       new_tab,
                             MidoriBrowser* browser)
{
    gchar* location_entry_search;
    gchar* new_uri;
    gint n;

    g_object_get (browser->settings, "location-entry-search",
                  &location_entry_search, NULL);
    new_uri = sokoke_magic_uri (uri, browser->search_engines);
    if (!new_uri && strstr (location_entry_search, "%s"))
        new_uri = g_strdup_printf (location_entry_search, uri);
    else if (!new_uri)
        new_uri = g_strdup (location_entry_search);
    g_free (location_entry_search);
    if (new_tab)
    {
        n = midori_browser_add_uri (browser, new_uri);
        midori_browser_set_current_page (browser, n);
    }
    else
        _midori_browser_open_uri (browser, new_uri);
    g_free (new_uri);
    gtk_widget_grab_focus (midori_browser_get_current_tab (browser));
}

static void
midori_browser_menu_feed_item_activate_cb (GtkWidget*     widget,
                                           MidoriBrowser* browser)
{
    const gchar* uri;

    uri = g_object_get_data (G_OBJECT (widget), "uri");
    _midori_browser_open_uri (browser, uri);
}

static void
_action_location_secondary_icon_released (GtkAction*     action,
                                          GtkWidget*     widget,
                                          MidoriBrowser* browser)
{
    MidoriView* view;
    KatzeArray* news_feeds;
    GtkWidget* menu;
    guint n, i;
    GjsValue* feed;
    const gchar* uri;
    const gchar* title;
    GtkWidget* menuitem;

    view = (MidoriView*)midori_browser_get_current_tab (browser);
    if (view)
    {
        news_feeds = NULL /* midori_view_get_news_feeds (view) */;
        n = news_feeds ? katze_array_get_length (news_feeds) : 0;
        if (n)
        {
            menu = gtk_menu_new ();
            for (i = 0; i < n; i++)
            {
                if (!(feed = katze_array_get_nth_item (news_feeds, i)))
                    continue;

                uri = gjs_value_get_attribute_string (feed, "href");
                if (gjs_value_has_attribute (feed, "title"))
                    title = gjs_value_get_attribute_string (feed, "title");
                else
                    title = uri;
                if (!*title)
                    title = uri;
                menuitem = sokoke_image_menu_item_new_ellipsized (title);
                /* FIXME: Get the real icon */
                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (
                    menuitem), gtk_image_new_from_stock (STOCK_NEWS_FEED,
                    GTK_ICON_SIZE_MENU));
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
                g_object_set_data (G_OBJECT (menuitem), "uri", (gchar*)uri);
                g_signal_connect (menuitem, "activate",
                    G_CALLBACK (midori_browser_menu_feed_item_activate_cb),
                    browser);
                gtk_widget_show (menuitem);
            }
            sokoke_widget_popup (widget, GTK_MENU (menu), NULL,
                                 SOKOKE_MENU_POSITION_CURSOR);
        }
    }
}

static void
_action_search_activate (GtkAction*     action,
                         MidoriBrowser* browser)
{
    if (!GTK_WIDGET_VISIBLE (browser->search))
        gtk_widget_show (browser->search);
    if (!GTK_WIDGET_VISIBLE (browser->navigationbar))
        gtk_widget_show (browser->navigationbar);
    gtk_widget_grab_focus (browser->search);
}

static gboolean
midori_browser_search_focus_out_event_cb (GtkWidget*     widget,
                                          GdkEventFocus* event,
                                          MidoriBrowser* browser)
{
    gboolean show_navigationbar;
    gboolean show_web_search;

    g_object_get (browser->settings,
                  "show-navigationbar", &show_navigationbar,
                  "show-web-search", &show_web_search,
                  NULL);
    if (!show_navigationbar)
        gtk_widget_hide (browser->navigationbar);
    if (!show_web_search)
        gtk_widget_hide (browser->search);
    return FALSE;
}

static void
midori_panel_bookmarks_row_activated_cb (GtkTreeView*       treeview,
                                         GtkTreePath*       path,
                                         GtkTreeViewColumn* column,
                                         MidoriBrowser*     browser)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    const gchar* uri;

    model = gtk_tree_view_get_model (treeview);

    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);
        if (uri && *uri)
            _midori_browser_open_uri (browser, uri);
    }
}

static void
midori_panel_bookmarks_cursor_or_row_changed_cb (GtkTreeView*   tree_view,
                                                 MidoriBrowser* browser)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    gboolean is_separator;

    if (sokoke_tree_view_get_selected_iter (tree_view, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);

        is_separator = !KATZE_IS_ARRAY (item) && !katze_item_get_uri (item);
        _action_set_sensitive (browser, "BookmarkEdit", !is_separator);
        _action_set_sensitive (browser, "BookmarkDelete", TRUE);
    }
    else
    {
        _action_set_sensitive (browser, "BookmarkEdit", FALSE);
        _action_set_sensitive (browser, "BookmarkDelete", FALSE);
    }
}

static void
_midori_panel_bookmarks_popup (GtkWidget*      widget,
                               GdkEventButton* event,
                               KatzeItem*      item,
                               MidoriBrowser*  browser)
{
    const gchar* uri;

    uri = katze_item_get_uri (item);

    _action_set_sensitive (browser, "BookmarkOpen", uri != NULL);
    _action_set_sensitive (browser, "BookmarkOpenTab", uri != NULL);
    _action_set_sensitive (browser, "BookmarkOpenWindow", uri != NULL);

    sokoke_widget_popup (widget, GTK_MENU (browser->popup_bookmark),
                         event, SOKOKE_MENU_POSITION_CURSOR);
}

static gboolean
midori_panel_bookmarks_button_release_event_cb (GtkWidget*      widget,
                                                GdkEventButton* event,
                                                MidoriBrowser*  browser)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    const gchar* uri;
    gint n;

    if (event->button != 2 && event->button != 3)
        return FALSE;

    if (sokoke_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);
        if (event->button == 2)
        {
            if (uri && *uri)
            {
                n = midori_browser_add_uri (browser, uri);
                midori_browser_set_current_page (browser, n);
            }
        }
        else
            _midori_panel_bookmarks_popup (widget, event, item, browser);
        return TRUE;
    }
    return FALSE;
}

static void
midori_panel_bookmarks_popup_menu_cb (GtkWidget*     widget,
                                      MidoriBrowser* browser)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    if (sokoke_tree_view_get_selected_iter (GTK_TREE_VIEW (widget), &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        _midori_panel_bookmarks_popup (widget, NULL, item, browser);
    }
}

static void
_tree_store_insert_folder (GtkTreeStore* treestore,
                           GtkTreeIter*  parent,
                           KatzeArray*   array)
{
    guint n, i;
    KatzeItem* item;
    GtkTreeIter iter;

    n = katze_array_get_length (array);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (array, i);
        gtk_tree_store_insert_with_values (treestore, &iter, parent, n,
                                           0, item, -1);
        g_object_ref (item);
        if (KATZE_IS_ARRAY (item))
            _tree_store_insert_folder (treestore, &iter, KATZE_ARRAY (item));
    }
}

static void
midori_browser_bookmarks_item_render_icon_cb (GtkTreeViewColumn* column,
                                              GtkCellRenderer*   renderer,
                                              GtkTreeModel*      model,
                                              GtkTreeIter*       iter,
                                              GtkWidget*         treeview)
{
    KatzeItem* item;
    GdkPixbuf* pixbuf;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (G_UNLIKELY (!item))
        return;
    if (G_UNLIKELY (!katze_item_get_parent (item)))
    {
        gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
        g_object_unref (item);
        return;
    }

    /* TODO: Would it be better to not do this on every redraw? */
    pixbuf = NULL;
    if (KATZE_IS_ARRAY (item))
        pixbuf = gtk_widget_render_icon (treeview, GTK_STOCK_DIRECTORY,
                                         GTK_ICON_SIZE_MENU, NULL);
    else if (katze_item_get_uri (item))
        pixbuf = gtk_widget_render_icon (treeview, STOCK_BOOKMARK,
                                         GTK_ICON_SIZE_MENU, NULL);
    g_object_set (renderer, "pixbuf", pixbuf, NULL);
    if (pixbuf)
        g_object_unref (pixbuf);
}

static void
midori_browser_bookmarks_item_render_text_cb (GtkTreeViewColumn* column,
                                              GtkCellRenderer*   renderer,
                                              GtkTreeModel*      model,
                                              GtkTreeIter*       iter,
                                              GtkWidget*         treeview)
{
    KatzeItem* item;

    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (G_UNLIKELY (!item))
        return;
    if (G_UNLIKELY (!katze_item_get_parent (item)))
    {
        gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
        g_object_unref (item);
        return;
    }

    if (KATZE_IS_ARRAY (item) || katze_item_get_uri (item))
        g_object_set (renderer, "markup", NULL,
                      "text", katze_item_get_name (item), NULL);
    else
        g_object_set (renderer, "markup", _("<i>Separator</i>"), NULL);
}

static void
_midori_browser_create_bookmark_menu (MidoriBrowser* browser,
                                      KatzeArray*    array,
                                      GtkWidget*     menu);

static void
midori_browser_bookmark_menu_folder_activate_cb (GtkWidget*     menuitem,
                                                 MidoriBrowser* browser)
{
    GtkWidget* menu;
    KatzeArray* array;

    menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem));
    gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback) gtk_widget_destroy, NULL);
    array = (KatzeArray*)g_object_get_data (G_OBJECT (menuitem), "KatzeArray");
    _midori_browser_create_bookmark_menu (browser, array, menu);
    /* Remove all menuitems when the menu is hidden.
       FIXME: We really *want* the line below, but it won't work like that
       g_signal_connect_after (menu, "hide", G_CALLBACK (gtk_container_foreach), gtk_widget_destroy); */
    gtk_widget_show (menuitem);
}

static void
midori_browser_bookmarkbar_folder_activate_cb (GtkToolItem*   toolitem,
                                               MidoriBrowser* browser)
{
    GtkWidget* menu;
    KatzeArray* array;

    menu = gtk_menu_new ();
    array = (KatzeArray*)g_object_get_data (G_OBJECT (toolitem), "KatzeArray");
    _midori_browser_create_bookmark_menu (browser, array, menu);
    /* Remove all menuitems when the menu is hidden.
       FIXME: We really *should* run the line below, but it won't work like that
       g_signal_connect (menu, "hide", G_CALLBACK (gtk_container_foreach),
                         gtk_widget_destroy); */
    sokoke_widget_popup (GTK_WIDGET (toolitem), GTK_MENU (menu),
		         NULL, SOKOKE_MENU_POSITION_LEFT);
}

static void
midori_browser_menu_bookmarks_item_activate_cb (GtkWidget*     widget,
                                                MidoriBrowser* browser)
{
    KatzeItem* item;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (widget), "KatzeItem");
    _midori_browser_open_uri (browser, katze_item_get_uri (item));
    gtk_widget_grab_focus (midori_browser_get_current_tab (browser));
}

static void
_midori_browser_create_bookmark_menu (MidoriBrowser* browser,
                                      KatzeArray*    array,
                                      GtkWidget*     menu)
{
    guint i, n;
    KatzeItem* item;
    const gchar* title;
    GtkWidget* menuitem;
    GtkWidget* submenu;
    GtkWidget* icon;

    n = katze_array_get_length (array);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (array, i);
        title = katze_item_get_name (item);

        if (KATZE_IS_ARRAY (item))
        {
            /* FIXME: what about the "folded" status */
            menuitem = sokoke_image_menu_item_new_ellipsized (title);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
                gtk_image_new_from_stock (GTK_STOCK_DIRECTORY,
                                          GTK_ICON_SIZE_MENU));
            submenu = gtk_menu_new ();
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_browser_bookmark_menu_folder_activate_cb),
                browser);
            g_object_set_data (G_OBJECT (menuitem), "KatzeArray", item);
        }
        else if (katze_item_get_uri (item))
        {
            menuitem = sokoke_image_menu_item_new_ellipsized (title);
            icon = gtk_image_new_from_stock (STOCK_BOOKMARK, GTK_ICON_SIZE_MENU);
            gtk_widget_show (icon);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_browser_menu_bookmarks_item_activate_cb),
                browser);
            g_object_set_data (G_OBJECT (menuitem), "KatzeItem", item);
        }
        else
            menuitem = gtk_separator_menu_item_new ();
         gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
         gtk_widget_show (menuitem);
    }
}

static void
_action_bookmark_add_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    midori_browser_edit_bookmark_dialog_new (browser, NULL);
}

static void
_action_manage_search_engines_activate (GtkAction*     action,
                                        MidoriBrowser* browser)
{
    GtkWidget* dialog;

    dialog = midori_search_entry_get_dialog (
        MIDORI_SEARCH_ENTRY (browser->search));
    if (GTK_WIDGET_VISIBLE (dialog))
        gtk_window_present (GTK_WINDOW (dialog));
    else
        gtk_widget_show (dialog);
}

static void
_action_tab_previous_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    gint n = gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
    gtk_notebook_set_current_page (GTK_NOTEBOOK (browser->notebook), n - 1);
}

static void
_action_tab_next_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    /* Advance one tab or jump to the first one if we are at the last one */
    gint n = gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
    if (n == gtk_notebook_get_n_pages (GTK_NOTEBOOK (browser->notebook)) - 1)
        n = -1;
    gtk_notebook_set_current_page (GTK_NOTEBOOK (browser->notebook), n + 1);
}

static const gchar* credits_authors[] = {
    "Christian Dywan <christian@twotoasts.de>", NULL };
static const gchar* credits_documenters[] = {
    "Christian Dywan <christian@twotoasts.de>" };
static const gchar* credits_artists[] = {
    "Nancy Runge <nancy@twotoasts.de>", NULL };

static const gchar* license =
 "This library is free software; you can redistribute it and/or\n"
 "modify it under the terms of the GNU Lesser General Public\n"
 "License as published by the Free Software Foundation; either\n"
 "version 2.1 of the License, or (at your option) any later version.\n";

static void
_action_about_activate_link (GtkAboutDialog* about,
                             const gchar*    link,
                             gpointer        user_data)
{
    MidoriBrowser* browser;
    gint n;

    browser = MIDORI_BROWSER (user_data);
    n = midori_browser_add_uri (browser, link);
    midori_browser_set_current_page (browser, n);
}

static void
_action_about_activate_email (GtkAboutDialog* about,
                              const gchar*    link,
                              gpointer        user_data)
{
    gchar* command = g_strconcat ("xdg-open ", link, NULL);
    g_spawn_command_line_async (command, NULL);
    g_free (command);
}

static void
_action_about_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
    gtk_about_dialog_set_email_hook (_action_about_activate_email, NULL, NULL);
    gtk_about_dialog_set_url_hook (_action_about_activate_link, browser, NULL);
    gtk_show_about_dialog (GTK_WINDOW (browser),
        "logo-icon-name", gtk_window_get_icon_name (GTK_WINDOW (browser)),
        "name", PACKAGE_NAME,
        "version", PACKAGE_VERSION,
        "comments", _("A lightweight web browser."),
        "copyright", "Copyright © 2007-2008 Christian Dywan",
        "website", "http://www.twotoasts.de",
        "authors", credits_authors,
        "documenters", credits_documenters,
        "artists", credits_artists,
        "license", license,
        "wrap-license", TRUE,
        "translator-credits", _("translator-credits"),
        NULL);
}

static void
_action_help_link_activate (GtkAction*     action,
                            MidoriBrowser* browser)
{
    const gchar* action_name;
    const gchar* uri;
    gint n;

    action_name = gtk_action_get_name (action);
    if  (!strncmp ("HelpContents", action_name, 12))
    {
        #ifdef DOCDIR
        uri = DOCDIR "/midori/user/midori.html";
        if (!g_file_test (uri, G_FILE_TEST_EXISTS))
            uri = "error:nodocs " DOCDIR "/midori/user/midori.html";
        #else
        uri = "error:nodocs " DATADIR "/doc/midori/user/midori.html";
        #endif
    }
    else if  (!strncmp ("HelpFAQ", action_name, 7))
        uri = "http://wiki.xfce.org/_export/xhtml/midori_faq";
    else if  (!strncmp ("HelpBugs", action_name, 8))
        uri = "http://www.twotoasts.de/bugs/";
    else
        uri = NULL;

    if (uri)
    {
        n = midori_browser_add_uri (browser, uri);
        midori_browser_set_current_page (browser, n);
    }

}


static void
_action_panel_activate (GtkToggleAction* action,
                        MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-panel", active, NULL);
    sokoke_widget_set_visible (browser->panel, active);
}

static void
_action_open_in_panel_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    GtkWidget* view;
    const gchar* uri;
    gint n;

    view = midori_browser_get_current_tab (browser);
    uri = midori_view_get_display_uri (MIDORI_VIEW (view));
    /* FIXME: Don't assign the uri here, update it properly while navigating */
    g_object_set (browser->settings, "last-pageholder-uri", uri, NULL);
    n = midori_panel_page_num (MIDORI_PANEL (browser->panel),
                               browser->panel_pageholder);
    midori_panel_set_current_page (MIDORI_PANEL (browser->panel), n);
    gtk_widget_show (browser->panel);
    midori_view_set_uri (MIDORI_VIEW (browser->panel_pageholder), uri);
}


static void
midori_panel_notify_position_cb (GObject*       object,
                                 GParamSpec*    arg1,
                                 MidoriBrowser* browser)
{
    gboolean position = gtk_paned_get_position (GTK_PANED (object));
    g_object_set (browser->settings, "last-panel-position", position, NULL);
}

static gboolean
midori_panel_close_cb (MidoriPanel*   panel,
                       MidoriBrowser* browser)
{
    _action_set_active (browser, "Panel", FALSE);
    return FALSE;
}

static void
gtk_notebook_switch_page_cb (GtkWidget*       notebook,
                             GtkNotebookPage* page,
                             guint            page_num,
                             MidoriBrowser*   browser)
{
    GtkWidget* view;
    const gchar* uri;
    GtkAction* action;
    const gchar* title;
    gchar* window_title;

    view = midori_browser_get_current_tab (browser);
    uri = midori_view_get_display_uri (MIDORI_VIEW (view));
    action = _action_by_name (browser, "Location");
    midori_location_action_set_uri (MIDORI_LOCATION_ACTION (action), uri);

    title = midori_view_get_display_title (MIDORI_VIEW (view));
    window_title = g_strconcat (title, " - ",
                                g_get_application_name (), NULL);
    gtk_window_set_title (GTK_WINDOW (browser), window_title);
    g_free (window_title);

    g_object_notify (G_OBJECT (browser), "uri");

    _midori_browser_set_statusbar_text (browser, NULL);
    _midori_browser_update_interface (browser);
    _midori_browser_update_progress (browser, MIDORI_VIEW (view));
}

static void
_action_bookmark_open_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    GtkTreeView* tree_view;
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    const gchar* uri;

    tree_view = GTK_TREE_VIEW (browser->panel_bookmarks);
    if (sokoke_tree_view_get_selected_iter (tree_view, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);
        if (uri && *uri)
            _midori_browser_open_uri (browser, uri);
    }
}

static void
_action_bookmark_open_tab_activate (GtkAction*     action,
                                    MidoriBrowser* browser)
{
    GtkTreeView* tree_view;
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    const gchar* uri;
    gint n;

    tree_view = GTK_TREE_VIEW (browser->panel_bookmarks);
    if (sokoke_tree_view_get_selected_iter (tree_view, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);
        if (uri && *uri)
        {
            n = midori_browser_add_item (browser, item);
            _midori_browser_set_current_page_smartly (browser, n);
        }
    }
}

static void
_action_bookmark_open_window_activate (GtkAction*     action,
                                       MidoriBrowser* browser)
{
    GtkTreeView* tree_view;
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    const gchar* uri;
    gint n;

    tree_view = GTK_TREE_VIEW (browser->panel_bookmarks);
    if (sokoke_tree_view_get_selected_iter (tree_view, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        uri = katze_item_get_uri (item);
        if (uri && *uri)
        {
            n = midori_browser_add_item (browser, item);
            _midori_browser_set_current_page_smartly (browser, n);
        }
    }
}

static void
_action_bookmark_edit_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    GtkTreeView* tree_view;
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;

    tree_view = GTK_TREE_VIEW (browser->panel_bookmarks);
    if (sokoke_tree_view_get_selected_iter (tree_view, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        /* if (katze_item_get_uri (item)) */
            midori_browser_edit_bookmark_dialog_new (browser, item);
    }
}

static void
_action_undo_tab_close_activate (GtkAction*     action,
                                 MidoriBrowser* browser)
{
    guint last;
    KatzeItem* item;
    guint n;

    /* Reopen the most recent trash item */
    last = katze_array_get_length (browser->trash) - 1;
    item = katze_array_get_nth_item (browser->trash, last);
    n = midori_browser_add_item (browser, item);
    midori_browser_set_current_page (browser, n);
    katze_array_remove_item (browser->trash, item);
    _midori_browser_update_actions (browser);
}

static void
_action_trash_empty_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    katze_array_clear (browser->trash);
    _midori_browser_update_actions (browser);
}

static void
_action_bookmark_delete_activate (GtkAction*     action,
                                  MidoriBrowser* browser)
{
    GtkTreeView* tree_view;
    GtkTreeModel* model;
    GtkTreeIter iter;
    KatzeItem* item;
    KatzeArray* parent;

    tree_view = GTK_TREE_VIEW (browser->panel_bookmarks);
    if (sokoke_tree_view_get_selected_iter (tree_view, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        parent = katze_item_get_parent (item);
        katze_array_remove_item (parent, item);
        /* FIXME: This is a preliminary hack, until we fix it properly again */
        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
        g_object_unref (item);
    }
}

static const GtkActionEntry entries[] = {
 { "File", NULL, N_("_File") },
 { "WindowNew", STOCK_WINDOW_NEW,
   NULL, "<Ctrl>n",
   N_("Open a new window"), G_CALLBACK (_action_window_new_activate) },
 { "TabNew", STOCK_TAB_NEW,
   NULL, "<Ctrl>t",
   N_("Open a new tab"), G_CALLBACK (_action_tab_new_activate) },
 { "Open", GTK_STOCK_OPEN,
   NULL, "<Ctrl>o",
   N_("Open a file"), G_CALLBACK (_action_open_activate) },
 { "SaveAs", GTK_STOCK_SAVE_AS,
   NULL, "<Ctrl>s",
   N_("Save to a file"), NULL/*G_CALLBACK (_action_saveas_activate)*/ },
 { "TabClose", NULL,
   N_("_Close Tab"), "<Ctrl>w",
   N_("Close the current tab"), G_CALLBACK (_action_tab_close_activate) },
 { "WindowClose", NULL,
   N_("C_lose Window"), "<Ctrl><Shift>w",
   N_("Close this window"), G_CALLBACK (_action_window_close_activate) },
 { "Print", GTK_STOCK_PRINT,
   NULL, "<Ctrl>p",
   N_("Print the current page"), G_CALLBACK (_action_print_activate) },
 { "Quit", GTK_STOCK_QUIT,
   NULL, "<Ctrl>q",
   N_("Quit the application"), G_CALLBACK (_action_quit_activate) },

 { "Edit", NULL, N_("_Edit"), NULL, NULL, G_CALLBACK (_action_edit_activate) },
 { "Cut", GTK_STOCK_CUT,
   NULL, "<Ctrl>x",
   N_("Cut the selected text"), G_CALLBACK (_action_cut_activate) },
 { "Copy", GTK_STOCK_COPY,
   NULL, "<Ctrl>c",
   N_("Copy the selected text"), G_CALLBACK (_action_copy_activate) },
 { "Copy_", GTK_STOCK_COPY,
   NULL, "<Ctrl>c",
   N_("Copy the selected text"), G_CALLBACK (_action_copy_activate) },
 { "Paste", GTK_STOCK_PASTE,
   NULL, "<Ctrl>v",
   N_("Paste text from the clipboard"), G_CALLBACK (_action_paste_activate) },
 { "Delete", GTK_STOCK_DELETE,
   NULL, NULL,
   N_("Delete the selected text"), G_CALLBACK (_action_delete_activate) },
 { "SelectAll", GTK_STOCK_SELECT_ALL,
   NULL, "<Ctrl>a",
   N_("Select all text"), G_CALLBACK (_action_select_all_activate) },
 { "Find", GTK_STOCK_FIND,
   NULL, "<Ctrl>f",
   N_("Find a word or phrase in the page"), G_CALLBACK (_action_find_activate) },
 { "FindNext", GTK_STOCK_GO_FORWARD,
   N_("Find _Next"), "<Ctrl>g",
   N_("Find the next occurrence of a word or phrase"), G_CALLBACK (_action_find_next_activate) },
 { "FindPrevious", GTK_STOCK_GO_BACK,
   N_("Find _Previous"), "<Ctrl><Shift>g",
   N_("Find the previous occurrence of a word or phrase"), G_CALLBACK (_action_find_previous_activate) },
 { "FindQuick", GTK_STOCK_FIND,
   N_("_Quick Find"), "period",
   N_("Quickly jump to a word or phrase"), NULL/*G_CALLBACK (_action_find_quick_activate)*/ },
 { "Preferences", GTK_STOCK_PREFERENCES,
   NULL, "<Ctrl><Alt>p",
   N_("Configure the application preferences"), G_CALLBACK (_action_preferences_activate) },

 { "View", NULL, N_("_View") },
 { "Toolbars", NULL, N_("_Toolbars") },
 { "Reload", GTK_STOCK_REFRESH,
   NULL, "<Ctrl>r",
   N_("Reload the current page"), G_CALLBACK (_action_reload_stop_activate) },
 { "Stop", GTK_STOCK_STOP,
   NULL, "Escape",
   N_("Stop loading the current page"), G_CALLBACK (_action_reload_stop_activate) },
 { "ReloadStop", GTK_STOCK_STOP,
   NULL, "<Ctrl>r",
   N_("Reload the current page"), G_CALLBACK (_action_reload_stop_activate) },
 { "ZoomIn", GTK_STOCK_ZOOM_IN,
   NULL, "<Ctrl>plus",
   N_("Increase the zoom level"), G_CALLBACK (_action_zoom_in_activate) },
 { "ZoomOut", GTK_STOCK_ZOOM_OUT,
   NULL, "<Ctrl>minus",
   N_("Decrease the zoom level"), G_CALLBACK (_action_zoom_out_activate) },
 { "ZoomNormal", GTK_STOCK_ZOOM_100,
   NULL, "<Ctrl>0",
   N_("Reset the zoom level"), G_CALLBACK (_action_zoom_normal_activate) },
 { "SourceView", NULL,
   N_("View _Source"), "",
   N_("View the source code of the page"), G_CALLBACK (_action_source_view_activate) },
 { "SelectionSourceView", NULL,
    N_("View Selection Source"), "",
    N_("View the source code of the selection"), NULL/*G_CALLBACK (_action_selection_source_view_activate)*/ },
 { "Fullscreen", GTK_STOCK_FULLSCREEN,
   NULL, "F11",
   N_("Toggle fullscreen view"), G_CALLBACK (_action_fullscreen_activate) },

 { "Go", NULL, N_("_Go") },
 { "Back", GTK_STOCK_GO_BACK,
   NULL, "<Alt>Left",
   N_("Go back to the previous page"), G_CALLBACK (_action_back_activate) },
 { "Forward", GTK_STOCK_GO_FORWARD,
   NULL, "<Alt>Right",
   N_("Go forward to the next page"), G_CALLBACK (_action_forward_activate) },
 { "Homepage", STOCK_HOMEPAGE,
   NULL, "<Alt>Home",
   N_("Go to your homepage"), G_CALLBACK (_action_homepage_activate) },
 { "Search", GTK_STOCK_FIND,
   N_("_Web Search..."), "<Ctrl>k",
   N_("Run a web search"), G_CALLBACK (_action_search_activate) },
 { "OpenInPageholder", GTK_STOCK_JUMP_TO,
   N_("Open in Page_holder..."), "",
   N_("Open the current page in the pageholder"), G_CALLBACK (_action_open_in_panel_activate) },
 { "Trash", STOCK_USER_TRASH,
   NULL, "",
/*  N_("Reopen a previously closed tab or window"), G_CALLBACK (_action_trash_activate) }, */
   N_("Reopen a previously closed tab or window"), NULL },
 { "TrashEmpty", GTK_STOCK_CLEAR,
   N_("Empty Trash"), "",
   N_("Delete the contents of the trash"), G_CALLBACK (_action_trash_empty_activate) },
 { "UndoTabClose", GTK_STOCK_UNDELETE,
   N_("Undo Close Tab"), "<Ctrl><Shift>t",
   N_("Open the last closed tab"), G_CALLBACK (_action_undo_tab_close_activate) },

 { "Bookmarks", NULL, N_("_Bookmarks") },
 { "BookmarkAdd", STOCK_BOOKMARK_ADD,
   NULL, "<Ctrl>d",
   N_("Add a new bookmark"), G_CALLBACK (_action_bookmark_add_activate) },
 { "BookmarkOpen", GTK_STOCK_OPEN,
   NULL, "",
   N_("Open the selected bookmark"), G_CALLBACK (_action_bookmark_open_activate) },
 { "BookmarkOpenTab", STOCK_TAB_NEW,
   N_("Open in New _Tab"), "",
   N_("Open the selected bookmark in a new tab"), G_CALLBACK (_action_bookmark_open_tab_activate) },
 { "BookmarkOpenWindow", STOCK_WINDOW_NEW,
   N_("Open in New _Window"), "",
   N_("Open the selected bookmark in a new window"), G_CALLBACK (_action_bookmark_open_window_activate) },
 { "BookmarkEdit", GTK_STOCK_EDIT,
   NULL, "",
   N_("Edit the selected bookmark"), G_CALLBACK (_action_bookmark_edit_activate) },
 { "BookmarkDelete", GTK_STOCK_DELETE,
   NULL, "",
   N_("Delete the selected bookmark"), G_CALLBACK (_action_bookmark_delete_activate) },

 { "Tools", NULL, N_("_Tools") },
 { "ManageSearchEngines", GTK_STOCK_PROPERTIES,
   N_("_Manage Search Engines"), "<Ctrl><Alt>s",
   N_("Add, edit and remove search engines..."),
   G_CALLBACK (_action_manage_search_engines_activate) },

 { "Window", NULL, N_("_Window") },
 { "TabPrevious", GTK_STOCK_GO_BACK,
   N_("_Previous Tab"), "<Ctrl>Page_Up",
   N_("Switch to the previous tab"), G_CALLBACK (_action_tab_previous_activate) },
 { "TabNext", GTK_STOCK_GO_FORWARD,
   N_("_Next Tab"), "<Ctrl>Page_Down",
   N_("Switch to the next tab"), G_CALLBACK (_action_tab_next_activate) },

 { "Help", NULL, N_("_Help") },
 { "HelpContents", GTK_STOCK_HELP,
   N_("_Contents"), "F1",
   N_("Show the documentation"), G_CALLBACK (_action_help_link_activate) },
 { "HelpFAQ", NULL,
   N_("_Frequent questions"), NULL,
   N_("Show the Frequently Asked Questions"), G_CALLBACK (_action_help_link_activate) },
 { "HelpBugs", NULL,
   N_("_Report a bug"), NULL,
   N_("Open Midori's bug tracker"), G_CALLBACK (_action_help_link_activate) },
 { "About", GTK_STOCK_ABOUT,
   NULL, "",
   N_("Show information about the program"), G_CALLBACK (_action_about_activate) },
 };
 static const guint entries_n = G_N_ELEMENTS (entries);

static const GtkToggleActionEntry toggle_entries[] = {
 { "PrivateBrowsing", NULL,
   N_("P_rivate Browsing"), "",
   N_("Don't save any private data while browsing"), NULL/*G_CALLBACK (_action_private_browsing_activate)*/,
   FALSE },

 { "Navigationbar", NULL,
   N_("_Navigationbar"), "",
   N_("Show navigationbar"), G_CALLBACK (_action_navigationbar_activate),
   FALSE },
 { "Panel", NULL,
   N_("Side_panel"), "F9",
   N_("Show sidepanel"), G_CALLBACK (_action_panel_activate),
   FALSE },
 { "Bookmarkbar", NULL,
   N_("_Bookmarkbar"), "",
   N_("Show bookmarkbar"), G_CALLBACK (_action_bookmarkbar_activate),
   FALSE },
 { "Transferbar", NULL,
   N_("_Transferbar"), "",
   N_("Show transferbar"), NULL/*G_CALLBACK (_action_transferbar_activate)*/,
   FALSE },
 { "Statusbar", NULL,
   N_("_Statusbar"), "",
   N_("Show statusbar"), G_CALLBACK (_action_statusbar_activate),
   FALSE },
 };
 static const guint toggle_entries_n = G_N_ELEMENTS (toggle_entries);

static void
midori_browser_window_state_event_cb (MidoriBrowser*       browser,
                                      GdkEventWindowState* event)
{
    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
        {
            gtk_widget_hide (browser->menubar);
            g_object_set (browser->button_fullscreen,
                          "stock-id", GTK_STOCK_LEAVE_FULLSCREEN, NULL);
            gtk_widget_show (browser->button_fullscreen);
        }
        else
        {
            gtk_widget_show (browser->menubar);
            gtk_widget_hide (browser->button_fullscreen);
            g_object_set (browser->button_fullscreen,
                          "stock-id", GTK_STOCK_FULLSCREEN, NULL);
        }
    }
}

static void
midori_browser_size_allocate_cb (MidoriBrowser* browser,
                                 GtkAllocation* allocation)
{
    GtkWidget* widget = GTK_WIDGET (browser);

    if (GTK_WIDGET_REALIZED (widget))
    {
        GdkWindowState state = gdk_window_get_state (widget->window);
        if (!(state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)))
        {
            g_object_set (browser->settings,
                          "last-window-width", allocation->width,
                          "last-window-height", allocation->height, NULL);
        }
    }
}

static void
midori_browser_destroy_cb (MidoriBrowser* browser)
{
    /* Destroy tabs first, so child widgets don't need special care */
    gtk_container_foreach (GTK_CONTAINER (browser->notebook),
                           (GtkCallback) gtk_widget_destroy, NULL);
}

static const gchar* ui_markup =
 "<ui>"
  "<menubar>"
   "<menu action='File'>"
    "<menuitem action='WindowNew'/>"
    "<menuitem action='TabNew'/>"
    "<separator/>"
    "<menuitem action='Open'/>"
    "<separator/>"
    "<menuitem action='SaveAs'/>"
    "<separator/>"
    "<menuitem action='TabClose'/>"
    "<menuitem action='WindowClose'/>"
    "<separator/>"
    "<menuitem action='Print'/>"
    "<menuitem action='PrivateBrowsing'/>"
    "<separator/>"
    "<menuitem action='Quit'/>"
   "</menu>"
   "<menu action='Edit'>"
    "<menuitem action='Cut'/>"
    "<menuitem action='Copy'/>"
    "<menuitem action='Paste'/>"
    "<menuitem action='Delete'/>"
    "<separator/>"
    "<menuitem action='SelectAll'/>"
    "<separator/>"
    "<menuitem action='Preferences'/>"
   "</menu>"
   "<menu action='View'>"
    "<menu action='Toolbars'>"
     "<menuitem action='Navigationbar'/>"
     "<menuitem action='Bookmarkbar'/>"
     "<menuitem action='Transferbar'/>"
     "<menuitem action='Statusbar'/>"
    "</menu>"
    "<menuitem action='Panel'/>"
    "<separator/>"
    "<menuitem action='Reload'/>"
    "<menuitem action='Stop'/>"
    "<separator/>"
    "<menuitem action='ZoomIn'/>"
    "<menuitem action='ZoomOut'/>"
    "<menuitem action='ZoomNormal'/>"
    "<separator/>"
    "<menuitem action='SourceView'/>"
    "<menuitem action='Fullscreen'/>"
   "</menu>"
   "<menu action='Go'>"
    "<menuitem action='Back'/>"
    "<menuitem action='Forward'/>"
    "<menuitem action='Homepage'/>"
    "<menuitem action='Location'/>"
    "<menuitem action='Search'/>"
    "<menuitem action='OpenInPageholder'/>"
    "<menu action='Trash'>"
    /* Closed tabs shall be prepended here */
     "<separator/>"
     "<menuitem action='TrashEmpty'/>"
    "</menu>"
    "<menuitem action='UndoTabClose'/>"
    "<separator/>"
    "<menuitem action='Find'/>"
    "<menuitem action='FindNext'/>"
    "<menuitem action='FindPrevious'/>"
   "</menu>"
   "<menu action='Bookmarks'>"
    "<menuitem action='BookmarkAdd'/>"
    "<separator/>"
    /* Bookmarks shall be appended here */
   "</menu>"
   "<menu action='Tools'>"
    "<menuitem action='ManageSearchEngines'/>"
    /* Panel items shall be appended here */
   "</menu>"
   "<menu action='Window'>"
    "<menuitem action='TabPrevious'/>"
    "<menuitem action='TabNext'/>"
    "<separator/>"
    /* All open tabs shall be appended here */
   "</menu>"
   "<menu action='Help'>"
    "<menuitem action='HelpContents'/>"
    "<menuitem action='HelpFAQ'/>"
    "<menuitem action='HelpBugs'/>"
    "<separator/>"
    "<menuitem action='About'/>"
   "</menu>"
  "</menubar>"
  "<toolbar name='toolbar_navigation'>"
   "<toolitem action='TabNew'/>"
   "<toolitem action='Back'/>"
   "<toolitem action='Forward'/>"
   "<toolitem action='ReloadStop'/>"
   "<toolitem action='Homepage'/>"
   "<toolitem action='Location'/>"
   "<placeholder name='Search'/>"
   "<placeholder name='Trash'/>"
  "</toolbar>"
  "<toolbar name='toolbar_bookmarks'>"
   "<toolitem action='BookmarkAdd'/>"
   "<toolitem action='BookmarkEdit'/>"
   "<toolitem action='BookmarkDelete'/>"
  "</toolbar>"
  "<popup name='popup_bookmark'>"
   "<menuitem action='BookmarkOpen'/>"
   "<menuitem action='BookmarkOpenTab'/>"
   "<menuitem action='BookmarkOpenWindow'/>"
   "<separator/>"
   "<menuitem action='BookmarkEdit'/>"
   "<menuitem action='BookmarkDelete'/>"
  "</popup>"
 "</ui>";

static void
midori_browser_realize_cb (GtkStyle*      style,
                           MidoriBrowser* browser)
{
    GdkScreen* screen;
    GtkIconTheme* icon_theme;

    screen = gtk_widget_get_screen (GTK_WIDGET (browser));
    if (screen)
    {
        icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, "midori"))
            gtk_window_set_icon_name (GTK_WINDOW (browser), "midori");
        else
            gtk_window_set_icon_name (GTK_WINDOW (browser), "web-browser");
    }
}

static void
midori_browser_search_activate_cb (GtkWidget*     widget,
                                   MidoriBrowser* browser)
{
    KatzeArray* search_engines;
    const gchar* keywords;
    guint last_web_search;
    KatzeItem* item;
    const gchar* url;
    gchar* location_entry_search;
    gchar* search;

    search_engines = browser->search_engines;
    keywords = gtk_entry_get_text (GTK_ENTRY (widget));
    g_object_get (browser->settings, "last-web-search", &last_web_search, NULL);
    item = katze_array_get_nth_item (search_engines, last_web_search);
    if (item)
    {
        location_entry_search = NULL;
        url = katze_item_get_uri (item);
    }
    else /* The location entry search is our fallback */
    {
        g_object_get (browser->settings, "location-entry-search",
                      &location_entry_search, NULL);
        url = location_entry_search;
    }
    if (strstr (url, "%s"))
        search = g_strdup_printf (url, keywords);
    else
        search = g_strconcat (url, " ", keywords, NULL);
    sokoke_entry_append_completion (GTK_ENTRY (widget), keywords);
    _midori_browser_open_uri (browser, search);
    g_free (search);
    g_free (location_entry_search);
}

static void
midori_browser_entry_clear_icon_released_cb (GtkIconEntry* entry,
                                             gint          icon_pos,
                                             gint          button,
                                             gpointer      user_data)
{
    if (icon_pos == GTK_ICON_ENTRY_SECONDARY)
        gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static void
midori_browser_search_notify_current_item_cb (GObject*       gobject,
                                              GParamSpec*    arg1,
                                              MidoriBrowser* browser)
{
    MidoriSearchEntry* search_entry;
    KatzeItem* item;
    guint index;

    search_entry = MIDORI_SEARCH_ENTRY (browser->search);
    item = midori_search_entry_get_current_item (search_entry);
    if (item)
        index = katze_array_get_item_index (browser->search_engines, item);
    else
        index = 0;

    g_object_set (browser->settings, "last-web-search", index, NULL);
}

static void
midori_browser_init (MidoriBrowser* browser)
{
    GtkToolItem* toolitem;
    GtkRcStyle* rcstyle;

    browser->settings = midori_web_settings_new ();
    browser->bookmarks = NULL;
    browser->trash = NULL;
    browser->search_engines = NULL;

    /* Setup the window metrics */
    g_signal_connect (browser, "realize",
                      G_CALLBACK (midori_browser_realize_cb), browser);
    g_signal_connect (browser, "window-state-event",
                      G_CALLBACK (midori_browser_window_state_event_cb), NULL);
    g_signal_connect (browser, "size-allocate",
                      G_CALLBACK (midori_browser_size_allocate_cb), NULL);
    g_signal_connect (browser, "destroy",
                      G_CALLBACK (midori_browser_destroy_cb), NULL);
    gtk_window_set_icon_name (GTK_WINDOW (browser), "web-browser");
    gtk_window_set_title (GTK_WINDOW (browser), g_get_application_name ());
    GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (browser), vbox);
    gtk_widget_show (vbox);

    /* Let us see some ui manager magic */
    browser->action_group = gtk_action_group_new ("Browser");
    gtk_action_group_set_translation_domain (browser->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (browser->action_group,
                                  entries, entries_n, browser);
    gtk_action_group_add_toggle_actions (browser->action_group,
        toggle_entries, toggle_entries_n, browser);
    GtkUIManager* ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, browser->action_group, 0);
    gtk_window_add_accel_group (GTK_WINDOW (browser),
                                gtk_ui_manager_get_accel_group (ui_manager));

    GError* error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_markup, -1, &error))
    {
        /* TODO: Should this be a message dialog? When does this happen? */
        g_message ("User interface couldn't be created: %s", error->message);
        g_error_free (error);
    }

    GtkAction* action;
    /* Make all actions except toplevel menus which lack a callback insensitive
       This will vanish once all actions are implemented */
    guint i;
    for (i = 0; i < entries_n; i++)
    {
        action = gtk_action_group_get_action (browser->action_group,
                                              entries[i].name);
        gtk_action_set_sensitive (action,
                                  entries[i].callback || !entries[i].tooltip);
    }
    for (i = 0; i < toggle_entries_n; i++)
    {
        action = gtk_action_group_get_action (browser->action_group,
                                              toggle_entries[i].name);
        gtk_action_set_sensitive (action, toggle_entries[i].callback != NULL);
    }

    /* _action_set_active(browser, "Transferbar", config->toolbarTransfers); */

    action = g_object_new (MIDORI_TYPE_LOCATION_ACTION,
        "name", "Location",
        "label", _("_Location..."),
        "stock-id", GTK_STOCK_JUMP_TO,
        "tooltip", _("Open a particular location"),
        /* FIXME: Due to a bug in GtkIconEntry we need to set an initial icon */
        "secondary-icon", STOCK_NEWS_FEED,
        NULL);
    g_object_connect (action,
                      "signal::activate",
                      _action_location_activate, browser,
                      "signal::active-changed",
                      _action_location_active_changed, browser,
                      "signal::focus-out",
                      _action_location_focus_out, browser,
                      "signal::reset-uri",
                      _action_location_reset_uri, browser,
                      "signal::submit-uri",
                      _action_location_submit_uri, browser,
                      "signal::secondary-icon-released",
                      _action_location_secondary_icon_released, browser,
                      NULL);
    gtk_action_group_add_action_with_accel (browser->action_group,
        action, "<Ctrl>L");
    g_object_unref (action);

    /* Create the menubar */
    browser->menubar = gtk_ui_manager_get_widget (ui_manager, "/menubar");
    GtkWidget* menuitem = gtk_menu_item_new ();
    gtk_widget_show (menuitem);
    browser->throbber = katze_throbber_new ();
    gtk_widget_show (browser->throbber);
    gtk_container_add (GTK_CONTAINER (menuitem), browser->throbber);
    gtk_widget_set_sensitive (menuitem, FALSE);
    gtk_menu_item_set_right_justified (GTK_MENU_ITEM (menuitem), TRUE);
    gtk_menu_shell_append (GTK_MENU_SHELL (browser->menubar), menuitem);
    gtk_box_pack_start (GTK_BOX (vbox), browser->menubar, FALSE, FALSE, 0);
    menuitem = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Trash");
    g_signal_connect (menuitem, "activate",
                      G_CALLBACK (midori_browser_menu_trash_activate_cb),
                      browser);
    browser->menu_bookmarks = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Bookmarks")));
    menuitem = gtk_separator_menu_item_new ();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (browser->menu_bookmarks), menuitem);
    browser->popup_bookmark = gtk_ui_manager_get_widget (
        ui_manager, "/popup_bookmark");
    g_object_ref (browser->popup_bookmark);
    browser->menu_tools = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Tools")));
    menuitem = gtk_separator_menu_item_new ();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (browser->menu_tools), menuitem);
    browser->menu_window = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Window")));
    menuitem = gtk_separator_menu_item_new ();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (browser->menu_window), menuitem);
    gtk_widget_show (browser->menubar);
    _action_set_sensitive (browser, "PrivateBrowsing", FALSE);

    /* Create the navigationbar */
    browser->navigationbar = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation");
    /* FIXME: settings should be connected with screen changes */
    GtkSettings* gtk_settings = gtk_widget_get_settings (GTK_WIDGET (browser));
    if (gtk_settings)
        g_signal_connect (gtk_settings, "notify::gtk-toolbar-style",
            G_CALLBACK (midori_browser_navigationbar_notify_style_cb), browser);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (browser->navigationbar), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), browser->navigationbar, FALSE, FALSE, 0);
    browser->button_tab_new = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation/TabNew");
    g_object_set (_action_by_name (browser, "Back"), "is-important", TRUE, NULL);
    browser->button_homepage = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation/Homepage");

    /* Search */
    browser->search = midori_search_entry_new ();
    /* TODO: Make this actively resizable or enlarge to fit contents?
             The interface is somewhat awkward and ought to be rethought
             Display "show in context menu" search engines as "completion actions" */
    sokoke_entry_setup_completion (GTK_ENTRY (browser->search));
    g_object_connect (browser->search,
                      "signal::activate",
                      midori_browser_search_activate_cb, browser,
                      "signal::focus-out-event",
                      midori_browser_search_focus_out_event_cb, browser,
                      "signal::notify::current-item",
                      midori_browser_search_notify_current_item_cb, browser,
                      NULL);
    toolitem = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (toolitem), browser->search);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar), toolitem, -1);
    action = gtk_action_group_get_action (browser->action_group, "Trash");
    browser->button_trash = gtk_action_create_tool_item (action);
    g_signal_connect (browser->button_trash, "clicked",
                      G_CALLBACK (midori_browser_menu_trash_activate_cb), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar),
                        GTK_TOOL_ITEM (browser->button_trash), -1);
    sokoke_container_show_children (GTK_CONTAINER (browser->navigationbar));
    gtk_widget_hide (browser->navigationbar);
    action = gtk_action_group_get_action (browser->action_group, "Fullscreen");
    browser->button_fullscreen = gtk_action_create_tool_item (action);
    gtk_widget_hide (browser->button_fullscreen);
    g_signal_connect (browser->button_fullscreen, "clicked",
                      G_CALLBACK (_action_fullscreen_activate), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar),
                        GTK_TOOL_ITEM (browser->button_fullscreen), -1);

    /* Bookmarkbar */
    browser->bookmarkbar = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (browser->bookmarkbar),
                               GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (browser->bookmarkbar),
                           GTK_TOOLBAR_BOTH_HORIZ);
    gtk_box_pack_start (GTK_BOX (vbox), browser->bookmarkbar, FALSE, FALSE, 0);

    /* Superuser warning */
    GtkWidget* hbox;
    if ((hbox = sokoke_superuser_warning_new ()))
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    /* Create the panel */
    GtkWidget* hpaned = gtk_hpaned_new ();
    g_signal_connect (hpaned, "notify::position",
                      G_CALLBACK (midori_panel_notify_position_cb),
                      browser);
    gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
    gtk_widget_show (hpaned);
    browser->panel = g_object_new (MIDORI_TYPE_PANEL,
                                "shadow-type", GTK_SHADOW_IN,
                                "menu", browser->menu_tools,
                                NULL);
    g_signal_connect (browser->panel, "close",
                      G_CALLBACK (midori_panel_close_cb), browser);
    gtk_paned_pack1 (GTK_PANED (hpaned), browser->panel, FALSE, FALSE);

    /* Bookmarks */
    GtkWidget* box = gtk_vbox_new (FALSE, 0);
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkTreeStore* treestore = gtk_tree_store_new (1, KATZE_TYPE_ITEM);
    GtkWidget* treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (treestore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
    column = gtk_tree_view_column_new ();
    renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_pixbuf,
        (GtkTreeCellDataFunc)midori_browser_bookmarks_item_render_icon_cb,
        treeview, NULL);
    renderer_text = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer_text, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer_text,
        (GtkTreeCellDataFunc)midori_browser_bookmarks_item_render_text_cb,
        treeview, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    g_object_unref (treestore);
    g_object_connect (treeview,
                      "signal::row-activated",
                      midori_panel_bookmarks_row_activated_cb, browser,
                      "signal::cursor-changed",
                      midori_panel_bookmarks_cursor_or_row_changed_cb, browser,
                      "signal::columns-changed",
                      midori_panel_bookmarks_cursor_or_row_changed_cb, browser,
                      "signal::button-release-event",
                      midori_panel_bookmarks_button_release_event_cb, browser,
                      "signal::popup-menu",
                      midori_panel_bookmarks_popup_menu_cb, browser,
                      NULL);
    midori_panel_bookmarks_cursor_or_row_changed_cb (GTK_TREE_VIEW (treeview),
                                                     browser);
    gtk_box_pack_start (GTK_BOX (box), treeview, TRUE, TRUE, 0);
    browser->panel_bookmarks = treeview;
    gtk_widget_show_all (box);
    GtkWidget* toolbar = gtk_ui_manager_get_widget (ui_manager,
                                                    "/toolbar_bookmarks");
    _action_set_sensitive (browser, "BookmarkAdd", FALSE);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);
    gtk_widget_show_all (toolbar);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              box, toolbar,
                              STOCK_BOOKMARKS, _("Bookmarks"));

    /* Transfers */
    GtkWidget* panel = midori_view_new ();
    gtk_widget_show (panel);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              panel, NULL,
                              STOCK_TRANSFERS, _("Transfers"));

    /* Console */
    browser->panel_console = midori_console_new ();
    gtk_widget_show (browser->panel_console);
    toolbar = midori_console_get_toolbar (MIDORI_CONSOLE (browser->panel_console));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              browser->panel_console, toolbar,
                              STOCK_CONSOLE, _("Console"));

    /* History */
    panel = midori_view_new ();
    gtk_widget_show (panel);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              panel, NULL,
                              STOCK_HISTORY, _("History"));

    /* Pageholder */
    browser->panel_pageholder = midori_view_new ();
    gtk_widget_show (browser->panel_pageholder);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              browser->panel_pageholder, NULL,
                              STOCK_PAGE_HOLDER, _("Pageholder"));

    /* Userscripts */
    panel = midori_addons_new (GTK_WIDGET (browser), MIDORI_ADDON_USER_SCRIPTS);
    gtk_widget_show (panel);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (panel));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              panel, toolbar,
                              STOCK_SCRIPTS, _("Userscripts"));
    /* Userstyles */
    panel = midori_addons_new (GTK_WIDGET (browser), MIDORI_ADDON_USER_STYLES);
    gtk_widget_show (panel);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (panel));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              panel, toolbar,
                              STOCK_STYLES, _("Userstyles"));

    /* Extensions */
    panel = midori_addons_new (GTK_WIDGET (browser), MIDORI_ADDON_EXTENSIONS);
    gtk_widget_show (panel);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (panel));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (browser->panel),
                              panel, toolbar,
                              STOCK_EXTENSIONS, _("Extensions"));

    /* Notebook, containing all views */
    browser->notebook = gtk_notebook_new ();
    /* Remove the inner border between scrollbars and the window border */
    rcstyle = gtk_rc_style_new ();
    rcstyle->xthickness = rcstyle->ythickness = 0;
    gtk_widget_modify_style (browser->notebook, rcstyle);
    g_object_unref (rcstyle);
    gtk_notebook_set_scrollable (GTK_NOTEBOOK (browser->notebook), TRUE);
    gtk_paned_pack2 (GTK_PANED (hpaned), browser->notebook, FALSE, FALSE);
    g_signal_connect_after (browser->notebook, "switch-page",
                            G_CALLBACK (gtk_notebook_switch_page_cb),
                            browser);
    gtk_widget_show (browser->notebook);

    /* Incremental findbar */
    browser->find = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (browser->find), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (browser->find), GTK_TOOLBAR_BOTH_HORIZ);
    toolitem = gtk_tool_item_new ();
    gtk_container_set_border_width (GTK_CONTAINER (toolitem), 6);
    gtk_container_add (GTK_CONTAINER (toolitem),
                       gtk_label_new_with_mnemonic (_("_Inline find:")));
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), toolitem, -1);
    browser->find_text = gtk_icon_entry_new ();
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (browser->find_text),
                                        GTK_ICON_ENTRY_PRIMARY,
                                        GTK_STOCK_FIND);
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (browser->find_text),
                                        GTK_ICON_ENTRY_SECONDARY,
                                        GTK_STOCK_CLEAR);
    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (browser->find_text),
                                       GTK_ICON_ENTRY_SECONDARY, TRUE);
    g_signal_connect (browser->find_text, "icon_released",
        G_CALLBACK (midori_browser_entry_clear_icon_released_cb), NULL);
    g_signal_connect (browser->find_text, "activate",
        G_CALLBACK (_action_find_next_activate), browser);
    toolitem = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (toolitem), browser->find_text);
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), toolitem, -1);
    toolitem = (GtkToolItem*)gtk_action_create_tool_item
        (_action_by_name (browser, "FindPrevious"));
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), NULL);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (_action_find_previous_activate), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), toolitem, -1);
    toolitem = (GtkToolItem*)gtk_action_create_tool_item
        (_action_by_name (browser, "FindNext"));
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), NULL);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (_action_find_next_activate), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), toolitem, -1);
    browser->find_case = gtk_toggle_tool_button_new_from_stock (
        GTK_STOCK_SPELL_CHECK);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (browser->find_case), _("Match Case"));
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (browser->find_case), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), browser->find_case, -1);
    browser->find_highlight = gtk_toggle_tool_button_new_from_stock (
        GTK_STOCK_SELECT_ALL);
    g_signal_connect (browser->find_highlight, "toggled",
                      G_CALLBACK (_find_highlight_toggled), browser);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (browser->find_highlight),
                               _("Highlight Matches"));
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (browser->find_highlight), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), browser->find_highlight, -1);
    toolitem = gtk_separator_tool_item_new ();
    gtk_separator_tool_item_set_draw (
        GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("Close Findbar"));
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (midori_browser_find_button_close_clicked_cb), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (browser->find), toolitem, -1);
    sokoke_container_show_children (GTK_CONTAINER (browser->find));
    gtk_box_pack_start (GTK_BOX (vbox), browser->find, FALSE, FALSE, 0);

    /* Statusbar */
    browser->statusbar = gtk_statusbar_new ();
    /* Adjust the statusbar's padding to avoid child overlapping */
    rcstyle = gtk_rc_style_new ();
    rcstyle->xthickness = rcstyle->ythickness = -4;
    gtk_widget_modify_style (browser->statusbar, rcstyle);
    g_object_unref (rcstyle);
    gtk_box_pack_start (GTK_BOX (vbox), browser->statusbar, FALSE, FALSE, 0);
    browser->progressbar = gtk_progress_bar_new ();
    /* Set the progressbar's height to 1 to fit it in the statusbar */
    gtk_widget_set_size_request (browser->progressbar, -1, 1);
    gtk_box_pack_start (GTK_BOX (browser->statusbar), browser->progressbar,
                        FALSE, FALSE, 3);

    g_object_unref (ui_manager);

    #if !HAVE_GIO
    _action_set_sensitive (browser, "SourceView", FALSE);
    #endif

    #ifndef WEBKIT_CHECK_VERSION
    _action_set_sensitive (browser, "ZoomIn", FALSE);
    _action_set_sensitive (browser, "ZoomOut", FALSE);
    #endif
}

static void
midori_browser_dispose (GObject* object)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);

    /* We are done, the session mustn't change anymore */
    if (browser->proxy_array)
        katze_object_assign (browser->proxy_array, NULL);

    G_OBJECT_CLASS (midori_browser_parent_class)->dispose (object);
}

static void
midori_browser_finalize (GObject* object)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);

    g_free (browser->statusbar_text);

    if (browser->settings)
        g_object_unref (browser->settings);
    if (browser->bookmarks)
        g_object_unref (browser->bookmarks);
    if (browser->trash)
        g_object_unref (browser->trash);
    if (browser->search_engines)
        g_object_unref (browser->search_engines);

    G_OBJECT_CLASS (midori_browser_parent_class)->finalize (object);
}

static void
_midori_browser_set_toolbar_style (MidoriBrowser*     browser,
                                   MidoriToolbarStyle toolbar_style)
{
    GtkToolbarStyle gtk_toolbar_style;
    GtkSettings* gtk_settings = gtk_widget_get_settings (GTK_WIDGET (browser));
    if (toolbar_style == MIDORI_TOOLBAR_DEFAULT && gtk_settings)
        g_object_get (gtk_settings, "gtk-toolbar-style", &gtk_toolbar_style, NULL);
    else
    {
        switch (toolbar_style)
        {
        case MIDORI_TOOLBAR_ICONS:
            gtk_toolbar_style = GTK_TOOLBAR_ICONS;
            break;
        case MIDORI_TOOLBAR_TEXT:
            gtk_toolbar_style = GTK_TOOLBAR_TEXT;
            break;
        case MIDORI_TOOLBAR_BOTH:
            gtk_toolbar_style = GTK_TOOLBAR_BOTH;
            break;
        case MIDORI_TOOLBAR_BOTH_HORIZ:
        case MIDORI_TOOLBAR_DEFAULT:
            gtk_toolbar_style = GTK_TOOLBAR_BOTH_HORIZ;
        }
    }
    gtk_toolbar_set_style (GTK_TOOLBAR (browser->navigationbar),
                           gtk_toolbar_style);
}

static void
_midori_browser_update_settings (MidoriBrowser* browser)
{
    KatzeItem* item;

    gboolean remember_last_window_size;
    gint last_window_width, last_window_height;
    gint last_panel_position, last_panel_page;
    gboolean show_navigationbar, show_bookmarkbar, show_panel, show_statusbar;
    gboolean show_new_tab, show_homepage, show_web_search, show_trash;
    MidoriToolbarStyle toolbar_style;
    gint last_web_search;
    gchar* last_pageholder_uri;
    gboolean close_buttons_on_tabs;

    g_object_get (browser->settings,
                  "remember-last-window-size", &remember_last_window_size,
                  "last-window-width", &last_window_width,
                  "last-window-height", &last_window_height,
                  "last-panel-position", &last_panel_position,
                  "last-panel-page", &last_panel_page,
                  "show-navigationbar", &show_navigationbar,
                  "show-bookmarkbar", &show_bookmarkbar,
                  "show-panel", &show_panel,
                  "show-statusbar", &show_statusbar,
                  "show-new-tab", &show_new_tab,
                  "show-homepage", &show_homepage,
                  "show-web-search", &show_web_search,
                  "show-trash", &show_trash,
                  "toolbar-style", &toolbar_style,
                  "last-web-search", &last_web_search,
                  "last-pageholder-uri", &last_pageholder_uri,
                  "close-buttons-on-tabs", &close_buttons_on_tabs,
                  NULL);

    GdkScreen* screen = gtk_window_get_screen (GTK_WINDOW (browser));
    const gint default_width = (gint)gdk_screen_get_width (screen) / 1.7;
    const gint default_height = (gint)gdk_screen_get_height (screen) / 1.7;

    if (remember_last_window_size)
    {
        if (last_window_width && last_window_height)
            gtk_window_set_default_size (GTK_WINDOW (browser),
                                         last_window_width, last_window_height);
        else
            gtk_window_set_default_size (GTK_WINDOW (browser),
                                         default_width, default_height);
    }

    _midori_browser_set_toolbar_style (browser, toolbar_style);

    if (browser->search_engines)
    {
        item = katze_array_get_nth_item (browser->search_engines,
                                         last_web_search);
        if (item)
            midori_search_entry_set_current_item (
                MIDORI_SEARCH_ENTRY (browser->search), item);
    }

    gtk_paned_set_position (GTK_PANED (gtk_widget_get_parent (browser->panel)),
                            last_panel_position);
    midori_panel_set_current_page (MIDORI_PANEL (browser->panel), last_panel_page);
    midori_view_set_uri (MIDORI_VIEW (browser->panel_pageholder),
                         last_pageholder_uri);

    _action_set_active (browser, "Navigationbar", show_navigationbar);
    _action_set_active (browser, "Bookmarkbar", show_bookmarkbar);
    _action_set_active (browser, "Panel", show_panel);
    _action_set_active (browser, "Statusbar", show_statusbar);

    sokoke_widget_set_visible (browser->button_tab_new, show_new_tab);
    sokoke_widget_set_visible (browser->button_homepage, show_homepage);
    sokoke_widget_set_visible (browser->search, show_web_search);
    sokoke_widget_set_visible (browser->button_trash, show_trash);

    g_free (last_pageholder_uri);
}

static void
midori_browser_settings_notify (MidoriWebSettings* web_settings,
                                GParamSpec*        pspec,
                                MidoriBrowser*     browser)
{
    const gchar* name;
    GValue value = {0, };

    name = g_intern_string (pspec->name);
    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (browser->settings), name, &value);

    if (name == g_intern_string ("toolbar-style"))
        _midori_browser_set_toolbar_style (browser, g_value_get_enum (&value));
    else if (name == g_intern_string ("show-new-tab"))
        sokoke_widget_set_visible (browser->button_tab_new,
            g_value_get_boolean (&value));
    else if (name == g_intern_string ("show-homepage"))
        sokoke_widget_set_visible (browser->button_homepage,
            g_value_get_boolean (&value));
    else if (name == g_intern_string ("show-web-search"))
        sokoke_widget_set_visible (browser->search,
            g_value_get_boolean (&value));
    else if (name == g_intern_string ("show-trash"))
        sokoke_widget_set_visible (browser->button_trash,
            g_value_get_boolean (&value));
    else if (!g_object_class_find_property (G_OBJECT_GET_CLASS (web_settings),
                                             name))
         g_warning (_("Unexpected setting '%s'"), name);
    g_value_unset (&value);
}

static void
midori_browser_load_bookmarks (MidoriBrowser* browser)
{
    guint i, n;
    KatzeItem* item;
    const gchar* title;
    const gchar* desc;
    GtkToolItem* toolitem;
    GtkTreeModel* treestore;

    // FIXME: Clear bookmarks menu
    // FIXME: Clear bookmarkbar
    // FIXME: Clear bookmark panel

    _action_set_sensitive (browser, "BookmarkAdd", FALSE);

    if (!browser->bookmarks)
        return;

    _midori_browser_create_bookmark_menu (browser, browser->bookmarks,
                                          browser->menu_bookmarks);
    n = katze_array_get_length (browser->bookmarks);
    for (i = 0; i < n; i++)
    {
        item = katze_array_get_nth_item (browser->bookmarks, i);
        title = katze_item_get_name (item);
        desc = katze_item_get_text (item);

        if (KATZE_IS_ARRAY (item))
        {
            toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DIRECTORY);
            gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), title);
            gtk_tool_item_set_is_important (toolitem, TRUE);
            g_signal_connect (toolitem, "clicked",
                G_CALLBACK (midori_browser_bookmarkbar_folder_activate_cb),
                browser);
            if (desc && *desc)
                gtk_tool_item_set_tooltip_text (toolitem, desc);
            g_object_set_data (G_OBJECT (toolitem), "KatzeArray", item);
        }
        else if (katze_item_get_uri (item))
        {
            toolitem = gtk_tool_button_new_from_stock (STOCK_BOOKMARK);
            gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), title);
            gtk_tool_item_set_is_important (toolitem, TRUE);
            g_signal_connect (toolitem, "clicked",
                G_CALLBACK (midori_browser_menu_bookmarks_item_activate_cb),
                browser);
            if (desc && *desc)
                gtk_tool_item_set_tooltip_text (toolitem, desc);
            g_object_set_data (G_OBJECT (toolitem), "KatzeItem", item);
        }
        else
            toolitem = gtk_separator_tool_item_new ();
        gtk_toolbar_insert (GTK_TOOLBAR (browser->bookmarkbar), toolitem, -1);
    }
    sokoke_container_show_children (GTK_CONTAINER (browser->bookmarkbar));

    treestore = gtk_tree_view_get_model (GTK_TREE_VIEW (browser->panel_bookmarks));
    _tree_store_insert_folder (GTK_TREE_STORE (treestore),
                               NULL, browser->bookmarks);
    midori_panel_bookmarks_cursor_or_row_changed_cb (
        GTK_TREE_VIEW (browser->panel_bookmarks), browser);

    _action_set_sensitive (browser, "BookmarkAdd", TRUE);
}

static void
midori_browser_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);
    guint last_web_search;
    KatzeItem* item;

    switch (prop_id)
    {
    case PROP_URI:
        _midori_browser_open_uri (browser, g_value_get_string (value));
        break;
    case PROP_TAB:
        midori_browser_set_current_tab (browser, g_value_get_object (value));
        break;
    case PROP_STATUSBAR_TEXT:
        _midori_browser_set_statusbar_text (browser, g_value_get_string (value));
        break;
    case PROP_SETTINGS:
        if (browser->settings)
            g_signal_handlers_disconnect_by_func (browser->settings,
                                                  midori_browser_settings_notify,
                                                  browser);
        katze_object_assign (browser->settings, g_value_get_object (value));
        g_object_ref (browser->settings);
        _midori_browser_update_settings (browser);
        g_signal_connect (browser->settings, "notify",
                      G_CALLBACK (midori_browser_settings_notify), browser);
        gtk_container_foreach (GTK_CONTAINER (browser->notebook),
            (GtkCallback) midori_view_set_settings, browser->settings);
        break;
    case PROP_BOOKMARKS:
        ; /* FIXME: Disconnect handlers */
        katze_object_assign (browser->bookmarks, g_value_get_object (value));
        g_object_ref (browser->bookmarks);
        midori_browser_load_bookmarks (browser);
        /* FIXME: Connect to updates */
        break;
    case PROP_TRASH:
        ; /* FIXME: Disconnect handlers */
        katze_object_assign (browser->trash, g_value_get_object (value));
        g_object_ref (browser->trash);
        /* FIXME: Connect to updates */
        _midori_browser_update_actions (browser);
        break;
    case PROP_SEARCH_ENGINES:
        ; /* FIXME: Disconnect handlers */
        katze_object_assign (browser->search_engines, g_value_get_object (value));
        g_object_ref (browser->search_engines);
        g_object_set (browser->search, "search-engines",
                      browser->search_engines, NULL);
        /* FIXME: Connect to updates */
        if (browser->settings)
        {
            g_object_get (browser->settings, "last-web-search",
                          &last_web_search, NULL);
            item = katze_array_get_nth_item (browser->search_engines,
                                             last_web_search);
            g_object_set (browser->search, "current-item", item, NULL);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_browser_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);

    switch (prop_id)
    {
    case PROP_MENUBAR:
        g_value_set_object (value, browser->menubar);
        break;
    case PROP_NAVIGATIONBAR:
        g_value_set_object (value, browser->navigationbar);
        break;
    case PROP_URI:
        g_value_set_string (value, midori_browser_get_current_uri (browser));
        break;
    case PROP_TAB:
        g_value_set_object (value, midori_browser_get_current_tab (browser));
        break;
    case PROP_STATUSBAR:
        g_value_set_object (value, browser->statusbar);
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, browser->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, browser->settings);
        break;
    case PROP_BOOKMARKS:
        g_value_set_object (value, browser->bookmarks);
        break;
    case PROP_TRASH:
        g_value_set_object (value, browser->trash);
        break;
    case PROP_SEARCH_ENGINES:
        g_value_set_object (value, browser->search_engines);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_browser_new:
 *
 * Creates a new browser widget.
 *
 * A browser is a window with a menubar, toolbars, a notebook, panels
 * and a statusbar. You should mostly treat it as an opaque widget.
 *
 * Return value: a new #MidoriBrowser
 **/
MidoriBrowser*
midori_browser_new (void)
{
    MidoriBrowser* browser = g_object_new (MIDORI_TYPE_BROWSER,
                                           NULL);

    return browser;
}

/**
 * midori_browser_add_tab:
 * @browser: a #MidoriBrowser
 * @widget: a view
 *
 * Appends a view in the form of a new tab and creates an
 * according item in the Window menu.
 *
 * Return value: the index of the new tab, or -1 in case of an error
 **/
gint
midori_browser_add_tab (MidoriBrowser* browser,
                        GtkWidget*     view)
{
    g_signal_emit (browser, signals[ADD_TAB], 0, view);
    return gtk_notebook_page_num (GTK_NOTEBOOK (browser->notebook), view);
}

/**
 * midori_browser_remove_tab:
 * @browser: a #MidoriBrowser
 * @widget: a view
 *
 * Removes an existing view from the browser,
 * including an associated menu item.
 **/
void
midori_browser_remove_tab (MidoriBrowser* browser,
                           GtkWidget*     view)
{
    g_signal_emit (browser, signals[REMOVE_TAB], 0, view);
}

/**
 * midori_browser_add_item:
 * @browser: a #MidoriBrowser
 * @item: an item
 *
 * Appends a new view as described by @item.
 *
 * Note: Currently this will always be a #MidoriWebView.
 *
 * Return value: the index of the new tab, or -1 in case of an error
 **/
gint
midori_browser_add_item (MidoriBrowser* browser,
                         KatzeItem*     item)
{
    const gchar* uri;
    const gchar* title;
    GtkWidget* view;

    g_return_val_if_fail (KATZE_IS_ITEM (item), -1);

    uri = katze_item_get_uri (item);
    title = katze_item_get_name (item);
    view = g_object_new (MIDORI_TYPE_VIEW,
                         "title", title,
                         "settings", browser->settings,
                         NULL);
    midori_view_set_uri (MIDORI_VIEW (view), uri);
    gtk_widget_show (view);

    return midori_browser_add_tab (browser, view);
}

/**
 * midori_browser_add_uri:
 * @browser: a #MidoriBrowser
 * @uri: an URI
 *
 * Appends an uri in the form of a new view.
 *
 * Note: Currently this will always be a #MidoriView.
 *
 * Return value: the index of the new view, or -1
 **/
gint
midori_browser_add_uri (MidoriBrowser* browser,
                        const gchar*   uri)
{
    GtkWidget* view;

    view = g_object_new (MIDORI_TYPE_VIEW,
                         "settings", browser->settings,
                         NULL);
    midori_view_set_uri (MIDORI_VIEW (view), uri);
    gtk_widget_show (view);

    return midori_browser_add_tab (browser, view);
}

/**
 * midori_browser_activate_action:
 * @browser: a #MidoriBrowser
 * @name: name of the action
 *
 * Activates the specified action.
 **/
void
midori_browser_activate_action (MidoriBrowser* browser,
                                const gchar*   name)
{
    g_signal_emit (browser, signals[ACTIVATE_ACTION], 0, name);
}

/**
 * midori_browser_get_current_uri:
 * @browser: a #MidoriBrowser
 *
 * Determines the URI loaded in the current view.
 *
 * If there is no view present at all, %NULL is returned.
 *
 * Return value: the current URI, or %NULL
 **/
const gchar*
midori_browser_get_current_uri (MidoriBrowser* browser)
{
    GtkWidget* view;

    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    view = midori_browser_get_current_tab (browser);
    return midori_view_get_display_uri (MIDORI_VIEW (view));
}

/**
 * midori_browser_set_current_page:
 * @browser: a #MidoriBrowser
 * @n: the index of a page
 *
 * Switches to the page with the index @n.
 *
 * The widget will also grab the focus automatically.
 **/
void
midori_browser_set_current_page (MidoriBrowser* browser,
                                 gint           n)
{
    GtkWidget* view;

    gtk_notebook_set_current_page (GTK_NOTEBOOK (browser->notebook), n);
    view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (browser->notebook), n);
    if (view && midori_view_is_blank (MIDORI_VIEW (view)))
        gtk_action_activate (_action_by_name (browser, "Location"));
    else
        gtk_widget_grab_focus (view);
}

/**
 * midori_browser_get_current_page:
 * @browser: a #MidoriBrowser
 *
 * Determines the currently selected page.
 *
 * If there is no page present at all, %NULL is returned.
 *
 * Return value: the selected page, or -1
 **/
gint
midori_browser_get_current_page (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), -1);

    return gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
}

/**
 * midori_browser_set_current_tab:
 * @browser: a #MidoriBrowser
 * @view: a #GtkWidget
 *
 * Switches to the page containing @view.
 *
 * The widget will also grab the focus automatically.
 **/
void
midori_browser_set_current_tab (MidoriBrowser* browser,
                                GtkWidget*     view)
{
    gint n;

    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (GTK_IS_WIDGET (view));

    n = gtk_notebook_page_num (GTK_NOTEBOOK (browser->notebook), view);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (browser->notebook), n);
    if (view && midori_view_is_blank (MIDORI_VIEW (view)))
        gtk_action_activate (_action_by_name (browser, "Location"));
    else
        gtk_widget_grab_focus (view);
}

/**
 * midori_browser_get_current_tab:
 * @browser: a #MidoriBrowser
 *
 * Retrieves the currently selected tab.
 *
 * If there is no tab present at all, %NULL is returned.
 *
 * See also midori_browser_get_current_page().
 *
 * Return value: the selected tab, or %NULL
 **/
GtkWidget*
midori_browser_get_current_tab (MidoriBrowser* browser)
{
    gint n;

    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    n = gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
    if (n >= 0)
    {
        return gtk_notebook_get_nth_page (GTK_NOTEBOOK (browser->notebook), n);
    }
    else
        return NULL;
}

/**
 * midori_browser_get_proxy_array:
 * @browser: a #MidoriBrowser
 *
 * Retrieves a proxy array representing the respective proxy items
 * of the present views that can be used for session management.
 *
 * The folder is created on the first call and will be updated to reflect
 * changes to all items automatically.
 *
 * Note that this implicitly creates proxy items of all views.
 *
 * Note: Calling this function doesn't add a reference and the browser
 *       may release its reference at some point.
 *
 * Return value: the proxy #KatzeArray
 **/
KatzeArray*
midori_browser_get_proxy_array (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    if (!browser->proxy_array)
    {
        browser->proxy_array = katze_array_new (KATZE_TYPE_ITEM);
        /* FIXME: Fill in items of all present views */
    }
    return browser->proxy_array;
}

/**
 * midori_browser_quit:
 * @browser: a #MidoriBrowser
 *
 * Quits the browser, including any other browser windows.
 *
 * This function relys on the application implementing
 * the MidoriBrowser::quit signal. If the browser was added
 * to the MidoriApp, this is handled automatically.
 **/
void
midori_browser_quit (MidoriBrowser* browser)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    g_signal_emit (browser, signals[QUIT], 0);
}
