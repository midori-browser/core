/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-webview.h"

#include "global.h"
#include "sokoke.h"

#include <webkit/webkitwebframe.h>
#include <string.h>

// This is unstable API, so we need to declare it
gchar*
webkit_web_view_get_selected_text (WebKitWebView* web_view);

G_DEFINE_TYPE (MidoriWebView, midori_web_view, WEBKIT_TYPE_WEB_VIEW)

struct _MidoriWebViewPrivate
{
    GtkWidget* tab_icon;
    GtkWidget* tab_label;
    GtkWidget* tab_close;
    GdkPixbuf* icon;
    gchar* uri;
    gchar* title;
    gboolean is_loading;
    gint progress;
    gchar* statusbar_text;
    gchar* link_uri;

    gint tab_label_size;
    gboolean close_button;
    gboolean middle_click_goto;
    MidoriWebSettings* settings;

    GtkWidget* proxy_menu_item;
    GtkWidget* proxy_tab_label;
    KatzeXbelItem* proxy_xbel_item;
};

#define MIDORI_WEB_VIEW_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
     MIDORI_TYPE_WEB_VIEW, MidoriWebViewPrivate))

enum
{
    PROP_0,

    PROP_ICON,
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
    CLOSE,
    NEW_TAB,
    NEW_WINDOW,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_web_view_finalize (GObject* object);

static void
midori_web_view_set_property (GObject* object,
                              guint prop_id,
                              const GValue* value,
                              GParamSpec* pspec);

static void
midori_web_view_get_property (GObject* object,
                              guint prop_id,
                              GValue* value,
                              GParamSpec* pspec);

/*static WebKitWebView*
midori_web_view_create_web_view (WebKitWebView* web_view)
{
    MidoriWebView* new_web_view = NULL;
    g_signal_emit (web_view, signals[NEW_WINDOW], 0, &new_web_view);
    if (new_web_view)
        return WEBKIT_WEB_VIEW (new_web_view);
    return WEBKIT_WEB_VIEW (midori_web_view_new ());
}*/

static void
midori_web_view_class_init (MidoriWebViewClass* class)
{
    signals[PROGRESS_STARTED] = g_signal_new (
        "progress-started",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, progress_started),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    signals[PROGRESS_CHANGED] = g_signal_new (
        "progress-changed",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, progress_changed),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    signals[PROGRESS_DONE] = g_signal_new (
        "progress-done",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, progress_done),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    signals[LOAD_DONE] = g_signal_new (
        "load-done",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, load_done),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        WEBKIT_TYPE_WEB_FRAME);

    signals[ELEMENT_MOTION] = g_signal_new(
        "element-motion",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, element_motion),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[CLOSE] = g_signal_new(
        "close",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, close),
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    signals[NEW_TAB] = g_signal_new(
        "new-tab",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, new_tab),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[NEW_WINDOW] = g_signal_new(
        "new-window",
        G_TYPE_FROM_CLASS(class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriWebViewClass, new_window),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    /*WEBKIT_WEB_VIEW_CLASS (class)->create_web_view = g_signal_new ("create-web-view",
            G_TYPE_FROM_CLASS(class),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(MidoriWebViewClass, create_web_view),
            0,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            MIDORI_TYPE_WEB_VIEW);*/

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_web_view_finalize;
    gobject_class->set_property = midori_web_view_set_property;
    gobject_class->get_property = midori_web_view_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_ICON,
                                     g_param_spec_object (
                                     "icon",
                                     "Icon",
                                     _("The icon of the currently loaded page"),
                                     GDK_TYPE_PIXBUF,
                                     G_PARAM_READWRITE));

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
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     _("The associated settings"),
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE));

    g_type_class_add_private (class, sizeof (MidoriWebViewPrivate));
}

/*static void
webkit_web_view_load_started (MidoriWebView* web_view,
                              WebKitWebFrame* web_frame)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    priv->is_loading = TRUE;
    priv->progress = -1;
    katze_throbber_set_animated(KATZE_THROBBER(priv->tab_icon), TRUE);
}*/

static void
_midori_web_view_set_uri (MidoriWebView* web_view,
                          const gchar*   uri)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    katze_assign (priv->uri, g_strdup (uri));
    if (priv->proxy_xbel_item)
    {
        const gchar* uri = midori_web_view_get_display_uri (web_view);
        katze_xbel_bookmark_set_href (priv->proxy_xbel_item, uri);
    }
    g_object_set (web_view, "title", NULL, NULL);
}

