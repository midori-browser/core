#include "katze/gtk3-compat.h"

#if !GTK_CHECK_VERSION (3, 2, 0) && !defined (HAVE_HILDON_2_2)
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

static gboolean
sokoke_on_entry_focus_in_event (GtkEntry*      entry,
                                GdkEventFocus* event,
                                gpointer       userdata)
{
    gint has_default = GPOINTER_TO_INT (
        g_object_get_data (G_OBJECT (entry), "sokoke_has_default"));
    if (has_default)
    {
        gtk_entry_set_text (entry, "");
        g_object_set_data (G_OBJECT (entry), "sokoke_has_default",
                           GINT_TO_POINTER (0));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_NORMAL);
    }
    return FALSE;
}

static gboolean
sokoke_on_entry_focus_out_event (GtkEntry*      entry,
                                 GdkEventFocus* event,
                                 gpointer       userdata)
{
    const gchar* text = gtk_entry_get_text (entry);
    if (text && !*text)
    {
        const gchar* default_text = (const gchar*)g_object_get_data (
            G_OBJECT (entry), "sokoke_default_text");
        gtk_entry_set_text (entry, default_text);
        g_object_set_data (G_OBJECT (entry),
                           "sokoke_has_default", GINT_TO_POINTER (1));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_ITALIC);
    }
    return FALSE;
}

static void
sokoke_on_entry_drag_data_received (GtkEntry*       entry,
                                    GdkDragContext* drag_context,
                                    gint            x,
                                    gint            y,
                                    guint           timestamp,
                                    gpointer        user_data)
{
    sokoke_on_entry_focus_in_event (entry, NULL, NULL);
}

void
gtk_entry_set_placeholder_text (GtkEntry*    entry,
                                const gchar* default_text)
{
    /* Note: The default text initially overwrites any previous text */
    gchar* old_value = g_object_get_data (G_OBJECT (entry),
                                          "sokoke_default_text");
    if (!old_value)
    {
        g_object_set_data (G_OBJECT (entry), "sokoke_has_default",
                           GINT_TO_POINTER (1));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_ITALIC);
        gtk_entry_set_text (entry, default_text);
        g_signal_connect (entry, "drag-data-received",
            G_CALLBACK (sokoke_on_entry_drag_data_received), NULL);
        g_signal_connect (entry, "focus-in-event",
            G_CALLBACK (sokoke_on_entry_focus_in_event), NULL);
        g_signal_connect (entry, "focus-out-event",
           G_CALLBACK (sokoke_on_entry_focus_out_event), NULL);
    }
    else if (!gtk_widget_has_focus (GTK_WIDGET (entry)))
    {
        gint has_default = GPOINTER_TO_INT (
            g_object_get_data (G_OBJECT (entry), "sokoke_has_default"));
        if (has_default)
        {
            gtk_entry_set_text (entry, default_text);
            sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                                PANGO_STYLE_ITALIC);
        }
    }
    g_object_set_data (G_OBJECT (entry), "sokoke_default_text",
                       (gpointer)default_text);
}
#endif

#if !GTK_CHECK_VERSION (2, 12, 0)

void
gtk_widget_set_has_tooltip (GtkWidget* widget,
                            gboolean   has_tooltip)
{
    /* Do nothing */
}

void
gtk_widget_set_tooltip_text (GtkWidget*   widget,
                             const gchar* text)
{
    if (text && *text)
    {
        static GtkTooltips* tooltips = NULL;
        if (G_UNLIKELY (!tooltips))
            tooltips = gtk_tooltips_new ();
        gtk_tooltips_set_tip (tooltips, widget, text, NULL);
    }
}

void
gtk_tool_item_set_tooltip_text (GtkToolItem* toolitem,
                                const gchar* text)
{
    if (text && *text)
    {
        static GtkTooltips* tooltips = NULL;
        if (G_UNLIKELY (!tooltips))
            tooltips = gtk_tooltips_new ();

        gtk_tool_item_set_tooltip (toolitem, tooltips, text, NULL);
    }
}

#endif

