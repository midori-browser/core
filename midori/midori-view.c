/*
 Copyright (C) 2007-2013 Christian Dywan <christian@twotoasts.de>
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

#ifdef HAVE_GCR
    #define GCR_API_SUBJECT_TO_CHANGE
    #include <gcr/gcr.h>
#endif

#if !defined (HAVE_WEBKIT2)
SoupMessage*
midori_map_get_message (SoupMessage* message);
#endif

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include "katze/katze.h"

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef G_OS_WIN32
    #include <sys/utsname.h>
#endif

static void
midori_view_item_meta_data_changed (KatzeItem*   item,
                                    const gchar* key,
                                    MidoriView*  view);

static void
_midori_view_set_settings (MidoriView*        view,
                           MidoriWebSettings* settings);

#ifdef HAVE_WEBKIT2
static void
midori_view_uri_scheme_res (WebKitURISchemeRequest* request,
                            gpointer                user_data);

static void
midori_view_download_started_cb (WebKitWebContext* context,
                                   WebKitDownload*   download,
                                   MidoriView * view);
static gboolean
midori_view_download_query_action (MidoriView*     view,
                                   WebKitDownload*   download,
                                   const gchar * suggested_filename );
#endif

static gboolean
midori_view_display_error (MidoriView*     view,
                           const gchar*    uri,
                           const gchar*    error_icon,
                           const gchar*    title,
                           const gchar*    message,
                           const gchar*    description,
                           const gchar*    suggestions,
                           const gchar*    try_again,
#ifndef HAVE_WEBKIT2
                           WebKitWebFrame* web_frame);
#else
                           void*           web_frame);
#endif

struct _MidoriView
{
    MidoriTab parent_instance;

    gchar* title;
    GdkPixbuf* icon;
    gchar* icon_uri;
    gboolean minimized;
    WebKitHitTestResult* hit_test;
    gchar* link_uri;
    gboolean button_press_handled;
    gboolean has_selection;
    gchar* selected_text;
    MidoriWebSettings* settings;
    GtkWidget* web_view;
    KatzeArray* news_feeds;

    gboolean open_tabs_in_the_background;
    MidoriNewPage open_new_pages_in;
    gint find_links;
    gint alerts;

    GtkWidget* tab_label;
    GtkWidget* menu_item;
    PangoEllipsizeMode ellipsize;
    KatzeItem* item;
    gint scrollh, scrollv;
    GtkWidget* scrolled_window;

    #if GTK_CHECK_VERSION (3, 2, 0)
    GtkWidget* overlay;
    GtkWidget* overlay_label;
    GtkWidget* overlay_find;
    #endif
};

struct _MidoriViewClass
{
    MidoriTabClass parent_class;
};

G_DEFINE_TYPE (MidoriView, midori_view, MIDORI_TYPE_TAB);

enum
{
    PROP_0,

    PROP_TITLE,
    PROP_ICON,
    PROP_MINIMIZED,
    PROP_ZOOM_LEVEL,
    PROP_NEWS_FEEDS,
    PROP_SETTINGS
};

enum {
    NEW_TAB,
    NEW_WINDOW,
    NEW_VIEW,
    DOWNLOAD_REQUESTED,
    ADD_BOOKMARK,
    ABOUT_CONTENT,

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

static GObject*
midori_view_constructor (GType                  type,
                         guint                  n_construct_properties,
                         GObjectConstructParam* construct_properties);

static void
midori_view_class_init (MidoriViewClass* class)
{
    GObjectClass* gobject_class;
    GtkWidgetClass* gtkwidget_class;
    GParamFlags flags;

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

    /**
     * MidoriView::about-content:
     * @view: the object on which the signal is emitted
     * @uri: the about URI
     *
     * Emitted when loading the about content
     *
     * Return value: the view content as string
     *
     * Since: 0.5.5
     */
    signals[ABOUT_CONTENT] = g_signal_new (
        "about-content",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        g_signal_accumulator_true_handled,
        NULL,
        midori_cclosure_marshal_BOOLEAN__STRING,
        G_TYPE_BOOLEAN, 1,
        G_TYPE_STRING);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->constructor = midori_view_constructor;
    gobject_class->finalize = midori_view_finalize;
    gobject_class->set_property = midori_view_set_property;
    gobject_class->get_property = midori_view_get_property;

    gtkwidget_class = GTK_WIDGET_CLASS (class);
    gtkwidget_class->focus_in_event = midori_view_focus_in_event;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS;

    g_object_class_install_property (gobject_class,
                                     PROP_TITLE,
                                     g_param_spec_string (
                                     "title",
                                     "Title",
                                     "The title of the currently loaded page",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON,
                                     g_param_spec_object (
                                     "icon",
                                     "Icon",
                                     "The icon of the view",
                                     GDK_TYPE_PIXBUF,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

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
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "The associated settings",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
midori_view_set_title (MidoriView* view, const gchar* title)
{
    const gchar* uri = midori_tab_get_uri (MIDORI_TAB (view));
    katze_assign (view->title, g_strdup (midori_tab_get_display_title (title, uri)));
    view->ellipsize = midori_tab_get_display_ellipsize (view->title, uri);
    if (view->menu_item)
        gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (
                            view->menu_item))), view->title);
    katze_item_set_name (view->item, view->title);
}

static void
midori_view_apply_icon (MidoriView*  view,
                        GdkPixbuf*   icon,
                        const gchar* icon_name)
{
    katze_item_set_icon (view->item, icon_name);
    /* katze_item_get_image knows about this pixbuf */
    if (icon != NULL)
        g_object_ref (icon);
    g_object_set_data_full (G_OBJECT (view->item), "pixbuf", icon,
                            (GDestroyNotify)g_object_unref);
    katze_object_assign (view->icon, icon);
    g_object_notify (G_OBJECT (view), "icon");

    if (view->menu_item)
    {
        GtkWidget* image = katze_item_get_image (view->item, view->web_view);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (view->menu_item), image);
    }
}

static void
midori_view_unset_icon (MidoriView* view)
{
    GdkScreen* screen;
    GtkIconTheme* icon_theme;
    gchar* content_type;
    GIcon* icon;
    GtkIconInfo* icon_info;
    GdkPixbuf* pixbuf = NULL;

    content_type = g_content_type_from_mime_type (
        midori_tab_get_mime_type (MIDORI_TAB (view)));
    icon = g_content_type_get_icon (content_type);
    g_free (content_type);
    g_themed_icon_append_name (G_THEMED_ICON (icon), "text-html");

    if ((screen = gtk_widget_get_screen (view->web_view))
        && (icon_theme = gtk_icon_theme_get_for_screen (screen)))
    {
        if ((icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme, icon, 16, 0)))
            pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
    }

    midori_view_apply_icon (view, pixbuf, NULL);
    g_object_unref (icon);
}

static void
_midori_web_view_load_icon (MidoriView* view)
{
    gint icon_width = 16, icon_height = 16;
    GtkSettings* settings = gtk_widget_get_settings (view->web_view);
    gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
    GdkPixbuf* pixbuf = NULL;
    #ifdef HAVE_WEBKIT2
    cairo_surface_t* surface = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (view->web_view));
    if (surface != NULL
     && (pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
        cairo_image_surface_get_width (surface),
        cairo_image_surface_get_height (surface))))
    {
        GdkPixbuf* pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf,
            icon_width, icon_height, GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        midori_view_apply_icon (view, pixbuf_scaled, view->icon_uri);
    }
    #else
    if ((pixbuf = webkit_web_view_try_get_favicon_pixbuf (
        WEBKIT_WEB_VIEW (view->web_view), icon_width, icon_height)))
        midori_view_apply_icon (view, pixbuf, view->icon_uri);
    #endif
}

static void
midori_view_update_load_status (MidoriView*      view,
                                MidoriLoadStatus load_status)
{
    if (midori_tab_get_load_status (MIDORI_TAB (view)) != load_status)
        midori_tab_set_load_status (MIDORI_TAB (view), load_status);
}

/**
 * midori_view_get_tls_info
 * @view: a #MidoriView
 * @request: a #WebKitNetworkRequest with WebKit1, otherwise %NULL
 * @tls_cert: variable to store the certificate
 * @tls_flags: variable to store the flags
 * @hostname: variable to store the hostname
 *
 * Returns %TRUE if the the host is secure and trustworthy.
 **/
gboolean
midori_view_get_tls_info (MidoriView*           view,
                          void*                 request,
                          GTlsCertificate**     tls_cert,
                          GTlsCertificateFlags* tls_flags,
                          gchar**               hostname)
{
    #ifdef HAVE_WEBKIT2
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    *hostname = midori_uri_parse_hostname (webkit_web_view_get_uri (web_view), NULL);
    gboolean success = webkit_web_view_get_tls_info (web_view, tls_cert, tls_flags);
    if (*tls_cert != NULL)
        g_object_ref (*tls_cert);
    return success && *tls_flags == 0;
    #else
    SoupMessage* message = midori_map_get_message (webkit_network_request_get_message (request));
    if (message != NULL)
    {
        SoupURI* uri = soup_message_get_uri (message);
        *hostname = uri ? g_strdup (uri->host) : NULL;
        g_object_get (message, "tls-certificate", tls_cert, "tls-errors", tls_flags, NULL);
        if (soup_message_get_flags (message) & SOUP_MESSAGE_CERTIFICATE_TRUSTED)
            return TRUE;
        return *tls_flags == 0;
    }
    *tls_cert = NULL;
    *tls_flags = 0;
    *hostname = NULL;
    return FALSE;
    #endif
}

