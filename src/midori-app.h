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
#include "midori-trash.h"

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
typedef struct _MidoriAppPrivate         MidoriAppPrivate;
typedef struct _MidoriAppClass           MidoriAppClass;

struct _MidoriApp
{
    GObject parent_instance;

    MidoriAppPrivate* priv;
};

struct _MidoriAppClass
{
    GObjectClass parent_class;

    /* Signals */
    void
    (*add_browser)            (MidoriApp*     app,
                               MidoriBrowser* browser);
    void
    (*quit)                   (MidoriApp* app);
};

GType
midori_app_get_type               (void);

MidoriApp*
midori_app_new                    (void);

MidoriWebSettings*
midori_app_get_web_settings       (MidoriApp* app);

MidoriTrash*
midori_app_get_trash              (MidoriApp* app);

G_END_DECLS

#endif /* __MIDORI_APP_H__ */
