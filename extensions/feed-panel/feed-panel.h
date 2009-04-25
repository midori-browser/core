/*
 Copyright (C) 2009 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __FEED_PANEL_H__
#define __FEED_PANEL_H__

#include <midori/midori.h>

G_BEGIN_DECLS

#define FEED_TYPE_PANEL \
    (feed_panel_get_type ())
#define FEED_PANEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), FEED_TYPE_PANEL, FeedPanel))
#define FEED_PANEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), FEED_TYPE_PANEL, FeedPanelClass))
#define FEED_IS_PANEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FEED_TYPE_PANEL))
#define FEED_IS_PANEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), FEED_TYPE_PANEL))
#define FEED_PANEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), FEED_TYPE_PANEL, FeedPanelClass))

typedef struct _FeedPanel                FeedPanel;
typedef struct _FeedPanelClass           FeedPanelClass;

void
feed_panel_add_feeds                     (FeedPanel*  panel,
                                          KatzeItem* feed);

void
feed_panel_set_editable                  (FeedPanel* panel,
                                          gboolean   editable);

GType
feed_panel_get_type                      (void);

GtkWidget*
feed_panel_new                           (void);

G_END_DECLS

#endif /* __FEED_PANEL_H__ */