static gboolean
midori_view_web_view_navigation_decision_cb (WebKitWebView*             web_view,
                                             #ifdef HAVE_WEBKIT2
                                             WebKitPolicyDecision*      decision,
                                             WebKitPolicyDecisionType   decision_type,
                                             #else
                                             WebKitWebFrame*            web_frame,
                                             WebKitNetworkRequest*      request,
                                             WebKitWebNavigationAction* action,
                                             WebKitWebPolicyDecision*   decision,
                                             #endif
                                             MidoriView*                view)
{
    #ifdef HAVE_WEBKIT2
    if (decision_type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
    {
        WebKitURIResponse* response = webkit_response_policy_decision_get_response (
            WEBKIT_RESPONSE_POLICY_DECISION (decision));
        const gchar* mime_type = webkit_uri_response_get_mime_type (response);
        midori_tab_set_mime_type (MIDORI_TAB (view), mime_type);
        katze_item_set_meta_string (view->item, "mime-type", mime_type);
        if (!webkit_web_view_can_show_mime_type (web_view, mime_type))
        {
            webkit_policy_decision_download (decision);
            return TRUE;
        }
        webkit_policy_decision_use (decision);
        return TRUE;
    }
    else if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    {
        const gchar* uri = webkit_uri_request_get_uri (
            webkit_navigation_policy_decision_get_request (WEBKIT_NAVIGATION_POLICY_DECISION (decision)));
        g_signal_emit (view, signals[NEW_TAB], 0, uri, FALSE);
        webkit_policy_decision_ignore(decision);
        return FALSE;
    }
    else if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    {
    }
    else
    {
        g_debug ("Unhandled policy decision type %d", decision_type);
        return FALSE;
    }

    WebKitURIRequest * request = webkit_navigation_policy_decision_get_request (WEBKIT_NAVIGATION_POLICY_DECISION (decision));
    const gchar* uri = webkit_uri_request_get_uri (request);
    #else
    const gchar* uri = webkit_network_request_get_uri (request);
    #endif
    if (g_str_has_prefix (uri, "geo:") && strstr (uri, ","))
    {
        gchar* new_uri = sokoke_magic_uri (uri, TRUE, FALSE);
        midori_view_set_uri (view, new_uri);
        g_free (new_uri);
        return TRUE;
    }
    else if (g_str_has_prefix (uri, "data:image/"))
    {
        /* For security reasons, main content served as data: is limited to images
           http://lcamtuf.coredump.cx/switch/ */
        #ifdef HAVE_WEBKIT2
        webkit_policy_decision_ignore (decision);
        #else
        webkit_web_policy_decision_ignore (decision);
        #endif
        return TRUE;
    }
    #ifdef HAVE_GCR
    else if (/* midori_tab_get_special (MIDORI_TAB (view)) && */ !strncmp (uri, "https", 5))
    {
        /* We show an error page if the certificate is invalid.
           If a "special", unverified page loads a form, it must be that page.
           if (webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED)
           FIXME: Verify more stricly that this cannot be eg. a simple Reload */
        #ifdef HAVE_WEBKIT2
        if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        #else
        if (webkit_web_navigation_action_get_reason (action) == WEBKIT_WEB_NAVIGATION_REASON_RELOAD)
        #endif
        {
            GTlsCertificate* tls_cert;
            GTlsCertificateFlags tls_flags;
            gchar* hostname;
            if (!midori_view_get_tls_info (view, request, &tls_cert, &tls_flags, &hostname)
             && tls_cert != NULL)
            {
                GcrCertificate* gcr_cert;
                GByteArray* der_cert;

                g_object_get (tls_cert, "certificate", &der_cert, NULL);
                gcr_cert = gcr_simple_certificate_new (der_cert->data, der_cert->len);
                g_byte_array_unref (der_cert);
                if (hostname && !gcr_trust_is_certificate_pinned (gcr_cert, GCR_PURPOSE_SERVER_AUTH, hostname, NULL, NULL))
                {
                    GError* error = NULL;
                    gcr_trust_add_pinned_certificate (gcr_cert, GCR_PURPOSE_SERVER_AUTH, hostname, NULL, &error);
                    if (error != NULL)
                    {
                        gchar* slots = g_strjoinv (" , ", (gchar**)gcr_pkcs11_get_trust_lookup_uris ());
                        gchar* title = g_strdup_printf ("Error granting trust: %s", error->message);
                        midori_tab_stop_loading (MIDORI_TAB (view));
                        midori_view_display_error (view, NULL, NULL, NULL, title, slots, NULL,
                            _("Trust this website"), NULL);
                        g_free (title);
                        g_free (slots);
                        g_error_free (error);
                    }
                }
                g_object_unref (gcr_cert);
            }
            if (tls_cert != NULL)
                g_object_unref (tls_cert);
            g_free (hostname);
        }
    }
    #endif

    if (katze_item_get_meta_integer (view->item, "delay") == MIDORI_DELAY_PENDING_UNDELAY)
    {
        midori_tab_set_special (MIDORI_TAB (view), FALSE);
        katze_item_set_meta_integer (view->item, "delay", MIDORI_DELAY_UNDELAYED);
    }

    #ifndef HAVE_WEBKIT2
    /* Remove link labels */
    JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
    gchar* result = sokoke_js_script_eval (js_context,
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
    #endif

    gboolean handled = FALSE;
    g_signal_emit_by_name (view, "navigation-requested", uri, &handled);
    if (handled)
    {
        #ifdef HAVE_WEBKIT2
        webkit_policy_decision_ignore (decision);
        #else
        webkit_web_policy_decision_ignore (decision);
        #endif
        return TRUE;
    }

    return FALSE;
}

static void
midori_view_load_started (MidoriView* view)
{
    midori_view_update_load_status (view, MIDORI_LOAD_PROVISIONAL);
    midori_tab_set_progress (MIDORI_TAB (view), 0.0);
    midori_tab_set_load_error (MIDORI_TAB (view), MIDORI_LOAD_ERROR_NONE);
}

#ifdef HAVE_GCR
const gchar*
midori_location_action_tls_flags_to_string (GTlsCertificateFlags flags);
#endif

static void
midori_view_load_committed (MidoriView* view)
{
    katze_assign (view->icon_uri, NULL);

    GList* children = gtk_container_get_children (GTK_CONTAINER (view));
    for (; children; children = g_list_next (children))
        if (g_object_get_data (G_OBJECT (children->data), "midori-infobar-cb"))
            gtk_widget_destroy (children->data);
    g_list_free (children);
    view->alerts = 0;

    const gchar* uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW  (view->web_view));
    if (g_strcmp0 (uri, katze_item_get_uri (view->item)))
    {
        midori_tab_set_uri (MIDORI_TAB (view), uri);
        katze_item_set_uri (view->item, uri);
        midori_tab_set_special (MIDORI_TAB (view), FALSE);
    }

    katze_item_set_added (view->item, time (NULL));
    g_object_set (view, "title", NULL, NULL);
    midori_view_unset_icon (view);

    if (!strncmp (uri, "https", 5))
    {
        #ifdef HAVE_WEBKIT2
        void* request = NULL;
        #else
        WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
        WebKitWebDataSource* source = webkit_web_frame_get_data_source (web_frame);
        WebKitNetworkRequest* request = webkit_web_data_source_get_request (source);
        #endif
        GTlsCertificate* tls_cert;
        GTlsCertificateFlags tls_flags;
        gchar* hostname;
        if (midori_view_get_tls_info (view, request, &tls_cert, &tls_flags, &hostname))
            midori_tab_set_security (MIDORI_TAB (view), MIDORI_SECURITY_TRUSTED);
        #ifdef HAVE_GCR
        else if (!midori_tab_get_special (MIDORI_TAB (view)) && tls_cert != NULL)
        {
            GcrCertificate* gcr_cert;
            GByteArray* der_cert;

            g_object_get (tls_cert, "certificate", &der_cert, NULL);
            gcr_cert = gcr_simple_certificate_new (der_cert->data, der_cert->len);
            g_byte_array_unref (der_cert);
            if (gcr_trust_is_certificate_pinned (gcr_cert, GCR_PURPOSE_SERVER_AUTH, hostname, NULL, NULL))
                midori_tab_set_security (MIDORI_TAB (view), MIDORI_SECURITY_TRUSTED);
            else
            {
                midori_tab_set_security (MIDORI_TAB (view), MIDORI_SECURITY_UNKNOWN);
                midori_tab_stop_loading (MIDORI_TAB (view));
                midori_view_display_error (view, NULL, NULL, NULL, _("Security unknown"),
                    midori_location_action_tls_flags_to_string (tls_flags), NULL,
                    _("Trust this website"),
                    NULL);
            }
            g_object_unref (gcr_cert);
        }
        #endif
        else
            midori_tab_set_security (MIDORI_TAB (view), MIDORI_SECURITY_UNKNOWN);
        #ifdef HAVE_GCR
        if (tls_cert != NULL)
            g_object_unref (tls_cert);
        g_free (hostname);
        #endif
    }
    else
        midori_tab_set_security (MIDORI_TAB (view), MIDORI_SECURITY_NONE);

    view->find_links = -1;
    
    midori_view_update_load_status (view, MIDORI_LOAD_COMMITTED);

}

static void
webkit_web_view_progress_changed_cb (WebKitWebView* web_view,
                                     GParamSpec*    pspec,
                                     MidoriView*    view)
{
    gdouble progress = 1.0;
    g_object_get (web_view, pspec->name, &progress, NULL);
    midori_tab_set_progress (MIDORI_TAB (view), progress);
}

#ifdef HAVE_WEBKIT2
static void
midori_view_uri_scheme_res (WebKitURISchemeRequest* request,
                            gpointer                user_data)
{
    const gchar* uri = webkit_uri_scheme_request_get_uri (request);
    WebKitWebView* web_view = webkit_uri_scheme_request_get_web_view (request);
    MidoriView* view = midori_view_get_for_widget (GTK_WIDGET (web_view));
#else
static void
midori_view_web_view_resource_request_cb (WebKitWebView*         web_view,
                                          WebKitWebFrame*        web_frame,
                                          WebKitWebResource*     web_resource,
                                          WebKitNetworkRequest*  request,
                                          WebKitNetworkResponse* response,
                                          MidoriView*            view)
{
    const gchar* uri = webkit_network_request_get_uri (request);
#endif

    /* Only apply custom URIs to special pages for security purposes */
    if (!midori_tab_get_special (MIDORI_TAB (view)))
        return;

    if (g_str_has_prefix (uri, "res://"))
    {
        gchar* filepath = midori_paths_get_res_filename (&uri[6]);
        #ifdef HAVE_WEBKIT2
        gchar* contents;
        gsize length;
        if (g_file_get_contents (filepath, &contents, &length, NULL))
        {
            gchar* content_type = g_content_type_guess (filepath, (guchar*)contents, length, NULL);
            gchar* mime_type = g_content_type_get_mime_type (content_type);
            GInputStream* stream = g_memory_input_stream_new_from_data (contents, -1, g_free);
            webkit_uri_scheme_request_finish (request, stream, -1, mime_type);
            g_object_unref (stream);
            g_free (mime_type);
            g_free (content_type);
        }
        #else
        gchar* file_uri = g_filename_to_uri (filepath, NULL, NULL);
        webkit_network_request_set_uri (request, file_uri);
        g_free (file_uri);
        #endif
        g_free (filepath);
    }
    else if (g_str_has_prefix (uri, "stock://"))
    {
        GdkPixbuf* pixbuf;
        const gchar* icon_name = &uri[8] ? &uri[8] : "";
        gint icon_size = GTK_ICON_SIZE_MENU;
        static gint icon_size_large_dialog = 0;

        if (!icon_size_large_dialog)
        {
            gint width = 48, height = 48;
            gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &width, &height);
            icon_size_large_dialog = gtk_icon_size_register ("large-dialog", width * 2, height * 2);
        }

        if (g_ascii_isalpha (icon_name[0]))
        {
            if (g_str_has_prefix (icon_name, "dialog/"))
            {
                icon_name = &icon_name [strlen("dialog/")];
                icon_size = icon_size_large_dialog;
            }
            else
                icon_size = GTK_ICON_SIZE_BUTTON;
        }
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
            data_uri = g_strconcat ("data:image/png;base64,", encoded, NULL);
            g_free (encoded);
            #ifdef HAVE_WEBKIT2
            GInputStream* stream = g_memory_input_stream_new_from_data (buffer, buffer_size, g_free);
            webkit_uri_scheme_request_finish (request, stream, -1, "image/png");
            g_object_unref (stream);
            #else
            g_free (buffer);
            webkit_network_request_set_uri (request, data_uri);
            #endif
            g_free (data_uri);
            return;
        }
    }
}

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

/**
 * midori_view_add_info_bar
 * @view: a #MidoriView
 * @message_type: a #GtkMessageType
 * @message: a message string
 * @response_cb: (scope async): a response callback
 * @user_data: user data passed to the callback
 * @first_button_text: button text or stock ID
 * @...: first response ID, then more text - response ID pairs
 *
 * Adds an infobar (or equivalent) to the view. Activation of a
 * button invokes the specified callback. The infobar is
 * automatically destroyed if the location changes or reloads.
 *
 * Return value: (transfer none): an infobar widget
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

    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);
    g_return_val_if_fail (message != NULL, NULL);

    va_start (args, first_button_text);

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

    va_end (args);
    label = gtk_label_new (message);
    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_container_add (GTK_CONTAINER (content_area), label);
    gtk_widget_show_all (infobar);
    gtk_box_pack_start (GTK_BOX (view), infobar, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (view), infobar, 0);
    g_object_set_data (G_OBJECT (infobar), "midori-infobar-cb", response_cb);
    if (data_object != NULL)
        g_object_set_data_full (G_OBJECT (infobar), "midori-infobar-da",
            g_object_ref (data_object), g_object_unref);
    return infobar;
}

#ifdef HAVE_WEBKIT2
static gboolean
midori_view_web_view_permission_request_cb (WebKitWebView*           web_view,
                                            WebKitPermissionRequest* decision,
                                            MidoriView*              view)
{
    /* if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST (decision))
    {
        TODO: return TRUE;
    } */
    return FALSE;
}
#else
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

static void
midori_view_location_response_cb (GtkWidget*                       infobar,
                                  gint                             response,
                                  WebKitGeolocationPolicyDecision* decision)
{
    if (response == GTK_RESPONSE_ACCEPT)
        webkit_geolocation_policy_allow (decision);
    else
        webkit_geolocation_policy_deny (decision);
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
    midori_view_add_info_bar (view, GTK_MESSAGE_QUESTION,
        message, G_CALLBACK (midori_view_location_response_cb), decision,
        _("_Deny"), GTK_RESPONSE_REJECT, _("_Allow"), GTK_RESPONSE_ACCEPT,
        NULL);
    g_free (hostname);
    g_free (message);
    return TRUE;
}
#endif

void
midori_view_set_html (MidoriView*     view,
                      const gchar*    data,
                      const gchar*    uri,
                      void*           web_frame)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));
    g_return_if_fail (data != NULL);

    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    if (!uri)
        uri = "about:blank";
#ifndef HAVE_WEBKIT2
    WebKitWebFrame* main_frame = webkit_web_view_get_main_frame (web_view);
    if (!web_frame)
        web_frame = main_frame;
    if (web_frame == main_frame)
    {
        katze_item_set_uri (view->item, uri);
        midori_tab_set_special (MIDORI_TAB (view), TRUE);
    }
    webkit_web_frame_load_alternate_string (
        web_frame, data, uri, uri);
#else
    /* XXX: with webkit2 ensure child frames do not set tab URI/special/html */
    katze_item_set_uri (view->item, uri);
    midori_tab_set_special (MIDORI_TAB (view), TRUE);
    webkit_web_view_load_alternate_html (web_view, data, uri, uri);
#endif
}

static gboolean
midori_view_display_error (MidoriView*     view,
                           const gchar*    uri,
                           const gchar*    error_icon,
                           const gchar*    title,
                           const gchar*    message,
                           const gchar*    description,
                           const gchar*    suggestions,
                           const gchar*    try_again,
#ifndef HAVE_WEBKIT2
                           WebKitWebFrame* web_frame)
#else
                           void*           web_frame)
#endif
{
    gchar* path = midori_paths_get_res_filename ("error.html");
    gchar* template;

    if (g_file_get_contents (path, &template, NULL, NULL))
    {
        gchar* title_escaped;
        const gchar* icon;
        gchar* favicon;
        gchar* result;
        gboolean is_main_frame;

        #ifdef HAVE_WEBKIT2
        is_main_frame = TRUE;
        #else
        is_main_frame = web_frame && (webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view)) == web_frame);
        #endif

        #if !GTK_CHECK_VERSION (3, 0, 0)
        /* g_object_get_valist: object class `GtkSettings' has no property named `gtk-button-images' */
        g_type_class_unref (g_type_class_ref (GTK_TYPE_BUTTON));
        #endif

        GtkSettings* gtk_settings = gtk_widget_get_settings (view->web_view);
        gboolean show_button_images = gtk_settings != NULL
          && katze_object_get_boolean (gtk_settings, "gtk-button-images");
        if (uri == NULL)
            uri = midori_tab_get_uri (MIDORI_TAB (view));
        title_escaped = g_markup_escape_text (title ? title : view->title, -1);
        icon = katze_item_get_icon (view->item);
        favicon = icon && !g_str_has_prefix (icon, "stock://")
          ? g_strdup_printf ("<link rel=\"shortcut icon\" href=\"%s\" />", icon) : NULL;
        result = sokoke_replace_variables (template,
            "{dir}", gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL ?
                "rtl" : "ltr",
            "{title}", title_escaped,
            "{favicon}", katze_str_non_null (favicon),
            "{error_icon}", katze_str_non_null (error_icon),
            "{message}", message,
            "{description}", description,
            "{suggestions}", katze_str_non_null (suggestions),
            "{tryagain}", try_again,
            "{uri}", uri,
            "{hide-button-images}", show_button_images ? "" : "display:none",
            "{autofocus}", is_main_frame ? "autofocus=\"true\" " : "",
            NULL);
        g_free (favicon);
        g_free (title_escaped);
        g_free (template);

        midori_view_set_html (view, result, uri, web_frame);

        g_free (result);
        g_free (path);

        return TRUE;
    }
    g_free (path);

    return FALSE;
}

