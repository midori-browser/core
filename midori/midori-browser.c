/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "config.h"

#include "midori-browser.h"

#include "webSearch.h"

#include "main.h"
#include "sokoke.h"
#include "midori-webview.h"
#include "midori-preferences.h"
#include "midori-panel.h"
#include "midori-addons.h"
#include "midori-console.h"
#include "midori-trash.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libsexy/sexy.h>
#include <string.h>

G_DEFINE_TYPE (MidoriBrowser, midori_browser, GTK_TYPE_WINDOW)

struct _MidoriBrowserPrivate
{
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
    GtkWidget* location_icon;
    GtkWidget* location;
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

    gchar* uri;
    gchar* title;
    gchar* statusbar_text;
    MidoriWebSettings* settings;

    KatzeXbelItem* proxy_xbel_folder;
    MidoriTrash* trash;
};

#define MIDORI_BROWSER_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_BROWSER, MidoriBrowserPrivate))

enum
{
    PROP_0,

    PROP_MENUBAR,
    PROP_NAVIGATIONBAR,
    PROP_TAB,
    PROP_STATUSBAR,
    PROP_SETTINGS,
    PROP_STATUSBAR_TEXT,
    PROP_TRASH
};

enum
{
    WINDOW_OBJECT_CLEARED,
    STATUSBAR_TEXT_CHANGED,
    ELEMENT_MOTION,
    NEW_WINDOW,

    ADD_TAB,
    ADD_URI,
    ACTIVATE_ACTION,
    QUIT,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

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
    MidoriBrowserPrivate* priv = browser->priv;

    return gtk_action_group_get_action (priv->action_group, name);
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
_midori_browser_update_actions (MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    guint n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), n > 1);
    _action_set_sensitive (browser, "TabClose", n > 1);
    _action_set_sensitive (browser, "TabPrevious", n > 1);
    _action_set_sensitive (browser, "TabNext", n > 1);

    if (priv->trash)
    {
        gboolean trash_empty = midori_trash_is_empty (priv->trash);
        _action_set_sensitive (browser, "UndoTabClose", !trash_empty);
        _action_set_sensitive (browser, "Trash", !trash_empty);
    }
}

static void
_midori_browser_update_interface (MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean loading = FALSE;
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    if (web_view)
    {
        loading = midori_web_view_is_loading (MIDORI_WEB_VIEW (web_view));
        _action_set_sensitive (browser, "ZoomNormal",
            midori_web_view_get_zoom_level (MIDORI_WEB_VIEW (web_view)) != 1.0);
        if (!g_object_class_find_property (G_OBJECT_GET_CLASS (web_view),
                                           "zoom-level"))
        {
            _action_set_sensitive (browser, "ZoomIn", FALSE);
            _action_set_sensitive (browser, "ZoomOut", FALSE);
        }
        _action_set_sensitive (browser, "Back",
            webkit_web_view_can_go_back (WEBKIT_WEB_VIEW (web_view)));
        _action_set_sensitive (browser, "Forward",
            webkit_web_view_can_go_forward (WEBKIT_WEB_VIEW (web_view)));
        _action_set_sensitive (browser, "Reload", !loading);
        _action_set_sensitive (browser, "Stop", loading);
        _action_set_sensitive (browser, "Print", TRUE);
    }
    else
        _action_set_sensitive (browser, "Print", FALSE);

    GtkAction* action = gtk_action_group_get_action (priv->action_group,
                                                     "ReloadStop");
    if (!loading)
    {
        gtk_widget_set_sensitive (priv->throbber, FALSE);
        g_object_set (action,
                      "stock-id", GTK_STOCK_REFRESH,
                      "tooltip", _("Reload the current page"), NULL);
        gtk_widget_hide (priv->progressbar);
    }
    else
    {
        gtk_widget_set_sensitive (priv->throbber, TRUE);
        g_object_set (action,
                      "stock-id", GTK_STOCK_STOP,
                      "tooltip", _("Stop loading the current page"), NULL);
        gtk_widget_show (priv->progressbar);
    }
    katze_throbber_set_animated (KATZE_THROBBER (priv->throbber), loading);
    gtk_image_set_from_stock (GTK_IMAGE (priv->location_icon),
                              GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
}

static GtkWidget*
_midori_browser_scrolled_for_child (MidoriBrowser* browser,
                                    GtkWidget*     child)
{
    GtkWidget* scrolled = gtk_widget_get_parent (child);
    if (GTK_IS_VIEWPORT (scrolled))
        scrolled = gtk_widget_get_parent (scrolled);
    return scrolled;
}

static GtkWidget*
_midori_browser_child_for_scrolled (MidoriBrowser* browser,
                                    GtkWidget*     scrolled)
{
    GtkWidget* child = gtk_bin_get_child (GTK_BIN (scrolled));
    if (GTK_IS_VIEWPORT (child))
        child = gtk_bin_get_child (GTK_BIN (child));
    return child;
}

static void
_midori_browser_set_statusbar_text (MidoriBrowser* browser,
                                    const gchar*   text)
{
    MidoriBrowserPrivate* priv = browser->priv;

    katze_assign (priv->statusbar_text, g_strdup (text));
    gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 1);
    gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), 1,
                        priv->statusbar_text ? priv->statusbar_text : "");
}

static void
_midori_browser_set_current_page_smartly (MidoriBrowser* browser,
                                          gint           n)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean open_tabs_in_the_background;
    g_object_get (priv->settings, "open-tabs-in-the-background",
                  &open_tabs_in_the_background, NULL);
    if (!open_tabs_in_the_background)
        midori_browser_set_current_page (browser, n);
}

static void
_midori_browser_update_progress (MidoriBrowser* browser,
                                 gint           progress)
{
    MidoriBrowserPrivate* priv = browser->priv;

    if (progress > -1)
    {
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progressbar),
                                       progress ? progress / 100.0 : 0);
        gchar* message = g_strdup_printf (_("%d%% loaded"), progress);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progressbar),
                                   message);
        g_free (message);
    }
    else
    {
            gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progressbar));
            gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progressbar),
                                       NULL);
    }
}

static void
midori_web_view_window_object_cleared_cb (GtkWidget*         web_view,
                                          WebKitWebFrame*    web_frame,
                                          JSGlobalContextRef js_context,
                                          JSObjectRef        js_window,
                                          MidoriBrowser*     browser)
{
    g_signal_emit (browser, signals[WINDOW_OBJECT_CLEARED], 0,
                   web_frame, js_context, js_window);
}

static void
midori_web_view_load_started_cb (GtkWidget*      web_view,
                                 WebKitWebFrame* web_frame,
                                 MidoriBrowser*  browser)
{
    if (web_view == midori_browser_get_current_web_view (browser))
    {
        _midori_browser_update_interface (browser);
        _midori_browser_set_statusbar_text (browser, NULL);
    }
}

static void
midori_web_view_progress_started_cb (GtkWidget*     web_view,
                                     guint          progress,
                                     MidoriBrowser* browser)
{
    if (web_view == midori_browser_get_current_web_view (browser))
        _midori_browser_update_progress (browser, progress);
}

static void
midori_web_view_progress_changed_cb (GtkWidget*     web_view,
                                     guint          progress,
                                     MidoriBrowser* browser)
{
    if (web_view == midori_browser_get_current_web_view (browser))
        _midori_browser_update_progress (browser, progress);
}

static void
midori_web_view_progress_done_cb (GtkWidget*     web_view,
                                  guint          progress,
                                  MidoriBrowser* browser)
{
    if (web_view == midori_browser_get_current_web_view (browser))
        _midori_browser_update_progress (browser, progress);
}

static void
midori_web_view_load_done_cb (GtkWidget*      web_view,
                              WebKitWebFrame* web_frame,
                              MidoriBrowser*  browser)
{
    if (web_view == midori_browser_get_current_web_view (browser))
    {
        _midori_browser_update_interface (browser);
        _midori_browser_set_statusbar_text (browser, NULL);
    }
}

static void
midori_web_view_title_changed_cb (GtkWidget*      web_view,
                                  WebKitWebFrame* web_frame,
                                  const gchar*    title,
                                  MidoriBrowser*  browser)
{
    if (web_view == midori_browser_get_current_web_view (browser))
    {
        const gchar* title = midori_web_view_get_display_title (
            MIDORI_WEB_VIEW (web_view));
        gchar* window_title = g_strconcat (title, " - ",
            g_get_application_name (), NULL);
        gtk_window_set_title (GTK_WINDOW (browser), window_title);
        g_free (window_title);
    }
}

static void
midori_web_view_statusbar_text_changed_cb (MidoriWebView*  web_view,
                                           const gchar*    text,
                                           MidoriBrowser*  browser)
{
    _midori_browser_set_statusbar_text (browser, text);
}

static void
midori_web_view_element_motion_cb (MidoriWebView* web_View,
                                   const gchar*   link_uri,
                                   MidoriBrowser* browser)
{
    _midori_browser_set_statusbar_text (browser, link_uri);
}

static void
midori_web_view_load_committed_cb (GtkWidget*      web_view,
                                   WebKitWebFrame* web_frame,
                                   MidoriBrowser*  browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    if (web_view == midori_browser_get_current_web_view (browser))
    {
        const gchar* uri = midori_web_view_get_display_uri (MIDORI_WEB_VIEW (web_view));
        gtk_entry_set_text (GTK_ENTRY (priv->location), uri);
        _midori_browser_set_statusbar_text (browser, NULL);
    }
}

static gboolean
midori_web_view_console_message_cb (GtkWidget*     web_view,
                                    const gchar*   message,
                                    guint          line,
                                    const gchar*   source_id,
                                    MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    midori_console_add (MIDORI_CONSOLE (priv->panel_console),
                        message, line, source_id);
    return TRUE;
}

static void
midori_web_view_populate_popup_cb (GtkWidget*     web_view,
                                   GtkWidget*     menu,
                                   MidoriBrowser* browser)
{
    const gchar* uri = midori_web_view_get_link_uri (MIDORI_WEB_VIEW (web_view));
    if (uri)
    {
        // TODO: bookmark link
    }

    if (webkit_web_view_has_selection (WEBKIT_WEB_VIEW (web_view)))
    {
        // TODO: view selection source
    }

    if (!uri && !webkit_web_view_has_selection (WEBKIT_WEB_VIEW (web_view)))
    {
        GtkAction* action = _action_by_name (browser, "UndoTabClose");
        GtkWidget* menuitem = gtk_action_create_menu_item (action);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        menuitem = gtk_separator_menu_item_new ();
        gtk_widget_show (menuitem);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        action = _action_by_name (browser, "BookmarkAdd");
        menuitem = gtk_action_create_menu_item (action);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        action = _action_by_name (browser, "SaveAs");
        menuitem = gtk_action_create_menu_item (action);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        action = _action_by_name (browser, "SourceView");
        menuitem = gtk_action_create_menu_item (action);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        action = _action_by_name (browser, "Print");
        menuitem = gtk_action_create_menu_item (action);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    }
}

