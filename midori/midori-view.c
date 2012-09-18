/*
 Copyright (C) 2007-2010 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Jean-François Guchens <zcx000@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-view.h"
#include "midori-browser.h"
#include "midori-searchaction.h"
#include "midori-app.h"
#include "midori-platform.h"
#include "midori-core.h"
#include "midori-findbar.h"

#include "marshal.h"

#include <config.h>

#ifdef HAVE_GRANITE
#include <granite.h>
#endif

#ifdef HAVE_GCR
    #define GCR_API_SUBJECT_TO_CHANGE
    #include <gcr/gcr.h>

SoupMessage*
midori_map_get_message (SoupMessage* message);
#endif

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef G_OS_WIN32
    #include <sys/utsname.h>
#endif

#if !WEBKIT_CHECK_VERSION (1, 4, 3)
/* This is unstable API, so we need to declare it */
gchar*
webkit_web_view_get_selected_text (WebKitWebView* web_view);
#endif

static void
midori_view_construct_web_view (MidoriView* view);

static void
midori_view_item_meta_data_changed (KatzeItem*   item,
                                    const gchar* key,
                                    MidoriView*  view);

static void
_midori_view_set_settings (MidoriView*        view,
                           MidoriWebSettings* settings);

static GdkPixbuf*
midori_view_web_view_get_snapshot (GtkWidget* web_view,
                                   gint       width,
                                   gint       height);

static gboolean
midori_view_display_error (MidoriView*     view,
                           const gchar*    uri,
                           const gchar*    title,
                           const gchar*    message,
                           const gchar*    description,
                           const gchar*    try_again,
                           WebKitWebFrame* web_frame);

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
    WebKitHitTestResult* hit_test;
    gchar* link_uri;
    gboolean button_press_handled;
    gboolean has_selection;
    gchar* selected_text;
    MidoriWebSettings* settings;
    GtkWidget* web_view;
    KatzeArray* news_feeds;

    gboolean middle_click_opens_selection;
    gboolean open_tabs_in_the_background;
    gboolean close_buttons_on_tabs;
    MidoriNewPage open_new_pages_in;
    gint find_links;
    gint alerts;

    GtkWidget* menu_item;
    PangoEllipsizeMode ellipsize;
    #ifdef HAVE_GRANITE
    GraniteWidgetsTab* tab;
    #else
    GtkWidget* tab_label;
    GtkWidget* tab_icon;
    GtkWidget* tab_title;
    GtkWidget* tab_close;
    #endif
    KatzeItem* item;
    gint scrollh, scrollv;
    gboolean back_forward_set;
    GtkWidget* scrolled_window;

    #if GTK_CHECK_VERSION (3, 2, 0)
    GtkWidget* overlay;
    GtkWidget* overlay_label;
    GtkWidget* overlay_find;
    #endif
};

struct _MidoriViewClass
{
    GtkVBoxClass parent_class;
};

G_DEFINE_TYPE (MidoriView, midori_view, GTK_TYPE_VBOX);

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
    PROP_SETTINGS
};

enum {
    ACTIVATE_ACTION,
    CONSOLE_MESSAGE,
    ATTACH_INSPECTOR,
    DETACH_INSPECTOR,
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

    /**
     * MidoriView::detach-inspector:
     * @view: the object on which the signal is emitted
     *
     * Emitted when an open inspector that was previously
     * attached to the window is now detached again.
     *
     * Since: 0.3.4
     */
     signals[DETACH_INSPECTOR] = g_signal_new (
        "detach-inspector",
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
     * @user_initiated: %TRUE if the user actively opened the new view
     *
     * Emitted when a new view is created. The value of
     * @where determines where to open the view according
     * to how it was opened and user preferences.
     *
     * Since: 0.1.2
     *
     * Since 0.3.4 a boolean argument was added.
     */
    signals[NEW_VIEW] = g_signal_new (
        "new-view",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__OBJECT_ENUM_BOOLEAN,
        G_TYPE_NONE, 3,
        MIDORI_TYPE_VIEW,
        MIDORI_TYPE_NEW_VIEW,
        G_TYPE_BOOLEAN);

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
                                     flags));

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
                                     NULL,
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
}

static void
midori_view_set_title (MidoriView* view, const gchar* title)
{
    const gchar* display_title;

    if (!title)
        title = view->uri;
    /* Work-around libSoup not setting a proper directory title */
    else if (!strcmp (title, "OMG!") && g_str_has_prefix (view->uri, "file://"))
        title = view->uri;

    katze_assign (view->title, g_strdup (title));

    /* Render filename as title of patches */
    if (title && (g_str_has_suffix (title, ".diff")
               || g_str_has_suffix (title, ".patch")))
    {
        gchar* prefix = strrchr (title, '/');
        if (prefix != NULL)
            katze_assign (view->title, g_strdup (prefix + 1));
    }

    #ifndef G_OS_WIN32
    /* If left-to-right text is combined with right-to-left text the default
       behaviour of Pango can result in awkwardly aligned text. For example
       "‪بستيان نوصر (hadess) | An era comes to an end - Midori" becomes
       "hadess) | An era comes to an end - Midori) بستيان نوصر". So to prevent
       this we insert an LRE character before the title which indicates that
       we want left-to-right but retains the direction of right-to-left text. */
    if (title && !g_str_has_prefix (title, "‪"))
    {
        gchar* new_title = g_strconcat ("‪", view->title, NULL);
        katze_assign (view->title, new_title);
    }
    #endif

    display_title = midori_view_get_display_title (view);
        /* If the title starts with the presumed name of the website, we
            ellipsize differently, to emphasize the subtitle */
            SoupURI* uri = soup_uri_new (view->uri);
            const gchar* host = uri ? (uri->host ? uri->host : "") : "";
            const gchar* name = g_str_has_prefix (host, "www.") ? &host[4] : host;
            guint i = 0;
            while (name[i++])
                if (name[i] == '.')
                    break;
            if (!g_ascii_strncasecmp (display_title, name, i))
                view->ellipsize = PANGO_ELLIPSIZE_START;
            else
                view->ellipsize = PANGO_ELLIPSIZE_END;
            if (uri)
                soup_uri_free (uri);
    #ifdef HAVE_GRANITE
    if (view->tab)
        g_object_set (view->tab,
            "label", display_title, "ellipsize-mode", view->ellipsize, NULL);
    #else
    if (view->tab_label)
    {
        gtk_label_set_text (GTK_LABEL (view->tab_title), display_title);
        gtk_widget_set_tooltip_text (view->tab_icon, display_title);
        gtk_widget_set_tooltip_text (view->tab_title, display_title);
        if (gtk_label_get_angle (GTK_LABEL (view->tab_title)) == 0.0)
            gtk_label_set_ellipsize (GTK_LABEL (view->tab_title), view->ellipsize);
    }
    #endif
    if (view->menu_item)
        gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (
                            view->menu_item))), display_title);
    katze_item_set_name (view->item, display_title);
}

static void
midori_view_apply_icon (MidoriView*  view,
                        GdkPixbuf*   icon,
                        const gchar* icon_name)
{
    katze_item_set_icon (view->item, icon_name);
    /* katze_item_get_image knows about this pixbuf */
    g_object_set_data_full (G_OBJECT (view->item), "pixbuf", g_object_ref (icon),
                            (GDestroyNotify)g_object_unref);
    katze_object_assign (view->icon, icon);
    g_object_notify (G_OBJECT (view), "icon");

    #ifdef HAVE_GRANITE
    if (view->tab)
        g_object_set (view->tab, "icon", icon, NULL);
    #else
    if (view->tab_icon)
    {
        if (icon_name && !strchr (icon_name, '/'))
            katze_throbber_set_static_icon_name (KATZE_THROBBER (view->tab_icon),
                                                 icon_name);
        else
            katze_throbber_set_static_pixbuf (KATZE_THROBBER (view->tab_icon),
                                              view->icon);
    }
    #endif
    if (view->menu_item)
    {
        GtkWidget* image = katze_item_get_image (view->item);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (view->menu_item), image);
    }
}

static void
midori_view_unset_icon (MidoriView* view)
{
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    GIcon* icon;
    gchar** icon_names;
    GdkPixbuf* pixbuf = NULL;

    if (!((screen = gtk_widget_get_screen (view->web_view))
        && (icon_theme = gtk_icon_theme_get_for_screen (screen))))
        return;

    if (view->mime_type != NULL)
    {
        gchar* content_type = g_content_type_from_mime_type (view->mime_type);
        icon = g_content_type_get_icon (content_type);
        g_free (content_type);
        g_themed_icon_append_name (G_THEMED_ICON (icon), "text-html");
    }
    else
        icon = g_themed_icon_new ("text-html");

    if ((icon_names = (gchar**)g_themed_icon_get_names (G_THEMED_ICON (icon))))
    while (*icon_names)
    {
        if ((pixbuf = gtk_icon_theme_load_icon (icon_theme, *icon_names, 16, 0, NULL)))
            break;
        icon_names++;
    }
    g_object_unref (icon);

    midori_view_apply_icon (view, pixbuf, GTK_STOCK_FILE);
}

#if !WEBKIT_CHECK_VERSION (1, 8, 0)
static void
katze_net_object_maybe_unref (gpointer object)
{
    if (object)
        g_object_unref (object);
}

static GHashTable*
midori_view_get_memory (void)
{
    static GHashTable* memory = NULL;
    if (!memory)
        memory = g_hash_table_new_full (g_str_hash, g_str_equal,
            g_free, katze_net_object_maybe_unref);
    return (memory);
}

static gboolean
katze_net_icon_status_cb (KatzeNetRequest*  request,
                          MidoriView*       view)
{
    switch (request->status)
    {
        case KATZE_NET_VERIFIED:
            if (request->mime_type && strncmp (request->mime_type, "image/", 6))
            {
                return FALSE;
            }
            break;
        case KATZE_NET_MOVED:
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

static void
katze_net_icon_transfer_cb (KatzeNetRequest*  request,
                            MidoriView*       view)
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
        gchar* icon_file = katze_net_get_cached_path (NULL, view->icon_uri, "icons");
        if ((fp = fopen (icon_file, "wb")))
        {
            ret  = fwrite (request->data, 1, request->length, fp);
            fclose (fp);
            if ((ret - request->length != 0))
            {
                g_warning ("Error writing to file %s "
                           "in  katze_net_icon_transfer_cb()", icon_file);
            }
        }
        g_free (icon_file);

        pixbuf = katze_pixbuf_new_from_buffer ((guchar*)request->data,
                            request->length, request->mime_type, NULL);

    }

    if (!pixbuf)
    {
        midori_view_unset_icon (view);
        return;
    }

    g_hash_table_insert (midori_view_get_memory (), g_strdup (view->icon_uri), pixbuf);
    settings = gtk_widget_get_settings (view->web_view);
    gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf, icon_width, icon_height, GDK_INTERP_BILINEAR);
    midori_view_apply_icon (view, pixbuf_scaled, view->icon_uri);
}
#endif

static void
_midori_web_view_load_icon (MidoriView* view)
{
    GdkPixbuf* pixbuf = NULL;
    gint icon_width, icon_height;
    GtkSettings* settings;

    #if WEBKIT_CHECK_VERSION (1, 8, 0)
    settings = gtk_widget_get_settings (view->web_view);
    gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
                                       &icon_width, &icon_height);
    if ((pixbuf = webkit_web_view_try_get_favicon_pixbuf (
        WEBKIT_WEB_VIEW (view->web_view), 16, 16)))
        midori_view_apply_icon (view, pixbuf, view->icon_uri);
    #else
    GdkPixbuf* pixbuf_scaled;

    if (!midori_uri_is_http (view->icon_uri))
        katze_assign (view->icon_uri, NULL);

    if (midori_uri_is_http (view->uri) || g_str_has_prefix (view->uri, "file://"))
    {
        gchar* icon_file = NULL;
        if (!view->icon_uri)
        {
            guint i = 8;
            while (view->uri[i] != '\0' && view->uri[i] != '/')
                i++;
            if (view->uri[i] == '/')
            {
                gchar* path = g_strndup (view->uri, i);
                view->icon_uri = g_strdup_printf ("%s/favicon.ico", path);
                g_free (path);
            }
            else
                view->icon_uri = g_strdup_printf ("%s/favicon.ico", view->uri);
        }

        if (g_hash_table_lookup_extended (midori_view_get_memory (),
                                          view->icon_uri, NULL, (gpointer)&pixbuf))
        {
            g_warn_if_fail (pixbuf != NULL);
            g_object_ref (pixbuf);
        }
        else if ((icon_file = katze_net_get_cached_path (NULL, view->icon_uri, "icons")) &&
                 (pixbuf = gdk_pixbuf_new_from_file (icon_file, NULL)))
        {
            g_hash_table_insert (midori_view_get_memory (),
                g_strdup (view->icon_uri), g_object_ref (pixbuf));
        }
        else if (!view->special)
        {
            katze_net_load_uri (NULL, view->icon_uri,
                (KatzeNetStatusCb)katze_net_icon_status_cb,
                (KatzeNetTransferCb)katze_net_icon_transfer_cb, view);
        }

        g_free (icon_file);
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
        midori_view_apply_icon (view, pixbuf, view->icon_uri);
    }
    #endif
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

    #ifdef HAVE_GRANITE
    if (view->tab)
        g_object_set (view->tab, "working",
            midori_view_get_progress (view) > 0.0, NULL);
    #else
    if (view->tab_icon)
        katze_throbber_set_animated (KATZE_THROBBER (view->tab_icon),
            midori_view_get_progress (view) > 0.0);
    #endif
}

