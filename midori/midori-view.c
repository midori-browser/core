/*
 Copyright (C) 2007-2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Jean-François Guchens <zcx000@gmail.com>

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
#include "midori-stock.h"
#include "midori-browser.h"

#include "marshal.h"
#include "sokoke.h"

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#ifndef G_OS_WIN32
    #include <sys/utsname.h>
#endif

/* This is unstable API, so we need to declare it */
gchar*
webkit_web_view_get_selected_text (WebKitWebView* web_view);
/* This is public API since WebKitGTK+ 1.1.6 */
#if !WEBKIT_CHECK_VERSION (1, 1, 6)
void
webkit_web_frame_print (WebKitWebFrame* web_frame);
#endif

GdkPixbuf*
midori_search_action_get_icon (KatzeItem*    item,
                               GtkWidget*    widget,
                               const gchar** icon_name);

static void
midori_view_construct_web_view (MidoriView* view);

static void
midori_view_item_meta_data_changed (KatzeItem*   item,
                                    const gchar* key,
                                    MidoriView*  view);

struct _MidoriView
{
    GtkVBox parent_instance;

    gchar* uri;
    gboolean special;
    gchar* title;
    MidoriSecurity security;
    gchar* mime_type;
    GdkPixbuf* icon;
    gchar* icon_uri;
    gdouble progress;
    MidoriLoadStatus load_status;
    gboolean minimized;
    gchar* statusbar_text;
    #if WEBKIT_CHECK_VERSION (1, 1, 15)
    WebKitHitTestResult* hit_test;
    #endif
    gchar* link_uri;
    gboolean has_selection;
    gchar* selected_text;
    MidoriWebSettings* settings;
    GtkWidget* web_view;
    GtkWidget* thumb_view;
    KatzeArray* news_feeds;

    gboolean speed_dial_in_new_tabs;
    gchar* download_manager;
    gchar* news_aggregator;
    gboolean ask_for_destination_folder;
    gboolean middle_click_opens_selection;
    gboolean open_tabs_in_the_background;
    gboolean close_buttons_on_tabs;
    MidoriNewPage open_new_pages_in;
    gboolean find_while_typing;

    GtkWidget* menu_item;
    GtkWidget* tab_label;
    /* GtkWidget* tooltip_image; */
    GtkWidget* tab_icon;
    GtkWidget* tab_title;
    GtkWidget* tab_close;
    KatzeItem* item;
    gint scrollh, scrollv;
    gboolean back_forward_set;

    KatzeNet* net;
    GHashTable* memory;

    GtkWidget* scrolled_window;
};

struct _MidoriViewClass
{
    GtkVBoxClass parent_class;
};

G_DEFINE_TYPE (MidoriView, midori_view, GTK_TYPE_VBOX);

GType
midori_load_status_get_type (void)
{
    static GType type = 0;
    static const GEnumValue values[] = {
     { MIDORI_LOAD_PROVISIONAL, "MIDORI_LOAD_PROVISIONAL", "Load Provisional" },
     { MIDORI_LOAD_COMMITTED, "MIDORI_LOAD_COMMITTED", "Load Committed" },
     { MIDORI_LOAD_FINISHED, "MIDORI_LOAD_FINISHED", "Load Finished" },
     { 0, NULL, NULL }
    };

    if (type)
        return type;

    type = g_enum_register_static ("MidoriLoadStatus", values);
    return type;
}

GType
midori_new_view_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_NEW_VIEW_TAB, "MIDORI_NEW_VIEW_TAB", "New view in a tab" },
         { MIDORI_NEW_VIEW_BACKGROUND, "MIDORI_NEW_VIEW_BACKGROUND",
             "New view in a background tab" },
         { MIDORI_NEW_VIEW_WINDOW, "MIDORI_NEW_VIEW_WINDOW",
             "New view in a window" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriNewView", values);
    }
    return type;
}

GType
midori_security_get_type (void)
{
    static GType type = 0;
    if (!type)
    {
        static const GEnumValue values[] = {
         { MIDORI_SECURITY_NONE, "MIDORI_SECURITY_NONE", "No security" },
         { MIDORI_SECURITY_UNKNOWN, "MIDORI_SECURITY_UNKNOWN", "Security unknown" },
         { MIDORI_SECURITY_TRUSTED, "MIDORI_SECURITY_TRUSTED", "Trusted security" },
         { 0, NULL, NULL }
        };
        type = g_enum_register_static ("MidoriSecurity", values);
    }
    return type;
}

enum
{
    PROP_0,

    PROP_URI,
    PROP_TITLE,
    PROP_SECURITY,
    PROP_MIME_TYPE,
    PROP_ICON,
    PROP_LOAD_STATUS,
    PROP_PROGRESS,
    PROP_MINIMIZED,
    PROP_ZOOM_LEVEL,
    PROP_NEWS_FEEDS,
    PROP_STATUSBAR_TEXT,
    PROP_SETTINGS,
    PROP_NET
};

