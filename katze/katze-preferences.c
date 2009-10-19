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

#define HAVE_HILDON 0 /* FIXME: Implement Hildonized version */

#include <string.h>
#include <glib/gi18n.h>

struct _KatzePreferencesPrivate
{
    #if HAVE_HILDON

    #else
    GtkWidget* notebook;
    GtkWidget* toolbar;
    GtkWidget* toolbutton;
    GtkSizeGroup* sizegroup;
    GtkSizeGroup* sizegroup2;
    GtkWidget* page;
    GtkWidget* frame;
    GtkWidget* box;
    GtkWidget* hbox;
    #endif
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
    if (response == GTK_RESPONSE_CLOSE)
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
                  "has-separator", FALSE,
                  NULL);
    g_free (dialog_title);

    #if HAVE_HILDON

    #else
    priv->notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (priv->notebook), 6);

    #if HAVE_OSX
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (preferences->notebook), FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (preferences->notebook), FALSE);
    priv->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (priv->toolbar), GTK_TOOLBAR_BOTH);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->toolbar), FALSE);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (preferences)->vbox),
                        priv->toolbar, FALSE, FALSE, 0);
    #else
    priv->toolbar = NULL;
    #endif
    priv->toolbutton = NULL;
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (preferences)->vbox),
                      priv->notebook, FALSE, FALSE, 4);

    priv->sizegroup = NULL;
    priv->sizegroup2 = NULL;
    priv->page = NULL;
    priv->frame = NULL;
    priv->box = NULL;
    priv->hbox = NULL;
    #endif

    #if HAVE_OSX
    GtkWidget* icon;
    hbox = gtk_hbox_new (FALSE, 0);
    button = gtk_button_new ();
    icon = gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    g_signal_connect (button, "clicked",
        G_CALLBACK (katze_preferences_help_clicked_cb), preferences);
    gtk_box_pack_end (GTK_BOX (hbox),
        button, FALSE, FALSE, 4);
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (preferences)->vbox),
        hbox, FALSE, FALSE, 0);
    #endif
    gtk_widget_show_all (GTK_DIALOG (preferences)->vbox);

    #if !HAVE_OSX
    gtk_dialog_add_buttons (GTK_DIALOG (preferences),
        GTK_STOCK_HELP, GTK_RESPONSE_HELP,
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
 * Return value: a new #KatzePreferences
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

/**
 * katze_preferences_add_category:
 * @preferences: a #KatzePreferences instance
 * @label: a category label
 * @icon: an icon name
 *
 * Adds a new category with the specified label to the dialog.
 *
 * Since: 0.2.1
 **/
void
katze_preferences_add_category (KatzePreferences* preferences,
                                const gchar*      label,
                                const gchar*      icon)
{
    KatzePreferencesPrivate* priv = preferences->priv;

    priv->page = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (priv->page);
    #if HAVE_OSX
    priv->toolbutton = GTK_WIDGET (priv->toolbutton ?
        gtk_radio_tool_button_new_from_widget (
        GTK_RADIO_TOOL_BUTTON (priv->toolbutton))
        : gtk_radio_tool_button_new (NULL));
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (priv->toolbutton), label);
    gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (priv->toolbutton), icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
                        GTK_TOOL_ITEM (priv->toolbutton), -1);
    g_signal_connect (priv->toolbutton, "clicked",
        G_CALLBACK (katze_preferences_toolbutton_clicked_cb), page);
    if (priv->toolbutton)
        g_object_set_data (G_OBJECT (priv->toolbutton), "notebook", priv->notebook);
    #endif
    gtk_container_set_border_width (GTK_CONTAINER (priv->page), 4);
    gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
                              priv->page, gtk_label_new (label));
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
 * @label: a group label
 *
 * Adds a new group with the specified label to the dialog.
 *
 * Since: 0.2.1
 **/
void
katze_preferences_add_group (KatzePreferences* preferences,
                             const gchar*      label)
{
    KatzePreferencesPrivate* priv = preferences->priv;

    priv->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->sizegroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->frame = katze_hig_frame_new (label);
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

/**
 * katze_preferences_add_option:
 * @preferences: a #KatzePreferences instance
 * @object: the object to proxy
 * @property: the property to proxy
 * @label: a label, or %NULL
 * @widget: a widget representing an option
 * @type: "filled", "indented", or "spanned"
 *
 * Adds an option to the dialog, with a label.
 *
 * If @label is %NULL, it will be filled in from the property.
 *
 * Since: 0.2.1
 **/
void
katze_preferences_add_option (KatzePreferences* preferences,
                              gpointer          object,
                              const gchar*      property,
                              const gchar*      label,
                              GtkWidget*        widget,
                              const gchar*      type)
{
    g_return_if_fail (KATZE_IS_PREFERENCES (preferences));
    g_return_if_fail (G_IS_OBJECT (object));
    g_return_if_fail (property != NULL);
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (type != NULL);

    if (label)
        katze_preferences_add_widget (preferences,
            gtk_label_new_with_mnemonic (label), "indented");
    else
        katze_preferences_add_widget (preferences,
            katze_property_label (object, property), "indented");
    katze_preferences_add_widget (preferences, widget, type);
}
