/*
 Copyright (C) 2009 Dale Whittaker <dale@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "feed-panel.h"
#include "feed-atom.h"
#include "feed-rss.h"

#include <midori/midori.h>

#define EXTENSION_NAME "Feed Panel"

#define feed_get_flags(feed) \
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT ((feed)), "flags"))

#define feed_set_flags(feed, flags) \
    g_object_set_data (G_OBJECT ((feed)), "flags", \
                       GINT_TO_POINTER ((flags)))

#define feed_has_flags(feed, flags) \
    ((flags) & feed_get_flags ((feed)))

#define feed_add_flags(feed, flags) \
    feed_set_flags ((feed), (feed_get_flags ((feed)) | (flags)))

#define feed_remove_flags(feed, flags) \
    feed_set_flags ((feed), (feed_get_flags ((feed)) & ~(flags)))

typedef struct
{
    MidoriBrowser* browser;
    MidoriExtension* extension;
    GtkWidget* panel;
    KatzeArray* feeds;
    GSList* parsers;

    guint source_id;
    gboolean is_running;

} FeedPrivate;

typedef struct
{
    MidoriExtension* extension;
    GSList* parsers;
    KatzeArray* feed;

} FeedNetPrivate;

enum
{
    FEED_READ = 1,
    FEED_REMOVE
};

static void
feed_app_add_browser_cb (MidoriApp*       app,
                         MidoriBrowser*   browser,
                         MidoriExtension* extension);

static gboolean
secondary_icon_released_cb (GtkAction*     action,
                            GtkWidget*     widget,
                            FeedPrivate*   priv);

static void
feed_deactivate_cb (MidoriExtension* extension,
                    FeedPrivate*     priv)
{
    if (priv)
    {
        MidoriApp* app = midori_extension_get_app (extension);
        GtkActionGroup* action_group;
        GtkAction* action;

        action_group = midori_browser_get_action_group (priv->browser);
        action = gtk_action_group_get_action (action_group, "Location");
        g_signal_handlers_disconnect_by_func (action,
                secondary_icon_released_cb, priv);

        g_signal_handlers_disconnect_by_func (app,
                feed_app_add_browser_cb, extension);
        g_signal_handlers_disconnect_by_func (extension,
                feed_deactivate_cb, priv);

        if (priv->source_id)
            g_source_remove (priv->source_id);
        g_slist_foreach (priv->parsers, (GFunc)g_free, NULL);
        g_slist_free (priv->parsers);
        if (priv->feeds)
            g_object_unref (priv->feeds);
        gtk_widget_destroy (priv->panel);
        g_free (priv);
    }
}

static KatzeArray*
feed_add_item (KatzeArray*  feeds,
               const gchar* uri)
{
        if (katze_array_find_token (feeds, uri))
        {
            GtkWidget* dialog;

            dialog = gtk_message_dialog_new (
                NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                _("Error"));
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                _("Feed '%s' already exists"), uri);
            gtk_window_set_title (GTK_WINDOW (dialog), EXTENSION_NAME);
            gtk_widget_show (dialog);
            g_signal_connect_swapped (dialog, "response",
                G_CALLBACK (gtk_widget_destroy), dialog);
            return NULL;
        }
        else
        {
            KatzeArray* child;

            KatzeArray* feed = katze_array_new (KATZE_TYPE_ARRAY);
            child = katze_array_new (KATZE_TYPE_ITEM);
            katze_item_set_uri (KATZE_ITEM (feed), uri);
            katze_item_set_token (KATZE_ITEM (feed), uri);
            katze_item_set_uri (KATZE_ITEM (child), uri);
            katze_array_add_item (feeds, feed);
            katze_array_add_item (feed, child);
            return feed;
        }
}

static void
feed_save_items (MidoriExtension* extension,
                 KatzeArray*      feed)
{
    KatzeItem* item;
    gchar** sfeeds;
    gint i, n;

    g_return_if_fail (KATZE_IS_ARRAY (feed));

    n = katze_array_get_length (feed);
    sfeeds = g_new (gchar*, n + 1);

    i = 0;
    KATZE_ARRAY_FOREACH_ITEM (item, feed)
    {
        sfeeds[i] = (gchar*) katze_item_get_uri (KATZE_ITEM (item));
        i++;
    }
    sfeeds[n] = NULL;

    midori_extension_set_string_list (extension, "feeds", sfeeds, n);
    g_free (sfeeds);
}

static void
feed_handle_net_error (FeedNetPrivate* netpriv,
                       const gchar*    msg)
{
    KatzeItem* child;
    const gchar* uri;
    gint n;

    n = katze_array_get_length (netpriv->feed);
    g_assert (n == 1);
    child = katze_array_get_nth_item (netpriv->feed, 0);
    g_assert (KATZE_IS_ARRAY (child));

    uri  = katze_item_get_uri (KATZE_ITEM (netpriv->feed));
    katze_item_set_name (child, uri);
    katze_item_set_text (child, msg);
    katze_item_set_uri (child, NULL);
    feed_remove_flags (netpriv->feed, FEED_READ);
}

static gboolean
feed_status_cb (KatzeNetRequest*  request,
                FeedNetPrivate*   netpriv)
{
    if (request->status == KATZE_NET_FAILED ||
        request->status == KATZE_NET_NOT_FOUND)
    {
        gchar* msg;

        msg = g_strdup_printf (_("Error loading feed '%s'"),
                        katze_item_get_uri (KATZE_ITEM (netpriv->feed)));
        feed_handle_net_error (netpriv, msg);
        g_free (msg);

        return FALSE;
    }
    return TRUE;
}

static void
feed_transfer_cb (KatzeNetRequest* request,
                  FeedNetPrivate*  netpriv)
{
    GError* error;

    if (request->status == KATZE_NET_MOVED)
        return;

    g_return_if_fail (KATZE_IS_ARRAY (netpriv->feed));

    error = NULL;

    if (request->data)
    {
        KatzeArray* item;
        const gchar* uri;
        gint n;

        n = katze_array_get_length (netpriv->feed);
        g_assert (n == 1);
        item = katze_array_get_nth_item (netpriv->feed, 0);
        g_assert (KATZE_IS_ARRAY (item));
        uri = katze_item_get_uri (KATZE_ITEM (netpriv->feed));
        katze_item_set_uri (KATZE_ITEM (item), uri);

        if (!parse_feed (request->data, request->length,
             netpriv->parsers, item, &error))
        {
            feed_handle_net_error (netpriv, error->message);
            g_error_free (error);
        }

        if (feed_has_flags (netpriv->feed, FEED_REMOVE))
        {
            KatzeArray* parent;

            /* deferred remove */
            parent = katze_item_get_parent (KATZE_ITEM (netpriv->feed));
            katze_array_remove_item (parent, netpriv->feed);
            feed_save_items (netpriv->extension, parent);
        }
        else
        {
            feed_remove_flags (netpriv->feed, FEED_REMOVE);
            feed_remove_flags (netpriv->feed, FEED_READ);
        }
    }

    netpriv->parsers = NULL;
    netpriv->feed = NULL;
    g_free (netpriv);
}