static gboolean
webkit_web_view_load_error_cb (WebKitWebView*  web_view,
#ifdef HAVE_WEBKIT2
                               WebKitLoadEvent load_event,
#else
                               WebKitWebFrame* web_frame,
#endif
                               const gchar*    uri,
                               GError*         error,
                               MidoriView*     view)
{
    /*in WebKit2's UIProcess/API/gtk/WebKitLoaderClient.cpp,
    didFailProvisionalLoadWithErrorForFrame early-returns if the frame isn't
    main, so we know that the pertinent frame here is the view's main frame--so
    it's safe for midori_view_display_error to assume it fills in a main frame*/
    #ifdef HAVE_WEBKIT2
    void* web_frame = NULL;
    void* main_frame = NULL;
    #else
    WebKitWebFrame* main_frame = webkit_web_view_get_main_frame (web_view);
    #endif
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

    if (!g_network_monitor_get_network_available (g_network_monitor_get_default ()))
    {
        title = g_strdup_printf (_("You are not connected to a network"));
        message = g_strdup_printf (_("Your computer must be connected to a network to reach “%s”. "
                                     "Connect to a wireless access point or attach a network cable and try again."), 
                                     midori_uri_parse_hostname(uri, NULL));
    } 
    else if (!g_network_monitor_can_reach (g_network_monitor_get_default (), 
                                           g_network_address_parse_uri ("http://midori-browser.org/", 80, NULL), 
                                           NULL, 
                                           NULL))
    {
        title = g_strdup_printf (_("You are not connected to the Internet"));
        message = g_strdup_printf (_("Your computer appears to be connected to a network, but can't reach “%s”. "
                                     "Check your network settings and try again."), 
                                     midori_uri_parse_hostname(uri, NULL));
    } 
    else
    {
        title = g_strdup_printf (_("Midori can't find the page you're looking for"));
        message = g_strdup_printf (_("The page located at “%s” cannot be found. "
                                     "Check the web address for misspelled words and try again."), 
                                     midori_uri_parse_hostname(uri, NULL));
    }

    result = midori_view_display_error (view, uri, "stock://dialog/network-error", title,
                                        message, error->message, NULL,
                                        _("Try Again"), web_frame);

    /* if the main frame for the whole tab has a network error, set tab error status */
    if (web_frame == main_frame)
        midori_tab_set_load_error (MIDORI_TAB (view), MIDORI_LOAD_ERROR_NETWORK);

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
midori_view_load_finished (MidoriView* view)
{
    midori_view_apply_scroll_position (view);
    #ifndef HAVE_WEBKIT2

    {
        WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
        JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
        /* Icon: URI, News Feed: $URI|title, Search: :URI|title */
        gchar* value = sokoke_js_script_eval (js_context,
        "(function (l) { var f = new Array (); for (var i in l) "
        "{ var t = l[i].type; var r = l[i].rel; "
        "if (t && (t.indexOf ('rss') != -1 || t.indexOf ('atom') != -1)) "
        "f.push ('$' + l[i].href + '|' + l[i].title);"
        "else if (r == 'search' && t == 'application/opensearchdescription+xml') "
        "f.push (':' + l[i].href + '|' + l[i].title); } "
        "return f; })("
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

            news_feeds_continue:
            current_item++;
        }
        g_strfreev (items);

        g_object_set_data_full (G_OBJECT (view), "news-feeds", default_uri, g_free);
        g_free (value);
    }
    #endif

    midori_tab_set_progress (MIDORI_TAB (view), 1.0);
    midori_view_update_load_status (view, MIDORI_LOAD_FINISHED);
}

#ifdef HAVE_WEBKIT2
static void
midori_view_web_view_crashed_cb (WebKitWebView* web_view,
                                 MidoriView*    view)
{
    const gchar* uri = webkit_web_view_get_uri (web_view);
    gchar* title = g_strdup_printf (_("Oops - %s"), uri);
    gchar* message = g_strdup_printf (_("Something went wrong with '%s'."), uri);
    midori_view_display_error (view, uri, NULL, title,
        message, "", NULL, _("Try again"), NULL);
    g_free (message);
    g_free (title);
}

static void
midori_view_web_view_load_changed_cb (WebKitWebView*  web_view,
                                      WebKitLoadEvent load_event,
                                      MidoriView*     view)
{
    g_object_freeze_notify (G_OBJECT (view));

    switch (load_event)
    {
    case WEBKIT_LOAD_STARTED:
        midori_view_load_started (view);
        break;
    case WEBKIT_LOAD_REDIRECTED:
        /* Not implemented */
        break;
    case WEBKIT_LOAD_COMMITTED:
        midori_view_load_committed (view);
        break;
    case WEBKIT_LOAD_FINISHED:
        midori_view_load_finished (view);
        break;
    default:
        g_warn_if_reached ();
    }

    g_object_thaw_notify (G_OBJECT (view));
}
#else
static void
midori_view_web_view_notify_load_status_cb (WebKitWebView* web_view,
                                            GParamSpec*    pspec,
                                            MidoriView*    view)
{
    g_object_freeze_notify (G_OBJECT (view));

    switch (webkit_web_view_get_load_status (web_view))
    {
    case WEBKIT_LOAD_PROVISIONAL:
        midori_view_load_started (view);
        break;
    case WEBKIT_LOAD_COMMITTED:
        midori_view_load_committed (view);
        break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
        /* Not implemented */
        break;
    case WEBKIT_LOAD_FINISHED:
    case WEBKIT_LOAD_FAILED:
        midori_view_load_finished (view);
        break;
    default:
        g_warn_if_reached ();
    }

    g_object_thaw_notify (G_OBJECT (view));
}
#endif

static void
midori_web_view_notify_icon_uri_cb (WebKitWebView* web_view,
                                    GParamSpec*    pspec,
                                    MidoriView*    view)
{
#ifdef HAVE_WEBKIT2
    const gchar* uri = webkit_web_view_get_uri (web_view);
    WebKitWebContext* context = webkit_web_context_get_default ();
    WebKitFaviconDatabase* favicon_database = webkit_web_context_get_favicon_database (context);
    gchar* icon_uri = webkit_favicon_database_get_favicon_uri (favicon_database, uri);
#else
    gchar* icon_uri = g_strdup (webkit_web_view_get_icon_uri (web_view));
#endif
    katze_assign (view->icon_uri, icon_uri);
    _midori_web_view_load_icon (view);
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

#ifndef HAVE_WEBKIT2
static void
webkit_web_view_statusbar_text_changed_cb (WebKitWebView* web_view,
                                           const gchar*   text,
                                           MidoriView*    view)
{
    midori_tab_set_statusbar_text (MIDORI_TAB (view), text);
}
#endif

#if GTK_CHECK_VERSION(3, 2, 0)
static gboolean
midori_view_overlay_frame_enter_notify_event_cb (GtkOverlay*       overlay,
                                                 GdkEventCrossing* event,
                                                 GtkWidget*        frame)
{
    /* Flip horizontal position of the overlay frame */
    gtk_widget_set_halign (frame,
        gtk_widget_get_halign (frame) == GTK_ALIGN_START
        ? GTK_ALIGN_END : GTK_ALIGN_START);
    return FALSE;
}
#endif

static gboolean
midori_view_web_view_leave_notify_event_cb (WebKitWebView*    web_view,
                                            GdkEventCrossing* event,
                                            MidoriView*       view)
{
    midori_tab_set_statusbar_text (MIDORI_TAB (view), NULL);
    return FALSE;
}

static void
webkit_web_view_hovering_over_link_cb (WebKitWebView*       web_view,
                                       #ifdef HAVE_WEBKIT2
                                       WebKitHitTestResult* hit_test_result,
                                       guint                modifiers,
                                       #else
                                       const gchar*         tooltip,
                                       const gchar*         link_uri,
                                       #endif
                                       MidoriView*          view)
{
    #ifdef HAVE_WEBKIT2
    katze_object_assign (view->hit_test, g_object_ref (hit_test_result));
    if (!webkit_hit_test_result_context_is_link (hit_test_result))
    {
        katze_assign (view->link_uri, NULL);
        return;
    }
    const gchar* link_uri = webkit_hit_test_result_get_link_uri (hit_test_result);
    #endif

    katze_assign (view->link_uri, g_strdup (link_uri));
    if (link_uri && g_str_has_prefix (link_uri, "mailto:"))
    {
        gchar* text = g_strdup_printf (_("Send a message to %s"), &link_uri[7]);
        midori_tab_set_statusbar_text (MIDORI_TAB (view), text);
        g_free (text);
    }
    else
        midori_tab_set_statusbar_text (MIDORI_TAB (view), link_uri);
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
#ifndef HAVE_WEBKIT2
    g_return_if_fail (MIDORI_IS_VIEW (view));

    if (gtk_widget_get_window (view->web_view))
    {

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
#endif
}

#define MIDORI_KEYS_MODIFIER_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK \
    | GDK_MOD1_MASK | GDK_META_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK )

static gboolean
midori_view_web_view_button_press_event_cb (WebKitWebView*  web_view,
                                            GdkEventButton* event,
                                            MidoriView*     view)
{
    const gchar* link_uri;
    gboolean background;

    event->state = event->state & MIDORI_KEYS_MODIFIER_MASK;
    midori_view_ensure_link_uri (view, NULL, NULL, event);
    link_uri = midori_view_get_link_uri (view);
    view->button_press_handled = FALSE;

    if (midori_debug ("mouse"))
        g_message ("%s button %d\n", G_STRFUNC, event->button);

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
        #if GTK_CHECK_VERSION (3, 4, 0)
        if (katze_object_get_boolean (gtk_widget_get_settings (view->web_view), "gtk-enable-primary-paste"))
        #else
        if (midori_settings_get_middle_click_opens_selection (MIDORI_SETTINGS (view->settings)))
        #endif
        {
            #ifndef HAVE_WEBKIT2
            WebKitHitTestResult* result = webkit_web_view_get_hit_test_result (web_view, event);
            WebKitHitTestResultContext context = katze_object_get_int (result, "context");
            gboolean is_editable = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
            g_object_unref (result);
            if (!is_editable)
            {
                gchar* uri;
                GtkClipboard* clipboard = gtk_clipboard_get_for_display (
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
                        gchar* new_uri = sokoke_magic_uri (uri, TRUE, FALSE);
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
            #endif
        }
        if (MIDORI_MOD_SCROLL (event->state))
        {
            midori_view_set_zoom_level (MIDORI_VIEW (view), 1.0);
            return FALSE; /* Allow Ctrl + Middle click */
        }
        return FALSE;
        break;
    case 3:
        /* Older versions don't have the context-menu signal */
        #if WEBKIT_CHECK_VERSION (1, 10, 0)
        if (event->state & GDK_CONTROL_MASK)
        #endif
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
#ifdef G_OS_WIN32
    case 4:
#else
    case 8:
#endif
        midori_view_go_back (view);
        view->button_press_handled = TRUE;
        return TRUE;
#ifdef G_OS_WIN32
    case 5:
#else
    case 9:
#endif
        midori_tab_go_forward (MIDORI_TAB (view));
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

static void
handle_link_hints (WebKitWebView* web_view,
                   GdkEventKey*   event,
                   MidoriView*    view)
{
#ifndef HAVE_WEBKIT2
    gint digit = g_ascii_digit_value (event->keyval);
    gunichar uc = gdk_keyval_to_unicode (event->keyval);
    gchar* result = NULL;
    WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (web_view);
    JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);

    if (view->find_links < 0)
    {
        /* Links are currently off, turn them on */
        midori_tab_inject_stylesheet (MIDORI_TAB (view), ".midoriHKD87346 {"
            " font-size:small !important; font-weight:bold !important;"
            " z-index:500; border-radius:0.3em; line-height:1 !important;"
            " background: white !important; color: black !important;"
            " border:1px solid gray; padding:0 0.1em !important;"
            " position:absolute; display:inline !important; }");
        midori_tab_inject_stylesheet (MIDORI_TAB (view), ".midori_access_key_fc04de {"
            " font-size:small !important; font-weight:bold !important;"
            " z-index:500; border-radius:0.3em; line-height:1 !important;"
            " background: black !important; color: white !important;"
            " border:1px solid gray; padding:0 0.1em 0.2em 0.1em !important;"
            " position:absolute; display:inline !important; }");
        result = sokoke_js_script_eval (js_context,
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
#endif
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

    #ifdef HAVE_WEBKIT2
    WebKitHitTestResultContext context = katze_object_get_int (view->hit_test, "context");
    if (!(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE))
    #else
    if (!webkit_web_view_can_cut_clipboard (web_view)
        && !webkit_web_view_can_paste_clipboard (web_view))
    #endif
    {
        gchar* text = character ? g_strdup_printf ("%c", character) : NULL;
        #if GTK_CHECK_VERSION(3, 2, 0)
        midori_findbar_search_text (MIDORI_FINDBAR (view->overlay_find),
            (GtkWidget*)view, TRUE, katze_str_non_null (text));
        #else
        g_signal_emit_by_name (view, "search-text", TRUE, katze_str_non_null (text));
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
midori_web_view_menu_new_window_activate_cb (GtkAction* action,
                                             gpointer   user_data)
{
    MidoriView* view = user_data;
    g_signal_emit (view, signals[NEW_WINDOW], 0, view->link_uri);
}

static void
midori_web_view_menu_link_copy_activate_cb (GtkAction* widget,
                                            gpointer   user_data)
{
    MidoriView* view = user_data;
    if (g_str_has_prefix (view->link_uri, "mailto:"))
        sokoke_widget_copy_clipboard (view->web_view, view->link_uri + 7, NULL, NULL);
    else
        sokoke_widget_copy_clipboard (view->web_view, view->link_uri, NULL, NULL);
}

static void
midori_view_download_uri (MidoriView*        view,
                          MidoriDownloadType type,
                          const gchar*       uri)
{
#ifdef HAVE_WEBKIT2
    WebKitDownload* download = webkit_web_view_download_uri (WEBKIT_WEB_VIEW (view->web_view), uri);
    WebKitWebContext * web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (view->web_view));
    midori_download_set_type (download, type);
    g_signal_emit_by_name (web_context, "download-started", download, view);
#else
    WebKitNetworkRequest* request = webkit_network_request_new (uri);
    WebKitDownload* download = webkit_download_new (request);
    g_object_unref (request);
    midori_download_set_type (download, type);
    gboolean handled;
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    #endif
}

static void
midori_web_view_menu_save_activate_cb (GtkAction* action,
                                       gpointer   user_data)
{
    MidoriView* view = user_data;
    midori_view_download_uri (view, MIDORI_DOWNLOAD_SAVE_AS, view->link_uri);
}

static void
midori_web_view_menu_image_new_tab_activate_cb (GtkAction* action,
                                                gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    if (view->open_new_pages_in == MIDORI_NEW_PAGE_WINDOW)
        g_signal_emit (view, signals[NEW_WINDOW], 0, uri);
    else
        g_signal_emit (view, signals[NEW_TAB], 0, uri,
            view->open_tabs_in_the_background);
    g_free (uri);
}

/**
 * midori_view_get_resources:
 * @view: a #MidoriView
 *
 * Obtain a list of the resources loaded by the page shown in the view.
 *
 * Return value: (transfer full) (element-type WebKitWebResource): the resources
 **/
GList*
midori_view_get_resources (MidoriView* view)
{
#ifndef HAVE_WEBKIT2
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
    WebKitWebFrame* frame = webkit_web_view_get_main_frame (web_view);
    WebKitWebDataSource* data_source = webkit_web_frame_get_data_source (frame);
    GList* resources = webkit_web_data_source_get_subresources (data_source);
    resources = g_list_prepend (resources, webkit_web_data_source_get_main_resource (data_source));
    g_list_foreach (resources, (GFunc)g_object_ref, NULL);
    return resources;
#else
    return NULL;
#endif
}

static GString*
midori_view_get_data_for_uri (MidoriView*  view,
                              const gchar* uri)
{
    GList* resources = midori_view_get_resources (view);
    GString* result = NULL;

#ifndef HAVE_WEBKIT2
    GList* list;
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
#endif
    g_list_foreach (resources, (GFunc)g_object_unref, NULL);
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
    GdkAtom target = gtk_selection_data_get_target (selection_data);
    /* if (gtk_selection_data_targets_include_image (selection_data, TRUE)) */
    if (gtk_targets_include_image (&target, 1, TRUE))
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
    if (gtk_targets_include_text (&target, 1))
        gtk_selection_data_set_text (selection_data, uri, -1);
    g_free (uri);
}

static void
midori_web_view_menu_image_copy_activate_cb (GtkAction* action,
                                             gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    g_object_set_data (G_OBJECT (view->hit_test), "view", view);
    sokoke_widget_copy_clipboard (view->web_view,
        uri, midori_view_clipboard_get_image_cb, view->hit_test);
    g_free (uri);
}

static void
midori_web_view_menu_image_save_activate_cb (GtkAction* action,
                                             gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* uri = katze_object_get_string (view->hit_test, "image-uri");
    midori_view_download_uri (view, MIDORI_DOWNLOAD_SAVE_AS, uri);
    g_free (uri);
}

static void
midori_web_view_menu_video_copy_activate_cb (GtkAction* action,
                                             gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    sokoke_widget_copy_clipboard (view->web_view, uri, NULL, NULL);
    g_free (uri);
}

static void
midori_web_view_menu_video_save_activate_cb (GtkAction* action,
                                             gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* uri = katze_object_get_string (view->hit_test, "media-uri");
    midori_view_download_uri (view, MIDORI_DOWNLOAD_SAVE_AS, uri);
    g_free (uri);
}

#ifndef HAVE_WEBKIT2
static void
midori_view_menu_open_email_activate_cb (GtkAction* action,
                                         gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* data = (gchar*)g_object_get_data (G_OBJECT (action), "uri");
    gchar* uri = g_strconcat ("mailto:", data, NULL);
    gboolean handled = FALSE;
    g_signal_emit_by_name (view, "open-uri", uri, &handled);
    g_free (uri);
}
#endif

static void
midori_view_menu_open_link_tab_activate_cb (GtkAction* action,
                                            gpointer   user_data)
{
    MidoriView* view = user_data;
    gchar* data = (gchar*)g_object_get_data (G_OBJECT (action), "uri");
    gchar* uri = sokoke_magic_uri (data, TRUE, FALSE);
    if (!uri)
        uri = g_strdup (data);
    g_signal_emit (view, signals[NEW_TAB], 0, uri,
                   view->open_tabs_in_the_background);
    g_free (uri);
}

static void
midori_web_view_menu_background_tab_activate_cb (GtkAction* action,
                                                 gpointer   user_data)
{
    MidoriView* view = user_data;
    g_signal_emit (view, signals[NEW_TAB], 0, view->link_uri,
                   !view->open_tabs_in_the_background);
}

#ifndef HAVE_WEBKIT2
static void
midori_web_view_menu_search_web_activate_cb (GtkAction* action,
                                             gpointer   user_data)
{
    MidoriView* view = user_data;
    const gchar* search = g_object_get_data (G_OBJECT (action), "search");
    if (search == NULL)
        search = midori_settings_get_location_entry_search (MIDORI_SETTINGS (view->settings));
    gchar* uri = midori_uri_for_search (search, view->selected_text);

    if (view->open_new_pages_in == MIDORI_NEW_PAGE_WINDOW)
        g_signal_emit (view, signals[NEW_WINDOW], 0, uri);
    /* FIXME: need a way to override behavior (middle click)
    else if (view->open_new_pages_in == MIDORI_NEW_PAGE_CURRENT)
        midori_view_set_uri (view, uri); */
    else
        g_signal_emit (view, signals[NEW_TAB], 0, uri,
            view->open_tabs_in_the_background);

    g_free (uri);
}
#endif

static void
midori_view_tab_label_menu_window_new_cb (GtkAction* action,
                                          gpointer   user_data)
{
    MidoriView* view = user_data;
    g_signal_emit (view, signals[NEW_WINDOW], 0,
        midori_view_get_display_uri (MIDORI_VIEW (view)));
}

#ifndef HAVE_WEBKIT2
static void
midori_web_view_open_frame_in_new_tab_cb (GtkAction* action,
                                          gpointer   user_data)
{
    MidoriView* view = user_data;
    WebKitWebFrame* web_frame = webkit_web_view_get_focused_frame (WEBKIT_WEB_VIEW (view->web_view));
    g_signal_emit (view, signals[NEW_TAB], 0,
        webkit_web_frame_get_uri (web_frame), view->open_tabs_in_the_background);
}
#endif

static void
midori_view_inspect_element_activate_cb (GtkAction* action,
                                         gpointer   user_data)
{
    MidoriView* view = user_data;
    WebKitWebInspector* inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (view->web_view));
    #ifndef HAVE_WEBKIT2
    WebKitHitTestResult* hit_test_result = view->hit_test;
    gint x = katze_object_get_int (hit_test_result, "x");
    gint y = katze_object_get_int (hit_test_result, "y");
    webkit_web_inspector_inspect_coordinates (inspector, x, y);
    #endif
    webkit_web_inspector_show (inspector);
}

static void
midori_view_add_search_engine_cb (GtkWidget*  widget,
                                  MidoriView* view)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (view->web_view);
    GtkActionGroup* actions = midori_browser_get_action_group (browser);
    GtkAction* action = gtk_action_group_get_action (actions, "Search");
    KatzeItem* item = g_object_get_data (G_OBJECT (widget), "item");
    midori_search_action_get_editor (MIDORI_SEARCH_ACTION (action), item, TRUE);
}

/**
 * midori_view_get_page_context_action:
 * @view: a #MidoriView
 * @hit_test_result: a #WebKitHitTestResult
 *
 * Populates actions depending on the hit test result.
 *
 * Since: 0.5.5
 */
MidoriContextAction*
midori_view_get_page_context_action (MidoriView*          view,
                                     WebKitHitTestResult* hit_test_result)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);
    g_return_val_if_fail (hit_test_result != NULL, NULL);

    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (view));
    GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (browser)));
    WebKitHitTestResultContext context = katze_object_get_int (hit_test_result, "context");
    GtkActionGroup* actions = midori_browser_get_action_group (browser);
    MidoriContextAction* menu = midori_context_action_new ("PageContextMenu", NULL, NULL, NULL);
    midori_context_action_add_action_group (menu, actions);

    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)
    {
        /* Enforce update of actions - there's no "selection-changed" signal */
        midori_tab_update_actions (MIDORI_TAB (view), actions, NULL, NULL);
        midori_context_action_add_by_name (menu, "Undo");
        midori_context_action_add_by_name (menu, "Redo");
        midori_context_action_add (menu, NULL);
        midori_context_action_add_by_name (menu, "Cut");
        midori_context_action_add_by_name (menu, "Copy");
        midori_context_action_add_by_name (menu, "Paste");
        midori_context_action_add_by_name (menu, "Delete");
        midori_context_action_add (menu, NULL);
        midori_context_action_add_by_name (menu, "SelectAll");
        midori_context_action_add (menu, NULL);
        KatzeItem* item = midori_search_action_get_engine_for_form (
            WEBKIT_WEB_VIEW (view->web_view), view->ellipsize);
        if (item != NULL)
        {
            GtkAction* action = gtk_action_new ("AddSearchEngine", _("Add _search engine..."), NULL, NULL);
            g_object_set_data (G_OBJECT (action), "item", item);
            g_signal_connect (action, "activate",
                              G_CALLBACK (midori_view_add_search_engine_cb), view);
            midori_context_action_add (menu, action);
        }
        /* FIXME: input methods */
        /* FIXME: font */
        /* FIXME: insert unicode character */
    }

    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
    {
        if (midori_paths_get_runtime_mode () == MIDORI_RUNTIME_MODE_APP)
        {
            GtkAction* action = gtk_action_new ("OpenLinkTab", _("Open _Link"), NULL, STOCK_TAB_NEW);
            g_object_set_data_full (G_OBJECT (action), "uri", g_strdup (view->link_uri), (GDestroyNotify)g_free);
            g_signal_connect (action, "activate", G_CALLBACK (midori_view_menu_open_link_tab_activate_cb), view);
            midori_context_action_add (menu, action);
        }
        else if (!midori_view_always_same_tab (view->link_uri))
        {
            GtkAction* action = gtk_action_new ("OpenLinkTab", _("Open Link in New _Tab"), NULL, STOCK_TAB_NEW);
            g_object_set_data_full (G_OBJECT (action), "uri", g_strdup (view->link_uri), (GDestroyNotify)g_free);
            g_signal_connect (action, "activate", G_CALLBACK (midori_view_menu_open_link_tab_activate_cb), view);
            midori_context_action_add (menu, action);
            midori_context_action_add_simple (menu, "OpenLinkForegroundTab",
                view->open_tabs_in_the_background
                ? _("Open Link in _Foreground Tab") : _("Open Link in _Background Tab"), NULL, NULL,
                midori_web_view_menu_background_tab_activate_cb, view);
            midori_context_action_add_simple (menu, "OpenLinkWindow", _("Open Link in New _Window"), NULL, STOCK_WINDOW_NEW,
                midori_web_view_menu_new_window_activate_cb, view);
        }

        midori_context_action_add_simple (menu, "CopyLinkDestination", _("Copy Link de_stination"), NULL, NULL,
            midori_web_view_menu_link_copy_activate_cb, view);

        if (!midori_view_always_same_tab (view->link_uri))
        {
            /* GTK_STOCK_SAVE_AS is lacking the underline */
            midori_context_action_add_simple (menu, "SaveLinkAs", _("Save _As…"), NULL, GTK_STOCK_SAVE_AS,
                midori_web_view_menu_save_activate_cb, view);
        }
    }

    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE)
    {
        if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
            midori_context_action_add (menu, NULL);

        midori_context_action_add_simple (menu, "OpenImageNewTab",
            view->open_new_pages_in == MIDORI_NEW_PAGE_WINDOW
            ? _("Open _Image in New Window") : _("Open _Image in New Tab")
            , NULL, STOCK_TAB_NEW,
            midori_web_view_menu_image_new_tab_activate_cb, view);
        midori_context_action_add_simple (menu, "CopyImage", _("Copy Im_age"), NULL, NULL,
            midori_web_view_menu_image_copy_activate_cb, view);
        midori_context_action_add_simple (menu, "SaveImage", _("Save I_mage"), NULL, GTK_STOCK_SAVE,
            midori_web_view_menu_image_save_activate_cb, view);
    }

    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA)
    {
        midori_context_action_add_simple (menu, "CopyVideoAddress", _("Copy Video _Address"), NULL, NULL,
            midori_web_view_menu_video_copy_activate_cb, view);
        midori_context_action_add_simple (menu, "DownloadVideo", _("Download _Video"), NULL, GTK_STOCK_SAVE,
            midori_web_view_menu_video_save_activate_cb, view);
    }

    #ifndef HAVE_WEBKIT2
    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION)
    {
        if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
            midori_context_action_add (menu, NULL);

        /* Ensure view->selected_text */
        midori_view_has_selection (view);
        if (midori_uri_is_valid (view->selected_text))
        {
            /* :// and @ together would mean login credentials */
            if (g_str_has_prefix (view->selected_text, "mailto:")
             || (strchr (view->selected_text, '@') != NULL
              && strstr (view->selected_text, "://") == NULL))
            {
                gchar* text = g_strdup_printf (_("Send a message to %s"), view->selected_text);
                GtkAction* action = (GtkAction*)midori_context_action_new_escaped ("SendMessage", text, NULL, GTK_STOCK_JUMP_TO);
                g_object_set_data_full (G_OBJECT (action), "uri", g_strdup (view->selected_text), (GDestroyNotify)g_free);
                g_signal_connect (action, "activate", G_CALLBACK (midori_view_menu_open_email_activate_cb), view);
                midori_context_action_add (menu, action);
                g_free (text);
            }
            else
            {
                GtkAction* action = gtk_action_new ("OpenAddressInNewTab", _("Open Address in New _Tab"), NULL, GTK_STOCK_JUMP_TO);
                g_object_set_data_full (G_OBJECT (action), "uri", g_strdup (view->selected_text), (GDestroyNotify)g_free);
                g_signal_connect (action, "activate", G_CALLBACK (midori_view_menu_open_link_tab_activate_cb), view);
                midori_context_action_add (menu, action);
            }
        }

        KatzeArray* search_engines = katze_object_get_object (browser, "search-engines");
        if (search_engines != NULL)
        {
            MidoriContextAction* searches = midori_context_action_new ("SearchWith", _("Search _with"), NULL, NULL);
            midori_context_action_add (menu, GTK_ACTION (searches));

            KatzeItem* item;
            guint i = 0;
            KATZE_ARRAY_FOREACH_ITEM (item, search_engines)
            {
                GdkPixbuf* pixbuf;
                gchar* search_option = g_strdup_printf ("SearchWith%u", i);
                GtkAction* action = (GtkAction*)midori_context_action_new_escaped (search_option, katze_item_get_name (item), NULL, STOCK_EDIT_FIND);
                g_free (search_option);
                midori_context_action_add (searches, action);
                if ((pixbuf = midori_paths_get_icon (katze_item_get_uri (item), NULL)))
                {
                    gtk_action_set_gicon (action, G_ICON (pixbuf));
                    g_object_unref (pixbuf);
                }
                else
                {
                    GIcon* icon = g_themed_icon_new_with_default_fallbacks ("edit-find-option-symbolic");
                    gtk_action_set_gicon (action, icon);
                }
                gtk_action_set_always_show_image (GTK_ACTION (action), TRUE);
                g_object_set_data (G_OBJECT (action), "search", (gchar*)katze_item_get_uri (item));
                g_signal_connect (action, "activate",
                    G_CALLBACK (midori_web_view_menu_search_web_activate_cb), view);
                i++;
            }
            g_object_unref (search_engines);
        }
        if (midori_settings_get_location_entry_search (MIDORI_SETTINGS (view->settings)) != NULL)
            midori_context_action_add_simple (menu, "SearchWeb", _("_Search the Web"), NULL, GTK_STOCK_FIND,
                midori_web_view_menu_search_web_activate_cb, view);
    }
    #endif

    if (context == WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT)
    {
        midori_context_action_add_by_name (menu, "Back");
        midori_context_action_add_by_name (menu, "Forward");
        midori_context_action_add_by_name (menu, "Stop");
        midori_context_action_add_by_name (menu, "Reload");
    }

    /* No need to have Copy twice, which is already in the editable menu */
    if (!(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE))
    {
        midori_context_action_add (menu, NULL);
        /* Enforce update of actions - there's no "selection-changed" signal */
        midori_tab_update_actions (MIDORI_TAB (view), actions, NULL, NULL);
        midori_context_action_add_by_name (menu, "Copy");
        midori_context_action_add_by_name (menu, "SelectAll");
    }

    if (context == WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT)
    {
        midori_context_action_add (menu, NULL);
        midori_context_action_add_by_name (menu, "UndoTabClose");
        #ifndef HAVE_WEBKIT2
        WebKitWebView* web_view = WEBKIT_WEB_VIEW (view->web_view);
        if (webkit_web_view_get_focused_frame (web_view) != webkit_web_view_get_main_frame (web_view))
            midori_context_action_add_simple (menu, "OpenFrameInNewTab", _("Open _Frame in New Tab"), NULL, NULL,
                midori_web_view_open_frame_in_new_tab_cb, view);
        #endif
        midori_context_action_add_simple (menu, "OpenInNewWindow", _("Open in New _Window"), NULL, STOCK_WINDOW_NEW,
            midori_view_tab_label_menu_window_new_cb, view);
        midori_context_action_add_by_name (menu, "ZoomIn");
        midori_context_action_add_by_name (menu, "ZoomOut");

        MidoriContextAction* encodings = midori_context_action_new ("Encoding", _("_Encoding"), NULL, NULL);
        midori_context_action_add (menu, GTK_ACTION (encodings));
        midori_context_action_add_by_name (encodings, "EncodingAutomatic");
        midori_context_action_add_by_name (encodings, "EncodingChinese");
        midori_context_action_add_by_name (encodings, "EncodingChineseSimplified");
        midori_context_action_add_by_name (encodings, "EncodingJapanese");
        midori_context_action_add_by_name (encodings, "EncodingKorean");
        midori_context_action_add_by_name (encodings, "EncodingRussian");
        midori_context_action_add_by_name (encodings, "EncodingUnicode");
        midori_context_action_add_by_name (encodings, "EncodingWestern");
        midori_context_action_add_by_name (encodings, "EncodingCustom");

        midori_context_action_add (menu, NULL);
        midori_context_action_add_by_name (menu, "BookmarkAdd");
        midori_context_action_add_by_name (menu, "AddSpeedDial");
        midori_context_action_add_by_name (menu, "SaveAs");
        midori_context_action_add_by_name (menu, "SourceView");
        midori_context_action_add_by_name (menu, "SourceViewDom");
        if (!g_object_get_data (G_OBJECT (browser), "midori-toolbars-visible"))
            midori_context_action_add_by_name (menu, "Navigationbar");
        if (state & GDK_WINDOW_STATE_FULLSCREEN)
            midori_context_action_add_by_name (menu, "Fullscreen");
    }

    if (katze_object_get_boolean (view->settings, "enable-developer-extras"))
        midori_context_action_add_simple (menu, "InspectElement", _("Inspect _Element"), NULL, NULL,
            midori_view_inspect_element_activate_cb, view);

    g_signal_emit_by_name (view, "context-menu", hit_test_result, menu);
    return menu;
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
 * Deprecated: 0.5.5: Use midori_view_get_page_context_action().
 */
