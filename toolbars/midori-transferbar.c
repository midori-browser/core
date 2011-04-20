/*
 Copyright (C) 2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-transferbar.h"

#include "midori-browser.h"
#include "sokoke.h"

#include <glib/gi18n.h>

struct _MidoriTransferbar
{
    GtkToolbar parent_instance;

    GList* infos;
    GtkToolItem* clear;
};

struct _MidoriTransferbarClass
{
    GtkToolbarClass parent_class;
};

G_DEFINE_TYPE (MidoriTransferbar, midori_transferbar, GTK_TYPE_TOOLBAR);

static void
midori_transferbar_class_init (MidoriTransferbarClass* class)
{
    /* Nothing to do */
}

#if WEBKIT_CHECK_VERSION (1, 1, 3)
typedef struct
{
    WebKitDownload* download;
    GtkWidget* button;
    GtkWidget* toolitem;
    MidoriTransferbar* transferbar;
} TransferInfo;

static gboolean
midori_transferbar_info_free (gpointer data)
{
    TransferInfo* info = data;
    MidoriTransferbar* transferbar = info->transferbar;

    transferbar->infos = g_list_remove (transferbar->infos, info);
    g_object_unref (info->download);
    gtk_widget_destroy (info->toolitem);
    g_slice_free (TransferInfo, info);

    if (!transferbar->infos || !g_list_nth_data (transferbar->infos, 0))
        gtk_widget_hide (GTK_WIDGET (transferbar->clear));

    return FALSE;
}

static void
midori_transferbar_button_destroy_cb (GtkWidget*    button,
                                      TransferInfo* info)
{
    g_idle_add (midori_transferbar_info_free, info);
}

static void
midori_transferbar_download_notify_progress_cb (WebKitDownload* download,
                                                GParamSpec*     pspec,
                                                GtkWidget*      progress)
{
    gchar* tooltip;
    gchar* text;
    guint64 size;

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress),
        webkit_download_get_progress (download));
    size = webkit_download_get_current_size (download);

    if (size == webkit_download_get_total_size (download))
    {
        gtk_widget_set_tooltip_text (progress,
            gtk_progress_bar_get_text (GTK_PROGRESS_BAR (progress)));
        return;
    }

    tooltip = midori_download_prepare_tooltip_text (download);
    text = g_strdup_printf ("%s\n%s",
        gtk_progress_bar_get_text (GTK_PROGRESS_BAR (progress)), tooltip);
    gtk_widget_set_tooltip_text (progress, text);

    g_free (tooltip);
    g_free (text);
}

static void
midori_transferbar_download_notify_status_cb (WebKitDownload* download,
                                              GParamSpec*     pspec,
                                              TransferInfo*   info)
{
    GtkWidget* button = info->button;
    GtkWidget* icon;

    switch (webkit_download_get_status (download))
    {
        case WEBKIT_DOWNLOAD_STATUS_FINISHED:
        {
            MidoriBrowser* browser = midori_browser_get_for_widget (button);
            #if WEBKIT_CHECK_VERSION (1, 1, 14)
            WebKitNetworkRequest* request;
            #endif
            const gchar* original_uri;
            gchar** fingerprint;

            icon = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
            gtk_button_set_image (GTK_BUTTON (button), icon);
            if (g_object_get_data (G_OBJECT (download), "open-download"))
                gtk_button_clicked (GTK_BUTTON (button));

            if (1)
            {
                const gchar* uri = webkit_download_get_destination_uri (download);
                gchar* path = soup_uri_decode (uri);
                gchar* filename = g_strrstr (path, "/") + 1;
                gchar* msg = g_strdup_printf (
                    _("The file '<b>%s</b>' has been downloaded."), filename);
                g_free (path);
                g_signal_emit_by_name (browser, "send-notification",
                                       _("Transfer completed"), msg);
                g_free (msg);
            }

            /* Link Fingerprint */
            #if WEBKIT_CHECK_VERSION (1, 1, 14)
            request = webkit_download_get_network_request (download);
            original_uri = g_object_get_data (G_OBJECT (request), "midori-original-uri");
            #else
            original_uri = webkit_download_get_uri (download);
            #endif
            fingerprint = g_strsplit (original_uri, "#!md5!", 2);
            if (fingerprint && fingerprint[0] && fingerprint[1])
            {
                gchar* filename = g_filename_from_uri (
                    webkit_download_get_destination_uri (download), NULL, NULL);
                gchar* contents;
                gsize length;
                gboolean y = g_file_get_contents (filename, &contents, &length, NULL);
                gchar* checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
                    (guchar*)contents, length);
                g_free (filename);
                g_free (contents);
                if (!y || !g_str_equal (fingerprint[1], checksum))
                    gtk_image_set_from_stock (GTK_IMAGE (icon),
                        GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
                g_free (checksum);
            }
            else
            {
                gchar* filename = g_filename_from_uri (
                    webkit_download_get_destination_uri (download), NULL, NULL);
                g_strfreev (fingerprint);
                fingerprint = g_strsplit (original_uri, "#!sha1!", 2);
                if (fingerprint && fingerprint[0] && fingerprint[1])
                {
                    gchar* contents;
                    gsize length;
                    gboolean y = g_file_get_contents (filename, &contents, &length, NULL);
                    gchar* checksum = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
                        (guchar*)contents, length);
                    g_free (contents);
                    if (!y || !g_str_equal (fingerprint[1], checksum))
                        gtk_image_set_from_stock (GTK_IMAGE (icon),
                            GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
                    g_free (checksum);
                }
                g_free (filename);
            }
            g_strfreev (fingerprint);
            break;
        }
        case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
        case WEBKIT_DOWNLOAD_STATUS_ERROR:
            icon = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
            gtk_button_set_image (GTK_BUTTON (button), icon);
            break;
        default:
            break;
    }
}