static gboolean
midori_view_web_view_navigation_decision_cb (WebKitWebView*             web_view,
                                             WebKitWebFrame*            web_frame,
                                             WebKitNetworkRequest*      request,
                                             WebKitWebNavigationAction* action,
                                             WebKitWebPolicyDecision*   decision,
                                             MidoriView*                view)
{
    JSContextRef js_context;
    gchar* result;
    const gchar* uri = webkit_network_request_get_uri (request);
    if (g_str_has_prefix (uri, "geo:") && strstr (uri, ","))
    {
        gchar* new_uri = sokoke_magic_uri (uri);
        midori_view_set_uri (view, new_uri);
        g_free (new_uri);
        return TRUE;
    }
    else if (sokoke_external_uri (uri))
    {
        if (sokoke_show_uri (gtk_widget_get_screen (GTK_WIDGET (web_view)),
                             uri, GDK_CURRENT_TIME, NULL))
        {
            webkit_web_policy_decision_ignore (decision);
            return TRUE;
        }
    }
    else if (g_str_has_prefix (uri, "data:image/"))
    {
        /* For security reasons, main content served as data: is limited to images
           http://lcamtuf.coredump.cx/switch/ */
        webkit_web_policy_decision_ignore (decision);
        return TRUE;
    }
    #ifdef HAVE_GCR
    else if (/* view->special && */ !strncmp (uri, "https", 5))
    {
        /* We show an error page if the certificate is invalid.
           If a "special", unverified page loads a form, it must be that page.
           if (webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED)
           FIXME: Verify more stricly that this cannot be eg. a simple Reload */
        if (webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_RELOAD)
        {
            SoupMessage* message = webkit_network_request_get_message (request);
            if (!(soup_message_get_flags (message) & SOUP_MESSAGE_CERTIFICATE_TRUSTED))
            {
                SoupURI* soup_uri = soup_message_get_uri (message);
                GTlsCertificate* tls_cert;
                GcrCertificate* gcr_cert;
                GByteArray* der_cert;

                message = midori_map_get_message (message);
                g_object_get (message, "tls-certificate", &tls_cert, NULL);
                g_return_val_if_fail (tls_cert != NULL, FALSE);
                g_object_get (tls_cert, "certificate", &der_cert, NULL);
                gcr_cert = gcr_simple_certificate_new (der_cert->data, der_cert->len);
                g_byte_array_unref (der_cert);
                if (soup_uri && soup_uri->host && !gcr_trust_is_certificate_pinned (gcr_cert, GCR_PURPOSE_SERVER_AUTH, soup_uri->host, NULL, NULL))
                {
                    GError* error = NULL;
                    gcr_trust_add_pinned_certificate (gcr_cert, GCR_PURPOSE_SERVER_AUTH, soup_uri->host, NULL, &error);
                    if (error != NULL)
                    {
                        gchar* slots = g_strjoinv (" , ", (gchar**)gcr_pkcs11_get_trust_lookup_uris ());
                        gchar* title = g_strdup_printf ("Error granting trust: %s", error->message);
                        midori_view_stop_loading (view);
                        midori_view_display_error (view, view->uri,
                            view->title ? view->title : view->uri, title, slots,
                            _("Trust this website"), NULL);
                        g_free (title);
                        g_free (slots);
                        g_error_free (error);
                    }
                }
                else
                    g_warn_if_reached ();
                g_object_unref (gcr_cert);
                g_object_unref (tls_cert);
            }
        }
    }
    #endif
    view->special = FALSE;

    /* Remove link labels */
    js_context = webkit_web_frame_get_global_context (web_frame);
    result = sokoke_js_script_eval (js_context,
        "(function (links) {"
        "if (links != undefined && links.length > 0) {"
        "   for (var i = links.length - 1; i >= 0; i--) {"
        "       var parent = links[i].parentNode;"
        "       parent.removeChild(links[i]); } } }) ("
        "document.getElementsByClassName ('midoriHKD87346'));",
        NULL);
    g_free (result);
    result = sokoke_js_script_eval (js_context,
        "(function (links) {"
        "if (links != undefined && links.length > 0) {"
        "   for (var i = links.length - 1; i >= 0; i--) {"
        "       var parent = links[i].parentNode;"
        "       parent.removeChild(links[i]); } } }) ("
        "document.getElementsByClassName ('midori_access_key_fc04de'));",
        NULL);
    g_free (result);
    view->find_links = -1;

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

#ifdef HAVE_GCR
const gchar*
midori_location_action_tls_flags_to_string (GTlsCertificateFlags flags);
#endif

static void
webkit_web_view_load_committed_cb (WebKitWebView*  web_view,
                                   WebKitWebFrame* web_frame,
                                   MidoriView*     view)
{
    const gchar* uri;
    GList* children;

    if (web_frame != webkit_web_view_get_main_frame (web_view))
        return;

    g_object_freeze_notify (G_OBJECT (view));

    uri = webkit_web_frame_get_uri (web_frame);
    g_return_if_fail (uri != NULL);
    katze_assign (view->icon_uri, NULL);

    children = gtk_container_get_children (GTK_CONTAINER (view));
    for (; children; children = g_list_next (children))
        if (g_object_get_data (G_OBJECT (children->data), "midori-infobar-cb"))
            gtk_widget_destroy (children->data);
    g_list_free (children);
    view->alerts = 0;

    if (g_strcmp0 (uri, katze_item_get_uri (view->item)))
    {
        katze_assign (view->uri, midori_uri_format_for_display (uri));
        katze_item_set_uri (view->item, uri);
    }

    katze_item_set_added (view->item, time (NULL));
    katze_item_set_meta_integer (view->item, "history-step", -1);

    g_object_notify (G_OBJECT (view), "uri");
    g_object_set (view, "title", NULL, NULL);

    midori_view_unset_icon (view);

    if (!strncmp (uri, "https", 5))
    {
        #if defined (HAVE_LIBSOUP_2_29_91)
        WebKitWebDataSource *source;
        WebKitNetworkRequest *request;
        SoupMessage *message;

        source = webkit_web_frame_get_data_source (web_frame);
        request = webkit_web_data_source_get_request (source);
        message = webkit_network_request_get_message (request);

        if (message
         && soup_message_get_flags (message) & SOUP_MESSAGE_CERTIFICATE_TRUSTED)
            view->security = MIDORI_SECURITY_TRUSTED;
        #ifdef HAVE_GCR
        else if (!view->special && message != NULL)
        {
            GTlsCertificate* tls_cert;
            GcrCertificate* gcr_cert;
            GByteArray* der_cert;
            SoupURI* soup_uri;

            message = midori_map_get_message (message);
            g_object_get (message, "tls-certificate", &tls_cert, NULL);
            g_return_if_fail (tls_cert != NULL);
            g_object_get (tls_cert, "certificate", &der_cert, NULL);
            gcr_cert = gcr_simple_certificate_new (der_cert->data, der_cert->len);
            g_byte_array_unref (der_cert);
            soup_uri = soup_message_get_uri (message);
            if (gcr_trust_is_certificate_pinned (gcr_cert, GCR_PURPOSE_SERVER_AUTH, soup_uri->host, NULL, NULL))
                view->security = MIDORI_SECURITY_TRUSTED;
            else
            {
                GTlsCertificateFlags tls_flags;
                view->security = MIDORI_SECURITY_UNKNOWN;
                g_object_get (message, "tls-errors", &tls_flags, NULL);
                midori_view_stop_loading (view);
                midori_view_display_error (
                    view, view->uri, view->title ? view->title : view->uri,
                    _("Security unknown"),
                    midori_location_action_tls_flags_to_string (tls_flags),
                    _("Trust this website"),
                    NULL);
            }
            g_object_unref (tls_cert);
            g_object_unref (gcr_cert);
        }
        #endif
        else
        #endif
            view->security = MIDORI_SECURITY_UNKNOWN;
    }
    else
        view->security = MIDORI_SECURITY_NONE;
    g_object_notify (G_OBJECT (view), "security");

    view->find_links = -1;
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
    if (!view->special && !midori_view_is_blank (view))
        return;

    if (g_str_has_prefix (uri, "res://"))
    {
        gchar* filepath = midori_paths_get_res_filename (&uri[6]);
        gchar* file_uri = g_filename_to_uri (filepath, NULL, NULL);
        g_free (filepath);
        webkit_network_request_set_uri (request, file_uri);
        g_free (file_uri);
    }
    else if (g_str_has_prefix (uri, "stock://"))
    {
        GdkPixbuf* pixbuf;
        const gchar* icon_name = &uri[8] ? &uri[8] : "";
        gint icon_size = GTK_ICON_SIZE_MENU;
        GdkScreen* screen = gtk_widget_get_screen (GTK_WIDGET (view));
        GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen (screen);
        gint real_icon_size;
        GtkIconInfo* icon_info;
        const gchar* icon_filename;
        static gint icon_size_large_dialog = 0;

        if (!icon_size_large_dialog)
            icon_size_large_dialog = gtk_icon_size_register ("large-dialog", 64, 64);

        if (g_ascii_isalpha (icon_name[0]))
            icon_size = strstr (icon_name, "dialog") ?
                icon_size_large_dialog : GTK_ICON_SIZE_BUTTON;
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

        /* If available, load SVG icon as SVG markup */
        gtk_icon_size_lookup_for_settings (
            gtk_widget_get_settings (GTK_WIDGET (view)),
                icon_size, &real_icon_size, &real_icon_size);
        icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name,
            real_icon_size, GTK_ICON_LOOKUP_FORCE_SVG);
        icon_filename = icon_info ? gtk_icon_info_get_filename (icon_info) : NULL;
        if (icon_filename && g_str_has_suffix (icon_filename, ".svg"))
        {
            gchar* buffer;
            gsize buffer_size;
            if (g_file_get_contents (icon_filename, &buffer, &buffer_size, NULL))
            {
                gchar* encoded = g_base64_encode ((guchar*)buffer, buffer_size);
                gchar* data_uri = g_strconcat ("data:image/svg+xml;base64,", encoded, NULL);
                g_free (buffer);
                g_free (encoded);
                webkit_network_request_set_uri (request, data_uri);
                g_free (data_uri);
                return;
            }
        }

        /* Render icon as a PNG at the desired size */
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

#define HAVE_GTK_INFO_BAR GTK_CHECK_VERSION (2, 18, 0)
#define HAVE_OFFSCREEN GTK_CHECK_VERSION (2, 20, 0)

#if HAVE_GTK_INFO_BAR
static void
midori_view_infobar_response_cb (GtkWidget* infobar,
                                 gint       response,
                                 gpointer   data_object)
{
    void (*response_cb) (GtkWidget*, gint, gpointer);
    response_cb = g_object_get_data (G_OBJECT (infobar), "midori-infobar-cb");
    if (response_cb != NULL)
        response_cb (infobar, response, data_object);
    gtk_widget_destroy (infobar);
}
#else
static void
midori_view_info_bar_button_cb (GtkWidget* button,
                                gpointer   data_object)
{
    GtkWidget* infobar = gtk_widget_get_parent (gtk_widget_get_parent (button));
    gint response = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "midori-infobar-response"));
    void (*response_cb) (GtkWidget*, gint, gpointer);
    response_cb = g_object_get_data (G_OBJECT (infobar), "midori-infobar-cb");
    if (response_cb != NULL)
        response_cb (infobar, response, data_object);
    gtk_widget_destroy (infobar);
}
#endif

/**
 * midori_view_add_info_bar
 * @view: a #MidoriView
 * @message_type: a #GtkMessageType
 * @message: a message string
 * @response_cb: a response callback
 * @user_data: user data passed to the callback
 * @first_button_text: button text or stock ID
 * @...: first response ID, then more text - response ID pairs
 *
 * Adds an infobar (or equivalent) to the view. Activation of a
 * button invokes the specified callback. The infobar is
 * automatically destroyed if the location changes or reloads.
 *
 * Return value: an infobar widget
 *
 * Since: 0.2.9
 **/
GtkWidget*
midori_view_add_info_bar (MidoriView*    view,
                          GtkMessageType message_type,
                          const gchar*   message,
                          GCallback      response_cb,
                          gpointer       data_object,
                          const gchar*   first_button_text,
                          ...)
{
    GtkWidget* infobar;
    GtkWidget* action_area;
    GtkWidget* content_area;
    GtkWidget* label;
    va_list args;
    const gchar* button_text;

    g_return_val_if_fail (message != NULL, NULL);

    va_start (args, first_button_text);

    #if HAVE_GTK_INFO_BAR
    infobar = gtk_info_bar_new ();
    for (button_text = first_button_text; button_text;
         button_text = va_arg (args, const gchar*))
    {
        gint response_id = va_arg (args, gint);
        gtk_info_bar_add_button (GTK_INFO_BAR (infobar),
                                 button_text, response_id);
    }
    gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), message_type);
    content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));
    action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (infobar));
    gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
                                    GTK_ORIENTATION_HORIZONTAL);
    g_signal_connect (infobar, "response",
        G_CALLBACK (midori_view_infobar_response_cb), data_object);
    #else
    infobar = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (infobar), 4);

    content_area = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (infobar), content_area, TRUE, TRUE, 0);
    action_area = gtk_hbutton_box_new ();
    for (button_text = first_button_text; button_text;
         button_text = va_arg (args, const gchar*))
    {
        gint response_id = va_arg (args, gint);
        GtkWidget* button = gtk_button_new_with_mnemonic (button_text);
        g_object_set_data (G_OBJECT (button), "midori-infobar-response",
                           GINT_TO_POINTER (response_id));
        g_signal_connect (button, "clicked",
            G_CALLBACK (midori_view_info_bar_button_cb), data_object);
        gtk_box_pack_start (GTK_BOX (action_area), button, FALSE, FALSE, 0);
        if (response_id == GTK_RESPONSE_HELP)
            gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area),
                                                button, TRUE);
    }
    gtk_box_pack_start (GTK_BOX (infobar), action_area, FALSE, FALSE, 0);
    #endif

    va_end (args);
    label = gtk_label_new (message);
    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_container_add (GTK_CONTAINER (content_area), label);
    gtk_widget_show_all (infobar);
    gtk_box_pack_start (GTK_BOX (view), infobar, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (view), infobar, 0);
    g_object_set_data (G_OBJECT (infobar), "midori-infobar-cb", response_cb);
    return infobar;
}

static void
midori_view_database_response_cb (GtkWidget*         infobar,
                                  gint               response,
                                  WebKitWebDatabase* database)
{
    if (response != GTK_RESPONSE_ACCEPT)
    {
        WebKitSecurityOrigin* origin = webkit_web_database_get_security_origin (database);
        webkit_security_origin_set_web_database_quota (origin, 0);
        webkit_web_database_remove (database);
    }
    /* TODO: Remember the decision */
}

static void
midori_view_web_view_database_quota_exceeded_cb (WebKitWebView*     web_view,
                                                 WebKitWebFrame*    web_frame,
                                                 WebKitWebDatabase* database,
                                                 MidoriView*        view)
{
    const gchar* uri = webkit_web_frame_get_uri (web_frame);
    MidoriSiteDataPolicy policy = midori_web_settings_get_site_data_policy (view->settings, uri);

    switch (policy)
    {
    case MIDORI_SITE_DATA_BLOCK:
    {
        WebKitSecurityOrigin* origin = webkit_web_database_get_security_origin (database);
        webkit_security_origin_set_web_database_quota (origin, 0);
        webkit_web_database_remove (database);
    }
    case MIDORI_SITE_DATA_ACCEPT:
    case MIDORI_SITE_DATA_PRESERVE:
        return;
    case MIDORI_SITE_DATA_UNDETERMINED:
    {
        gchar* hostname = midori_uri_parse_hostname (uri, NULL);
        gchar* message = g_strdup_printf (_("%s wants to save an HTML5 database."),
                                          hostname && *hostname ? hostname : uri);
        midori_view_add_info_bar (view, GTK_MESSAGE_QUESTION, message,
            G_CALLBACK (midori_view_database_response_cb), database,
            _("_Deny"), GTK_RESPONSE_REJECT, _("_Allow"), GTK_RESPONSE_ACCEPT,
            NULL);
        g_free (hostname);
        g_free (message);
    }
    }
}

#if WEBKIT_CHECK_VERSION (1, 1, 23)
static void
midori_view_location_response_cb (GtkWidget*                       infobar,
                                  gint                             response,
                                  WebKitGeolocationPolicyDecision* decision)
{
    if (response == GTK_RESPONSE_ACCEPT)
        webkit_geolocation_policy_allow (decision);
    else
        webkit_geolocation_policy_deny (decision);
    g_object_unref (decision);
}

static gboolean
midori_view_web_view_geolocation_decision_cb (WebKitWebView*                   web_view,
                                              WebKitWebFrame*                  web_frame,
                                              WebKitGeolocationPolicyDecision* decision,
                                              MidoriView*                      view)
{
    const gchar* uri = webkit_web_frame_get_uri (web_frame);
    gchar* hostname = midori_uri_parse_hostname (uri, NULL);
    gchar* message = g_strdup_printf (_("%s wants to know your location."),
                                     hostname && *hostname ? hostname : uri);
    /* FIXME: decision should be released even if neither buttons's pressed */
    midori_view_add_info_bar (view, GTK_MESSAGE_QUESTION,
        message, G_CALLBACK (midori_view_location_response_cb), g_object_ref (decision),
        _("_Deny"), GTK_RESPONSE_REJECT, _("_Allow"), GTK_RESPONSE_ACCEPT,
        NULL);
    g_free (hostname);
    g_free (message);
    return TRUE;
}
#endif