static void
webkit_web_view_load_committed (MidoriWebView*  web_view,
                                WebKitWebFrame* web_frame)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    priv->progress = 0;
    const gchar* uri = webkit_web_frame_get_uri (web_frame);
    _midori_web_view_set_uri (web_view, uri);
}

static void
webkit_web_view_load_started (MidoriWebView*  web_view,
                              WebKitWebFrame* web_frame)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    // FIXME: This is a hack, until signals are fixed upstream
    priv->is_loading = TRUE;
    if (priv->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (priv->tab_icon), TRUE);

    priv->progress = 0;
    g_signal_emit (web_view, signals[PROGRESS_STARTED], 0, priv->progress);
}

static void
webkit_web_view_progress_changed (MidoriWebView* web_view, gint progress)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    priv->progress = progress;
    g_signal_emit (web_view, signals[PROGRESS_CHANGED], 0, priv->progress);
}

static void
webkit_web_view_load_finished (MidoriWebView* web_view)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    priv->progress = 100;
    g_signal_emit (web_view, signals[PROGRESS_DONE], 0, priv->progress);
}

static void
webkit_web_frame_load_done (WebKitWebFrame* web_frame, gboolean success,
                            MidoriWebView*  web_view)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    priv->is_loading = FALSE;
    priv->progress = -1;
    if (priv->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (priv->tab_icon), FALSE);
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
    g_object_set (web_view, "statusbar-text", text, NULL);
}

static void
webkit_web_view_hovering_over_link (MidoriWebView* web_view,
                                    const gchar*   tooltip,
                                    const gchar*   link_uri)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    katze_assign (priv->link_uri, g_strdup (link_uri));
    g_signal_emit (web_view, signals[ELEMENT_MOTION], 0, link_uri);
}

static gboolean
gtk_widget_button_press_event (MidoriWebView*  web_view,
                               GdkEventButton* event)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    GdkModifierType state = (GdkModifierType)0;
    gint x, y;
    gdk_window_get_pointer (NULL, &x, &y, &state);
    switch (event->button)
    {
    case 1:
        if (!priv->link_uri)
            return FALSE;
        if (state & GDK_SHIFT_MASK)
        {
            // Open link in new window
            g_signal_emit (web_view, signals[NEW_WINDOW], 0, priv->link_uri);
            return TRUE;
        }
        else if(state & GDK_MOD1_MASK)
        {
            // Open link in new tab
            g_signal_emit (web_view, signals[NEW_TAB], 0, priv->link_uri);
            return TRUE;
        }
        break;
    case 2:
        if (state & GDK_CONTROL_MASK)
        {
            // FIXME: Reset font multiplier or zoom level
            return FALSE; // Allow Ctrl + Middle click
        }
        else
        {
            if (!priv->link_uri)
                return FALSE;
            // Open link in new tab
            g_signal_emit (web_view, signals[NEW_TAB], 0, priv->link_uri);
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
    MidoriWebViewPrivate* priv = web_view->priv;

    if (event->button == 2 && priv->middle_click_goto)
    {
        GdkModifierType state = (GdkModifierType) event->state;
        GtkClipboard* clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
        gchar* uri = gtk_clipboard_wait_for_text (clipboard);
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
        // FIXME: Increase or decrease the font multiplier or zoom level
        if (event->direction == GDK_SCROLL_DOWN)
            ;
        else if(event->direction == GDK_SCROLL_UP)
            ;
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
webkit_web_view_populate_popup_cb (GtkWidget*     web_view,
                                   GtkWidget*     menu)
{
    const gchar* uri = midori_web_view_get_link_uri (MIDORI_WEB_VIEW (web_view));
    if (uri)
    {
        GtkWidget* menuitem = gtk_image_menu_item_new_with_mnemonic (
            _("Open Link in New _Tab"));
        GdkScreen* screen = gtk_widget_get_screen (web_view);
        GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, STOCK_TAB_NEW))
        {
            GtkWidget* icon = gtk_image_new_from_stock (STOCK_TAB_NEW,
                                                        GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 1);
        g_object_set_data (G_OBJECT (menuitem), "uri", (gchar*)uri);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), web_view);
        gtk_widget_show (menuitem);
    }

    if (!uri && webkit_web_view_has_selection (WEBKIT_WEB_VIEW (web_view)))
    {
        gchar* text = webkit_web_view_get_selected_text (
            WEBKIT_WEB_VIEW (web_view));
        if (text && strchr (text, '.') && !strchr (text, ' '))
        {
            GtkWidget* menuitem = gtk_image_menu_item_new_with_mnemonic (
                _("Open URL in New _Tab"));
            GtkWidget* icon = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO,
                                                        GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, -1);
            g_object_set_data (G_OBJECT (menuitem), "uri", text);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), web_view);
            gtk_widget_show (menuitem);
        }
        // FIXME: We are leaking 'text' which is not const but should be.
    }
}