static gboolean
midori_web_view_leave_notify_event_cb (GtkWidget*        web_view,
                                       GdkEventCrossing* event,
                                       MidoriBrowser*    browser)
{
    _midori_browser_set_statusbar_text (browser, NULL);
    return TRUE;
}

static void
midori_web_view_new_tab_cb (GtkWidget*     web_view,
                            const gchar*   uri,
                            MidoriBrowser* browser)
{
    gint n = midori_browser_add_uri (browser, uri);
    _midori_browser_set_current_page_smartly (browser, n);
}

static void
midori_web_view_new_window_cb (GtkWidget*     web_view,
                               const gchar*   uri,
                               MidoriBrowser* browser)
{
    g_signal_emit (browser, signals[NEW_WINDOW], 0, uri);
}

static void
midori_web_view_close_cb (GtkWidget*     web_view,
                          MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    if (priv->proxy_xbel_folder)
    {
        KatzeXbelItem* xbel_item = midori_web_view_get_proxy_xbel_item (
            MIDORI_WEB_VIEW (web_view));
        const gchar* uri = katze_xbel_bookmark_get_href (xbel_item);
        if (priv->trash && uri && *uri)
            midori_trash_prepend_xbel_item (priv->trash, xbel_item);
        katze_xbel_folder_remove_item (priv->proxy_xbel_folder, xbel_item);
        katze_xbel_item_unref (xbel_item);
    }
    GtkWidget* scrolled = _midori_browser_scrolled_for_child (browser, web_view);
    guint n = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), scrolled);
    gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), n);

    _midori_browser_update_actions (browser);
}

static gboolean
midori_web_view_destroy_cb (GtkWidget*     widget,
                            MidoriBrowser* browser)
{
    _midori_browser_update_actions (browser);
    return FALSE;
}

static void
_midori_browser_will_quit (MidoriBrowser* browser)
{
    // Nothing to do
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
midori_cclosure_marshal_INT__OBJECT (GClosure*     closure,
                                     GValue*       return_value,
                                     guint         n_param_values,
                                     const GValue* param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data)
{
    typedef gint(*GMarshalFunc_INT__OBJECT) (gpointer  data1,
                                             gpointer  arg_1,
                                             gpointer  data2);
    register GMarshalFunc_INT__OBJECT callback;
    register GCClosure* cc = (GCClosure*) closure;
    register gpointer data1, data2;
    gint v_return;

    g_return_if_fail (return_value != NULL);
    g_return_if_fail (n_param_values == 2);

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
    callback = (GMarshalFunc_INT__OBJECT) (marshal_data
        ? marshal_data : cc->callback);
    v_return = callback (data1,
                         g_value_get_object (param_values + 1),
                         data2);
    g_value_set_int (return_value, v_return);
}

static void
midori_cclosure_marshal_INT__STRING (GClosure*     closure,
                                     GValue*       return_value,
                                     guint         n_param_values,
                                     const GValue* param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data)
{
    typedef gint(*GMarshalFunc_INT__STRING) (gpointer      data1,
                                             const gchar*  arg_1,
                                             gpointer      data2);
    register GMarshalFunc_INT__STRING callback;
    register GCClosure* cc = (GCClosure*) closure;
    register gpointer data1, data2;
    gint v_return;

    g_return_if_fail (return_value != NULL);
    g_return_if_fail (n_param_values == 2);

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
    callback = (GMarshalFunc_INT__STRING) (marshal_data
        ? marshal_data : cc->callback);
    v_return = callback (data1,
                         g_value_get_string (param_values + 1),
                         data2);
    g_value_set_int (return_value, v_return);
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

    signals[STATUSBAR_TEXT_CHANGED] = g_signal_new (
        "statusbar-text-changed",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriBrowserClass, statusbar_text_changed),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[ELEMENT_MOTION] = g_signal_new (
        "element-motion",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriBrowserClass, element_motion),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

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
        midori_cclosure_marshal_INT__OBJECT,
        G_TYPE_INT, 1,
        GTK_TYPE_WIDGET);

    signals[ADD_URI] = g_signal_new (
        "add-uri",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, add_uri),
        0,
        NULL,
        midori_cclosure_marshal_INT__STRING,
        G_TYPE_INT, 1,
        G_TYPE_STRING);

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

    class->add_tab = midori_browser_add_tab;
    class->add_uri = midori_browser_add_uri;
    class->activate_action = midori_browser_activate_action;
    class->quit = _midori_browser_will_quit;

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
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
    * MidoriBrowser:settings:
    *
    * An associated settings instance that is shared among all web views.
    *
    * Setting this value is propagated to every present web view. Also
    * every newly created web view will use this instance automatically.
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
                                     MIDORI_TYPE_TRASH,
                                     G_PARAM_READWRITE));

    g_type_class_add_private (class, sizeof (MidoriBrowserPrivate));
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
    MidoriBrowserPrivate* priv = browser->priv;

    gint n = midori_browser_add_uri (browser, "");
    midori_browser_set_current_page (browser, n);
    gtk_widget_grab_focus (priv->location);
}

static void
_action_open_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    static gchar* last_dir = NULL;
    gchar* uri = NULL;
    gboolean folder_set = FALSE;
    GtkWidget* dialog = gtk_file_chooser_dialog_new (
        ("Open file"), GTK_WINDOW (browser),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
        NULL);
     gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_OPEN);
     gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (browser));

     // base the start folder on the current web view's uri if it is local
     GtkWidget* web_view = midori_browser_get_current_web_view (browser);
     if (web_view)
         g_object_get (web_view, "uri", &uri, NULL);
     if (uri)
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
         g_free (uri);
     }

     if (!folder_set && last_dir && *last_dir)
         gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), last_dir);

     if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
     {
         uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
         gchar* folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
         g_object_set (web_view, "uri", uri, NULL);

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
    GtkWidget* scrolled = _midori_browser_scrolled_for_child (browser, widget);
    gtk_widget_destroy (scrolled);
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
    GtkWidget* web_view = midori_browser_get_current_tab (browser);
    if (web_view)
        webkit_web_view_execute_script (WEBKIT_WEB_VIEW (web_view), "print ();");
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
    gboolean has_selection;

    if (WEBKIT_IS_WEB_VIEW (widget))
    {
        WebKitWebView* web_view = WEBKIT_WEB_VIEW (widget);
        can_cut = webkit_web_view_can_cut_clipboard (web_view);
        can_copy = webkit_web_view_can_copy_clipboard (web_view);
        can_paste = webkit_web_view_can_paste_clipboard (web_view);
    }
    else if (GTK_IS_EDITABLE (widget))
    {
        GtkEditable* editable = GTK_EDITABLE (widget);
        has_selection = gtk_editable_get_selection_bounds (editable, NULL, NULL);
        can_cut = has_selection && gtk_editable_get_editable (editable);
        can_copy = has_selection;
        can_paste = gtk_editable_get_editable (editable);
    }

    _action_set_sensitive (browser, "Cut", can_cut);
    _action_set_sensitive (browser, "Copy", can_copy);
    _action_set_sensitive (browser, "Paste", can_paste);
    _action_set_sensitive (browser, "Delete", can_cut);
    _action_set_sensitive (browser, "SelectAll", FALSE);
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
        else if (GTK_IS_EDITABLE(widget))
            gtk_editable_delete_selection (GTK_EDITABLE (widget));
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
            g_signal_emit_by_name (widget, "select-all");
    }
}

static void
_action_find_activate(GtkAction*     action,
                      MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    if (GTK_WIDGET_VISIBLE (priv->find))
    {
        GtkWidget* web_view = midori_browser_get_current_web_view (browser);
        webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (web_view));
        gtk_toggle_tool_button_set_active (
            GTK_TOGGLE_TOOL_BUTTON (priv->find_highlight), FALSE);
        gtk_widget_hide (priv->find);
    }
    else
    {
        GtkWidget* icon = gtk_image_new_from_stock (GTK_STOCK_FIND,
                                                    GTK_ICON_SIZE_MENU);
        sexy_icon_entry_set_icon (SEXY_ICON_ENTRY (priv->find_text),
                                  SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE (icon));
        gtk_entry_set_text (GTK_ENTRY (priv->find_text), "");
        gtk_widget_show (priv->find);
        gtk_widget_grab_focus (GTK_WIDGET (priv->find_text));
    }
}

static void
_midori_browser_find (MidoriBrowser* browser,
                      gboolean       forward)
{
    MidoriBrowserPrivate* priv = browser->priv;

    const gchar* text = gtk_entry_get_text (GTK_ENTRY (priv->find_text));
    const gboolean case_sensitive = gtk_toggle_tool_button_get_active (
        GTK_TOGGLE_TOOL_BUTTON(priv->find_case));
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    if (GTK_WIDGET_VISIBLE (priv->find))
        webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (web_view));
    gboolean found = webkit_web_view_search_text (WEBKIT_WEB_VIEW (web_view),
        text, case_sensitive, forward, TRUE);
    if (GTK_WIDGET_VISIBLE (priv->find))
    {
        GtkWidget* icon;
        if (found)
            icon = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
        else
            icon = gtk_image_new_from_stock (GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
        sexy_icon_entry_set_icon (SEXY_ICON_ENTRY (priv->find_text),
            SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE (icon));
        webkit_web_view_mark_text_matches (WEBKIT_WEB_VIEW (web_view), text,
                                           case_sensitive, 0);
        const gboolean highlight = gtk_toggle_tool_button_get_active (
            GTK_TOGGLE_TOOL_BUTTON (priv->find_highlight));
        webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (web_view),
                                                    highlight);
    }
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
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    const gboolean highlight = gtk_toggle_tool_button_get_active (toolitem);
    webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (web_view),
                                                highlight);
}

static void
midori_browser_find_button_close_clicked_cb (GtkWidget*     widget,
                                             MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gtk_widget_hide (priv->find);
}

static void
midori_browser_navigationbar_notify_style_cb (GObject*       object,
                                              GParamSpec*    arg1,
                                              MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    MidoriToolbarStyle toolbar_style;
    GtkToolbarStyle gtk_toolbar_style;

    g_object_get (priv->settings, "toolbar-style", &toolbar_style, NULL);
    if (toolbar_style == MIDORI_TOOLBAR_DEFAULT)
    {
        g_object_get (priv->settings,
                      "gtk-toolbar-style", &gtk_toolbar_style, NULL);
        gtk_toolbar_set_style (GTK_TOOLBAR (priv->navigationbar),
                               gtk_toolbar_style);
    }
}

static void
midori_browser_menu_trash_item_activate_cb (GtkWidget*     menuitem,
                                            MidoriBrowser* browser)
{
    // Create a new web view with an uri which has been closed before
    KatzeXbelItem* item = g_object_get_data (G_OBJECT (menuitem),
                                             "KatzeXbelItem");
    const gchar* uri = katze_xbel_bookmark_get_href (item);
    gint n = midori_browser_add_uri (browser, uri);
    midori_browser_set_current_page (browser, n);
    katze_xbel_item_unref (item);
}