enum {
    ACTIVATE_ACTION,
    CONSOLE_MESSAGE,
    CONTEXT_READY,
    ATTACH_INSPECTOR,
    NEW_TAB,
    NEW_WINDOW,
    NEW_VIEW,
    DOWNLOAD_REQUESTED,
    SEARCH_TEXT,
    ADD_BOOKMARK,
    SAVE_AS,
    ADD_SPEED_DIAL,

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

static gboolean
midori_view_focus_in_event (GtkWidget*     widget,
                            GdkEventFocus* event);

static void
midori_view_settings_notify_cb (MidoriWebSettings* settings,
                                GParamSpec*        pspec,
                                MidoriView*        view);

static void
midori_view_speed_dial_get_thumb (GtkWidget*   web_view,
                                 const gchar* message,
                                 MidoriView*  view);

static void
midori_view_speed_dial_save (GtkWidget*   web_view,
                             const gchar* message);

static void
midori_view_class_init (MidoriViewClass* class)
{
    GObjectClass* gobject_class;
    GtkWidgetClass* gtkwidget_class;
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

    signals[CONTEXT_READY] = g_signal_new (
        "context-ready",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    signals[ATTACH_INSPECTOR] = g_signal_new (
        "attach-inspector",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_WIDGET);

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

    /**
     * MidoriView::new-view:
     * @view: the object on which the signal is emitted
     * @new_view: a newly created view
     * @where: where to open the view
     *
     * Emitted when a new view is created. The value of
     * @where determines where to open the view according
     * to how it was opened and user preferences.
     *
     * Since: 0.1.2
     */
    signals[NEW_VIEW] = g_signal_new (
        "new-view",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__OBJECT_ENUM,
        G_TYPE_NONE, 2,
        MIDORI_TYPE_VIEW,
        MIDORI_TYPE_NEW_VIEW);

    /**
     * MidoriView::download-requested:
     * @view: the object on which the signal is emitted
     * @download: a new download
     *
     * Emitted when a new download is requested, if a
     * file cannot be displayed or a download was started
     * from the context menu.
     *
     * If the download should be accepted, a callback
     * has to return %TRUE, and the download will also
     * be started automatically.
     *
     * Note: This requires WebKitGTK 1.1.3.
     *
     * Return value: %TRUE if the download was handled
     *
     * Since: 0.1.5
     */
    signals[DOWNLOAD_REQUESTED] = g_signal_new (
        "download-requested",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        g_signal_accumulator_true_handled,
        NULL,
        midori_cclosure_marshal_BOOLEAN__OBJECT,
        G_TYPE_BOOLEAN, 1,
        G_TYPE_OBJECT);

    /**
     * MidoriView::search-text:
     * @view: the object on which the signal is emitted
     * @found: whether the search was successful
     * @typing: whether the search was initiated by typing
     *
     * Emitted when a search is performed. Either manually
     * invoked or automatically by typing. The value of typing
     * is actually the text the user typed.
     *
     * Note that in 0.1.3 the argument @typing was introduced.
     */
    signals[SEARCH_TEXT] = g_signal_new (
        "search-text",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__BOOLEAN_STRING,
        G_TYPE_NONE, 2,
        G_TYPE_BOOLEAN,
        G_TYPE_STRING);

    /**
     * MidoriView::add-bookmark:
     * @view: the object on which the signal is emitted
     * @uri: the bookmark URI
     *
     * Emitted when a bookmark is added.
     *
     * Deprecated: 0.2.7
     */
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

    signals[SAVE_AS] = g_signal_new (
        "save-as",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    /**
     * MidoriView::add-speed-dial:
     * @view: the object on which the signal is emitted
     * @uri: the URI to add to the speed dial
     *
     * Emitted when an URI is added to the spee dial page.
     *
     * Since: 0.1.7
     */
    signals[ADD_SPEED_DIAL] = g_signal_new (
        "add-speed-dial",
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

    gtkwidget_class = GTK_WIDGET_CLASS (class);
    gtkwidget_class->focus_in_event = midori_view_focus_in_event;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS;

    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     "Uri",
                                     "The URI of the currently loaded page",
                                     "about:blank",
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_TITLE,
                                     g_param_spec_string (
                                     "title",
                                     "Title",
                                     "The title of the currently loaded page",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * MidoriView:security:
     *
     * The security status of the loaded page.
     *
     * Since: 0.2.5
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SECURITY,
                                     g_param_spec_enum (
                                     "security",
                                     "Security",
                                     "The security of the currently loaded page",
                                     MIDORI_TYPE_SECURITY,
                                     MIDORI_SECURITY_NONE,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriView:mime-type:
    *
    * The MIME type of the currently loaded page.
    *
    * Since: 0.1.2
    */
    g_object_class_install_property (gobject_class,
                                     PROP_MIME_TYPE,
                                     g_param_spec_string (
                                     "mime-type",
                                     "MIME Type",
                                     "The MIME type of the currently loaded page",
                                     "text/html",
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON,
                                     g_param_spec_object (
                                     "icon",
                                     "Icon",
                                     "The icon of the view",
                                     GDK_TYPE_PIXBUF,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_LOAD_STATUS,
                                     g_param_spec_enum (
                                     "load-status",
                                     "Load Status",
                                     "The current loading status",
                                     MIDORI_TYPE_LOAD_STATUS,
                                     MIDORI_LOAD_FINISHED,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_PROGRESS,
                                     g_param_spec_double (
                                     "progress",
                                     "Progress",
                                     "The current loading progress",
                                     0.0, 1.0, 0.0,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriView:minimized:
    *
    * Whether the view is minimized or in normal state.
    *
    * Minimizing a view indicates that only the icon should
    * be advertised rather than the full blown tab label and
    * it might otherwise be presented specially.
    *
    * Since: 0.1.8
    */
    g_object_class_install_property (gobject_class,
                                     PROP_MINIMIZED,
                                     g_param_spec_boolean (
                                     "minimized",
                                     "Minimized",
                                     "Whether the view is minimized or in normal state",
                                     FALSE,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_ZOOM_LEVEL,
                                     g_param_spec_float (
                                     "zoom-level",
                                     "Zoom Level",
                                     "The current zoom level",
                                     G_MINFLOAT,
                                     G_MAXFLOAT,
                                     1.0f,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriView:news-feeds:
    *
    * The news feeds advertised by the currently loaded page.
    *
    * Since: 0.1.7
    */
    g_object_class_install_property (gobject_class,
                                     PROP_NEWS_FEEDS,
                                     g_param_spec_object (
                                     "news-feeds",
                                     "News Feeds",
                                     "The list of available news feeds",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR_TEXT,
                                     g_param_spec_string (
                                     "statusbar-text",
                                     "Statusbar Text",
                                     "The text displayed in the statusbar",
                                     "",
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "The associated settings",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_NET,
                                     g_param_spec_object (
                                     "net",
                                     "Net",
                                     "The associated net",
                                     KATZE_TYPE_NET,
                                     flags));
}

static void
midori_view_update_title (MidoriView* view)
{
    #ifndef G_OS_WIN32
    /* If left-to-right text is combined with right-to-left text the default
       behaviour of Pango can result in awkwardly aligned text. For example
       "‪بستيان نوصر (hadess) | An era comes to an end - Midori" becomes
       "hadess) | An era comes to an end - Midori) بستيان نوصر". So to prevent
       this we insert an LRE character before the title which indicates that
       we want left-to-right but retains the direction of right-to-left text. */
    if (view->title && !g_str_has_prefix (view->title, "‪"))
    {
        gchar* new_title = g_strconcat ("‪", view->title, NULL);
        katze_assign (view->title, new_title);
    }
    #endif
    #define title midori_view_get_display_title (view)
    if (view->tab_label)
    {
        /* If the title starts with the presumed name of the website, we
            ellipsize differently, to emphasize the subtitle */
        if (gtk_label_get_angle (GTK_LABEL (view->tab_title)) == 0.0)
        {
            SoupURI* uri = soup_uri_new (view->uri);
            const gchar* host = uri ? (uri->host ? uri->host : "") : "";
            const gchar* name = g_str_has_prefix (host, "www.") ? &host[4] : host;
            guint i = 0;
            while (name[i++])
                if (name[i] == '.')
                    break;
            if (!g_ascii_strncasecmp (title, name, i))
                gtk_label_set_ellipsize (GTK_LABEL (view->tab_title), PANGO_ELLIPSIZE_START);
            else
                gtk_label_set_ellipsize (GTK_LABEL (view->tab_title), PANGO_ELLIPSIZE_END);
            if (uri)
                soup_uri_free (uri);
        }
        gtk_label_set_text (GTK_LABEL (view->tab_title), title);
        #if 1
        gtk_widget_set_tooltip_text (view->tab_title, title);
        #endif
    }
    if (view->menu_item)
        gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (
                            view->menu_item))), title);
    if (view->item)
        katze_item_set_name (view->item, title);
    #undef title
}

static void
midori_view_apply_icon (MidoriView*  view,
                        GdkPixbuf*   icon,
                        const gchar* icon_name)
{
    if (view->item)
        katze_item_set_icon (view->item, icon_name);
    katze_object_assign (view->icon, icon);
    g_object_notify (G_OBJECT (view), "icon");

    if (view->tab_icon)
    {
        if (icon_name)
            katze_throbber_set_static_icon_name (KATZE_THROBBER (view->tab_icon),
                                                 icon_name);
        else
            katze_throbber_set_static_pixbuf (KATZE_THROBBER (view->tab_icon),
                                              view->icon);
    }
    if (view->menu_item)
    {
        GtkWidget* image;
        if (icon_name)
            image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
        else
            image = gtk_image_new_from_pixbuf (view->icon);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (view->menu_item), image);
    }
}

static gboolean
midori_view_mime_icon (MidoriView*   view,
                       GtkIconTheme* icon_theme,
                       const gchar*  format,
                       const gchar*  part1,
                       const gchar*  part2)
{
    gchar* icon_name;
    GdkPixbuf* icon;

    icon_name = part2 ? g_strdup_printf (format, part1, part2)
        : g_strdup_printf (format, part1);
    if (!(icon = gtk_icon_theme_load_icon (icon_theme, icon_name, 16, 0, NULL)))
    {
        g_free (icon_name);
        return FALSE;
    }

    g_object_ref (icon);
    midori_view_apply_icon (view, icon, icon_name);
    g_free (icon_name);
    return TRUE;
}

static void
midori_view_update_icon (MidoriView* view,
                         GdkPixbuf*  icon)
{
    GdkScreen* screen;
    GtkIconTheme* theme;
    gchar** parts = NULL;

    if (icon)
    {
        midori_view_apply_icon (view, icon, NULL);
        return;
    }

    if (!((screen = gtk_widget_get_screen (GTK_WIDGET (view)))
        && (theme = gtk_icon_theme_get_for_screen (screen))))
        return;

    if (!((parts = g_strsplit (view->mime_type, "/", 2)) && (*parts && parts[1])))
    {
        g_strfreev (parts);
        /* This is a hack to have a Find icon in the location while the
           blank page has a File icon. */
        icon = gtk_widget_render_icon (GTK_WIDGET (view),
            GTK_STOCK_FIND, GTK_ICON_SIZE_MENU, NULL);
        midori_view_apply_icon (view, icon, GTK_STOCK_FILE);
        return;
    }

    if (midori_view_mime_icon (view, theme, "%s-%s", *parts, parts[1]))
        return;
    if (midori_view_mime_icon (view, theme, "gnome-mime-%s-%s", *parts, parts[1]))
        return;
    if (midori_view_mime_icon (view, theme, "%s-x-generic", *parts, NULL))
        return;
    if (midori_view_mime_icon (view, theme, "gnome-mime-%s-x-generic", *parts, NULL))
        return;

    icon = gtk_widget_render_icon (GTK_WIDGET (view),
        GTK_STOCK_FILE, GTK_ICON_SIZE_MENU, NULL);
    midori_view_apply_icon (view, icon, NULL);
}

static void
midori_view_icon_cb (GdkPixbuf*  icon,
                     MidoriView* view)
{
    midori_view_update_icon (view, icon);
}

typedef void (*KatzeNetIconCb) (GdkPixbuf*  icon,
                                MidoriView* view);

typedef struct
{
    gchar* icon_file;
    gchar* icon_uri;
    MidoriView* view;
} KatzeNetIconPriv;

void
katze_net_icon_priv_free (KatzeNetIconPriv* priv)
{
    g_free (priv->icon_file);
    g_free (priv->icon_uri);
    g_free (priv);
}

gboolean
katze_net_icon_status_cb (KatzeNetRequest*  request,
                          KatzeNetIconPriv* priv)
{
    switch (request->status)
    {
    case KATZE_NET_VERIFIED:
        if (request->mime_type && strncmp (request->mime_type, "image/", 6))
        {
            katze_net_icon_priv_free (priv);
            return FALSE;
        }
        break;
    case KATZE_NET_MOVED:
        break;
    default:
        katze_net_icon_priv_free (priv);
        return FALSE;
    }

    return TRUE;
}

void
katze_net_icon_transfer_cb (KatzeNetRequest*  request,
                            KatzeNetIconPriv* priv)
{
    GdkPixbuf* pixbuf;
    FILE* fp;
    GdkPixbuf* pixbuf_scaled;
    gint icon_width, icon_height;
    size_t ret;
    GtkSettings* settings;

    if (request->status == KATZE_NET_MOVED)
        return;

    pixbuf = NULL;
    if (request->data)
    {
        if ((fp = fopen (priv->icon_file, "wb")))
        {
            ret = fwrite (request->data, 1, request->length, fp);
            fclose (fp);
            if ((ret - request->length) != 0)
            {
                g_warning ("Error writing to file %s "
                           "in katze_net_icon_transfer_cb()", priv->icon_file);
            }
            pixbuf = gdk_pixbuf_new_from_file (priv->icon_file, NULL);
        }
        else
            pixbuf = katze_pixbuf_new_from_buffer ((guchar*)request->data,
                request->length, request->mime_type, NULL);
        if (pixbuf)
            g_object_ref (pixbuf);
        g_hash_table_insert (priv->view->memory,
            g_strdup (priv->icon_file), pixbuf);
    }

    if (!pixbuf)
    {
        midori_view_icon_cb (NULL, priv->view);
        katze_net_icon_priv_free (priv);
        return;
    }

    settings = gtk_widget_get_settings (priv->view->web_view);
    gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
                                       &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    katze_assign (priv->view->icon_uri, g_strdup (priv->icon_uri));
    midori_view_icon_cb (pixbuf_scaled, priv->view);
    katze_net_icon_priv_free (priv);
}


static void
_midori_web_view_load_icon (MidoriView* view)
{
    GdkPixbuf* pixbuf;
    KatzeNetIconPriv* priv;
    gchar* icon_uri;
    gchar* icon_file;
    gint icon_width, icon_height;
    GdkPixbuf* pixbuf_scaled;
    GtkSettings* settings;

    pixbuf = NULL;
    icon_uri = g_strdup (view->icon_uri);

    if ((icon_uri && g_str_has_prefix (icon_uri, "http"))
        || g_str_has_prefix (view->uri, "http"))
    {
        if (!icon_uri)
        {
            guint i = 8;
            while (view->uri[i] != '\0' && view->uri[i] != '/')
                i++;
            if (view->uri[i] == '/')
            {
                icon_uri = g_strdup (view->uri);
                icon_uri[i] = '\0';
                icon_uri = g_strdup_printf ("%s/favicon.ico", icon_uri);
            }
            else
                icon_uri = g_strdup_printf ("%s/favicon.ico", view->uri);
        }

        icon_file = katze_net_get_cached_path (NULL, icon_uri, "icons");
        if (g_hash_table_lookup_extended (view->memory,
                                          icon_file, NULL, (gpointer)&pixbuf))
        {
            g_free (icon_file);
            if (pixbuf)
            {
                g_object_ref (pixbuf);
                katze_assign (view->icon_uri, icon_uri);
            }
        }
        else if ((pixbuf = gdk_pixbuf_new_from_file (icon_file, NULL)))
        {
            g_free (icon_file);
            katze_assign (view->icon_uri, icon_uri);
        }
        else
        {
            priv = g_new0 (KatzeNetIconPriv, 1);
            priv->icon_file = icon_file;
            priv->icon_uri = icon_uri;
            priv->view = view;

            katze_net_load_uri (NULL, icon_uri,
                (KatzeNetStatusCb)katze_net_icon_status_cb,
                (KatzeNetTransferCb)katze_net_icon_transfer_cb, priv);
        }
    }

    if (pixbuf)
    {
        settings = gtk_widget_get_settings (view->web_view);
        gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
                                           &icon_width, &icon_height);
        pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width,
            icon_height, GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        pixbuf = pixbuf_scaled;
    }

    midori_view_update_icon (view, pixbuf);
}

static void
midori_view_update_load_status (MidoriView*      view,
                                MidoriLoadStatus load_status)
{
    if (view->load_status == load_status)
        return;

    if (load_status == MIDORI_LOAD_FINISHED)
        view->special = FALSE;

    view->load_status = load_status;
    g_object_notify (G_OBJECT (view), "load-status");

    if (view->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (view->tab_icon),
            view->load_status != MIDORI_LOAD_FINISHED);
}

static gboolean
midori_view_web_view_navigation_decision_cb (WebKitWebView*             web_view,
                                             WebKitWebFrame*            web_frame,
                                             WebKitNetworkRequest*      request,
                                             WebKitWebNavigationAction* action,
                                             WebKitWebPolicyDecision*   decision,
                                             MidoriView*                view)
{
    const gchar* uri = webkit_network_request_get_uri (request);
    if (g_str_has_prefix (uri, "mailto:") || g_str_has_prefix (uri, "tel:"))
    {
        if (sokoke_show_uri (gtk_widget_get_screen (GTK_WIDGET (web_view)),
                             uri, GDK_CURRENT_TIME, NULL))
        {
            webkit_web_policy_decision_ignore (decision);
            return TRUE;
        }
    }
    view->special = FALSE;
    return FALSE;
}

static void
webkit_web_view_load_started_cb (WebKitWebView*  web_view,
                                 WebKitWebFrame* web_frame,
                                 MidoriView*     view)
{
    g_object_freeze_notify (G_OBJECT (view));

    midori_view_update_load_status (view, MIDORI_LOAD_PROVISIONAL);
    view->progress = 0.0;
    g_object_notify (G_OBJECT (view), "progress");

    g_object_thaw_notify (G_OBJECT (view));
}

static void
webkit_web_view_load_committed_cb (WebKitWebView*  web_view,
                                   WebKitWebFrame* web_frame,
                                   MidoriView*     view)
{
    const gchar* uri;

    g_object_freeze_notify (G_OBJECT (view));

    uri = webkit_web_frame_get_uri (web_frame);
    g_return_if_fail (uri != NULL);
    katze_assign (view->uri, sokoke_format_uri_for_display (uri));
    katze_assign (view->icon_uri, NULL);
    if (view->item)
    {
        #if 0
        /* Load back forward history from meta data. WebKit does not seem to
          respect the order of items, so the feature is unusable. */
        if (!view->back_forward_set)
        {
            WebKitWebBackForwardList* list;
            gchar* key;
            guint i;
            const gchar* data;
            WebKitWebHistoryItem* item;

            list = webkit_web_view_get_back_forward_list (web_view);

            key = g_strdup ("back4");
            for (i = 4; i > 0; i--)
            {
                key[4] = 48 + i;
                if ((data = katze_item_get_meta_string (view->item, key)))
                {
                    item = webkit_web_history_item_new_with_data (data, NULL);
                    webkit_web_back_forward_list_add_item (list, item);
                    g_object_unref (item);
                }
            }

            #if 0
            key[0] = 'f';
            key[1] = 'o';
            key[2] = 'r';
            key[3] = 'e';
            for (i = 4; i > 0; i--)
            {
                key[4] = 48 + i;
                item = webkit_web_history_item_new_with_data (data, NULL);
                webkit_web_back_forward_list_add_item (list, item);
                g_object_unref (item);
            }
            #endif
            g_free (key);
            view->back_forward_set = TRUE;
        }
        #endif

        katze_item_set_uri (view->item, uri);
        katze_item_set_added (view->item, time (NULL));
    }
    g_object_notify (G_OBJECT (view), "uri");
    g_object_set (view, "title", NULL, NULL);

    midori_view_update_icon (view, NULL);

    if (!strncmp (uri, "https", 5))
    {
#if WEBKIT_CHECK_VERSION (1, 1, 14) && defined (HAVE_LIBSOUP_2_29_91)
        WebKitWebDataSource *source;
        WebKitNetworkRequest *request;
        SoupMessage *message;

        source = webkit_web_frame_get_data_source (web_frame);
        request = webkit_web_data_source_get_request (source);
        message = webkit_network_request_get_message (request);

        if (message
         && soup_message_get_flags (message) & SOUP_MESSAGE_CERTIFICATE_TRUSTED)
            view->security = MIDORI_SECURITY_TRUSTED;
        else
#endif
            view->security = MIDORI_SECURITY_UNKNOWN;
    }
    else
        view->security = MIDORI_SECURITY_NONE;
    g_object_notify (G_OBJECT (view), "security");

    midori_view_update_load_status (view, MIDORI_LOAD_COMMITTED);

    g_object_thaw_notify (G_OBJECT (view));

}

static void
webkit_web_view_progress_changed_cb (WebKitWebView* web_view,
                                     gint           progress,
                                     MidoriView*    view)
{
    view->progress = progress ? progress / 100.0 : 0.0;
    g_object_notify (G_OBJECT (view), "progress");
}

#if WEBKIT_CHECK_VERSION (1, 1, 6)
#if WEBKIT_CHECK_VERSION (1, 1, 14)
static void
midori_view_web_view_resource_request_cb (WebKitWebView*         web_view,
                                          WebKitWebFrame*        web_frame,
                                          WebKitWebResource*     web_resource,
                                          WebKitNetworkRequest*  request,
                                          WebKitNetworkResponse* response,
                                          MidoriView*            view)
{
    const gchar* uri = webkit_network_request_get_uri (request);

    /* Only apply custom URIs to special pages for security purposes */
    if (!view->special)
        return;

    if (g_str_has_prefix (uri, "res://"))
    {
        gchar* filename = g_build_filename ("midori/res", &uri[5], NULL);
        gchar* filepath = sokoke_find_data_filename (filename);
        gchar* file_uri;

        g_free (filename);
        file_uri = g_filename_to_uri (filepath, NULL, NULL);
        g_free (filepath);
        webkit_network_request_set_uri (request, file_uri);
        g_free (file_uri);
    }
    else if (g_str_has_prefix (uri, "stock://"))
    {
        GdkPixbuf* pixbuf;
        const gchar* icon_name = &uri[8] ? &uri[8] : "";
        gint icon_size = GTK_ICON_SIZE_MENU;

        if (g_ascii_isalpha (icon_name[0]))
            icon_size = strstr (icon_name, "dialog") ?
                GTK_ICON_SIZE_DIALOG : GTK_ICON_SIZE_BUTTON;
        else if (g_ascii_isdigit (icon_name[0]))
        {
            guint i = 0;
            while (icon_name[i])
                if (icon_name[i++] == '/')
                {
                    gchar* size = g_strndup (icon_name, i - 1);
                    icon_size = atoi (size);
                    /* Compatibility: map pixel to symbolic size */
                    if (icon_size == 16)
                        icon_size = GTK_ICON_SIZE_MENU;
                    g_free (size);
                    icon_name = &icon_name[i];
                }
        }

        pixbuf = gtk_widget_render_icon (GTK_WIDGET (view), icon_name, icon_size, NULL);
        if (!pixbuf)
            pixbuf = gtk_widget_render_icon (GTK_WIDGET (view),
                GTK_STOCK_MISSING_IMAGE, icon_size, NULL);
        if (pixbuf)
        {
            gboolean success;
            gchar* buffer;
            gsize buffer_size;
            gchar* encoded;
            gchar* data_uri;

            success = gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &buffer_size, "png", NULL, NULL);
            g_object_unref (pixbuf);
            if (!success)
                return;

            encoded = g_base64_encode ((guchar*)buffer, buffer_size);
            g_free (buffer);
            data_uri = g_strconcat ("data:image/png;base64,", encoded, NULL);
            g_free (encoded);
            webkit_network_request_set_uri (request, data_uri);
            g_free (data_uri);
            return;
        }
    }
}
#endif

static void
midori_view_load_alternate_string (MidoriView*     view,
                                   const gchar*    data,
                                   const gchar*    res_root,
                                   const gchar*    uri,
                                   WebKitWebFrame* web_frame)
{
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    if (!web_frame)
        web_frame = webkit_web_view_get_main_frame (web_view);
    view->special = TRUE;
    #if WEBKIT_CHECK_VERSION (1, 1, 14)
    webkit_web_frame_load_alternate_string (
        web_frame, data, uri, uri);
    #elif WEBKIT_CHECK_VERSION (1, 1, 6)
    webkit_web_frame_load_alternate_string (
        web_frame, data, res_root, uri);
    #else
    webkit_web_view_load_html_string (
        web_view, data, res_root);
    #endif
}

static gboolean
midori_view_display_error (MidoriView*     view,
                           const gchar*    uri,
                           const gchar*    title,
                           const gchar*    message,
                           const gchar*    description,
                           const gchar*    try_again,
                           WebKitWebFrame* web_frame)
{
    gchar* template_file = g_build_filename ("midori", "res", "error.html", NULL);
    gchar* path = sokoke_find_data_filename (template_file);
    gchar* template;

    g_free (template_file);
    if (g_file_get_contents (path, &template, NULL, NULL))
    {
        #if !WEBKIT_CHECK_VERSION (1, 1, 14)
        SoupServer* res_server;
        guint port;
        #endif
        gchar* res_root;
        gchar* stock_root;
        gchar* result;

        #if WEBKIT_CHECK_VERSION (1, 1, 14)
        res_root = g_strdup ("res:/");
        stock_root = g_strdup ("stock:/");
        #else
        res_server = sokoke_get_res_server ();
        port = soup_server_get_port (res_server);
        res_root = g_strdup_printf ("http://localhost:%d/res", port);
        stock_root = g_strdup_printf ("http://localhost:%d/stock", port);
        #endif

        result = sokoke_replace_variables (template,
            "{title}", title,
            "{message}", message,
            "{description}", description,
            "{tryagain}", try_again,
            "{res}", res_root,
            "{stock}", stock_root,
            NULL);
        g_free (template);

        midori_view_load_alternate_string (view,
            result, res_root, uri, web_frame);

        g_free (res_root);
        g_free (stock_root);
        g_free (result);
        g_free (path);

        return TRUE;
    }
    g_free (path);

    return FALSE;
}

static gboolean
webkit_web_view_load_error_cb (WebKitWebView*  web_view,
                               WebKitWebFrame* web_frame,
                               const gchar*    uri,
                               GError*         error,
                               MidoriView*     view)
{
    gchar* title = g_strdup_printf (_("Error - %s"), uri);
    gchar* message = g_strdup_printf (_("The page '%s' couldn't be loaded."), uri);
    gboolean result = midori_view_display_error (view, uri, title,
        message, error->message, _("Try again"), web_frame);
    g_free (message);
    g_free (title);
    return result;
}
#else
static void
webkit_web_frame_load_done_cb (WebKitWebFrame* web_frame,
                               gboolean        success,
                               MidoriView*     view)
{
    gchar* title;
    gchar* data;
    gchar* logo_path;
    gchar* logo_uri;

    if (!success)
    {
        /* i18n: The title of the 404 - Not found error page */
        title = g_strdup_printf (_("Not found - %s"), view->uri);
        katze_assign (view->title, title);
        logo_path = sokoke_find_data_filename ("midori/logo-shade.png");
        logo_uri = g_filename_to_uri (logo_path, NULL, NULL);
        g_free (logo_path);
        data = g_strdup_printf (
            "<html><head><title>%s</title></head>"
            "<body><h1>%s</h1>"
            "<img src=\"%s\" "
            "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
            "<p />The page you were opening doesn't exist."
            "<p />Try to <a href=\"%s\">load the page again</a>, "
            "or move on to another page."
            "</body></html>",
            title, title, logo_uri, view->uri);
        webkit_web_view_load_html_string (
            WEBKIT_WEB_VIEW (view->web_view), data, view->uri);
        g_free (title);
        g_free (data);
        g_free (logo_uri);
    }

    midori_view_update_load_status (view, MIDORI_LOAD_FINISHED);
}
#endif

static void
midori_view_apply_scroll_position (MidoriView* view)
{
    if (view->scrollh > -2)
    {
        if (view->scrollh > 0)
        {
            GtkAdjustment* adjustment = katze_object_get_object (view->scrolled_window, "hadjustment");
            gtk_adjustment_set_value (adjustment, view->scrollh);
            g_object_unref (adjustment);
        }
        view->scrollh = -3;
    }
    if (view->scrollv > -2)
    {
        if (view->scrollv > 0)
        {
            GtkAdjustment* adjustment = katze_object_get_object (view->scrolled_window, "vadjustment");
            gtk_adjustment_set_value (adjustment, view->scrollv);
            g_object_unref (adjustment);
        }
        view->scrollv = -3;
    }
}

static void
webkit_web_view_load_finished_cb (WebKitWebView*  web_view,
                                  WebKitWebFrame* web_frame,
                                  MidoriView*     view)
{
    g_object_freeze_notify (G_OBJECT (view));

    /* TODO: Find a better condition than a finished load.
      Apparently WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT is too early. */
    midori_view_apply_scroll_position (view);

    view->progress = 1.0;
    g_object_notify (G_OBJECT (view), "progress");
    midori_view_update_load_status (view, MIDORI_LOAD_FINISHED);

    if (1)
    {
        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
        /* This snippet joins the available news feeds into a string like this:
           URI1|title1,URI2|title2
           FIXME: Ensure separators contained in the string can't break it */
        gchar* value = sokoke_js_script_eval (js_context,
        "(function (l) { var f = new Array (); for (i in l) "
        "{ var t = l[i].type; var r = l[i].rel; "
        "if (t && (t.indexOf ('rss') != -1 || t.indexOf ('atom') != -1)) "
        "f.push (l[i].href + '|' + l[i].title);"
        #if !WEBKIT_CHECK_VERSION (1, 1, 18)
        "else if (r && r.indexOf ('icon') != -1) f.push (l[i].href); "
        #endif
        "} return f; })("
        "document.getElementsByTagName ('link'));", NULL);
        gchar** items = g_strsplit (value, ",", 0);
        guint i = 0;
        gchar* default_uri = NULL;

        katze_array_clear (view->news_feeds);
        if (items != NULL)
        while (items[i] != NULL)
        {
            gchar** parts = g_strsplit (items[i], "|", 2);
            if (parts == NULL)
                ;
            else if (*parts && parts[1])
            {
                KatzeItem* item = g_object_new (KATZE_TYPE_ITEM,
                    "uri", parts[0], "name", parts[1], NULL);
                katze_array_add_item (view->news_feeds, item);
                g_object_unref (item);
                if (!default_uri)
                    default_uri = g_strdup (parts[0]);
            }
            #if !WEBKIT_CHECK_VERSION (1, 1, 18)
            else
                katze_assign (view->icon_uri, g_strdup (*parts));
            #endif

            g_strfreev (parts);
            i++;
        }
        g_strfreev (items);
        g_object_set_data_full (G_OBJECT (view), "news-feeds", default_uri, g_free);
        g_free (value);
        /* Ensure load-status is notified again, whether it changed or not */
        g_object_notify (G_OBJECT (view), "load-status");
    }

    _midori_web_view_load_icon (view);

    g_object_thaw_notify (G_OBJECT (view));
}

#if WEBKIT_CHECK_VERSION (1, 1, 18)
static void
midori_web_view_notify_icon_uri_cb (WebKitWebView* web_view,
                                    GParamSpec*    pspec,
                                    MidoriView*    view)
{
    katze_assign (view->icon_uri, katze_object_get_string (web_view, "icon-uri"));
    _midori_web_view_load_icon (view);
}
#endif

#if WEBKIT_CHECK_VERSION (1, 1, 4)
static void
webkit_web_view_notify_uri_cb (WebKitWebView* web_view,
                               GParamSpec*    pspec,
                               MidoriView*    view)
{
    #if 0
    if (view->item)
    {
        /* Save back forward history as meta data. This is disabled
          because we can't reliably restore these atm. */
        WebKitWebView* web_view;
        WebKitWebBackForwardList* list;
        GList* back;
        GList* forward;

        web_view = WEBKIT_WEB_VIEW (view->web_view);
        list = webkit_web_view_get_back_forward_list (web_view);
        back = webkit_web_back_forward_list_get_back_list_with_limit (list, 5);
        forward = webkit_web_back_forward_list_get_forward_list_with_limit (list, 5);
        guint i;
        WebKitWebHistoryItem* item;
        gchar* key = g_strdup ("back0");

        i = 0;
        while ((item = g_list_nth_data (back, i++)))
        {
            katze_item_set_meta_string (view->item, key,
                webkit_web_history_item_get_uri (item));
            key[4] = 48 + i;
        }

        #if 0
        key[0] = 'f';
        key[1] = 'o';
        key[2] = 'r';
        key[3] = 'e';
        key[4] = 48;
        i = 0;
        while ((item = g_list_nth_data (forward, i++)))
        {
            katze_item_set_meta_string (view->item, key,
                webkit_web_history_item_get_uri (item));
            key[4] = 48 + i;
        }
        #endif
        g_free (key);
    }
    #endif

    g_object_get (web_view, "uri", &view->uri, NULL);
    g_object_notify (G_OBJECT (view), "uri");
}

static void
webkit_web_view_notify_title_cb (WebKitWebView* web_view,
                                 GParamSpec*    pspec,
                                 MidoriView*    view)
{
    g_object_get (web_view, "title", &view->title, NULL);
    midori_view_update_title (view);
    g_object_notify (G_OBJECT (view), "title");
}
#else
static void
webkit_web_view_title_changed_cb (WebKitWebView*  web_view,
                                  WebKitWebFrame* web_frame,
                                  const gchar*    title,
                                  MidoriView*     view)
{
    g_object_set (view, "title", title, NULL);
}
#endif

static void
webkit_web_view_statusbar_text_changed_cb (WebKitWebView* web_view,
                                           const gchar*   text,
                                           MidoriView*    view)
{
    g_object_set (G_OBJECT (view), "statusbar-text", text, NULL);
}

static gboolean
midori_view_web_view_leave_notify_event_cb (WebKitWebView*    web_view,
                                            GdkEventCrossing* event,
                                            MidoriView*       view)
{
    g_object_set (G_OBJECT (view), "statusbar-text", NULL, NULL);
    return FALSE;
}

static void
webkit_web_view_hovering_over_link_cb (WebKitWebView* web_view,
                                       const gchar*   tooltip,
                                       const gchar*   link_uri,
                                       MidoriView*    view)
{
    #if !(WEBKIT_CHECK_VERSION (2, 18, 0) && defined (HAVE_LIBSOUP_2_29_3))
    sokoke_prefetch_uri (link_uri, NULL, NULL);
    #endif

    katze_assign (view->link_uri, g_strdup (link_uri));
    if (link_uri && g_str_has_prefix (link_uri, "mailto:"))
    {
        gchar* text = g_strdup_printf (_("Send a message to %s"), &link_uri[7]);
        g_object_set (view, "statusbar-text", text, NULL);
        g_free (text);
    }
    else
        g_object_set (view, "statusbar-text", link_uri, NULL);
}

#define MIDORI_KEYS_MODIFIER_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK \
    | GDK_MOD1_MASK | GDK_META_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK )

static gboolean
gtk_widget_button_press_event_cb (WebKitWebView*  web_view,
                                  GdkEventButton* event,
                                  MidoriView*     view)
{
    GtkClipboard* clipboard;
    gchar* uri;
    gchar* new_uri;
    const gchar* link_uri;
    gboolean background;

    event->state = event->state & MIDORI_KEYS_MODIFIER_MASK;
    link_uri = midori_view_get_link_uri (MIDORI_VIEW (view));

    switch (event->button)
    {
    case 1:
        if (!link_uri)
            return FALSE;
        if (MIDORI_MOD_NEW_TAB (event->state))
        {
            /* Open link in new tab */
            background = view->open_tabs_in_the_background;
            if (MIDORI_MOD_BACKGROUND (event->state))
                background = !background;
            g_signal_emit (view, signals[NEW_TAB], 0, link_uri, background);
            return TRUE;
        }
        else if (MIDORI_MOD_NEW_WINDOW (event->state))
        {
            /* Open link in new window */
            g_signal_emit (view, signals[NEW_WINDOW], 0, link_uri);
            return TRUE;
        }
        break;
    case 2:
        if (link_uri)
        {
            /* Open link in new tab */
            background = view->open_tabs_in_the_background;
            if (MIDORI_MOD_BACKGROUND (event->state))
                background = !background;
            g_signal_emit (view, signals[NEW_TAB], 0, link_uri, background);
            return TRUE;
        }
        else if (MIDORI_MOD_SCROLL (event->state))
        {
            midori_view_set_zoom_level (MIDORI_VIEW (view), 1.0);
            return FALSE; /* Allow Ctrl + Middle click */
        }
        else if (view->middle_click_opens_selection)
        {
            gboolean is_editable;
            #if WEBKIT_CHECK_VERSION (1, 1, 15)
            WebKitHitTestResult* result;
            WebKitHitTestResultContext context;

            result = webkit_web_view_get_hit_test_result (web_view, event);
            context = katze_object_get_int (result, "context");
            is_editable = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
            g_object_unref (result);
            #else
            is_editable = webkit_web_view_can_paste_clipboard (WEBKIT_WEB_VIEW (view->web_view));
            #endif
            if (is_editable)
                return FALSE;

            clipboard = gtk_clipboard_get_for_display (
                gtk_widget_get_display (GTK_WIDGET (view)),
                GDK_SELECTION_PRIMARY);
            if ((uri = gtk_clipboard_wait_for_text (clipboard)))
            {
                guint i = 0;
                while (uri[i++] != '\0')
                    if (uri[i] == '\n' || uri[i] == '\r')
                        uri[i] = ' ';
                g_strstrip (uri);

                /* Hold Alt to search for the selected word */
                if (event->state & GDK_MOD1_MASK)
                {
                    new_uri = sokoke_magic_uri (uri);
                    if (!new_uri)
                    {
                        gchar* search;
                        g_object_get (view->settings, "location-entry-search",
                                      &search, NULL);
                        new_uri = sokoke_search_uri (search, uri);
                    }
                    katze_assign (uri, new_uri);
                }
                else if (!strstr (uri, "://"))
                {
                    g_free (uri);
                    return FALSE;
                }

                if (MIDORI_MOD_NEW_TAB (event->state))
                {
                    background = view->open_tabs_in_the_background;
                    if (MIDORI_MOD_BACKGROUND (event->state))
                        background = !background;
                    g_signal_emit (view, signals[NEW_TAB], 0, uri, background);
                }
                else
                {
                    midori_view_set_uri (MIDORI_VIEW (view), uri);
                    gtk_widget_grab_focus (GTK_WIDGET (view));
                }
                g_free (uri);
                return TRUE;
            }
        }
        break;
    #if WEBKIT_CHECK_VERSION (1, 1, 15)
    case 3:
        if (event->state & GDK_CONTROL_MASK)
        {
            /* Ctrl + Right-click suppresses javascript button handling */
            GtkWidget* menu = gtk_menu_new ();
            midori_view_populate_popup (view, menu, TRUE);
            katze_widget_popup (GTK_WIDGET (web_view), GTK_MENU (menu), event,
                                KATZE_MENU_POSITION_CURSOR);
            return TRUE;
        }
        break;
    #endif
    case 8:
        midori_view_go_back (view);
        return TRUE;
    case 9:
        midori_view_go_forward (view);
        return TRUE;
    /*
     * On some fancier mice the scroll wheel can be used to scroll horizontally.
     * A middle click usually registers both a middle click (2) and a
     * horizontal scroll (11 or 12).
     * We catch horizontal scrolls and ignore them to prevent middle clicks from
     * accidentally being interpreted as first button clicks.
     */
    case 11:
        return TRUE;
    case 12:
        return TRUE;
    }

    /* We propagate the event, since it may otherwise be stuck in WebKit */
    g_signal_emit_by_name (view, "event", event, &background);

    return FALSE;
}

static gboolean
gtk_widget_key_press_event_cb (WebKitWebView* web_view,
                               GdkEventKey*   event,
                               MidoriView*    view)
{
    guint character;

    if (event->keyval == '.' || event->keyval == '/' || event->keyval == GDK_KP_Divide)
        character = '\0';
    else if (view->find_while_typing)
        character = gdk_unicode_to_keyval (event->keyval);
    else
        return FALSE;

    /* Skip control characters */
    if (character == (event->keyval | 0x01000000))
        return FALSE;

    if (!webkit_web_view_can_cut_clipboard (web_view)
        && !webkit_web_view_can_paste_clipboard (web_view))
    {
        gchar* text = character ? g_strdup_printf ("%c", character) : g_strdup ("");
        g_signal_emit (view, signals[SEARCH_TEXT], 0, TRUE, text);
        g_free (text);
        return TRUE;
    }

    return FALSE;
}

static gboolean
gtk_widget_scroll_event_cb (WebKitWebView*  web_view,
                            GdkEventScroll* event,
                            MidoriView*     view)
{
    event->state = event->state & MIDORI_KEYS_MODIFIER_MASK;

    if (MIDORI_MOD_SCROLL (event->state))
    {
        if (event->direction == GDK_SCROLL_DOWN)
            midori_view_set_zoom_level (view,
                midori_view_get_zoom_level (view) - 0.25f);
        else if(event->direction == GDK_SCROLL_UP)
            midori_view_set_zoom_level (view,
                midori_view_get_zoom_level (view) + 0.25f);
        return TRUE;
    }
    else
        return FALSE;
}

#if WEBKIT_CHECK_VERSION (1, 1, 15)
static void
midori_web_view_set_clipboard (GtkWidget*   widget,
                                      const gchar* text)
{
    GdkDisplay* display = gtk_widget_get_display (widget);
    GtkClipboard* clipboard;

    clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (clipboard, text, -1);
    clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text (clipboard, text, -1);
}

static void
midori_web_view_menu_new_window_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    g_signal_emit (view, signals[NEW_WINDOW], 0, view->link_uri);
}

static void
midori_web_view_menu_web_app_activate_cb (GtkWidget*  widget,
                                          MidoriView* view)
{
    /* FIXME: Use the same binary that is running right now */
    sokoke_spawn_program ("midori -a", view->link_uri, FALSE);
}

static void
midori_web_view_menu_link_copy_activate_cb (GtkWidget*  widget,
                                            MidoriView* view)
{
    midori_web_view_set_clipboard (widget, view->link_uri);
}

static void
midori_web_view_menu_save_activate_cb (GtkWidget*  widget,
                                       MidoriView* view)
{
    WebKitNetworkRequest* request = webkit_network_request_new (view->link_uri);
    WebKitDownload* download = webkit_download_new (request);
    gboolean handled;
    g_object_unref (request);
    if (view->ask_for_destination_folder)
        g_object_set_data (G_OBJECT (download), "save-as-download", (void*)0xdeadbeef);
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    if (!view->ask_for_destination_folder)
        webkit_download_start (download);
}

static void
midori_web_view_menu_image_new_tab_activate_cb (GtkWidget*  widget,
                                                MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    g_signal_emit (view, signals[NEW_TAB], 0, uri,
                   view->open_tabs_in_the_background);
    g_free (uri);
}

static void
midori_web_view_menu_image_copy_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    midori_web_view_set_clipboard (widget, uri);
    g_free (uri);
}

static void
midori_web_view_menu_image_save_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    WebKitNetworkRequest* request = webkit_network_request_new (uri);
    WebKitDownload* download = webkit_download_new (request);
    gboolean handled;
    g_object_unref (request);
    if (view->ask_for_destination_folder)
        g_object_set_data (G_OBJECT (download), "save-as-download", (void*)0xdeadbeef);
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    if (!view->ask_for_destination_folder)
        webkit_download_start (download);
    g_free (uri);
}

static void
midori_web_view_menu_video_copy_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    midori_web_view_set_clipboard (widget, uri);
    g_free (uri);
}

static void
midori_web_view_menu_video_save_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    WebKitNetworkRequest* request = webkit_network_request_new (uri);
    WebKitDownload* download = webkit_download_new (request);
    gboolean handled;
    g_object_unref (request);
    if (view->ask_for_destination_folder)
        g_object_set_data (G_OBJECT (download), "save-as-download", (void*)0xdeadbeef);
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    if (!view->ask_for_destination_folder)
        webkit_download_start (download);
    g_free (uri);
}

static void
midori_web_view_menu_video_download_activate_cb (GtkWidget*  widget,
                                                 MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    sokoke_spawn_program (view->download_manager, uri, FALSE);
    g_free (uri);
}
#endif

static void
midori_web_view_menu_new_tab_activate_cb (GtkWidget*  widget,
                                          MidoriView* view)
{
    if (view->link_uri)
        g_signal_emit (view, signals[NEW_TAB], 0, view->link_uri,
                       view->open_tabs_in_the_background);
    else
    {
        gchar* data = (gchar*)g_object_get_data (G_OBJECT (widget), "uri");
        if (strchr (data, '@'))
        {
            gchar* uri = g_strconcat ("mailto:", data, NULL);
            sokoke_show_uri (gtk_widget_get_screen (widget),
                             uri, GDK_CURRENT_TIME, NULL);
            g_free (uri);
        }
        else
        {
            gchar* uri = sokoke_magic_uri (data);
            if (!uri)
                uri = g_strdup (data);
            g_signal_emit (view, signals[NEW_TAB], 0, uri,
                           view->open_tabs_in_the_background);
            g_free (uri);
        }
    }
}

#if WEBKIT_CHECK_VERSION (1, 1, 15)
static void
midori_web_view_menu_background_tab_activate_cb (GtkWidget*  widget,
                                                 MidoriView* view)
{
    g_signal_emit (view, signals[NEW_TAB], 0, view->link_uri,
                   !view->open_tabs_in_the_background);
}
#endif

static void
midori_web_view_menu_search_web_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* search;
    gchar* uri;

    if ((search = g_object_get_data (G_OBJECT (widget), "search")))
        search = g_strdup (search);
    else
        g_object_get (view->settings, "location-entry-search",
                      &search, NULL);
    uri = sokoke_search_uri (search, view->selected_text);
    g_free (search);

    g_signal_emit (view, signals[NEW_TAB], 0, uri,
        view->open_tabs_in_the_background);

    g_free (uri);
}

#if WEBKIT_CHECK_VERSION (1, 1, 15)
static void
midori_web_view_menu_copy_activate_cb (GtkWidget*  widget,
                                       MidoriView* view)
{
    midori_web_view_set_clipboard (widget, view->selected_text);
}
#endif

#if !WEBKIT_CHECK_VERSION (1, 1, 3)
static void
midori_web_view_menu_save_as_activate_cb (GtkWidget*  widget,
                                          MidoriView* view)
{
    g_signal_emit (view, signals[SAVE_AS], 0, view->link_uri);
}
#endif

static void
midori_web_view_menu_download_activate_cb (GtkWidget*  widget,
                                           MidoriView* view)
{
    sokoke_spawn_program (view->download_manager, view->link_uri, FALSE);
}

static void
midori_view_tab_label_menu_window_new_cb (GtkWidget* menuitem,
                                          GtkWidget* view)
{
    g_signal_emit (view, signals[NEW_WINDOW], 0,
        midori_view_get_display_uri (MIDORI_VIEW (view)));
}

#if WEBKIT_CHECK_VERSION (1, 1, 17)
static void
midori_web_view_menu_inspect_element_activate_cb (GtkWidget*  widget,
                                                  MidoriView* view)
{
    WebKitWebInspector* inspector;
    gint x, y;

    inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (view->web_view));
    x = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "x"));
    y = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "y"));
    webkit_web_inspector_inspect_coordinates (inspector, x, y);
}
#endif

