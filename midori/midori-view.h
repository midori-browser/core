/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_VIEW_H__
#define __MIDORI_VIEW_H__

#include "midori-websettings.h"
#include "midori-core.h"

#include <katze/katze.h>

#ifdef HAVE_GRANITE
    #include <granite/granite.h>
#endif

G_BEGIN_DECLS

typedef enum
{
    MIDORI_DELAY_UNDELAYED = -1, /* The view is in a regular undelayed state */
    MIDORI_DELAY_DELAYED = 1, /* The view is delayed but has not displayed any indication of such */
    MIDORI_DELAY_PENDING_UNDELAY = -2 /* The view is delayed and showing a message asking to be undelayed */
} MidoriDelay;

#define MIDORI_TYPE_VIEW \
    (midori_view_get_type ())

typedef enum
{
    MIDORI_DOWNLOAD_CANCEL,
    MIDORI_DOWNLOAD_OPEN,
    MIDORI_DOWNLOAD_SAVE,
    MIDORI_DOWNLOAD_SAVE_AS,
    MIDORI_DOWNLOAD_OPEN_IN_VIEWER,
} MidoriDownloadType;

#define MIDORI_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_VIEW, MidoriView))
#define MIDORI_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_VIEW, MidoriViewClass))
#define MIDORI_IS_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_VIEW))
#define MIDORI_IS_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_VIEW))
#define MIDORI_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_VIEW, MidoriViewClass))

typedef struct _MidoriView                MidoriView;
typedef struct _MidoriViewClass           MidoriViewClass;

GType
midori_view_get_type                   (void) G_GNUC_CONST;

GtkWidget*
midori_view_new_with_title             (const gchar*       title,
                                        MidoriWebSettings* settings,
                                        gboolean           append);

GtkWidget*
midori_view_new_with_item              (KatzeItem*         item,
                                        MidoriWebSettings* settings);

void
midori_view_set_settings               (MidoriView*        view,
                                        MidoriWebSettings* settings);

gdouble
midori_view_get_progress               (MidoriView*        view);

MidoriLoadStatus
midori_view_get_load_status            (MidoriView*        view);

void
midori_view_set_uri                    (MidoriView*        view,
                                        const gchar*       uri);

void
midori_view_set_html                   (MidoriView*        view,
                                        const gchar*       data,
                                        const gchar*       uri,
                                        void*              web_frame);

void
midori_view_set_overlay_text           (MidoriView*        view,
                                        const gchar*       text);

gboolean
midori_view_is_blank                   (MidoriView*        view);

const gchar*
midori_view_get_display_uri            (MidoriView*        view);

const gchar*
midori_view_get_display_title          (MidoriView*        view);

GdkPixbuf*
midori_view_get_icon                   (MidoriView*        view);

const gchar*
midori_view_get_icon_uri               (MidoriView*        view);

const gchar*
midori_view_get_link_uri               (MidoriView*        view);

gboolean
midori_view_has_selection              (MidoriView*        view);

const gchar*
midori_view_get_selected_text          (MidoriView*        view);

GtkWidget*
midori_view_get_proxy_menu_item        (MidoriView*        view);

GtkWidget*
midori_view_get_tab_menu               (MidoriView*        view);

GtkWidget*
midori_view_duplicate                  (MidoriView*        view);

PangoEllipsizeMode
midori_view_get_label_ellipsize        (MidoriView*        view);

#ifdef HAVE_GRANITE
GraniteWidgetsTab*
midori_view_get_tab                    (MidoriView*        view);

void
midori_view_set_tab                    (MidoriView*        view,
                                        GraniteWidgetsTab* tab);
#endif

GtkWidget*
midori_view_get_proxy_tab_label        (MidoriView*        view);

KatzeItem*
midori_view_get_proxy_item             (MidoriView*        view);

gfloat
midori_view_get_zoom_level             (MidoriView*        view);

gboolean
midori_view_can_zoom_in                (MidoriView*        view);

gboolean
midori_view_can_zoom_out               (MidoriView*        view);

void
midori_view_set_zoom_level             (MidoriView*        view,
                                        gfloat             zoom_level);

void
midori_view_reload                     (MidoriView*        view,
                                        gboolean           from_cache);

gboolean
midori_view_can_go_back                (MidoriView*        view);

void
midori_view_go_back                    (MidoriView*        view);

gboolean
midori_view_can_go_forward             (MidoriView*        view);

void
midori_view_go_forward                 (MidoriView*        view);


void midori_view_go_back_or_forward    (MidoriView*        view,
                                        gint               steps);

gboolean
midori_view_can_go_back_or_forward     (MidoriView*        view,
                                        gint               steps);

const gchar*
midori_view_get_previous_page          (MidoriView*        view);

const gchar*
midori_view_get_next_page              (MidoriView*        view);

void
midori_view_print                      (MidoriView*        view);

gboolean
midori_view_can_view_source            (MidoriView*        view);

gchar*
midori_view_save_source                (MidoriView*        view,
                                        const gchar*       uri,
                                        const gchar*       outfile);

void
midori_view_search_text                (MidoriView*        view,
                                        const gchar*       text,
                                        gboolean           case_sensitive,
                                        gboolean           forward);

gboolean
midori_view_execute_script             (MidoriView*        view,
                                        const gchar*       script,
                                        gchar**            exception);

GdkPixbuf*
midori_view_get_snapshot               (MidoriView*        view,
                                        gint               width,
                                        gint               height);

GtkWidget*
midori_view_get_web_view               (MidoriView*        view);

MidoriView*
midori_view_get_for_widget             (GtkWidget*         web_view);

void
midori_view_populate_popup             (MidoriView*        view,
                                        GtkWidget*         menu,
                                        gboolean           manual);

GtkWidget*
midori_view_add_info_bar               (MidoriView*        view,
                                        GtkMessageType     message_type,
                                        const gchar*       message,
                                        GCallback          response_cb,
                                        gpointer           user_data,
                                        const gchar*       first_button_text,
                                        ...);

const gchar*
midori_view_fallback_extension         (MidoriView*        view,
                                        const gchar*       extension);

GList*
midori_view_get_resources              (MidoriView*        view);

void
midori_view_list_versions              (GString*           markup,
                                        gboolean           html);

void
midori_view_list_plugins               (MidoriView*        view,
                                        GString*           markup,
                                        gboolean           html);

void
midori_view_set_colors                 (MidoriView*        view,
                                        GdkColor*          fg_color,
                                        GdkColor*          bg_color);

gboolean
midori_view_get_tls_info               (MidoriView*        view,
                                        void*              request,
                                        GTlsCertificate**     tls_cert,
                                        GTlsCertificateFlags* tls_flags,
                                        gchar**               hostname);

G_END_DECLS

#endif /* __MIDORI_VIEW_H__ */