static void
midori_view_load_alternate_string (MidoriView*     view,
                                   const gchar*    data,
                                   const gchar*    uri,
                                   WebKitWebFrame* web_frame)
{
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    if (!web_frame)
        web_frame = webkit_web_view_get_main_frame (web_view);
    view->special = TRUE;
    webkit_web_frame_load_alternate_string (
        web_frame, data, uri, uri);
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
    gchar* path = midori_paths_get_res_filename ("error.html");
    gchar* template;

    if (g_file_get_contents (path, &template, NULL, NULL))
    {
        gchar* title_escaped;
        const gchar* icon;
        gchar* result;

        title_escaped = g_markup_escape_text (title, -1);
        icon = katze_item_get_icon (view->item);
        result = sokoke_replace_variables (template,
            "{dir}", gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL ?
                "rtl" : "ltr",
            "{title}", title_escaped,
            "{icon}", icon ? icon : "",
            "{message}", message,
            "{description}", description,
            "{tryagain}", try_again,
            "{uri}", uri,
            NULL);
        g_free (title_escaped);
        g_free (template);

        midori_view_load_alternate_string (view,
            result, uri, web_frame);

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
    gchar* title;
    gchar* message;
    gboolean result;

    /* The unholy trinity; also ignored in Webkit's default error handler */
    switch (error->code)
    {
    case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
        /* A plugin will take over. That's expected, it's not fatal. */
    case WEBKIT_NETWORK_ERROR_CANCELLED:
        /* Mostly initiated by JS redirects. */
    case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
        /* A frame load is cancelled because of a download. */
        return FALSE;
    }

    title = g_strdup_printf (_("Error - %s"), uri);
    message = g_strdup_printf (_("The page '%s' couldn't be loaded."), uri);
    result = midori_view_display_error (view, uri, title,
        message, error->message, _("Try again"), web_frame);
    g_free (message);
    g_free (title);
    return result;
}

static void
midori_view_apply_scroll_position (MidoriView* view)
{
    if (view->scrollh > -2)
    {
        if (view->scrollh > 0)
        {
            GtkScrolledWindow* scrolled = GTK_SCROLLED_WINDOW (view->scrolled_window);
            GtkAdjustment* adjustment = gtk_scrolled_window_get_hadjustment (scrolled);
            gtk_adjustment_set_value (adjustment, view->scrollh);
        }
        view->scrollh = -3;
    }
    if (view->scrollv > -2)
    {
        if (view->scrollv > 0)
        {
            GtkScrolledWindow* scrolled = GTK_SCROLLED_WINDOW (view->scrolled_window);
            GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment (scrolled);
            gtk_adjustment_set_value (adjustment, view->scrollv);
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

    midori_view_apply_scroll_position (view);

    if (web_frame == webkit_web_view_get_main_frame (web_view))
    {
        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
        /* Icon: URI, News Feed: $URI|title, Search: :URI|title */
        gchar* value = sokoke_js_script_eval (js_context,
        "(function (l) { var f = new Array (); for (var i in l) "
        "{ var t = l[i].type; var r = l[i].rel; "
        "if (t && (t.indexOf ('rss') != -1 || t.indexOf ('atom') != -1)) "
        "f.push ('$' + l[i].href + '|' + l[i].title);"
        "else if (r == 'search' && t == 'application/opensearchdescription+xml') "
        "f.push (':' + l[i].href + '|' + l[i].title); "
        #if !WEBKIT_CHECK_VERSION (1, 1, 18)
        "else if (r && r.indexOf ('icon') != -1) f.push (l[i].href); "
        #endif
        "} return f; })("
        "document.getElementsByTagName ('link'));", NULL);

        /* FIXME: If URI or title contains , parsing will break */
        gchar** items = g_strsplit (value, ",", 0);
        gchar** current_item = items;
        gchar* default_uri = NULL;

        if (view->news_feeds != NULL)
            katze_array_clear (view->news_feeds);
        else
            view->news_feeds = katze_array_new (KATZE_TYPE_ITEM);

        while (current_item && *current_item)
        {
            const gchar* uri_and_title = *current_item;
            if (uri_and_title[0] == '$')
            {
                const gchar* title;
                gchar* uri;
                KatzeItem* item;

                uri_and_title++;
                if (uri_and_title == NULL)
                    continue;
                title = strchr (uri_and_title, '|');
                if (title == NULL)
                    goto news_feeds_continue;
                title++;

                uri = g_strndup (uri_and_title, title - 1 - uri_and_title);
                item = g_object_new (KATZE_TYPE_ITEM,
                    "uri", uri, "name", title, NULL);
                katze_array_add_item (view->news_feeds, item);
                g_object_unref (item);
                if (!default_uri)
                    default_uri = uri;
                else
                    g_free (uri);
            }
            else if (uri_and_title[0] == ':')
            {
                const gchar* title;

                uri_and_title++;
                if (uri_and_title == NULL)
                    continue;
                title = strchr (uri_and_title, '|');
                if (title == NULL)
                    goto news_feeds_continue;
                title++;
                /* TODO: Parse search engine XML
                midori_view_add_info_bar (view, GTK_MESSAGE_INFO, title,
                    G_CALLBACK (midori_view_open_search_response_cb), view,
                    _("_Save Search engine"), GTK_RESPONSE_ACCEPT, NULL); */
            }
            #if !WEBKIT_CHECK_VERSION (1, 1, 18)
            else
                katze_assign (view->icon_uri, g_strdup (uri_and_title));
            #endif

            news_feeds_continue:
            current_item++;
        }
        g_strfreev (items);

        g_object_set_data_full (G_OBJECT (view), "news-feeds", default_uri, g_free);
        g_free (value);

        #if !WEBKIT_CHECK_VERSION (1, 4, 3)
        _midori_web_view_load_icon (view);
        #endif
    }

    view->progress = 1.0;
    g_object_notify (G_OBJECT (view), "progress");
    midori_view_update_load_status (view, MIDORI_LOAD_FINISHED);

    g_object_thaw_notify (G_OBJECT (view));
}

#if WEBKIT_CHECK_VERSION (1, 1, 18)
static void
midori_web_view_notify_icon_uri_cb (WebKitWebView* web_view,
                                    GParamSpec*    pspec,
                                    MidoriView*    view)
{
    const gchar* icon_uri = webkit_web_view_get_icon_uri (web_view);
    katze_assign (view->icon_uri, g_strdup (icon_uri));
    _midori_web_view_load_icon (view);
}
#endif

static void
webkit_web_view_notify_uri_cb (WebKitWebView* web_view,
                               GParamSpec*    pspec,
                               MidoriView*    view)
{
    katze_assign (view->uri, g_strdup (webkit_web_view_get_uri (web_view)));
    g_object_notify (G_OBJECT (view), "uri");
}

static void
webkit_web_view_notify_title_cb (WebKitWebView* web_view,
                                 GParamSpec*    pspec,
                                 MidoriView*    view)
{
    const gchar* title = webkit_web_view_get_title (web_view);
    midori_view_set_title (view, title);
    g_object_notify (G_OBJECT (view), "title");
}

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
    #if !(WEBKIT_CHECK_VERSION (1, 3, 1) && defined (HAVE_LIBSOUP_2_29_3))
    sokoke_prefetch_uri (view->settings, link_uri, NULL, NULL);
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

static gboolean
midori_view_always_same_tab (const gchar* uri)
{
    /* No opening in tab, window or app for Javascript or mailto links */
    return g_str_has_prefix (uri, "javascript:") || g_str_has_prefix (uri, "mailto:");
}

static void
midori_view_ensure_link_uri (MidoriView* view,
                             gint        *x,
                             gint        *y,
                             GdkEventButton* event)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    if (view->web_view && gtk_widget_get_window (view->web_view))
    {
        GdkEventButton ev;

        if (!event) {
            gint ex, ey;
            event = &ev;
            gdk_window_get_pointer (gtk_widget_get_window (view->web_view), &ex, &ey, NULL);
            event->x = ex;
            event->y = ey;
        }

        if (x != NULL)
            *x = event->x;
        if (y != NULL)
            *y = event->y;

        katze_object_assign (view->hit_test,
            g_object_ref (
            webkit_web_view_get_hit_test_result (
            WEBKIT_WEB_VIEW (view->web_view), event)));
        katze_assign (view->link_uri,
             katze_object_get_string (view->hit_test, "link-uri"));
    }
}

#define MIDORI_KEYS_MODIFIER_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK \
    | GDK_MOD1_MASK | GDK_META_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK )

static gboolean
midori_view_web_view_button_press_event_cb (WebKitWebView*  web_view,
                                            GdkEventButton* event,
                                            MidoriView*     view)
{
    GtkClipboard* clipboard;
    gchar* uri;
    gchar* new_uri;
    const gchar* link_uri;
    gboolean background;

    event->state = event->state & MIDORI_KEYS_MODIFIER_MASK;
    midori_view_ensure_link_uri (view, NULL, NULL, event);
    link_uri = midori_view_get_link_uri (view);
    view->button_press_handled = FALSE;

    switch (event->button)
    {
    case 1:
        if (!link_uri)
            return FALSE;

        if (midori_view_always_same_tab (link_uri))
            return FALSE;

        if (MIDORI_MOD_NEW_TAB (event->state))
        {
            /* Open link in new tab */
            background = view->open_tabs_in_the_background;
            if (MIDORI_MOD_BACKGROUND (event->state))
                background = !background;
            g_signal_emit (view, signals[NEW_TAB], 0, link_uri, background);
            view->button_press_handled = TRUE;
            return TRUE;
        }
        else if (MIDORI_MOD_NEW_WINDOW (event->state))
        {
            /* Open link in new window */
            g_signal_emit (view, signals[NEW_WINDOW], 0, link_uri);
            view->button_press_handled = TRUE;
            return TRUE;
        }
        break;
    case 2:
        if (link_uri)
        {
            if (midori_view_always_same_tab (link_uri))
                return FALSE;

            /* Open link in new tab */
            background = view->open_tabs_in_the_background;
            if (MIDORI_MOD_BACKGROUND (event->state))
                background = !background;
            g_signal_emit (view, signals[NEW_TAB], 0, link_uri, background);
            view->button_press_handled = TRUE;
            return TRUE;
        }
        if (view->middle_click_opens_selection)
        {
            gboolean is_editable;
            WebKitHitTestResult* result;
            WebKitHitTestResultContext context;

            result = webkit_web_view_get_hit_test_result (web_view, event);
            context = katze_object_get_int (result, "context");
            is_editable = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
            g_object_unref (result);
            if (!is_editable)
            {
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
                            gchar* search = katze_object_get_string (
                                view->settings, "location-entry-search");
                            new_uri = midori_uri_for_search (search, uri);
                            g_free (search);
                        }
                        katze_assign (uri, new_uri);
                    }
                    else if (midori_uri_is_location (uri))
                    {
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
                        view->button_press_handled = TRUE;
                        return TRUE;
                    }
                    else
                    {
                        g_free (uri);
                    }
                }
            }
        }
        if (MIDORI_MOD_SCROLL (event->state))
        {
            midori_view_set_zoom_level (MIDORI_VIEW (view), 1.0);
            return FALSE; /* Allow Ctrl + Middle click */
        }
        return FALSE;
        break;
    case 3:
        if (event->state & GDK_CONTROL_MASK)
        {
            /* Ctrl + Right-click suppresses javascript button handling */
            GtkWidget* menu = gtk_menu_new ();
            midori_view_populate_popup (view, menu, TRUE);
            katze_widget_popup (GTK_WIDGET (web_view), GTK_MENU (menu), event,
                                KATZE_MENU_POSITION_CURSOR);
            view->button_press_handled = TRUE;
            return TRUE;
        }
        break;
    case 8:
        midori_view_go_back (view);
        view->button_press_handled = TRUE;
        return TRUE;
    case 9:
        midori_view_go_forward (view);
        view->button_press_handled = TRUE;
        return TRUE;
    /*
     * On some fancier mice the scroll wheel can be used to scroll horizontally.
     * A middle click usually registers both a middle click (2) and a
     * horizontal scroll (11 or 12).
     * We catch horizontal scrolls and ignore them to prevent middle clicks from
     * accidentally being interpreted as first button clicks.
     */
    case 11:
    case 12:
        view->button_press_handled = TRUE;
        return TRUE;
    }

    /* We propagate the event, since it may otherwise be stuck in WebKit */
    g_signal_emit_by_name (view, "event", event, &background);

    return FALSE;
}

static gboolean
midori_view_web_view_button_release_event_cb (WebKitWebView*  web_view,
                                              GdkEventButton* event,
                                              MidoriView*     view)
{
    gboolean button_press_handled = view->button_press_handled;
    view->button_press_handled = FALSE;

    return button_press_handled;
}

static gboolean
midori_view_inspector_window_key_press_event_cb (GtkWidget*   window,
                                                 GdkEventKey* event,
                                                 gpointer     user_data)
{
    /* Close window on Ctrl+W */
    if (event->keyval == 'w' && (event->state & GDK_CONTROL_MASK))
        gtk_widget_destroy (window);

    return FALSE;
}

static void
handle_link_hints (WebKitWebView* web_view,
                   GdkEventKey*   event,
                   MidoriView*    view)
{
    gint digit = g_ascii_digit_value (event->keyval);
    gunichar uc = gdk_keyval_to_unicode (event->keyval);
    gchar* result = NULL;
    WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (web_view);
    JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);

    if (view->find_links < 0)
    {
        /* Links are currently off, turn them on */
        result = sokoke_js_script_eval (js_context,
            " var style_func = (function (selector, rule) { "
            " var style = document.createElement ('style');"
            " style.setAttribute ('type', 'text/css');"
            " var heads = document.getElementsByTagName ('head');"
            " heads[0].appendChild (style);"
            " document.styleSheets[0].insertRule (selector + ' ' + rule);"
            " } );"
            " style_func ('.midoriHKD87346', '{ "
            " font-size:small !important; font-weight:bold !important;"
            " z-index:500; border-radius:0.3em; line-height:1 !important;"
            " background: white !important; color: black !important;"
            " border:1px solid gray; padding:0 0.1em !important;"
            " position:absolute; display:inline !important; }');"
            " style_func ('.midori_access_key_fc04de', '{ "
            " font-size:small !important; font-weight:bold !important;"
            " z-index:500; border-radius:0.3em; line-height:1 !important;"
            " background: black !important; color: white !important;"
            " border:1px solid gray; padding:0 0.1em 0.2em 0.1em !important;"
            " position:absolute; display:inline !important; }');"
            " var label_count = 0;"
            " for (i in document.links) {"
            "   if (document.links[i].href && document.links[i].insertBefore) {"
            "       var child = document.createElement ('span');"
            "       if (document.links[i].accessKey && isNaN (document.links[i].accessKey)) {"
            "           child.setAttribute ('class', 'midori_access_key_fc04de');"
            "           child.appendChild (document.createTextNode (document.links[i].accessKey));"
            "       } else {"
            "         child.setAttribute ('class', 'midoriHKD87346');"
            "         child.appendChild (document.createTextNode (label_count));"
            "         label_count++;"
            "       }"
            "       document.links[i].insertBefore (child); } }",
            NULL);
        view->find_links = 0; /* Links are now on */
        g_free (result);
        return;
    }

    if (event->keyval == '.')
    {
        /* Pressed '.' with links on, so turn them off */
        result = sokoke_js_script_eval (js_context,
            "var links = document.getElementsByClassName ('midoriHKD87346');"
            "for (var i = links.length - 1; i >= 0; i--) {"
            "   var parent = links[i].parentNode;"
            "   parent.removeChild(links[i]); }",
            NULL);
        g_free (result);
        result = sokoke_js_script_eval (js_context,
            "var links = document.getElementsByClassName ('midori_access_key_fc04de');"
            "if (links != undefined && links.length > 0) {"
            "   for (var i = links.length - 1; i >= 0; i--) {"
            "       var parent = links[i].parentNode;"
            "       parent.removeChild(links[i]); } }",
            NULL);
        g_free (result);
        view->find_links = -1;
        return;
    }

    /* Links are already on at this point, so process the input character */

    if (digit != -1 && event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_Escape)
    {
        /* Got a digit, add it to the link count/ number */
        if (view->find_links > 0)
            view->find_links *= 10;
        view->find_links += digit;
        return;
    }

    if (event->keyval == GDK_KEY_Escape)
    {
        // Clear the link count/number
        view->find_links = 0;
        return;
    }

    if (g_unichar_isalpha (uc))
    {
        /* letter pressed if we have a corresponding accessKey and grab URI */
        gchar* script = NULL;
        gchar* utf8 = NULL;
        gulong sz = g_unichar_to_utf8 (uc, NULL);

        utf8 = g_malloc0 (sz);
        g_unichar_to_utf8 (uc, utf8);
        script = g_strdup_printf (
            "var l = 'undefined';"
            "for (i in document.links) {"
            "   if ( document.links[i].href &&"
            "        document.links[i].accessKey == \"%s\" )"
            "   {"
            "       l = document.links[i].href;"
            "       break;"
            "   }"
            "}"
            "if (l != 'undefined') { l; }"
            , utf8
        );
        g_free (utf8);
        result = sokoke_js_script_eval (js_context, script, NULL);
        g_free (script);
    }
    else if (event->keyval == GDK_KEY_Return)
    {
        /* Return pressed, grab URI if we have a link with the entered number */
        gchar* script = g_strdup_printf (
            "var links = document.getElementsByClassName ('midoriHKD87346');"
            "var i = %d; var return_key = %d;"
            "if (return_key) {"
            "    if (typeof links[i] != 'undefined')"
            "        links[i].parentNode.href; }",
            view->find_links, event->keyval == GDK_KEY_Return
            );
        result = sokoke_js_script_eval (js_context, script, NULL);
        g_free (script);
    }

    /* Check the URI we grabbed to see if it's valid, if so go there */
    if (midori_uri_is_location (result))
    {
        if (MIDORI_MOD_NEW_TAB (event->state))
        {
            gboolean background = view->open_tabs_in_the_background;
            if (MIDORI_MOD_BACKGROUND (event->state))
                background = !background;
            g_signal_emit (view, signals[NEW_TAB], 0, result, background);
        }
        else
            midori_view_set_uri (view, result);
        view->find_links = -1; /* Turn off link mode */
    }
    else /* Invalid URI, start over... */
        view->find_links = 0;

    if (result)
        g_free (result);
    return;
}