static void
midori_transferbar_download_button_clicked_cb (GtkWidget*    button,
                                               TransferInfo* info)
{
    WebKitDownload* download = info->download;

    switch (webkit_download_get_status (download))
    {
        case WEBKIT_DOWNLOAD_STATUS_STARTED:
            webkit_download_cancel (download);
            break;
        case WEBKIT_DOWNLOAD_STATUS_FINISHED:
        {
            const gchar* uri = webkit_download_get_destination_uri (download);
            GtkWidget* icon = gtk_button_get_image (GTK_BUTTON (button));
            gchar* stock_id;
            gtk_image_get_stock (GTK_IMAGE (icon), &stock_id, NULL);
            if (g_str_equal (stock_id, GTK_STOCK_DIALOG_WARNING))
            {
                sokoke_message_dialog (GTK_MESSAGE_WARNING,
                    _("The downloaded file is erroneous."),
                    _("The checksum provided with the link did not match. " \
                      "This means the file is probably incomplete or was " \
                      "modified afterwards."),
                    TRUE);
            }
            else if (sokoke_show_uri (gtk_widget_get_screen (button),
                uri, gtk_get_current_event_time (), NULL))
                gtk_widget_destroy (button);
            break;
        }
        case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
            gtk_widget_destroy (button);
        default:
            break;
    }
}

