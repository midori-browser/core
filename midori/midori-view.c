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

#include "midori-view.h"
#include "midori-source.h"

#include "midori-stock.h"

#include "compat.h"
#include "sokoke.h"
#include "gjs.h"

#include <string.h>
#include <stdlib.h>
#if HAVE_GIO
    #include <gio/gio.h>
#endif
#include <glib/gi18n.h>
#include <webkit/webkit.h>

/* This is unstable API, so we need to declare it */
gchar*
webkit_web_view_get_selected_text (WebKitWebView* web_view);
void
webkit_web_frame_print (WebKitWebFrame* web_frame);

struct _MidoriView
{
    GtkScrolledWindow parent_instance;

    gchar* uri;
    gchar* title;
    GdkPixbuf* icon;
    gdouble progress;
    MidoriLoadStatus load_status;
    gchar* statusbar_text;
    gchar* link_uri;
    gboolean has_selection;
    gchar* selected_text;
    MidoriWebSettings* settings;
    GtkWidget* web_view;
    gboolean window_object_cleared;

    gchar* download_manager;
    gboolean middle_click_opens_selection;
    gboolean open_tabs_in_the_background;
    gboolean close_buttons_on_tabs;

    GtkWidget* menu_item;
    GtkWidget* tab_label;
    GtkWidget* tab_icon;
    GtkWidget* tab_title;
    GtkWidget* tab_close;
    KatzeItem* item;
};

struct _MidoriViewClass
{
    GtkScrolledWindowClass parent_class;
};

G_DEFINE_TYPE (MidoriView, midori_view, GTK_TYPE_SCROLLED_WINDOW)

GType
midori_load_status_get_type (void)
{
    static GType type = 0;
    if (type)
        return type;
    static const GEnumValue values[] = {
     { MIDORI_LOAD_PROVISIONAL, "MIDORI_LOAD_PROVISIONAL", N_("Load Provisional") },
     { MIDORI_LOAD_COMMITTED, "MIDORI_LOAD_COMMITTED", N_("Load Committed") },
     { MIDORI_LOAD_FINISHED, "MIDORI_LOAD_FINISHED", N_("Load Finished") },
     { 0, NULL, NULL }
    };
    type = g_enum_register_static ("MidoriLoadStatus", values);
    return type;
}

enum
{
    PROP_0,

    PROP_URI,
    PROP_TITLE,
    PROP_ICON,
    PROP_LOAD_STATUS,
    PROP_PROGRESS,
    PROP_ZOOM_LEVEL,
    PROP_STATUSBAR_TEXT,
    PROP_SETTINGS
};