void
midori_view_populate_popup (MidoriView* view,
                            GtkWidget*  menu,
                            gboolean    manual)
{
    g_return_if_fail (MIDORI_IS_VIEW (view));
    g_return_if_fail (GTK_IS_MENU_SHELL (menu));

    GdkEvent* event = gtk_get_current_event();
    midori_view_ensure_link_uri (view, NULL, NULL, (GdkEventButton *)event);
    gdk_event_free (event);
    MidoriContextAction* context_action = midori_view_get_page_context_action (view, view->hit_test);
    midori_context_action_create_menu (context_action, GTK_MENU (menu), FALSE);
}

#if WEBKIT_CHECK_VERSION (1, 10, 0)
static gboolean
midori_view_web_view_context_menu_cb (WebKitWebView*       web_view,
                                      #ifdef HAVE_WEBKIT2
                                      WebKitContextMenu*   context_menu,
                                      GdkEvent*            event,
                                      WebKitHitTestResult* hit_test_result,
                                      #else
                                      GtkMenu*             default_menu,
                                      WebKitHitTestResult* hit_test_result,
                                      gboolean             keyboard,
                                      #endif
                                      MidoriView*          view)
{
    #ifndef HAVE_WEBKIT2
    GdkEvent* event = gtk_get_current_event();
    midori_view_ensure_link_uri (view, NULL, NULL, (GdkEventButton *)event);
    gdk_event_free (event);
    #endif
    MidoriContextAction* menu = midori_view_get_page_context_action (view, hit_test_result);
    /* Retain specific menu items we can't re-create easily */
    guint guesses = 0, guesses_max = 10; /* Maximum number of spelling suggestions */
    #ifdef HAVE_WEBKIT2
    GList* items = webkit_context_menu_get_items (context_menu), *item, *preserved = NULL;
    for (item = items; item; item = g_list_next (item))
    {
        WebKitContextMenuAction stock_action = webkit_context_menu_item_get_stock_action (item->data);
        if (stock_action == WEBKIT_CONTEXT_MENU_ACTION_SPELLING_GUESS && guesses++ < guesses_max)
            preserved = g_list_append (preserved, g_object_ref (item->data));
    }
    webkit_context_menu_remove_all (context_menu);
    for (item = preserved; item; item = g_list_next (item))
    {
        webkit_context_menu_append (context_menu, item->data);
        g_object_unref (item->data);
    }
    g_list_free (preserved);
    midori_context_action_create_webkit_context_menu (menu, context_menu);
    #else
    GList* items = gtk_container_get_children (GTK_CONTAINER (default_menu)), *item;
    for (item = items; item; item = g_list_next (item))
    {
        /* Private API: Source/WebCore/platform/ContextMenuItem.h */
        int stock_action = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item->data), "webkit-context-menu"));
        const int ContextMenuItemTagSpellingGuess = 30;
        if (stock_action == ContextMenuItemTagSpellingGuess && guesses++ < guesses_max)
            continue;
        else
            gtk_widget_destroy (item->data);
    }
    g_list_free (items);
    midori_context_action_create_menu (menu, default_menu, FALSE);
    #endif
    return FALSE;
}
#endif

