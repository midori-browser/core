/*
 Copyright (C) 2011-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze/gtk3-compat.h"

#if !GTK_CHECK_VERSION (3, 2, 0)
static void
sokoke_widget_set_pango_font_style (GtkWidget* widget,
                                    PangoStyle style)
{
    /* Conveniently change the pango font style
       For some reason we need to reset if we actually want the normal style */
    if (style == PANGO_STYLE_NORMAL)
        gtk_widget_modify_font (widget, NULL);
    else
    {
        PangoFontDescription* font_description = pango_font_description_new ();
        pango_font_description_set_style (font_description, PANGO_STYLE_ITALIC);
        gtk_widget_modify_font (widget, font_description);
        pango_font_description_free (font_description);
    }
}

/* returns TRUE if the entry is currently showing its placeholder text */
static gboolean
sokoke_entry_is_showing_default (GtkEntry* entry)
{
    gint showing_default = GPOINTER_TO_INT (
        g_object_get_data (G_OBJECT (entry), "sokoke_showing_default"));

    const gchar* text = gtk_entry_get_text (entry);
    const gchar* default_text = (const gchar*)g_object_get_data (
        G_OBJECT (entry), "sokoke_default_text");

    return showing_default && !g_strcmp0(text, default_text);
}

/* returns TRUE if the entry is not being used by the user to perform entry or
hold data at a given moment */
static gboolean
sokoke_entry_is_idle (GtkEntry* entry)
{
    const gchar* text = gtk_entry_get_text (entry);
    
    /* if the default is visible or the user has left the entry blank */
    return sokoke_entry_is_showing_default(entry) ||
        (text && !*text && !gtk_widget_has_focus (GTK_WIDGET (entry)));
}

static gboolean
sokoke_on_entry_text_changed (GtkEntry*   entry,
                              GParamSpec* pspec,
                              gpointer    userdata);

static void
sokoke_hide_placeholder_text (GtkEntry* entry)
{
    if(sokoke_entry_is_showing_default (entry))
    {
        g_signal_handlers_block_by_func (entry, sokoke_on_entry_text_changed, NULL);
        gtk_entry_set_text (entry, "");
        g_signal_handlers_unblock_by_func (entry, sokoke_on_entry_text_changed, NULL);
    }
    g_object_set_data (G_OBJECT (entry), "sokoke_showing_default",
                       GINT_TO_POINTER (0));
    sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                        PANGO_STYLE_NORMAL);
}

static gboolean
sokoke_on_entry_focus_in_event (GtkEntry*      entry,
                                GdkEventFocus* event,
                                gpointer       userdata)
{
    sokoke_hide_placeholder_text (entry);
    return FALSE;
}

static void
sokoke_show_placeholder_text (GtkEntry* entry)
{
    /* no need to do work if the widget is unfocused with placeholder */
    if(sokoke_entry_is_showing_default (entry))
        return;

    /* no need to do work if the placeholder is already visible */
    const gchar* text = gtk_entry_get_text (entry);
    if (text && !*text)
    {
        const gchar* default_text = (const gchar*)g_object_get_data (
            G_OBJECT (entry), "sokoke_default_text");
        g_object_set_data (G_OBJECT (entry),
                           "sokoke_showing_default", GINT_TO_POINTER (1));
        g_signal_handlers_block_by_func (entry, sokoke_on_entry_text_changed, NULL);
        gtk_entry_set_text (entry, default_text);
        g_signal_handlers_unblock_by_func (entry, sokoke_on_entry_text_changed, NULL);
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_ITALIC);
    }
}

static void
sokoke_on_entry_drag_leave (GtkEntry*       entry,
                             GdkDragContext* drag_context,
                             guint           timestamp,
                             gpointer        user_data)
{
    sokoke_show_placeholder_text (entry);
}

static gboolean
sokoke_on_entry_text_changed (GtkEntry*   entry,
                              GParamSpec* pspec,
                              gpointer    userdata)
{
    if(sokoke_entry_is_idle (entry))
    {
        sokoke_show_placeholder_text (entry);
    }
    else
    {
        sokoke_hide_placeholder_text (entry);
    }

    return TRUE;
}

static gboolean
sokoke_on_entry_focus_out_event (GtkEntry*      entry,
                                 GdkEventFocus* event,
                                 gpointer       userdata)
{
    sokoke_show_placeholder_text (entry);
    return FALSE;
}