enum {
    ACTIVATE_ACTION,
    CONSOLE_MESSAGE,
    WINDOW_OBJECT_CLEARED,
    NEW_TAB,
    NEW_WINDOW,
    SEARCH_TEXT,
    ADD_BOOKMARK,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_view_finalize (GObject* object);

static void
midori_view_set_property (GObject*      object,
                          guint         prop_id,
                          const GValue* value,
                          GParamSpec*   pspec);

static void
midori_view_get_property (GObject*    object,
                          guint       prop_id,
                          GValue*     value,
                          GParamSpec* pspec);

static void
midori_view_settings_notify_cb (MidoriWebSettings* settings,
                                GParamSpec*        pspec,
                                MidoriView*        view);

static void
midori_cclosure_marshal_VOID__STRING_BOOLEAN (GClosure*     closure,
                                              GValue*       return_value,
                                              guint         n_param_values,
                                              const GValue* param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data)
{
    typedef void(*GMarshalFunc_VOID__STRING_BOOLEAN) (gpointer  data1,
                                                      gpointer  arg_1,
                                                      gboolean  arg_2,
                                                      gpointer  data2);
    register GMarshalFunc_VOID__STRING_BOOLEAN callback;
    register GCClosure* cc = (GCClosure*) closure;
    register gpointer data1, data2;

    g_return_if_fail (n_param_values == 3);

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
    callback = (GMarshalFunc_VOID__STRING_BOOLEAN) (marshal_data
        ? marshal_data : cc->callback);
    callback (data1,
              (gchar*)g_value_get_string (param_values + 1),
              g_value_get_boolean (param_values + 2),
              data2);
}

static void
midori_cclosure_marshal_VOID__STRING_INT_STRING (GClosure*     closure,
                                                 GValue*       return_value,
                                                 guint         n_param_values,
                                                 const GValue* param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data)
{
    typedef void(*GMarshalFunc_VOID__STRING_INT_STRING) (gpointer  data1,
                                                         gpointer  arg_1,
                                                         gint      arg_2,
                                                         gpointer  arg_3,
                                                         gpointer  data2);
    register GMarshalFunc_VOID__STRING_INT_STRING callback;
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
    callback = (GMarshalFunc_VOID__STRING_INT_STRING) (marshal_data
        ? marshal_data : cc->callback);
    callback (data1,
              (gchar*)g_value_get_string (param_values + 1),
              g_value_get_int (param_values + 2),
              (gchar*)g_value_get_string (param_values + 3),
              data2);
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
midori_view_class_init (MidoriViewClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    signals[ACTIVATE_ACTION] = g_signal_new (
        "activate-action",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[CONSOLE_MESSAGE] = g_signal_new (
        "console-message",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__STRING_INT_STRING,
        G_TYPE_NONE, 3,
        G_TYPE_STRING,
        G_TYPE_INT,
        G_TYPE_STRING);

    signals[WINDOW_OBJECT_CLEARED] = g_signal_new (
        "window-object-cleared",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__OBJECT_POINTER_POINTER,
        G_TYPE_NONE, 3,
        WEBKIT_TYPE_WEB_FRAME,
        G_TYPE_POINTER,
        G_TYPE_POINTER);

    signals[NEW_TAB] = g_signal_new (
        "new-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__STRING_BOOLEAN,
        G_TYPE_NONE, 2,
        G_TYPE_STRING,
        G_TYPE_BOOLEAN);

    signals[NEW_WINDOW] = g_signal_new (
        "new-window",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    signals[SEARCH_TEXT] = g_signal_new (
        "search-text",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE, 1,
        G_TYPE_BOOLEAN);

    signals[ADD_BOOKMARK] = g_signal_new (
        "add-bookmark",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_view_finalize;
    gobject_class->set_property = midori_view_set_property;
    gobject_class->get_property = midori_view_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     "Uri",
                                     "The URI of the currently loaded page",
                                     "about:blank",
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_TITLE,
                                     g_param_spec_string (
                                     "title",
                                     "Title",
                                     "The title of the currently loaded page",
                                     NULL,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON,
                                     g_param_spec_object (
                                     "icon",
                                     "Icon",
                                     "The icon of the view",
                                     GDK_TYPE_PIXBUF,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_LOAD_STATUS,
                                     g_param_spec_enum (
                                     "load-status",
                                     "Load Status",
                                     "The current loading status",
                                     MIDORI_TYPE_LOAD_STATUS,
                                     MIDORI_LOAD_FINISHED,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_PROGRESS,
                                     g_param_spec_double (
                                     "progress",
                                     "Progress",
                                     "The current loading progress",
                                     0.0, 1.0, 0.0,
                                     G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_ZOOM_LEVEL,
                                     g_param_spec_float (
                                     "zoom-level",
                                     "Zoom Level",
                                     "The current zoom level",
                                     G_MINFLOAT,
                                     G_MAXFLOAT,
                                     1.0f,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR_TEXT,
                                     g_param_spec_string (
                                     "statusbar-text",
                                     "Statusbar Text",
                                     "The text displayed in the statusbar",
                                     "",
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "The associated settings",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE));
}

static void
midori_view_notify_icon_cb (MidoriView* view,
                            GParamSpec  pspec)
{
    if (view->tab_icon)
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (view->tab_icon),
                                          view->icon);
    if (view->menu_item)
        gtk_image_menu_item_set_image (
            GTK_IMAGE_MENU_ITEM (view->menu_item),
                gtk_image_new_from_pixbuf (view->icon));
}

#if HAVE_GIO
void
loadable_icon_finish_cb (GdkPixbuf*    icon,
                         GAsyncResult* res,
                         MidoriView*   view)
{
    GdkPixbuf* pixbuf;
    GInputStream* stream;
    GError* error;
    GdkPixbuf* pixbuf_scaled;
    gint icon_width, icon_height;

    pixbuf = NULL;
    stream = g_loadable_icon_load_finish (G_LOADABLE_ICON (icon),
                                          res, NULL, NULL);
    if (stream)
    {
        error = NULL;
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
        if (error)
            g_warning (_("Icon couldn't be loaded: %s\n"), error->message);
        g_object_unref (stream);
    }
    if (!pixbuf)
        pixbuf = gtk_widget_render_icon (GTK_WIDGET (view),
            GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);

    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    katze_object_assign (view->icon, pixbuf_scaled);
    g_object_notify (G_OBJECT (view), "icon");
}

void
file_info_finish_cb (GFile*        icon_file,
                     GAsyncResult* res,
                     MidoriView*   view)
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
                0, NULL, (GAsyncReadyCallback)loadable_icon_finish_cb, view);
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
            (GAsyncReadyCallback)file_info_finish_cb, view);
        return;
    }

    pixbuf = gtk_widget_render_icon (GTK_WIDGET (view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    view->icon = pixbuf_scaled;
    g_object_notify (G_OBJECT (view), "icon");
}
#endif

static void
_midori_web_view_load_icon (MidoriView* view)
{
    #if HAVE_GIO
    GFile* file;
    GFile* icon_file;
    #endif
    GdkPixbuf* pixbuf;
    gint icon_width, icon_height;
    GdkPixbuf* pixbuf_scaled;

    #if HAVE_GIO
    if (view->uri)
    {
        file = g_file_new_for_uri (view->uri);
        icon_file = g_file_get_child (file, "favicon.ico");
        g_file_query_info_async (icon_file,
            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
            G_FILE_QUERY_INFO_NONE, 0, NULL,
            (GAsyncReadyCallback)file_info_finish_cb, view);
        return;
    }
    #endif

    pixbuf = gtk_widget_render_icon (GTK_WIDGET (view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    view->icon = pixbuf_scaled;
    g_object_notify (G_OBJECT (view), "icon");
}

static void
midori_view_notify_load_status_cb (MidoriView* view,
                                   GParamSpec  pspec)
{
    if (view->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (view->tab_icon),
            view->load_status != MIDORI_LOAD_FINISHED);

    if (view->load_status == MIDORI_LOAD_COMMITTED)
        _midori_web_view_load_icon (view);
}

static void
webkit_web_view_load_started_cb (WebKitWebView*  web_view,
                                 WebKitWebFrame* web_frame,
                                 MidoriView*     view)
{
    view->window_object_cleared = FALSE;

    view->load_status = MIDORI_LOAD_PROVISIONAL;
    g_object_notify (G_OBJECT (view), "load-status");

    view->progress = 0.0;
    g_object_notify (G_OBJECT (view), "progress");
}

static void
webkit_web_view_load_committed_cb (WebKitWebView*  web_view,
                                   WebKitWebFrame* web_frame,
                                   MidoriView*     view)
{
    const gchar* uri;
    GdkPixbuf* icon;

    uri = webkit_web_frame_get_uri (web_frame);
    katze_assign (view->uri, g_strdup (uri));
    g_object_notify (G_OBJECT (view), "uri");
    g_object_set (view, "title", NULL, NULL);

    icon = gtk_widget_render_icon (GTK_WIDGET (view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    katze_object_assign (view->icon, icon);
    g_object_notify (G_OBJECT (view), "icon");

    view->load_status = MIDORI_LOAD_COMMITTED;
    g_object_notify (G_OBJECT (view), "load-status");
}

static void
webkit_web_view_progress_changed_cb (WebKitWebView* web_view,
                                     gint           progress,
                                     MidoriView*    view)
{
    view->progress = progress ? progress / 100.0 : 0.0;
    g_object_notify (G_OBJECT (view), "progress");
}

/*
static void
gjs_value_links_foreach_cb (GjsValue*   link,
                            MidoriView* view)
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
                katze_array_add_item (view->news_feeds, link);
                g_signal_emit_by_name (view, "news-feed-ready",
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
                    0, NULL, (GAsyncReadyCallback)loadable_icon_finish_cb, view);
            }
        }
#endif
    }
}
*/

static void
webkit_web_frame_load_done_cb (WebKitWebFrame* web_frame,
                               gboolean        success,
                               MidoriView*     view)
{
    gchar* data;
    /*JSContextRef js_context;
    JSValueRef js_window;
    GjsValue* value;
    GjsValue* document;
    GjsValue* links; */

    if (!success)
    {
        /* Simply print a 404 error page on the fly. */
        data = g_strdup_printf (
            "<html><head><title>Not found - %s</title></head>"
            "<body><h1>Not found - %s</h1>"
            "<img src=\"file://" DATADIR "/midori/logo-shade.png\" "
            "style=\"position: absolute; right: 15px; bottom: 15px;\">"
            "<p />The page you were opening doesn't exist."
            "<p />Try to <a href=\"%s\">load the page again</a>, "
            "or move on to another page."
            "</body></html>",
            view->uri, view->uri, view->uri);
        webkit_web_view_load_html_string (
            WEBKIT_WEB_VIEW (view->web_view), data, view->uri);
        g_free (data);
    }

    /* js_context = webkit_web_frame_get_global_context (web_frame);
    value = gjs_value_new (js_context, NULL);
    document = gjs_value_get_by_name (value, "document");
    links = gjs_value_get_elements_by_tag_name (document, "link");
    katze_array_clear (web_view->news_feeds);
    gjs_value_foreach (links, (GjsCallback)gjs_value_links_foreach_cb, web_view);
    g_object_unref (links);
    g_object_unref (document);
    g_object_unref (value); */

    view->load_status = MIDORI_LOAD_FINISHED;
    g_object_notify (G_OBJECT (view), "load-status");
}

static void
webkit_web_view_load_finished_cb (WebKitWebView*  web_view,
                                  WebKitWebFrame* web_frame,
                                  MidoriView*     view)
{
    view->progress = 1.0;
    g_object_notify (G_OBJECT (view), "progress");
}

static void
webkit_web_view_title_changed_cb (WebKitWebView*  web_view,
                                  WebKitWebFrame* web_frame,
                                  const gchar*    title,
                                  MidoriView*     view)
{
    g_object_set (view, "title", title, NULL);
}

static void
webkit_web_view_statusbar_text_changed_cb (WebKitWebView* web_view,
                                           const gchar*   text,
                                           MidoriView*    view)
{
    g_object_set (G_OBJECT (view), "statusbar-text", text, NULL);
}

static void
webkit_web_view_hovering_over_link_cb (WebKitWebView* web_view,
                                       const gchar*   tooltip,
                                       const gchar*   link_uri,
                                       MidoriView*    view)
{
    katze_assign (view->link_uri, g_strdup (link_uri));
    g_object_set (G_OBJECT (view), "statusbar-text", link_uri, NULL);
}

static gboolean
gtk_widget_button_press_event_cb (WebKitWebView*  web_view,
                                  GdkEventButton* event,
                                  MidoriView*     view)
{
    GdkModifierType state;
    gint x, y;
    GtkClipboard* clipboard;
    gchar* uri;
    gchar* new_uri;
    const gchar* link_uri;
    gboolean background;

    gdk_window_get_pointer (NULL, &x, &y, &state);
    link_uri = midori_view_get_link_uri (MIDORI_VIEW (view));

    switch (event->button)
    {
    case 1:
        if (!link_uri)
            return FALSE;
        if (state & GDK_SHIFT_MASK)
        {
            /* Open link in new window */
            g_signal_emit_by_name (view, "new-window", link_uri);
            return TRUE;
        }
        else if (state & GDK_MOD1_MASK)
        {
            /* Open link in new tab */
            background = view->open_tabs_in_the_background;
            if (state & GDK_CONTROL_MASK)
                background = !background;
            g_signal_emit_by_name (view, "new-tab", link_uri, background);
            return TRUE;
        }
        break;
    case 2:
        if (link_uri)
        {
            /* Open link in new tab */
            background = view->open_tabs_in_the_background;
            if (state & GDK_CONTROL_MASK)
                background = !background;
            g_signal_emit_by_name (view, "new-tab", link_uri, background);
            return TRUE;
        }
        else if (state & GDK_CONTROL_MASK)
        {
            midori_view_set_zoom_level (MIDORI_VIEW (view), 1.0);
            return FALSE; /* Allow Ctrl + Middle click */
        }
        else if (view->middle_click_opens_selection)
        {
            state = (GdkModifierType) event->state;
            clipboard = gtk_clipboard_get_for_display (
                gtk_widget_get_display (GTK_WIDGET (view)),
                GDK_SELECTION_PRIMARY);
            uri = gtk_clipboard_wait_for_text (clipboard);
            if (uri && strchr (uri, '.') && !strchr (uri, ' '))
            {
                new_uri = sokoke_magic_uri (uri, NULL);
                if (state & GDK_CONTROL_MASK)
                {
                    background = view->open_tabs_in_the_background;
                    if (state & GDK_CONTROL_MASK)
                        background = !background;
                    g_signal_emit_by_name (view, "new-tab", new_uri, background);
                }
                else
                {
                    midori_view_set_uri (MIDORI_VIEW (view), new_uri);
                    gtk_widget_grab_focus (GTK_WIDGET (view));
                }
                g_free (new_uri);
                g_free (uri);
                return TRUE;
            }
        }
        break;
    }

    return FALSE;
}

static gboolean
gtk_widget_scroll_event_cb (WebKitWebView*  web_view,
                            GdkEventScroll* event,
                            MidoriView*     view)
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
midori_web_view_menu_new_tab_activate_cb (GtkWidget*  widget,
                                          MidoriView* view)
{
    g_signal_emit (view, signals[NEW_TAB], 0, view->link_uri,
        view->open_tabs_in_the_background);
}

static void
midori_web_view_menu_new_window_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    g_signal_emit (view, signals[NEW_WINDOW], 0, view->link_uri);
}

static void
midori_web_view_menu_download_activate_cb (GtkWidget*  widget,
                                           MidoriView* view)
{
    sokoke_spawn_program (view->download_manager, view->link_uri);
}

static void
midori_web_view_menu_add_bookmark_activate_cb (GtkWidget*  widget,
                                               MidoriView* view)
{
    g_signal_emit (view, signals[ADD_BOOKMARK], 0, view->link_uri);
}

static void
midori_web_view_menu_action_activate_cb (GtkWidget*  widget,
                                         MidoriView* view)
{
    const gchar* action = g_object_get_data (G_OBJECT (widget), "action");
    g_signal_emit_by_name (view, "activate-action", action);
}

static void
webkit_web_view_populate_popup_cb (WebKitWebView* web_view,
                                   GtkWidget*     menu,
                                   MidoriView*    view)
{
    GtkWidget* menuitem;
    GtkWidget* icon;
    gchar* stock_id;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    GList* items;
    gboolean has_selection;

    /* We do not want to modify the Edit menu.
       The only reliable indicator is inspecting the first item. */
    items = gtk_container_get_children (GTK_CONTAINER (menu));
    menuitem = (GtkWidget*)g_list_nth_data (items, 0);
    if (GTK_IS_IMAGE_MENU_ITEM (menuitem))
    {
        icon = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menuitem));
        gtk_image_get_stock (GTK_IMAGE (icon), &stock_id, NULL);
        if (!strcmp (stock_id, GTK_STOCK_CUT))
            return;
    }

    has_selection = midori_view_has_selection (view);

    if (view->link_uri)
    {
        menuitem = gtk_image_menu_item_new_with_mnemonic (
            _("Open Link in New _Tab"));
        screen = gtk_widget_get_screen (GTK_WIDGET (view));
        icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, STOCK_TAB_NEW))
        {
            icon = gtk_image_new_from_stock (STOCK_TAB_NEW, GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 1);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), view);
        gtk_widget_show (menuitem);
        /* hack to implement New Window */
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        menuitem = (GtkWidget*)g_list_nth_data (items, 2);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_new_window_activate_cb), view);
        menuitem = (GtkWidget*)g_list_nth_data (items, 3);
        /* hack to disable non-functional Download File */
        gtk_widget_set_sensitive (menuitem, FALSE);
        g_list_free (items);
        if (view->download_manager && *view->download_manager)
        {
            menuitem = gtk_image_menu_item_new_with_mnemonic (
                _("Download Link with Download _Manager"));
            icon = gtk_image_new_from_stock (GTK_STOCK_SAVE_AS,
                                             GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 4);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_web_view_menu_download_activate_cb), view);
            gtk_widget_show (menuitem);
        }
        menuitem = gtk_image_menu_item_new_from_stock (STOCK_BOOKMARK_ADD, NULL);
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 5);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_add_bookmark_activate_cb), view);
        gtk_widget_show (menuitem);
    }

    if (!view->link_uri && has_selection)
    {
        if (strchr (view->selected_text, '.')
            && !strchr (view->selected_text, ' '))
        {
            menuitem = gtk_image_menu_item_new_with_mnemonic (
                _("Open URL in New _Tab"));
            icon = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO,
                                             GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
            gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, -1);
            g_object_set_data (G_OBJECT (menuitem), "uri", view->selected_text);
            g_signal_connect (menuitem, "activate",
                G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), view);
            gtk_widget_show (menuitem);
        }
        /* FIXME: view selection source */
    }

    if (!view->link_uri && !has_selection)
    {
        /* FIXME: Make this sensitive only when there is a tab to undo */
        menuitem = gtk_image_menu_item_new_with_mnemonic (_("Undo Close Tab"));
        icon = gtk_image_new_from_stock (GTK_STOCK_UNDELETE, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "action", "UndoTabClose");
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_action_activate_cb), view);
        gtk_widget_show (menuitem);
        menuitem = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
        menuitem = gtk_image_menu_item_new_from_stock (STOCK_BOOKMARK_ADD, NULL);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "action", "BookmarkAdd");
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_action_activate_cb), view);
        gtk_widget_show (menuitem);
        menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, NULL);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "action", "SaveAs");
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_action_activate_cb), view);
        gtk_widget_show (menuitem);
        /* FIXME: Make this sensitive once it's implemented */
        gtk_widget_set_sensitive (menuitem, FALSE);
        menuitem = gtk_image_menu_item_new_with_mnemonic (_("View _Source"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "action", "SourceView");
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_action_activate_cb), view);
        gtk_widget_show (menuitem);
        if (!midori_view_can_view_source (view))
            gtk_widget_set_sensitive (menuitem, FALSE);
        menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_PRINT, NULL);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_object_set_data (G_OBJECT (menuitem), "action", "Print");
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_action_activate_cb), view);
        gtk_widget_show (menuitem);
    }
}