static GtkWidget*
midori_view_insert_menu_item (GtkMenuShell* menu,
                             gint          position,
                             const gchar*  label,
                             const gchar*  stock_id,
                             GCallback     callback,
                             GtkWidget*    widget)
{
    GtkWidget* menuitem;

    if (label)
    {
        menuitem = gtk_image_menu_item_new_with_mnemonic (label);
        if (stock_id)
        {
            GdkScreen* screen = gtk_widget_get_screen (widget);
            GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen (screen);
            if (gtk_icon_theme_has_icon (icon_theme, stock_id))
            {
                GtkWidget* icon = gtk_image_new_from_stock (stock_id,
                    GTK_ICON_SIZE_MENU);
                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
                    icon);
            }
        }
    }
    else
        menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, position);
    if (callback)
        g_signal_connect (menuitem, "activate", callback, widget);
    else
        gtk_widget_set_sensitive (menuitem, FALSE);
    return menuitem;
}

/**
 * midori_view_populate_popup:
 * @view: a #MidoriView
 * @menu: a #GtkMenu
 * @manual: %TRUE if this a manually created popup
 *
 * Populates the given @menu with context menu items
 * according to the position of the mouse pointer. This
 * can be used in situations where a custom hotkey
 * opens the context menu or the default behaviour
 * needs to be intercepted.
 *
 * @manual should usually be %TRUE, except for the
 * case where @menu was created by the #WebKitWebView.
 *
 * Since: 0.2.5
 */
