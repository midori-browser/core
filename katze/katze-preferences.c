/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-preferences.h"

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

struct _KatzePreferencesPrivate
{
    GtkWidget* notebook;
    GtkWidget* toolbar;
    GtkWidget* toolbutton;
    GtkSizeGroup* sizegroup;
    GtkSizeGroup* sizegroup2;
    GtkWidget* page;
    GtkWidget* frame;
    GtkWidget* box;
    GtkWidget* hbox;
};

G_DEFINE_TYPE (KatzePreferences, katze_preferences, GTK_TYPE_DIALOG);

static void
katze_preferences_finalize (GObject* object);

static void
katze_preferences_class_init (KatzePreferencesClass* class)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = katze_preferences_finalize;

    g_type_class_add_private (class, sizeof (KatzePreferencesPrivate));
}

static void
katze_preferences_response_cb (KatzePreferences* preferences,
                               gint              response)
{
    if (response == GTK_RESPONSE_CLOSE || response == GTK_RESPONSE_APPLY)
        gtk_widget_destroy (GTK_WIDGET (preferences));
}

static void
katze_preferences_init (KatzePreferences* preferences)
{
    KatzePreferencesPrivate* priv;
    gchar* dialog_title;

    preferences->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE ((preferences),
        KATZE_TYPE_PREFERENCES, KatzePreferencesPrivate);

    dialog_title = g_strdup_printf (_("Preferences for %s"),
                                    g_get_application_name ());
    g_object_set (preferences,
                  "icon-name", GTK_STOCK_PREFERENCES,
                  "title", dialog_title,
#if !GTK_CHECK_VERSION (2, 22, 0)
                  "has-separator", FALSE,
#endif
                  NULL);
    g_free (dialog_title);

    #if !HAVE_OSX
    gtk_dialog_add_buttons (GTK_DIALOG (preferences),
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
        NULL);
    katze_widget_add_class (gtk_dialog_get_widget_for_response (
        GTK_DIALOG (preferences), GTK_RESPONSE_HELP), "help_button");

    gtk_dialog_add_buttons (GTK_DIALOG (preferences),
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        NULL);
    #endif

    g_object_connect (preferences,
        "signal::response", katze_preferences_response_cb, NULL,
        NULL);

}

static void
katze_preferences_finalize (GObject* object)
{
    G_OBJECT_CLASS (katze_preferences_parent_class)->finalize (object);
}

/**
 * katze_preferences_new:
 * @parent: the parent window, or %NULL
 *
 * Creates a new preferences dialog.
 *
 * Return value: (transfer full): a new #KatzePreferences
 *
 * Since: 0.2.1
 **/
GtkWidget*
katze_preferences_new (GtkWindow* parent)
{
    KatzePreferences* preferences;

    g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);

    preferences = g_object_new (KATZE_TYPE_PREFERENCES,
                                "transient-for", parent,
                                NULL);

    return GTK_WIDGET (preferences);
}

#if HAVE_OSX
static void
katze_preferences_help_clicked_cb (GtkWidget* button,
                                    GtkDialog* dialog)
{
    gtk_dialog_response (dialog, GTK_RESPONSE_HELP);
}

static void
katze_preferences_toolbutton_clicked_cb (GtkWidget* toolbutton,
                                         GtkWidget* page)
{
    gpointer notebook = g_object_get_data (G_OBJECT (toolbutton), "notebook");
    guint n = gtk_notebook_page_num (notebook, page);
    gtk_notebook_set_current_page (notebook, n);
}
#endif