static void
webkit_web_view_console_message_cb (GtkWidget*   web_view,
                                    const gchar* message,
                                    guint        line,
                                    const gchar* source_id,
                                    MidoriView*  view)
{
    g_signal_emit_by_name (view, "console-message", message, line, source_id);
}

static void
webkit_web_view_window_object_cleared_cb (GtkWidget*      web_view,
                                          WebKitWebFrame* web_frame,
                                          JSContextRef    js_context,
                                          JSObjectRef     js_window,
                                          MidoriView*     view)
{
    g_signal_emit (view, signals[WINDOW_OBJECT_CLEARED], 0,
                   web_frame, js_context, js_window);
}

static void
midori_view_init (MidoriView* view)
{
    view->uri = NULL;
    view->title = NULL;
    view->icon = gtk_widget_render_icon (GTK_WIDGET (view), GTK_STOCK_FILE,
                                         GTK_ICON_SIZE_MENU, NULL);
    view->progress = 0.0;
    view->load_status = MIDORI_LOAD_FINISHED;
    view->statusbar_text = NULL;
    view->link_uri = NULL;
    view->selected_text = NULL;
    view->settings = midori_web_settings_new ();
    view->item = NULL;

    view->download_manager = NULL;

    g_object_connect (view,
                      "signal::notify::icon",
                      midori_view_notify_icon_cb, NULL,
                      "signal::notify::load-status",
                      midori_view_notify_load_status_cb, NULL,
                      NULL);

    view->web_view = NULL;

    /* Adjustments are not created automatically */
    g_object_set (view, "hadjustment", NULL, "vadjustment", NULL, NULL);
}