static void
midori_browser_menu_trash_activate_cb (GtkWidget*     widget,
                                       MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* menu = gtk_menu_new ();
    guint n = midori_trash_get_n_items (priv->trash);
    GtkWidget* menuitem;
    guint i;
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = midori_trash_get_nth_xbel_item (priv->trash, i);
        const gchar* title = katze_xbel_item_get_title (item);
        const gchar* uri = katze_xbel_bookmark_get_href (item);
        menuitem = gtk_image_menu_item_new_with_label (title ? title : uri);
        // FIXME: Get the real icon
        GtkWidget* icon = gtk_image_new_from_stock (GTK_STOCK_FILE,
                                                    GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        gtk_menu_shell_append(GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "KatzeXbelItem", item);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_browser_menu_trash_item_activate_cb), browser);
        gtk_widget_show (menuitem);
    }

    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    GtkAction* action = gtk_action_group_get_action (priv->action_group,
                                                     "TrashEmpty");
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
_action_preferences_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    // Show the preferences dialog. Create it if necessary.
    static GtkWidget* dialog = NULL;
    if (GTK_IS_DIALOG (dialog))
        gtk_window_present (GTK_WINDOW (dialog));
    else
    {
        MidoriBrowserPrivate* priv = browser->priv;

        dialog = midori_preferences_new (GTK_WINDOW (browser),
                                         priv->settings);
        gtk_widget_show (dialog);
    }
}

static void
_action_navigationbar_activate (GtkToggleAction* action,
                                MidoriBrowser*   browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (priv->settings, "show-navigationbar", active, NULL);
    sokoke_widget_set_visible (priv->navigationbar, active);
}

static void
_action_bookmarkbar_activate (GtkToggleAction* action,
                              MidoriBrowser*   browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (priv->settings, "show-bookmarkbar", active, NULL);
    sokoke_widget_set_visible (priv->bookmarkbar, active);
}

static void
_action_statusbar_activate (GtkToggleAction* action,
                            MidoriBrowser*   browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (priv->settings, "show-statusbar", active, NULL);
    sokoke_widget_set_visible (priv->statusbar, active);
}

static void
_action_reload_stop_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    gchar* stock_id;
    g_object_get (action, "stock-id", &stock_id, NULL);
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    // Refresh or stop, depending on the stock id
    if (!strcmp (stock_id, GTK_STOCK_REFRESH))
    {
        /*GdkModifierType state = (GdkModifierType)0;
        gint x, y;
        gdk_window_get_pointer (NULL, &x, &y, &state);
        gboolean from_cache = state & GDK_SHIFT_MASK;*/
        webkit_web_view_reload (WEBKIT_WEB_VIEW (web_view));
    }
    else
        webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (web_view));
    g_free (stock_id);
}

static void
_action_zoom_in_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    if (web_view && g_object_class_find_property (
        G_OBJECT_GET_CLASS (web_view), "zoom-level"))
    {
        MidoriBrowserPrivate* priv = browser->priv;

        gfloat zoom_level, zoom_step;
        g_object_get (web_view, "zoom-level", &zoom_level, NULL);
        g_object_get (priv->settings, "zoom-step", &zoom_step, NULL);
        g_object_set (web_view, "zoom-level", zoom_level + zoom_step, NULL);
    }
}

static void
_action_zoom_out_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    if (web_view && g_object_class_find_property (
        G_OBJECT_GET_CLASS (web_view), "zoom-level"))
    {
        MidoriBrowserPrivate* priv = browser->priv;

        gfloat zoom_level, zoom_step;
        g_object_get (web_view, "zoom-level", &zoom_level, NULL);
        g_object_get (priv->settings, "zoom-step", &zoom_step, NULL);
        g_object_set (web_view, "zoom-level", zoom_level - zoom_step, NULL);
    }
}

static void
_action_zoom_normal_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    if (web_view && g_object_class_find_property (
        G_OBJECT_GET_CLASS (web_view), "zoom-level"))
        g_object_set (web_view, "zoom-level", 1.0, NULL);
}

/*static void
_action_source_view_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    gchar* source = webkit_web_view_copy_source (WEBKIT_WEB_VIEW (web_view));
    webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (web_view), source, "");
    g_free (source);
}*/

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
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    webkit_web_view_go_back (WEBKIT_WEB_VIEW (web_view));
}

static void
_action_forward_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    webkit_web_view_go_forward (WEBKIT_WEB_VIEW (web_view));
}

static void
_action_homepage_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gchar* homepage;

    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    g_object_get (priv->settings, "homepage", &homepage, NULL);
    g_object_set (web_view, "uri", homepage, NULL);
    g_free (homepage);
}

static gboolean
midori_browser_location_key_press_event_cb (GtkWidget*     widget,
                                            GdkEventKey*   event,
                                            MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;
    gchar* location_entry_search;

    switch (event->keyval)
    {
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_Return:
    {
        const gchar* uri = gtk_entry_get_text (GTK_ENTRY (widget));
        if (uri)
        {
            g_object_get (priv->settings, "location-entry-search",
                          &location_entry_search, NULL);
            gchar* new_uri = sokoke_magic_uri (uri, location_entry_search);
            g_free (location_entry_search);
            // TODO: Use new_uri intermediately when completion is better
            /* TODO Completion should be generated from history, that is
                    the uri as well as the title. */
            sokoke_entry_append_completion (GTK_ENTRY (widget), uri);
            GtkWidget* web_view = midori_browser_get_current_web_view (browser);
            g_object_set (web_view, "uri", new_uri, NULL);
            g_free (new_uri);
            gtk_widget_grab_focus (web_view);
        }
        return TRUE;
    }
    case GDK_Escape:
    {
        GtkWidget* web_view = midori_browser_get_current_web_view (browser);
        const gchar* uri = midori_web_view_get_display_uri (
            MIDORI_WEB_VIEW (web_view));
        gtk_entry_set_text (GTK_ENTRY (widget), uri);
        return TRUE;
    }
    }
    return FALSE;
}

static void
_action_location_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    if (!GTK_WIDGET_VISIBLE (priv->navigationbar))
        gtk_widget_show (priv->navigationbar);
    gtk_widget_grab_focus (priv->location);
}

static gboolean
midori_browser_location_focus_out_event_cb (GtkWidget*     widget,
                                            GdkEventFocus* event,
                                            MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean show_navigationbar;
    g_object_get (priv->settings,
                  "show-navigationbar", &show_navigationbar,
                  NULL);
    if (!show_navigationbar)
        gtk_widget_hide (priv->navigationbar);
    return FALSE;
}

static void
_action_search_activate (GtkAction*     action,
                         MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    if (!GTK_WIDGET_VISIBLE (priv->search))
        gtk_widget_show (priv->search);
    if (!GTK_WIDGET_VISIBLE (priv->navigationbar))
        gtk_widget_show (priv->navigationbar);
    gtk_widget_grab_focus (priv->search);
}

static gboolean
midori_browser_search_focus_out_event_cb (GtkWidget*     widget,
                                          GdkEventFocus* event,
                                          MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean show_navigationbar;
    gboolean show_web_search;
    g_object_get (priv->settings,
                  "show-navigationbar", &show_navigationbar,
                  "show-web-search", &show_web_search,
                  NULL);
    if (!show_navigationbar)
        gtk_widget_hide (priv->navigationbar);
    if (!show_web_search)
        gtk_widget_hide (priv->search);
    return FALSE;
}

static void
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeXbelItem* bookmark)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean new_bookmark = !bookmark;
    GtkWidget* dialog = gtk_dialog_new_with_buttons (
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
    GtkSizeGroup* sizegroup =  gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    if (new_bookmark)
    {
        GtkWidget* web_view = midori_browser_get_current_web_view (browser);
        bookmark = katze_xbel_bookmark_new ();
        if (MIDORI_IS_WEB_VIEW (web_view))
        {
            katze_xbel_item_set_title (bookmark,
                midori_web_view_get_display_title (MIDORI_WEB_VIEW (web_view)));
            katze_xbel_bookmark_set_href (bookmark,
                midori_web_view_get_display_uri (MIDORI_WEB_VIEW (web_view)));
        }
    }

    GtkWidget* hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    GtkWidget* label = gtk_label_new_with_mnemonic (_("_Title:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_title = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_title), TRUE);
    const gchar* title = katze_xbel_item_get_title (bookmark);
    gtk_entry_set_text (GTK_ENTRY (entry_title), title ? title : "");
    gtk_box_pack_start (GTK_BOX (hbox), entry_title, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Description:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    GtkWidget* entry_desc = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_desc), TRUE);
    if (!new_bookmark)
    {
        const gchar* desc = katze_xbel_item_get_desc (bookmark);
        gtk_entry_set_text (GTK_ENTRY (entry_desc), desc ? desc : "");
    }
    gtk_box_pack_start (GTK_BOX (hbox), entry_desc, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
    gtk_widget_show_all (hbox);

    GtkWidget* entry_uri = NULL;
    if (katze_xbel_item_is_bookmark (bookmark))
    {
        hbox = gtk_hbox_new (FALSE, 8);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
        label = gtk_label_new_with_mnemonic (_("_URL:"));
        gtk_size_group_add_widget (sizegroup, label);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        entry_uri = gtk_entry_new ();
        gtk_entry_set_activates_default (GTK_ENTRY (entry_uri), TRUE);
        gtk_entry_set_text (GTK_ENTRY (entry_uri),
                            katze_xbel_bookmark_get_href (bookmark));
        gtk_box_pack_start (GTK_BOX(hbox), entry_uri, TRUE, TRUE, 0);
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
        gtk_widget_show_all (hbox);
    }

    GtkWidget* combo_folder = NULL;
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
        katze_xbel_item_set_title (bookmark,
            gtk_entry_get_text (GTK_ENTRY (entry_title)));
        katze_xbel_item_set_desc (bookmark,
            gtk_entry_get_text(GTK_ENTRY(entry_desc)));
        if (katze_xbel_item_is_bookmark (bookmark))
            katze_xbel_bookmark_set_href (bookmark,
                gtk_entry_get_text (GTK_ENTRY (entry_uri)));

        // FIXME: We want to choose a folder
        if (new_bookmark)
        {
            katze_xbel_folder_append_item (bookmarks, bookmark);
            GtkTreeView* treeview = GTK_TREE_VIEW (priv->panel_bookmarks);
            GtkTreeModel* treemodel = gtk_tree_view_get_model (treeview);
            GtkTreeIter iter;
            gtk_tree_store_insert_with_values (GTK_TREE_STORE (treemodel),
                &iter, NULL, G_MAXINT, 0, bookmark, -1);
            katze_xbel_item_ref (bookmark);
        }

        // FIXME: update navigationbar
        // FIXME: Update panel in other windows
    }
    gtk_widget_destroy (dialog);
}

static void
midori_panel_bookmarks_row_activated_cb (GtkTreeView*       treeview,
                                         GtkTreePath*       path,
                                         GtkTreeViewColumn* column,
                                         MidoriBrowser*     browser)
{
    GtkTreeModel* model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        KatzeXbelItem* item;
        gtk_tree_model_get (model, &iter, 0, &item, -1);
        if (katze_xbel_item_is_bookmark (item))
        {
            GtkWidget* web_view = midori_browser_get_current_web_view (browser);
            const gchar* uri = katze_xbel_bookmark_get_href (item);
            g_object_set (web_view, "uri", uri, NULL);
        }
    }
}