static gboolean
midori_view_web_view_close_cb (WebKitWebView* web_view,
                               GtkWidget*     view)
{
    midori_browser_close_tab (midori_browser_get_for_widget (view), view);
    return TRUE;
}

static gboolean
webkit_web_view_web_view_ready_cb (GtkWidget*  web_view,
                                   MidoriView* view)
{
    MidoriNewView where = MIDORI_NEW_VIEW_TAB;
    GtkWidget* new_view = GTK_WIDGET (midori_view_get_for_widget (web_view));

#ifdef HAVE_WEBKIT2
    WebKitWindowProperties* features = webkit_web_view_get_window_properties (WEBKIT_WEB_VIEW (web_view));
#else
    WebKitWebWindowFeatures* features = webkit_web_view_get_window_features (WEBKIT_WEB_VIEW (web_view));
#endif
    gboolean locationbar_visible, menubar_visible, toolbar_visible;
    gint width, height;
    g_object_get (features,
                  "locationbar-visible", &locationbar_visible,
                  "menubar-visible", &menubar_visible,
                  "toolbar-visible", &toolbar_visible,
                  "width", &width,
                  "height", &height,
                  NULL);
    midori_tab_set_is_dialog (MIDORI_TAB (view),
     !locationbar_visible && !menubar_visible && !toolbar_visible
     && width > 0 && height > 0);

    /* Open windows opened by scripts in tabs if they otherwise
        would be replacing the page the user opened. */
    if (view->open_new_pages_in == MIDORI_NEW_PAGE_CURRENT)
        if (!midori_tab_get_is_dialog (MIDORI_TAB (view)))
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

    if (midori_tab_get_is_dialog (MIDORI_TAB (view)))
    {
        GtkWidget* toplevel = gtk_widget_get_toplevel (new_view);
        if (width > 0 && height > 0)
            gtk_widget_set_size_request (toplevel, width, height);
#ifdef HAVE_WEBKIT2
        g_signal_connect (web_view, "close",
                          G_CALLBACK (midori_view_web_view_close_cb), new_view);
#else
        g_signal_connect (web_view, "close-web-view",
                          G_CALLBACK (midori_view_web_view_close_cb), new_view);
#endif
    }

    return TRUE;
}

static GtkWidget*
webkit_web_view_create_web_view_cb (GtkWidget*      web_view,
#ifndef HAVE_WEBKIT2
                                    WebKitWebFrame* web_frame,
#endif
                                    MidoriView*     view)
{
    MidoriView* new_view;

#ifdef HAVE_WEBKIT2
    const gchar* uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view));
#else
    const gchar* uri = webkit_web_frame_get_uri (web_frame);
#endif
    if (view->open_new_pages_in == MIDORI_NEW_PAGE_CURRENT)
        new_view = view;
    else
    {
        KatzeItem* item = katze_item_new ();
        item->uri = g_strdup (uri);
        new_view = (MidoriView*)midori_view_new_from_view (view, item, NULL);
#ifdef HAVE_WEBKIT2
        g_signal_connect (new_view->web_view, "ready-to-show",
                          G_CALLBACK (webkit_web_view_web_view_ready_cb), view);
#else
        g_signal_connect (new_view->web_view, "web-view-ready",
                          G_CALLBACK (webkit_web_view_web_view_ready_cb), view);
#endif
    }
    g_object_set_data_full (G_OBJECT (new_view), "opener-uri", g_strdup (uri), g_free);
    return new_view->web_view;
}

#ifndef HAVE_WEBKIT2
static gboolean
webkit_web_view_mime_type_decision_cb (GtkWidget*               web_view,
                                       WebKitWebFrame*          web_frame,
                                       WebKitNetworkRequest*    request,
                                       const gchar*             mime_type,
                                       WebKitWebPolicyDecision* decision,
                                       MidoriView*              view)
{
    /* FIXME: Never download plugins from sub resources */
    if (!strcmp (mime_type, "application/x-shockwave-flash"))
        if (strcmp (midori_tab_get_uri (MIDORI_TAB (view)), webkit_network_request_get_uri (request)))
            return FALSE;

    if (webkit_web_view_can_show_mime_type (WEBKIT_WEB_VIEW (web_view), mime_type))
    {
        if (web_frame == webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view)))
        {
            g_warn_if_fail (mime_type != NULL);
            midori_tab_set_mime_type (MIDORI_TAB (view), mime_type);
            katze_item_set_meta_string (view->item, "mime-type", mime_type);
            if (view->icon == NULL)
                midori_view_unset_icon (view);
        }

        return FALSE;
    }

    g_object_set_data(G_OBJECT (view), "download-mime-type", (gpointer)mime_type);
    webkit_web_policy_decision_download (decision);
    g_object_set_data(G_OBJECT (view), "download-mime-type", NULL);
    return TRUE;
}
#endif