static void
midori_view_finalize (GObject* object)
{
    MidoriView* view;
    /* WebKitWebFrame* web_frame; */

    view = MIDORI_VIEW (object);

    g_signal_handlers_disconnect_by_func (view->settings,
        midori_view_settings_notify_cb, view);

    g_free (view->uri);
    g_free (view->title);
    if (view->icon)
        g_object_unref (view->icon);
    g_free (view->statusbar_text);
    g_free (view->link_uri);
    g_free (view->selected_text);
    if (view->settings)
        g_object_unref (view->settings);
    if (view->item)
        g_object_unref (view->item);

    g_free (view->download_manager);

    /* web_frame = webkit_web_view_get_main_frame
        (WEBKIT_WEB_VIEW (view->web_view));
    g_signal_handlers_disconnect_by_func (web_frame,
        webkit_web_frame_load_done, view); */

    G_OBJECT_CLASS (midori_view_parent_class)->finalize (object);
}

static void
midori_view_set_property (GObject*      object,
                          guint         prop_id,
                          const GValue* value,
                          GParamSpec*   pspec)
{
    MidoriView* view;

    view = MIDORI_VIEW (object);

    switch (prop_id)
    {
    case PROP_TITLE:
        katze_assign (view->title, g_value_dup_string (value));
        #define title midori_view_get_display_title (view)
        if (view->tab_label)
        {
            gtk_label_set_text (GTK_LABEL (view->tab_title), title);
            gtk_widget_set_tooltip_text (view->tab_title, title);
        }
        if (view->menu_item)
            gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (
                                view->menu_item))), title);
        if (view->item)
            katze_item_set_name (view->item, title);
        #undef title
        break;
    case PROP_ZOOM_LEVEL:
        midori_view_set_zoom_level (view, g_value_get_float (value));
        break;
    case PROP_STATUSBAR_TEXT:
        katze_assign (view->statusbar_text, g_value_dup_string (value));
        break;
    case PROP_SETTINGS:
        midori_view_set_settings (view, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_view_get_property (GObject*    object,
                          guint       prop_id,
                          GValue*     value,
                          GParamSpec* pspec)
{
    MidoriView* view = MIDORI_VIEW (object);

    switch (prop_id)
    {
    case PROP_URI:
        g_value_set_string (value, view->uri);
        break;
    case PROP_TITLE:
        g_value_set_string (value, view->title);
        break;
    case PROP_PROGRESS:
        g_value_set_double (value, midori_view_get_progress (view));
        break;
    case PROP_LOAD_STATUS:
        g_value_set_enum (value, midori_view_get_load_status (view));
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_float (value, midori_view_get_zoom_level (view));
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, view->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, view->settings);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_view_new:
 * @view: a #MidoriView
 *
 * Creates a new view.
 *
 * Return value: a new #MidoriView
 **/
GtkWidget*
midori_view_new (void)
{
    return g_object_new (MIDORI_TYPE_VIEW, NULL);
}

static void
_update_label_size (GtkWidget* label,
                    gint       size)
{
    gint width, height;

    if (size < 1)
        size = 10;

    sokoke_widget_get_text_size (label, "M", &width, &height);
    gtk_widget_set_size_request (label, width * size, -1);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
}

static void
_midori_view_update_settings (MidoriView* view)
{
    g_object_get (view->settings,
        "download-manager", &view->download_manager,
        "close-buttons-on-tabs", &view->close_buttons_on_tabs,
        "middle-click-opens-selection", &view->middle_click_opens_selection,
        "open-tabs-in-the-background", &view->open_tabs_in_the_background,
        NULL);
}

static void
midori_view_settings_notify_cb (MidoriWebSettings* settings,
                                GParamSpec*        pspec,
                                MidoriView*        view)
{
    const gchar* name;
    GValue value = { 0, };

    name = g_intern_string (g_param_spec_get_name (pspec));
    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (view->settings), name, &value);

    if (name == g_intern_string ("download-manager"))
    {
        katze_assign (view->download_manager, g_value_dup_string (&value));
    }
    else if (name == g_intern_string ("close-buttons-on-tabs"))
    {
        view->close_buttons_on_tabs = g_value_get_boolean (&value);
        sokoke_widget_set_visible (view->tab_close,
                                   view->close_buttons_on_tabs);
    }
    else if (name == g_intern_string ("middle-click-opens-selection"))
    {
        view->middle_click_opens_selection = g_value_get_boolean (&value);
    }
    else if (name == g_intern_string ("open-tabs-in-the-background"))
    {
        view->open_tabs_in_the_background = g_value_get_boolean (&value);
    }

    g_value_unset (&value);
}

/**
 * midori_view_set_settings:
 * @view: a #MidoriView
 * @settings: a #MidoriWebSettings
 *
 * Assigns a settings instance to the view.
 **/
void
midori_view_set_settings (MidoriView*        view,
                          MidoriWebSettings* settings)
{
    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);
    katze_object_assign (view->settings, g_object_ref (settings));
    if (view->web_view)
        g_object_set (view->web_view, "settings", view->settings, NULL);
    _midori_view_update_settings (view);
    g_signal_connect (settings, "notify",
        G_CALLBACK (midori_view_settings_notify_cb), view);
    g_object_notify (G_OBJECT (view), "settings");
}