static void
midori_panel_bookmarks_cursor_or_row_changed_cb (GtkTreeView*   treeview,
                                                 MidoriBrowser* browser)
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);

            gboolean is_separator = katze_xbel_item_is_separator (item);
            _action_set_sensitive (browser, "BookmarkEdit", !is_separator);
            _action_set_sensitive (browser, "BookmarkDelete", TRUE);
        }
        else
        {
            _action_set_sensitive (browser, "BookmarkEdit", FALSE);
            _action_set_sensitive (browser, "BookmarkDelete", FALSE);
        }
    }
}

static void
_midori_panel_bookmarks_popup (GtkWidget*      widget,
                               GdkEventButton* event,
                               KatzeXbelItem*  item,
                               MidoriBrowser*  browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean is_bookmark = katze_xbel_item_is_bookmark (item);

    _action_set_sensitive (browser, "BookmarkOpen", is_bookmark);
    _action_set_sensitive (browser, "BookmarkOpenTab", is_bookmark);
    _action_set_sensitive (browser, "BookmarkOpenWindow", is_bookmark);

    sokoke_widget_popup (widget, GTK_MENU (priv->popup_bookmark),
		         event, SOKOKE_MENU_POSITION_CURSOR);
}

static gboolean
midori_panel_bookmarks_button_release_event_cb (GtkWidget*      widget,
                                                GdkEventButton* event,
                                                MidoriBrowser*  browser)
{
    if (event->button != 2 && event->button != 3)
        return FALSE;

    GtkTreeSelection* selection = gtk_tree_view_get_selection (
        GTK_TREE_VIEW (widget));
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            if (event->button == 2 && katze_xbel_item_is_bookmark (item))
            {
                const gchar* uri = katze_xbel_bookmark_get_href (item);
                gint n = midori_browser_add_uri (browser, uri);
                midori_browser_set_current_page (browser, n);
            }
            else
                _midori_panel_bookmarks_popup (widget, event, item, browser);
            return TRUE;
        }
    }
    return FALSE;
}

static void
midori_panel_bookmarks_popup_menu_cb (GtkWidget*     widget,
                                      MidoriBrowser* browser)
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection (
        GTK_TREE_VIEW (widget));
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            _midori_panel_bookmarks_popup (widget, NULL, item, browser);
        }
    }
}

static void
_tree_store_insert_folder (GtkTreeStore*  treestore,
                           GtkTreeIter*   parent,
                           KatzeXbelItem* folder)
{
    guint n = katze_xbel_folder_get_n_items (folder);
    guint i;
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (folder, i);
        GtkTreeIter iter;
        gtk_tree_store_insert_with_values (treestore, &iter, parent, n,
                                           0, item, -1);
        katze_xbel_item_ref (item);
        if (katze_xbel_item_is_folder (item))
            _tree_store_insert_folder (treestore, &iter, item);
    }
}

static void
midori_browser_bookmarks_item_render_icon_cb (GtkTreeViewColumn* column,
                                              GtkCellRenderer*   renderer,
                                              GtkTreeModel*      model,
                                              GtkTreeIter*       iter,
                                              GtkWidget*         treeview)
{
    KatzeXbelItem* item;
    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (G_UNLIKELY (!item))
        return;
    if (G_UNLIKELY (!katze_xbel_item_get_parent (item)))
    {
        gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
        katze_xbel_item_unref (item);
        return;
    }

    // TODO: Would it be better to not do this on every redraw?
    GdkPixbuf* pixbuf = NULL;
    if (katze_xbel_item_is_bookmark (item))
        pixbuf = gtk_widget_render_icon (treeview, STOCK_BOOKMARK,
                                         GTK_ICON_SIZE_MENU, NULL);
    else if (katze_xbel_item_is_folder (item))
        pixbuf = gtk_widget_render_icon (treeview, GTK_STOCK_DIRECTORY,
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
    KatzeXbelItem* item;
    gtk_tree_model_get (model, iter, 0, &item, -1);

    if (G_UNLIKELY (!item))
        return;
    if (G_UNLIKELY (!katze_xbel_item_get_parent (item)))
    {
        gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
        katze_xbel_item_unref (item);
        return;
    }

    if (katze_xbel_item_is_separator (item))
        g_object_set (renderer, "markup", _("<i>Separator</i>"), NULL);
    else
        g_object_set (renderer, "markup", NULL,
                      "text", katze_xbel_item_get_title (item), NULL);
}

static void
_midori_browser_create_bookmark_menu (MidoriBrowser* browser,
                                      KatzeXbelItem* folder,
                                      GtkWidget*     menu);

static void
midori_browser_bookmark_menu_folder_activate_cb (GtkWidget*     menuitem,
                                                 MidoriBrowser* browser)
{
    GtkWidget* menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem));
    gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback) gtk_widget_destroy, NULL);//...
    KatzeXbelItem* folder = (KatzeXbelItem*)g_object_get_data(G_OBJECT (menuitem), "KatzeXbelItem");
    _midori_browser_create_bookmark_menu (browser, folder, menu);
    // Remove all menuitems when the menu is hidden.
    // FIXME: We really *want* the line below, but it won't work like that
    //g_signal_connect_after (menu, "hide", G_CALLBACK (gtk_container_foreach), gtk_widget_destroy);
    gtk_widget_show (menuitem);
}

static void
midori_browser_bookmarkbar_folder_activate_cb (GtkToolItem*   toolitem,
                                               MidoriBrowser* browser)
{
    GtkWidget* menu = gtk_menu_new ();
    KatzeXbelItem* folder = (KatzeXbelItem*)g_object_get_data (
        G_OBJECT (toolitem), "KatzeXbelItem");
    _midori_browser_create_bookmark_menu (browser, folder, menu);
    // Remove all menuitems when the menu is hidden.
    // FIXME: We really *should* run the line below, but it won't work like that
    /*g_signal_connect (menu, "hide", G_CALLBACK (gtk_container_foreach),
                      gtk_widget_destroy);*/
    sokoke_widget_popup (GTK_WIDGET (toolitem), GTK_MENU (menu),
		         NULL, SOKOKE_MENU_POSITION_LEFT);
}

static void
midori_browser_menu_bookmarks_item_activate_cb (GtkWidget*     widget,
                                                MidoriBrowser* browser)
{
    KatzeXbelItem* item = (KatzeXbelItem*)g_object_get_data (G_OBJECT (widget),
                                                             "KatzeXbelItem");
    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    g_object_set (web_view, "uri", katze_xbel_bookmark_get_href (item), NULL);
    gtk_widget_grab_focus (web_view);
}

static void
_midori_browser_create_bookmark_menu (MidoriBrowser* browser,
                                      KatzeXbelItem* folder,
                                      GtkWidget*     menu)
{
    guint n = katze_xbel_folder_get_n_items (folder);
    guint i;
    for (i = 0; i < n; i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (folder, i);
        const gchar* title = katze_xbel_item_is_separator (item)
            ? "" : katze_xbel_item_get_title (item);
        /* const gchar* desc = katze_xbel_item_is_separator (item)
            ? "" : katze_xbel_item_get_desc (item); */
        GtkWidget* menuitem = NULL;
        switch (katze_xbel_item_get_kind (item))
        {
        case KATZE_XBEL_ITEM_KIND_FOLDER:
            // FIXME: what about katze_xbel_folder_is_folded?
            menuitem = gtk_image_menu_item_new_with_label (title);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
                gtk_image_new_from_stock (GTK_STOCK_DIRECTORY,
                                          GTK_ICON_SIZE_MENU));
            GtkWidget* _menu = gtk_menu_new ();
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), _menu);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_browser_bookmark_menu_folder_activate_cb),
                browser);
            g_object_set_data (G_OBJECT (menuitem), "KatzeXbelItem", item);
            break;
        case KATZE_XBEL_ITEM_KIND_BOOKMARK:
            menuitem = gtk_image_menu_item_new_with_label (title);
            GtkWidget* image = gtk_image_new_from_stock (STOCK_BOOKMARK,
                                                         GTK_ICON_SIZE_MENU);
            gtk_widget_show (image);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
                                           image);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_browser_menu_bookmarks_item_activate_cb),
                browser);
            g_object_set_data (G_OBJECT (menuitem), "KatzeXbelItem", item);
            break;
        case KATZE_XBEL_ITEM_KIND_SEPARATOR:
            menuitem = gtk_separator_menu_item_new ();
            break;
        default:
            g_warning ("Unknown xbel item kind");
         }
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
    // Show the Manage search engines dialog. Create it if necessary.
    static GtkWidget* dialog;
    if (GTK_IS_DIALOG (dialog))
        gtk_window_present (GTK_WINDOW (dialog));
    else
    {
        dialog = webSearch_manageSearchEngines_dialog_new (browser);
        gtk_widget_show (dialog);
    }
}

static void
_action_tab_previous_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gint n = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), n - 1);
}

static void
_action_tab_next_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    // Advance one tab or jump to the first one if we are at the last one
    gint n = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
    if (n == gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook)) - 1)
        n = -1;
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), n + 1);
}

static void
midori_browser_window_menu_item_activate_cb (GtkWidget* widget,
                                             GtkWidget* web_view)
{
    MidoriBrowser* browser = MIDORI_BROWSER (gtk_widget_get_toplevel (web_view));
    if (!browser)
    {
        g_warning ("Orphaned web view");
        return;
    }

    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* scrolled = _midori_browser_scrolled_for_child (browser, web_view);
    guint n = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), scrolled);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), n);
}

static const gchar* credits_authors[] = {
    "Christian Dywan <christian@twotoasts.de>", NULL };
static const gchar* credits_documenters/*[]*/ = /*{
    */NULL/* }*/;
static const gchar* credits_artists[] = {
    "Nancy Runge <nancy@twotoasts.de>", NULL };

static const gchar* license =
 "This library is free software; you can redistribute it and/or\n"
 "modify it under the terms of the GNU Lesser General Public\n"
 "License as published by the Free Software Foundation; either\n"
 "version 2.1 of the License, or (at your option) any later version.\n";

static void
_action_about_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
    gtk_show_about_dialog (GTK_WINDOW (browser),
        "logo-icon-name", gtk_window_get_icon_name (GTK_WINDOW (browser)),
        "name", PACKAGE_NAME,
        "version", PACKAGE_VERSION,
        "comments", _("A lightweight web browser."),
        "copyright", "Copyright © 2007-2008 Christian Dywan",
        "website", "http://software.twotoasts.de",
        "authors", credits_authors,
        "documenters", credits_documenters,
        "artists", credits_artists,
        "license", license,
        "wrap-license", TRUE,
        "translator-credits", _("translator-credits"),
        NULL);
}