static void
_midori_web_view_update_tab_label_size (MidoriWebView* web_view)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    if (priv->tab_label)
    {
        if (priv->tab_label_size > -1)
        {
            gint width, height;
            sokoke_widget_get_text_size (priv->tab_label, "M",
                                         &width, &height);
            gtk_widget_set_size_request (priv->tab_label,
                                         width * priv->tab_label_size, -1);
            gtk_label_set_ellipsize (GTK_LABEL (priv->tab_label),
                                     PANGO_ELLIPSIZE_END);
        }
        else
        {
            gtk_widget_set_size_request (priv->tab_label, -1, -1);
            gtk_label_set_ellipsize (GTK_LABEL (priv->tab_label),
                                     PANGO_ELLIPSIZE_NONE);
        }
    }
}

static void
_midori_web_view_update_settings (MidoriWebView* web_view)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    g_object_get (G_OBJECT (priv->settings),
                  "tab-label-size", &priv->tab_label_size,
                  "close-buttons-on-tabs", &priv->close_button,
                  "middle-click-opens-selection", &priv->middle_click_goto,
                  NULL);
}

static void
midori_web_view_settings_notify (MidoriWebSettings* web_settings,
                                 GParamSpec*        pspec,
                                 MidoriWebView*     web_view)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    const gchar* name = g_intern_string (pspec->name);
    GValue value = {0, };
    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (priv->settings), name, &value);

    if (name == g_intern_string ("tab-label-size"))
    {
        priv->tab_label_size = g_value_get_int (&value);
        _midori_web_view_update_tab_label_size (web_view);
    }
    else if (name == g_intern_string ("close-buttons-on-tabs"))
    {
        priv->close_button = g_value_get_boolean (&value);
        if (priv->tab_close)
            sokoke_widget_set_visible (priv->tab_close, priv->close_button);
    }
    else if (name == g_intern_string ("middle-click-opens-selection"))
        priv->middle_click_goto = g_value_get_boolean (&value);
    else if (!g_object_class_find_property (G_OBJECT_GET_CLASS (web_settings),
                                            name))
        g_warning("Unexpected setting '%s'", name);
    g_value_unset(&value);
}

static void
midori_web_view_init (MidoriWebView* web_view)
{
    web_view->priv = MIDORI_WEB_VIEW_GET_PRIVATE (web_view);

    MidoriWebViewPrivate* priv = web_view->priv;
    priv->is_loading = FALSE;
    priv->progress = -1;

    priv->settings = midori_web_settings_new ();
    _midori_web_view_update_settings (web_view);
    g_signal_connect (priv->settings, "notify",
                      G_CALLBACK(midori_web_view_settings_notify), web_view);

    WebKitWebFrame* web_frame;
    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));

    g_object_connect (web_view,
                      //"signal::load-started",
                      //webkit_web_view_load_started, NULL,
                      "signal::load-committed",
                      webkit_web_view_load_committed, NULL,
                      "signal::load-started",
                      webkit_web_view_load_started, NULL,
                      "signal::load-progress-changed",
                      webkit_web_view_progress_changed, NULL,
                      "signal::load-finished",
                      webkit_web_view_load_finished, NULL,
                      //"signal::load-done",
                      //webkit_web_view_load_done, NULL,
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
    MidoriWebViewPrivate* priv = web_view->priv;

    if (priv->icon)
        g_object_unref (priv->icon);
    g_free (priv->uri);
    g_free (priv->title);
    g_free (priv->statusbar_text);
    g_free (priv->link_uri);

    if (priv->proxy_menu_item)
        gtk_widget_destroy (priv->proxy_menu_item);
    if (priv->proxy_xbel_item)
        katze_xbel_item_unref (priv->proxy_xbel_item);

    if (priv->settings)
        g_object_unref (priv->settings);

    G_OBJECT_CLASS (midori_web_view_parent_class)->finalize (object);
}