/**
 * midori_view_load_status:
 * @web_view: a #MidoriView
 *
 * Determines the current loading status of a view.
 *
 * Return value: the current #MidoriLoadStatus
 **/
MidoriLoadStatus
midori_view_get_load_status (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), MIDORI_LOAD_FINISHED);

    return view->load_status;
}

/**
 * midori_view_get_progress:
 * @view: a #MidoriView
 *
 * Retrieves the current loading progress as
 * a fraction between 0.0 and 1.0.
 *
 * Return value: the current loading progress
 **/
gdouble
midori_view_get_progress (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), 0.0);

    return view->progress;
}

static void
midori_view_construct_web_view (MidoriView* view)
{
    WebKitWebFrame* web_frame;

    view->web_view = webkit_web_view_new ();

    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));

    g_object_connect (view->web_view,
                      "signal::load-started",
                      webkit_web_view_load_started_cb, view,
                      "signal::load-committed",
                      webkit_web_view_load_committed_cb, view,
                      "signal::load-progress-changed",
                      webkit_web_view_progress_changed_cb, view,
                      "signal::load-finished",
                      webkit_web_view_load_finished_cb, view,
                      "signal::title-changed",
                      webkit_web_view_title_changed_cb, view,
                      "signal::status-bar-text-changed",
                      webkit_web_view_statusbar_text_changed_cb, view,
                      "signal::hovering-over-link",
                      webkit_web_view_hovering_over_link_cb, view,
                      "signal::button-press-event",
                      gtk_widget_button_press_event_cb, view,
                      "signal::scroll-event",
                      gtk_widget_scroll_event_cb, view,
                      "signal::populate-popup",
                      webkit_web_view_populate_popup_cb, view,
                      "signal::console-message",
                      webkit_web_view_console_message_cb, view,
                      "signal::window-object-cleared",
                      webkit_web_view_window_object_cleared_cb, view,
                      NULL);
    g_object_connect (web_frame,
                      "signal::load-done",
                      webkit_web_frame_load_done_cb, view,
                      NULL);

    g_object_set (view->web_view, "settings", view->settings, NULL);

    gtk_widget_show (view->web_view);
    gtk_container_add (GTK_CONTAINER (view), view->web_view);
}

