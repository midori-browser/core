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

#ifdef HAVE_GRANITE
    #if HAVE_OSX
        #error FIXME granite on OSX is not implemented
    #endif
    #include <granite.h>
#endif

#if HAVE_HILDON
    #include "katze-scrolled.h"
    #include <hildon/hildon.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

struct _KatzePreferencesPrivate
{
    #if HAVE_HILDON
    GtkWidget* scrolled;
    GtkSizeGroup* sizegroup;
    GtkSizeGroup* sizegroup2;
    GtkWidget* box;
    GtkWidget* hbox;
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
    if (response == GTK_RESPONSE_CLOSE || response == GTK_RESPONSE_APPLY)
        gtk_widget_destroy (GTK_WIDGET (preferences));
}

#ifdef HAVE_HILDON_2_2
static void
katze_preferences_size_request_cb (KatzePreferences* preferences,
                                   GtkRequisition*   requisition)
{
    GdkScreen* screen = gtk_widget_get_screen (GTK_WIDGET (preferences));
    if (gdk_screen_get_height (screen) > gdk_screen_get_width (screen))
        gtk_widget_hide (gtk_dialog_get_action_area (GTK_DIALOG (preferences)));
    else
        gtk_widget_show (gtk_dialog_get_action_area (GTK_DIALOG (preferences)));
}
#endif

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
#if !GTK_CHECK_VERSION (3, 0, 0)
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
        #if HAVE_HILDON
        GTK_STOCK_SAVE, GTK_RESPONSE_APPLY,
        #else
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
        #endif
        NULL);
    #endif

    g_object_connect (preferences,
        "signal::response", katze_preferences_response_cb, NULL,
        NULL);

    #ifdef HAVE_HILDON_2_2
    katze_preferences_size_request_cb (preferences, NULL);
    g_object_connect (preferences,
        "signal::size-request", katze_preferences_size_request_cb, NULL,
        NULL);
    #endif
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

static void
katze_preferences_prepare (KatzePreferences* preferences)
{
    KatzePreferencesPrivate* priv = preferences->priv;

    #if HAVE_HILDON
    GtkWidget* viewport;

    priv->scrolled = katze_scrolled_new (NULL, NULL);
    gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (preferences))),
                      priv->scrolled, TRUE, TRUE, 4);
    viewport = gtk_viewport_new (NULL, NULL);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_container_add (GTK_CONTAINER (priv->scrolled), viewport);
    priv->box = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (viewport), priv->box);

    priv->hbox = NULL;
    priv->sizegroup = NULL;
    priv->sizegroup2 = NULL;

    g_signal_connect (priv->scrolled, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &priv->scrolled);
    #else
    #ifdef HAVE_GRANITE
    /* FIXME: granite: should return GtkWidget* like GTK+ */
    priv->notebook = (GtkWidget*)granite_widgets_static_notebook_new (FALSE);
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
    priv->toolbar = NULL;
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
    #endif

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

/**
 * katze_preferences_add_category:
 * @preferences: a #KatzePreferences instance
 * @label: a category label
 * @icon: an icon name
 *
 * Adds a new category with the specified label to the dialog.
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

    #if HAVE_HILDON
    GtkWidget* widget;
    gchar* markup;

    if (!priv->scrolled)
        katze_preferences_prepare (preferences);

    widget = gtk_label_new (NULL);
    gtk_widget_show (widget);
    markup = g_markup_printf_escaped ("<b>%s</b>", label);
    gtk_label_set_markup (GTK_LABEL (widget), markup);
    g_free (markup);
    gtk_box_pack_start (GTK_BOX (priv->box), widget, TRUE, TRUE, 0);

    priv->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->sizegroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->hbox = NULL;
    #else
    if (!priv->notebook)
        katze_preferences_prepare (preferences);

    priv->page = gtk_vbox_new (FALSE, 0);
    priv->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    gtk_widget_show (priv->page);
    gtk_container_set_border_width (GTK_CONTAINER (priv->page), 4);
    #ifdef HAVE_GRANITE
    granite_widgets_static_notebook_append_page (
        GRANITE_WIDGETS_STATIC_NOTEBOOK (priv->notebook),
        priv->page, GTK_LABEL (gtk_label_new (label)));
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
    #endif

    return priv->page;
}

#if !HAVE_HILDON
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
#endif

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
    #if !HAVE_HILDON
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
    #endif
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
    #ifdef HAVE_HILDON_2_2
    else if (HILDON_IS_CHECK_BUTTON (widget) || HILDON_IS_PICKER_BUTTON (widget))
        _type = g_intern_string ("indented");
    #endif

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
        #if HAVE_HILDON
        if (!GTK_IS_SPIN_BUTTON (widget) && !GTK_IS_LABEL (widget))
        #else
        if (!GTK_IS_SPIN_BUTTON (widget))
        #endif
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

    #if HAVE_HILDON
    if (GTK_IS_BUTTON (widget) && !GTK_WIDGET_IS_SENSITIVE (widget))
        gtk_widget_hide (widget);
    #endif
}