void
midori_transferbar_add_download_item (MidoriTransferbar* transferbar,
                                      WebKitDownload*    download)
{
    GtkWidget* box;
    GtkWidget* icon;
    GtkToolItem* toolitem;
    GtkWidget* button;
    GtkWidget* progress;
    const gchar* uri;
    gint width;
    TransferInfo* info;

    box = gtk_hbox_new (FALSE, 0);
    progress = gtk_progress_bar_new ();
    gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (progress),
                                    PANGO_ELLIPSIZE_MIDDLE);
    if ((uri = webkit_download_get_destination_uri (download)))
    {
        gchar* path = soup_uri_decode (uri);
        gchar* filename = g_strrstr (path, "/") + 1;
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress), filename);
        g_free (path);
    }
    else
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress),
            webkit_download_get_suggested_filename (download));
    sokoke_widget_get_text_size (progress, "M", &width, NULL);
    gtk_widget_set_size_request (progress, width * 10, 1);
    /* Avoid a bug in WebKit */
    if (webkit_download_get_status (download) != WEBKIT_DOWNLOAD_STATUS_CREATED)
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress),
            webkit_download_get_progress (download));
    gtk_box_pack_start (GTK_BOX (box), progress, FALSE, FALSE, 0);
    icon = gtk_image_new_from_stock (GTK_STOCK_CANCEL, GTK_ICON_SIZE_MENU);
    button = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
    gtk_container_add (GTK_CONTAINER (button), icon);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    toolitem = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (toolitem), box);
    gtk_widget_show_all (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (transferbar), toolitem, 0);
    gtk_widget_show (GTK_WIDGET (transferbar->clear));

    info = g_slice_new (TransferInfo);
    info->download = g_object_ref (download);
    info->button = button;
    info->toolitem = GTK_WIDGET (toolitem);
    info->transferbar = transferbar;
    g_signal_connect (button, "destroy",
                      G_CALLBACK (midori_transferbar_button_destroy_cb), info);
    transferbar->infos = g_list_prepend (transferbar->infos, info);

    g_signal_connect (download, "notify::progress",
        G_CALLBACK (midori_transferbar_download_notify_progress_cb), progress);
    g_signal_connect (download, "notify::status",
        G_CALLBACK (midori_transferbar_download_notify_status_cb), info);
    g_signal_connect (button, "clicked",
        G_CALLBACK (midori_transferbar_download_button_clicked_cb), info);
    g_object_set_data_full (G_OBJECT (download), "last-time", g_new0 (gdouble, 1), g_free);
    g_object_set_data_full (G_OBJECT (download), "last-size", g_new0 (guint64, 1), g_free);
}

static void
midori_transferbar_clear_clicked_cb (GtkWidget*         button,
                                     MidoriTransferbar* transferbar)
{
    GList* list;

    for (list = transferbar->infos; list != NULL; list = g_list_next (list))
    {
        TransferInfo* info = list->data;
        WebKitDownloadStatus status = webkit_download_get_status (info->download);
        if (status == WEBKIT_DOWNLOAD_STATUS_ERROR
         || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED
         || status == WEBKIT_DOWNLOAD_STATUS_FINISHED)
        {
            gtk_widget_destroy (info->button);
        }
    }
}
#endif

static void
midori_transferbar_init (MidoriTransferbar* transferbar)
{
    gtk_toolbar_set_style (GTK_TOOLBAR (transferbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (transferbar), GTK_ICON_SIZE_MENU);

    transferbar->clear = gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (transferbar->clear), _("Clear All"));
    gtk_tool_item_set_is_important (transferbar->clear, TRUE);
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    g_signal_connect (transferbar->clear, "clicked",
        G_CALLBACK (midori_transferbar_clear_clicked_cb), transferbar);
    #endif
    gtk_toolbar_insert (GTK_TOOLBAR (transferbar), transferbar->clear, -1);

    transferbar->infos = NULL;
}

gboolean
midori_transferbar_confirm_delete (MidoriTransferbar* transferbar)
{
    GtkWidget* dialog = NULL;
    gboolean cancel = FALSE;
    #if WEBKIT_CHECK_VERSION (1, 1, 3)
    GList* list;
    gboolean all_done = TRUE;

    for (list = transferbar->infos; list != NULL; list = g_list_next (list))
    {
        TransferInfo* info = list->data;
        WebKitDownloadStatus status = webkit_download_get_status (info->download);

        if (status != WEBKIT_DOWNLOAD_STATUS_FINISHED
         && status != WEBKIT_DOWNLOAD_STATUS_CANCELLED
         && status != WEBKIT_DOWNLOAD_STATUS_ERROR)
        {
            all_done = FALSE;
            break;
        }
    }

    if (!all_done)
    #else
    if (transferbar->infos || g_list_nth_data (transferbar->infos, 0))
    #endif
    {
        GtkWidget* widget = gtk_widget_get_toplevel (GTK_WIDGET (transferbar));
        dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
            _("Some files are being downloaded"));
        gtk_window_set_title (GTK_WINDOW (dialog),
            _("Some files are being downloaded"));
        gtk_dialog_add_button (GTK_DIALOG (dialog),
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button (GTK_DIALOG (dialog),
            _("_Quit Midori"), GTK_RESPONSE_ACCEPT);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
            _("The transfers will be cancelled if Midori quits."));
    }
    if (dialog != NULL)
    {
        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_CANCEL)
            cancel = TRUE;
        gtk_widget_destroy (dialog);
    }

    return cancel;
}

