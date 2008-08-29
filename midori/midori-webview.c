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

#include "midori-webview.h"

#include "main.h"
#include "gjs.h"
#include "sokoke.h"
#include "compat.h"

#if HAVE_GIO
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
    gboolean window_object_cleared;

    GdkPixbuf* icon;
    gchar* uri;
    gchar* title;
    gdouble progress;
    MidoriLoadStatus load_status;
    gchar* statusbar_text;
    gchar* link_uri;
    KatzeArray* news_feeds;

    MidoriWebSettings* settings;

    GtkWidget* menu_item;
    GtkWidget* tab_icon;
    GtkWidget* tab_title;
    KatzeXbelItem* xbel_item;
};

G_DEFINE_TYPE (MidoriWebView, midori_web_view, WEBKIT_TYPE_WEB_VIEW)

GType
midori_load_status_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_LOAD_PROVISIONAL, "MIDORI_LOAD_PROVISIONAL", N_("Load Provisional") },
         { MIDORI_LOAD_COMMITTED, "MIDORI_LOAD_COMMITTED", N_("Load Committed") },
         { MIDORI_LOAD_FINISHED, "MIDORI_LOAD_FINISHED", N_("Load Finished") },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriLoadStatus", values);
    }
    return type;
}

enum
{
    PROP_0,

    PROP_URI,
    PROP_TITLE,
    PROP_PROGRESS,
    PROP_MLOAD_STATUS,
    PROP_STATUSBAR_TEXT,
    PROP_SETTINGS
};

enum {
    ICON_READY,
    NEWS_FEED_READY,
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
midori_cclosure_marshal_VOID__STRING_STRING_STRING (GClosure*     closure,
                                                    GValue*       return_value,
                                                    guint         n_param_values,
                                                    const GValue* param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data)
{
    typedef void(*GMarshalFunc_VOID__STRING_STRING_STRING) (gpointer      data1,
                                                            const gchar*  arg_1,
                                                            const gchar*  arg_2,
                                                            const gchar*  arg_3,
                                                            gpointer      data2);
    register GMarshalFunc_VOID__STRING_STRING_STRING callback;
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
    callback = (GMarshalFunc_VOID__STRING_STRING_STRING) (marshal_data
        ? marshal_data : cc->callback);
    callback (data1,
              g_value_get_string (param_values + 1),
              g_value_get_string (param_values + 2),
              g_value_get_string (param_values + 3),
              data2);
}

static void
midori_web_view_class_init (MidoriWebViewClass* class)
{
    signals[ICON_READY] = g_signal_new (
        "icon-ready",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, icon_ready),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GDK_TYPE_PIXBUF);