void
midori_view_populate_popup (MidoriView* view,
                            GtkWidget*  menu,
                            gboolean    manual)
{
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    GtkWidget* widget = GTK_WIDGET (view);
    MidoriBrowser* browser = midori_browser_get_for_widget (widget);
    GtkActionGroup* actions = midori_browser_get_action_group (browser);
    GtkMenuShell* menu_shell = GTK_MENU_SHELL (menu);
    GtkWidget* menuitem;
    GtkWidget* icon;
    gchar* stock_id;
    GList* items;
    gboolean has_selection;
    gboolean is_editable;
    gboolean is_document;
    GtkWidget* label;
    guint i;

    #if WEBKIT_CHECK_VERSION (1, 1, 15)
    gint x, y;
    GdkEventButton event;
    WebKitHitTestResultContext context;
    gboolean is_image;
    gboolean is_media;

    gdk_window_get_pointer (view->web_view->window, &x, &y, NULL);
    event.x = x;
    event.y = y;
    katze_object_assign (view->hit_test,
        webkit_web_view_get_hit_test_result (web_view, &event));
    context = katze_object_get_int (view->hit_test, "context");
    /* Ensure view->link_uri is correct. */
    katze_assign (view->link_uri,
        katze_object_get_string (view->hit_test, "link-uri"));
    has_selection = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION;
    /* Ensure view->selected_text */
    midori_view_has_selection (view);
    is_editable = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
    is_image = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
    is_media = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA;
    is_document = !view->link_uri && !has_selection && !is_image && !is_media;
    #else
    /* There is no guarantee view->link_uri is correct in case
        gtk-touchscreen-mode is enabled, nothing we can do. */
    has_selection = midori_view_has_selection (view);
    is_document = !view->link_uri && !has_selection;

    /* Unfortunately inspecting the menu is the only way to
       determine that the mouse is over a text area or selection. */
    items = gtk_container_get_children (GTK_CONTAINER (menu));
    menuitem = (GtkWidget*)g_list_nth_data (items, 0);
    if (GTK_IS_IMAGE_MENU_ITEM (menuitem))
    {
        icon = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menuitem));
        gtk_image_get_stock (GTK_IMAGE (icon), &stock_id, NULL);
        if (!strcmp (stock_id, GTK_STOCK_FIND))
        {
            gtk_widget_hide (menuitem);
            gtk_widget_set_no_show_all (menuitem, TRUE);
            menuitem = (GtkWidget*)g_list_nth_data (items, 1);
            gtk_widget_hide (menuitem);
            menuitem = (GtkWidget*)g_list_nth_data (items, 2);
            icon = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menuitem));
            gtk_image_get_stock (GTK_IMAGE (icon), &stock_id, NULL);
        }
        is_editable = !strcmp (stock_id, GTK_STOCK_CUT);
        if (is_document && !strcmp (stock_id, GTK_STOCK_OPEN))
            is_document = FALSE;
    }
    else
        is_editable = FALSE;
    g_list_free (items);
    #endif

    if (is_editable)
    {
        #if WEBKIT_CHECK_VERSION (1, 1, 14)
        menuitem = gtk_separator_menu_item_new ();
        gtk_menu_shell_prepend (menu_shell, menuitem);
        gtk_widget_show (menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Redo"));
        gtk_widget_set_sensitive (menuitem,
            webkit_web_view_can_redo (web_view));
        gtk_menu_shell_prepend (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Undo"));
        gtk_widget_set_sensitive (menuitem,
            webkit_web_view_can_undo (web_view));
        gtk_menu_shell_prepend (menu_shell, menuitem);
        #endif
        if (manual)
        {
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Cut"));
            gtk_widget_set_sensitive (menuitem,
                webkit_web_view_can_cut_clipboard (web_view));
            gtk_menu_shell_append (menu_shell, menuitem);
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Copy"));
            gtk_widget_set_sensitive (menuitem,
                webkit_web_view_can_copy_clipboard (web_view));
            gtk_menu_shell_append (menu_shell, menuitem);
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Paste"));
            gtk_widget_set_sensitive (menuitem,
                webkit_web_view_can_paste_clipboard (web_view));
            gtk_menu_shell_append (menu_shell, menuitem);
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Delete"));
            gtk_widget_set_sensitive (menuitem,
                webkit_web_view_can_cut_clipboard (web_view));
            gtk_menu_shell_append (menu_shell, menuitem);
            menuitem = gtk_separator_menu_item_new ();
            gtk_widget_show (menuitem);
            gtk_menu_shell_append (menu_shell, menuitem);
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "SelectAll"));
            gtk_menu_shell_append (menu_shell, menuitem);
            /* FIXME: We are missing Font, Input Methods and Insert Character */
            #if WEBKIT_CHECK_VERSION (1, 1, 17)
            if (katze_object_get_boolean (view->settings, "enable-developer-extras"))
            {
                menuitem = gtk_separator_menu_item_new ();
                gtk_widget_show (menuitem);
                gtk_menu_shell_append (menu_shell, menuitem);
                menuitem = midori_view_insert_menu_item (menu_shell, -1,
                    _("Inspect _Element"), NULL,
                    G_CALLBACK (midori_web_view_menu_inspect_element_activate_cb),
                    widget);
                gtk_widget_show (menuitem);
                g_object_set_data (G_OBJECT (menuitem), "x", GINT_TO_POINTER (x));
                g_object_set_data (G_OBJECT (menuitem), "y", GINT_TO_POINTER (y));
             }
             #endif
        }
        return;
    }

    items = gtk_container_get_children (GTK_CONTAINER (menu));
    menuitem = (GtkWidget*)g_list_nth_data (items, 0);
    /* Form control: no items */
    if (!manual && !menuitem)
    {
        g_list_free (items);
        return;
    }
    /* Form control: separator and Inspect element */
    if (!manual && GTK_IS_SEPARATOR_MENU_ITEM (menuitem) && g_list_length (items) == 2)
    {
        gtk_widget_destroy (menuitem);
        g_list_free (items);
        return;
    }
    g_list_free (items);
    /* Link and/ or image, but falsely reported as document */
    if (is_document)
    {
        if (GTK_IS_IMAGE_MENU_ITEM (menuitem))
        {
            icon = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menuitem));
            gtk_image_get_stock (GTK_IMAGE (icon), &stock_id, NULL);
            if (stock_id && !strcmp (stock_id, GTK_STOCK_OPEN))
                return;
        }
    }

    #if WEBKIT_CHECK_VERSION (1, 1, 15)
    if (!is_document)
    {
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        i = 0;
        while ((menuitem = g_list_nth_data (items, i++)))
            gtk_widget_destroy (menuitem);
        g_list_free (items);
    }
    if (view->link_uri)
    {
        midori_view_insert_menu_item (menu_shell, -1,
            _("Open Link in New _Tab"), STOCK_TAB_NEW,
            G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            view->open_tabs_in_the_background
            ? _("Open Link in _Foreground Tab")
            : _("Open Link in _Background Tab"), NULL,
            G_CALLBACK (midori_web_view_menu_background_tab_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Open Link in New _Window"), STOCK_WINDOW_NEW,
            G_CALLBACK (midori_web_view_menu_new_window_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Open Link as Web A_pplication"), NULL,
            G_CALLBACK (midori_web_view_menu_web_app_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Copy Link de_stination"), NULL,
            G_CALLBACK (midori_web_view_menu_link_copy_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            view->ask_for_destination_folder ? _("_Save Link destination")
            : _("_Download Link destination"), NULL,
            G_CALLBACK (midori_web_view_menu_save_activate_cb), widget);
        if (view->download_manager && *view->download_manager)
            midori_view_insert_menu_item (menu_shell, -1,
            _("Download with Download _Manager"), STOCK_TRANSFER,
            G_CALLBACK (midori_web_view_menu_download_activate_cb), widget);
    }

    if (is_image)
    {
        if (view->link_uri)
            gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        midori_view_insert_menu_item (menu_shell, -1,
            _("Open _Image in New Tab"), STOCK_TAB_NEW,
            G_CALLBACK (midori_web_view_menu_image_new_tab_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Copy Image _Address"), NULL,
            G_CALLBACK (midori_web_view_menu_image_copy_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            view->ask_for_destination_folder ? _("Save I_mage")
            : _("Download I_mage"), GTK_STOCK_SAVE,
            G_CALLBACK (midori_web_view_menu_image_save_activate_cb), widget);
    }

    if (is_media)
    {
        midori_view_insert_menu_item (menu_shell, -1,
            _("Copy Video _Address"), NULL,
            G_CALLBACK (midori_web_view_menu_video_copy_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            FALSE ? _("Save _Video") : _("Download _Video"), GTK_STOCK_SAVE,
            G_CALLBACK (midori_web_view_menu_video_save_activate_cb), widget);
        if (view->download_manager && *view->download_manager)
            midori_view_insert_menu_item (menu_shell, -1,
            _("Download with Download _Manager"), STOCK_TRANSFER,
            G_CALLBACK (midori_web_view_menu_video_download_activate_cb), widget);
    }

    if (has_selection)
    {
        gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        midori_view_insert_menu_item (menu_shell, -1, NULL, GTK_STOCK_COPY,
            G_CALLBACK (midori_web_view_menu_copy_activate_cb), widget);
    }
    #else
    if (view->link_uri)
    {
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        menuitem = (GtkWidget*)g_list_nth_data (items, 0);
        /* hack to hide menu item */
        gtk_widget_hide (menuitem);
        midori_view_insert_menu_item (menu_shell, 1,
            _("Open Link in New _Tab"), STOCK_TAB_NEW,
            G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), widget);
        g_list_free (items);
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        menuitem = (GtkWidget*)g_list_nth_data (items, 2);
        /* hack to localize menu item */
        label = gtk_bin_get_child (GTK_BIN (menuitem));
        gtk_label_set_label (GTK_LABEL (label), _("Open Link in New _Window"));
        menuitem = (GtkWidget*)g_list_nth_data (items, 3);
        g_list_free (items);
        #if WEBKIT_CHECK_VERSION (1, 1, 3)
        /* hack to localize menu item */
        label = gtk_bin_get_child (GTK_BIN (menuitem));
        gtk_label_set_label (GTK_LABEL (label), _("_Download Link destination"));
        #else
        /* hack to disable non-functional Download File */
        gtk_widget_hide (menuitem);
        gtk_widget_set_no_show_all (menuitem, TRUE);
        midori_view_insert_menu_item (menu_shell, 3,
            _("_Save Link destination"), NULL,
            G_CALLBACK (midori_web_view_menu_save_as_activate_cb), widget);
        #endif
        if (view->download_manager && *view->download_manager)
            midori_view_insert_menu_item (menu_shell, 4,
            _("Download with Download _Manager"), STOCK_TRANSFER,
            G_CALLBACK (midori_web_view_menu_download_activate_cb), widget);
    }
    #endif

    if (!view->link_uri && has_selection)
    {
        GtkWidget* window;

        window = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
        i = 0;
        if (katze_object_has_property (window, "search-engines"))
        {
            KatzeArray* search_engines;
            KatzeItem* item;
            GtkWidget* sub_menu = gtk_menu_new ();

            menuitem = gtk_image_menu_item_new_with_mnemonic (_("Search _with"));
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), sub_menu);
            gtk_menu_shell_insert (menu_shell, menuitem, 1);

            search_engines = katze_object_get_object (window, "search-engines");
            while ((item = katze_array_get_nth_item (search_engines, i++)))
            {
                GdkPixbuf* pixbuf;
                const gchar* icon_name;

                menuitem = gtk_image_menu_item_new_with_mnemonic (katze_item_get_name (item));
                pixbuf = midori_search_action_get_icon (item,
                    GTK_WIDGET (web_view), &icon_name);
                if (pixbuf)
                {
                    icon = gtk_image_new_from_pixbuf (pixbuf);
                    g_object_unref (pixbuf);
                }
                else
                    icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
                #if GTK_CHECK_VERSION (2, 16, 0)
                gtk_image_menu_item_set_always_show_image (
                    GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
                #endif
                gtk_menu_shell_insert (GTK_MENU_SHELL (sub_menu), menuitem, i - 1);
                g_object_set_data (G_OBJECT (menuitem), "search",
                                   (gchar*)katze_item_get_uri (item));
                g_signal_connect (menuitem, "activate",
                    G_CALLBACK (midori_web_view_menu_search_web_activate_cb), view);
            }
            g_object_unref (search_engines);
        }
        #if WEBKIT_CHECK_VERSION (1, 1, 15)
        midori_view_insert_menu_item (menu_shell, 0,
            _("_Search the Web"), GTK_STOCK_FIND,
            G_CALLBACK (midori_web_view_menu_search_web_activate_cb), widget);
        #else
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        menuitem = (GtkWidget*)g_list_nth_data (items, 0);
        /* hack to localize menu item */
        label = gtk_bin_get_child (GTK_BIN (menuitem));
        gtk_label_set_label (GTK_LABEL (label), _("_Search the Web"));
        /* hack to implement Search the Web */
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_web_view_menu_search_web_activate_cb), view);
        g_list_free (items);
        #endif

        g_strstrip (view->selected_text);
        if (view->selected_text && !strchr (view->selected_text, ' ')
            && (strchr (view->selected_text, '.') || g_strstr_len (view->selected_text, 9, "://")))
        {
            if (strchr (view->selected_text, '@'))
            {
                gchar* text = g_strdup_printf (_("Send a message to %s"), view->selected_text);
                menuitem = midori_view_insert_menu_item (menu_shell, -1,
                    text, GTK_STOCK_JUMP_TO,
                    G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), widget);
                g_free (text);
            }
            else
                menuitem = midori_view_insert_menu_item (menu_shell, -1,
                    _("Open Address in New _Tab"), GTK_STOCK_JUMP_TO,
                    G_CALLBACK (midori_web_view_menu_new_tab_activate_cb), widget);
            g_object_set_data (G_OBJECT (menuitem), "uri", view->selected_text);
        }
    }

    if (is_document)
    {
        if (manual)
        {
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Back"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Forward"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Stop"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Reload"));
        gtk_menu_shell_append (menu_shell, menuitem);
        }
        else
        {
        items = gtk_container_get_children (GTK_CONTAINER (menu));
        #if HAVE_HILDON
        gtk_widget_hide (g_list_nth_data (items, 2));
        gtk_widget_set_no_show_all (g_list_nth_data (items, 2), TRUE);
        gtk_widget_hide (g_list_nth_data (items, 3));
        gtk_widget_set_no_show_all (g_list_nth_data (items, 3), TRUE);
        #endif
        menuitem = (GtkWidget*)g_list_nth_data (items, 3);
        /* hack to localize menu item */
        if (GTK_IS_BIN (menuitem))
        {
            GtkStockItem stock_item;
            if (gtk_stock_lookup (GTK_STOCK_REFRESH, &stock_item))
            {
                label = gtk_bin_get_child (GTK_BIN (menuitem));
                gtk_label_set_label (GTK_LABEL (label), stock_item.label);
            }
        }
        g_list_free (items);
        }

        gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "UndoTabClose"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = gtk_image_menu_item_new_from_stock (STOCK_WINDOW_NEW, NULL);
        gtk_menu_item_set_label (GTK_MENU_ITEM (menuitem), _("Open in New _Window"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_view_tab_label_menu_window_new_cb), view);

        #if WEBKIT_CHECK_VERSION (1, 1, 15)
        /* if (webkit_web_view_get_main_frame (web_view) != frame_under_mouse)
        {
            midori_view_insert_menu_item (menu_shell, -1,
                _("Open _Frame in New Tab"), NULL,
                G_CALLBACK (midori_web_view_menu_frame_new_tab_activate_cb), widget);
            midori_view_insert_menu_item (menu_shell, -1,
                _("Open _Frame in New Window"), NULL,
                G_CALLBACK (midori_web_view_menu_frame_new_window_activate_cb), widget);
        } */
        #endif

        if (!g_object_get_data (G_OBJECT (browser), "midori-toolbars-visible"))
        {
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Menubar"));
            gtk_menu_shell_append (menu_shell, menuitem);
        }

        #if !HAVE_HILDON
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "ZoomIn"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "ZoomOut"));
        gtk_menu_shell_append (menu_shell, menuitem);
        #endif

        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Encoding"));
        gtk_menu_shell_append (menu_shell, menuitem);
        if (gtk_widget_get_sensitive (menuitem))
        {
            GtkWidget* sub_menu;
            static const GtkActionEntry encodings[] = {
              { "EncodingAutomatic" },
              { "EncodingChinese" },
              { "EncodingJapanese" },
              { "EncodingKorean" },
              { "EncodingRussian" },
              { "EncodingUnicode" },
              { "EncodingWestern" },
              { "EncodingCustom" },
            };

            sub_menu = gtk_menu_new ();
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), sub_menu);

            for (i = 0; i < G_N_ELEMENTS (encodings); i++)
            {
                menuitem = sokoke_action_create_popup_menu_item (
                    gtk_action_group_get_action (actions, encodings[i].name));
                gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), menuitem);
            }
        }

        #if HAVE_HILDON
        gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "CompactAdd"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Fullscreen"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "PrivateBrowsing"));
        gtk_menu_shell_append (menu_shell, menuitem);
        #else
        gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "BookmarkAdd"));
        gtk_menu_shell_append (menu_shell, menuitem);

        if (view->speed_dial_in_new_tabs && !midori_view_is_blank (view))
        {
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "AddSpeedDial"));
            gtk_menu_shell_append (menu_shell, menuitem);
        }
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "AddDesktopShortcut"));
        gtk_menu_shell_append (menu_shell, menuitem);
        gtk_widget_set_no_show_all (menuitem, TRUE);
        #endif

        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "SaveAs"));
        gtk_menu_shell_append (menu_shell, menuitem);
        /* Currently views that don't support source, don't support
           saving either. If that changes, we need to think of something. */
        if (!midori_view_can_view_source (view))
            gtk_widget_set_sensitive (menuitem, FALSE);
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "SourceView"));
        gtk_menu_shell_append (menu_shell, menuitem);
    }

    #if WEBKIT_CHECK_VERSION (1, 1, 17)
    if ((!is_document || manual)
      && katze_object_get_boolean (view->settings, "enable-developer-extras"))
    {
        gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        menuitem = midori_view_insert_menu_item (menu_shell, -1,
            _("Inspect _Element"), NULL,
            G_CALLBACK (midori_web_view_menu_inspect_element_activate_cb), widget);
        g_object_set_data (G_OBJECT (menuitem), "x", GINT_TO_POINTER (x));
        g_object_set_data (G_OBJECT (menuitem), "y", GINT_TO_POINTER (y));
    }
    #endif

    gtk_widget_show_all (menu);
}