static void
midori_browser_location_changed_cb (GtkWidget*     widget,
                                    MidoriBrowser* browser)
{
    // Preserve changes to the uri
    /*const gchar* newUri = gtk_entry_get_text(GTK_ENTRY(widget));
    katze_xbel_bookmark_set_href(browser->sessionItem, newUri);*/
    // FIXME: If we want this feature, this is the wrong approach
}

static void
_action_panel_activate (GtkToggleAction* action,
                        MidoriBrowser*   browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (priv->settings, "show-panel", active, NULL);
    sokoke_widget_set_visible (priv->panel, active);
}

static void
_action_open_in_panel_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    const gchar* uri = midori_web_view_get_display_uri (MIDORI_WEB_VIEW (web_view));
    // FIXME: Don't assign the uri here, update it properly while navigating
    g_object_set (priv->settings, "last-pageholder-uri", uri, NULL);
    gint n = midori_panel_page_num (MIDORI_PANEL (priv->panel),
                                    priv->panel_pageholder);
    midori_panel_set_current_page (MIDORI_PANEL (priv->panel), n);
    gtk_widget_show (priv->panel);
    g_object_set (priv->panel_pageholder, "uri", uri, NULL);
}


static void
midori_panel_notify_position_cb (GObject*       object,
                                 GParamSpec*    arg1,
                                 MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean position = gtk_paned_get_position (GTK_PANED (object));
    g_object_set (priv->settings, "last-panel-position", position, NULL);
}

static gboolean
midori_panel_close_cb (MidoriPanel*   panel,
                       MidoriBrowser* browser)
{
    _action_set_active (browser, "Panel", FALSE);
    return FALSE;
}

static void
gtk_notebook_switch_page_cb (GtkWidget*       widget,
                             GtkNotebookPage* page,
                             guint            page_num,
                             MidoriBrowser*   browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* web_view = midori_browser_get_current_web_view (browser);
    const gchar* uri = midori_web_view_get_display_uri (MIDORI_WEB_VIEW (web_view));
    gtk_entry_set_text (GTK_ENTRY (priv->location), uri);
    const gchar* title = midori_web_view_get_display_title (
        MIDORI_WEB_VIEW (web_view));
    gchar* window_title = g_strconcat (title, " - ",
                                       g_get_application_name (), NULL);
    gtk_window_set_title (GTK_WINDOW (browser), window_title);
    g_free (window_title);
    _midori_browser_set_statusbar_text (browser, NULL);
    _midori_browser_update_interface (browser);
}

static void
_action_bookmark_open_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkTreeView* treeview = GTK_TREE_VIEW (priv->panel_bookmarks);
    GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            if (katze_xbel_item_is_bookmark (item))
                g_object_set (midori_browser_get_current_web_view (browser),
                              "uri", katze_xbel_bookmark_get_href(item), NULL);
        }
    }
}

static void
_action_bookmark_open_tab_activate (GtkAction*     action,
                                    MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkTreeView* treeview = GTK_TREE_VIEW (priv->panel_bookmarks);
    GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            if (katze_xbel_item_is_bookmark (item))
            {
                gint n = midori_browser_add_xbel_item (browser, item);
                _midori_browser_set_current_page_smartly (browser, n);
            }
        }
    }
}

static void
_action_bookmark_open_window_activate (GtkAction*     action,
                                       MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkTreeView* treeview = GTK_TREE_VIEW (priv->panel_bookmarks);
    GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            if (katze_xbel_item_is_bookmark (item))
            {
                gint n = midori_browser_add_xbel_item (browser, item);
                _midori_browser_set_current_page_smartly (browser, n);
            }
        }
    }
}

static void
_action_bookmark_edit_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkTreeView* treeview = GTK_TREE_VIEW (priv->panel_bookmarks);
    GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            if (!katze_xbel_item_is_separator (item))
                midori_browser_edit_bookmark_dialog_new (browser, item);
        }
    }
}

static void
_action_undo_tab_close_activate (GtkAction*     action,
                                 MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    // Reopen the most recent trash item
    KatzeXbelItem* item = midori_trash_get_nth_xbel_item (priv->trash, 0);
    gint n = midori_browser_add_xbel_item (browser, item);
    midori_browser_set_current_page (browser, n);
    midori_trash_remove_nth_item (priv->trash, 0);
    _midori_browser_update_actions (browser);
}

static void
_action_trash_empty_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    midori_trash_empty (priv->trash);
    _midori_browser_update_actions (browser);
}

static void
_action_bookmark_delete_activate (GtkAction* action,
                                  MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkTreeView* treeview = GTK_TREE_VIEW (priv->panel_bookmarks);
    GtkTreeSelection* selection = gtk_tree_view_get_selection (treeview);
    if (selection)
    {
        GtkTreeModel* model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
            KatzeXbelItem* item;
            gtk_tree_model_get (model, &iter, 0, &item, -1);
            KatzeXbelItem* parent = katze_xbel_item_get_parent (item);
            katze_xbel_folder_remove_item (parent, item);
            katze_xbel_item_unref (item);
        }
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
 { "PageSetup", GTK_STOCK_PROPERTIES,
   N_("Pa_ge Setup"), "",
   "Configure your print settings", NULL/*G_CALLBACK (_action_page_setup_activate)*/ },
 { "PrintPreview", GTK_STOCK_PRINT_PREVIEW,
   NULL, "",
   N_("Show a preview of the printed page"), NULL/*G_CALLBACK (_action_print_preview_activate)*/ },
 { "Print", GTK_STOCK_PRINT,
   NULL, "<Ctrl>p",
   N_("Print the current page"), G_CALLBACK (_action_print_activate) },
 { "Quit", GTK_STOCK_QUIT,
   NULL, "<Ctrl>q",
   N_("Quit the application"), G_CALLBACK (_action_quit_activate) },

 { "Edit", NULL, N_("_Edit"), NULL, NULL, G_CALLBACK (_action_edit_activate) },
 { "Undo", GTK_STOCK_UNDO,
   NULL, "<Ctrl>z",
   N_("Undo the last modification"), NULL/*G_CALLBACK (_action_undo_activate)*/ },
 { "Redo", GTK_STOCK_REDO,
   NULL, "<Ctrl><Shift>z",
   N_("Redo the last modification"), NULL/*G_CALLBACK (_action_redo_activate)*/ },
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
   N_("View Source"), "",
   N_("View the source code of the page"), /*G_CALLBACK (_action_source_view_activate)*/ },
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
 { "Location", GTK_STOCK_JUMP_TO,
   N_("Location..."), "<Ctrl>l",
   N_("Open a particular location"), G_CALLBACK (_action_location_activate) },
 { "Search", GTK_STOCK_FIND,
   N_("Web Search..."), "<Ctrl><Shift>f",
   N_("Run a web search"), G_CALLBACK (_action_search_activate) },
 { "OpenInPageholder", GTK_STOCK_JUMP_TO,
   N_("Open in Page_holder..."), "",
   N_("Open the current page in the pageholder"), G_CALLBACK (_action_open_in_panel_activate) },
 { "Trash", STOCK_USER_TRASH,
   N_("Closed Tabs and Windows"), "",
   N_("Reopen a previously closed tab or window"), NULL },
 { "TrashEmpty", GTK_STOCK_CLEAR,
   N_("Empty Trash"), "",
   N_("Delete the contents of the trash"), G_CALLBACK (_action_trash_empty_activate) },
 { "UndoTabClose", GTK_STOCK_UNDELETE,
   N_("Undo Close Tab"), "",
   N_("Open the last closed tab"), G_CALLBACK (_action_undo_tab_close_activate) },

 { "Bookmarks", NULL, N_("_Bookmarks") },
 { "BookmarkAdd", STOCK_BOOKMARK_ADD,
   NULL, "<Ctrl>d",
   N_("Add a new bookmark"), G_CALLBACK (_action_bookmark_add_activate) },
 { "BookmarksManage", NULL,
   N_("_Manage Bookmarks"), "<Ctrl>b",
   N_("Add, edit and remove bookmarks..."), NULL/*G_CALLBACK (_action_bookmarks_manage_activate)*/ },
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
 { "TabOverview", NULL,
   N_("Tab _Overview"), "",
   N_("Show an overview of all open tabs"), NULL/*G_CALLBACK (_action_tab_overview_activate)*/ },

 { "Help", NULL, N_("_Help") },
 { "HelpContents", GTK_STOCK_HELP,
   N_("_Contents"), "F1",
   N_("Show the documentation"), NULL/*G_CALLBACK (_action_help_contents_activate)*/ },
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
 { "WorkOffline", GTK_STOCK_DISCONNECT,
   N_("_Work Offline"), "",
   N_("Work without a network connection"), NULL/*G_CALLBACK (_action_work_offline_activate)*/,
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
    MidoriBrowserPrivate* priv = browser->priv;

    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
        {
            gtk_widget_hide (priv->menubar);
            g_object_set (priv->button_fullscreen,
                          "stock-id", GTK_STOCK_LEAVE_FULLSCREEN, NULL);
            gtk_widget_show (priv->button_fullscreen);
        }
        else
        {
            gtk_widget_show (priv->menubar);
            gtk_widget_hide (priv->button_fullscreen);
            g_object_set (priv->button_fullscreen,
                          "stock-id", GTK_STOCK_FULLSCREEN, NULL);
        }
    }
}

static void
midori_browser_size_allocate_cb (MidoriBrowser* browser,
                                 GtkAllocation* allocation)
{
    MidoriBrowserPrivate* priv = browser->priv;
    GtkWidget* widget = GTK_WIDGET (browser);

    if (GTK_WIDGET_REALIZED (widget))
    {
        GdkWindowState state = gdk_window_get_state (widget->window);
        if (!(state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)))
        {
            g_object_set (priv->settings,
                          "last-window-width", allocation->width,
                          "last-window-height", allocation->height, NULL);
        }
    }
}

static void
midori_browser_destroy_cb (MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    // Destroy tabs first, so widgets can still see window elements on destroy
    gtk_container_foreach (GTK_CONTAINER (priv->notebook),
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
    "<menuitem action='PageSetup'/>"
    "<menuitem action='PrintPreview'/>"
    "<menuitem action='Print'/>"
    "<separator/>"
    "<menuitem action='PrivateBrowsing'/>"
    "<menuitem action='WorkOffline'/>"
    "<separator/>"
    "<menuitem action='Quit'/>"
   "</menu>"
   "<menu action='Edit'>"
    "<menuitem action='Undo'/>"
    "<menuitem action='Redo'/>"
    "<separator/>"
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
    // Closed tabs shall be prepended here
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
    "<menuitem action='BookmarksManage'/>"
    "<separator/>"
    // Bookmarks shall be appended here
   "</menu>"
   "<menu action='Tools'>"
    "<menuitem action='ManageSearchEngines'/>"
    // Panel items shall be appended here
   "</menu>"
   "<menu action='Window'>"
    "<menuitem action='TabPrevious'/>"
    "<menuitem action='TabNext'/>"
    "<menuitem action='TabOverview'/>"
    "<separator/>"
    // All open tabs shall be appended here
   "</menu>"
   "<menu action='Help'>"
    "<menuitem action='HelpContents'/>"
    "<menuitem action='About'/>"
   "</menu>"
  "</menubar>"
  "<toolbar name='toolbar_navigation'>"
   "<toolitem action='TabNew'/>"
   "<toolitem action='Back'/>"
   "<toolitem action='Forward'/>"
   "<toolitem action='ReloadStop'/>"
   "<toolitem action='Homepage'/>"
   "<placeholder name='Location'/>"
   "<placeholder name='Search'/>"
   "<placeholder name='TabTrash'/>"
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
midori_browser_realize_cb (GtkStyle* style, MidoriBrowser* browser)
{
    GdkScreen* screen = gtk_widget_get_screen (GTK_WIDGET (browser));
    if (screen)
    {
        GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, "midori"))
            gtk_window_set_icon_name (GTK_WINDOW (browser), "midori");
        else
            gtk_window_set_icon_name (GTK_WINDOW (browser), "web-browser");
    }
}

