/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-webview.h"

#include "main.h"
#include "sokoke.h"
#include "compat.h"

#if GLIB_CHECK_VERSION (2, 16, 0)
#include <gio/gio.h>
#endif
#include <webkit/webkit.h>
#include <string.h>

/* This is unstable API, so we need to declare it */
gchar*
webkit_web_view_get_selected_text (WebKitWebView* web_view);

struct _MidoriWebView
{
    WebKitWebView parent_instance;

    gchar* uri;
    gchar* title;
    gboolean is_loading;
    gint progress;
    gchar* statusbar_text;
    gchar* link_uri;

    gboolean middle_click_opens_selection;
    MidoriWebSettings* settings;

    GtkWidget* menu_item;
    GtkWidget* tab_icon;
    GtkWidget* tab_title;
    KatzeXbelItem* xbel_item;
};

G_DEFINE_TYPE (MidoriWebView, midori_web_view, WEBKIT_TYPE_WEB_VIEW)

enum
{
    PROP_0,

    PROP_URI,
    PROP_TITLE,
    PROP_STATUSBAR_TEXT,
    PROP_SETTINGS
};

enum {
    LOAD_STARTED,
    PROGRESS_STARTED,
    PROGRESS_CHANGED,
    PROGRESS_DONE,
    LOAD_DONE,
    ELEMENT_MOTION,
    NEW_TAB,
    NEW_WINDOW,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_web_view_finalize (GObject* object);

static void
midori_web_view_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec);

static void
midori_web_view_get_property (GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec);

static void
midori_web_view_class_init (MidoriWebViewClass* class)
{
    signals[PROGRESS_STARTED] = g_signal_new (
        "progress-started",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, progress_started),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    signals[PROGRESS_CHANGED] = g_signal_new (
        "progress-changed",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, progress_changed),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    signals[PROGRESS_DONE] = g_signal_new (
        "progress-done",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, progress_done),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    signals[LOAD_DONE] = g_signal_new (
        "load-done",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, load_done),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        WEBKIT_TYPE_WEB_FRAME);

    signals[ELEMENT_MOTION] = g_signal_new (
        "element-motion",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, element_motion),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[NEW_TAB] = g_signal_new (
        "new-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, new_tab),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[NEW_WINDOW] = g_signal_new (
        "new-window",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, new_window),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_web_view_finalize;
    gobject_class->set_property = midori_web_view_set_property;
    gobject_class->get_property = midori_web_view_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     "Uri",
                                     _("The URI of the currently loaded page"),
                                     "",
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_TITLE,
                                     g_param_spec_string (
                                     "title",
                                     "Title",
                                     _("The title of the currently loaded page"),
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR_TEXT,
                                     g_param_spec_string (
                                     "statusbar-text",
                                     "Statusbar Text",
                                     _("The text that is displayed in the statusbar"),
                                     "",
                                     G_PARAM_READABLE));

    g_object_class_override_property (gobject_class,
                                      PROP_SETTINGS,
                                      "settings");
}

/*static void
webkit_web_view_load_started (MidoriWebView* web_view,
                              WebKitWebFrame* web_frame)
{
    web_view->is_loading = TRUE;
    web_view->progress = -1;
    katze_throbber_set_animated (KATZE_THROBBER (web_view->tab_icon), TRUE);
}*/

static void
_midori_web_view_set_uri (MidoriWebView* web_view,
                          const gchar*   uri)
{
    katze_assign (web_view->uri, g_strdup (uri));
    if (web_view->xbel_item)
    {
        const gchar* uri = midori_web_view_get_display_uri (web_view);
        katze_xbel_bookmark_set_href (web_view->xbel_item, uri);
    }
    g_object_set (web_view, "title", NULL, NULL);
}

static void
webkit_web_view_load_committed (MidoriWebView*  web_view,
                                WebKitWebFrame* web_frame)
{
    const gchar* uri;
    GdkPixbuf* icon;

    web_view->progress = 0;
    uri = webkit_web_frame_get_uri (web_frame);
    _midori_web_view_set_uri (web_view, uri);
    if (web_view->tab_icon)
    {
        icon = midori_web_view_get_icon (web_view);
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (web_view->tab_icon),
                                          icon);
        g_object_unref (icon);
    }
}