static void
update_feed (FeedPrivate* priv,
             KatzeItem*   feed)
{
    if (!(feed_has_flags (feed, FEED_READ)))
    {
        FeedNetPrivate* netpriv;

        feed_add_flags (feed, FEED_READ);
        netpriv = g_new0 (FeedNetPrivate, 1);
        netpriv->parsers = priv->parsers;
        netpriv->extension = priv->extension;
        netpriv->feed = KATZE_ARRAY (feed);

        katze_net_load_uri (NULL,
                            katze_item_get_uri (feed),
                            (KatzeNetStatusCb) feed_status_cb,
                            (KatzeNetTransferCb) feed_transfer_cb,
                            netpriv);
    }
}

static gboolean
update_feeds (FeedPrivate* priv)
{
    KatzeItem* feed;
    gint i;
    gint n;

    if (!priv->is_running)
    {
        priv->is_running = TRUE;
        n = katze_array_get_length (priv->feeds);

        for (i = 0; i < n; i++)
        {
            feed = katze_array_get_nth_item (priv->feeds, i);
            update_feed (priv, feed);
        }
    }
    priv->is_running = FALSE;
    return TRUE;
}

static gboolean
secondary_icon_released_cb (GtkAction*     action,
                            GtkWidget*     widget,
                            FeedPrivate*   priv)
{
    GtkWidget* view;

    g_assert (KATZE_IS_ARRAY (priv->feeds));

    if (gtk_window_get_focus (GTK_WINDOW (priv->browser)) == widget)
        return FALSE;

    if ((view = midori_browser_get_current_tab (priv->browser)))
    {
        const gchar* uri;

        uri = g_object_get_data (G_OBJECT (view), "news-feeds");
        if (uri && *uri)
        {
            KatzeArray* feed;

            if ((feed = feed_add_item (priv->feeds, uri)))
            {
                MidoriPanel* panel = katze_object_get_object (priv->browser, "panel");
                gint i = midori_panel_page_num (panel, priv->panel);
                midori_panel_set_current_page (panel, i);
                gtk_widget_show (GTK_WIDGET (panel));
                g_object_unref (panel);
                feed_save_items (priv->extension, priv->feeds);
                update_feed (priv, KATZE_ITEM (feed));
                return TRUE;
            }
        }
    }

    return FALSE;
}