static void
midori_browser_init (MidoriBrowser* browser)
{
    browser->priv = MIDORI_BROWSER_GET_PRIVATE (browser);

    MidoriBrowserPrivate* priv = browser->priv;

    // Setup the window metrics
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

    // Let us see some ui manager magic
    priv->action_group = gtk_action_group_new ("Browser");
    gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
                                  entries, entries_n, browser);
    gtk_action_group_add_toggle_actions (priv->action_group,
        toggle_entries, toggle_entries_n, browser);
    GtkUIManager* ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, priv->action_group, 0);
    gtk_window_add_accel_group (GTK_WINDOW (browser),
                                gtk_ui_manager_get_accel_group (ui_manager));

    GError* error = NULL;
    if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_markup, -1, &error))
    {
        // TODO: Should this be a message dialog? When does this happen?
        g_message ("User interface couldn't be created: %s", error->message);
        g_error_free (error);
    }

    GtkAction* action;
    // Make all actions except toplevel menus which lack a callback insensitive
    // This will vanish once all actions are implemented
    guint i;
    for (i = 0; i < entries_n; i++)
    {
        action = gtk_action_group_get_action(priv->action_group,
                                             entries[i].name);
        gtk_action_set_sensitive (action,
                                  entries[i].callback || !entries[i].tooltip);
    }
    for (i = 0; i < toggle_entries_n; i++)
    {
        action = gtk_action_group_get_action (priv->action_group,
                                              toggle_entries[i].name);
        gtk_action_set_sensitive (action, toggle_entries[i].callback != NULL);
    }

    //_action_set_active(browser, "Transferbar", config->toolbarTransfers);

    // Create the menubar
    priv->menubar = gtk_ui_manager_get_widget (ui_manager, "/menubar");
    GtkWidget* menuitem = gtk_menu_item_new ();
    gtk_widget_show (menuitem);
    priv->throbber = katze_throbber_new();
    gtk_widget_show(priv->throbber);
    gtk_container_add (GTK_CONTAINER (menuitem), priv->throbber);
    gtk_widget_set_sensitive (menuitem, FALSE);
    gtk_menu_item_set_right_justified (GTK_MENU_ITEM (menuitem), TRUE);
    gtk_menu_shell_append (GTK_MENU_SHELL (priv->menubar), menuitem);
    gtk_box_pack_start (GTK_BOX (vbox), priv->menubar, FALSE, FALSE, 0);
    menuitem = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Trash");
    g_signal_connect (menuitem, "activate",
                      G_CALLBACK (midori_browser_menu_trash_activate_cb),
                      browser);
    priv->menu_bookmarks = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Bookmarks")));
    menuitem = gtk_separator_menu_item_new ();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu_bookmarks), menuitem);
    priv->popup_bookmark = gtk_ui_manager_get_widget (
        ui_manager, "/popup_bookmark");
    g_object_ref (priv->popup_bookmark);
    priv->menu_tools = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Tools")));
    menuitem = gtk_separator_menu_item_new();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu_tools), menuitem);
    priv->menu_window = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Window")));
    menuitem = gtk_separator_menu_item_new();
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu_window), menuitem);
    gtk_widget_show (priv->menubar);
    _action_set_sensitive (browser, "PrivateBrowsing", FALSE);
    _action_set_sensitive (browser, "WorkOffline", FALSE);

    // Create the navigationbar
    priv->navigationbar = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation");
    // FIXME: settings should be connected with screen changes
    GtkSettings* gtk_settings = gtk_widget_get_settings (GTK_WIDGET (browser));
    if (gtk_settings)
        g_signal_connect (gtk_settings, "notify::gtk-toolbar-style",
            G_CALLBACK (midori_browser_navigationbar_notify_style_cb), browser);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->navigationbar), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), priv->navigationbar, FALSE, FALSE, 0);
    priv->button_tab_new = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation/TabNew");
    g_object_set (_action_by_name (browser, "Back"), "is-important", TRUE, NULL);
    priv->button_homepage = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation/Homepage");

    // Location
    priv->location = sexy_icon_entry_new ();
    sokoke_entry_setup_completion (GTK_ENTRY (priv->location));
    priv->location_icon = gtk_image_new ();
    sexy_icon_entry_set_icon (SEXY_ICON_ENTRY (priv->location)
     , SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE (priv->location_icon));
    sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (priv->location));
    g_object_connect (priv->location,
                      "signal::key-press-event",
                      midori_browser_location_key_press_event_cb, browser,
                      "signal::focus-out-event",
                      midori_browser_location_focus_out_event_cb, browser,
                      "signal::changed",
                      midori_browser_location_changed_cb, browser,
                      NULL);
    GtkToolItem* toolitem = gtk_tool_item_new ();
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_container_add (GTK_CONTAINER(toolitem), priv->location);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->navigationbar), toolitem, -1);

    // Search
    priv->search = sexy_icon_entry_new ();
    sexy_icon_entry_set_icon_highlight (SEXY_ICON_ENTRY (priv->search),
                                        SEXY_ICON_ENTRY_PRIMARY, TRUE);
    // TODO: Make this actively resizable or enlarge to fit contents?
    // FIXME: The interface is somewhat awkward and ought to be rethought
    // TODO: Display "show in context menu" search engines as "completion actions"
    sokoke_entry_setup_completion (GTK_ENTRY (priv->search));
    g_object_connect (priv->search,
                      "signal::icon-released",
                      on_webSearch_icon_released, browser,
                      "signal::key-press-event",
                      on_webSearch_key_down, browser,
                      "signal::scroll-event",
                      on_webSearch_scroll, browser,
                      "signal::activate",
                      on_webSearch_activate, browser,
                      "signal::focus-out-event",
                      midori_browser_search_focus_out_event_cb, browser,
                      NULL);
    toolitem = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (toolitem), priv->search);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->navigationbar), toolitem, -1);
    action = gtk_action_group_get_action (priv->action_group, "Trash");
    priv->button_trash = gtk_action_create_tool_item (action);
    g_signal_connect (priv->button_trash, "clicked",
                      G_CALLBACK (midori_browser_menu_trash_activate_cb), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->navigationbar),
                        GTK_TOOL_ITEM (priv->button_trash), -1);
    sokoke_container_show_children (GTK_CONTAINER (priv->navigationbar));
    action = gtk_action_group_get_action (priv->action_group, "Fullscreen");
    priv->button_fullscreen = gtk_action_create_tool_item (action);
    gtk_widget_hide (priv->button_fullscreen);
    g_signal_connect (priv->button_fullscreen, "clicked",
                      G_CALLBACK (_action_fullscreen_activate), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->navigationbar),
                        GTK_TOOL_ITEM (priv->button_fullscreen), -1);

    // Bookmarkbar
    priv->bookmarkbar = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->bookmarkbar),
                               GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (priv->bookmarkbar),
                           GTK_TOOLBAR_BOTH_HORIZ);
    _midori_browser_create_bookmark_menu (browser, bookmarks,
                                          priv->menu_bookmarks);
    for (i = 0; i < katze_xbel_folder_get_n_items (bookmarks); i++)
    {
        KatzeXbelItem* item = katze_xbel_folder_get_nth_item (bookmarks, i);
        const gchar* title = katze_xbel_item_is_separator (item)
         ? "" : katze_xbel_item_get_title (item);
        const gchar* desc = katze_xbel_item_is_separator (item)
         ? "" : katze_xbel_item_get_desc (item);
        switch (katze_xbel_item_get_kind (item))
        {
        case KATZE_XBEL_ITEM_KIND_FOLDER:
            toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_DIRECTORY);
            gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), title);
            gtk_tool_item_set_is_important(toolitem, TRUE);
            g_signal_connect (toolitem, "clicked",
                G_CALLBACK (midori_browser_bookmarkbar_folder_activate_cb),
                browser);
            sokoke_tool_item_set_tooltip_text(toolitem, desc);
            g_object_set_data (G_OBJECT (toolitem), "KatzeXbelItem", item);
            break;
        case KATZE_XBEL_ITEM_KIND_BOOKMARK:
            toolitem = gtk_tool_button_new_from_stock (STOCK_BOOKMARK);
            gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), title);
            gtk_tool_item_set_is_important(toolitem, TRUE);
            g_signal_connect (toolitem, "clicked",
                G_CALLBACK (midori_browser_menu_bookmarks_item_activate_cb),
                browser);
            sokoke_tool_item_set_tooltip_text(toolitem, desc);
            g_object_set_data (G_OBJECT (toolitem), "KatzeXbelItem", item);
            break;
        case KATZE_XBEL_ITEM_KIND_SEPARATOR:
            toolitem = gtk_separator_tool_item_new ();
            break;
        default:
            g_warning ("Unknown item kind");
        }
        gtk_toolbar_insert (GTK_TOOLBAR (priv->bookmarkbar), toolitem, -1);
    }
    sokoke_container_show_children (GTK_CONTAINER (priv->bookmarkbar));
    gtk_box_pack_start (GTK_BOX (vbox), priv->bookmarkbar, FALSE, FALSE, 0);

    // Superuser warning
    GtkWidget* hbox;
    if ((hbox = sokoke_superuser_warning_new ()))
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    // Create the panel
    GtkWidget* hpaned = gtk_hpaned_new ();
    g_signal_connect (hpaned, "notify::position",
                      G_CALLBACK (midori_panel_notify_position_cb),
                      browser);
    gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
    gtk_widget_show (hpaned);
    priv->panel = g_object_new (MIDORI_TYPE_PANEL,
                                "shadow-type", GTK_SHADOW_IN,
                                "menu", priv->menu_tools,
                                NULL);
    g_signal_connect (priv->panel, "close",
                      G_CALLBACK (midori_panel_close_cb), browser);
    gtk_paned_pack1 (GTK_PANED (hpaned), priv->panel, FALSE, FALSE);

    // Bookmarks
    GtkWidget* box = gtk_vbox_new (FALSE, 0);
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer_text;
    GtkCellRenderer* renderer_pixbuf;
    GtkTreeStore* treestore = gtk_tree_store_new (1, KATZE_TYPE_XBEL_ITEM);
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
    _tree_store_insert_folder (GTK_TREE_STORE (treestore), NULL, bookmarks);
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
    priv->panel_bookmarks = treeview;
    gtk_widget_show_all (box);
    GtkWidget* toolbar = gtk_ui_manager_get_widget (ui_manager,
                                                    "/toolbar_bookmarks");
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);
    gtk_widget_show_all (toolbar);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              box, toolbar,
                              "vcard", _("Bookmarks"));

    // Transfers
    GtkWidget* panel = midori_web_view_new ();
    gtk_widget_show (panel);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              panel, NULL,
                              "package", _("Transfers"));

    // Console
    priv->panel_console = midori_console_new ();
    gtk_widget_show (priv->panel_console);
    toolbar = midori_console_get_toolbar (MIDORI_CONSOLE (priv->panel_console));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              priv->panel_console, toolbar,
                              "terminal", _("Console"));

    // History
    panel = midori_web_view_new ();
    gtk_widget_show (panel);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              panel, NULL,
                              "document-open-recent", _("History"));

    // Pageholder
    priv->panel_pageholder = g_object_new (MIDORI_TYPE_WEB_VIEW,
                                           "uri", "",
                                           NULL);
    gtk_widget_show (priv->panel_pageholder);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              priv->panel_pageholder, NULL,
                              GTK_STOCK_CONVERT, _("Pageholder"));

    // Userscripts
    panel = midori_addons_new (GTK_WIDGET (browser), MIDORI_ADDON_USER_SCRIPTS);
    gtk_widget_show (panel);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (panel));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              panel, toolbar,
                              "", _("Userscripts"));
    // Userstyles
    /*panel = midori_addons_new (GTK_WIDGET (browser), MIDORI_ADDON_USER_STYLES);
    gtk_widget_show (panel);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (panel));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              panel, toolbar,
                              "", _("Userstyles"));*/

    // Notebook, containing all web_views
    priv->notebook = gtk_notebook_new ();
    gtk_notebook_set_scrollable (GTK_NOTEBOOK (priv->notebook), TRUE);
    gtk_paned_pack2 (GTK_PANED (hpaned), priv->notebook, FALSE, FALSE);
    g_signal_connect_after (priv->notebook, "switch-page",
                            G_CALLBACK (gtk_notebook_switch_page_cb),
                            browser);
    gtk_widget_show (priv->notebook);

    // Incremental findbar
    priv->find = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->find), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (priv->find), GTK_TOOLBAR_BOTH_HORIZ);
    toolitem = gtk_tool_item_new ();
    gtk_container_set_border_width (GTK_CONTAINER (toolitem), 6);
    gtk_container_add (GTK_CONTAINER (toolitem),
                       gtk_label_new_with_mnemonic (_("_Inline find:")));
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), toolitem, -1);
    priv->find_text = sexy_icon_entry_new ();
    GtkWidget* icon = gtk_image_new_from_stock (GTK_STOCK_FIND,
                                                GTK_ICON_SIZE_MENU);
    sexy_icon_entry_set_icon (SEXY_ICON_ENTRY(priv->find_text),
                              SEXY_ICON_ENTRY_PRIMARY, GTK_IMAGE(icon));
    sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY(priv->find_text));
    g_signal_connect (priv->find_text, "activate",
        G_CALLBACK (_action_find_next_activate), browser);
    toolitem = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (toolitem), priv->find_text);
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR(priv->find), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (_action_find_previous_activate), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
    gtk_tool_item_set_is_important (toolitem, TRUE);
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (_action_find_next_activate), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), toolitem, -1);
    priv->find_case = gtk_toggle_tool_button_new_from_stock (
        GTK_STOCK_SPELL_CHECK);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (priv->find_case), _("Match Case"));
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (priv->find_case), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), priv->find_case, -1);
    priv->find_highlight = gtk_toggle_tool_button_new_from_stock (
        GTK_STOCK_SELECT_ALL);
    g_signal_connect (priv->find_highlight, "toggled",
                      G_CALLBACK (_find_highlight_toggled), browser);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (priv->find_highlight),
                               "Highlight Matches");
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (priv->find_highlight), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), priv->find_highlight, -1);
    toolitem = gtk_separator_tool_item_new ();
    gtk_separator_tool_item_set_draw (
        GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("Close Findbar"));
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (midori_browser_find_button_close_clicked_cb), browser);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->find), toolitem, -1);
    sokoke_container_show_children (GTK_CONTAINER (priv->find));
    gtk_box_pack_start (GTK_BOX (vbox), priv->find, FALSE, FALSE, 0);

    // Statusbar
    // TODO: fix children overlapping statusbar border
    priv->statusbar = gtk_statusbar_new ();
    gtk_box_pack_start (GTK_BOX (vbox), priv->statusbar, FALSE, FALSE, 0);
    priv->progressbar = gtk_progress_bar_new ();
    // Setting the progressbar's height to 1 makes it fit in the statusbar
    gtk_widget_set_size_request (priv->progressbar, -1, 1);
    gtk_box_pack_start (GTK_BOX (priv->statusbar), priv->progressbar,
                        FALSE, FALSE, 3);

    // Extensions
    panel = midori_addons_new (GTK_WIDGET (browser), MIDORI_ADDON_EXTENSIONS);
    gtk_widget_show (panel);
    toolbar = midori_addons_get_toolbar (MIDORI_ADDONS (panel));
    gtk_widget_show (toolbar);
    midori_panel_append_page (MIDORI_PANEL (priv->panel),
                              panel, toolbar,
                              "", _("Extensions"));

    g_object_unref (ui_manager);
}