static void
webkit_web_view_populate_popup_cb (WebKitWebView* web_view,
                                   GtkWidget*     menu,
                                   MidoriView*    view)
{
    midori_view_populate_popup (view, menu, FALSE);
}

#if HAVE_HILDON
static void
midori_view_web_view_tap_and_hold_cb (GtkWidget*  web_view,
                                      gpointer    data)
{
    gint x, y;
    GdkEvent event;
    gboolean result;

    /* Emulate a pointer motion above the tap position
      and a right click at the according position. */
    gdk_window_get_pointer (web_view->window, &x, &y, NULL);
    event.any.type = GDK_MOTION_NOTIFY;
    event.any.window = web_view->window;
    event.motion.x = x;
    event.motion.y = y;
    g_signal_emit_by_name (web_view, "motion-notify-event", &event, &result);

    event.any.type = GDK_BUTTON_PRESS;
    event.any.window = web_view->window;
    event.button.axes = NULL;
    event.button.x = x;
    event.button.y = y;
    event.button.button = 3;
    g_signal_emit_by_name (web_view, "button-press-event", &event, &result);
}
#endif

static gboolean
webkit_web_view_web_view_ready_cb (GtkWidget*  web_view,
                                   MidoriView* view)
{
    GtkWidget* new_view = gtk_widget_get_parent (gtk_widget_get_parent (web_view));
    MidoriNewView where = MIDORI_NEW_VIEW_TAB;

    /* FIXME: Open windows opened by scripts in tabs if they otherwise
        would be replacing the page the user opened. */
    if (view->open_new_pages_in == MIDORI_NEW_PAGE_CURRENT)
        return TRUE;

    if (view->open_new_pages_in == MIDORI_NEW_PAGE_TAB)
    {
        if (view->open_tabs_in_the_background)
            where = MIDORI_NEW_VIEW_BACKGROUND;
    }
    else if (view->open_new_pages_in == MIDORI_NEW_PAGE_WINDOW)
        where = MIDORI_NEW_VIEW_WINDOW;

    gtk_widget_show (new_view);
    g_signal_emit (view, signals[NEW_VIEW], 0, new_view, where);

    return TRUE;
}

static GtkWidget*
webkit_web_view_create_web_view_cb (GtkWidget*      web_view,
                                    WebKitWebFrame* web_frame,
                                    MidoriView*     view)
{
    MidoriView* new_view;

    if (view->open_new_pages_in == MIDORI_NEW_PAGE_CURRENT)
        new_view = view;
    else
    {
        new_view = g_object_new (MIDORI_TYPE_VIEW,
            "settings", view->settings,
            NULL);
        midori_view_construct_web_view (new_view);
        g_signal_connect (new_view->web_view, "web-view-ready",
                          G_CALLBACK (webkit_web_view_web_view_ready_cb), view);
    }
    return new_view->web_view;
}

static gboolean
webkit_web_view_mime_type_decision_cb (GtkWidget*               web_view,
                                       WebKitWebFrame*          web_frame,
                                       WebKitNetworkRequest*    request,
                                       const gchar*             mime_type,
                                       WebKitWebPolicyDecision* decision,
                                       MidoriView*              view)
{
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    GtkWidget* dialog;
    gchar* content_type;
    gchar* description;
    #if GTK_CHECK_VERSION (2, 14, 0)
    GIcon* icon;
    GtkWidget* image;
    #endif
    gchar* title;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    gint response;
    #else
    gchar* uri;
    #endif

    if (web_frame != webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view)))
        return FALSE;

    if (webkit_web_view_can_show_mime_type (WEBKIT_WEB_VIEW (web_view), mime_type))
    {
        #if WEBKIT_CHECK_VERSION (1, 1, 14)
        gboolean view_source = FALSE;
        /* Dedicated source code views are always pseudo-blank pages */
        if (midori_view_is_blank (view))
            view_source = webkit_web_view_get_view_source_mode (WEBKIT_WEB_VIEW (web_view));

        /* Render raw XML, including news feeds, as source */
        if (!view_source && (!strcmp (mime_type, "application/xml")
                          || !strcmp (mime_type, "text/xml")))
            view_source = TRUE;
        webkit_web_view_set_view_source_mode (WEBKIT_WEB_VIEW (web_view), view_source);
        #endif

        katze_assign (view->mime_type, g_strdup (mime_type));
        midori_view_update_icon (view, NULL);
        g_object_notify (G_OBJECT (view), "mime-type");

        return FALSE;
    }

    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    dialog = gtk_message_dialog_new (
        NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        _("Open or download file"));
    content_type = g_content_type_from_mime_type (mime_type);
    if (!content_type)
    #ifdef G_OS_WIN32
        content_type = g_content_type_get_mime_type ("*");
    #else
        content_type = g_strdup ("application/octet-stream");
    #endif
    description = g_content_type_get_description (content_type);
    #if GTK_CHECK_VERSION (2, 14, 0)
    icon = g_content_type_get_icon (content_type);
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
    g_object_unref (icon);
    gtk_widget_show (image);
    gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
    #endif
    g_free (content_type);
    if (g_strrstr (description, mime_type))
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
        _("File Type: '%s'"), mime_type);
    else
       gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
       _("File Type: %s ('%s')"), description, mime_type);
    g_free (description);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    /* i18n: A file open dialog title, ie. "Open http://fila.com/manual.tgz" */
    title = g_strdup_printf (_("Open %s"),
        webkit_network_request_get_uri (request));
    gtk_window_set_title (GTK_WINDOW (dialog), title);
    g_free (title);
    screen = gtk_widget_get_screen (dialog);
    if (screen)
    {
        icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, STOCK_TRANSFER))
            gtk_window_set_icon_name (GTK_WINDOW (dialog), STOCK_TRANSFER);
        else
            gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_OPEN);
    }
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
        GTK_STOCK_SAVE, 1,
        GTK_STOCK_SAVE_AS, 4,
        GTK_STOCK_CANCEL, 2,
        GTK_STOCK_OPEN, 3,
        NULL);
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_object_set_data (G_OBJECT (view), "open-download", (gpointer)0);
    switch (response)
    {
       case 4:
            g_object_set_data (G_OBJECT (view), "save-as-download", (gpointer)1);
            webkit_web_policy_decision_download (decision);
            webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (view->web_view));
            return TRUE;
        case 3:
            g_object_set_data (G_OBJECT (view), "open-download", (gpointer)1);
        case 1:
            webkit_web_policy_decision_download (decision);
            /* Apparently WebKit will continue loading which ends in an error.
               It's unclear whether it's a bug or we are doing something wrong. */
            webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (view->web_view));
            return TRUE;
        case 2:
        default:
            /* Apparently WebKit will continue loading which ends in an error.
               It's unclear whether it's a bug or we are doing something wrong. */
            webkit_web_view_stop_loading (WEBKIT_WEB_VIEW (view->web_view));
            return FALSE;
    }
    #else
    katze_assign (view->mime_type, NULL);
    midori_view_update_icon (view, NULL);
    g_object_notify (G_OBJECT (view), "mime-type");

    uri = g_strdup_printf ("error:nodisplay %s",
        webkit_network_request_get_uri (request));
    midori_view_set_uri (view, uri);
    g_free (uri);

    return TRUE;
    #endif
}

#if WEBKIT_CHECK_VERSION (1, 1, 3)
static gboolean
webkit_web_view_download_requested_cb (GtkWidget*      web_view,
                                       WebKitDownload* download,
                                       MidoriView*     view)
{
    gboolean handled;
    g_object_set_data (G_OBJECT (download), "open-download",
        g_object_get_data (G_OBJECT (view), "open-download"));
    g_object_set_data (G_OBJECT (download), "save-as-download",
        g_object_get_data (G_OBJECT (view), "save-as-download"));
    g_object_set_data (G_OBJECT (view), "open-download", (gpointer)0);
    g_object_set_data (G_OBJECT (view), "save-as-download", (gpointer)0);
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    return handled;
}
#endif

static gboolean
webkit_web_view_console_message_cb (GtkWidget*   web_view,
                                    const gchar* message,
                                    guint        line,
                                    const gchar* source_id,
                                    MidoriView*  view)
{
    if (!strncmp (message, "speed_dial-get-thumbnail", 22))
        midori_view_speed_dial_get_thumb (web_view, message, view);
    else if (!strncmp (message, "speed_dial-save", 13))
        midori_view_speed_dial_save (web_view, message);
    else
        g_signal_emit (view, signals[CONSOLE_MESSAGE], 0, message, line, source_id);
    return TRUE;
}

#if WEBKIT_CHECK_VERSION (1, 1, 5)
static gboolean
midori_view_web_view_print_requested_cb (GtkWidget*      web_view,
                                         WebKitWebFrame* web_frame,
                                         MidoriView*     view)
{
    midori_view_print (view);
    return TRUE;
}
#endif

static void
webkit_web_view_window_object_cleared_cb (GtkWidget*      web_view,
                                          WebKitWebFrame* web_frame,
                                          JSContextRef    js_context,
                                          JSObjectRef     js_window,
                                          MidoriView*     view)
{
    g_signal_emit (view, signals[CONTEXT_READY], 0, js_context);
}

static void
midori_view_hadjustment_notify_value_cb (GtkAdjustment* hadjustment,
                                         GParamSpec*    pspec,
                                         MidoriView*    view)
{
    gint value = (gint)gtk_adjustment_get_value (hadjustment);
    if (view->item)
        katze_item_set_meta_integer (view->item, "scrollh", value);
}

static void
midori_view_notify_hadjustment_cb (MidoriView* view,
                                   GParamSpec* pspec,
                                   gpointer    data)
{
    GtkAdjustment* hadjustment = katze_object_get_object (view->scrolled_window, "hadjustment");
    g_signal_connect (hadjustment, "notify::value",
        G_CALLBACK (midori_view_hadjustment_notify_value_cb), view);
    g_object_unref (hadjustment);
}

static void
midori_view_vadjustment_notify_value_cb (GtkAdjustment* vadjustment,
                                         GParamSpec*    pspec,
                                         MidoriView*    view)
{
    gint value = (gint)gtk_adjustment_get_value (vadjustment);
    if (view->item)
        katze_item_set_meta_integer (view->item, "scrollv", value);
}

static void
midori_view_notify_vadjustment_cb (MidoriView* view,
                                   GParamSpec* pspec,
                                   gpointer    data)
{
    GtkAdjustment* vadjustment = katze_object_get_object (view->scrolled_window, "vadjustment");
    g_signal_connect (vadjustment, "notify::value",
        G_CALLBACK (midori_view_vadjustment_notify_value_cb), view);
    g_object_unref (vadjustment);
}

void
katze_net_object_maybe_unref (gpointer object)
{
    if (object)
        g_object_unref (object);
}

static void
midori_view_init (MidoriView* view)
{
    view->uri = NULL;
    view->title = NULL;
    view->security = MIDORI_SECURITY_NONE;
    view->mime_type = g_strdup ("");
    view->icon = NULL;
    view->icon_uri = NULL;
    view->memory = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, katze_net_object_maybe_unref);
    view->progress = 0.0;
    view->load_status = MIDORI_LOAD_FINISHED;
    view->minimized = FALSE;
    view->statusbar_text = NULL;
    #if WEBKIT_CHECK_VERSION (1, 1, 15)
    view->hit_test = NULL;
    #endif
    view->link_uri = NULL;
    view->selected_text = NULL;
    view->news_feeds = katze_array_new (KATZE_TYPE_ITEM);

    view->item = NULL;
    view->scrollh = view->scrollv = -2;
    view->back_forward_set = FALSE;

    view->download_manager = NULL;
    view->news_aggregator = NULL;
    view->web_view = NULL;
    /* Adjustments are not created initially, but overwritten later */
    view->scrolled_window = katze_scrolled_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view->scrolled_window),
                                         GTK_SHADOW_NONE);
    gtk_container_add (GTK_CONTAINER (view), view->scrolled_window);

    g_signal_connect (view->scrolled_window, "notify::hadjustment",
        G_CALLBACK (midori_view_notify_hadjustment_cb), view);
    g_signal_connect (view->scrolled_window, "notify::vadjustment",
        G_CALLBACK (midori_view_notify_vadjustment_cb), view);
}

static void
midori_view_finalize (GObject* object)
{
    MidoriView* view;

    view = MIDORI_VIEW (object);

    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);
    if (view->item)
        g_signal_handlers_disconnect_by_func (view->item,
            midori_view_item_meta_data_changed, view);

    if (view->thumb_view)
        gtk_widget_destroy (view->thumb_view);

    katze_assign (view->uri, NULL);
    katze_assign (view->title, NULL);
    katze_object_assign (view->icon, NULL);
    katze_assign (view->icon_uri, NULL);
    g_hash_table_destroy (view->memory);
    katze_assign (view->statusbar_text, NULL);
    katze_assign (view->link_uri, NULL);
    katze_assign (view->selected_text, NULL);
    katze_object_assign (view->news_feeds, NULL);

    katze_object_assign (view->settings, NULL);
    katze_object_assign (view->item, NULL);

    katze_assign (view->download_manager, NULL);
    katze_assign (view->news_aggregator, NULL);

    katze_object_assign (view->net, NULL);

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
        midori_view_update_title (view);
        break;
    case PROP_MINIMIZED:
        view->minimized = g_value_get_boolean (value);
        if (view->item)
        {
            g_signal_handlers_block_by_func (view->item,
                midori_view_item_meta_data_changed, view);
            katze_item_set_meta_integer (view->item, "minimized",
                                         view->minimized ? 1 : -1);
            g_signal_handlers_unblock_by_func (view->item,
                midori_view_item_meta_data_changed, view);
        }
        if (view->tab_label)
            sokoke_widget_set_visible (view->tab_title, !view->minimized);
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
    case PROP_NET:
        katze_object_assign (view->net, g_value_dup_object (value));
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
    case PROP_SECURITY:
        g_value_set_enum (value, view->security);
        break;
    case PROP_MIME_TYPE:
        g_value_set_string (value, view->mime_type);
        break;
    case PROP_ICON:
        g_value_set_object (value, view->icon);
        break;
    case PROP_PROGRESS:
        g_value_set_double (value, midori_view_get_progress (view));
        break;
    case PROP_LOAD_STATUS:
        g_value_set_enum (value, midori_view_get_load_status (view));
        break;
    case PROP_MINIMIZED:
        g_value_set_boolean (value, view->minimized);
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_float (value, midori_view_get_zoom_level (view));
        break;
    case PROP_NEWS_FEEDS:
        g_value_set_object (value, view->news_feeds);
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, view->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, view->settings);
        break;
    case PROP_NET:
        g_value_set_object (value, view->net);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean
midori_view_focus_in_event (GtkWidget*     widget,
                            GdkEventFocus* event)
{
    MidoriView* view = MIDORI_VIEW (widget);

    /* Always propagate focus to the child web view,
     * create it if it's not there yet. */
    if (!view->web_view)
        midori_view_construct_web_view (view);
    gtk_widget_grab_focus (view->web_view);
    return TRUE;
}

/**
 * midori_view_new:
 * @net: a #KatzeNet, or %NULL
 *
 * Creates a new view.
 *
 * Return value: a new #MidoriView
 **/
GtkWidget*
midori_view_new (KatzeNet* net)
{
    return g_object_new (MIDORI_TYPE_VIEW, NULL);
}