static void
katze_preferences_prepare (KatzePreferences* preferences)
{
    KatzePreferencesPrivate* priv = preferences->priv;
    
    #if GTK_CHECK_VERSION (3, 10, 0) && !HAVE_OSX
    priv->notebook = gtk_stack_new ();
    #else
    priv->notebook = gtk_notebook_new ();
    #endif
    gtk_container_set_border_width (GTK_CONTAINER (priv->notebook), 6);

    #if HAVE_OSX
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
    priv->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (priv->toolbar), GTK_TOOLBAR_BOTH);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->toolbar), FALSE);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (preferences))),
                        priv->toolbar, FALSE, FALSE, 0);
    #else
    #if GTK_CHECK_VERSION (3, 10, 0) && !HAVE_OSX
        priv->toolbar = gtk_stack_switcher_new ();
        gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (priv->toolbar), GTK_STACK (priv->notebook));
        gtk_widget_set_halign (priv->toolbar, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (preferences))),
                        priv->toolbar, FALSE, FALSE, 0);
    #else
        priv->toolbar = NULL;
    #endif

    #endif
    priv->toolbutton = NULL;
    gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (preferences))),
                      priv->notebook, TRUE, TRUE, 4);

    priv->sizegroup = NULL;
    priv->sizegroup2 = NULL;
    priv->page = NULL;
    priv->frame = NULL;
    priv->box = NULL;
    priv->hbox = NULL;

    g_signal_connect (priv->notebook, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &priv->notebook);

    #if HAVE_OSX
    GtkWidget* icon;
    GtkWidget* hbox = gtk_hbox_new (FALSE, 0);
    GtkWidget* button = gtk_button_new ();
    icon = gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    g_signal_connect (button, "clicked",
        G_CALLBACK (katze_preferences_help_clicked_cb), preferences);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 4);
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (preferences)->action_area),
        hbox, FALSE, FALSE, 0);
    #endif
    gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (preferences)));
}

#if GTK_CHECK_VERSION (3, 10, 0) & !HAVE_OSX
/* these functions are used to clear the 100-px width set in GTK3's
update_button function in gtk/gtkstackswitcher.c */

static void
clear_size_request (GtkWidget* widget)
{
    gtk_widget_set_size_request (widget, -1, -1);
}

static void
workaround_stack_switcher_sizing (GtkStackSwitcher* switcher)
{
    gtk_container_forall (GTK_CONTAINER (switcher), (GtkCallback)clear_size_request, NULL);
}
#endif

/**
 * katze_preferences_add_category:
 * @preferences: a #KatzePreferences instance
 * @label: a category label
 * @icon: an icon name
 *
 * Adds a new category with the specified label to the dialog.
 *
 * Return value: (transfer none): a new #GtkBox in the preferences widget to
 * hold the category's widgets
 *
 * Since: 0.2.1
 *
 * Since 0.3.4 a #GtkBox is returned that can be packed into.
 **/
GtkWidget*
katze_preferences_add_category (KatzePreferences* preferences,
                                const gchar*      label,
                                const gchar*      icon)
{
    KatzePreferencesPrivate* priv;

    g_return_val_if_fail (KATZE_IS_PREFERENCES (preferences), NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (icon != NULL, NULL);

    priv = preferences->priv;

    if (!priv->notebook)
        katze_preferences_prepare (preferences);

    priv->page = gtk_vbox_new (FALSE, 0);
    priv->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    gtk_widget_show (priv->page);
    gtk_container_set_border_width (GTK_CONTAINER (priv->page), 4);
    #if GTK_CHECK_VERSION (3, 10, 0) & !HAVE_OSX
    gtk_stack_add_titled (GTK_STACK (priv->notebook), 
                         priv->page, label, label);
    workaround_stack_switcher_sizing (GTK_STACK_SWITCHER (priv->toolbar));
    #else
    gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
                              priv->page, gtk_label_new (label));

    #endif

    #if HAVE_OSX
    priv->toolbutton = GTK_WIDGET (priv->toolbutton ?
        gtk_radio_tool_button_new_from_widget (
        GTK_RADIO_TOOL_BUTTON (priv->toolbutton))
        : gtk_radio_tool_button_new (NULL));
    gtk_widget_show (priv->toolbutton);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (priv->toolbutton), label);
    gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (priv->toolbutton), icon);
    gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar),
                        GTK_TOOL_ITEM (priv->toolbutton), -1);
    g_signal_connect (priv->toolbutton, "clicked",
        G_CALLBACK (katze_preferences_toolbutton_clicked_cb), priv->page);
    if (priv->toolbutton)
        g_object_set_data (G_OBJECT (priv->toolbutton), "notebook", priv->notebook);
    #endif

    return priv->page;
}