static void
webkit_web_view_load_started (MidoriWebView*  web_view,
                              WebKitWebFrame* web_frame)
{
    /* FIXME: This is a hack, until signals are fixed upstream */
    web_view->is_loading = TRUE;
    if (web_view->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (web_view->tab_icon), TRUE);

    web_view->progress = 0;
    g_signal_emit (web_view, signals[PROGRESS_STARTED], 0, web_view->progress);
}

static void
webkit_web_view_progress_changed (MidoriWebView* web_view, gint progress)
{
    web_view->progress = progress;
    g_signal_emit (web_view, signals[PROGRESS_CHANGED], 0, web_view->progress);
}

static void
webkit_web_view_load_finished (MidoriWebView* web_view)
{
    web_view->progress = 100;
    g_signal_emit (web_view, signals[PROGRESS_DONE], 0, web_view->progress);
}

static void
webkit_web_frame_load_done (WebKitWebFrame* web_frame, gboolean success,
                            MidoriWebView*  web_view)
{
    GdkPixbuf* icon;

    web_view->is_loading = FALSE;
    web_view->progress = -1;
    if (web_view->tab_icon || web_view->menu_item)
    {
        icon = midori_web_view_get_icon (web_view);

        if (web_view->tab_icon)
        {
            katze_throbber_set_animated (KATZE_THROBBER (web_view->tab_icon),
                                         FALSE);
            katze_throbber_set_static_pixbuf (KATZE_THROBBER (web_view->tab_icon),
                                              icon);
        }

        if (web_view->menu_item)
            gtk_image_menu_item_set_image (
                GTK_IMAGE_MENU_ITEM (web_view->menu_item),
                    gtk_image_new_from_pixbuf (icon));

        g_object_unref (icon);
    }
    g_signal_emit (web_view, signals[LOAD_DONE], 0, web_frame);
}

static void
webkit_web_view_title_changed (MidoriWebView*  web_view,
                               WebKitWebFrame* web_frame, const gchar* title)
{
    g_object_set (web_view, "title", title, NULL);
}

static void
webkit_web_view_statusbar_text_changed (MidoriWebView*  web_view,
                                        const gchar*    text)
{
    katze_assign (web_view->statusbar_text, g_strdup (text));
    g_object_notify (G_OBJECT (web_view), "statusbar-text");
}

static void
webkit_web_view_hovering_over_link (MidoriWebView* web_view,
                                    const gchar*   tooltip,
                                    const gchar*   link_uri)
{
    katze_assign (web_view->link_uri, g_strdup (link_uri));
    g_signal_emit (web_view, signals[ELEMENT_MOTION], 0, link_uri);
}

static gboolean
gtk_widget_button_press_event (MidoriWebView*  web_view,
                               GdkEventButton* event)
{
    GdkModifierType state = (GdkModifierType)0;
    gint x, y;
    gdk_window_get_pointer (NULL, &x, &y, &state);
    switch (event->button)
    {
    case 1:
        if (!web_view->link_uri)
            return FALSE;
        if (state & GDK_SHIFT_MASK)
        {
            /* Open link in new window */
            g_signal_emit (web_view, signals[NEW_WINDOW], 0, web_view->link_uri);
            return TRUE;
        }
        else if(state & GDK_MOD1_MASK)
        {
            /* Open link in new tab */
            g_signal_emit (web_view, signals[NEW_TAB], 0, web_view->link_uri);
            return TRUE;
        }
        break;
    case 2:
        if (state & GDK_CONTROL_MASK)
        {
            webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (web_view), 1.0);
            return FALSE; /* Allow Ctrl + Middle click */
        }
        else
        {
            if (!web_view->link_uri)
                return FALSE;
            /* Open link in new tab */
            g_signal_emit (web_view, signals[NEW_TAB], 0, web_view->link_uri);
            return TRUE;
        }
        break;
    case 3:
        return FALSE;
    }
    return FALSE;
}