/**
 * midori_view_set_uri:
 * @view: a #MidoriView
 *
 * Opens the specified URI in the view.
 *
 * Pass an URI prefixed with "view-source:" in
 * order to create a source view.
 **/
void
midori_view_set_uri (MidoriView*  view,
                     const gchar* uri)
{
    GtkWidget* widget;
    gchar* data;

    g_return_if_fail (MIDORI_IS_VIEW (view));

    if (!view->web_view && view->uri
        && g_str_has_prefix (view->uri, "view-source:"))
    {
        g_signal_emit (view, signals[NEW_TAB], 0, uri);
    }
    else if (!view->web_view && uri && g_str_has_prefix (uri, "view-source:"))
    {
        katze_assign (view->uri, g_strdup (uri));
        g_object_notify (G_OBJECT (view), "uri");
        data = g_strdup_printf ("%s - %s", _("Source"), &uri[12]);
        g_object_set (view, "title", data, NULL);
        g_free (data);
        katze_object_assign (view->icon,
            gtk_widget_render_icon (GTK_WIDGET (view),
                GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU, NULL));
        widget = midori_source_new (&uri[12]);
        gtk_container_add (GTK_CONTAINER (view), widget);
        gtk_widget_show (widget);
    }
    else
    {
        if (!view->web_view)
            midori_view_construct_web_view (view);
        /* This is not prefectly elegant, but creating an
           error page inline is the simplest solution. */
        if (g_str_has_prefix (uri, "error:"))
        {
            data = NULL;
            if (!strncmp (uri, "error:nodocs ", 13))
            {
                katze_assign (view->uri, g_strdup (&uri[13]));
                data = g_strdup_printf (
                    "<html><head><title>No documentation installed</title></head>"
                    "<body><h1>No documentation installed</h1>"
                    "<img src=\"file://" DATADIR "/midori/logo-shade.png\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px;\">"
                    "<p />There is no documentation installed at %s."
                    "You may want to ask your distribution or "
                    "package maintainer for it or if this a custom build "
                    "verify that the build is setup properly."
                    "</body></html>",
                    view->uri);
            }
            if (data)
            {
                webkit_web_view_load_html_string (
                    WEBKIT_WEB_VIEW (view->web_view), data, view->uri);
                g_free (data);
                g_object_notify (G_OBJECT (view), "uri");
                return;
            }
        }
        else
        {
            katze_assign (view->uri, g_strdup (uri));
            webkit_web_view_open (WEBKIT_WEB_VIEW (view->web_view), uri);
        }
    }
}

/**
 * midori_view_is_blank:
 * @view: a #MidoriView
 *
 * Determines whether the view is currently empty.
 **/
gboolean
midori_view_is_blank (MidoriView*  view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), TRUE);

    return midori_view_get_display_uri (view)[0] == '\0';
}

/**
 * midori_view_get_icon:
 * @view: a #MidoriView
 *
 * Retrieves the icon of the view.
 *
 * Return value: a #GdkPixbuf
 **/
GdkPixbuf*
midori_view_get_icon (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->icon;
}

/**
 * midori_view_get_display_uri:
 * @view: a #MidoriView
 *
 * Retrieves a string that is suitable for displaying.
 *
 * Note that "about:blank" is represented as "".
 *
 * You can assume that the string is not %NULL.
 *
 * Return value: an URI string
 **/
const gchar*
midori_view_get_display_uri (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), "");

    /* Something in the stack tends to turn "" into "about:blank".
       Yet for practical purposes we prefer "".  */
    if (view->uri && !strcmp (view->uri, "about:blank"))
        return "";

    if (view->uri && *view->uri)
        return view->uri;
    return "";
}

/**
 * midori_view_get_display_title:
 * @view: a #MidoriView
 *
 * Retrieves a string that is suitable for displaying
 * as a title. Most of the time this will be the title
 * or the current URI.
 *
 * An empty page is represented as "about:blank".
 *
 * You can assume that the string is not %NULL.
 *
 * Return value: a title string
 **/
const gchar*
midori_view_get_display_title (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), "about:blank");

    if (midori_view_is_blank (view))
        return "about:blank";

    if (view->title && *view->title)
        return view->title;
    return midori_view_get_display_uri (view);
}

/**
 * midori_view_get_link_uri:
 * @view: a #MidoriView
 *
 * Retrieves the uri of the currently focused link,
 * particularly while the mouse hovers a link or a
 * context menu is being opened.
 *
 * Return value: an URI string, or %NULL if there is no link focussed
 **/
const gchar*
midori_view_get_link_uri (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->link_uri;
}

/**
 * midori_view_has_selection:
 * @view: a #MidoriView
 *
 * Determines whether something in the view is selected.
 *
 * This function returns %FALSE if there is a selection
 * that effectively only consists of whitespace.
 *
 * Return value: %TRUE if effectively there is a selection
 **/