static GtkWidget*
katze_hig_frame_new (const gchar* title)
{
    /* Create a frame with no actual frame but a bold label and indentation */
    GtkWidget* frame = gtk_frame_new (NULL);
    #ifdef G_OS_WIN32
    gtk_frame_set_label (GTK_FRAME (frame), title);
    #else
    gchar* title_bold = g_strdup_printf ("<b>%s</b>", title);
    GtkWidget* label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), title_bold);
    g_free (title_bold);
    gtk_frame_set_label_widget (GTK_FRAME (frame), label);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    #endif
    return frame;
}

/**
 * katze_preferences_add_group:
 * @preferences: a #KatzePreferences instance
 * @label: a group label, or %NULL
 *
 * Adds a new group with the specified label to the dialog.
 *
 * Since: 0.2.1
 *
 * Since 0.3.4 you can pass %NULL to hide the label.
 **/
void
katze_preferences_add_group (KatzePreferences* preferences,
                             const gchar*      label)
{
    KatzePreferencesPrivate* priv;

    g_return_if_fail (KATZE_IS_PREFERENCES (preferences));

    priv = preferences->priv;
    priv->sizegroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->frame = label ? katze_hig_frame_new (label) :
        g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (priv->frame), 4);
    gtk_box_pack_start (GTK_BOX (priv->page), priv->frame, FALSE, FALSE, 0);
    priv->box = gtk_vbox_new (FALSE, 4);
    gtk_container_set_border_width (GTK_CONTAINER (priv->box), 4);
    gtk_container_add (GTK_CONTAINER (priv->frame), priv->box);
    gtk_widget_show_all (priv->frame);
}

/**
 * katze_preferences_add_widget:
 * @preferences: a #KatzePreferences instance
 * @widget: a widget representing an option
 * @type: "filled", "indented", or "spanned"
 *
 * Adds a widget to the dialog.
 *
 * Since: 0.2.1
 **/
void
katze_preferences_add_widget (KatzePreferences* preferences,
                              GtkWidget*        widget,
                              const gchar*      type)
{
    KatzePreferencesPrivate* priv;
    const gchar* _type;

    g_return_if_fail (KATZE_IS_PREFERENCES (preferences));
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (type != NULL);

    priv = preferences->priv;
    _type = g_intern_string  (type);

    /* Showing implicitly widget and children is not the best idea,
      but lots of repeated function calls aren't either. */
    gtk_widget_show_all (widget);

    if (!priv->hbox)
        _type = g_intern_string ("indented");

    if (_type != g_intern_static_string ("spanned"))
    {
        priv->hbox = gtk_hbox_new (FALSE, 4);
        gtk_widget_show (priv->hbox);
        gtk_box_pack_start (GTK_BOX (priv->hbox), widget, TRUE, FALSE, 0);
    }

    if (_type == g_intern_static_string ("filled"))
        gtk_box_pack_start (GTK_BOX (priv->box), priv->hbox, TRUE, FALSE, 0);
    else if (_type == g_intern_static_string ("indented"))
    {
        GtkWidget* align = gtk_alignment_new (0, 0.5, 0, 0);
        gtk_widget_show (align);
        gtk_container_add (GTK_CONTAINER (align), priv->hbox);
        if (!GTK_IS_SPIN_BUTTON (widget))
            gtk_size_group_add_widget (priv->sizegroup, widget);
        gtk_box_pack_start (GTK_BOX (priv->box), align, TRUE, FALSE, 0);
    }
    else if (_type == g_intern_static_string ("spanned"))
    {
        GtkWidget* align = gtk_alignment_new (0, 0.5, 0, 0);
        gtk_widget_show (align);
        gtk_container_add (GTK_CONTAINER (align), widget);
        if (!GTK_IS_LABEL (widget) && !GTK_IS_SPIN_BUTTON (widget)
            && !(GTK_IS_BUTTON (widget) && !GTK_IS_TOGGLE_BUTTON (widget)))
            gtk_size_group_add_widget (priv->sizegroup2, widget);
        gtk_box_pack_start (GTK_BOX (priv->hbox), align, TRUE, FALSE, 0);
    }
}