static gboolean
gtk_widget_button_press_event_after (MidoriWebView*  web_view,
                                     GdkEventButton* event)
{
    GdkModifierType state;
    GtkClipboard* clipboard;
    gchar* uri;

    if (event->button == 2 && web_view->middle_click_opens_selection)
    {
        state = (GdkModifierType) event->state;
        clipboard = gtk_clipboard_get_for_display (
            gtk_widget_get_display (GTK_WIDGET (web_view)),
            GDK_SELECTION_PRIMARY);
        uri = gtk_clipboard_wait_for_text (clipboard);
        if (uri && strchr (uri, '.') && !strchr (uri, ' '))
        {
            if (state & GDK_CONTROL_MASK)
                g_signal_emit (web_view, signals[NEW_TAB], 0, uri);
            else
                g_object_set (web_view, "uri", uri, NULL);
            g_free (uri);
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
gtk_widget_scroll_event (MidoriWebView*  web_view,
                         GdkEventScroll* event)
{
    GdkModifierType state = (GdkModifierType)0;
    gint x, y;

    gdk_window_get_pointer (NULL, &x, &y, &state);
    if (state & GDK_CONTROL_MASK)
    {
        if (event->direction == GDK_SCROLL_DOWN)
            webkit_web_view_zoom_out (WEBKIT_WEB_VIEW (web_view));
        else if(event->direction == GDK_SCROLL_UP)
            webkit_web_view_zoom_in (WEBKIT_WEB_VIEW (web_view));
        return TRUE;
    }
    else
        return FALSE;
}

static void
midori_web_view_menu_new_tab_activate_cb (GtkWidget*     widget,
                                          MidoriWebView* web_view)
{
    const gchar* uri = g_object_get_data (G_OBJECT (widget), "uri");
    g_signal_emit (web_view, signals[NEW_TAB], 0, uri);
}

static void
midori_web_view_menu_new_window_activate_cb (GtkWidget*     widget,
                                             MidoriWebView* web_view)
{
    const gchar* uri = g_object_get_data (G_OBJECT (widget), "uri");
    g_signal_emit (web_view, signals[NEW_WINDOW], 0, uri);
}

static void
webkit_web_view_populate_popup_cb (GtkWidget*     web_view,
                                   GtkWidget*     menu)
{
    const gchar* uri;
    GtkWidget* menuitem;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    GtkWidget* icon;
    gchar* text;
    GList* items;

    uri = midori_web_view_get_link_uri (MIDORI_WEB_VIEW (web_view));
    if (uri)
    {
        menuitem = gtk_image_menu_item_new_with_mnemonic (
            _("Open Link in New _Tab"));
        screen = gtk_widget_get_screen (web_view);
        icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, STOCK_TAB_NEW))
        {
            icon = gtk_image_new_from_stock (STOCK_TAB_NEW, GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 1);
        g_object_set_data (G_OBJECT (menuitem), "uri", (gchar*)uri);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), web_view);
        gtk_widget_show (menuitem);
        /* hack to implement New Window */
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        menuitem = (GtkWidget*)g_list_nth_data (items, 2);
        g_object_set_data (G_OBJECT (menuitem), "uri", (gchar*)uri);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_new_window_activate_cb), web_view);
        menuitem = (GtkWidget*)g_list_nth_data (items, 3);
        /* hack to disable non-functional Download File */
        gtk_widget_set_sensitive (menuitem, FALSE);
        g_list_free (items);
    }

    if (!uri && webkit_web_view_has_selection (WEBKIT_WEB_VIEW (web_view)))
    {
        text = webkit_web_view_get_selected_text (WEBKIT_WEB_VIEW (web_view));
        if (text && strchr (text, '.') && !strchr (text, ' '))
        {
            menuitem = gtk_image_menu_item_new_with_mnemonic (
                _("Open URL in New _Tab"));
            icon = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO,
                                             GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, -1);
            g_object_set_data (G_OBJECT (menuitem), "uri", text);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), web_view);
            gtk_widget_show (menuitem);
        }
        /* text should be const, but it is allocated, so we must free it */
        g_free (text);
    }
}

static void
_midori_web_view_update_settings (MidoriWebView* web_view)
{
    g_object_get (G_OBJECT (web_view->settings),
                  "middle-click-opens-selection", &web_view->middle_click_opens_selection,
                  NULL);
}