static gboolean
gtk_widget_key_press_event_cb (WebKitWebView* web_view,
                               GdkEventKey*   event,
                               MidoriView*    view)
{
    guint character;

    event->state = event->state & MIDORI_KEYS_MODIFIER_MASK;

    /* Handle oddities in Russian keyboard layouts */
    if (event->hardware_keycode == ';' || event->hardware_keycode == '=')
        event->keyval = ',';
    else if (event->hardware_keycode == '<')
        event->keyval = '.';

    /* Find links by number: . to show links, type number, Return to go */
    if ( event->keyval == '.' || view->find_links > -1 )
    {
        handle_link_hints (web_view, event, view);
        return FALSE;
    }

    /* Find inline */
    if (event->keyval == ',' || event->keyval == '/' || event->keyval == GDK_KEY_KP_Divide)
        character = '\0';
    else
        return FALSE;

    /* Skip control characters */
    if (character == (event->keyval | 0x01000000))
        return FALSE;

    if (!webkit_web_view_can_cut_clipboard (web_view)
        && !webkit_web_view_can_paste_clipboard (web_view))
    {
        gchar* text = character ? g_strdup_printf ("%c", character) : NULL;
        #if GTK_CHECK_VERSION(3, 2, 0)
        midori_findbar_search_text (MIDORI_FINDBAR (view->overlay_find),
            (GtkWidget*)view, TRUE, katze_str_non_null (text));
        #else
        g_signal_emit (view, signals[SEARCH_TEXT], 0, TRUE, text ? text : "");
        #endif
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
                midori_view_get_zoom_level (view) - 0.10f);
        else if(event->direction == GDK_SCROLL_UP)
            midori_view_set_zoom_level (view,
                midori_view_get_zoom_level (view) + 0.10f);
        return TRUE;
    }
    else
        return FALSE;
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
    sokoke_spawn_app (view->link_uri, FALSE);
}

static void
midori_web_view_menu_link_copy_activate_cb (GtkWidget*  widget,
                                            MidoriView* view)
{
    if (g_str_has_prefix (view->link_uri, "mailto:"))
        sokoke_widget_copy_clipboard (widget, view->link_uri + 7, NULL, NULL);
    else
        sokoke_widget_copy_clipboard (widget, view->link_uri, NULL, NULL);
}

static void
midori_view_download_uri (MidoriView*        view,
                          MidoriDownloadType type,
                          const gchar*       uri)
{
    WebKitNetworkRequest* request = webkit_network_request_new (uri);
    WebKitDownload* download = webkit_download_new (request);
    gboolean handled;
    g_object_unref (request);
    midori_download_set_type (download, type);
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
}

static void
midori_web_view_menu_save_activate_cb (GtkWidget*  widget,
                                       MidoriView* view)
{
    midori_view_download_uri (view, MIDORI_DOWNLOAD_SAVE_AS, view->link_uri);
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

static GString*
midori_view_get_data_for_uri (MidoriView*  view,
                              const gchar* uri)
{
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    WebKitWebFrame* frame = webkit_web_view_get_main_frame (web_view);
    WebKitWebDataSource* data_source = webkit_web_frame_get_data_source (frame);
    GList* resources = webkit_web_data_source_get_subresources (data_source);
    GList* list;
    GString* result;

    for (list = resources; list; list = g_list_next (list))
    {
        WebKitWebResource* resource = WEBKIT_WEB_RESOURCE (list->data);
        GString* data = webkit_web_resource_get_data (resource);
        if (!g_strcmp0 (webkit_web_resource_get_uri (resource), uri))
        {
            result = data;
            break;
        }
    }
    g_list_free (resources);
    return result;
}

static void
midori_view_clipboard_get_image_cb (GtkClipboard*     clipboard,
                                    GtkSelectionData* selection_data,
                                    guint             info,
                                    gpointer          user_data)
{
    MidoriView* view = MIDORI_VIEW (g_object_get_data (user_data, "view"));
    WebKitHitTestResult* hit_test = user_data;
    gchar* uri = katze_object_get_string (hit_test, "image-uri");
    /* if (gtk_selection_data_targets_include_image (selection_data, TRUE)) */
    {
        GString* data = midori_view_get_data_for_uri (view, uri);
        if (data != NULL)
        {
            GInputStream* stream = g_memory_input_stream_new_from_data (data->str, data->len, NULL);
            GError* error = NULL;
            GdkPixbuf* pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
            g_object_unref (stream);
            if (error != NULL)
            {
                g_critical ("Error copying pixbuf: %s\n", error->message);
                g_error_free (error);
            }
            gtk_selection_data_set_pixbuf (selection_data, pixbuf);
            g_object_unref (pixbuf);
        }
        else
            g_warn_if_reached ();
    }
    /* if (gtk_selection_data_targets_include_text (selection_data)) */
        gtk_selection_data_set_text (selection_data, uri, -1);
    g_free (uri);
}

static void
midori_web_view_menu_image_copy_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    g_object_set_data (G_OBJECT (view->hit_test), "view", view);
    sokoke_widget_copy_clipboard (widget,
        uri, midori_view_clipboard_get_image_cb, view->hit_test);
    g_free (uri);
}

static void
midori_web_view_menu_image_save_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    midori_view_download_uri (view, MIDORI_DOWNLOAD_SAVE_AS, uri);
    g_free (uri);
}

static void
midori_web_view_open_in_viewer_cb (GtkWidget* widget,
                                   MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    midori_view_download_uri (view, MIDORI_DOWNLOAD_OPEN_IN_VIEWER, uri);
    g_free (uri);
}

static void
midori_web_view_menu_video_copy_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    sokoke_widget_copy_clipboard (widget, uri, NULL, NULL);
    g_free (uri);
}

static void
midori_web_view_menu_video_save_activate_cb (GtkWidget*  widget,
                                             MidoriView* view)
{
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    midori_view_download_uri (view, MIDORI_DOWNLOAD_SAVE_AS, uri);
    g_free (uri);
}

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

static void
midori_web_view_menu_background_tab_activate_cb (GtkWidget*  widget,
                                                 MidoriView* view)
{
    g_signal_emit (view, signals[NEW_TAB], 0, view->link_uri,
                   !view->open_tabs_in_the_background);
}

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
    uri = midori_uri_for_search (search, view->selected_text);
    g_free (search);

    g_signal_emit (view, signals[NEW_TAB], 0, uri,
        view->open_tabs_in_the_background);

    g_free (uri);
}

static void
midori_web_view_menu_copy_activate_cb (GtkWidget*  widget,
                                       MidoriView* view)
{
    sokoke_widget_copy_clipboard (widget, view->selected_text, NULL, NULL);
}

static void
midori_view_tab_label_menu_window_new_cb (GtkWidget* menuitem,
                                          GtkWidget* view)
{
    g_signal_emit (view, signals[NEW_WINDOW], 0,
        midori_view_get_display_uri (MIDORI_VIEW (view)));
}

static void
midori_web_view_open_frame_in_new_tab_cb (GtkWidget*  widget,
                                          MidoriView* view)
{
    WebKitWebFrame* web_frame = webkit_web_view_get_focused_frame (WEBKIT_WEB_VIEW (view->web_view));
    g_signal_emit (view, signals[NEW_TAB], 0,
        webkit_web_frame_get_uri (web_frame), view->open_tabs_in_the_background);
}

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
    webkit_web_inspector_show (inspector);
}

#if WEBKIT_CHECK_VERSION (1, 5, 0)
static void
midori_view_menu_add_search_engine_cb (GtkWidget*  widget,
                                       MidoriView* view)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view->web_view);
    GtkActionGroup* actions = midori_browser_get_action_group (browser);
    GtkAction* action = gtk_action_group_get_action (actions, "Search");
    KatzeItem* item = g_object_get_data (G_OBJECT (widget), "item");
    midori_search_action_get_editor (MIDORI_SEARCH_ACTION (action), item, TRUE);
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
    GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (browser)));
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

    gint x, y;
    WebKitHitTestResultContext context;
    gboolean is_image;
    gboolean is_media;

    midori_view_ensure_link_uri (view, &x, &y, NULL);
    context = katze_object_get_int (view->hit_test, "context");
    has_selection = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION;
    /* Ensure view->selected_text */
    if (!midori_view_has_selection (view))
        has_selection = false;
    is_editable = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
    is_image = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
    is_media = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA;
    is_document = !view->link_uri && !has_selection && !is_image && !is_media;

    if (is_editable)
    {
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

        #if WEBKIT_CHECK_VERSION (1, 5, 0)
        {
            KatzeItem* item = midori_search_action_get_engine_for_form (
                WEBKIT_WEB_VIEW (view->web_view), view->ellipsize);
            if (item != NULL)
            {
                menuitem = midori_view_insert_menu_item (menu_shell, -1,
                    _("Add _search engine..."), NULL,
                    G_CALLBACK (midori_view_menu_add_search_engine_cb), widget);
                g_object_set_data (G_OBJECT (menuitem), "item", item);
                gtk_menu_shell_append (menu_shell, menuitem);
                gtk_widget_show (menuitem);
            }
        }
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
        if (!midori_view_always_same_tab (view->link_uri))
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
        }

        midori_view_insert_menu_item (menu_shell, -1,
            _("Copy Link de_stination"), NULL,
            G_CALLBACK (midori_web_view_menu_link_copy_activate_cb), widget);

        if (!midori_view_always_same_tab (view->link_uri))
        {
        midori_view_insert_menu_item (menu_shell, -1,
            NULL, GTK_STOCK_SAVE_AS,
            G_CALLBACK (midori_web_view_menu_save_activate_cb), widget);
        }
    }

    if (is_image)
    {
        if (view->link_uri)
            gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        midori_view_insert_menu_item (menu_shell, -1,
            _("Open _Image in New Tab"), STOCK_TAB_NEW,
            G_CALLBACK (midori_web_view_menu_image_new_tab_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Copy Im_age"), NULL,
            G_CALLBACK (midori_web_view_menu_image_copy_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Save I_mage"), GTK_STOCK_SAVE,
            G_CALLBACK (midori_web_view_menu_image_save_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            _("Open in Image _Viewer"), GTK_STOCK_OPEN,
            G_CALLBACK (midori_web_view_open_in_viewer_cb), widget);
    }

    if (is_media)
    {
        midori_view_insert_menu_item (menu_shell, -1,
            _("Copy Video _Address"), NULL,
            G_CALLBACK (midori_web_view_menu_video_copy_activate_cb), widget);
        midori_view_insert_menu_item (menu_shell, -1,
            FALSE ? _("Save _Video") : _("Download _Video"), GTK_STOCK_SAVE,
            G_CALLBACK (midori_web_view_menu_video_save_activate_cb), widget);
    }

    if (has_selection)
    {
        gtk_menu_shell_append (menu_shell, gtk_separator_menu_item_new ());
        midori_view_insert_menu_item (menu_shell, -1, NULL, GTK_STOCK_COPY,
            G_CALLBACK (midori_web_view_menu_copy_activate_cb), widget);
    }

    if (!view->link_uri && has_selection)
    {
        GtkWidget* window;
        KatzeArray* search_engines = NULL;

        window = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
        i = 0;
        if (katze_object_has_property (window, "search-engines"))
            search_engines = katze_object_get_object (window, "search-engines");

        if (search_engines != NULL)
        {
            KatzeItem* item;
            GtkWidget* sub_menu = gtk_menu_new ();

            menuitem = gtk_image_menu_item_new_with_mnemonic (_("Search _with"));
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), sub_menu);
            gtk_menu_shell_insert (menu_shell, menuitem, 1);

            KATZE_ARRAY_FOREACH_ITEM (item, search_engines)
            {
                GdkPixbuf* pixbuf;
                const gchar* icon_name;

                menuitem = gtk_image_menu_item_new_with_mnemonic (katze_item_get_name (item));
                pixbuf = midori_search_action_get_icon (item,
                    GTK_WIDGET (web_view), &icon_name, FALSE);
                if (pixbuf)
                {
                    icon = gtk_image_new_from_pixbuf (pixbuf);
                    g_object_unref (pixbuf);
                }
                else
                    icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
                gtk_image_menu_item_set_always_show_image (
                    GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
                gtk_menu_shell_insert (GTK_MENU_SHELL (sub_menu), menuitem, i);
                g_object_set_data (G_OBJECT (menuitem), "search",
                                   (gchar*)katze_item_get_uri (item));
                g_signal_connect (menuitem, "activate",
                    G_CALLBACK (midori_web_view_menu_search_web_activate_cb), view);
                i++;
            }
            g_object_unref (search_engines);
        }
        midori_view_insert_menu_item (menu_shell, 0,
            _("_Search the Web"), GTK_STOCK_FIND,
            G_CALLBACK (midori_web_view_menu_search_web_activate_cb), widget);
        /* FIXME: choose 3 most frequently */

        g_strstrip (view->selected_text);
        if (midori_uri_is_valid (view->selected_text))
        {
            /* :// and @ together would mean login credentials */
            if (g_str_has_prefix (view->selected_text, "mailto:")
             || (strchr (view->selected_text, '@') != NULL
              && strstr (view->selected_text, "://") == NULL))
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

        if (webkit_web_view_get_focused_frame (web_view) != webkit_web_view_get_main_frame (web_view))
            menuitem = midori_view_insert_menu_item (menu_shell, -1,
                _("Open _Frame in New Tab"), NULL,
                G_CALLBACK (midori_web_view_open_frame_in_new_tab_cb), widget);

        menuitem = gtk_image_menu_item_new_from_stock (STOCK_WINDOW_NEW, NULL);
        gtk_menu_item_set_label (GTK_MENU_ITEM (menuitem), _("Open in New _Window"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_signal_connect (menuitem, "activate",
            G_CALLBACK (midori_view_tab_label_menu_window_new_cb), view);

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
              { "EncodingChineseSimplified" },
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

        if (!midori_view_is_blank (view) && !midori_paths_is_readonly ())
        {
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "AddSpeedDial"));
            gtk_menu_shell_append (menu_shell, menuitem);
        }
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "AddDesktopShortcut"));
        gtk_menu_shell_append (menu_shell, menuitem);
        #endif

        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "SaveAs"));
        gtk_menu_shell_append (menu_shell, menuitem);
        menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "SourceView"));
        gtk_menu_shell_append (menu_shell, menuitem);

        if (!g_object_get_data (G_OBJECT (browser), "midori-toolbars-visible"))
        {
            menuitem = sokoke_action_create_popup_menu_item (
                gtk_action_group_get_action (actions, "Navigationbar"));
            gtk_menu_shell_append (menu_shell, menuitem);
        }
    }

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

    if (state & GDK_WINDOW_STATE_FULLSCREEN)
    {
        menuitem = sokoke_action_create_popup_menu_item (
            gtk_action_group_get_action (actions, "Fullscreen"));

        gtk_image_menu_item_set_use_stock (GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
        gtk_menu_item_set_label (GTK_MENU_ITEM (menuitem), GTK_STOCK_LEAVE_FULLSCREEN);
        gtk_menu_shell_append (menu_shell, menuitem);
    }

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
    event.any.window = gtk_widget_get_window (web_view);
    gdk_window_get_pointer (event.any.window, &x, &y, NULL);
    event.any.type = GDK_MOTION_NOTIFY;
    event.motion.x = x;
    event.motion.y = y;
    g_signal_emit_by_name (web_view, "motion-notify-event", &event, &result);

    event.any.type = GDK_BUTTON_PRESS;
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
    MidoriNewView where = MIDORI_NEW_VIEW_TAB;
    GtkWidget* new_view = GTK_WIDGET (midori_view_get_for_widget (web_view));

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
    g_signal_emit (view, signals[NEW_VIEW], 0, new_view, where, FALSE);

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
        new_view = (MidoriView*)midori_view_new_with_item (NULL, view->settings);
        g_signal_connect (new_view->web_view, "web-view-ready",
                          G_CALLBACK (webkit_web_view_web_view_ready_cb), view);
    }
    g_object_set_data_full (G_OBJECT (new_view), "opener-uri",
        g_strdup (webkit_web_frame_get_uri (web_frame)), g_free);
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
    if (webkit_web_view_can_show_mime_type (WEBKIT_WEB_VIEW (web_view), mime_type))
    {
        gboolean view_source = FALSE;
        /* Dedicated source code views are always pseudo-blank pages */
        if (midori_view_is_blank (view))
            view_source = webkit_web_view_get_view_source_mode (WEBKIT_WEB_VIEW (web_view));

        webkit_web_view_set_view_source_mode (WEBKIT_WEB_VIEW (web_view), view_source);

        if (web_frame == webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view)))
        {
            katze_assign (view->mime_type, g_strdup (mime_type));
            katze_item_set_meta_string (view->item, "mime-type", mime_type);
            midori_view_unset_icon (view);
            g_object_notify (G_OBJECT (view), "mime-type");
        }

        return FALSE;
    }
    g_object_set_data(G_OBJECT (view), "download-mime-type", (gpointer)mime_type);
    webkit_web_policy_decision_download (decision);
    g_object_set_data(G_OBJECT (view), "download-mime-type", NULL);
    return TRUE;
}