static void
midori_web_view_set_property (GObject*      object,
                              guint         prop_id,
                              const GValue* value,
                              GParamSpec*   pspec)
{
    MidoriWebView* web_view = MIDORI_WEB_VIEW (object);
    MidoriWebViewPrivate* priv = web_view->priv;

    switch (prop_id)
    {
    case PROP_ICON:
        katze_object_assign (priv->icon, g_value_get_object (value));
        g_object_ref (priv->icon);
        if (priv->tab_icon)
            katze_throbber_set_static_pixbuf (KATZE_THROBBER (priv->tab_icon),
                                              priv->icon);
        break;
    case PROP_URI:
    {
        const gchar* uri = g_value_get_string (value);
        if (uri && *uri)
        {
            // FIXME: Autocomplete the uri
            webkit_web_view_open (WEBKIT_WEB_VIEW (web_view), uri);
        }
        break;
    }
    case PROP_TITLE:
        katze_assign (priv->title, g_value_dup_string (value));
        const gchar* title = midori_web_view_get_display_title (web_view);
        if (priv->tab_label)
        {
            gtk_label_set_text (GTK_LABEL (priv->tab_label), title);
            sokoke_widget_set_tooltip_text (priv->tab_label, title);
        }
        if (priv->proxy_menu_item)
            gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (
                                priv->proxy_menu_item))), title);
        if (priv->proxy_xbel_item)
            katze_xbel_item_set_title (priv->proxy_xbel_item, title);
        break;
    case PROP_STATUSBAR_TEXT:
        katze_assign (priv->statusbar_text, g_value_dup_string (value));
        break;
    case PROP_SETTINGS:
        g_signal_handlers_disconnect_by_func (priv->settings,
                                              midori_web_view_settings_notify,
                                              web_view);
        katze_object_assign (priv->settings, g_value_get_object (value));
        g_object_ref (priv->settings);
        _midori_web_view_update_settings (web_view);
        g_signal_connect (priv->settings, "notify",
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
    MidoriWebViewPrivate* priv = web_view->priv;

    switch (prop_id)
    {
    case PROP_ICON:
        g_value_set_object (value, priv->icon);
        break;
    case PROP_URI:
        g_value_set_string (value, priv->uri);
        break;
    case PROP_TITLE:
        g_value_set_string (value, priv->title);
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, priv->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, priv->settings);
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
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), FALSE);

    MidoriWebViewPrivate* priv = web_view->priv;

    if (!priv->proxy_menu_item)
    {
        const gchar* title = midori_web_view_get_display_title (web_view);
        GtkWidget* menu_item = gtk_image_menu_item_new_with_label (title);
        GtkWidget* icon = gtk_image_new_from_stock (GTK_STOCK_FILE,
                                                    GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), icon);

        priv->proxy_menu_item = menu_item;
    }
    return priv->proxy_menu_item;
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
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    MidoriWebViewPrivate* priv = web_view->priv;

    if (!priv->tab_icon)
    {
        priv->tab_icon = katze_throbber_new ();
        if (priv->icon)
            katze_throbber_set_static_pixbuf (KATZE_THROBBER(priv->tab_icon),
                                              priv->icon);
        else
            katze_throbber_set_static_stock_id (KATZE_THROBBER(priv->tab_icon),
                                                GTK_STOCK_FILE);
    }
    return priv->tab_icon;
}

static gboolean
midori_web_view_tab_label_button_release_event (GtkWidget* tab_label,
                                                GdkEventButton* event,
                                                MidoriWebView* web_view)
{
    MidoriWebViewPrivate* priv = web_view->priv;

    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
        // Toggle the label visibility on double click
        GtkWidget* child = gtk_bin_get_child (GTK_BIN (tab_label));
        GList* children = gtk_container_get_children (GTK_CONTAINER (child));
        child = (GtkWidget*)g_list_nth_data (children, 1);
        gboolean visible = gtk_widget_get_child_visible (GTK_WIDGET (child));
        gtk_widget_set_child_visible (GTK_WIDGET (child), !visible);
        gint width, height;
        sokoke_widget_get_text_size(tab_label, "M", &width, &height);
        gtk_widget_set_size_request (child, !visible
         ? width * priv->tab_label_size : 0, !visible ? -1 : 0);
        g_list_free (children);
        return TRUE;
    }
    else if (event->button == 2)
    {
        // Close the web view on middle click
        g_signal_emit (web_view, signals[CLOSE], 0);
        return TRUE;
    }

    return FALSE;
}

