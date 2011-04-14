/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __KATZE_PREFERENCES_H__
#define __KATZE_PREFERENCES_H__

#include "katze-utils.h"

G_BEGIN_DECLS

#define KATZE_TYPE_PREFERENCES \
    (katze_preferences_get_type ())
#define KATZE_PREFERENCES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_PREFERENCES, KatzePreferences))
#define KATZE_PREFERENCES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_PREFERENCES, KatzePreferencesClass))
#define KATZE_IS_PREFERENCES(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_PREFERENCES))
#define KATZE_IS_PREFERENCES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_PREFERENCES))
#define KATZE_PREFERENCES_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_PREFERENCES, KatzePreferencesClass))

typedef struct _KatzePreferences                KatzePreferences;
typedef struct _KatzePreferencesClass           KatzePreferencesClass;
typedef struct _KatzePreferencesPrivate         KatzePreferencesPrivate;

struct _KatzePreferences
{
    GtkDialog parent_instance;

    KatzePreferencesPrivate* priv;
};

struct _KatzePreferencesClass
{
    GtkDialogClass parent_class;
};

GType
katze_preferences_get_type               (void) G_GNUC_CONST;

GtkWidget*
katze_preferences_new                    (GtkWindow*          parent);

GtkWidget*
katze_preferences_add_category           (KatzePreferences* preferences,
                                          const gchar*      label,
                                          const gchar*      icon);

void
katze_preferences_add_group              (KatzePreferences* preferences,
                                          const gchar*      label);

void
katze_preferences_add_widget             (KatzePreferences* preferences,
                                          GtkWidget*        widget,
                                          const gchar*      type);

G_END_DECLS

#endif /* __KATZE_PREFERENCES_H__ */