static void
midori_web_view_settings_notify (MidoriWebSettings* web_settings,
                                 GParamSpec*        pspec,
                                 MidoriWebView*     web_view)
{
    const gchar* name = g_intern_string (pspec->name);
    GValue value = {0, };
    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (web_view->settings), name, &value);

    if (name == g_intern_string ("middle-click-opens-selection"))
        web_view->middle_click_opens_selection = g_value_get_boolean (&value);
    else if (!g_object_class_find_property (G_OBJECT_GET_CLASS (web_settings),
                                             name))
         g_warning (_("Unexpected setting '%s'"), name);
    g_value_unset (&value);
}

static void
midori_web_view_init (MidoriWebView* web_view)
{
    web_view->is_loading = FALSE;
    web_view->progress = -1;

    web_view->settings = midori_web_settings_new ();
    _midori_web_view_update_settings (web_view);
    g_object_set (web_view, "WebKitWebView::settings", web_view->settings, NULL);
    g_signal_connect (web_view->settings, "notify",
                      G_CALLBACK (midori_web_view_settings_notify), web_view);

    WebKitWebFrame* web_frame;
    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));

    g_object_connect (web_view,
                      /* "signal::load-started",
                      webkit_web_view_load_started, NULL, */
                      "signal::load-committed",
                      webkit_web_view_load_committed, NULL,
                      "signal::load-started",
                      webkit_web_view_load_started, NULL,
                      "signal::load-progress-changed",
                      webkit_web_view_progress_changed, NULL,
                      "signal::load-finished",
                      webkit_web_view_load_finished, NULL,
                      /* "signal::load-done",
                      webkit_web_view_load_done, NULL, */
                      "signal::title-changed",
                      webkit_web_view_title_changed, NULL,
                      "signal::status-bar-text-changed",
                      webkit_web_view_statusbar_text_changed, NULL,
                      "signal::hovering-over-link",
                      webkit_web_view_hovering_over_link, NULL,
                      "signal::button-press-event",
                      gtk_widget_button_press_event, NULL,
                      "signal_after::button-press-event",
                      gtk_widget_button_press_event_after, NULL,
                      "signal::scroll-event",
                      gtk_widget_scroll_event, NULL,
                      "signal::populate-popup",
                      webkit_web_view_populate_popup_cb, NULL,
                      NULL);
    g_object_connect (web_frame,
                      "signal::load-done",
                      webkit_web_frame_load_done, web_view,
                      NULL);
}

static void
midori_web_view_finalize (GObject* object)
{
    MidoriWebView* web_view = MIDORI_WEB_VIEW (object);

    g_free (web_view->uri);
    g_free (web_view->title);
    g_free (web_view->statusbar_text);
    g_free (web_view->link_uri);

    if (web_view->xbel_item)
        katze_xbel_item_unref (web_view->xbel_item);

    if (web_view->settings)
    {
        g_signal_handlers_disconnect_by_func (web_view->settings,
                                              midori_web_view_settings_notify,
                                              web_view);
        g_object_unref (web_view->settings);
    }

    G_OBJECT_CLASS (midori_web_view_parent_class)->finalize (object);
}