gboolean
midori_view_has_selection (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    view->selected_text = webkit_web_view_get_selected_text (
            WEBKIT_WEB_VIEW (view->web_view));
    if (view->selected_text && *view->selected_text)
        return TRUE;
    else
        return FALSE;
}

/**
 * midori_view_get_selected_text:
 * @view: a #MidoriView
 *
 * Retrieves the currently selected text.
 *
 * Return value: the selected text, or %NULL
 **/
const gchar*
midori_view_get_selected_text (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (midori_view_has_selection (view))
        return midori_view_get_selected_text (view);
    else
        return NULL;
}

/**
 * midori_view_can_cut_clipboard:
 * @view: a #MidoriView
 *
 * Determines whether a selection can be cut.
 *
 * Return value: %TRUE if a selection can be cut
 **/
gboolean
midori_view_can_cut_clipboard (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_cut_clipboard (
            WEBKIT_WEB_VIEW (view->web_view));
    else
        return FALSE;
}

/**
 * midori_view_can_copy_clipboard:
 * @view: a #MidoriView
 *
 * Determines whether a selection can be copied.
 *
 * Return value: %TRUE if a selection can be copied
 **/
gboolean
midori_view_can_copy_clipboard (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_copy_clipboard (
            WEBKIT_WEB_VIEW (view->web_view));
    else
        return FALSE;
}

/**
 * midori_view_can_paste_clipboard:
 * @view: a #MidoriView
 *
 * Determines whether a selection can be pasted.
 *
 * Return value: %TRUE if a selection can be pasted
 **/
gboolean
midori_view_can_paste_clipboard (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_paste_clipboard (
            WEBKIT_WEB_VIEW (view->web_view));
    else
        return FALSE;
}

/**
 * midori_view_get_proxy_menu_item:
 * @view: a #MidoriView
 *
 * Retrieves a proxy menu item that is typically added to a Window menu
 * and which on activation switches to the right window/ tab.
 *
 * The item is created on the first call and will be updated to reflect
 * changes to the icon and title automatically.
 *
 * The menu item is valid until it is removed from its container.
 *
 * Return value: the proxy #GtkMenuItem
 **/
GtkWidget*
midori_view_get_proxy_menu_item (MidoriView* view)
{
    const gchar* title;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->menu_item)
    {
        title = midori_view_get_display_title (view);
        view->menu_item = sokoke_image_menu_item_new_ellipsized (title);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (view->menu_item),
            gtk_image_new_from_pixbuf (view->icon));

        g_signal_connect (view->menu_item, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->menu_item);
    }
    return view->menu_item;
}

static gboolean
midori_view_tab_label_button_release_event (GtkWidget*      tab_label,
                                            GdkEventButton* event,
                                            GtkWidget*      widget)
{
    if (event->button == 2)
    {
        /* Close the widget on middle click */
        gtk_widget_destroy (widget);
        return TRUE;
    }

    return FALSE;
}

static void
midori_view_tab_icon_style_set (GtkWidget* tab_icon,
                                GtkStyle*  previous_style)
{
    GtkSettings* gtk_settings;
    gint width, height;

    gtk_settings = gtk_widget_get_settings (tab_icon);
    gtk_icon_size_lookup_for_settings (gtk_settings, GTK_ICON_SIZE_MENU,
                                       &width, &height);
    gtk_widget_set_size_request (tab_icon, width + 2, height + 2);
}

static void
midori_view_tab_close_clicked (GtkWidget* tab_close,
                               GtkWidget* widget)
{
    gtk_widget_destroy (widget);
}

/**
 * midori_view_get_proxy_tab_label:
 * @view: a #MidoriView
 *
 * Retrieves a proxy tab label that is typically used when
 * adding the view to a notebook.
 *
 * The label is created on the first call and will be updated to reflect
 * changes of the loading progress and title.
 *
 * The label is valid until it is removed from its container.
 *
 * Return value: the proxy #GtkEventBox
 **/
GtkWidget*
midori_view_get_proxy_tab_label (MidoriView* view)
{
    GtkWidget* event_box;
    GtkWidget* hbox;
    GtkRcStyle* rcstyle;
    GtkWidget* image;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->tab_label)
    {
        view->tab_icon = katze_throbber_new ();
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (view->tab_icon),
            midori_view_get_icon (view));

        view->tab_title = gtk_label_new (midori_view_get_display_title (view));

        event_box = gtk_event_box_new ();
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
        hbox = gtk_hbox_new (FALSE, 1);
        gtk_container_border_width (GTK_CONTAINER (hbox), 2);
        gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (hbox));
        gtk_misc_set_alignment (GTK_MISC (view->tab_icon), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (hbox), view->tab_icon, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (view->tab_title), 0.0, 0.5);
        /* TODO: make the tab initially look "unvisited" until it's focused */
        gtk_box_pack_start (GTK_BOX (hbox), view->tab_title, FALSE, TRUE, 0);
        _update_label_size (view->tab_title, 10);

        view->tab_close = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (view->tab_close), GTK_RELIEF_NONE);
        gtk_button_set_focus_on_click (GTK_BUTTON (view->tab_close), FALSE);
        rcstyle = gtk_rc_style_new ();
        rcstyle->xthickness = rcstyle->ythickness = 0;
        gtk_widget_modify_style (view->tab_close, rcstyle);
        g_object_unref (rcstyle);
        image = katze_throbber_new ();
        katze_throbber_set_static_stock_id (KATZE_THROBBER (image),
                                            GTK_STOCK_CLOSE);
        gtk_button_set_image (GTK_BUTTON (view->tab_close), image);
        gtk_misc_set_alignment (GTK_MISC (image), 0.0, 0.0);
        gtk_box_pack_end (GTK_BOX (hbox), view->tab_close, FALSE, FALSE, 0);
        gtk_widget_show_all (GTK_WIDGET (event_box));

        if (!view->close_buttons_on_tabs)
            gtk_widget_hide (view->tab_close);

        g_signal_connect (event_box, "button-release-event",
            G_CALLBACK (midori_view_tab_label_button_release_event), view);
        g_signal_connect (view->tab_close, "style-set",
            G_CALLBACK (midori_view_tab_icon_style_set), NULL);
        g_signal_connect (view->tab_close, "clicked",
            G_CALLBACK (midori_view_tab_close_clicked), view);

        view->tab_label = event_box;
        g_signal_connect (view->tab_label, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->tab_label);
    }
    return view->tab_label;
}