static void
midori_web_view_tab_close_style_set (GtkWidget*     tab_close,
                                     GtkStyle*      previous_style,
                                     MidoriWebView* web_view)
{
    GtkSettings* gtk_settings = gtk_widget_get_settings (tab_close);
    gint width, height;
    gtk_icon_size_lookup_for_settings (gtk_settings, GTK_ICON_SIZE_BUTTON,
                                       &width, &height);
    gtk_widget_set_size_request (tab_close, width + 2, height + 2);
}

static void
midori_web_view_tab_close_clicked (GtkWidget*     tab_close,
                                   MidoriWebView* web_view)
{
    g_signal_emit (web_view, signals[CLOSE], 0);
}

/**
 * midori_web_view_get_proxy_tab_label:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a proxy tab label that is typically used as the label of
 * a #GtkNotebook page.
 *
 * The label is created on the first call and will be updated to reflect
 * changes to the icon and title automatically.
 *
 * The icon embedded in the label will reflect the loading status of the
 * web view.
 *
 * Note: This fails if a proxy tab icon has been created already.
 *
 * Return value: the proxy #GtkEventBox
 **/
GtkWidget*
midori_web_view_get_proxy_tab_label (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    MidoriWebViewPrivate* priv = web_view->priv;

    GtkWidget* proxy_tab_icon = priv->tab_icon;
    g_return_val_if_fail (!proxy_tab_icon, NULL);

    if (!priv->proxy_tab_label)
    {
        priv->tab_icon = midori_web_view_get_proxy_tab_icon (web_view);

        GtkWidget* event_box = gtk_event_box_new ();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX (event_box), FALSE);
        GtkWidget* hbox = gtk_hbox_new (FALSE, 1);
        gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (hbox));
        gtk_box_pack_start (GTK_BOX (hbox), priv->tab_icon, FALSE, FALSE, 0);
        const gchar* title = midori_web_view_get_display_title (web_view);
        priv->tab_label = gtk_label_new (title);
        gtk_misc_set_alignment (GTK_MISC (priv->tab_label), 0.0, 0.5);
        // TODO: make the tab initially look "unvisited" until it's focused
        gtk_box_pack_start (GTK_BOX (hbox), priv->tab_label, FALSE, TRUE, 0);
        priv->proxy_tab_label = event_box;
        _midori_web_view_update_tab_label_size (web_view);

        GtkWidget* close_button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
        gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
        GtkRcStyle* rcstyle = gtk_rc_style_new ();
        rcstyle->xthickness = rcstyle->ythickness = 0;
        gtk_widget_modify_style(close_button, rcstyle);
        GtkWidget* image = gtk_image_new_from_stock (GTK_STOCK_CLOSE,
                                                     GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON(close_button), image);
        gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
        gtk_widget_show_all (GTK_WIDGET (event_box));
        if (!priv->close_button)
            gtk_widget_hide (close_button);
        priv->tab_close = close_button;

        g_signal_connect(priv->proxy_tab_label, "button-release-event",
                         G_CALLBACK(midori_web_view_tab_label_button_release_event),
                         web_view);
        g_signal_connect(priv->tab_close, "style-set",
                         G_CALLBACK(midori_web_view_tab_close_style_set),
                         web_view);
        g_signal_connect(priv->tab_close, "clicked",
                         G_CALLBACK(midori_web_view_tab_close_clicked),
                         web_view);
    }
    return priv->proxy_tab_label;
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
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    MidoriWebViewPrivate* priv = web_view->priv;

    if (!priv->proxy_xbel_item)
    {
        priv->proxy_xbel_item = katze_xbel_bookmark_new ();
        const gchar* uri = midori_web_view_get_display_uri (web_view);
        katze_xbel_bookmark_set_href (priv->proxy_xbel_item, uri);
        const gchar* title = midori_web_view_get_display_title (web_view);
        katze_xbel_item_set_title (priv->proxy_xbel_item, title);
    }
    return priv->proxy_xbel_item;
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

    MidoriWebViewPrivate* priv = web_view->priv;
    return priv->is_loading;
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

    MidoriWebViewPrivate* priv = web_view->priv;
    return priv->progress;
}

/**
 * midori_web_view_get_uri:
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

    MidoriWebViewPrivate* priv = web_view->priv;
    return priv->uri ? priv->uri : "";
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

    MidoriWebViewPrivate* priv = web_view->priv;

    if (priv->title)
        return priv->title;
    if (priv->uri)
        return priv->uri;
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

    MidoriWebViewPrivate* priv = web_view->priv;
    return priv->link_uri;
}