static void
midori_web_view_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec)
{
    MidoriWebView* web_view = MIDORI_WEB_VIEW (object);

    switch (prop_id)
    {
    case PROP_URI:
    {
        const gchar* uri = g_value_get_string (value);
        if (uri && *uri)
        {
            /* FIXME: Autocomplete the uri */
            webkit_web_view_open (WEBKIT_WEB_VIEW (web_view), uri);
        }
        break;
    }
    case PROP_TITLE:
        katze_assign (web_view->title, g_value_dup_string (value));
        const gchar* title = midori_web_view_get_display_title (web_view);
        if (web_view->tab_title)
        {
            gtk_label_set_text (GTK_LABEL (web_view->tab_title), title);
            gtk_widget_set_tooltip_text (web_view->tab_title, title);
        }
        if (web_view->menu_item)
            gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (
                                web_view->menu_item))), title);
        if (web_view->xbel_item)
            katze_xbel_item_set_title (web_view->xbel_item, title);
        break;
    case PROP_SETTINGS:
        g_signal_handlers_disconnect_by_func (web_view->settings,
                                              midori_web_view_settings_notify,
                                              web_view);
        katze_object_assign (web_view->settings, g_value_get_object (value));
        g_object_ref (web_view->settings);
        _midori_web_view_update_settings (web_view);
        g_object_set (object, "WebKitWebView::settings", web_view->settings, NULL);
        g_signal_connect (web_view->settings, "notify",
                          G_CALLBACK (midori_web_view_settings_notify), web_view);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_web_view_get_property (GObject*    object,
                              guint       prop_id,
                              GValue*     value,
                              GParamSpec* pspec)
{
    MidoriWebView* web_view = MIDORI_WEB_VIEW (object);

    switch (prop_id)
    {
    case PROP_URI:
        g_value_set_string (value, web_view->uri);
        break;
    case PROP_TITLE:
        g_value_set_string (value, web_view->title);
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, web_view->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, web_view->settings);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_web_view_new:
 *
 * Creates a new web view widget.
 *
 * Return value: a new #MidoriWebView
 **/
GtkWidget*
midori_web_view_new (void)
{
    MidoriWebView* web_view = g_object_new (MIDORI_TYPE_WEB_VIEW,
                                            NULL);

    return GTK_WIDGET (web_view);
}

/**
 * midori_web_view_set_settings:
 * @web_view: a #MidoriWebView
 * @web_settings: a #MidoriWebSettings
 *
 * Assigns a settings instance to the web view.
 **/
void
midori_web_view_set_settings (MidoriWebView*     web_view,
                              MidoriWebSettings* web_settings)
{
    g_object_set (web_view, "settings", web_settings, NULL);
}

/**
 * midori_web_view_get_proxy_menu_item:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a proxy menu item that is typically added to a Window menu
 * and which on activation switches to the right window/ tab.
 *
 * The item is created on the first call and will be updated to reflect
 * changes to the icon and title automatically.
 *
 * Note: The item is only valid as the web view is embedded in a #GtkNotebook.
 *
 * Return value: the proxy #GtkMenuItem or %NULL
 **/
GtkWidget*
midori_web_view_get_proxy_menu_item (MidoriWebView* web_view)
{
    const gchar* title;
    GtkWidget* menu_item;
    GdkPixbuf* icon;

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), FALSE);

    if (!web_view->menu_item)
    {
        title = midori_web_view_get_display_title (web_view);
        menu_item = gtk_image_menu_item_new_with_label (title);
        icon = midori_web_view_get_icon (web_view);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
            gtk_image_new_from_pixbuf (icon));
        g_object_unref (icon);

        web_view->menu_item = menu_item;
    }
    return web_view->menu_item;
}

/**
 * midori_web_view_get_proxy_tab_icon:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a proxy tab icon that is typically used in a tab label.
 *
 * The icon is created on the first call and will be updated to reflect
 * loading progress and changes of the actual icon.
 *
 * Note: If a proxy tab label has been created before, this represents
 * the existing icon used in the label.
 *
 * Return value: the proxy #GtkImage
 **/
GtkWidget*
midori_web_view_get_proxy_tab_icon (MidoriWebView* web_view)
{
    GdkPixbuf* icon;

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    if (!web_view->tab_icon)
    {
        web_view->tab_icon = katze_throbber_new ();
        icon = midori_web_view_get_icon (web_view);
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (web_view->tab_icon),
                                          icon);
        g_object_unref (icon);

        g_signal_connect (web_view->tab_icon, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &web_view->tab_icon);
    }
    return web_view->tab_icon;
}

/**
 * midori_web_view_get_proxy_tab_title:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a proxy tab title that is typically used as the label
 * of a #GtkNotebook page.
 *
 * The title is created on the first call and will be updated to
 * reflect changes automatically.
 *
 * Return value: the proxy #GtkLabel
 **/
GtkWidget*
midori_web_view_get_proxy_tab_title (MidoriWebView* web_view)
{
    const gchar* title;

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    if (!web_view->tab_title)
    {
        title = midori_web_view_get_display_title (web_view);
        web_view->tab_title = gtk_label_new (title);

        g_signal_connect (web_view->tab_title, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &web_view->tab_title);
    }
    return web_view->tab_title;
}

/**
 * midori_web_view_get_proxy_xbel_item:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a proxy xbel item that can be used for bookmark storage as
 * well as session management.
 *
 * The item is created on the first call and will be updated to reflect
 * changes to the title and href automatically.
 *
 * Note: Currently the item is always a bookmark, but this might change
 * in the future.
 *
 * Return value: the proxy #KatzeXbelItem
 **/