static void
_midori_view_update_settings (MidoriView* view)
{
    gboolean zoom_text_and_images, kinetic_scrolling;

    g_object_get (view->settings,
        "speed-dial-in-new-tabs", &view->speed_dial_in_new_tabs,
        "download-manager", &view->download_manager,
        "news-aggregator", &view->news_aggregator,
        "zoom-text-and-images", &zoom_text_and_images,
        "kinetic-scrolling", &kinetic_scrolling,
        "close-buttons-on-tabs", &view->close_buttons_on_tabs,
        "open-new-pages-in", &view->open_new_pages_in,
        "ask-for-destination-folder", &view->ask_for_destination_folder,
        "middle-click-opens-selection", &view->middle_click_opens_selection,
        "open-tabs-in-the-background", &view->open_tabs_in_the_background,
        "find-while-typing", &view->find_while_typing,
        NULL);

    if (view->web_view)
        g_object_set (view->web_view,
                      "full-content-zoom", zoom_text_and_images, NULL);
    g_object_set (view->scrolled_window, "kinetic-scrolling", kinetic_scrolling, NULL);
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

    if (name == g_intern_string ("speed-dial-in-new-tabs"))
    {
        view->speed_dial_in_new_tabs = g_value_get_boolean (&value);
    }
    else if (name == g_intern_string ("download-manager"))
    {
        katze_assign (view->download_manager, g_value_dup_string (&value));
    }
    else if (name == g_intern_string ("news-aggregator"))
    {
        katze_assign (view->news_aggregator, g_value_dup_string (&value));
    }
    else if (name == g_intern_string ("zoom-text-and-images"))
    {
        if (view->web_view)
            g_object_set (view->web_view, "full-content-zoom",
                          g_value_get_boolean (&value), NULL);
    }
    else if (name == g_intern_string ("kinetic-scrolling"))
    {
        g_object_set (view, "kinetic-scrolling",
                      g_value_get_boolean (&value), NULL);
    }
    else if (name == g_intern_string ("close-buttons-on-tabs"))
    {
        view->close_buttons_on_tabs = g_value_get_boolean (&value);
        sokoke_widget_set_visible (view->tab_close,
                                   view->close_buttons_on_tabs);
    }
    else if (name == g_intern_string ("open-new-pages-in"))
    {
        view->open_new_pages_in = g_value_get_enum (&value);
    }
    else if (name == g_intern_string ("ask-for-destination-folder"))
    {
        view->ask_for_destination_folder = g_value_get_boolean (&value);
    }
    else if (name == g_intern_string ("middle-click-opens-selection"))
    {
        view->middle_click_opens_selection = g_value_get_boolean (&value);
    }
    else if (name == g_intern_string ("open-tabs-in-the-background"))
    {
        view->open_tabs_in_the_background = g_value_get_boolean (&value);
    }
    else if (name == g_intern_string ("find-while-typing"))
    {
        view->find_while_typing = g_value_get_boolean (&value);
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
    g_return_if_fail (MIDORI_IS_VIEW (view));
    g_return_if_fail (!settings || MIDORI_IS_WEB_SETTINGS (settings));

    if (view->settings == settings)
        return;

    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);

    katze_object_assign (view->settings, settings);
    if (settings)
    {
        g_object_ref (settings);
        if (view->web_view)
            g_object_set (view->web_view, "settings", settings, NULL);
        _midori_view_update_settings (view);
        g_signal_connect (settings, "notify",
            G_CALLBACK (midori_view_settings_notify_cb), view);
    }
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
midori_view_web_inspector_construct_window (gpointer       inspector,
                                            WebKitWebView* web_view,
                                            GtkWidget*     inspector_view,
                                            MidoriView*    view)
{
    gchar* title;
    GtkWidget* window;
    GtkWidget* toplevel;
    GdkScreen* screen;
    gint width, height;
    GtkIconTheme* icon_theme;
    GdkPixbuf* icon;
    GdkPixbuf* gray_icon;

    title = g_strdup_printf (_("Inspect page - %s"), "");
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), title);
    g_free (title);

    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
    if (gtk_widget_is_toplevel (toplevel))
    {
        screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
        width = gdk_screen_get_width (screen) / 1.7;
        height = gdk_screen_get_height (screen) / 1.7;
        gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    }

    /* Attempt to make a gray version of the icon on the fly */
    icon_theme = gtk_icon_theme_get_for_screen (
        gtk_widget_get_screen (GTK_WIDGET (view)));
    icon = gtk_icon_theme_load_icon (icon_theme, "midori", 32,
        GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    if (icon)
    {
        gray_icon = gdk_pixbuf_copy (icon);
        if (gray_icon)
        {
            gdk_pixbuf_saturate_and_pixelate (gray_icon, gray_icon, 0.1f, FALSE);
            gtk_window_set_icon (GTK_WINDOW (window), gray_icon);
            g_object_unref (gray_icon);
        }
        g_object_unref (icon);
    }
    else
        gtk_window_set_icon_name (GTK_WINDOW (window), "midori");
    gtk_container_add (GTK_CONTAINER (window), inspector_view);
    gtk_widget_show_all (window);

    /* FIXME: Update window title with URI */
}

static WebKitWebView*
midori_view_web_inspector_inspect_web_view_cb (gpointer       inspector,
                                               WebKitWebView* web_view,
                                               MidoriView*    view)
{
    GtkWidget* inspector_view = webkit_web_view_new ();
    #if HAVE_HILDON
    gtk_widget_tap_and_hold_setup (view->web_view, NULL, NULL, 0);
    g_signal_connect (view->web_view, "tap-and-hold",
                      G_CALLBACK (midori_view_web_view_tap_and_hold_cb), NULL);
    #endif
    midori_view_web_inspector_construct_window (inspector,
        web_view, inspector_view, view);
    return WEBKIT_WEB_VIEW (inspector_view);
}

static gboolean
midori_view_web_inspector_show_window_cb (gpointer    inspector,
                                          MidoriView* view)
{
    GtkWidget* inspector_view;
    GtkWidget* window;

    g_object_get (inspector, "web-view", &inspector_view, NULL);
    window = gtk_widget_get_toplevel (inspector_view);
    if (!window)
        return FALSE;
    gtk_window_present (GTK_WINDOW (window));
    return TRUE;
}

static gboolean
midori_view_web_inspector_attach_window_cb (gpointer    inspector,
                                            MidoriView* view)
{
    GtkWidget* inspector_view = katze_object_get_object (inspector, "web-view");
    g_signal_emit (view, signals[ATTACH_INSPECTOR], 0, inspector_view);
    g_object_unref (inspector_view);
    return TRUE;
}

static gboolean
midori_view_web_inspector_detach_window_cb (gpointer    inspector,
                                            MidoriView* view)
{
    GtkWidget* inspector_view = katze_object_get_object (inspector, "web-view");
    GtkWidget* parent = gtk_widget_get_parent (inspector_view);
    if (GTK_IS_WINDOW (parent))
        return FALSE;
    gtk_widget_hide (parent);
    gtk_container_remove (GTK_CONTAINER (parent), inspector_view);
    midori_view_web_inspector_construct_window (inspector,
        WEBKIT_WEB_VIEW (view->web_view), inspector_view, view);
    g_object_unref (inspector_view);
    return TRUE;
}

static void
midori_view_construct_web_view (MidoriView* view)
{
    WebKitWebFrame* web_frame;
    gpointer inspector;

    g_return_if_fail (!view->web_view);

    view->web_view = webkit_web_view_new ();

    /* Load something to avoid a bug where WebKit might not set a main frame */
    webkit_web_view_open (WEBKIT_WEB_VIEW (view->web_view), "");
    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));

    #if HAVE_HILDON
    gtk_widget_tap_and_hold_setup (view->web_view, NULL, NULL, 0);
    g_signal_connect (view->web_view, "tap-and-hold",
                      G_CALLBACK (midori_view_web_view_tap_and_hold_cb), NULL);
    #endif

    g_object_connect (view->web_view,
                      "signal::navigation-policy-decision-requested",
                      midori_view_web_view_navigation_decision_cb, view,
                      #if WEBKIT_CHECK_VERSION (1, 1, 14)
                      "signal::resource-request-starting",
                      midori_view_web_view_resource_request_cb, view,
                      #endif
                      "signal::load-started",
                      webkit_web_view_load_started_cb, view,
                      "signal::load-committed",
                      webkit_web_view_load_committed_cb, view,
                      "signal::load-progress-changed",
                      webkit_web_view_progress_changed_cb, view,
                      "signal::load-finished",
                      webkit_web_view_load_finished_cb, view,
                      #if WEBKIT_CHECK_VERSION (1, 1, 18)
                      "signal::notify::icon-uri",
                      midori_web_view_notify_icon_uri_cb, view,
                      #endif
                      #if WEBKIT_CHECK_VERSION (1, 1, 4)
                      "signal::notify::uri",
                      webkit_web_view_notify_uri_cb, view,
                      "signal::notify::title",
                      webkit_web_view_notify_title_cb, view,
                      #else
                      "signal::title-changed",
                      webkit_web_view_title_changed_cb, view,
                      #endif
                      "signal::status-bar-text-changed",
                      webkit_web_view_statusbar_text_changed_cb, view,
                      "signal::leave-notify-event",
                      midori_view_web_view_leave_notify_event_cb, view,
                      "signal::hovering-over-link",
                      webkit_web_view_hovering_over_link_cb, view,
                      "signal::button-press-event",
                      gtk_widget_button_press_event_cb, view,
                      "signal-after::key-press-event",
                      gtk_widget_key_press_event_cb, view,
                      "signal::scroll-event",
                      gtk_widget_scroll_event_cb, view,
                      "signal::populate-popup",
                      webkit_web_view_populate_popup_cb, view,
                      "signal::console-message",
                      webkit_web_view_console_message_cb, view,
                      "signal::window-object-cleared",
                      webkit_web_view_window_object_cleared_cb, view,
                      "signal::create-web-view",
                      webkit_web_view_create_web_view_cb, view,
                      "signal-after::mime-type-policy-decision-requested",
                      webkit_web_view_mime_type_decision_cb, view,
                      #if WEBKIT_CHECK_VERSION (1, 1, 3)
                      "signal::download-requested",
                      webkit_web_view_download_requested_cb, view,
                      #endif
                      #if WEBKIT_CHECK_VERSION (1, 1, 5)
                      "signal::print-requested",
                      midori_view_web_view_print_requested_cb, view,
                      #endif
                      #if WEBKIT_CHECK_VERSION (1, 1, 6)
                      "signal-after::load-error",
                      webkit_web_view_load_error_cb, view,
                      #endif
                      NULL);

    #if !WEBKIT_CHECK_VERSION (1, 1, 6)
    g_object_connect (web_frame,
                      "signal::load-done",
                      webkit_web_frame_load_done_cb, view,
                      NULL);
    #endif

    if (view->settings)
    {
        g_object_set (view->web_view, "settings", view->settings,
            "full-content-zoom", katze_object_get_boolean (view->settings,
                "zoom-text-and-images"), NULL);
    }

    gtk_container_add (GTK_CONTAINER (view->scrolled_window), view->web_view);
    gtk_widget_show_all (view->scrolled_window);

    inspector = katze_object_get_object (view->web_view, "web-inspector");
    g_object_connect (inspector,
                      "signal::inspect-web-view",
                      midori_view_web_inspector_inspect_web_view_cb, view,
                      "signal::show-window",
                      midori_view_web_inspector_show_window_cb, view,
                      "signal::attach-window",
                      midori_view_web_inspector_attach_window_cb, view,
                      "signal::detach-window",
                      midori_view_web_inspector_detach_window_cb, view,
                      NULL);
    g_object_unref (inspector);
}

/**
 * midori_view_set_uri:
 * @view: a #MidoriView
 *
 * Opens the specified URI in the view.
 **/
void
midori_view_set_uri (MidoriView*  view,
                     const gchar* uri)
{
    gchar* data;

    g_return_if_fail (MIDORI_IS_VIEW (view));

    /* Treat "about:blank" and "" equally, see midori_view_is_blank(). */
    if (!uri || !strcmp (uri, "about:blank")) uri = "";

    if (g_getenv ("MIDORI_UNARMED") == NULL)
    {
        if (!view->web_view)
            midori_view_construct_web_view (view);

        if (view->speed_dial_in_new_tabs && !strcmp (uri, ""))
        {
            #if !WEBKIT_CHECK_VERSION (1, 1, 14)
            SoupServer* res_server;
            guint port;
            #endif
            gchar* res_root;
            gchar* speed_dial_head;
            gchar* speed_dial_body;
            gchar* body_fname;
            gchar* stock_root;
            gchar* filepath;

            katze_assign (view->uri, g_strdup (""));

            filepath = sokoke_find_data_filename ("midori/res/speeddial-head.html");
            g_file_get_contents (filepath, &speed_dial_head, NULL, NULL);
            g_free (filepath);
            if (G_UNLIKELY (!speed_dial_head))
                speed_dial_head = g_strdup ("");

            #if WEBKIT_CHECK_VERSION (1, 1, 14)
            res_root = g_strdup ("res:/");
            stock_root = g_strdup ("stock:/");
            #else
            res_server = sokoke_get_res_server ();
            port = soup_server_get_port (res_server);
            res_root = g_strdup_printf ("http://localhost:%d/res", port);
            stock_root = g_strdup_printf ("http://localhost:%d/stock", port);
            #endif
            body_fname = g_build_filename (sokoke_set_config_dir (NULL),
                                           "speeddial.json", NULL);

            if (g_access (body_fname, F_OK) != 0)
            {
                filepath = sokoke_find_data_filename ("midori/res/speeddial.json");
                if (g_file_get_contents (filepath,
                                         &speed_dial_body, NULL, NULL))
                    g_file_set_contents (body_fname, speed_dial_body, -1, NULL);
                else
                    speed_dial_body = g_strdup ("");
                g_free (filepath);
            }
            else
                g_file_get_contents (body_fname, &speed_dial_body, NULL, NULL);

            data = sokoke_replace_variables (speed_dial_head,
                "{res}", res_root,
                "{stock}", stock_root,
                "{json_data}", speed_dial_body,
                "{title}", _("Speed dial"),
                "{click_to_add}", _("Click to add a shortcut"),
                "{enter_shortcut_address}", _("Enter shortcut address"),
                "{enter_shortcut_name}", _("Enter shortcut title"),
                "{are_you_sure}", _("Are you sure you want to delete this shortcut?"),
                "{set_dial_size}", _("Set number of columns and rows"),
                "{enter_dial_size}", _("Enter number of columns and rows:"),
                "{invalid_dial_size}", _("Invalid input for the size of the speed dial"),
                "{set_thumb_size}", _("Thumb size:"),
                "{set_thumb_small}", _("Small"),
                "{set_thumb_normal}", _("Medium"),
                "{set_thumb_big}", _("Big"),  NULL);


            midori_view_load_alternate_string (view,
                data, res_root, "about:blank", NULL);

            g_free (res_root);
            g_free (stock_root);
            g_free (data);
            g_free (speed_dial_head);
            g_free (speed_dial_body);
            g_free (body_fname);
        }
        /* This is not prefectly elegant, but creating
           special pages inline is the simplest solution. */
        else if (g_str_has_prefix (uri, "error:") || g_str_has_prefix (uri, "about:"))
        {
            data = NULL;
            #if !WEBKIT_CHECK_VERSION (1, 1, 3)
            if (!strncmp (uri, "error:nodisplay ", 16))
            {
                gchar* title;
                gchar* logo_path;
                gchar* logo_uri;

                katze_assign (view->uri, g_strdup (&uri[16]));
                title = g_strdup_printf (_("Document cannot be displayed"));
                logo_path = sokoke_find_data_filename ("midori/logo-shade.png");
                logo_uri = g_filename_to_uri (logo_path, NULL, NULL);
                g_free (logo_path);
                data = g_strdup_printf (
                    "<html><head><title>%s</title></head>"
                    "<body><h1>%s</h1>"
                    "<img src=\"%s\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "<p />The document %s of type '%s' cannot be displayed."
                    "</body></html>",
                    title, title, logo_uri, view->uri, view->mime_type);
                g_free (title);
                g_free (logo_uri);
            }
            #endif
            if (!strncmp (uri, "error:nodocs ", 13))
            {
                gchar* title;
                gchar* logo_path;
                gchar* logo_uri;

                katze_assign (view->uri, g_strdup (&uri[13]));
                title = g_strdup_printf (_("No documentation installed"));
                logo_path = sokoke_find_data_filename ("midori/logo-shade.png");
                logo_uri = g_filename_to_uri (logo_path, NULL, NULL);
                g_free (logo_path);
                data = g_strdup_printf (
                    "<html><head><title>%s</title></head>"
                    "<body><h1>%s</h1>"
                    "<img src=\"%s\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "<p />There is no documentation installed at %s."
                    "You may want to ask your distribution or "
                    "package maintainer for it or if this a custom build "
                    "verify that the build is setup properly."
                    "</body></html>",
                    title, title, logo_uri, view->uri);
                g_free (title);
                g_free (logo_uri);
            }
            else if (!strcmp (uri, "about:version"))
            {
                gchar** argument_vector = sokoke_get_argv (NULL);
                gchar* command_line = g_strjoinv (" ", argument_vector);
                gchar* ident = katze_object_get_string (view->settings, "user-agent");
                #if defined (G_OS_WIN32)
                gchar* sys_name = g_strdup ("Windows");
                #else
                gchar* sys_name;
                struct utsname name;
                if (uname (&name) != -1)
                    sys_name = g_strdup_printf ("%s %s", name.sysname, name.machine);
                else
                    sys_name = g_strdup ("Unix");
                #endif

                katze_assign (view->uri, g_strdup (uri));
                #ifndef WEBKIT_USER_AGENT_MAJOR_VERSION
                    #define WEBKIT_USER_AGENT_MAJOR_VERSION 532
                    #define WEBKIT_USER_AGENT_MINOR_VERSION 1
                #endif
                #if defined (HAVE_LIBSOUP_2_29_3)
                    #define LIBSOUP_VERSION "2.29.3"
                #elif defined (HAVE_LIBSOUP_2_27_90)
                    #define LIBSOUP_VERSION "2.27.90"
                #else
                    #define LIBSOUP_VERSION "2.25.2"
                #endif
                #ifdef G_ENABLE_DEBUG
                    #define DEBUGGING " (Debug)"
                #else
                    #define DEBUGGING ""
                #endif
                data = g_strdup_printf (
                    "<html><head><title>about:version</title></head>"
                    "<body><h1>about:version</h1>"
                    "<img src=\"res://logo-shade.png\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "<table>"
                    "<tr><td>Command line</td><td>%s</td></tr>"
                    "<tr><td>Midori</td><td>" PACKAGE_VERSION "%s</td></tr>"
                    "<tr><td>WebKitGTK+</td><td>%d.%d.%d (%d.%d.%d)</td></tr>"
                    "<tr><td>GTK+</td><td>%d.%d.%d (%d.%d.%d)</td></tr>"
                    "<tr><td>Glib</td><td>%d.%d.%d (%d.%d.%d)</td></tr>"
                    "<tr><td>libsoup</td><td>%s</td></tr>"
                    "<tr><td>sqlite3</td><td>%s</td></tr>"
                    "<tr><td>libnotify</td><td>%s</td></tr>"
                    "<tr><td>libidn</td><td>%s</td></tr>"
                    "<tr><td>libunique</td><td>%s</td></tr>"
                    "<tr><td>libhildon</td><td>%s</td></tr>"
                    "<tr><td>Platform</td><td>%s</td></tr>"
                    "<tr><td>Identification</td><td>%s</td></tr>"
                    "</table>"
                    "</body></html>",
                    command_line,
                    DEBUGGING,
                    WEBKIT_MAJOR_VERSION,
                    WEBKIT_MINOR_VERSION,
                    WEBKIT_MICRO_VERSION,
                    webkit_major_version (),
                    webkit_minor_version (),
                    webkit_micro_version (),
                    GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                    gtk_major_version, gtk_minor_version, gtk_micro_version,
                    GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
                    glib_major_version, glib_minor_version, glib_micro_version,
                    LIBSOUP_VERSION,
                    HAVE_SQLITE ? "Yes" : "No",
                    HAVE_LIBNOTIFY ? "Yes" : "No",
                    HAVE_LIBIDN ? "Yes" : "No",
                    HAVE_UNIQUE ? "Yes" : "No",
                    HAVE_HILDON ? "Yes" : "No",
                    sys_name, ident);
                g_free (command_line);
                g_free (ident);
                g_free (sys_name);
            }
            else
            {
                katze_assign (view->uri, g_strdup (uri));
                data = g_strdup_printf (
                    "<html><head><title>%s</title></head><body><h1>%s</h1>"
                    "<img src=\"file://" MDATADIR "/midori/logo-shade.png\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "</body></html>", view->uri, view->uri);
            }

            webkit_web_view_load_html_string (
                WEBKIT_WEB_VIEW (view->web_view), data, view->uri);
            g_free (data);
            g_object_notify (G_OBJECT (view), "uri");
            if (view->item)
                katze_item_set_uri (view->item, uri);
            return;
        }
        else if (g_str_has_prefix (uri, "pause:"))
        {
            gchar* title;

            title = g_strdup_printf ("%s", view->title);
            katze_assign (view->uri, g_strdup (&uri[6]));
            midori_view_display_error (
                view, view->uri, title,
                _("Page loading delayed"),
                _("Loading delayed either due to a recent crash or startup preferences."),
                _("Load Page"),
                NULL);
            g_free (title);
            g_object_notify (G_OBJECT (view), "uri");
            if (view->item)
                katze_item_set_uri (view->item, uri);
        }
        else if (g_str_has_prefix (uri, "javascript:"))
        {
            gboolean result;
            gchar* exception;

            result = midori_view_execute_script (view, &uri[11], &exception);
            if (!result)
            {
                sokoke_message_dialog (GTK_MESSAGE_ERROR, "javascript:", exception);
                g_free (exception);
            }
        }
        else if (g_str_has_prefix (uri, "mailto:")
              || g_str_has_prefix (uri, "tel:")
              || g_str_has_prefix (uri, "callto:"))
        {
            sokoke_show_uri (NULL, uri, GDK_CURRENT_TIME, NULL);
        }
        else
        {
            katze_assign (view->uri, sokoke_format_uri_for_display (uri));
            g_object_notify (G_OBJECT (view), "uri");
            if (view->item)
                katze_item_set_uri (view->item, uri);
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
 * Retrieves the icon of the view, or a default icon. See
 * midori_view_get_icon_uri() if you need to distinguish
 * the origin of an icon.
 *
 * The returned icon is owned by the @view and must not be modified.
 *
 * Return value: a #GdkPixbuf, or %NULL
 **/
GdkPixbuf*
midori_view_get_icon (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->icon;
}

/**
 * midori_view_get_icon_uri:
 * @view: a #MidoriView
 *
 * Retrieves the address of the icon of the view
 * if the loaded website has an icon, otherwise
 * %NULL.
 * Note that if there is no icon uri, midori_view_get_icon()
 * will still return a default icon.
 *
 * The returned string is owned by the @view and must not be freed.
 *
 * Return value: a string, or %NULL
 *
 * Since: 0.2.5
 **/
const gchar*
midori_view_get_icon_uri (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->icon_uri;
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

    if (view->title && *view->title)
        return view->title;
    if (midori_view_is_blank (view))
        return _("Blank page");
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

    katze_assign (view->selected_text, webkit_web_view_get_selected_text (
        WEBKIT_WEB_VIEW (view->web_view)));
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
        return view->selected_text;
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
        view->menu_item = katze_image_menu_item_new_ellipsized (title);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (view->menu_item),
            gtk_image_new_from_pixbuf (view->icon));

        g_signal_connect (view->menu_item, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->menu_item);
    }
    return view->menu_item;
}

static void
midori_view_tab_label_menu_open_cb (GtkWidget* menuitem,
                                    GtkWidget* view)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view);
    midori_browser_set_current_tab (browser, view);
}