/**
 * midori_view_get_proxy_item:
 * @view: a #MidoriView
 *
 * Retrieves a proxy item that can be used for bookmark storage as
 * well as session management.
 *
 * The item is created on the first call and will be updated to reflect
 * changes to the title and uri automatically.
 *
 * Return value: the proxy #KatzeItem
 **/
KatzeItem*
midori_view_get_proxy_item (MidoriView* view)
{
    const gchar* uri;
    const gchar* title;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->item)
    {
        view->item = katze_item_new ();
        uri = midori_view_get_display_uri (view);
        katze_item_set_uri (view->item, uri);
        title = midori_view_get_display_title (view);
        katze_item_set_name (view->item, title);
    }
    return view->item;
}

/**
 * midori_view_get_zoom_level:
 * @view: a #MidoriView
 *
 * Determines the current zoom level of the view.
 *
 * Return value: the current zoom level
 **/
gfloat
midori_view_get_zoom_level (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), 1.0);

    #ifdef WEBKIT_CHECK_VERSION
    if (view->web_view != NULL)
        return webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (view->web_view));
    #endif
    return FALSE;
}

/**
 * midori_view_set_zoom_level:
 * @view: a #MidoriView
 * @zoom_level: the new zoom level
 *
 * Sets the current zoom level of the view.
 **/
void
midori_view_set_zoom_level (MidoriView* view,
                            gfloat      zoom_level)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    #ifdef WEBKIT_CHECK_VERSION
    webkit_web_view_set_zoom_level (
        WEBKIT_WEB_VIEW (view->web_view), zoom_level);
    #endif
}

gboolean
midori_view_can_zoom_in (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    #ifdef WEBKIT_CHECK_VERSION
    return view->web_view != NULL;
    #else
    return FALSE;
    #endif
}

gboolean
midori_view_can_zoom_out (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    #ifdef WEBKIT_CHECK_VERSION
    return view->web_view != NULL;
    #else
    return FALSE;
    #endif
}

#define can_do(what) \
gboolean \
midori_view_can_##what (MidoriView* view) \
{ \
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE); \
\
    return view->web_view != NULL; \
}

can_do (reload)
can_do (print)
#if HAVE_GIO
    can_do (view_source)
#else
    gboolean midori_view_can_view_source (MidoriView* view)
    {
        return view->web_view != NULL;
    }
#endif
can_do (find)

/**
 * midori_view_reload:
 * @view: a #MidoriView
 * @from_cache: whether to allow caching
 *
 * Reloads the view.
 *
 * Note: The @from_cache value is currently ignored.
 **/
void
midori_view_reload (MidoriView* view,
                    gboolean    from_cache)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_reload (WEBKIT_WEB_VIEW (view->web_view));
}

/**
 * midori_view_stop_loading
 * @view: a #MidoriView
 *
 * Stops loading the view if it is currently loading.
 **/
void
midori_view_stop_loading (MidoriView* view)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (view->web_view));
}

/**
 * midori_view_can_go_back
 * @view: a #MidoriView
 *
 * Determines whether the view can go back.
 **/
gboolean
midori_view_can_go_back (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_go_back (WEBKIT_WEB_VIEW (view->web_view));
    else
        return FALSE;
}

/**
 * midori_view_go_back
 * @view: a #MidoriView
 *
 * Goes back one page in the view.
 **/
void
midori_view_go_back (MidoriView* view)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_go_back (WEBKIT_WEB_VIEW (view->web_view));
}

/**
 * midori_view_can_go_forward
 * @view: a #MidoriView
 *
 * Determines whether the view can go forward.
 **/
gboolean
midori_view_can_go_forward (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_go_forward (WEBKIT_WEB_VIEW (view->web_view));
    else
        return FALSE;
}

/**
 * midori_view_go_forward
 * @view: a #MidoriView
 *
 * Goes forward one page in the view.
 **/
void
midori_view_go_forward (MidoriView* view)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_go_forward (WEBKIT_WEB_VIEW (view->web_view));
}

/**
 * midori_view_print
 * @view: a #MidoriView
 *
 * Prints the contents of the view.
 **/
void
midori_view_print (MidoriView* view)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    #ifdef WEBKIT_CHECK_VERSION
    webkit_web_frame_print (webkit_web_view_get_main_frame (
        WEBKIT_WEB_VIEW (view->web_view)));
    #else
    webkit_web_view_execute_script (
        WEBKIT_WEB_VIEW (view->web_view), "print();");
    #endif
}

/**
 * midori_view_unmark_text_matches
 * @view: a #MidoriView
 *
 * Unmarks the text matches in the view.
 **/
void
midori_view_unmark_text_matches (MidoriView* view)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (view->web_view));
}

/**
 * midori_view_search_text
 * @view: a #MidoriView
 * @text: a string
 * @case_sensitive: case sensitivity
 * @forward: whether to search forward
 *
 * Searches a text within the view.
 **/
void
midori_view_search_text (MidoriView*  view,
                         const gchar* text,
                         gboolean     case_sensitive,
                         gboolean     forward)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    g_signal_emit (view, signals[SEARCH_TEXT], 0,
        webkit_web_view_search_text (WEBKIT_WEB_VIEW (view->web_view),
            text, case_sensitive, forward, TRUE));
}

/**
 * midori_view_mark_text_matches
 * @view: a #MidoriView
 * @text: a string
 * @case_sensitive: case sensitivity
 *
 * Marks all text matches within the view.
 **/
void
midori_view_mark_text_matches (MidoriView*  view,
                               const gchar* text,
                               gboolean     case_sensitive)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_mark_text_matches (WEBKIT_WEB_VIEW (view->web_view),
        text, case_sensitive, 0);
}

/**
 * midori_view_set_highlight_text_matches
 * @view: a #MidoriView
 * @highlight: whether to highlight matches
 *
 * Whether to highlight all matches within the view.
 **/
void
midori_view_set_highlight_text_matches (MidoriView* view,
                                        gboolean    highlight)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_set_highlight_text_matches (
        WEBKIT_WEB_VIEW (view->web_view), highlight);
}