static gboolean
webkit_web_view_download_requested_cb (GtkWidget*      web_view,
                                       WebKitDownload* download,
                                       MidoriView*     view)
{
    gchar* opener_uri;
    gchar* hostname;
    GtkWidget* dialog;
    gchar* content_type;
    gchar* description;
    WebKitWebFrame* web_frame;
    WebKitWebDataSource* datasource;
    GString* details;
    GIcon* icon;
    GtkWidget* image;
    gchar* title;
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    gint response;
    gboolean handled;

    /* Opener may differ from displaying view:
       http://lcamtuf.coredump.cx/fldl/ http://lcamtuf.coredump.cx/switch/ */
    opener_uri = g_object_get_data (G_OBJECT (view), "opener-uri");
    hostname = midori_uri_parse_hostname (
        opener_uri ? opener_uri : midori_view_get_display_uri (view), NULL);
    dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        _("Open or download file from %s"), hostname);
    g_free (hostname);
    content_type = midori_download_get_content_type (download,
        g_object_get_data (G_OBJECT (view), "download-mime-type"));
    description = g_content_type_get_description (content_type);
    icon = g_content_type_get_icon (content_type);
    g_themed_icon_append_name (G_THEMED_ICON (icon), "text-html");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
    g_object_unref (icon);
    gtk_widget_show (image);
    gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);

    details = g_string_sized_new (20 * 4);
    g_string_append_printf (details, _("File Name: %s"),
        webkit_download_get_suggested_filename (download));
    g_string_append_c (details, '\n');

    if (g_strrstr (description, content_type))
        g_string_append_printf (details, _("File Type: '%s'"), content_type);
    else
        g_string_append_printf (details, _("File Type: %s ('%s')"), description, content_type);
    g_string_append_c (details, '\n');
    g_free (description);
    g_free (content_type);

    /* Link Fingerprint */
    /* We look at the original URI because redirection would lose the fragment */
    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));
    datasource = webkit_web_frame_get_provisional_data_source (web_frame);
    if (datasource)
    {
        gchar* fingerprint;
        gchar* fplabel;
        WebKitNetworkRequest* original_request = webkit_web_data_source_get_initial_request (datasource);
        const gchar* original_uri = webkit_network_request_get_uri (original_request);
        midori_uri_get_fingerprint (original_uri, &fingerprint, &fplabel);
        if (fplabel && fingerprint)
        {
            WebKitNetworkRequest* request = webkit_download_get_network_request (download);

            g_string_append (details, fplabel);
            g_string_append_c (details, ' ');
            g_string_append (details, fingerprint);
            g_string_append_c (details, '\n');

            /* Propagate original URI to make it available when the download finishes */
            g_object_set_data_full (G_OBJECT (request), "midori-original-uri",
                                    g_strdup (original_uri), g_free);
        }
        g_free (fplabel);
        g_free (fingerprint);

    }

    if (webkit_download_get_total_size (download) > webkit_download_get_current_size (download))
    {
        gchar* total = g_format_size (webkit_download_get_total_size (download));
        g_string_append_printf (details, _("Size: %s"), total);
        g_string_append_c (details, '\n');
        g_free (total);
    }

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
        "%s", details->str);
    g_string_free (details, TRUE);

    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    /* i18n: A file open dialog title, ie. "Open http://fila.com/manual.tgz" */
    title = g_strdup_printf (_("Open %s"), webkit_download_get_uri (download));
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
        GTK_STOCK_SAVE, MIDORI_DOWNLOAD_SAVE,
        GTK_STOCK_SAVE_AS, MIDORI_DOWNLOAD_SAVE_AS,
        GTK_STOCK_CANCEL, MIDORI_DOWNLOAD_CANCEL,
        GTK_STOCK_OPEN, MIDORI_DOWNLOAD_OPEN,
        NULL);

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (response == GTK_RESPONSE_DELETE_EVENT)
        response = MIDORI_DOWNLOAD_CANCEL;
    midori_download_set_type (download, response);

    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    return handled;
}

static gboolean
webkit_web_view_console_message_cb (GtkWidget*   web_view,
                                    const gchar* message,
                                    guint        line,
                                    const gchar* source_id,
                                    MidoriView*  view)
{
    if (g_object_get_data (G_OBJECT (webkit_get_default_session ()),
                           "pass-through-console"))
        return FALSE;

    if (!strncmp (message, "speed_dial-save", 13))
    {
        MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (view));
        MidoriSpeedDial* dial = katze_object_get_object (browser, "speed-dial");
        GError* error = NULL;
        midori_speed_dial_save_message (dial, message, &error);
        if (error != NULL)
        {
            g_critical ("Failed speed dial message: %s\n", error->message);
            g_error_free (error);
        }
    }
    else
        g_signal_emit (view, signals[CONSOLE_MESSAGE], 0, message, line, source_id);
    return TRUE;
}

static void
midori_view_script_response_cb (GtkWidget*  infobar,
                                gint        response,
                                MidoriView* view)
{
    view->alerts--;
}

static gboolean
midori_view_web_view_script_alert_cb (GtkWidget*      web_view,
                                      WebKitWebFrame* web_frame,
                                      const gchar*    message,
                                      MidoriView*     view)
{
    gchar* text;

    /* Allow a maximum of 5 alerts */
    if (view->alerts > 4)
        return TRUE;

    view->alerts++;
    /* i18n: The text of an infobar for JavaScript alert messages */
    text = g_strdup_printf ("JavaScript: %s", message);
    midori_view_add_info_bar (view, GTK_MESSAGE_WARNING, text,
        G_CALLBACK (midori_view_script_response_cb), view,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    g_free (text);
    return TRUE;
}

static gboolean
midori_view_web_view_print_requested_cb (GtkWidget*      web_view,
                                         WebKitWebFrame* web_frame,
                                         MidoriView*     view)
{
    midori_view_print (view);
    return TRUE;
}

static void
webkit_web_view_window_object_cleared_cb (GtkWidget*      web_view,
                                          WebKitWebFrame* web_frame,
                                          JSContextRef    js_context,
                                          JSObjectRef     js_window,
                                          MidoriView*     view)
{
    const gchar* page_uri;

    page_uri = webkit_web_frame_get_uri (web_frame);
    if (!midori_uri_is_http (page_uri))
        return;

    if (katze_object_get_boolean (view->settings, "enable-private-browsing"))
    {
        /* Mask language, architecture, no plugin list */
        gchar* result = sokoke_js_script_eval (js_context,
            "navigator = { 'appName': 'Netscape',"
                          "'appCodeName': 'Mozilla',"
                          "'appVersion': '5.0 (X11)',"
                          "'userAgent': navigator.userAgent,"
                          "'language': 'en-US',"
                          "'platform': 'Linux i686',"
                          "'cookieEnabled': true,"
                          "'javaEnabled': function () { return true; },"
                          "'mimeTypes': {},"
                          "'plugins': {'refresh': function () { } } };",
            NULL);
        g_free (result);
    }
}

static void
midori_view_hadjustment_notify_value_cb (GtkAdjustment* hadjustment,
                                         GParamSpec*    pspec,
                                         MidoriView*    view)
{
    gint value = (gint)gtk_adjustment_get_value (hadjustment);
    katze_item_set_meta_integer (view->item, "scrollh", value);
}

static void
midori_view_notify_hadjustment_cb (MidoriView* view,
                                   GParamSpec* pspec,
                                   gpointer    data)
{
    GtkScrolledWindow* scrolled = GTK_SCROLLED_WINDOW (view->scrolled_window);
    GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment (scrolled);
    g_signal_connect (hadjustment, "notify::value",
        G_CALLBACK (midori_view_hadjustment_notify_value_cb), view);
}

static void
midori_view_vadjustment_notify_value_cb (GtkAdjustment* vadjustment,
                                         GParamSpec*    pspec,
                                         MidoriView*    view)
{
    gint value = (gint)gtk_adjustment_get_value (vadjustment);
    katze_item_set_meta_integer (view->item, "scrollv", value);
}

static void
midori_view_notify_vadjustment_cb (MidoriView* view,
                                   GParamSpec* pspec,
                                   gpointer    data)
{
    GtkScrolledWindow* scrolled = GTK_SCROLLED_WINDOW (view->scrolled_window);
    GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment (scrolled);
    g_signal_connect (vadjustment, "notify::value",
        G_CALLBACK (midori_view_vadjustment_notify_value_cb), view);
}

static void
midori_view_init (MidoriView* view)
{
    view->uri = NULL;
    view->title = NULL;
    view->security = MIDORI_SECURITY_NONE;
    view->mime_type = NULL;
    view->icon = NULL;
    view->icon_uri = NULL;
    view->progress = 0.0;
    view->load_status = MIDORI_LOAD_FINISHED;
    view->minimized = FALSE;
    view->statusbar_text = NULL;
    view->hit_test = NULL;
    view->link_uri = NULL;
    view->selected_text = NULL;
    view->news_feeds = NULL;
    view->find_links = -1;
    view->alerts = 0;

    view->item = katze_item_new ();

    view->scrollh = view->scrollv = -2;
    view->back_forward_set = FALSE;

    #if GTK_CHECK_VERSION (3, 2, 0)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (view), GTK_ORIENTATION_VERTICAL);
    #endif

    view->web_view = NULL;
    /* Adjustments are not created initially, but overwritten later */
    view->scrolled_window = katze_scrolled_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view->scrolled_window),
                                         GTK_SHADOW_NONE);

    #if GTK_CHECK_VERSION(3, 2, 0)
    view->overlay = gtk_overlay_new ();
    gtk_widget_show (view->overlay);
    gtk_container_add (GTK_CONTAINER (view->overlay), view->scrolled_window);
    gtk_box_pack_start (GTK_BOX (view), view->overlay, TRUE, TRUE, 0);

    /* Overlays must be created before showing GtkOverlay as of GTK+ 3.2 */
    {
    GtkWidget* frame = gtk_frame_new (NULL);
    view->overlay_label = gtk_label_new (NULL);
    gtk_widget_show (view->overlay_label);
    gtk_container_add (GTK_CONTAINER (frame), view->overlay_label);
    gtk_widget_set_halign (frame, GTK_ALIGN_START);
    gtk_widget_set_valign (frame, GTK_ALIGN_END);
    gtk_overlay_add_overlay (GTK_OVERLAY (view->overlay), frame);
    }
    view->overlay_find = g_object_new (MIDORI_TYPE_FINDBAR, NULL);
    gtk_widget_set_halign (view->overlay_find, GTK_ALIGN_END);
    gtk_widget_set_valign (view->overlay_find, GTK_ALIGN_START);
    gtk_overlay_add_overlay (GTK_OVERLAY (view->overlay),
                             view->overlay_find);
    gtk_widget_set_no_show_all (view->overlay_find, TRUE);
    #else
    gtk_box_pack_start (GTK_BOX (view), view->scrolled_window, TRUE, TRUE, 0);
    #endif

    g_signal_connect (view->item, "meta-data-changed",
        G_CALLBACK (midori_view_item_meta_data_changed), view);
    g_signal_connect (view->scrolled_window, "notify::hadjustment",
        G_CALLBACK (midori_view_notify_hadjustment_cb), view);
    g_signal_connect (view->scrolled_window, "notify::vadjustment",
        G_CALLBACK (midori_view_notify_vadjustment_cb), view);

    midori_view_construct_web_view (view);
}

static void
midori_view_finalize (GObject* object)
{
    MidoriView* view;

    view = MIDORI_VIEW (object);

    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);
    g_signal_handlers_disconnect_by_func (view->item,
        midori_view_item_meta_data_changed, view);

    katze_assign (view->uri, NULL);
    katze_assign (view->title, NULL);
    katze_object_assign (view->icon, NULL);
    katze_assign (view->icon_uri, NULL);

    katze_assign (view->statusbar_text, NULL);
    katze_assign (view->link_uri, NULL);
    katze_assign (view->selected_text, NULL);
    katze_object_assign (view->news_feeds, NULL);

    katze_object_assign (view->settings, NULL);
    katze_object_assign (view->item, NULL);

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
        midori_view_set_title (view, g_value_get_string (value));
        break;
    case PROP_MINIMIZED:
        view->minimized = g_value_get_boolean (value);
        g_signal_handlers_block_by_func (view->item,
            midori_view_item_meta_data_changed, view);
        katze_item_set_meta_integer (view->item, "minimized",
                                     view->minimized ? 1 : -1);
        g_signal_handlers_unblock_by_func (view->item,
            midori_view_item_meta_data_changed, view);
        #ifdef HAVE_GRANITE
        if (view->tab)
            g_object_set (view->tab, "label",
                "fixed", view->minimized, NULL);
            //view->minimized ? NULL : midori_view_get_display_title(view), NULL);
        #else
        if (view->tab_label)
            sokoke_widget_set_visible (view->tab_title, !view->minimized);
        #endif
        break;
    case PROP_ZOOM_LEVEL:
        midori_view_set_zoom_level (view, g_value_get_float (value));
        break;
    case PROP_STATUSBAR_TEXT:
        katze_assign (view->statusbar_text, g_value_dup_string (value));
        break;
    case PROP_SETTINGS:
        _midori_view_set_settings (view, g_value_get_object (value));
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
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean
midori_view_focus_in_event (GtkWidget*     widget,
                            GdkEventFocus* event)
{
    /* Always propagate focus to the child web view */
    gtk_widget_grab_focus (midori_view_get_web_view (MIDORI_VIEW (widget)));
    return TRUE;
}

/**
 * midori_view_new:
 * @net: %NULL
 *
 * Creates a new view.
 *
 * Return value: a new #MidoriView
 *
 * Deprecated: 0.2.8: Use midori_view_new_with_item() instead.
 **/
GtkWidget*
midori_view_new (KatzeNet* net)
{
    return g_object_new (MIDORI_TYPE_VIEW, NULL);
}

static void
_midori_view_set_settings (MidoriView*        view,
                           MidoriWebSettings* settings)
{
    gboolean zoom_text_and_images, kinetic_scrolling;

    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);

    katze_object_assign (view->settings, settings);
    if (!settings)
        return;

    g_object_ref (settings);
    g_signal_connect (settings, "notify",
                      G_CALLBACK (midori_view_settings_notify_cb), view);

    g_object_set (view->web_view, "settings", settings, NULL);

    g_object_get (view->settings,
        "zoom-text-and-images", &zoom_text_and_images,
        "kinetic-scrolling", &kinetic_scrolling,
        "close-buttons-on-tabs", &view->close_buttons_on_tabs,
        "open-new-pages-in", &view->open_new_pages_in,
        "middle-click-opens-selection", &view->middle_click_opens_selection,
        "open-tabs-in-the-background", &view->open_tabs_in_the_background,
        NULL);

    if (view->web_view)
        g_object_set (view->web_view,
                      "full-content-zoom", zoom_text_and_images, NULL);
    g_object_set (view->scrolled_window, "kinetic-scrolling", kinetic_scrolling, NULL);
}

/**
 * midori_view_new_with_title:
 * @title: a title, or %NULL
 * @settings: a #MidoriWebSettings, or %NULL
 * @append: if %TRUE, the view should be appended
 *
 * Creates a new view with the specified parameters that
 * is visible by default.
 *
 * Return value: a new #MidoriView
 *
 * Since: 0.3.0
 * Deprecated: 0.4.3
 **/
GtkWidget*
midori_view_new_with_title (const gchar*       title,
                            MidoriWebSettings* settings,
                            gboolean           append)
{
    KatzeItem* item = katze_item_new ();
    item->name = g_strdup (title);
    if (append)
        katze_item_set_meta_integer (item, "append", 1);
    return midori_view_new_with_item (item, settings);
}

/**
 * midori_view_new_with_item:
 * @item: a #KatzeItem, or %NULL
 * @settings: a #MidoriWebSettings, or %NULL
 *
 * Creates a new view from an item that is visible by default.
 *
 * Return value: a new #MidoriView
 *
 * Since: 0.4.3
 **/
