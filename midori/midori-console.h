/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_CONSOLE_H__
#define __MIDORI_CONSOLE_H__

#include <gtk/gtk.h>

#include <katze/katze.h>

#include "midori-viewable.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_CONSOLE \
    (midori_console_get_type ())
#define MIDORI_CONSOLE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_CONSOLE, MidoriConsole))
#define MIDORI_CONSOLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_CONSOLE, MidoriConsoleClass))
#define MIDORI_IS_CONSOLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_CONSOLE))
#define MIDORI_IS_CONSOLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_CONSOLE))
#define MIDORI_CONSOLE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_CONSOLE, MidoriConsoleClass))

typedef struct _MidoriConsole                MidoriConsole;
typedef struct _MidoriConsoleClass           MidoriConsoleClass;

GType
midori_console_get_type               (void);

GtkWidget*
midori_console_new                    (void);

GtkWidget*
midori_console_get_toolbar            (MidoriViewable*      console);

void
midori_console_add                    (MidoriConsole*       console,
                                       const gchar*         message,
                                       gint                 line,
                                       const gchar*         source_id);

G_END_DECLS

#endif /* __MIDORI_CONSOLE_H__ */