gint
midori_save_dialog (const gchar* title,
                    const gchar * hostname,
                    const GString* details,
                    const gchar *content_type)
{   
    GIcon* icon;
    GtkWidget* image;
    GdkScreen* screen;
    GtkWidget* dialog= NULL;
    GtkIconTheme* icon_theme;
    gint response;
    dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        _("Open or download file from %s"), hostname);
    icon = g_content_type_get_icon (content_type);
    g_themed_icon_append_name (G_THEMED_ICON (icon), "text-html");
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
    gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
    g_object_unref (icon);
    gtk_widget_show (image);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
        "%s", details->str);
    screen = gtk_widget_get_screen (dialog);
    
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
    if (screen)
    {
        icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, MIDORI_STOCK_TRANSFER))
            gtk_window_set_icon_name (GTK_WINDOW (dialog), MIDORI_STOCK_TRANSFER);
        else
            gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_OPEN);
    }
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
        GTK_STOCK_SAVE, MIDORI_DOWNLOAD_SAVE,
        GTK_STOCK_SAVE_AS, MIDORI_DOWNLOAD_SAVE_AS,
        GTK_STOCK_CANCEL, MIDORI_DOWNLOAD_CANCEL,
        GTK_STOCK_OPEN, MIDORI_DOWNLOAD_OPEN,
        NULL);

    response = midori_dialog_run (GTK_DIALOG (dialog));
    
    gtk_widget_destroy (dialog);
    if (response == GTK_RESPONSE_DELETE_EVENT)
        response = MIDORI_DOWNLOAD_CANCEL;
    return response;
}

#ifdef HAVE_WEBKIT2
static gboolean
midori_view_download_decide_destination_cb (WebKitDownload*   download,
                                    const gchar * suggested_filename,
                                    MidoriView*      view)
{
    if(!midori_view_download_query_action (view, download, suggested_filename)) {
        webkit_download_cancel (download);
    }
    return TRUE;    //we must return TRUE because we handled the signal
}
                
static void
midori_view_download_started_cb (WebKitWebContext* context,
                                   WebKitDownload*   download,
                                   MidoriView          *view)
{
    g_signal_connect (download, "decide-destination",
                      G_CALLBACK (midori_view_download_decide_destination_cb), view);
}

static gboolean
midori_view_download_query_action (MidoriView* view,
                                   WebKitDownload*   download,
                                   const gchar * suggested_filename)
{
#else
static gboolean
midori_view_download_requested_cb (GtkWidget*      web_view,
                                   WebKitDownload* download,
                                   MidoriView*     view)
{
#endif
    gboolean handled = TRUE;
    gchar* hostname;
    gchar* content_type;
    gchar* description;
    gchar* title;
    gint response;
    GString* details;
    /* Opener may differ from displaying view:
       http://lcamtuf.coredump.cx/fldl/ http://lcamtuf.coredump.cx/switch/ */
    const gchar* opener_uri = g_object_get_data (G_OBJECT (view), "opener-uri");
    hostname = midori_uri_parse_hostname (
        opener_uri ? opener_uri : midori_view_get_display_uri (view), NULL);
    
    #ifdef HAVE_WEBKIT2
    content_type = g_content_type_guess (suggested_filename, NULL ,
                                            0 ,NULL);
    if (!content_type)
        content_type = g_strdup ("application/octet-stream");
    midori_download_set_filename (download, g_strdup (suggested_filename));
    #else
    content_type = midori_download_get_content_type (download,
        g_object_get_data (G_OBJECT (view), "download-mime-type"));
    #endif
    description = g_content_type_get_description (content_type);

    details = g_string_sized_new (20 * 4);
    #ifdef HAVE_WEBKIT2
    const gchar * suggestion = webkit_uri_response_get_suggested_filename (webkit_download_get_response (download));
    g_string_append_printf (details, _("File Name: %s"),
                suggestion ? suggestion : suggested_filename);
    #else
    g_string_append_printf (details, _("File Name: %s"),
        webkit_download_get_suggested_filename (download));
    #endif
    g_string_append_c (details, '\n');

    if (g_strrstr (description, content_type))
        g_string_append_printf (details, _("File Type: '%s'"), content_type);
    else
        g_string_append_printf (details, _("File Type: %s ('%s')"), description, content_type);
    g_string_append_c (details, '\n');

    #ifndef HAVE_WEBKIT2
    /* Link Fingerprint */
    /* We look at the original URI because redirection would lose the fragment */
    WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));
    WebKitWebDataSource* datasource = webkit_web_frame_get_provisional_data_source (web_frame);
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
    #endif

    #ifdef HAVE_WEBKIT2
    /* i18n: A file open dialog title, ie. "Open http://fila.com/manual.tgz" */
    title = g_strdup_printf (_("Open %s"), webkit_uri_request_get_uri (webkit_download_get_request (download)));
    #else
    title = g_strdup_printf (_("Open %s"), webkit_download_get_uri (download));
    #endif
    response = midori_save_dialog (title,
            hostname, details, content_type); //We prompt a dialog
    g_free (title);
    g_free (hostname);
    g_free (description);
    g_free (content_type);
    g_string_free (details, TRUE);
    midori_download_set_type (download, response);
    g_signal_emit (view, signals[DOWNLOAD_REQUESTED], 0, download, &handled);
    return handled;
}

#ifndef HAVE_WEBKIT2
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
        g_signal_emit_by_name (view, "console-message", message, line, source_id);
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

    if (midori_paths_get_runtime_mode () == MIDORI_RUNTIME_MODE_PRIVATE)
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
#endif

static void
midori_view_init (MidoriView* view)
{
    view->title = NULL;
    view->icon = NULL;
    view->icon_uri = NULL;
    view->hit_test = NULL;
    view->link_uri = NULL;
    view->selected_text = NULL;
    view->news_feeds = NULL;
    view->find_links = -1;
    view->alerts = 0;

    view->item = katze_item_new ();

    view->scrollh = view->scrollv = -2;
    #ifndef HAVE_WEBKIT2
    /* Adjustments are not created initially, but overwritten later */
    view->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view->scrolled_window),
                                         GTK_SHADOW_NONE);
    g_signal_connect (view->scrolled_window, "notify::hadjustment",
        G_CALLBACK (midori_view_notify_hadjustment_cb), view);
    g_signal_connect (view->scrolled_window, "notify::vadjustment",
        G_CALLBACK (midori_view_notify_vadjustment_cb), view);
    #endif

    g_signal_connect (view->item, "meta-data-changed",
        G_CALLBACK (midori_view_item_meta_data_changed), view);
}

static void
midori_view_finalize (GObject* object)
{
    MidoriView* view = MIDORI_VIEW (object);

    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);
    g_signal_handlers_disconnect_by_func (view->item,
        midori_view_item_meta_data_changed, view);

    katze_assign (view->title, NULL);
    katze_object_assign (view->icon, NULL);
    katze_assign (view->icon_uri, NULL);

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
    MidoriView* view = MIDORI_VIEW (object);

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
        break;
    case PROP_ZOOM_LEVEL:
        midori_view_set_zoom_level (view, g_value_get_float (value));
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
    case PROP_TITLE:
        g_value_set_string (value, view->title);
        break;
    case PROP_ICON:
        g_value_set_object (value, view->icon);
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_float (value, midori_view_get_zoom_level (view));
        break;
    case PROP_NEWS_FEEDS:
        g_value_set_object (value, view->news_feeds);
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

static void
_midori_view_set_settings (MidoriView*        view,
                           MidoriWebSettings* settings)
{
    gboolean zoom_text_and_images;
    gdouble zoom_level;

    if (view->settings)
        g_signal_handlers_disconnect_by_func (view->settings,
            midori_view_settings_notify_cb, view);

    katze_object_assign (view->settings, settings);
    if (!settings)
        return;

    g_object_ref (settings);
    g_signal_connect (settings, "notify",
                      G_CALLBACK (midori_view_settings_notify_cb), view);

    g_object_get (view->settings,
        "zoom-level", &zoom_level,
        "zoom-text-and-images", &zoom_text_and_images,
        "open-new-pages-in", &view->open_new_pages_in,
        "open-tabs-in-the-background", &view->open_tabs_in_the_background,
        NULL);

    webkit_web_view_set_settings (WEBKIT_WEB_VIEW (view->web_view), (void*)settings);
    #ifndef HAVE_WEBKIT2
    webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (view->web_view),
        zoom_text_and_images);
    #endif
    midori_view_set_zoom_level (view, zoom_level);
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
 * Return value: (transfer full): a new #MidoriView
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
 * Return value: (transfer full): a new #MidoriView
 *
 * Since: 0.4.3
 * Deprecated: 0.5.8: Use midori_view_new_from_view instead.
 **/
GtkWidget*
midori_view_new_with_item (KatzeItem*         item,
                           MidoriWebSettings* settings)
{
    return midori_view_new_from_view (NULL, item, settings);
}

/**
 * midori_view_new_from_view:
 * @view: a predating, related #MidoriView, or %NULL
 * @item: a #KatzeItem, or %NULL
 * @settings: a #MidoriWebSettings, or %NULL
 *
 * Creates a new view, visible by default.
 *
 * If a @view is specified the returned new view will share
 * its settings and if applicable re-use the rendering process.
 *
 * When @view should be passed:
 *     The new one created is a new tab/ window for the old @view
 *     A tab was duplicated
 *
 * When @view may be passed:
 *     Old and new view belong to the same website or group
 *
 * Don't pass a @view if:
 *     The new view is a completely new website
 *
 * The @item may contain title, URI and minimized status and will be copied.
 *
 * Usually @settings should be passed from an existing view or browser.
 *
 * Return value: (transfer full): a new #MidoriView
 *
 * Since: 0.5.8
 **/
GtkWidget*
midori_view_new_from_view (MidoriView*        related,
                           KatzeItem*         item,
                           MidoriWebSettings* settings)
{
    MidoriView* view = g_object_new (MIDORI_TYPE_VIEW,
                                     "related", MIDORI_TAB (related),
                                     "title", item ? katze_item_get_name (item) : NULL,
                                     NULL);
    if (!settings && related)
        settings = related->settings;
    if (settings)
        _midori_view_set_settings (view, settings);
    if (item)
    {
        katze_object_assign (view->item, katze_item_copy (item));
        midori_tab_set_minimized (MIDORI_TAB (view),
            katze_item_get_meta_string (view->item, "minimized") != NULL);
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

    if (name == g_intern_string ("open-new-pages-in"))
        view->open_new_pages_in = g_value_get_enum (&value);
    #ifndef HAVE_WEBKIT2
    else if (name == g_intern_string ("zoom-text-and-images"))
    {
        if (view->web_view)
            webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (view->web_view),
                g_value_get_boolean (&value));
    }
    #endif
    else if (name == g_intern_string ("open-tabs-in-the-background"))
        view->open_tabs_in_the_background = g_value_get_boolean (&value);
    else if (name == g_intern_string ("enable-javascript"))
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

    return midori_tab_get_load_status (MIDORI_TAB (view));
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

    return midori_tab_get_progress (MIDORI_TAB (view));
}

#ifndef HAVE_WEBKIT2
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
midori_view_web_inspector_construct_window (gpointer       inspector,
                                            WebKitWebView* web_view,
                                            GtkWidget*     inspector_view,
                                            MidoriView*    view)
{
    gchar* title;
    const gchar* label;
    GtkWidget* window;
    GtkWidget* toplevel;
    const gchar* icon_name;
    GtkIconTheme* icon_theme;
    GdkPixbuf* icon;
    GdkPixbuf* gray_icon;
    #if GTK_CHECK_VERSION (3, 0, 0)
    GtkWidget* scrolled;
    #endif

    label = midori_view_get_display_title (view);
    title = g_strdup_printf (_("Inspect page - %s"), label);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), title);
    g_free (title);

    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
    if (gtk_widget_is_toplevel (toplevel))
    {
        gtk_window_set_screen (GTK_WINDOW (window), gtk_window_get_screen (GTK_WINDOW (toplevel)));
        katze_window_set_sensible_default_size (GTK_WINDOW (window));
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
    gtk_widget_set_size_request (GTK_WIDGET (inspector_view), 700, 100);
    #if GTK_CHECK_VERSION (3, 0, 0)
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled), inspector_view);
    gtk_container_add (GTK_CONTAINER (window), scrolled);
    gtk_widget_show_all (scrolled);
    #else
    gtk_container_add (GTK_CONTAINER (window), inspector_view);
    gtk_widget_show_all (inspector_view);
    #endif

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
    midori_view_web_inspector_construct_window (inspector,
        web_view, inspector_view, view);
    return WEBKIT_WEB_VIEW (inspector_view);
}

static gboolean
midori_view_web_inspector_show_window_cb (WebKitWebInspector* inspector,
                                          MidoriView*         view)
{
    GtkWidget* inspector_view = GTK_WIDGET (webkit_web_inspector_get_web_view (inspector));
    GtkWidget* window = gtk_widget_get_toplevel (inspector_view);
    if (!window)
        return FALSE;
    if (katze_object_get_boolean (view->settings, "last-inspector-attached"))
    {
        gboolean handled = FALSE;
        g_signal_emit_by_name (inspector, "attach-window", &handled);
    }
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
    GtkWidget* inspector_view = GTK_WIDGET (webkit_web_inspector_get_web_view (inspector));
    g_signal_emit_by_name (view, "attach-inspector", inspector_view);
    return TRUE;
}

/**
 * midori_view_web_inspector_get_own_window:
 * @inspector: the inspector instance
 *
 * Get the widget containing the inspector, generally either a GtkWindow
 * or the container where it is "docked".
 *
 * Return value: (allow-none): the widget containing the inspector, or NULL.
 * 
 * Since: 0.5.10
 */
static GtkWidget*
midori_view_web_inspector_get_parent (gpointer inspector)
{
    GtkWidget* inspector_view = GTK_WIDGET (webkit_web_inspector_get_web_view (inspector));

    #if defined(HAVE_WEBKIT2) || GTK_CHECK_VERSION (3, 0, 0)
    GtkWidget* scrolled = gtk_widget_get_parent (inspector_view);
    if (!scrolled)
        return NULL;
    return gtk_widget_get_parent (scrolled);
    #else
    return gtk_widget_get_parent (inspector_view);
    #endif
}