GtkWidget*
midori_view_new_with_item (KatzeItem*         item,
                           MidoriWebSettings* settings)
{
    MidoriView* view = g_object_new (MIDORI_TYPE_VIEW,
                                     "title", item ? item->name : NULL,
                                     NULL);
    if (settings)
        _midori_view_set_settings (view, settings);
    if (item)
    {
        katze_object_assign (view->item, katze_item_copy (item));
        view->minimized = katze_item_get_meta_string (
            view->item, "minimized") != NULL;
    }
    gtk_widget_show ((GtkWidget*)view);
    return (GtkWidget*)view;
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

    if (name == g_intern_string ("zoom-text-and-images"))
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
        #ifndef HAVE_GRANITE
        sokoke_widget_set_visible (view->tab_close,
                                   view->close_buttons_on_tabs);
        #endif
    }
    else if (name == g_intern_string ("open-new-pages-in"))
        view->open_new_pages_in = g_value_get_enum (&value);
    else if (name == g_intern_string ("middle-click-opens-selection"))
        view->middle_click_opens_selection = g_value_get_boolean (&value);
    else if (name == g_intern_string ("open-tabs-in-the-background"))
        view->open_tabs_in_the_background = g_value_get_boolean (&value);
    else if (name == g_intern_string ("enable-scripts"))
    {
        /* Speed dial is only editable with scripts, so regenerate it */
        if (midori_view_is_blank (view))
            midori_view_reload (view, FALSE);
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
    g_return_if_fail (MIDORI_IS_WEB_SETTINGS (settings));

    if (view->settings == settings)
        return;

    _midori_view_set_settings (view, settings);
    g_object_notify (G_OBJECT (view), "settings");
}

/**
 * midori_view_load_status:
 * @web_view: a #MidoriView
 *
 * Determines the current loading status of a view. There is no
 * error state, unlike webkit_web_view_get_load_status().
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

    /* When we are finished, we don't want to *see* progress anymore */
    if (view->load_status == MIDORI_LOAD_FINISHED)
        return 0.0;
    /* Full progress but not finished: presumably all loaded */
    if (view->progress == 1.0)
        return 0.0;
    /* When loading we want to see at minimum 10% progress */
    return CLAMP (view->progress, 0.1, 1.0);
}

static void
midori_view_web_inspector_construct_window (gpointer       inspector,
                                            WebKitWebView* web_view,
                                            GtkWidget*     inspector_view,
                                            MidoriView*    view)
{
    gchar* title;
    const gchar* label;
    GtkWidget* window;
    GtkWidget* toplevel;
    GdkScreen* screen;
    gint width, height;
    const gchar* icon_name;
    GtkIconTheme* icon_theme;
    GdkPixbuf* icon;
    GdkPixbuf* gray_icon;

    label = midori_view_get_display_title (view);
    title = g_strdup_printf (_("Inspect page - %s"), label);
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
        /* 700x100 is the approximate useful minimum dimensions */
        gtk_widget_set_size_request (inspector_view, 700, 100);
    }

    /* Attempt to make a gray version of the icon on the fly */
    icon_name = gtk_window_get_icon_name (GTK_WINDOW (toplevel));
    icon_theme = gtk_icon_theme_get_for_screen (
        gtk_widget_get_screen (GTK_WIDGET (view)));
    icon = gtk_icon_theme_load_icon (icon_theme, icon_name, 32,
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
        gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
    #if GTK_CHECK_VERSION (3, 4, 0)
    gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (window), TRUE);
    #endif
    gtk_container_add (GTK_CONTAINER (window), inspector_view);
    gtk_widget_show_all (inspector_view);

    g_signal_connect (window, "key-press-event",
        G_CALLBACK (midori_view_inspector_window_key_press_event_cb), NULL);

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
midori_view_web_inspector_show_window_cb (WebKitWebInspector* inspector,
                                          MidoriView*         view)
{
    WebKitWebView* inspector_view = webkit_web_inspector_get_web_view (inspector);
    GtkWidget* window = gtk_widget_get_toplevel (GTK_WIDGET (inspector_view));
    if (!window)
        return FALSE;
    if (katze_object_get_boolean (view->settings, "last-inspector-attached"))
        g_signal_emit_by_name (inspector, "attach-window");
    else
    {
        gtk_widget_show (window);
        gtk_window_present (GTK_WINDOW (window));
    }
    return TRUE;
}

static gboolean
midori_view_web_inspector_attach_window_cb (gpointer    inspector,
                                            MidoriView* view)
{
    WebKitWebView* inspector_view = webkit_web_inspector_get_web_view (inspector);
    g_signal_emit (view, signals[ATTACH_INSPECTOR], 0, inspector_view);
    return TRUE;
}

static gboolean
midori_view_web_inspector_detach_window_cb (gpointer    inspector,
                                            MidoriView* view)
{
    WebKitWebView* inspector_view = webkit_web_inspector_get_web_view (inspector);
    GtkWidget* parent = gtk_widget_get_parent (GTK_WIDGET (inspector_view));
    if (GTK_IS_WINDOW (parent))
        return FALSE;

    gtk_widget_hide (parent);
    g_signal_emit (view, signals[DETACH_INSPECTOR], 0, inspector_view);
    midori_view_web_inspector_construct_window (inspector,
        WEBKIT_WEB_VIEW (view->web_view), GTK_WIDGET (inspector_view), view);
    return TRUE;
}

static gboolean
midori_view_web_inspector_close_window_cb (gpointer    inspector,
                                           MidoriView* view)
{
    WebKitWebView* inspector_view = webkit_web_inspector_get_web_view (inspector);
    GtkWidget* scrolled = gtk_widget_get_parent (GTK_WIDGET (inspector_view));
    if (!scrolled)
        return FALSE;
    gtk_widget_hide (gtk_widget_get_parent (scrolled));
    return TRUE;
}

static void
midori_view_construct_web_view (MidoriView* view)
{
    gpointer inspector;

    g_return_if_fail (!view->web_view);

    view->web_view = webkit_web_view_new ();

    /* Load something to avoid a bug where WebKit might not set a main frame */
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view->web_view), "");

    #if HAVE_HILDON
    gtk_widget_tap_and_hold_setup (view->web_view, NULL, NULL, 0);
    g_signal_connect (view->web_view, "tap-and-hold",
                      G_CALLBACK (midori_view_web_view_tap_and_hold_cb), NULL);
    #endif

    g_object_connect (view->web_view,
                      "signal::navigation-policy-decision-requested",
                      midori_view_web_view_navigation_decision_cb, view,
                      "signal::resource-request-starting",
                      midori_view_web_view_resource_request_cb, view,
                      "signal::database-quota-exceeded",
                      midori_view_web_view_database_quota_exceeded_cb, view,
                      #if WEBKIT_CHECK_VERSION (1, 1, 23)
                      "signal::geolocation-policy-decision-requested",
                      midori_view_web_view_geolocation_decision_cb, view,
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
                      "signal::notify::uri",
                      webkit_web_view_notify_uri_cb, view,
                      "signal::notify::title",
                      webkit_web_view_notify_title_cb, view,
                      "signal::status-bar-text-changed",
                      webkit_web_view_statusbar_text_changed_cb, view,
                      "signal::leave-notify-event",
                      midori_view_web_view_leave_notify_event_cb, view,
                      "signal::hovering-over-link",
                      webkit_web_view_hovering_over_link_cb, view,
                      "signal::button-press-event",
                      midori_view_web_view_button_press_event_cb, view,
                      "signal::button-release-event",
                      midori_view_web_view_button_release_event_cb, view,
                      "signal-after::key-press-event",
                      gtk_widget_key_press_event_cb, view,
                      "signal::scroll-event",
                      gtk_widget_scroll_event_cb, view,
                      "signal::populate-popup",
                      webkit_web_view_populate_popup_cb, view,
                      "signal::console-message",
                      webkit_web_view_console_message_cb, view,
                      "signal::script-alert",
                      midori_view_web_view_script_alert_cb, view,
                      "signal::window-object-cleared",
                      webkit_web_view_window_object_cleared_cb, view,
                      "signal::create-web-view",
                      webkit_web_view_create_web_view_cb, view,
                      "signal-after::mime-type-policy-decision-requested",
                      webkit_web_view_mime_type_decision_cb, view,
                      "signal::download-requested",
                      webkit_web_view_download_requested_cb, view,
                      "signal::print-requested",
                      midori_view_web_view_print_requested_cb, view,
                      "signal-after::load-error",
                      webkit_web_view_load_error_cb, view,
                      NULL);

    if (view->settings)
    {
        g_object_set (view->web_view, "settings", view->settings,
            "full-content-zoom", katze_object_get_boolean (view->settings,
                "zoom-text-and-images"), NULL);
    }

    gtk_container_add (GTK_CONTAINER (view->scrolled_window), view->web_view);
    gtk_widget_show_all (view->scrolled_window);

    inspector = webkit_web_view_get_inspector ((WebKitWebView*)view->web_view);
    g_object_connect (inspector,
                      "signal::inspect-web-view",
                      midori_view_web_inspector_inspect_web_view_cb, view,
                      "signal::show-window",
                      midori_view_web_inspector_show_window_cb, view,
                      "signal::attach-window",
                      midori_view_web_inspector_attach_window_cb, view,
                      "signal::detach-window",
                      midori_view_web_inspector_detach_window_cb, view,
                      "signal::close-window",
                      midori_view_web_inspector_close_window_cb, view,
                      NULL);
}

static void
list_netscape_plugins (GString*     ns_plugins,
                       JSContextRef js_context)
{
    if (!midori_web_settings_has_plugin_support ())
        return;

    /* Joins available plugins like this: URI1|title1,URI2|title2 */
    gchar* value = sokoke_js_script_eval (js_context,
        "function plugins (l) { var f = new Array (); for (var i in l) "
        "{ var p = l[i].name + '|' + l[i].filename; "
        "if (f.indexOf (p) == -1) f.push (p); } return f; }"
        "plugins (navigator.plugins)", NULL);
    gchar** items = g_strsplit (value, ",", 0);
    guint i = 0;
    g_string_append (ns_plugins, "<h2>Netscape Plugins:</h2><table>");
    if (items != NULL)
        while (items[i] != NULL)
        {
            gchar** parts = g_strsplit (items[i], "|", 2);
            if (parts && *parts && !g_str_equal (parts[1], "undefined"))
            {
                g_string_append (ns_plugins, "<tr><td>");
                g_string_append (ns_plugins, parts[1]);
                g_string_append (ns_plugins, "</td><td>");
                g_string_append (ns_plugins, parts[0]);
                g_string_append (ns_plugins, "</tr>");
            }
            g_strfreev (parts);
            i++;
        }
        if (g_str_has_prefix (value, "undefined"))
            g_string_append (ns_plugins, "<tr><td>No plugins found</td></tr>");
        g_string_append (ns_plugins, "</table>");
        g_strfreev (items);
        g_free (value);
}

static void
list_geolocation (GString* markup)
{
    g_string_append (markup,
    "<a href=\"http://dev.w3.org/geo/api/spec-source.html\" id=\"method\"></a>"
    "<span id=\"locationInfo\"><noscript>No Geolocation without Javascript</noscript></span>"
    "<script>"
    "function displayLocation (position) {"
    "var geouri = 'geo:' + position.coords.latitude + ',' + position.coords.longitude + ',' + position.coords.altitude + ',u=' + position.coords.accuracy;"
    "document.getElementById('locationInfo').innerHTML = '<a href=\"' + geouri + '\">' + geouri + '</a><br><code>'"
    "+ ' timestamp: ' + position.timestamp"
    "+ ' latitude: ' + position.coords.latitude"
    "+ ' longitude: ' + position.coords.longitude"
    "+ ' altitude: ' + position.coords.altitude + '<br>'"
    "+ ' accuracy: ' + position.coords.accuracy"
    "+ ' altitudeAccuracy: ' + position.coords.altitudeAccuracy"
    "+ ' heading: ' + position.coords.heading"
    "+ ' speed: ' + position.coords.speed"
    "+ '</code>'; }"
    "function handleError (error) {"
    "var errorMessage = '<b>' + ['Unknown error', 'Permission denied', 'Position failed', 'Timed out'][error.code] + '</b>';"
    "if (error.code == 3) document.getElementById('locationInfo').innerHTML += (' ' + errorMessage);"
    "else document.getElementById('locationInfo').innerHTML = errorMessage; }"
    "if (navigator.geolocation) {"
    "var options = { enableHighAccuracy: true, timeout: 60000, maximumAge: \"Infinite\" };"
    "  if (navigator.geolocation.watchPosition) {"
    "    document.getElementById('method').innerHTML = '<code>geolocation.watchPosition</code>:';"
    "    navigator.geolocation.watchPosition(displayLocation, handleError, options);"
    "  } else {"
    "    document.getElementById('method').innerHTML = '<code>geolocation.getCurrentPosition</code>:';"
    "    navigator.geolocation.getCurrentPosition(displayLocation, handleError);"
    "  }"
    "} else"
    "  document.getElementById('locationInfo').innerHTML = 'Geolocation unavailable';"
    "</script>");
}

static gchar*
list_video_formats (JSContextRef js_context)
{
    gchar* value = sokoke_js_script_eval (js_context,
        "var supported = function (format) { "
        "var video = document.createElement('video');"
        "return !!video.canPlayType && video.canPlayType (format) != 'no' "
        "? 'x' : '&nbsp;&nbsp;'; };"
        "' H264 [' +"
        "supported('video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\"') + ']' + "
        "' &nbsp; Ogg Theora [' + "
        "supported('video/ogg; codecs=\"theora, vorbis\"') + ']' + "
        "' &nbsp; WebM [' + "
        "supported('video/webm; codecs=\"vp8, vorbis\"') + ']' "
        "", NULL);
    return value;
}

static const gchar* valid_about_uris[] = {
    "about:widgets",
    "about:private",
    "error:nodocs",
    "http://.invalid",
    "about:geolocation",
    "about:paths",
};

static void
list_about_uris (GString* markup)
{
    g_string_append (markup, "<p>");
    guint i;
    for (i = 0; i < G_N_ELEMENTS (valid_about_uris); i++)
        g_string_append_printf (markup, "<a href=\"%s\">%s</a> &nbsp;",
                                valid_about_uris[i], valid_about_uris[i]);
}


/**
 * midori_view_set_uri:
 * @view: a #MidoriView
 *
 * Opens the specified URI in the view.
 *
 * Since 0.3.0 a warning is shown if the view is not yet
 * contained in a browser. This is because extensions
 * can't monitor page loading if that happens.
 **/