KatzeXbelItem*
midori_web_view_get_proxy_xbel_item (MidoriWebView* web_view)
{
    const gchar* uri;
    const gchar* title;

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    if (!web_view->xbel_item)
    {
        web_view->xbel_item = katze_xbel_bookmark_new ();
        uri = midori_web_view_get_display_uri (web_view);
        katze_xbel_bookmark_set_href (web_view->xbel_item, uri);
        title = midori_web_view_get_display_title (web_view);
        katze_xbel_item_set_title (web_view->xbel_item, title);
    }
    return web_view->xbel_item;
}

/**
 * midori_web_view_is_loading:
 * @web_view: a #MidoriWebView
 *
 * Determines whether currently a page is being loaded or not.
 *
 * Return value: %TRUE if a page is being loaded, %FALSE otherwise
 **/
gint
midori_web_view_is_loading (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), -1);

    return web_view->is_loading;
}

/**
 * midori_web_view_get_progress:
 * @web_view: a #MidoriWebView
 *
 * Retrieves the current loading progress in percent or -1 if no data
 * has been loaded so far.
 *
 * The value is undefined if no loading is in progress.
 *
 * Return value: the current loading progress or -1
 **/
gint
midori_web_view_get_progress (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), -1);

    return web_view->progress;
}

/**
 * midori_web_view_get_display_uri:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a string that is suitable for displaying, particularly an
 * empty URI is represented as "".
 *
 * You can assume that the string is not %NULL.
 *
 * Return value: an URI string
 **/
const gchar*
midori_web_view_get_display_uri (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), "");

    return web_view->uri ? web_view->uri : "";
}

/**
 * midori_web_view_get_display_title:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a string that is suitable for displaying as a title. Most of the
 * time this will be the title or the current URI.
 *
 * You can assume that the string is not %NULL.
 *
 * Return value: a title string
 **/
const gchar*
midori_web_view_get_display_title (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), "about:blank");

    if (web_view->title)
        return web_view->title;
    if (web_view->uri)
        return web_view->uri;
    return "about:blank";
}

/**
 * midori_web_view_get_link_uri:
 * @web_view: a #MidoriWebView
 *
 * Retrieves the uri of the currently focused link, particularly while the
 * mouse hovers a link or a context menu is being opened.
 *
 * Return value: an URI string, or %NULL if there is no link focussed
 **/
const gchar*
midori_web_view_get_link_uri (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    return web_view->link_uri;
}

/**
 * midori_web_view_get_icon:
 * @web_view: a #MidoriWebView
 *
 * Retrieves an icon associated with the currently loaded URI. If no
 * icon is available a default icon is used.
 *
 * The pixbuf is newly allocated and should be unreffed after use.
 *
 * Return value: a #GdkPixbuf
 **/
GdkPixbuf*
midori_web_view_get_icon (MidoriWebView* web_view)
{
    #if GLIB_CHECK_VERSION (2, 16, 0)
    GFile* file;
    GFile* parent;
    GFile* icon_file;
    GFileInfo* info;
    const gchar* content_type;
    GIcon* icon;
    GInputStream* stream;
    #endif
    GdkPixbuf* pixbuf;

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    #if GLIB_CHECK_VERSION (2, 16, 0)
    parent = g_file_new_for_uri (web_view->uri ? web_view->uri : "");
    icon = NULL;
    do
    {
        file = parent;
        icon_file = g_file_get_child (file, "favicon.ico");
        info = g_file_query_info (icon_file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                  G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (info)
        {
            content_type = g_file_info_get_content_type (info);
            /* favicon.ico can be image/x-ico or image/x-icon */
            icon = !strcmp (content_type, "image/x-ico")
                ? g_file_icon_new (icon_file) : NULL;
        }

        parent = g_file_get_parent (file);
    }
    while (!icon && parent);

    if (icon && (stream = g_loadable_icon_load (G_LOADABLE_ICON (icon),
                                                GTK_ICON_SIZE_MENU,
                                                NULL, NULL, NULL)))
    {
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (stream);
    }
    else
    #endif
        pixbuf = gtk_widget_render_icon (GTK_WIDGET (web_view),
            GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    #if GLIB_CHECK_VERSION (2, 16, 0)
    if (icon)
        g_object_unref (icon);
    g_object_unref (icon_file);
    g_object_unref (file);
    #endif
    return pixbuf;
}