static gboolean
midori_view_web_inspector_detach_window_cb (gpointer    inspector,
                                            MidoriView* view)
{
    GtkWidget* inspector_view = GTK_WIDGET (webkit_web_inspector_get_web_view (inspector));
    GtkWidget* parent = midori_view_web_inspector_get_parent (inspector);

    if (GTK_IS_WINDOW (parent))
        return FALSE;

    gtk_widget_hide (parent);
    g_signal_emit_by_name (view, "detach-inspector", inspector_view);
    midori_view_web_inspector_construct_window (inspector,
        WEBKIT_WEB_VIEW (view->web_view), inspector_view, view);
    return TRUE;
}

static gboolean
midori_view_web_inspector_close_window_cb (gpointer    inspector,
                                           MidoriView* view)
{
    GtkWidget* parent = midori_view_web_inspector_get_parent (inspector);

    if (!parent)
        return FALSE;

    gtk_widget_hide (parent);
    return TRUE;
}
#endif

static GObject*
midori_view_constructor (GType                  type,
                         guint                  n_construct_properties,
                         GObjectConstructParam* construct_properties)
{
    GObject* object = G_OBJECT_CLASS (midori_view_parent_class)->constructor (
        type, n_construct_properties, construct_properties);
    MidoriView* view = MIDORI_VIEW (object);

    view->web_view = GTK_WIDGET (midori_tab_get_web_view (MIDORI_TAB (view)));
    g_object_connect (view->web_view,
                      #ifdef HAVE_WEBKIT2
                      "signal::load-failed",
                      webkit_web_view_load_error_cb, view,
                      "signal::load-changed",
                      midori_view_web_view_load_changed_cb, view,
                      "signal::notify::estimated-load-progress",
                      webkit_web_view_progress_changed_cb, view,
                      "signal::notify::favicon",
                      midori_web_view_notify_icon_uri_cb, view,
                      "signal::mouse-target-changed",
                      webkit_web_view_hovering_over_link_cb, view,
                      "signal::decide-policy",
                      midori_view_web_view_navigation_decision_cb, view,
                      "signal::permission-request",
                      midori_view_web_view_permission_request_cb, view,
                      "signal::context-menu",
                      midori_view_web_view_context_menu_cb, view,
                      "signal::create",
                      webkit_web_view_create_web_view_cb, view,
                      #else
                      "signal::notify::load-status",
                      midori_view_web_view_notify_load_status_cb, view,
                      "signal::notify::progress",
                      webkit_web_view_progress_changed_cb, view,
                      "signal::script-alert",
                      midori_view_web_view_script_alert_cb, view,
                      "signal::window-object-cleared",
                      webkit_web_view_window_object_cleared_cb, view,
                      "signal::create-web-view",
                      webkit_web_view_create_web_view_cb, view,
                      "signal-after::mime-type-policy-decision-requested",
                      webkit_web_view_mime_type_decision_cb, view,
                      "signal::print-requested",
                      midori_view_web_view_print_requested_cb, view,
                      "signal-after::load-error",
                      webkit_web_view_load_error_cb, view,
                      "signal::navigation-policy-decision-requested",
                      midori_view_web_view_navigation_decision_cb, view,
                      "signal::resource-request-starting",
                      midori_view_web_view_resource_request_cb, view,
                      "signal::database-quota-exceeded",
                      midori_view_web_view_database_quota_exceeded_cb, view,
                      "signal::geolocation-policy-decision-requested",
                      midori_view_web_view_geolocation_decision_cb, view,
                      "signal::notify::icon-uri",
                      midori_web_view_notify_icon_uri_cb, view,
                      "signal::hovering-over-link",
                      webkit_web_view_hovering_over_link_cb, view,
                      "signal::status-bar-text-changed",
                      webkit_web_view_statusbar_text_changed_cb, view,
                      #if WEBKIT_CHECK_VERSION (1, 10, 0)
                      "signal::context-menu",
                      midori_view_web_view_context_menu_cb, view,
                      #endif
                      "signal::console-message",
                      webkit_web_view_console_message_cb, view,
                      "signal::download-requested",
                      midori_view_download_requested_cb, view,
                      #endif

                      "signal::notify::title",
                      webkit_web_view_notify_title_cb, view,
                      "signal::leave-notify-event",
                      midori_view_web_view_leave_notify_event_cb, view,
                      "signal::button-press-event",
                      midori_view_web_view_button_press_event_cb, view,
                      "signal::button-release-event",
                      midori_view_web_view_button_release_event_cb, view,
                      "signal-after::key-press-event",
                      gtk_widget_key_press_event_cb, view,
                      "signal::scroll-event",
                      gtk_widget_scroll_event_cb, view,
                      NULL);

    if (view->settings)
    {
        webkit_web_view_set_settings (WEBKIT_WEB_VIEW (view->web_view), (void*)view->settings);
        #ifndef HAVE_WEBKIT2
        webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (view->web_view),
            katze_object_get_boolean (view->settings, "zoom-text-and-images"));
        #endif
    }

    #ifdef HAVE_WEBKIT2
    if (g_signal_lookup ("web-process-crashed", WEBKIT_TYPE_WEB_VIEW))
        g_signal_connect (view->web_view, "web-process-crashed",
            (GCallback)midori_view_web_view_crashed_cb, view);
    view->scrolled_window = view->web_view;
    WebKitWebContext* context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (view->web_view));
    webkit_web_context_register_uri_scheme (context,
        "res", midori_view_uri_scheme_res, NULL, NULL);
    webkit_web_context_register_uri_scheme (context,
        "stock", midori_view_uri_scheme_res, NULL, NULL);
    g_signal_connect (context, "download-started",
        G_CALLBACK (midori_view_download_started_cb), view);
    #endif

    #if GTK_CHECK_VERSION(3, 2, 0)
    view->overlay = gtk_overlay_new ();
    gtk_widget_show (view->overlay);
    gtk_container_add (GTK_CONTAINER (view->overlay), view->scrolled_window);
    gtk_box_pack_start (GTK_BOX (view), view->overlay, TRUE, TRUE, 0);

    /* Overlays must be created before showing GtkOverlay as of GTK+ 3.2 */
    {
    GtkWidget* frame = gtk_frame_new (NULL);
    gtk_widget_set_no_show_all (frame, TRUE);
    view->overlay_label = gtk_label_new (NULL);
    gtk_widget_show (view->overlay_label);
    gtk_container_add (GTK_CONTAINER (frame), view->overlay_label);
    gtk_widget_set_halign (frame, GTK_ALIGN_START);
    gtk_widget_set_valign (frame, GTK_ALIGN_END);
    gtk_overlay_add_overlay (GTK_OVERLAY (view->overlay), frame);
    /* Enable enter-notify-event signals */
    gtk_widget_add_events (view->overlay, GDK_ENTER_NOTIFY_MASK);
    g_signal_connect (view->overlay, "enter-notify-event",
        G_CALLBACK (midori_view_overlay_frame_enter_notify_event_cb), frame);
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

    #ifndef HAVE_WEBKIT2
    gtk_container_add (GTK_CONTAINER (view->scrolled_window), view->web_view);

    gpointer inspector = webkit_web_view_get_inspector ((WebKitWebView*)view->web_view);
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
    #endif
    gtk_widget_show_all (view->scrolled_window);
    return object;
}

static void
midori_view_add_version (GString* markup,
                         gboolean html,
                         gchar*   text)
{
    if (html)
        g_string_append (markup, "<tr><td>");
    g_string_append (markup, text);
    if (html)
        g_string_append (markup, "</td></tr>");
    else
        g_string_append_c (markup, '\n');
    g_free (text);
}

void
midori_view_list_versions (GString* markup,
                           gboolean html)
{
    midori_view_add_version (markup, html, g_strdup_printf ("%s %s (%s) %s",
        g_get_application_name (), PACKAGE_VERSION, midori_app_get_name (NULL), gdk_get_program_class ()));
    midori_view_add_version (markup, html, g_strdup_printf ("GTK+ %s (%u.%u.%u)\tGlib %s (%u.%u.%u)",
        GTK_VERSION, gtk_major_version, gtk_minor_version, gtk_micro_version,
        GIO_VERSION, glib_major_version, glib_minor_version, glib_micro_version));
#ifndef HAVE_WEBKIT2
    midori_view_add_version (markup, html, g_strdup_printf ("WebKitGTK+ %s (%u.%u.%u)\tlibSoup %s",
        WEBKIT_VERSION, webkit_major_version (), webkit_minor_version (), webkit_micro_version (),
#else
    midori_view_add_version (markup, html, g_strdup_printf ("WebKit2GTK+ %s (%u.%u.%u)\tlibSoup %s",
        WEBKIT_VERSION, webkit_get_major_version (), webkit_get_minor_version (), webkit_get_micro_version (),
#endif
        LIBSOUP_VERSION));
    midori_view_add_version (markup, html, g_strdup_printf ("cairo %s (%s)\tlibnotify %s",
        CAIRO_VERSION_STRING, cairo_version_string (),
        LIBNOTIFY_VERSION));
    midori_view_add_version (markup, html, g_strdup_printf ("gcr %s\tgranite %s",
        GCR_VERSION, GRANITE_VERSION));
}

#ifdef HAVE_WEBKIT2
static void
midori_view_get_plugins_cb (GObject*      object,
                            GAsyncResult* result,
                            MidoriView*   view)
{
    GList* plugins = webkit_web_context_get_plugins_finish (WEBKIT_WEB_CONTEXT (object), result, NULL);
    g_object_set_data (object, "nsplugins", plugins);
    midori_view_reload (view, FALSE);
}
#endif

void
midori_view_list_plugins (MidoriView* view,
                          GString*    ns_plugins,
                          gboolean    html)
{
    if (!midori_web_settings_has_plugin_support ())
        return;

    if (html)
        g_string_append (ns_plugins, "<br><h2>Netscape Plugins:</h2>");
    else
        g_string_append_c (ns_plugins, '\n');

    #ifdef HAVE_WEBKIT2
    WebKitWebContext* context = webkit_web_context_get_default ();
    GList* plugins = g_object_get_data (G_OBJECT (context), "nsplugins");
    if (plugins == NULL)
    {
        midori_view_add_version (ns_plugins, html, g_strdup ("…"));
        webkit_web_context_get_plugins (context, NULL, (GAsyncReadyCallback)midori_view_get_plugins_cb, view);
    }
    else
        for (; plugins != NULL; plugins = g_list_next (plugins))
        {
            if (!midori_web_settings_skip_plugin (webkit_plugin_get_path (plugins->data)))
                midori_view_add_version (ns_plugins, html, g_strdup_printf ("%s\t%s",
                    webkit_plugin_get_name (plugins->data),
                    html ? webkit_plugin_get_description (plugins->data) : ""));
        }
    #else
    WebKitWebPluginDatabase* pdb = webkit_get_web_plugin_database ();
    GSList* plugins = webkit_web_plugin_database_get_plugins (pdb);
    GSList* plugin = plugins;
    for (; plugin != NULL; plugin = g_slist_next (plugin))
    {
        if (midori_web_settings_skip_plugin (webkit_web_plugin_get_path (plugin->data)))
            continue;
        midori_view_add_version (ns_plugins, html, g_strdup_printf ("%s\t%s",
            webkit_web_plugin_get_name (plugin->data),
            html ? webkit_web_plugin_get_description (plugin->data) : ""));
    }
    webkit_web_plugin_database_plugins_list_free (plugins);
    #endif
}

void
midori_view_list_video_formats (MidoriView* view,
                                GString*    formats,
                                gboolean    html)
{
#ifndef HAVE_WEBKIT2
    WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    JSContextRef js_context = webkit_web_frame_get_global_context (web_frame);
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
    midori_view_add_version (formats, html, g_strdup_printf ("Video Formats %s", value));
    g_free (value);
#endif
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
    g_return_if_fail (MIDORI_IS_VIEW (view));
    g_return_if_fail (uri != NULL);

    if (!gtk_widget_get_parent (GTK_WIDGET (view)))
        g_warning ("Calling %s() before adding the view to a browser. This "
                   "breaks extensions that monitor page loading.", G_STRFUNC);

    midori_uri_recursive_fork_protection (uri, TRUE);

    if (!midori_debug ("unarmed"))
    {
        gboolean handled = FALSE;
        if (g_str_has_prefix (uri, "about:"))
            g_signal_emit (view, signals[ABOUT_CONTENT], 0, uri, &handled);

        if (handled)
        {
            midori_tab_set_uri (MIDORI_TAB (view), uri);
            midori_tab_set_special (MIDORI_TAB (view), TRUE);
            katze_item_set_meta_integer (view->item, "delay", MIDORI_DELAY_UNDELAYED);
            katze_item_set_uri (view->item, midori_tab_get_uri (MIDORI_TAB (view)));
            return;
        }

        if (katze_item_get_meta_integer (view->item, "delay") == MIDORI_DELAY_DELAYED)
        {
            midori_tab_set_uri (MIDORI_TAB (view), uri);
            midori_tab_set_special (MIDORI_TAB (view), TRUE);
            katze_item_set_meta_integer (view->item, "delay", MIDORI_DELAY_PENDING_UNDELAY);
            midori_view_display_error (view, NULL, "stock://dialog/network-idle", NULL,
                _("Page loading delayed:"),
                _("Loading delayed either due to a recent crash or startup preferences."),
                NULL,
                _("Load Page"),
                NULL);
        }
        else if (g_str_has_prefix (uri, "javascript:"))
        {
            gchar* exception = NULL;
            gboolean result = midori_view_execute_script (view, &uri[11], &exception);
            if (!result)
            {
                sokoke_message_dialog (GTK_MESSAGE_ERROR, "javascript:",
                                       exception, FALSE);
                g_free (exception);
            }
        }
        else
        {
            if (sokoke_external_uri (uri))
            {
                g_signal_emit_by_name (view, "open-uri", uri, &handled);
                if (handled)
                    return;
            }

            midori_tab_set_uri (MIDORI_TAB (view), uri);
            katze_item_set_uri (view->item, midori_tab_get_uri (MIDORI_TAB (view)));
            katze_assign (view->title, NULL);
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

    return midori_tab_is_blank (MIDORI_TAB (view));
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
 * Return value: (transfer none): a #GdkPixbuf, or %NULL
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
 * Note that "about:blank" and "about:dial" are represented as "".
 *
 * You can assume that the string is not %NULL.
 *
 * Return value: an URI string
 **/
const gchar*
midori_view_get_display_uri (MidoriView* view)
{
    const gchar* uri;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), "");

    uri = midori_tab_get_uri (MIDORI_TAB (view));
    /* Something in the stack tends to turn "" into "about:blank".
       Yet for practical purposes we prefer "".  */
    if (!strcmp (uri, "about:blank")
     || !strcmp (uri, "about:dial")
     || !strcmp (uri, "about:new")
     || !strcmp (uri, "about:private"))
        return "";

    return uri;
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
#ifndef HAVE_WEBKIT2
    WebKitDOMDocument* doc;
    WebKitDOMDOMWindow* window;
    WebKitDOMDOMSelection* selection;
    WebKitDOMRange* range;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);


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

    if (view->selected_text && *view->selected_text)
        return TRUE;
    else
        return FALSE;
#else
    return FALSE;
#endif
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
        return g_strstrip (view->selected_text);
    return NULL;
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
 * Return value: (transfer none): the proxy #GtkMenuItem
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

/**
 * midori_view_duplicate
 * @view: a #MidoriView
 *
 * Create a new #MidoriView from an existing one by using
 * the item of the old view.
 *
 * Return value: (transfer full): the new #MidoriView
 **/
GtkWidget*
midori_view_duplicate (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    MidoriNewView where = MIDORI_NEW_VIEW_TAB;
    GtkWidget* new_view = midori_view_new_with_item (view->item, view->settings);
    g_signal_emit (view, signals[NEW_VIEW], 0, new_view, where, TRUE);
    midori_view_set_uri (MIDORI_VIEW (new_view), midori_tab_get_uri (MIDORI_TAB (view)));
    return new_view;
}

/**
 * midori_view_get_tab_menu:
 * @view: a #MidoriView
 *
 * Retrieves a menu that is typically shown when right-clicking
 * a tab label or equivalent representation.
 *
 * Return value: (transfer full): a #GtkMenu
 *
 * Since: 0.1.8
 * Deprecated: 0.5.7: Use MidoriNotebook API instead.
 **/
GtkWidget*
midori_view_get_tab_menu (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    GtkWidget* notebook = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (view)));
    MidoriContextAction* context_action = midori_notebook_get_tab_context_action (MIDORI_NOTEBOOK (notebook), MIDORI_TAB (view));
    GtkMenu* menu = midori_context_action_create_menu (context_action, NULL, FALSE);
    g_object_unref (context_action);
    return GTK_WIDGET (menu);
}

/**
 * midori_view_get_proxy_tab_label:
 * @view: a #MidoriView
 *
 * Retrieves a proxy tab label that is typically used when
 * adding the view to a notebook.
 *
 * Return value: (transfer none): the proxy #GtkEventBox
 *
 * Deprecated: 0.5.7: Don't use this label.
 **/
GtkWidget*
midori_view_get_proxy_tab_label (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    if (!view->tab_label)
    {
        view->tab_label = gtk_label_new ("dummy");
        gtk_widget_show (view->tab_label);
    }
    return view->tab_label;
}

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
 * Return value: (transfer none): the proxy #KatzeItem
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
        || !g_str_has_prefix (midori_tab_get_mime_type (MIDORI_TAB (view)), "image/"));
}