void
midori_view_set_uri (MidoriView*  view,
                     const gchar* uri)
{
    gchar* data;

    g_return_if_fail (MIDORI_IS_VIEW (view));

    if (!gtk_widget_get_parent (GTK_WIDGET (view)))
        g_warning ("Calling %s() before adding the view to a browser. This "
                   "breaks extensions that monitor page loading.", G_STRFUNC);

    if (!midori_debug ("unarmed"))
    {
        if (!uri || !strcmp (uri, "") || !strcmp (uri, "about:blank"))
        {
            MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (view));
            MidoriSpeedDial* dial = katze_object_get_object (browser, "speed-dial");
            const gchar* html;
            #ifdef G_ENABLE_DEBUG
            GTimer* timer = NULL;

            if (midori_debug ("startup"))
                timer = g_timer_new ();
            #endif

            katze_assign (view->uri, NULL);
            katze_assign (view->mime_type, NULL);
            katze_item_set_meta_string (view->item, "mime-type", NULL);
            katze_item_set_meta_integer (view->item, "delay", -1);

            html = midori_speed_dial_get_html (dial, NULL);
            midori_view_load_alternate_string (view, html, "about:blank", NULL);

            #ifdef G_ENABLE_DEBUG
            if (midori_debug ("startup"))
            {
                g_debug ("Speed Dial: \t%fs", g_timer_elapsed (timer, NULL));
                g_timer_destroy (timer);
            }
            #endif
        }
        /* This is not prefectly elegant, but creating
           special pages inline is the simplest solution. */
        else if (g_str_has_prefix (uri, "error:") || midori_uri_is_blank (uri))
        {
            data = NULL;
            if (!strncmp (uri, "error:nodocs ", 13))
            {
                gchar* title;

                katze_assign (view->uri, g_strdup (&uri[13]));
                title = g_strdup_printf (_("No documentation installed"));
                data = g_strdup_printf (
                    "<html><head><title>%s</title></head>"
                    "<body><h1>%s</h1>"
                    "<img src=\"res://logo-shade.png\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "<p />There is no documentation installed at %s. "
                    "You may want to ask your distribution or "
                    "package maintainer for it or if this a custom build "
                    "verify that the build is setup properly. "
                    "<a href=\"http://wiki.xfce.org/midori/faq\">View the FAQ online</a>"
                    "</body></html>",
                    title, title, view->uri);
                g_free (title);
            }
            else if (!strcmp (uri, "about:widgets"))
            {
                static const gchar* widgets[] = {
                    "<input value=\"demo\"%s>",
                    "<p><input type=\"password\" value=\"demo\"%s>",
                    "<p><input type=\"checkbox\" value=\"demo\"%s> demo",
                    "<p><input type=\"radio\" value=\"demo\"%s> demo",
                    "<p><select%s><option>foo bar</option><option selected>spam eggs</option></select>",
                    "<p><select%s size=\"3\"><option>foo bar</option><option selected>spam eggs</option></select>",
                    "<p><input type=\"file\"%s>",
                    "<p><input type=\"file\" multiple%s>",
                    "<input type=\"button\" value=\"demo\"%s>",
                    "<p><input type=\"email\" value=\"user@localhost.com\"%s>",
                    "<input type=\"url\" value=\"http://www.example.com\"%s>",
                    "<input type=\"tel\" value=\"+1 234 567 890\" pattern=\"^[0+][1-9 /-]*$\"%s>",
                    "<input type=\"number\" min=1 max=9 step=1 value=\"4\"%s>",
                    "<input type=\"range\" min=1 max=9 step=1 value=\"4\"%s>",
                    "<input type=\"date\" min=1990-01-01 max=2010-01-01%s>",
                    "<input type=\"search\" placeholder=\"demo\"%s>",
                    "<textarea%s>Lorem ipsum doloret sit amet…</textarea>",
                    "<input type=\"color\" value=\"#d1eeb9\"%s>",
                    "<progress min=1 max=9 value=4 %s></progress>",
                    "<keygen type=\"rsa\" challenge=\"235ldahlae983dadfar\"%s>",
                    "<p><input type=\"reset\"%s>",
                    "<input type=\"submit\"%s>",
                };
                guint i;
                GString* demo = g_string_new ("<html><head><style>"
                    ".fallback::-webkit-slider-thumb,"
                    ".fallback, .fallback::-webkit-file-upload-button {"
                    "-webkit-appearance: none !important }"
                    ".column { display:inline-block; vertical-align:top;"
                    "width:25%;margin-right:1% }</style><title>");
                g_string_append_printf (demo,
                    "%s</title></head><body><h1>%s</h1>", uri, uri);
                g_string_append (demo, "<div class=\"column\">");
                for (i = 0; i < G_N_ELEMENTS (widgets); i++)
                    g_string_append_printf (demo, widgets[i], "");
                g_string_append (demo, "</div><div class=\"column\">");
                for (i = 0; i < G_N_ELEMENTS (widgets); i++)
                    g_string_append_printf (demo, widgets[i], " disabled");
                g_string_append (demo, "</div><div class=\"column\">");
                for (i = 0; i < G_N_ELEMENTS (widgets); i++)
                    g_string_append_printf (demo, widgets[i], " class=\"fallback\"");
                g_string_append (demo, "</div>");
                katze_assign (view->uri, g_strdup (uri));
                data = g_string_free (demo, FALSE);
            }
            else if (!strcmp (uri, "about:private"))
            {
                katze_assign (view->uri, g_strdup (""));
                data = g_strdup_printf (
                    "<html><head><title>%s</title>"
                    "<link rel=\"stylesheet\" type=\"text/css\" href=\"res://about.css\">"
                    "</head><body><div id=\"container\">"
                    "<img id=\"logo\" src=\"res://logo-shade.png\">"
                    "<img id=\"icon\" src=\"stock://gtk-dialog-info\">"
                    "<div id=\"main\"><h1>%s</h1>"
                    "<p>%s</p><ul><li>%s</li><li>%s</li><li>%s</li></ul>"
                    "<p>%s</p><ul><li>%s</li><li>%s</li><li>%s</li><li>%s</li></ul>"
                    "</div><br style=\"clear: both\"></div></body></html>",
                    _("Private Browsing"), _("Private Browsing"),
                    _("Midori doesn't store any personal data:"),
                    _("No history or web cookies are being saved."),
                    _("Extensions are disabled."),
                    _("HTML5 storage, local database and application caches are disabled."),
                    _("Midori prevents websites from tracking the user:"),
                    _("Referrer URLs are stripped down to the hostname."),
                    _("DNS prefetching is disabled."),
                    _("The language and timezone are not revealed to websites."),
                    _("Flash and other Netscape plugins cannot be listed by websites."));
            }
            else if (!strcmp (uri, "about:geolocation"))
            {
                GString* markup = g_string_new ("");
                katze_assign (view->uri, g_strdup (uri));
                list_geolocation (markup);
                data = g_string_free (markup, FALSE);
            }
            else if (!strcmp (uri, "about:paths"))
            {
                gchar* res_dir = midori_paths_get_res_filename ("");
                gchar* lib_dir = midori_paths_get_lib_path (PACKAGE_NAME);
                data = g_strdup_printf ("<body><h1>%s</h1>"
                    "<p>config: <code>%s</code></p>"
                    "<p>res: <code>%s</code></p>"
                    "<p>lib: <code>%s</code></p>"
                    "<p>cache: <code>%s</code></p>"
                    "<p>tmp: <code>%s</code></p>"
                    "</body>",
                    uri, midori_paths_get_config_dir (), res_dir,
                    lib_dir, midori_paths_get_cache_dir (), midori_paths_get_tmp_dir ());
                g_free (res_dir);
                g_free (lib_dir);
                katze_assign (view->uri, g_strdup (uri));
            }
            else if (!strcmp (uri, "about:") || !strcmp (uri, "about:version"))
            {
                gchar* command_line = midori_paths_get_command_line_str ();
                gchar* architecture, *platform;
                const gchar* sys_name = midori_web_settings_get_system_name (
                    &architecture, &platform);
                gchar* ident = katze_object_get_string (view->settings, "user-agent");
                WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
                JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
                gchar* video_formats = list_video_formats (js_context);

                /* FIXME: This is for workarounding a crash deeper down the callstack on some systems. */
                static char const * const version_format_strings[] = {
                    " &nbsp; libsoup %s</td></tr>",
                    "<tr><td>cairo</td><td>%s ", "(%s)</td></tr>",
                    "<tr><td>gcr</td><td>%s</td></tr>",
                    "<tr><td>granite</td><td>%s</td></tr>",
                    "<tr><td>libnotify</td><td>%s</td></tr>",
                    "<tr><td>single instance</td><td>%s</td></tr>",
                    "<tr><td>Platform</td><td>%s ", "%s ", "%s</td></tr>",
                    "<tr><td>Identification</td><td><code>%s</code></td></tr>",
                    "<tr><td>Video&nbsp;Formats</td><td>%s</td></tr>",
                };
                gchar const* version_strings[] = {
                    LIBSOUP_VERSION,
                    CAIRO_VERSION_STRING, cairo_version_string (),
                    GCR_VERSION,
                    GRANITE_VERSION,
                    LIBNOTIFY_VERSION,
                    #ifdef HAVE_HILDON_2_2
                    "Hildon 2.2",
                    #elif HAVE_HILDON
                    "Hildon",
                    #elif HAVE_UNIQUE
                    "libunique " UNIQUE_VERSION,
                    #else
                    "Sockets",
                    #endif
                    platform, sys_name, architecture ? architecture : "", ident,
                    video_formats,
                };
                gsize i = 0;
                GString * tmp = g_string_new ("");
                GString* more = g_string_new ("");
                list_netscape_plugins (more, js_context);
                list_about_uris (more);

                katze_assign (view->uri, g_strdup (uri));
                g_string_append_printf (tmp,
                    "<html><head><title>about:version</title></head>"
                    "<body><h1>about:version</h1>"
                    "<p>%s</p>"
                    "<img src=\"res://logo-shade.png\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "<table>"
                    "<tr><td>Command&nbsp;line</td><td><code>%s</code></td></tr>"
                    "<tr><td>Midori</td><td>%s (%s)</td></tr>"
                    "<tr><td>GTK+</td><td>%d.%d.%d (%d.%d.%d)"
                    " &nbsp; Glib %d.%d.%d (%d.%d.%d)</td></tr>"
                    "<tr><td>WebKitGTK+</td><td>%d.%d.%d (%d.%d.%d)",
                    _("Version numbers in brackets show the version used at runtime."),
                    command_line,
                    PACKAGE_VERSION, midori_app_get_name (NULL),
                    GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                    gtk_major_version, gtk_minor_version, gtk_micro_version,
                    GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
                    glib_major_version, glib_minor_version, glib_micro_version,
                    WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION,
                    webkit_major_version (), webkit_minor_version (), webkit_micro_version ());

                for (i = 0;
                     i < sizeof (version_format_strings) / sizeof (version_format_strings[0]);
                     ++i)
                    g_string_append_printf (tmp, version_format_strings[i], version_strings[i]);

                g_string_append_printf (
                    tmp, "</table>%s</body></html>", (gchar*)(more->str));

                data = g_string_free (tmp, FALSE);

                g_free (command_line);
                g_free (ident);
                g_free (video_formats);
                g_string_free (more, TRUE);
           }
            else
            {
                katze_assign (view->uri, g_strdup (uri));
                data = g_strdup_printf (
                    "<html><head><title>%s</title></head><body><h1>%s</h1>"
                    "<img src=\"res://logo-shade.png\" "
                    "style=\"position: absolute; right: 15px; bottom: 15px; z-index: -9;\">"
                    "</body></html>", view->uri, view->uri);
            }

            webkit_web_view_load_html_string (
                WEBKIT_WEB_VIEW (view->web_view), data, view->uri);
            g_free (data);
            katze_item_set_meta_integer (view->item, "delay", -1);
            if (g_strcmp0 (view->item->uri, view->uri))
                katze_item_set_uri (view->item, view->uri);
            g_object_notify (G_OBJECT (view), "uri");
        }
        else if (katze_item_get_meta_boolean (view->item, "delay"))
        {
            katze_assign (view->uri, g_strdup (uri));
            katze_item_set_meta_integer (view->item, "delay", -2);
            midori_view_display_error (
                view, view->uri, view->title ? view->title : view->uri,
                _("Page loading delayed"),
                _("Loading delayed either due to a recent crash or startup preferences."),
                _("Load Page"),
                NULL);
            if (g_strcmp0 (view->item->uri, uri))
                katze_item_set_uri (view->item, uri);
            g_object_notify (G_OBJECT (view), "uri");
        }
        else if (g_str_has_prefix (uri, "javascript:"))
        {
            gboolean result;
            gchar* exception;

            result = midori_view_execute_script (view, &uri[11], &exception);
            if (!result)
            {
                sokoke_message_dialog (GTK_MESSAGE_ERROR, "javascript:",
                                       exception, FALSE);
                g_free (exception);
            }
        }
        else if (sokoke_external_uri (uri))
        {
            sokoke_show_uri (NULL, uri, GDK_CURRENT_TIME, NULL);
        }
        else
        {
            katze_assign (view->uri, midori_uri_format_for_display (uri));
            if (g_strcmp0 (view->item->uri, view->uri))
                katze_item_set_uri (view->item, view->uri);
            katze_assign (view->title, NULL);
            g_object_notify (G_OBJECT (view), "uri");
            webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view->web_view), uri);
        }
    }
}

/**
 * midori_view_set_overlay_text:
 * @view: a #MidoriView
 * @text: a URI or text string
 *
 * Show a specified URI or text on top of the view.
 * Has no effect with < GTK+ 3.2.0.
 *
 * Since: 0.4.5
 **/
void
midori_view_set_overlay_text (MidoriView*  view,
                              const gchar* text)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    #if GTK_CHECK_VERSION (3, 2, 0)
    if (text == NULL)
        gtk_widget_hide (gtk_widget_get_parent (view->overlay_label));
    else
    {
        gtk_label_set_text (GTK_LABEL (view->overlay_label), text);
        gtk_widget_show (gtk_widget_get_parent (view->overlay_label));
    }
    #endif
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

    return midori_uri_is_blank (midori_view_get_display_uri (view));
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
#if WEBKIT_CHECK_VERSION (1, 4, 3)
    WebKitDOMDocument* doc;
    WebKitDOMDOMWindow* window;
    WebKitDOMDOMSelection* selection;
    WebKitDOMRange* range;
#endif

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);


#if WEBKIT_CHECK_VERSION (1, 4, 3)
    doc = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view->web_view));
    window = webkit_dom_document_get_default_view (doc);
    selection = webkit_dom_dom_window_get_selection (window);
    if (selection == NULL
     || webkit_dom_dom_selection_get_range_count (selection) == 0)
        return FALSE;

    range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
    if (range == NULL)
        return FALSE;

    katze_assign (view->selected_text, webkit_dom_range_get_text (range));
#else
    katze_assign (view->selected_text, webkit_web_view_get_selected_text (
        WEBKIT_WEB_VIEW (view->web_view)));
#endif

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
midori_view_tab_label_menu_duplicate_tab_cb (GtkWidget*  menuitem,
                                             MidoriView* view)
{
    MidoriNewView where = MIDORI_NEW_VIEW_TAB;
    GtkWidget* new_view = midori_view_new_with_item (view->item, view->settings);
    g_signal_emit (view, signals[NEW_VIEW], 0, new_view, where, TRUE);
    midori_view_set_uri (MIDORI_VIEW (new_view), view->uri);
}

static void
midori_view_browser_close_tabs_cb (GtkWidget* view,
                                   gpointer   data)
{
    GtkWidget* remaining_view = data;
    if (view != remaining_view)
         midori_browser_remove_tab (midori_browser_get_for_widget (view), view);
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
    midori_browser_remove_tab (midori_browser_get_for_widget (view), view);
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
    gint pages;
    GtkWidget* menu;
    GtkWidget* menuitem;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    browser = midori_browser_get_for_widget (GTK_WIDGET (view));
    actions = midori_browser_get_action_group (browser);
    pages = midori_browser_get_n_pages (browser);

    menu = gtk_menu_new ();
    menuitem = sokoke_action_create_popup_menu_item (
        gtk_action_group_get_action (actions, "TabNew"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = sokoke_action_create_popup_menu_item (
        gtk_action_group_get_action (actions, "UndoTabClose"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
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
        view->minimized ? _("Show Tab _Label") : _("Show Tab _Icon Only"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_minimize_tab_cb), view);
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    menuitem = gtk_menu_item_new_with_mnemonic (
        g_dngettext (NULL, "Close Ot_her Tab", "Close Ot_her Tabs", pages - 1));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_close_other_tabs_cb), view);
    gtk_widget_set_sensitive (menuitem, pages > 1);
    menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (midori_view_tab_label_menu_close_cb), view);
    gtk_widget_show_all (menu);

    return menu;
}

#ifdef HAVE_GRANITE
GraniteWidgetsTab*
midori_view_get_tab (MidoriView* view)
{
    if (view->tab == NULL)
        view->tab = granite_widgets_tab_new (
            midori_view_get_display_title (view), G_ICON (view->icon), GTK_WIDGET (view));
    return view->tab;
}

void
midori_view_set_tab (MidoriView*        view,
                     GraniteWidgetsTab* tab)
{
    g_return_if_fail (view->tab == NULL);

    view->tab = tab;
    g_object_set (tab,
        "label", midori_view_get_display_title (view),
        "icon", G_ICON (view->icon),
        "page", GTK_WIDGET (view),
        NULL);
}
#else
static gboolean
midori_view_tab_label_button_press_event (GtkWidget*      tab_label,
                                          GdkEventButton* event,
                                          GtkWidget*      widget)
{
    if (event->button == 2)
    {
        /* Close the widget on middle click */
        midori_browser_remove_tab (midori_browser_get_for_widget (widget), widget);
        return TRUE;
    }
    else if (MIDORI_EVENT_CONTEXT_MENU (event))
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
    midori_browser_remove_tab (midori_browser_get_for_widget (widget), widget);
}

#if !GTK_CHECK_VERSION (3, 0, 0)
static void
midori_view_tab_icon_style_set_cb (GtkWidget* tab_close,
                                   GtkStyle*  previous_style)
{
    GtkRequisition size;
    gtk_widget_size_request (gtk_bin_get_child (GTK_BIN (tab_close)), &size);
    gtk_widget_set_size_request (tab_close, size.width, size.height);
}
#endif

