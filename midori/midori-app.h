/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_APP_H__
#define __MIDORI_APP_H__

#include <katze/katze.h>

#include "midori-browser.h"
#include "midori-websettings.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_APP \
    (midori_app_get_type ())
#define MIDORI_APP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_APP, MidoriApp))
#define MIDORI_APP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_APP, MidoriAppClass))
#define MIDORI_IS_APP(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_APP))
#define MIDORI_IS_APP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_APP))
#define MIDORI_APP_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_APP, MidoriAppClass))

typedef struct _MidoriApp                MidoriApp;
typedef struct _MidoriAppClass           MidoriAppClass;

GType
midori_app_get_type               (void) G_GNUC_CONST;

MidoriApp*
midori_app_new                    (const gchar*       name);

MidoriApp*
midori_app_new_proxy              (MidoriApp*         app);

const gchar*
midori_app_get_name               (MidoriApp*         app);

gboolean
midori_app_get_crashed            (MidoriApp*         app);

void
midori_app_set_instance_is_running(gboolean           is_running);

gboolean
midori_app_instance_is_running    (MidoriApp*         app);

gboolean
midori_app_instance_send_activate (MidoriApp*         app);

gboolean
midori_app_instance_send_new_browser (MidoriApp*      app);

gboolean
midori_app_instance_send_uris     (MidoriApp*         app,
                                   gchar**            uris);

gboolean
midori_app_send_command           (MidoriApp*         app,
                                   gchar**            command);

void
midori_app_add_browser            (MidoriApp*         app,
                                   MidoriBrowser*     browser);

MidoriBrowser*
midori_app_create_browser         (MidoriApp*         app);

MidoriBrowser*
midori_app_get_browser            (MidoriApp*         app);

GList*
midori_app_get_browsers           (MidoriApp*         app);

void
midori_app_quit                   (MidoriApp*         app);

void
midori_app_send_notification      (MidoriApp*         app,
                                   const gchar*       title,
                                   const gchar*       message);

void
midori_app_setup                  (gint               *argc,
                                   gchar**            *argument_vector,
                                   const GOptionEntry *entries);

gboolean
midori_debug                      (const gchar*       token);

void
midori_error                      (const gchar*       format,
                                   ...);

G_END_DECLS

#endif /* __MIDORI_APP_H__ */