static void
panel_add_feed_cb (FeedPanel*   panel,
                   FeedPrivate* priv)
{
    GtkWidget* dialog;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* label;
    GtkWidget* entry;

    dialog = gtk_dialog_new_with_buttons (
            _("New feed"), GTK_WINDOW (priv->browser),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
            NULL);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_ADD);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER(gtk_dialog_get_content_area( GTK_DIALOG (dialog))), 5);
    sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Address:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_entry_set_text (GTK_ENTRY (entry), "");
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox);
    gtk_widget_show_all (hbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        const gchar* uri;

        g_assert (KATZE_IS_ARRAY (priv->feeds));

        uri = gtk_entry_get_text (GTK_ENTRY (entry));
        if (uri && *uri)
        {
            KatzeArray* feed;

            feed = feed_add_item (priv->feeds, uri);
            if (feed)
            {
                feed_save_items (priv->extension, priv->feeds);
                update_feed (priv, KATZE_ITEM (feed));
            }
        }
    }
    gtk_widget_destroy (dialog);
}

static void
panel_remove_feed_cb (FeedPanel*   panel,
                      KatzeItem*   item,
                      FeedPrivate* priv)
{
    KatzeArray* feed;

    feed = katze_item_get_parent (item);

    g_assert (KATZE_IS_ARRAY (priv->feeds));
    g_assert (KATZE_IS_ARRAY (feed));

    if (feed_has_flags (feed, FEED_READ))
        feed_add_flags (feed, FEED_REMOVE);
    else
    {
        feed_add_flags (feed, FEED_READ);
        katze_array_remove_item (priv->feeds, feed);
        feed_save_items (priv->extension, priv->feeds);
    }
}

static void
feed_app_add_browser_cb (MidoriApp*       app,
                         MidoriBrowser*   browser,
                         MidoriExtension* extension)
{
    GtkWidget* panel;
    GtkWidget* addon;
    GtkActionGroup* action_group;
    GtkAction* action;
    KatzeArray* feeds;
    KatzeArray* feed;
    FeedPrivate* priv;
    gchar** sfeeds;
    gsize i;
    gsize n;

    priv = g_new0 (FeedPrivate, 1);

    panel = katze_object_get_object (browser, "panel");
    addon = feed_panel_new ();
    gtk_widget_show (addon);
    midori_panel_append_page (MIDORI_PANEL (panel), MIDORI_VIEWABLE (addon));
    g_object_unref (panel);

    feeds = katze_array_new (KATZE_TYPE_ARRAY);
    feed_panel_add_feeds (FEED_PANEL (addon), KATZE_ITEM (feeds));

    priv->extension = extension;
    priv->browser = browser;
    priv->panel = addon;
    priv->feeds = feeds;
    priv->parsers = g_slist_prepend (priv->parsers, atom_init_parser ());
    priv->parsers = g_slist_prepend (priv->parsers, rss_init_parser ());

    sfeeds = midori_extension_get_string_list (extension, "feeds", &n);
    if (sfeeds != NULL)
    for (i = 0; i < n; i++)
    {
        if (sfeeds[i])
        {
            feed = feed_add_item (feeds, sfeeds[i]);
            if (feed)
                update_feed (priv, KATZE_ITEM (feed));
        }
    }
    action_group = midori_browser_get_action_group (browser);
    action = gtk_action_group_get_action (action_group, "Location");

    g_signal_connect (addon, "add-feed",
        G_CALLBACK (panel_add_feed_cb), priv);
    g_signal_connect (addon, "remove-feed",
        G_CALLBACK (panel_remove_feed_cb), priv);
    g_signal_connect (action, "secondary-icon-released",
        G_CALLBACK (secondary_icon_released_cb), priv);
    g_signal_connect (extension, "deactivate",
        G_CALLBACK (feed_deactivate_cb), priv);

    priv->source_id = midori_timeout_add_seconds (
        600, (GSourceFunc) update_feeds, priv, NULL);
}

static void
feed_activate_cb (MidoriExtension* extension,
                  MidoriApp*       app)
{
    KatzeArray* browsers;
    MidoriBrowser* browser;

    browsers = katze_object_get_object (app, "browsers");
    KATZE_ARRAY_FOREACH_ITEM (browser, browsers)
        feed_app_add_browser_cb (app, browser, extension);
    g_object_unref (browsers);

    g_signal_connect (app, "add-browser",
        G_CALLBACK (feed_app_add_browser_cb), extension);
}

MidoriExtension*
extension_init (void)
{
    MidoriExtension* extension;

    extension = g_object_new (MIDORI_TYPE_EXTENSION,
        "name", _("Feed Panel"),
        "description", _("Read Atom/ RSS feeds"),
        "version", "0.1" MIDORI_VERSION_SUFFIX,
        "authors", "Dale Whittaker <dayul@users.sf.net>",
        NULL);

    midori_extension_install_string_list (extension, "feeds", NULL, 0);

    g_signal_connect (extension, "activate",
        G_CALLBACK (feed_activate_cb), NULL);

    return extension;
}