static void
midori_view_update_tab_title (MidoriView* view,
                              gdouble     angle)
{
    if (angle == 0.0)
        gtk_label_set_ellipsize (GTK_LABEL (view->tab_title), view->ellipsize);
    else
        gtk_label_set_ellipsize (GTK_LABEL (view->tab_title), PANGO_ELLIPSIZE_NONE);
    gtk_label_set_angle (GTK_LABEL (view->tab_title), angle);
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
#if GTK_CHECK_VERSION(3,0,0)
                                  GObject*  old_parent,
#else
                                  GtkObject*  old_parent,
#endif
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
            GtkWidget* align;

            if (angle == 0.0)
                box = gtk_hbox_new (FALSE, 1);
            else
                box = gtk_vbox_new (FALSE, 1);
            gtk_box_repack (GTK_BOX (box), view->tab_icon);
            gtk_box_repack (GTK_BOX (box), view->tab_title);
            align = gtk_widget_get_parent (view->tab_close);
            gtk_box_repack (GTK_BOX (box), align);

            gtk_container_remove (GTK_CONTAINER (tab_label),
                gtk_bin_get_child (GTK_BIN (tab_label)));
            gtk_container_add (GTK_CONTAINER (tab_label), GTK_WIDGET (box));
            gtk_widget_show (box);
        }

        midori_view_update_tab_title (view, angle);

        /* FIXME: Connect orientation notification */
    }
}

static void midori_view_tab_label_data_received (GtkWidget* widget,
                                                 GdkDragContext* context,
                                                 gint x,
                                                 gint y,
                                                 GtkSelectionData* data,
                                                 guint ttype,
                                                 guint timestamp,
                                                 MidoriView* view)
{
    gchar **uri;
    gchar* text;

    uri = gtk_selection_data_get_uris (data);
    if (uri != NULL)
    {
        midori_view_set_uri (view, uri[0]);
        g_strfreev (uri);
    }
    else
    {
        text = (gchar*) gtk_selection_data_get_text (data);
        midori_view_set_uri (view, text);
        g_free (text);
    }
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
    #if !GTK_CHECK_VERSION (3, 0, 0)
    static const gchar style_fixup[] =
        "style \"midori-close-button-style\"\n"
        "{\n"
        "GtkWidget::focus-padding = 0\n"
        "GtkWidget::focus-line-width = 0\n"
        "xthickness = 0\n"
        "ythickness = 0\n"
        "}\n"
        "widget \"*.midori-close-button\" style \"midori-close-button-style\"";
    #endif
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
        gtk_misc_set_padding (GTK_MISC (view->tab_title), 0, 0);

        event_box = gtk_event_box_new ();
        gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
        hbox = gtk_hbox_new (FALSE, 1);
        gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (hbox));
        midori_view_update_tab_title (view, 0.0);

        view->tab_close = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (view->tab_close), GTK_RELIEF_NONE);
        gtk_button_set_focus_on_click (GTK_BUTTON (view->tab_close), FALSE);
        #if !GTK_CHECK_VERSION (3, 0, 0)
        gtk_rc_parse_string (style_fixup);
        gtk_widget_set_name (view->tab_close, "midori-close-button");
        g_signal_connect (view->tab_close, "style-set",
            G_CALLBACK (midori_view_tab_icon_style_set_cb), NULL);
        #endif
        image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
        gtk_container_add (GTK_CONTAINER (view->tab_close), image);
        align = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
        gtk_container_add (GTK_CONTAINER (align), view->tab_close);

        if (katze_object_get_boolean (view->settings, "close-buttons-left"))
        {
            gtk_box_pack_end (GTK_BOX (hbox), view->tab_icon, FALSE, FALSE, 0);
            gtk_box_pack_end (GTK_BOX (hbox), view->tab_title, TRUE, TRUE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
        }
        else
        {
            gtk_box_pack_start (GTK_BOX (hbox), view->tab_icon, FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), view->tab_title, TRUE, TRUE, 0);
            gtk_box_pack_end (GTK_BOX (hbox), align, FALSE, FALSE, 0);
        }
        gtk_widget_show_all (GTK_WIDGET (event_box));

        if (view->minimized)
            gtk_widget_hide (view->tab_title);
        if (!view->close_buttons_on_tabs)
            gtk_widget_hide (view->tab_close);

        g_signal_connect (event_box, "button-press-event",
            G_CALLBACK (midori_view_tab_label_button_press_event), view);
        g_signal_connect (view->tab_close, "button-press-event",
            G_CALLBACK (midori_view_tab_label_button_press_event), view);
        g_signal_connect (view->tab_close, "clicked",
            G_CALLBACK (midori_view_tab_close_clicked), view);

        view->tab_label = event_box;
        g_signal_connect (view->tab_icon, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->tab_icon);
        g_signal_connect (view->tab_label, "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
                          &view->tab_label);

        g_signal_connect (view->tab_label, "parent-set",
                          G_CALLBACK (midori_view_tab_label_parent_set),
                          view);
        gtk_drag_dest_set (view->tab_label, GTK_DEST_DEFAULT_ALL, NULL,
                           0, GDK_ACTION_COPY);
        gtk_drag_dest_add_text_targets (view->tab_label);
        gtk_drag_dest_add_uri_targets (view->tab_label);
        g_signal_connect (view->tab_label, "drag-data-received",
                          G_CALLBACK (midori_view_tab_label_data_received),
                          view);
    }
    return view->tab_label;
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

    return view->ellipsize;
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
 * The item reflects changes to title (name), URI and MIME type (mime-type).
 *
 * Return value: the proxy #KatzeItem
 **/
KatzeItem*
midori_view_get_proxy_item (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

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

    return view->web_view != NULL
        && (katze_object_get_boolean (view->settings, "zoom-text-and-images")
        || !g_str_has_prefix (view->mime_type ? view->mime_type : "", "image/"));
}

gboolean
midori_view_can_zoom_out (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    return view->web_view != NULL
        && (katze_object_get_boolean (view->settings, "zoom-text-and-images")
        || !g_str_has_prefix (view->mime_type ? view->mime_type : "", "image/"));
}

gboolean
midori_view_can_view_source (MidoriView* view)
{
    gchar* content_type;
    gchar* text_type;
    gboolean is_text;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (midori_view_is_blank (view) || view->mime_type == NULL)
        return FALSE;

    content_type = g_content_type_from_mime_type (view->mime_type);
#ifdef _WIN32
    /* On Win32 text/plain maps to ".txt" but is_a expects "text" */
    text_type = g_strdup ("text");
#else
    text_type = g_content_type_from_mime_type ("text/plain");
#endif
    is_text = g_content_type_is_a (content_type, text_type);
    g_free (content_type);
    g_free (text_type);
    return is_text;
}

/**
 * midori_view_can_save:
 * @view: a #MidoriView
 *
 * Determines if the view can be saved to disk.
 *
 * Return value: %TRUE if the website or image can be saved
 *
 * Since: 0.4.3
 **/
gboolean
midori_view_can_save (MidoriView* view)
{
    GtkWidget* web_view;
    WebKitWebDataSource *data_source;
    WebKitWebFrame *frame;
    const GString *data;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (midori_view_is_blank (view) || view->mime_type == NULL)
        return FALSE;

    web_view = midori_view_get_web_view (view);
    if (webkit_web_view_get_view_source_mode (WEBKIT_WEB_VIEW (web_view)))
        return FALSE;

    frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));
    data_source = webkit_web_frame_get_data_source (frame);
    data = webkit_web_data_source_get_data (data_source);

    if (data != NULL)
        return TRUE;

    return FALSE;
}

/**
 * midori_view_save_source:
 * @view: a #MidoriView
 * @uri: an alternative destination URI, or %NULL
 * @outfile: a destination filename, or %NULL
 *
 * Saves the data in the view to disk.
 *
 * Return value: the destination filename
 *
 * Since: 0.4.4
 **/
gchar*
midori_view_save_source (MidoriView* view,
                         const gchar* uri,
                         const gchar* outfile)
{
    WebKitWebFrame *frame;
    WebKitWebDataSource *data_source;
    const GString *data;
    gchar* unique_filename;
    gint fd;
    FILE* fp;
    size_t ret;

    frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    data_source = webkit_web_frame_get_data_source (frame);
    data = webkit_web_data_source_get_data (data_source);

    if (uri == NULL)
        uri = midori_view_get_display_uri (view);

    if (!outfile)
    {
        gchar* extension = midori_download_get_extension_for_uri (uri, NULL);
        unique_filename = g_strdup_printf ("%s/%uXXXXXX%s", midori_paths_get_tmp_dir (),
            g_str_hash (uri), midori_download_fallback_extension (extension, view->mime_type));
        g_free (extension);
        katze_mkdir_with_parents (midori_paths_get_tmp_dir (), 0700);
        fd = g_mkstemp (unique_filename);
    }
    else
    {
        unique_filename = g_strdup (outfile);
        fd = g_open (unique_filename, O_WRONLY|O_CREAT, 0644);
    }

    if (fd != -1)
    {
        if ((fp = fdopen (fd, "w")))
        {
            ret = fwrite (data ? data->str : "", 1, data ? data->len : 0, fp);
            fclose (fp);
            if (ret - (data ? data->len : 0) != 0)
            {
                midori_view_add_info_bar (view, GTK_MESSAGE_ERROR,
                    unique_filename, NULL, view,
                    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
                katze_assign (unique_filename, NULL);
            }
        }
        close (fd);
    }
    return unique_filename;
}

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
    g_return_if_fail (MIDORI_IS_VIEW (view));

    if (midori_uri_is_blank (view->uri))
    {
        gchar* uri = g_strdup (view->uri);
        midori_view_set_uri (view, uri);
        g_free (uri);
    }
    else if (from_cache)
        webkit_web_view_reload (WEBKIT_WEB_VIEW (view->web_view));
    else
        webkit_web_view_reload_bypass_cache (WEBKIT_WEB_VIEW (view->web_view));
    katze_item_set_meta_integer (view->item, "delay", -1);
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
    /* Force the speed dial to kick in if going back to a blank page */
    if (midori_view_is_blank (view))
        midori_view_set_uri (view, "");
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
 * midori_view_go_back_or_forward
 * @view: a #MidoriView
 * @steps: number of steps to jump in history
 *
 * Goes back or forward in history.
 *
 * Since: 0.4.5
 **/
void
midori_view_go_back_or_forward (MidoriView* view,
                                gint        steps)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_go_back_or_forward (WEBKIT_WEB_VIEW (view->web_view), steps);
    /* Force the speed dial to kick in if going back to a blank page */
    if (midori_view_is_blank (view))
        midori_view_set_uri (view, "");
}

/**
 * midori_view_can_go_back_or_forward
 * @view: a #MidoriView
 * @steps: number of steps to jump in history
 *
 * Determines whether the view can go back or forward by number of steps.
 *
 * Since: 0.4.5
 **/
gboolean
midori_view_can_go_back_or_forward (MidoriView* view,
                                    gint        steps)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_go_back_or_forward (WEBKIT_WEB_VIEW (view->web_view), steps);
    else
        return FALSE;
}

static gchar*
midori_view_get_related_page (MidoriView*  view,
                              const gchar* rel,
                              const gchar* local)
{
    gchar* script;
    static gchar* uri = NULL;
    WebKitWebFrame* web_frame;
    JSContextRef js_context;

    if (!view->web_view)
        return NULL;

    web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    js_context = webkit_web_frame_get_global_context (web_frame);
    script = g_strdup_printf (
        "(function (tags) {"
        "for (var tag in tags) {"
        "var l = document.getElementsByTagName (tag);"
        "for (var i in l) { "
        "if ((l[i].rel && l[i].rel.toLowerCase () == '%s') "
        " || (l[i].innerHTML"
        "  && (l[i].innerHTML.toLowerCase ().indexOf ('%s') != -1 "
        "   || l[i].innerHTML.toLowerCase ().indexOf ('%s') != -1)))"
        "{ return l[i].href; } } } return 0; })("
        "{ link:'link', a:'a' });", rel, rel, local);
    katze_assign (uri, sokoke_js_script_eval (js_context, script, NULL));
    g_free (script);
    return uri && uri[0] != '0' ? uri : NULL;
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
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    /* i18n: word stem of "previous page" type links, case is not important */
    return midori_view_get_related_page (view, "prev", _("previous"));
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
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    /* i18n: word stem of "next page" type links, case is not important */
    return midori_view_get_related_page (view, "next", _("next"));
}

static GtkWidget*
midori_view_print_create_custom_widget_cb (GtkPrintOperation* operation,
                                           MidoriView*        view)
{
    GtkWidget* box;
    GtkWidget* button;

    box = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 4);
    button = katze_property_proxy (view->settings, "print-backgrounds", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Print background images"));
    gtk_widget_set_tooltip_text (button, _("Whether background images should be printed"));
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    gtk_widget_show_all (box);

    return box;
}

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
    GtkPrintOperation* operation;
    GError* error;

    g_return_if_fail (MIDORI_IS_VIEW (view));

    frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    operation = gtk_print_operation_new ();
    gtk_print_operation_set_custom_tab_label (operation, _("Features"));
    #if GTK_CHECK_VERSION (2, 18, 0)
    gtk_print_operation_set_embed_page_setup (operation, TRUE);
    #endif
    g_signal_connect (operation, "create-custom-widget",
        G_CALLBACK (midori_view_print_create_custom_widget_cb), view);
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

    #if GTK_CHECK_VERSION (3, 2, 0)
    if (gtk_widget_get_visible (view->overlay_find))
    {
        text = midori_findbar_get_text (MIDORI_FINDBAR (view->overlay_find));
        webkit_web_view_search_text (WEBKIT_WEB_VIEW (view->web_view),
            text, case_sensitive, forward, TRUE);
        return;
    }
    #endif
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
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);
    return midori_view_web_view_get_snapshot ((GtkWidget*)view->web_view, width, height);
}

static GdkPixbuf*
midori_view_web_view_get_snapshot (GtkWidget* web_view,
                                   gint       width,
                                   gint       height)
{
    GtkAllocation allocation;
    gboolean fast;
    gint x, y, w, h;
    GdkRectangle rect;
    #if !GTK_CHECK_VERSION (3, 0, 0)
    GdkWindow* window;
    GdkPixmap* pixmap;
    GdkEvent event;
    gboolean result;
    GdkColormap* colormap;
    #else
    cairo_surface_t* surface;
    cairo_t* cr;
    #endif
    GdkPixbuf* pixbuf;

    g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), NULL);

    gtk_widget_get_allocation (web_view, &allocation);
    x = allocation.x;
    y = allocation.y;
    w = allocation.width;
    h = allocation.height;

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

    #if GTK_CHECK_VERSION (3, 0, 0)
    surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                          allocation.width, allocation.height);
    cr = cairo_create (surface);
    cairo_rectangle (cr, x, y, width, height);
    cairo_clip (cr);
    gtk_widget_draw (web_view, cr);
    pixbuf = gdk_pixbuf_get_from_surface (surface, x, y, width, height);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);
    #else
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;

    window = gtk_widget_get_window (web_view);
    g_return_val_if_fail (window != NULL, NULL);

    pixmap = gdk_pixmap_new (window, w, h, gdk_drawable_get_depth (window));
    event.expose.type = GDK_EXPOSE;
    event.expose.window = pixmap;
    event.expose.send_event = FALSE;
    event.expose.count = 0;
    event.expose.area.x = 0;
    event.expose.area.y = 0;
    gdk_drawable_get_size (GDK_DRAWABLE (window),
        &event.expose.area.width, &event.expose.area.height);
    event.expose.region = gdk_region_rectangle (&event.expose.area);

    g_signal_emit_by_name (web_view, "expose-event", &event, &result);

    colormap = gdk_drawable_get_colormap (pixmap);
    pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap, 0, 0,
                                           0, 0, rect.width, rect.height);
    g_object_unref (pixmap);
    #endif

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
 * midori_view_get_for_widget:
 * @widget: a #GtkWidget
 *
 * Determines the view appropriate for the specified widget.
 *
 * Return value: a #MidoriView
 *
 * Since 0.4.5
 **/
MidoriView*
midori_view_get_for_widget (GtkWidget* web_view)
{
    GtkWidget* scrolled = gtk_widget_get_parent (web_view);
    #if GTK_CHECK_VERSION(3, 2, 0)
    GtkWidget* overlay = gtk_widget_get_parent (scrolled);
    GtkWidget* view = gtk_widget_get_parent (overlay);
    #else
    GtkWidget* view = gtk_widget_get_parent (scrolled);
    #endif
    return MIDORI_VIEW (view);
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