    signals[NEWS_FEED_READY] = g_signal_new (
        "news-feed-ready",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriWebViewClass, news_feed_ready),
        0,
        NULL,
        midori_cclosure_marshal_VOID__STRING_STRING_STRING,
        G_TYPE_NONE, 3,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING);

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

    if (!g_object_class_find_property (gobject_class, "uri"))
    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     "Uri",
                                     _("The URI of the currently loaded page"),
                                     "",
                                     flags));

    if (!g_object_class_find_property (gobject_class, "title"))
    g_object_class_install_property (gobject_class,
                                     PROP_TITLE,
                                     g_param_spec_string (
                                     "title",
                                     "Title",
                                     _("The title of the currently loaded page"),
                                     NULL,
                                     flags));

    if (!g_object_class_find_property (gobject_class, "progress"))
    g_object_class_install_property (gobject_class,
                                     PROP_PROGRESS,
                                     g_param_spec_double (
                                     "progress",
                                     "Progress",
                                     _("The current loading progress"),
                                     0.0, 1.0, 0.0,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_MLOAD_STATUS,
                                     g_param_spec_enum (
                                     "mload-status",
                                     "Load Status",
                                     _("The current loading status"),
                                     MIDORI_TYPE_LOAD_STATUS,
                                     MIDORI_LOAD_FINISHED,
                                     G_PARAM_READABLE));

    if (!g_object_class_find_property (gobject_class, "statusbar-text"))
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

static void
webkit_web_view_load_started (MidoriWebView*  web_view,
                              WebKitWebFrame* web_frame)
{
    web_view->window_object_cleared = FALSE;

    web_view->load_status = MIDORI_LOAD_PROVISIONAL;
    g_object_notify (G_OBJECT (web_view), "mload-status");
    if (web_view->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (web_view->tab_icon), TRUE);

    web_view->progress = 0.0;
    g_object_notify (G_OBJECT (web_view), "progress");
}

static void
webkit_web_view_window_object_cleared_cb (MidoriWebView*     web_view,
                                          WebKitWebFrame*    web_frame,
                                          JSGlobalContextRef js_context,
                                          JSObjectRef        js_window)
{
    web_view->window_object_cleared = TRUE;
}

#if HAVE_GIO
void
loadable_icon_finish_cb (GdkPixbuf*     icon,
                         GAsyncResult*  res,
                         MidoriWebView* web_view)
{
    GInputStream* stream;
    GdkPixbuf* pixbuf;
    GdkPixbuf* pixbuf_scaled;
    gint icon_width, icon_height;

    pixbuf = NULL;
    stream = g_loadable_icon_load_finish (G_LOADABLE_ICON (icon),
                                          res, NULL, NULL);
    if (stream)
    {
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (stream);
    }
    if (!pixbuf)
        pixbuf = gtk_widget_render_icon (GTK_WIDGET (web_view),
            GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);

    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    web_view->icon = pixbuf_scaled;
    g_signal_emit (web_view, signals[ICON_READY], 0, web_view->icon);
}

void
file_info_finish_cb (GFile*         icon_file,
                     GAsyncResult*  res,
                     MidoriWebView* web_view)
{
    GFileInfo* info;
    const gchar* content_type;
    GIcon* icon;
    GFile* parent;
    GFile* file;
    GdkPixbuf* pixbuf;
    gint icon_width, icon_height;
    GdkPixbuf* pixbuf_scaled;

    info = g_file_query_info_finish (G_FILE (icon_file), res, NULL);
    if (info)
    {
        content_type = g_file_info_get_content_type (info);
        if (g_str_has_prefix (content_type, "image/"))
        {
            icon = g_file_icon_new (icon_file);
            g_loadable_icon_load_async (G_LOADABLE_ICON (icon),
                0, NULL, (GAsyncReadyCallback)loadable_icon_finish_cb, web_view);
            return;
        }
    }

    file = g_file_get_parent (icon_file);
    parent = g_file_get_parent (file);
    /* We need to check if file equals the parent due to a GIO bug */
    if (parent && !g_file_equal (file, parent))
    {
        icon_file = g_file_get_child (parent, "favicon.ico");
        g_file_query_info_async (icon_file,
            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
            G_FILE_QUERY_INFO_NONE, 0, NULL,
            (GAsyncReadyCallback)file_info_finish_cb, web_view);
        return;
    }

    pixbuf = gtk_widget_render_icon (GTK_WIDGET (web_view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    web_view->icon = pixbuf_scaled;
    g_signal_emit (web_view, signals[ICON_READY], 0, web_view->icon);
}
#endif

static void
_midori_web_view_load_icon (MidoriWebView* web_view)
{
    #if HAVE_GIO
    GFile* file;
    GFile* icon_file;
    #endif
    GdkPixbuf* pixbuf;
    gint icon_width, icon_height;
    GdkPixbuf* pixbuf_scaled;

    #if HAVE_GIO
    if (web_view->uri)
    {
        file = g_file_new_for_uri (web_view->uri);
        icon_file = g_file_get_child (file, "favicon.ico");
        g_file_query_info_async (icon_file,
            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
            G_FILE_QUERY_INFO_NONE, 0, NULL,
            (GAsyncReadyCallback)file_info_finish_cb, web_view);
        return;
    }
    #endif

    pixbuf = gtk_widget_render_icon (GTK_WIDGET (web_view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    web_view->icon = pixbuf_scaled;
    g_signal_emit (web_view, signals[ICON_READY], 0, web_view->icon);
}

static void
webkit_web_view_load_committed (MidoriWebView*  web_view,
                                WebKitWebFrame* web_frame)
{
    const gchar* uri;
    GdkPixbuf* icon;

    uri = webkit_web_frame_get_uri (web_frame);
    katze_assign (web_view->uri, g_strdup (uri));
    if (web_view->xbel_item)
    {
        uri = midori_web_view_get_display_uri (web_view);
        katze_xbel_bookmark_set_href (web_view->xbel_item, uri);
    }
    g_object_notify (G_OBJECT (web_view), "uri");
    g_object_set (web_view, "title", NULL, NULL);

    icon = gtk_widget_render_icon (GTK_WIDGET (web_view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    katze_object_assign (web_view->icon, icon);
    _midori_web_view_load_icon (web_view);

    if (web_view->tab_icon)
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (web_view->tab_icon),
                                          icon);

    if (web_view->menu_item)
        gtk_image_menu_item_set_image (
            GTK_IMAGE_MENU_ITEM (web_view->menu_item),
                gtk_image_new_from_pixbuf (icon));

    web_view->load_status = MIDORI_LOAD_COMMITTED;
    g_object_notify (G_OBJECT (web_view), "mload-status");
}

static void
webkit_web_view_icon_ready (MidoriWebView* web_view,
                            GdkPixbuf*     icon)
{
    if (web_view->tab_icon)
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (web_view->tab_icon),
                                          icon);
    if (web_view->menu_item)
            gtk_image_menu_item_set_image (
                GTK_IMAGE_MENU_ITEM (web_view->menu_item),
                    gtk_image_new_from_pixbuf (icon));
}

static void
webkit_web_view_progress_changed (MidoriWebView* web_view, gint progress)
{
    web_view->progress = progress ? progress / 100.0 : 0.0;
    g_object_notify (G_OBJECT (web_view), "progress");
}

static void
gjs_value_links_foreach_cb (GjsValue*      link,
                            MidoriWebView* web_view)
{
    const gchar* type;
#if HAVE_GIO
    const gchar* rel;
    GFile* icon_file;
    GIcon* icon;
#endif

    if (gjs_value_is_object (link) && gjs_value_has_attribute (link, "href"))
    {
        if (gjs_value_has_attribute (link, "type"))
        {
            type = gjs_value_get_attribute_string (link, "type");
            if (!strcmp (type, "application/rss+xml")
                || !strcmp (type, "application/x.atom+xml")
                || !strcmp (type, "application/atom+xml"))
            {
                katze_array_add_item (web_view->news_feeds, link);
                g_signal_emit (web_view, signals[NEWS_FEED_READY], 0,
                    gjs_value_get_attribute_string (link, "href"), type,
                    gjs_value_has_attribute (link, "title")
                    ? gjs_value_get_attribute_string (link, "title") : NULL);
            }
        }
#if HAVE_GIO
        if (gjs_value_has_attribute (link, "rel"))
        {
            rel = gjs_value_get_attribute_string (link, "rel");
            if (!strcmp (rel, "icon") || !strcmp (rel, "shortcut icon"))
            {
                icon_file = g_file_new_for_uri (
                    gjs_value_get_attribute_string (link, "href"));
                icon = g_file_icon_new (icon_file);
                g_loadable_icon_load_async (G_LOADABLE_ICON (icon),
                    0, NULL, (GAsyncReadyCallback)loadable_icon_finish_cb, web_view);
            }
        }
#endif
    }
}

static void
webkit_web_frame_load_done (WebKitWebFrame* web_frame,
                            gboolean        success,
                            MidoriWebView*  web_view)
{
    JSContextRef js_context;
    JSValueRef js_window;
    GjsValue* value;
    GjsValue* document;
    GjsValue* links;

    /* If WebKit didn't emit the signal due to a bug, we will */
    if (!web_view->window_object_cleared)
    {
        js_context = webkit_web_frame_get_global_context (web_frame);
        js_window = JSContextGetGlobalObject (js_context);
        g_signal_emit_by_name (web_view, "window-object-cleared",
            web_frame, js_context, js_window);
    }

    value = gjs_value_new (webkit_web_frame_get_global_context (web_frame), NULL);
    document = gjs_value_get_by_name (value, "document");
    links = gjs_value_get_elements_by_tag_name (document, "link");
    katze_array_clear (web_view->news_feeds);
    gjs_value_foreach (links, (GjsCallback)gjs_value_links_foreach_cb, web_view);
    g_object_unref (links);
    g_object_unref (document);
    g_object_unref (value);

    if (web_view->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (web_view->tab_icon),
                                     FALSE);
    web_view->load_status = MIDORI_LOAD_FINISHED;
    g_object_notify (G_OBJECT (web_view), "mload-status");
}

static void
webkit_web_view_load_finished (MidoriWebView* web_view)
{
    web_view->progress = 1.0;
    g_object_notify (G_OBJECT (web_view), "progress");
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
gtk_widget_button_press_event_after (MidoriWebView*  web_view,
                                     GdkEventButton* event)
{
    GdkModifierType state;
    GtkClipboard* clipboard;
    gchar* uri;
    gchar* new_uri;

    if (event->button == 2 && sokoke_object_get_boolean
        (web_view->settings, "middle-click-opens-selection"))
    {
        state = (GdkModifierType) event->state;
        clipboard = gtk_clipboard_get_for_display (
            gtk_widget_get_display (GTK_WIDGET (web_view)),
            GDK_SELECTION_PRIMARY);
        uri = gtk_clipboard_wait_for_text (clipboard);
        if (uri && strchr (uri, '.') && !strchr (uri, ' '))
        {
            new_uri = sokoke_magic_uri (uri, NULL);
            if (state & GDK_CONTROL_MASK)
                g_signal_emit (web_view, signals[NEW_TAB], 0, new_uri);
            else
            {
                g_object_set (web_view, "uri", new_uri, NULL);
                gtk_widget_grab_focus (GTK_WIDGET (web_view));
            }
            g_free (new_uri);
            g_free (uri);
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
gtk_widget_button_release_event (MidoriWebView*  web_view,
                                 GdkEventButton* event)
{
    GtkClipboard* clipboard;
    gchar* text;

    /* Emulate the primary clipboard, which WebKit doesn't support */
    text = webkit_web_view_get_selected_text (WEBKIT_WEB_VIEW (web_view));
    clipboard = gtk_clipboard_get_for_display (
        gtk_widget_get_display (GTK_WIDGET (web_view)), GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text (clipboard, text, -1);
    g_free (text);
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
midori_web_view_menu_download_activate_cb (GtkWidget*     widget,
                                           MidoriWebView* web_view)
{
    gchar* program;
    const gchar* uri;

    g_object_get (web_view->settings, "download-manager", &program, NULL);
    uri = g_object_get_data (G_OBJECT (widget), "uri");
    sokoke_spawn_program (program, uri);
    g_free (program);
}

static void
webkit_web_view_populate_popup_cb (GtkWidget* web_view,
                                   GtkWidget* menu)
{
    const gchar* uri;
    GtkWidget* menuitem;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    GtkWidget* icon;
    gchar* text;
    GList* items;
    gchar* program;

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
        g_object_get (MIDORI_WEB_VIEW (web_view)->settings,
            "download-manager", &program, NULL);
        if (program && *program)
        {
            menuitem = gtk_image_menu_item_new_with_mnemonic (
                _("Download Link with Download _Manager"));
            icon = gtk_image_new_from_stock (GTK_STOCK_SAVE_AS,
                                             GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 4);
            g_object_set_data (G_OBJECT (menuitem), "uri", (gchar*)uri);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_web_view_menu_download_activate_cb), web_view);
            gtk_widget_show (menuitem);
        }
    }

    if (!uri && midori_web_view_has_selection (MIDORI_WEB_VIEW (web_view)))
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
midori_web_view_init (MidoriWebView* web_view)
{
    web_view->icon = gtk_widget_render_icon (GTK_WIDGET (web_view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    web_view->progress = 0.0;
    web_view->load_status = MIDORI_LOAD_FINISHED;
    web_view->news_feeds = katze_array_new (GJS_TYPE_VALUE);

    web_view->settings = midori_web_settings_new ();
    g_object_set (web_view, "WebKitWebView::settings", web_view->settings, NULL);

    WebKitWebFrame* web_frame;
    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));

    g_object_connect (web_view,
                      "signal::load-started",
                      webkit_web_view_load_started, NULL,
                      "signal::window-object-cleared",
                      webkit_web_view_window_object_cleared_cb, NULL,
                      "signal::load-committed",
                      webkit_web_view_load_committed, NULL,
                      "signal::icon-ready",
                      webkit_web_view_icon_ready, NULL,
                      "signal::load-progress-changed",
                      webkit_web_view_progress_changed, NULL,
                      "signal::load-finished",
                      webkit_web_view_load_finished, NULL,
                      "signal::title-changed",
                      webkit_web_view_title_changed, NULL,
                      "signal::status-bar-text-changed",
                      webkit_web_view_statusbar_text_changed, NULL,
                      "signal::hovering-over-link",
                      webkit_web_view_hovering_over_link, NULL,
                      "signal::button-press-event",
                      gtk_widget_button_press_event_after, NULL,
                      "signal::button-release-event",
                      gtk_widget_button_release_event, NULL,
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
    MidoriWebView* web_view;
    WebKitWebFrame* web_frame;

    web_view = MIDORI_WEB_VIEW (object);

    if (web_view->icon)
        g_object_unref (web_view->icon);
    g_free (web_view->uri);
    g_free (web_view->title);
    g_free (web_view->statusbar_text);
    g_free (web_view->link_uri);
    g_object_unref (web_view->news_feeds);

    if (web_view->settings)
        g_object_unref (web_view->settings);

    if (web_view->xbel_item)
        katze_xbel_item_unref (web_view->xbel_item);

    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));
    g_signal_handlers_disconnect_by_func (web_frame,
        webkit_web_frame_load_done, web_view);

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
            webkit_web_view_open (WEBKIT_WEB_VIEW (web_view), uri);
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
        katze_object_assign (web_view->settings, g_value_get_object (value));
        g_object_ref (web_view->settings);
        g_object_set (object, "WebKitWebView::settings", web_view->settings, NULL);
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
    case PROP_PROGRESS:
        g_value_set_double (value, web_view->progress);
        break;
    case PROP_MLOAD_STATUS:
        g_value_set_enum (value, web_view->load_status);
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

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), FALSE);

    if (!web_view->menu_item)
    {
        title = midori_web_view_get_display_title (web_view);
        web_view->menu_item = gtk_image_menu_item_new_with_label (title);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (web_view->menu_item),
            gtk_image_new_from_pixbuf (web_view->icon));

        g_signal_connect (web_view->menu_item, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &web_view->menu_item);
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
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    if (!web_view->tab_icon)
    {
        web_view->tab_icon = katze_throbber_new ();
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (web_view->tab_icon),
                                          web_view->icon);

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
 * midori_web_view_load_status:
 * @web_view: a #MidoriWebView
 *
 * Determines the current loading status of a page.
 *
 * Return value: the current #MidoriLoadStatus
 **/
MidoriLoadStatus
midori_web_view_get_load_status (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), MIDORI_LOAD_FINISHED);

    return web_view->load_status;
}

/**
 * midori_web_view_get_progress:
 * @web_view: a #MidoriWebView
 *
 * Retrieves the current loading progress as
 * a fraction between 0.0 and 1.0.
 *
 * Return value: the current loading progress
 **/
gdouble
midori_web_view_get_progress (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), 0.0);

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
 * midori_web_view_get_news_feeds:
 * @web_view: a #MidoriWebView
 *
 * Retrieves a list of news feeds for the current page
 * or %NULL if there are no feeds at all.
 *
 * Return value: a #KatzeArray, or %NULL
 **/
KatzeArray*
midori_web_view_get_news_feeds (MidoriWebView* web_view)
{
    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), NULL);

    if (!katze_array_is_empty (web_view->news_feeds))
        return web_view->news_feeds;
    return NULL;
}

/**
 * midori_web_view_has_selection:
 * @web_view: a #MidoriWebView
 *
 * Determines whether something on the page is selected.
 *
 * By contrast to webkit_web_view_has_selection() this
 * returns %FALSE if there is a selection that
 * effectively only consists of whitespace.
 *
 * Return value: %TRUE if effectively there is a selection
 **/
gboolean
midori_web_view_has_selection (MidoriWebView* web_view)
{
    gchar* text;

    g_return_val_if_fail (MIDORI_IS_WEB_VIEW (web_view), FALSE);

    text = webkit_web_view_get_selected_text (WEBKIT_WEB_VIEW (web_view));
    if (text && *text)
    {
        g_free (text);
        return TRUE;
    }
    g_free (text);
    return FALSE;
}
