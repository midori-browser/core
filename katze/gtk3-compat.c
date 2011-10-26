#include "katze/gtk3-compat.h"

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