static gboolean
sokoke_on_entry_drag_motion (GtkEntry*       entry,
                             GdkDragContext* drag_context,
                             gint            x,
                             gint            y,
                             guint           timestamp,
                             gpointer        user_data)
{
    sokoke_hide_placeholder_text (entry);
    return FALSE;
}

static gboolean
sokoke_on_entry_drag_drop (GtkEntry*       entry,
                           GdkDragContext* drag_context,
                           gint            x,
                           gint            y,
                           guint           timestamp,
                           gpointer        user_data)
{
    sokoke_hide_placeholder_text (entry);
    return FALSE;
}

static void
sokoke_on_entry_popup (GtkEntry  *entry,
                       GtkWidget *popup,
                       gpointer   user_data)
{
    /* If the user selects paste in the popup, we should hide the default
    when the menu closes so it pastes into a clean entry */
    g_signal_connect_swapped (popup, "destroy", G_CALLBACK (
        sokoke_hide_placeholder_text), entry);
}

void
gtk_entry_set_placeholder_text (GtkEntry*    entry,
                                const gchar* default_text)
{
    /* Note: The default text initially overwrites any previous text */
    gchar* old_default_text = g_object_get_data (G_OBJECT (entry), "sokoke_default_text");
    g_object_set_data (G_OBJECT (entry), "sokoke_default_text", (gpointer)default_text);

    if (default_text == NULL)
    {
        g_object_set_data (G_OBJECT (entry), "sokoke_showing_default", GINT_TO_POINTER (0));
        g_signal_handlers_disconnect_by_func (entry,
            G_CALLBACK (sokoke_on_entry_drag_motion), NULL);
        g_signal_handlers_disconnect_by_func (entry,
            G_CALLBACK (sokoke_on_entry_focus_in_event), NULL);
        g_signal_handlers_disconnect_by_func (entry,
            G_CALLBACK (sokoke_on_entry_drag_leave), NULL);
        g_signal_handlers_disconnect_by_func (entry,
            G_CALLBACK (sokoke_on_entry_drag_drop), NULL);
        g_signal_handlers_disconnect_by_func (entry,
           G_CALLBACK (sokoke_on_entry_focus_out_event), NULL);
        g_signal_handlers_disconnect_by_func (entry,
            G_CALLBACK (sokoke_on_entry_text_changed), NULL);
        g_signal_handlers_disconnect_by_func (entry,
            G_CALLBACK (sokoke_on_entry_popup), NULL);
    }
    else if (old_default_text == NULL)
    {
        g_object_set_data (G_OBJECT (entry), "sokoke_showing_default", GINT_TO_POINTER (1));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry), PANGO_STYLE_ITALIC);
        gtk_entry_set_text (entry, default_text);
        g_signal_connect (entry, "drag-motion",
            G_CALLBACK (sokoke_on_entry_drag_motion), NULL);
        g_signal_connect (entry, "focus-in-event",
            G_CALLBACK (sokoke_on_entry_focus_in_event), NULL);
        g_signal_connect (entry, "drag-leave",
            G_CALLBACK (sokoke_on_entry_drag_leave), NULL);
        g_signal_connect (entry, "drag-drop",
            G_CALLBACK (sokoke_on_entry_drag_drop), NULL);
        g_signal_connect (entry, "focus-out-event",
           G_CALLBACK (sokoke_on_entry_focus_out_event), NULL);
        g_signal_connect (entry, "notify::text",
            G_CALLBACK (sokoke_on_entry_text_changed), NULL);
        g_signal_connect (entry, "populate-popup",
            G_CALLBACK (sokoke_on_entry_popup), NULL);
    }
    else if (!gtk_widget_has_focus (GTK_WIDGET (entry)))
    {
        gint showing_default = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (entry), "sokoke_showing_default"));
        if (showing_default)
        {
            gtk_entry_set_text (entry, default_text);
            sokoke_widget_set_pango_font_style (GTK_WIDGET (entry), PANGO_STYLE_ITALIC);
        }
    }
}

const gchar*
gtk_entry_get_placeholder_text (GtkEntry* entry)
{
    return g_object_get_data (G_OBJECT (entry), "sokoke_default_text");
}
#endif