static void
midori_browser_finalize (GObject* object)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);
    MidoriBrowserPrivate* priv = browser->priv;

    g_free (priv->uri);
    g_free (priv->title);
    g_free (priv->statusbar_text);

    if (priv->proxy_xbel_folder)
        katze_xbel_item_unref (priv->proxy_xbel_folder);

    if (priv->settings)
        g_object_unref (priv->settings);
    if (priv->trash)
        g_object_unref (priv->trash);

    G_OBJECT_CLASS (midori_browser_parent_class)->finalize (object);
}

static void
_midori_browser_set_toolbar_style (MidoriBrowser*     browser,
                                   MidoriToolbarStyle toolbar_style)
{
    MidoriBrowserPrivate* priv = browser->priv;

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
    gtk_toolbar_set_style (GTK_TOOLBAR (priv->navigationbar),
                           gtk_toolbar_style);
}

static void
_midori_browser_update_settings (MidoriBrowser* browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    gboolean remember_last_window_size;
    gint last_window_width, last_window_height;
    gint last_panel_position, last_panel_page;
    gboolean show_navigationbar, show_bookmarkbar, show_panel, show_statusbar;
    gboolean small_toolbar, show_new_tab, show_homepage,
        show_web_search, show_trash;
    MidoriToolbarStyle toolbar_style;
    gint last_web_search;
    gchar* last_pageholder_uri;
    g_object_get (priv->settings,
                  "remember-last-window-size", &remember_last_window_size,
                  "last-window-width", &last_window_width,
                  "last-window-height", &last_window_height,
                  "last-panel-position", &last_panel_position,
                  "last-panel-page", &last_panel_page,
                  "show-navigationbar", &show_navigationbar,
                  "show-bookmarkbar", &show_bookmarkbar,
                  "show-panel", &show_panel,
                  "show-statusbar", &show_statusbar,
                  "small-toolbar", &small_toolbar,
                  "show-new-tab", &show_new_tab,
                  "show-homepage", &show_homepage,
                  "show-web-search", &show_web_search,
                  "show-trash", &show_trash,
                  "toolbar-style", &toolbar_style,
                  "last-web-search", &last_web_search,
                  "last-pageholder-uri", &last_pageholder_uri,
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
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->navigationbar),
                               small_toolbar ? GTK_ICON_SIZE_SMALL_TOOLBAR
                               : GTK_ICON_SIZE_LARGE_TOOLBAR);

    update_searchEngine (last_web_search, priv->search);

    gtk_paned_set_position (GTK_PANED (gtk_widget_get_parent (priv->panel)),
                            last_panel_position);
    midori_panel_set_current_page (MIDORI_PANEL (priv->panel), last_panel_page);
    g_object_set (priv->panel_pageholder, "uri", last_pageholder_uri, NULL);

    _action_set_active (browser, "Navigationbar", show_navigationbar);
    _action_set_active (browser, "Bookmarkbar", show_bookmarkbar);
    _action_set_active (browser, "Panel", show_panel);
    _action_set_active (browser, "Statusbar", show_statusbar);

    sokoke_widget_set_visible (priv->button_tab_new, show_new_tab);
    sokoke_widget_set_visible (priv->button_homepage, show_homepage);
    sokoke_widget_set_visible (priv->search, show_web_search);
    sokoke_widget_set_visible (priv->button_trash, show_trash);

    g_free (last_pageholder_uri);
}

static void
midori_browser_settings_notify (MidoriWebSettings* web_settings,
                                GParamSpec*        pspec,
                                MidoriBrowser*     browser)
{
    MidoriBrowserPrivate* priv = browser->priv;

    const gchar* name = g_intern_string (pspec->name);
    GValue value = {0, };

    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (priv->settings), name, &value);

    if (name == g_intern_string ("toolbar-style"))
        _midori_browser_set_toolbar_style (browser, g_value_get_enum (&value));
    else if (name == g_intern_string ("small-toolbar"))
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->navigationbar),
            g_value_get_boolean (&value) ? GTK_ICON_SIZE_SMALL_TOOLBAR
            : GTK_ICON_SIZE_LARGE_TOOLBAR);
    else if (name == g_intern_string ("show-new-tab"))
        sokoke_widget_set_visible (priv->button_tab_new,
            g_value_get_boolean (&value));
    else if (name == g_intern_string ("show-homepage"))
        sokoke_widget_set_visible (priv->button_homepage,
            g_value_get_boolean (&value));
    else if (name == g_intern_string ("show-web-search"))
        sokoke_widget_set_visible (priv->search,
            g_value_get_boolean (&value));
    else if (name == g_intern_string ("show-trash"))
        sokoke_widget_set_visible (priv->button_trash,
            g_value_get_boolean (&value));
    else if (!g_object_class_find_property (G_OBJECT_GET_CLASS (web_settings),
                                             name))
         g_warning (_("Unexpected setting '%s'"), name);
    g_value_unset (&value);
}