static void
midori_view_tab_label_menu_duplicate_tab_cb (GtkWidget*  menuitem,
                                             MidoriView* view)
{
    MidoriNewView where = MIDORI_NEW_VIEW_TAB;
    GtkWidget* new_view = g_object_new (MIDORI_TYPE_VIEW,
        "settings", view->settings, NULL);
    midori_view_set_uri (MIDORI_VIEW (new_view),
        midori_view_get_display_uri (view));
    gtk_widget_show (new_view);
    g_signal_emit (view, signals[NEW_VIEW], 0, new_view, where);
}

static void
midori_view_browser_close_tabs_cb (GtkWidget* view,
                                   gpointer   data)
{
    GtkWidget* remaining_view = data;
    if (view != remaining_view)
        gtk_widget_destroy (view);
}

static void
midori_view_tab_label_menu_close_other_tabs_cb (GtkWidget* menuitem,
                                                GtkWidget* view)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view);
    midori_browser_foreach (browser, midori_view_browser_close_tabs_cb, view);
}

static void
midori_view_tab_label_menu_minimize_tab_cb (GtkWidget*  menuitem,
                                            MidoriView* view)
{
    g_object_set (view, "minimized", !view->minimized, NULL);
}

static void
midori_view_tab_label_menu_close_cb (GtkWidget* menuitem,
                                     GtkWidget* view)
{
    gtk_widget_destroy (view);
}

/**
 * midori_view_get_tab_menu:
 * @view: a #MidoriView
 *
 * Retrieves a menu that is typically shown when right-clicking
 * a tab label or equivalent representation.
 *
 * Return value: a #GtkMenu
 *
 * Since: 0.1.8
 **/
GtkWidget*
midori_view_get_tab_menu (MidoriView* view)
{
    MidoriBrowser* browser;
    GtkActionGroup* actions;
    GtkWidget* menu;
    GtkWidget* menuitem;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    browser = midori_browser_get_for_widget (GTK_WIDGET (view));
    actions = midori_browser_get_action_group (browser);

    menu = gtk_menu_new ();
    menuitem = sokoke_action_create_popup_menu_item (
        gtk_action_group_get_action (actions, "TabNew"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = sokoke_action_create_popup_menu_item (
        gtk_action_group_get_action (actions, "UndoTabClose"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_open_cb), view);
    menuitem = gtk_image_menu_item_new_from_stock (STOCK_WINDOW_NEW, NULL);
    gtk_menu_item_set_label (GTK_MENU_ITEM (menuitem), _("Open in New _Window"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_window_new_cb), view);
    menuitem = gtk_menu_item_new_with_mnemonic (_("_Duplicate Tab"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_duplicate_tab_cb), view);
    menuitem = gtk_menu_item_new_with_mnemonic (
        view->minimized ? _("_Restore Tab") : _("_Minimize Tab"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_minimize_tab_cb), view);
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = gtk_menu_item_new_with_mnemonic (_("Close ot_her Tabs"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_close_other_tabs_cb), view);
    menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_close_cb), view);
    gtk_widget_show_all (menu);

    return menu;
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
    else if (event->button == 3)
    {
        /* Show a context menu on right click */
        GtkWidget* menu = midori_view_get_tab_menu (MIDORI_VIEW (widget));

        katze_widget_popup (widget, GTK_MENU (menu),
                            event, KATZE_MENU_POSITION_CURSOR);
        return TRUE;
    }

    return FALSE;
}

static void
midori_view_tab_close_clicked (GtkWidget* tab_close,
                               GtkWidget* widget)
{
    gtk_widget_destroy (widget);
}

static void
midori_view_tab_icon_style_set_cb (GtkWidget* tab_close,
                                   GtkStyle*  previous_style)
{
    GtkRequisition size;
    gtk_widget_size_request (gtk_bin_get_child (GTK_BIN (tab_close)), &size);
    gtk_widget_set_size_request (tab_close, size.width, size.height);
}

static void
midori_view_update_tab_title (GtkWidget* label,
                              gint       size,
                              gdouble    angle)
{
    gint width;

    sokoke_widget_get_text_size (label, "M", &width, NULL);
    if (angle == 0.0 || angle == 360.0)
    {
        gtk_widget_set_size_request (label, width * size, -1);
        if (gtk_label_get_ellipsize (GTK_LABEL (label)) != PANGO_ELLIPSIZE_START)
            gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    }
    else
    {
        gtk_widget_set_size_request (label, -1, width * size);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_NONE);
    }
    gtk_label_set_angle (GTK_LABEL (label), angle);
}

static void
gtk_box_repack (GtkBox*    box,
                GtkWidget* child)
{
    GtkWidget* old_box;
    gboolean expand, fill;
    guint padding;
    GtkPackType pack_type;

    old_box = gtk_widget_get_parent (child);
    g_return_if_fail (GTK_IS_BOX (old_box));

    gtk_box_query_child_packing (GTK_BOX (old_box), child,
        &expand, &fill, &padding, &pack_type);

    g_object_ref (child);
    gtk_container_remove (GTK_CONTAINER (old_box), child);
    if (pack_type == GTK_PACK_START)
        gtk_box_pack_start (box, child, expand, fill, padding);
    else
        gtk_box_pack_end (box, child, expand, fill, padding);
    g_object_unref (child);
}

static void
midori_view_tab_label_parent_set (GtkWidget*  tab_label,
                                  GtkObject*  old_parent,
                                  MidoriView* view)
{
    GtkWidget* parent;

    /* FIXME: Disconnect orientation notification
    if (old_parent)
        ; */

    if (!(parent = gtk_widget_get_parent (tab_label)))
        return;

    if (GTK_IS_NOTEBOOK (parent))
    {
        GtkPositionType pos;
        gdouble old_angle, angle;
        GtkWidget* box;

        pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (parent));
        old_angle = gtk_label_get_angle (GTK_LABEL (view->tab_title));
        switch (pos)
        {
        case GTK_POS_LEFT:
            angle = 90.0;
            break;
        case GTK_POS_RIGHT:
            angle = 270.0;
            break;
        default:
            angle = 0.0;
        }

        if (old_angle != angle)
        {
            if (angle == 0.0)
                box = gtk_hbox_new (FALSE, 1);
            else
                box = gtk_vbox_new (FALSE, 1);
            gtk_box_repack (GTK_BOX (box), view->tab_icon);
            gtk_box_repack (GTK_BOX (box), view->tab_title);
            gtk_box_repack (GTK_BOX (box), view->tab_close);

            gtk_container_remove (GTK_CONTAINER (tab_label),
                gtk_bin_get_child (GTK_BIN (tab_label)));
            gtk_container_add (GTK_CONTAINER (tab_label), GTK_WIDGET (box));
            gtk_widget_show (box);
        }

        midori_view_update_tab_title (view->tab_title, 10, angle);

        /* FIXME: Connect orientation notification */
    }
}

#if 0
static gboolean
midori_view_tab_label_query_tooltip_cb (GtkWidget*  tab_label,
                                        gint        x,
                                        gint        y,
                                        gboolean    keyboard,
                                        GtkTooltip* tooltip,
                                        MidoriView* view)
{
    if (view->speed_dial_in_new_tabs)
        gtk_tooltip_set_icon (tooltip, midori_view_get_snapshot (view, -160, -107));
    else
        gtk_tooltip_set_text (tooltip, midori_view_get_display_title (view));
    return TRUE;
}
#endif

/**
 * midori_view_get_label_ellipsize:
 * @view: a #MidoriView
 *
 * Determines how labels representing the view should be
 * ellipsized, which is helpful for alternative labels.
 *
 * Return value: how to ellipsize the label
 *
 * Since: 0.1.9
 **/
PangoEllipsizeMode
midori_view_get_label_ellipsize (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), PANGO_ELLIPSIZE_END);

    if (view->tab_label)
        return gtk_label_get_ellipsize (GTK_LABEL (view->tab_title));
    return PANGO_ELLIPSIZE_END;
}

/**
 * midori_view_get_proxy_tab_label:
 * @view: a #MidoriView
 *
 * Retrieves a proxy tab label that is typically used when
 * adding the view to a notebook.
 *
 * Note that the label actually adjusts its orientation
 * to the according tab position when used in a notebook.
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
    GtkWidget* align;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->tab_label)
    {
        view->tab_icon = katze_throbber_new ();
        katze_throbber_set_static_pixbuf (KATZE_THROBBER (view->tab_icon),
            midori_view_get_icon (view));

        view->tab_title = gtk_label_new (midori_view_get_display_title (view));
        gtk_misc_set_alignment (GTK_MISC (view->tab_title), 0.0, 0.5);

        event_box = gtk_event_box_new ();
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
        hbox = gtk_hbox_new (FALSE, 1);
        gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (hbox));
        midori_view_update_tab_title (view->tab_title, 10, 0.0);

        view->tab_close = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (view->tab_close), GTK_RELIEF_NONE);
        gtk_button_set_focus_on_click (GTK_BUTTON (view->tab_close), FALSE);
        rcstyle = gtk_rc_style_new ();
        rcstyle->xthickness = rcstyle->ythickness = 0;
        gtk_widget_modify_style (view->tab_close, rcstyle);
        g_object_unref (rcstyle);
        image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
        gtk_container_add (GTK_CONTAINER (view->tab_close), image);
        align = gtk_alignment_new (1.0, 0.0, 0.0, 0.0);
        gtk_container_add (GTK_CONTAINER (align), view->tab_close);

        #if HAVE_OSX
        gtk_box_pack_end (GTK_BOX (hbox), view->tab_icon, FALSE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (hbox), view->tab_title, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
        #else
        gtk_box_pack_start (GTK_BOX (hbox), view->tab_icon, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), view->tab_title, FALSE, TRUE, 0);
        gtk_box_pack_end (GTK_BOX (hbox), align, FALSE, FALSE, 0);
        #endif
        gtk_widget_show_all (GTK_WIDGET (event_box));

        if (view->minimized)
            gtk_widget_hide (view->tab_title);
        if (!view->close_buttons_on_tabs)
            gtk_widget_hide (view->tab_close);

        g_signal_connect (event_box, "button-release-event",
            G_CALLBACK (midori_view_tab_label_button_release_event), view);
        g_signal_connect (view->tab_close, "style-set",
            G_CALLBACK (midori_view_tab_icon_style_set_cb), NULL);
        g_signal_connect (view->tab_close, "clicked",
            G_CALLBACK (midori_view_tab_close_clicked), view);

        view->tab_label = event_box;
        #if 0
        gtk_widget_set_has_tooltip (view->tab_label, TRUE);
        g_signal_connect (view->tab_label, "query-tooltip",
            G_CALLBACK (midori_view_tab_label_query_tooltip_cb), view);
        #endif
        g_signal_connect (view->tab_icon, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->tab_icon);
        g_signal_connect (view->tab_label, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->tab_label);

        g_signal_connect (view->tab_label, "parent-set",
                          G_CALLBACK (midori_view_tab_label_parent_set),
                          view);
    }
    return view->tab_label;
}

static void
midori_view_item_meta_data_changed (KatzeItem*   item,
                                    const gchar* key,
                                    MidoriView*  view)
{
    if (g_str_equal (key, "minimized"))
        g_object_set (view, "minimized",
            katze_item_get_meta_string (item, key) != NULL, NULL);
    else if (g_str_has_prefix (key, "scroll"))
    {
        gint value = katze_item_get_meta_integer (item, key);
        if (view->scrollh == -2 && key[6] == 'h')
            view->scrollh = value > -1 ? value : 0;
        else if (view->scrollv == -2 && key[6] == 'v')
            view->scrollv = value > -1 ? value : 0;
        else
            return;
    }
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
        g_signal_connect (view->item, "meta-data-changed",
            G_CALLBACK (midori_view_item_meta_data_changed), view);
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
    g_return_val_if_fail (MIDORI_IS_VIEW (view), 1.0f);

    if (view->web_view != NULL)
        return webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (view->web_view));
    return 1.0f;
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

    webkit_web_view_set_zoom_level (
        WEBKIT_WEB_VIEW (view->web_view), zoom_level);
    g_object_notify (G_OBJECT (view), "zoom-level");
}

gboolean
midori_view_can_zoom_in (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    return view->web_view != NULL && !g_str_has_prefix (view->mime_type, "image/");
}

gboolean
midori_view_can_zoom_out (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    return view->web_view != NULL && !g_str_has_prefix (view->mime_type, "image/");
}

gboolean
midori_view_can_view_source (MidoriView* view)
{
    gchar* content_type;
    gchar* text_type;
    gboolean is_text;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (midori_view_is_blank (view))
        return FALSE;

    content_type = g_content_type_from_mime_type (view->mime_type);
    text_type = g_content_type_from_mime_type ("text/plain");
    is_text = g_content_type_is_a (content_type, text_type);
    g_free (content_type);
    g_free (text_type);
    return is_text;
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
can_do (find)

/**
 * midori_view_reload:
 * @view: a #MidoriView
 * @from_cache: whether to allow caching
 *
 * Reloads the view.
 **/
