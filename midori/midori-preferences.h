/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_PREFERENCES_H__
#define __MIDORI_PREFERENCES_H__

#include "midori-app.h"
#include "midori-websettings.h"

#include <katze/katze.h>

G_BEGIN_DECLS

#define MIDORI_TYPE_PREFERENCES \
    (midori_preferences_get_type ())
#define MIDORI_PREFERENCES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_PREFERENCES, MidoriPreferences))
#define MIDORI_PREFERENCES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_PREFERENCES, MidoriPreferencesClass))
#define MIDORI_IS_PREFERENCES(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_PREFERENCES))
#define MIDORI_IS_PREFERENCES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_PREFERENCES))
#define MIDORI_PREFERENCES_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_PREFERENCES, MidoriPreferencesClass))

typedef struct _MidoriPreferences                MidoriPreferences;
typedef struct _MidoriPreferencesClass           MidoriPreferencesClass;

struct _MidoriPreferencesClass
{
    KatzePreferencesClass parent_class;
};

GType
midori_preferences_get_type               (void) G_GNUC_CONST;

GtkWidget*
midori_preferences_new              (GtkWindow*         parent,
                                     MidoriWebSettings* settings);

void
midori_preferences_set_settings     (MidoriPreferences* preferences,
                                     MidoriWebSettings* settings);

void
midori_preferences_add_privacy_category (KatzePreferences*  preferences,
                                         MidoriWebSettings* settings);

void
midori_preferences_add_extension_category (KatzePreferences*  preferences,
                                           MidoriApp*         app);

G_END_DECLS

#endif /* __MIDORI_PREFERENCES_H__ */