gboolean
midori_view_can_zoom_out (MidoriView* view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    return view->web_view != NULL
        && (katze_object_get_boolean (view->settings, "zoom-text-and-images")
        || !g_str_has_prefix (midori_tab_get_mime_type (MIDORI_TAB (view)), "image/"));
}

#ifdef HAVE_WEBKIT2
static void
midori_web_resource_get_data_cb (WebKitWebResource *resource,
                          GAsyncResult *result,
                          GOutputStream *output_stream)
{
    guchar *data;
    gsize data_length;
    GInputStream *input_stream;
    GError *error = NULL;

    data = webkit_web_resource_get_data_finish (resource, result, &data_length, &error);
    if (!data) {
        g_printerr ("Failed to save page: %s", error->message);
        g_error_free (error);
        g_object_unref (output_stream);

        return;
    }

    input_stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
    g_output_stream_splice_async (output_stream, input_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL, NULL, NULL);
    g_object_unref (input_stream);
    g_object_unref (output_stream);
}

static void
midori_web_view_save_main_resource_cb (GFile *file,
                                     GAsyncResult *result,
                                     WebKitWebView *view)
{
    GFileOutputStream *output_stream;
    WebKitWebResource *resource;
    GError *error = NULL;

    output_stream = g_file_replace_finish (file, result, &error);
    if (!output_stream) {
        g_printerr ("Failed to save page: %s", error->message);
        g_error_free (error);
        return;
    }

    resource = webkit_web_view_get_main_resource (view);
    webkit_web_resource_get_data (resource, NULL,
                                (GAsyncReadyCallback)midori_web_resource_get_data_cb,
                                output_stream);                                
} 
#endif

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
midori_view_save_source (MidoriView*  view,
                         const gchar* uri,
                         const gchar* outfile,
                         gboolean     use_dom)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);
    
    if (uri == NULL)
        uri = midori_view_get_display_uri (view);

    if (g_str_has_prefix (uri, "file:///"))
        return g_filename_from_uri (uri, NULL, NULL);

#ifndef HAVE_WEBKIT2
    WebKitWebFrame *frame;
    WebKitWebDataSource *data_source;
    const GString *data;
    gchar* unique_filename;
    gint fd;
    FILE* fp;
    size_t ret;

    frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));

    if (use_dom)
    {
        WebKitDOMDocument* doc;

        #if WEBKIT_CHECK_VERSION (1, 9, 5)
        doc = webkit_web_frame_get_dom_document (frame);
        #else
        doc = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view->web_view));
        #endif

        WebKitDOMElement* root = webkit_dom_document_query_selector (doc, ":root", NULL);
        const gchar* content = webkit_dom_html_element_get_outer_html (WEBKIT_DOM_HTML_ELEMENT (root));
        data = g_string_new (content);
    } else {
        data_source = webkit_web_frame_get_data_source (frame);
        data = webkit_web_data_source_get_data (data_source);
    }

    if (!outfile)
    {
        gchar* extension = midori_download_get_extension_for_uri (uri, NULL);
        const gchar* mime_type = midori_tab_get_mime_type (MIDORI_TAB (view));
        unique_filename = g_strdup_printf ("%s/%uXXXXXX%s", midori_paths_get_tmp_dir (),
            g_str_hash (uri), midori_download_fallback_extension (extension, mime_type));
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
#else
    GFile *file;
    char *converted = NULL;
    WebKitWebView * web_view = WEBKIT_WEB_VIEW (view->web_view);
    g_return_val_if_fail (uri,NULL);

    if (!outfile)
        converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);
    else
        converted = g_strdup (outfile);

    file = g_file_new_for_uri (converted);

    if (g_str_has_suffix (uri, ".mht"))
        webkit_web_view_save_to_file (WEBKIT_WEB_VIEW (web_view), file, WEBKIT_SAVE_MODE_MHTML,
                                  NULL, NULL, NULL);
    else
        g_file_replace_async (file, NULL, FALSE,
                          G_FILE_CREATE_REPLACE_DESTINATION | G_FILE_CREATE_PRIVATE,
                          G_PRIORITY_DEFAULT, NULL,
                          (GAsyncReadyCallback)midori_web_view_save_main_resource_cb,
                          web_view);
    g_free (converted);
    g_object_unref (file);
    return converted;
#endif
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

    if (midori_tab_is_blank (MIDORI_TAB (view)))
    {
        /* Duplicate here because the URI pointer might change */
        gchar* uri = g_strdup (midori_tab_get_uri (MIDORI_TAB (view)));
        midori_view_set_uri (view, uri);
        g_free (uri);
    }
    else if (from_cache)
        webkit_web_view_reload (WEBKIT_WEB_VIEW (view->web_view));
    else
        webkit_web_view_reload_bypass_cache (WEBKIT_WEB_VIEW (view->web_view));
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
#ifndef HAVE_WEBKIT2
    g_return_if_fail (MIDORI_IS_VIEW (view));

    webkit_web_view_go_back_or_forward (WEBKIT_WEB_VIEW (view->web_view), steps);
    /* Force the speed dial to kick in if going back to a blank page */
    if (midori_view_is_blank (view))
        midori_view_set_uri (view, "");
#endif
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
#ifndef HAVE_WEBKIT2
    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);

    if (view->web_view)
        return webkit_web_view_can_go_back_or_forward (WEBKIT_WEB_VIEW (view->web_view), steps);
    else
        return FALSE;
#else
    return FALSE;
#endif
}

static gchar*
midori_view_get_related_page (MidoriView*  view,
                              const gchar* rel,
                              const gchar* local)
{
#ifndef HAVE_WEBKIT2
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
#else
    return NULL;
#endif
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

#ifndef HAVE_WEBKIT2
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
#endif

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

    GtkPrintSettings* settings = gtk_print_settings_new ();
    #if GTK_CHECK_VERSION (3, 6, 0)
    gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, midori_view_get_display_title (view));
    #endif

#ifdef HAVE_WEBKIT2
    WebKitPrintOperation* operation = webkit_print_operation_new (WEBKIT_WEB_VIEW (view->web_view));
    webkit_print_operation_set_print_settings (operation, settings);
    g_object_unref (settings);

    if (katze_object_get_boolean (view->settings, "print-without-dialog")) {
        webkit_print_operation_print (operation);
    }
    else {
        webkit_print_operation_run_dialog (operation,
            GTK_WINDOW (midori_browser_get_for_widget (view->web_view)));
    }
    g_object_unref (operation);
#else
    WebKitWebFrame* frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view->web_view));
    GtkPrintOperation* operation = gtk_print_operation_new ();
    gtk_print_operation_set_print_settings (operation, settings);
    g_object_unref (settings);
    gtk_print_operation_set_custom_tab_label (operation, _("Features"));
    gtk_print_operation_set_embed_page_setup (operation, TRUE);
    g_signal_connect (operation, "create-custom-widget",
        G_CALLBACK (midori_view_print_create_custom_widget_cb), view);
    GError* error = NULL;

    if (katze_object_get_boolean (view->settings, "print-without-dialog")) {
        webkit_web_frame_print_full (frame, operation,
            GTK_PRINT_OPERATION_ACTION_PRINT, &error);
    }
    else {
        webkit_web_frame_print_full (frame, operation,
            GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, &error);
    }
    g_object_unref (operation);

    if (error)
    {
        GtkWidget* window = gtk_widget_get_toplevel (GTK_WIDGET (view));
        GtkWidget* dialog = gtk_message_dialog_new (
            gtk_widget_is_toplevel (window) ? GTK_WINDOW (window) : NULL,
            GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "%s", error->message);
        g_error_free (error);

        g_signal_connect_swapped (dialog, "response",
            G_CALLBACK (gtk_widget_destroy), dialog);
        gtk_widget_show (dialog);
    }
#endif
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
        midori_tab_find (MIDORI_TAB (view), text, case_sensitive, forward);
        return;
    }
    #endif
    g_signal_emit_by_name (view, "search-text",
        midori_tab_find (MIDORI_TAB (view), text, case_sensitive, forward), NULL);
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
#ifndef HAVE_WEBKIT2
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
#else
    return FALSE;
#endif
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
 * Returns: (transfer full): a newly allocated #GdkPixbuf
 *
 * Since: 0.2.1
 * Deprecated: 0.5.4
 **/
GdkPixbuf*
midori_view_get_snapshot (MidoriView* view,
                          gint        width,
                          gint        height)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->icon ? g_object_ref (view->icon) : NULL;
}

/**
 * midori_view_get_web_view
 * @view: a #MidoriView
 *
 * Returns: (transfer none): The #WebKitWebView for this view
 *
 * Since: 0.2.5
 * Deprecated: 0.4.8: Use midori_tab_get_web_view() instead.
 **/
GtkWidget*
midori_view_get_web_view        (MidoriView*        view)
{
    g_return_val_if_fail (MIDORI_IS_VIEW (view), NULL);

    return view->web_view;
}

/**
 * midori_view_get_for_widget:
 * @web_view: a #GtkWidget of type #WebKitWebView
 *
 * Determines the MidoriView for the specified #WebkitWebView widget.
 *
 * Return value: (transfer none): a #MidoriView, or %NULL
 *
 * Since 0.4.5
 **/
MidoriView*
midori_view_get_for_widget (GtkWidget* web_view)
{
    g_return_val_if_fail (GTK_IS_WIDGET (web_view), NULL);

    #ifdef HAVE_WEBKIT2
    GtkWidget* scrolled = web_view;
    #else
    GtkWidget* scrolled = gtk_widget_get_parent (web_view);
    #endif
    #if GTK_CHECK_VERSION(3, 2, 0)
    GtkWidget* overlay = gtk_widget_get_parent (scrolled);
    GtkWidget* view = gtk_widget_get_parent (overlay);
    #else
    GtkWidget* view = gtk_widget_get_parent (scrolled);
    #endif
    return MIDORI_VIEW (view);
}
/**
 * midori_view_set_colors:
 * @view: a #MidoriView
 * @fg_color: a #GdkColor, or %NULL
 * @bg_color: a #GdkColor, or %NULL
 *
 * Sets colors on the label.
 *
 * Deprecated: 0.5.7: Use fg_color/ bg_color on Midori.Tab.
 **/
void
midori_view_set_colors (MidoriView* view,
                        GdkColor*   fg_color,
                        GdkColor*   bg_color)
{
    midori_tab_set_fg_color (MIDORI_TAB (view), fg_color);
    midori_tab_set_bg_color (MIDORI_TAB (view), bg_color);
}