static void
midori_browser_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);
    MidoriBrowserPrivate* priv = browser->priv;

    switch (prop_id)
    {
    case PROP_TAB:
        midori_browser_set_current_tab (browser, g_value_get_object (value));
        break;
    case PROP_STATUSBAR_TEXT:
        _midori_browser_set_statusbar_text (browser, g_value_get_string (value));
        break;
    case PROP_SETTINGS:
        if (priv->settings)
            g_signal_handlers_disconnect_by_func (priv->settings,
                                                  midori_browser_settings_notify,
                                                  browser);
        katze_object_assign (priv->settings, g_value_get_object (value));
        g_object_ref (priv->settings);
        _midori_browser_update_settings (browser);
        g_signal_connect (priv->settings, "notify",
                      G_CALLBACK (midori_browser_settings_notify), browser);
        // FIXME: Assigning settings must be conditional, if web view or not
        // FIXME: Assign settings only if the same settings object was used
        gtk_container_foreach (GTK_CONTAINER (priv->notebook),
                               (GtkCallback) midori_web_view_set_settings,
                               priv->settings);
        break;
    case PROP_TRASH:
        ; // FIXME: Disconnect handlers
        katze_object_assign (priv->trash, g_value_get_object (value));
        g_object_ref (priv->trash);
        // FIXME: Connect to updates
        _midori_browser_update_actions (browser);
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
    MidoriBrowserPrivate* priv = browser->priv;

    switch (prop_id)
    {
    case PROP_MENUBAR:
        g_value_set_object (value, priv->menubar);
        break;
    case PROP_NAVIGATIONBAR:
        g_value_set_object (value, priv->navigationbar);
        break;
    case PROP_TAB:
        g_value_set_object (value, midori_browser_get_current_tab (browser));
        break;
    case PROP_STATUSBAR:
        g_value_set_object (value, priv->statusbar);
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, priv->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, priv->settings);
        break;
    case PROP_TRASH:
        g_value_set_object (value, priv->trash);
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
 * @widget: a tab
 *
 * Appends an arbitrary widget in the form of a new tab and creates an
 * according item in the Window menu.
 *
 * Return value: the index of the new tab, or -1 in case of an error
 **/
gint
midori_browser_add_tab (MidoriBrowser* browser,
                        GtkWidget*     widget)
{
    g_return_val_if_fail (GTK_IS_WIDGET (widget), -1);

    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    GTK_WIDGET_SET_FLAGS (scrolled, GTK_CAN_FOCUS);
    GtkWidget* child;
    GObjectClass* gobject_class = G_OBJECT_GET_CLASS (widget);
    if (GTK_WIDGET_CLASS (gobject_class)->set_scroll_adjustments_signal)
        child = widget;
    else
    {
        child = gtk_viewport_new (NULL, NULL);
        gtk_widget_show (child);
        gtk_container_add (GTK_CONTAINER (child), widget);
    }
    gtk_container_add (GTK_CONTAINER (scrolled), child);
    gtk_widget_show (scrolled);

    GtkWidget* label = NULL;
    GtkWidget* menuitem = NULL;

    if (MIDORI_IS_WEB_VIEW (widget))
    {
        label = midori_web_view_get_proxy_tab_label (MIDORI_WEB_VIEW (widget));

        menuitem = midori_web_view_get_proxy_menu_item (MIDORI_WEB_VIEW (widget));

        if (priv->proxy_xbel_folder)
        {
            KatzeXbelItem* xbel_item = midori_web_view_get_proxy_xbel_item (
                MIDORI_WEB_VIEW (widget));
            katze_xbel_item_ref (xbel_item);
            katze_xbel_folder_append_item (priv->proxy_xbel_folder, xbel_item);
        }

        g_object_connect (widget,
                          "signal::window-object-cleared",
                          midori_web_view_window_object_cleared_cb, browser,
                          "signal::load-started",
                          midori_web_view_load_started_cb, browser,
                          "signal::load-committed",
                          midori_web_view_load_committed_cb, browser,
                          "signal::progress-started",
                          midori_web_view_progress_started_cb, browser,
                          "signal::progress-changed",
                          midori_web_view_progress_changed_cb, browser,
                          "signal::progress-done",
                          midori_web_view_progress_done_cb, browser,
                          "signal::load-done",
                          midori_web_view_load_done_cb, browser,
                          "signal::title-changed",
                          midori_web_view_title_changed_cb, browser,
                          "signal::status-bar-text-changed",
                          midori_web_view_statusbar_text_changed_cb, browser,
                          "signal::element-motion",
                          midori_web_view_element_motion_cb, browser,
                          "signal::console-message",
                          midori_web_view_console_message_cb, browser,
                          "signal::close",
                          midori_web_view_close_cb, browser,
                          "signal::new-tab",
                          midori_web_view_new_tab_cb, browser,
                          "signal::new-window",
                          midori_web_view_new_window_cb, browser,
                          "signal::populate-popup",
                          midori_web_view_populate_popup_cb, browser,
                          "signal::leave-notify-event",
                          midori_web_view_leave_notify_event_cb, browser,
                          "signal::destroy",
                          midori_web_view_destroy_cb, browser,
                          NULL);
    }

    if (menuitem)
    {
        gtk_widget_show (menuitem);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_browser_window_menu_item_activate_cb), scrolled);
        gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu_window), menuitem);
    }

    guint n = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
    gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook), scrolled,
                              label, n + 1);
    #if GTK_CHECK_VERSION(2, 10, 0)
    gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (priv->notebook),
                                      scrolled, TRUE);
    gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (priv->notebook),
                                     scrolled, TRUE);
    #endif
    _midori_browser_update_actions (browser);

    n = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), scrolled);
    return n;
}

/**
 * midori_browser_remove_tab:
 * @browser: a #MidoriBrowser
 * @widget: a tab
 *
 * Removes an existing tab from the browser, including an associated menu item.
 **/
void
midori_browser_remove_tab (MidoriBrowser* browser,
                           GtkWidget*     widget)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* scrolled = _midori_browser_scrolled_for_child (browser, widget);
    gtk_container_remove (GTK_CONTAINER (priv->notebook), scrolled);

    // FIXME: Remove the menuitem if this is a web view
}

/**
 * midori_browser_add_xbel_item:
 * @browser: a #MidoriBrowser
 * @xbel_item: a bookmark
 *
 * Appends a #KatzeXbelItem in the form of a new tab.
 *
 * Return value: the index of the new tab, or -1 in case of an error
 **/
gint
midori_browser_add_xbel_item (MidoriBrowser* browser,
                              KatzeXbelItem* xbel_item)
{
    MidoriBrowserPrivate* priv = browser->priv;

    g_return_val_if_fail (katze_xbel_item_is_bookmark (xbel_item), -1);

    const gchar* uri = katze_xbel_bookmark_get_href (xbel_item);
    const gchar* title = katze_xbel_item_get_title (xbel_item);
    GtkWidget* web_view = g_object_new (MIDORI_TYPE_WEB_VIEW,
                                        "uri", uri,
                                        "title", title,
                                        "settings", priv->settings,
                                        NULL);
    gtk_widget_show (web_view);

    return midori_browser_add_tab (browser, web_view);
}

/**
 * midori_browser_add_uri:
 * @browser: a #MidoriBrowser
 * @uri: an URI
 *
 * Appends an uri in the form of a new tab.
 *
 * Return value: the index of the new tab, or -1
 **/
gint
midori_browser_add_uri (MidoriBrowser* browser,
                        const gchar*   uri)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* web_view = g_object_new (MIDORI_TYPE_WEB_VIEW,
                                        "uri", uri,
                                        "settings", priv->settings,
                                        NULL);
    gtk_widget_show (web_view);

    return midori_browser_add_tab (browser, web_view);
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
    GtkAction* action = _action_by_name (browser, name);
    if (action)
        gtk_action_activate (action);
    else
        g_warning (_("Unexpected action '%s'."), name);
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
    MidoriBrowserPrivate* priv = browser->priv;

    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), n);
    GtkWidget* scrolled = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), n);
    GtkWidget* widget = _midori_browser_child_for_scrolled (browser, scrolled);
    if (widget && MIDORI_IS_WEB_VIEW (widget)
        && !strcmp (midori_web_view_get_display_uri (
        MIDORI_WEB_VIEW (widget)), ""))
        gtk_widget_grab_focus (priv->location);
    else
        gtk_widget_grab_focus (widget);
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

    MidoriBrowserPrivate* priv = browser->priv;

    return gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
}

/**
 * midori_browser_set_current_tab:
 * @browser: a #MidoriBrowser
 * @widget: a #GtkWidget
 *
 * Switches to the page containing @widget.
 *
 * The widget will also grab the focus automatically.
 **/
void
midori_browser_set_current_tab (MidoriBrowser* browser,
                                GtkWidget*     widget)
{
    MidoriBrowserPrivate* priv = browser->priv;

    GtkWidget* scrolled = _midori_browser_scrolled_for_child (browser, widget);
    gint n = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), scrolled);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), n);
    if (widget && MIDORI_IS_WEB_VIEW (widget)
        && !strcmp (midori_web_view_get_display_uri (
        MIDORI_WEB_VIEW (widget)), ""))
        gtk_widget_grab_focus (priv->location);
    else
        gtk_widget_grab_focus (widget);
}

/**
 * midori_browser_get_current_tab:
 * @browser: a #MidoriBrowser
 *
 * Retrieves the currently selected tab.
 *
 * If there is no tab present at all, %NULL is returned.
 *
 * Return value: the selected tab, or %NULL
 **/
GtkWidget*
midori_browser_get_current_tab (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    MidoriBrowserPrivate* priv = browser->priv;

    gint n = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
    if (n >= 0)
    {
        GtkWidget* widget = _midori_browser_child_for_scrolled (browser,
            gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), n));
        return widget;
    }
    else
        return NULL;
}

/**
 * midori_browser_get_current_web_view:
 * @browser: a #MidoriBrowser
 *
 * Determines the currently selected web view.
 *
 * If there is no web view selected or if there is no tab present
 * at all, %NULL is returned.
 *
 * See also midori_browser_get_current_page
 *
 * Return value: the selected web view, or %NULL
 **/
GtkWidget*
midori_browser_get_current_web_view (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    GtkWidget* web_view = midori_browser_get_current_tab (browser);
    return MIDORI_IS_WEB_VIEW (web_view) ? web_view : NULL;
}

/**
 * midori_browser_get_proxy_xbel_folder:
 * @browser: a #MidoriBrowser
 *
 * Retrieves a proxy xbel folder representing the respective proxy xbel items
 * of the present web views that can be used for session management.
 *
 * The folder is created on the first call and will be updated to reflect
 * changes to all items automatically.
 *
 * Note that this implicitly creates proxy xbel items of all web views.
 *
 * Return value: the proxy #KatzeXbelItem
 **/
KatzeXbelItem*
midori_browser_get_proxy_xbel_folder (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    MidoriBrowserPrivate* priv = browser->priv;

    if (!priv->proxy_xbel_folder)
    {
        priv->proxy_xbel_folder = katze_xbel_folder_new ();
        // FIXME: Fill in xbel items of all present web views
    }
    return priv->proxy_xbel_folder;
}

/**
 * midori_browser_quit:
 * @browser: a #MidoriBrowser
 *
 * Quits the browser, including any other browser windows.
 **/
void
midori_browser_quit (MidoriBrowser* browser)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    g_signal_emit (browser, signals[QUIT], 0);
}