void
midori_view_reload (MidoriView* view,
                    gboolean    from_cache)
{
    gchar* title;

    g_return_if_fail (MIDORI_IS_VIEW (view));

#if WEBKIT_CHECK_VERSION (1, 1, 14)
    title = NULL;
#elif WEBKIT_CHECK_VERSION (1, 1, 6)
    /* WebKit 1.1.6 doesn't handle "alternate content" flawlessly,
       so reloading via Javascript works but not via API calls. */
    title = g_strdup_printf (_("Error - %s"), view->uri);
#else
    /* Error pages are special, we want to try loading the destination
       again, not the error page which isn't even a proper page */
    title = g_strdup_printf (_("Error - %s"), view->uri);
#endif
    if (view->title && title && strstr (title, view->title))
        webkit_web_view_open (WEBKIT_WEB_VIEW (view->web_view), view->uri);
    else if (!(view->uri && *view->uri && strncmp (view->uri, "about:", 6)))
    {
        gchar* uri = g_strdup (view->uri);
        midori_view_set_uri (view, uri);
        g_free (uri);
    }
    else if (from_cache)
        webkit_web_view_reload (WEBKIT_WEB_VIEW (view->web_view));
    else
        webkit_web_view_reload_bypass_cache (WEBKIT_WEB_VIEW (view->web_view));

    g_free (title);
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
 * midori_view_get_previous_page
 * @view: a #MidoriView
 *
 * Determines the previous sub-page in the view.
 *
 * Return value: an URI, or %NULL
 *
 * Since: 0.2.3
 **/
const gchar*
midori_view_get_previous_page (MidoriView* view)
{
    static gchar* uri = NULL;
    WebKitWebFrame* web_frame;
    JSContextRef js_context;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->web_view)
        return NULL;

    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    js_context = webkit_web_frame_get_global_context (web_frame);
    katze_assign (uri, sokoke_js_script_eval (js_context,
        "(function (g) {"
        "var ind = ['prev','←','«','&lt;'];"
        "var nind = ['next','→','»','&gt;'];"
        "for (h in g) {"
        "l = g[h];"
        "for (i in l)"
        "if (l[i].rel && (l[i].rel == ind[0]))"
        "return l[i].href;"
        "for (j in ind)"
        "for (i in l)"
        "if (l[i].innerHTML"
        "&& (l[i].innerHTML.toLowerCase ().indexOf (ind[j]) != -1)"
        "&& (l[i].innerHTML.toLowerCase ().indexOf (nind[j]) == -1))"
        "return l[i].href;"
        "var wa = window.location.href.split (/\\d+/);"
        "var wn = window.location.href.split (/[^\\d]+/);"
        "wn = wn.slice (1,wn.length - 1);"
        "var cand = [];"
        "for (i in wn)"
        "{"
        "cand[i] = '';"
        "for (j = 0; j <= i; j++)"
        "{"
        "cand[i] += wa[j];"
        "if (wn[j])"
        "cand[i] += parseInt (wn[j]) - ((i == j) ? 1 : 0);"
        "}"
        "}"
        "for (j in cand)"
        "for (i in l)"
        "if (cand[j].length && l[i].href && (l[i].href.indexOf (cand[j]) == 0))"
        "return l[i].href;"
        "}"
        "return 0;"
        "}) ([document.getElementsByTagName ('link'),"
        "document.getElementsByTagName ('a')]);", NULL));
    return uri && uri[0] != '0' ? uri : NULL;
}

/**
 * midori_view_get_next_page
 * @view: a #MidoriView
 *
 * Determines the next sub-page in the view.
 *
 * Return value: an URI, or %NULL
 *
 * Since: 0.2.3
 **/
const gchar*
midori_view_get_next_page (MidoriView* view)
{
    static gchar* uri = NULL;
    WebKitWebFrame* web_frame;
    JSContextRef js_context;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->web_view)
        return NULL;

    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    js_context = webkit_web_frame_get_global_context (web_frame);
    katze_assign (uri, sokoke_js_script_eval (js_context,
        "(function (g) {"
        "var ind = ['next','→','»','&gt;'];"
        "var nind = ['prev','←','«','&lt;'];"
        "for (h in g) {"
        "l = g[h];"
        "for (i in l)"
        "if (l[i].rel && (l[i].rel == ind[0]))"
        "return l[i].href;"
        "for (j in ind)"
        "for (i in l)"
        "if (l[i].innerHTML"
        "&& (l[i].innerHTML.toLowerCase ().indexOf (ind[j]) != -1)"
        "&& (l[i].innerHTML.toLowerCase ().indexOf (nind[j]) == -1))"
        "return l[i].href;"
        "var wa = window.location.href.split (/\\d+/);"
        "var wn = window.location.href.split (/[^\\d]+/);"
        "wn = wn.slice (1,wn.length - 1);"
        "var cand = [];"
        "for (i in wn)"
        "{"
        "cand[i] = '';"
        "for (j = 0; j <= i; j++)"
        "{"
        "cand[i] += wa[j];"
        "if (wn[j])"
        "cand[i] += parseInt (wn[j]) + ((i == j) ? 1 : 0);"
        "}"
        "}"
        "for (j in cand)"
        "for (i in l)"
        "if (cand[j].length && l[i].href && (l[i].href.indexOf (cand[j]) == 0))"
        "return l[i].href;"
        "}"
        "return 0;"
        "}) ([document.getElementsByTagName ('link'),"
        "document.getElementsByTagName ('a')]);", NULL));
    return uri && uri[0] != '0' ? uri : NULL;
}
#if WEBKIT_CHECK_VERSION (1, 1, 5)
static GtkWidget*
midori_view_print_create_custom_widget_cb (GtkPrintOperation* operation,
                                           MidoriView*        view)
{
    GtkWidget* box;
    GtkWidget* button;

    box = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 4);
    button = gtk_check_button_new ();
    g_object_set_data (G_OBJECT (operation), "print-backgrounds", button);
    gtk_button_set_label (GTK_BUTTON (button), _("Print background images"));
    gtk_widget_set_tooltip_text (button, _("Whether background images should be printed"));
    if (katze_object_get_boolean (view->settings, "print-backgrounds"))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    gtk_widget_show_all (box);

    return box;
}

static void
midori_view_print_custom_widget_apply_cb (GtkPrintOperation* operation,
                                          GtkWidget*         widget,
                                          MidoriView*        view)
{
    GtkWidget* button;

    button = g_object_get_data (G_OBJECT (operation), "print-backgrounds");
    g_object_set (view->settings,
        "print-backgrounds",
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)),
        NULL);
}
#endif

static void
midori_view_print_response_cb (GtkWidget* dialog,
                               gint       response,
                               gpointer   data)
{
    gtk_widget_destroy (dialog);
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
    WebKitWebFrame* frame;
    #if WEBKIT_CHECK_VERSION (1, 1, 5)
    GtkPrintOperation* operation;
    GError* error;
    #endif

    g_return_if_fail (MIDORI_IS_VIEW (view));

    frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    #if WEBKIT_CHECK_VERSION (1, 1, 5)
    operation = gtk_print_operation_new ();
    gtk_print_operation_set_custom_tab_label (operation, _("Features"));
#if GTK_CHECK_VERSION (2, 18, 0)
    gtk_print_operation_set_embed_page_setup (operation, TRUE);
#endif
    g_signal_connect (operation, "create-custom-widget",
        G_CALLBACK (midori_view_print_create_custom_widget_cb), view);
    g_signal_connect (operation, "custom-widget-apply",
        G_CALLBACK (midori_view_print_custom_widget_apply_cb), view);
    error = NULL;
    webkit_web_frame_print_full (frame, operation,
        GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, &error);
    g_object_unref (operation);

    if (error)
    {
        GtkWidget* window = gtk_widget_get_toplevel (GTK_WIDGET (view));
        GtkWidget* dialog = gtk_message_dialog_new (
            gtk_widget_is_toplevel (window) ? GTK_WINDOW (window) : NULL,
            GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "%s", error->message);
        g_error_free (error);

        g_signal_connect (dialog, "response",
                          G_CALLBACK (midori_view_print_response_cb), NULL);
        gtk_widget_show (dialog);
    }
    #else
    webkit_web_frame_print (frame);
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
            text, case_sensitive, forward, TRUE), NULL);
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

/**
 * midori_view_execute_script
 * @view: a #MidoriView
 * @script: script code
 * @exception: location to store an exception message
 *
 * Execute a script on the view.
 *
 * Returns: %TRUE if the script was executed successfully
 **/
gboolean
midori_view_execute_script (MidoriView*  view,
                            const gchar* script,
                            gchar**      exception)
{
    WebKitWebFrame* web_frame;
    JSContextRef js_context;
    gchar* script_decoded;
    gchar* result;
    gboolean success;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);
    g_return_val_if_fail (script != NULL, FALSE);

    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    js_context = webkit_web_frame_get_global_context (web_frame);
    if ((script_decoded = soup_uri_decode (script)))
    {
        result = sokoke_js_script_eval (js_context, script_decoded, exception);
        g_free (script_decoded);
    }
    else
        result = sokoke_js_script_eval (js_context, script, exception);
    success = result != NULL;
    g_free (result);
    return success;
}

/**
 * midori_view_get_snapshot
 * @view: a #MidoriView
 * @width: the desired width
 * @height: the desired height
 *
 * Take a snapshot of the view at the given dimensions. The
 * view has to be mapped on the screen.
 *
 * If width and height are negative, the resulting
 * image is going to be optimized for speed.
 *
 * Returns: a newly allocated #GdkPixbuf
 *
 * Since: 0.2.1
 **/
GdkPixbuf*
midori_view_get_snapshot (MidoriView* view,
                          gint        width,
                          gint        height)
{
    GtkWidget* web_view;
    gboolean fast;
    gint x, y, w, h;
    GdkRectangle rect;
    GdkPixmap* pixmap;
    GdkEvent event;
    gboolean result;
    GdkColormap* colormap;
    GdkPixbuf* pixbuf;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);
    web_view = view->web_view;
    g_return_val_if_fail (web_view->window, NULL);

    x = web_view->allocation.x;
    y = web_view->allocation.y;
    w = web_view->allocation.width;
    h = web_view->allocation.height;

    /* If width and height are both negative, we try to render faster at
       the cost of correctness or beauty. Only a part of the page is
       rendered which makes it a lot faster and scaling isn't as nice. */
    fast = FALSE;
    if (width < 0 && height < 0)
    {
        width *= -1;
        height *= -1;
        w = w > 320 ? 320 : w;
        h = h > 240 ? 240 : h;
        fast = TRUE;
    }

    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;

    pixmap = gdk_pixmap_new (web_view->window, w, h,
        gdk_drawable_get_depth (web_view->window));
    event.expose.type = GDK_EXPOSE;
    event.expose.window = pixmap;
    event.expose.send_event = FALSE;
    event.expose.count = 0;
    event.expose.area.x = 0;
    event.expose.area.y = 0;
    gdk_drawable_get_size (GDK_DRAWABLE (web_view->window),
        &event.expose.area.width, &event.expose.area.height);
    event.expose.region = gdk_region_rectangle (&event.expose.area);

    g_signal_emit_by_name (web_view, "expose-event", &event, &result);

    colormap = gdk_drawable_get_colormap (pixmap);
    pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap, 0, 0,
                                           0, 0, rect.width, rect.height);
    g_object_unref (pixmap);

    if (width || height)
    {
        GdkPixbuf* scaled;
        if (!width)
            width = rect.width;
        if (!height)
            height = rect.height;

        scaled = gdk_pixbuf_scale_simple (pixbuf, width, height,
            fast ? GDK_INTERP_NEAREST : GDK_INTERP_TILES);
        g_object_unref (pixbuf);
        return scaled;
    }

    return pixbuf;
}

/**
 * midori_view_get_web_view
 * @view: a #MidoriView
 *
 * Returns: The #WebKitWebView for this view
 *
 * Since: 0.2.5
 **/
GtkWidget*
midori_view_get_web_view        (MidoriView*        view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->web_view;
}

/**
 * midori_view_get_security
 * @view: a #MidoriView
 *
 * Returns: The #MidoriSecurity for this view
 *
 * Since: 0.2.5
 **/
MidoriSecurity
midori_view_get_security (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), MIDORI_SECURITY_NONE);

    return view->security;
}

static void
thumb_view_load_status_cb (MidoriView* thumb_view,
                           GParamSpec* pspec,
                           MidoriView* view)
{
    GdkPixbuf* img;
    gchar* file_content;
    gchar* encoded;
    gchar* dom_id;
    gchar* js;
    gsize sz;

    if (katze_object_get_enum (thumb_view, "load-status") != MIDORI_LOAD_FINISHED)
        return;

    gtk_widget_realize (midori_view_get_web_view (MIDORI_VIEW (thumb_view)));
    img = midori_view_get_snapshot (MIDORI_VIEW (thumb_view), 240, 160);
    gdk_pixbuf_save_to_buffer (img, &file_content, &sz, "png", NULL, "compression", "7", NULL);
    encoded = g_base64_encode ((guchar *)file_content, sz );

    /* Call Javascript function to replace shortcut's content */
    dom_id = g_object_get_data (G_OBJECT (thumb_view), "dom-id");
    js = g_strdup_printf ("setThumbnail('%s','%s','%s');",
                          dom_id, encoded, thumb_view->uri);
    webkit_web_view_execute_script (WEBKIT_WEB_VIEW (view->web_view), js);
    free (js);
    g_object_unref (img);

    g_free (dom_id);
    g_free (encoded);
    g_free (file_content);

    g_signal_handlers_disconnect_by_func (
       thumb_view, thumb_view_load_status_cb, view);

    /* Destroying the view here may trigger a WebKitGTK+ 1.1.14 bug */
    #if !WEBKIT_CHECK_VERSION (1, 1, 14) || WEBKIT_CHECK_VERSION (1, 1, 15)
    gtk_widget_destroy (GTK_WIDGET (thumb_view));
    view->thumb_view = NULL;
    #endif
}

/**
 * midori_view_speed_dial_inject_thumb
 * @view: a #MidoriView
 * @filename: filename of the thumbnail
 * @dom_id: Id of the shortcut on speed_dial page in wich to inject content
 * @url: url of the shortcut
 */
static void
midori_view_speed_dial_inject_thumb (MidoriView* view,
                                     gchar*      filename,
                                     gchar*      dom_id,
                                     gchar*      url)
{
    GtkWidget* thumb_view;
    MidoriWebSettings* settings;
    GtkWidget* browser;
    GtkWidget* notebook;
    GtkWidget* label;

    browser = gtk_widget_get_toplevel (GTK_WIDGET (view));
    if (!GTK_IS_WINDOW (browser))
        return;

    /* What we are doing here is a bit of a hack. In order to render a
       thumbnail we need a new view and load the url in it. But it has
       to be visible and packed in a container. So we secretly pack it
       into the notebook of the parent browser. */
    notebook = katze_object_get_object (browser, "notebook");
    if (!notebook)
        return;

    if (!view->thumb_view)
    {
        view->thumb_view = midori_view_new (view->net);
        gtk_container_add (GTK_CONTAINER (notebook), view->thumb_view);
        /* We use an empty label. It's not invisible but at least hard to spot. */
        label = gtk_event_box_new ();
        gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), view->thumb_view, label);
        gtk_widget_show (view->thumb_view);
    }
    g_object_unref (notebook);
    thumb_view = view->thumb_view;
    settings = g_object_new (MIDORI_TYPE_WEB_SETTINGS, "enable-scripts", FALSE,
        "enable-plugins", FALSE, "auto-load-images", TRUE, NULL);
    midori_view_set_settings (MIDORI_VIEW (thumb_view), settings);

    g_object_set_data (G_OBJECT (thumb_view), "dom-id", dom_id);
    g_signal_connect (thumb_view, "notify::load-status",
        G_CALLBACK (thumb_view_load_status_cb), view);
    midori_view_set_uri (MIDORI_VIEW (thumb_view), url);
}

/**
 * midori_view_speed_dial_get_thumb
 * @web_view: a #WebkitView
 * @message: Console log data
 *
 * Load a thumbnail, and set the DOM
 *
 * message[0] == console message call
 * message[1] == shortcut id in the DOM
 * message[2] == shortcut uri
 *
 **/
static void
midori_view_speed_dial_get_thumb (GtkWidget*   web_view,
                                  const gchar* message,
                                  MidoriView*  view)
{
    gchar** t_data = g_strsplit (message," ", 4);

    if (t_data[1] == NULL || t_data[2] == NULL )
        return;

    midori_view_speed_dial_inject_thumb (view, NULL,
        g_strdup (t_data[1]), g_strdup (t_data[2]));
    g_strfreev (t_data);
}

/**
 * midori_view_speed_dial_save
 * @web_view: a #WebkitView
 *
 * Save speed_dial DOM structure to body template
 *
 **/
static void
midori_view_speed_dial_save (GtkWidget*   web_view,
                             const gchar* message)
{
    gchar* json = g_strdup (message + 15);
    gchar* fname = g_build_filename (sokoke_set_config_dir (NULL),
                                     "speeddial.json", NULL);

    GRegex* reg_double = g_regex_new ("\\\\\"", 0, 0, NULL);
    gchar* safe = g_regex_replace_literal (reg_double, json, -1, 0, "\\\\\"", 0, NULL);
    g_file_set_contents (fname, safe, -1, NULL);

    g_free (fname);
    g_free (json);
    g_free (safe);
    g_regex_unref (reg_double);
}
