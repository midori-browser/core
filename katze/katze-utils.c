/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-utils.h"

#include <glib/gi18n.h>

#include <string.h>

static void
proxy_toggle_button_toggled_cb (GtkToggleButton* button, GObject* object)
{
    gboolean toggled = gtk_toggle_button_get_active (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, toggled, NULL);
}

static void
proxy_file_file_set_cb (GtkFileChooser* button,
                        GObject*        object)
{
    const gchar* file = gtk_file_chooser_get_filename (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, file, NULL);
}

static void
proxy_folder_file_set_cb (GtkFileChooser* button,
                          GObject*        object)
{
    const gchar* file = gtk_file_chooser_get_current_folder (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, file, NULL);
}

static void
proxy_uri_file_set_cb (GtkFileChooser* button,
                       GObject*        object)
{
    const gchar* file = gtk_file_chooser_get_uri (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, file, NULL);
}

static gchar*
proxy_combo_box_text_changed_cb (GtkComboBox* button, GObject* object)
{
    gchar* text = gtk_combo_box_get_active_text (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, text, NULL);
    return FALSE;
}

static gboolean
proxy_entry_focus_out_event_cb (GtkEntry*      entry,
                                GdkEventFocus* event,
                                GObject*       object)
{
    const gchar* text = gtk_entry_get_text (entry);
    const gchar* property = g_object_get_data (G_OBJECT (entry), "property");
    g_object_set (object, property, text, NULL);
    return FALSE;
}

static gboolean
proxy_spin_button_changed_cb (GtkSpinButton* button, GObject* object)
{
    gint value = gtk_spin_button_get_value_as_int (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, value, NULL);
    return FALSE;
}

static gchar*
proxy_combo_box_changed_cb (GtkComboBox* button, GObject* object)
{
    gint value = gtk_combo_box_get_active (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, value, NULL);
    return FALSE;
}

/**
 * katze_property_proxy:
 * @object: a #GObject
 * @property: the name of a property
 * @hint: a special hint
 *
 * Create a widget of an appropriate type to represent the specified
 * object's property. If the property is writable changes of the value
 * through the widget will be reflected in the value of the property.
 *
 * Supported values for @hint are as follows:
 *     "blurb": the blurb of the property will be used to provide a kind
 *         of label, instead of the name.
 *     "file": the widget created will be particularly suitable for
 *         choosing an existing filename.
 *     "folder": the widget created will be particularly suitable for
 *         choosing an existing folder.
 *     "uri": the widget created will be particularly suitable for
 *         choosing an existing filename, encoded as an URI.
 *     "font": the widget created will be particularly suitable for
 *         choosing a font from installed fonts.
 *
 * Any other values for @hint are silently ignored.
 *
 * Return value: a new widget
 **/
GtkWidget*
katze_property_proxy (gpointer     object,
                      const gchar* property,
                      const gchar* hint)
{
    g_return_val_if_fail (G_IS_OBJECT (object), NULL);
    GObjectClass* class = G_OBJECT_GET_CLASS (object);
    GParamSpec* pspec = g_object_class_find_property (class, property);
    if (!pspec)
    {
        g_warning (_("Property '%s' is invalid for %s"),
                   property, G_OBJECT_CLASS_NAME (class));
        return gtk_label_new (property);
    }
    GType type = G_PARAM_SPEC_TYPE (pspec);
    const gchar* nick = g_param_spec_get_nick (pspec);
    const gchar* _hint = g_intern_string (hint);
    if (_hint == g_intern_string ("blurb"))
        nick = g_param_spec_get_blurb (pspec);
    GtkWidget* widget;
    gchar* string = NULL;
    if (type == G_TYPE_PARAM_BOOLEAN)
    {
        widget = gtk_check_button_new_with_label (gettext (nick));
        gboolean toggled;
        g_object_get (object, property, &toggled, NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), toggled);
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (proxy_toggle_button_toggled_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("file"))
    {
        widget = gtk_file_chooser_button_new (_("Choose file"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget),
                                       string ? string : "");
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_file_file_set_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("folder"))
    {
        widget = gtk_file_chooser_button_new (_("Choose folder"),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget),
                                             string ? string : "");
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_folder_file_set_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("uri"))
    {
        widget = gtk_file_chooser_button_new (_("Choose file"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (widget),
                                  string ? string : "");
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_uri_file_set_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("font"))
    {
        widget = gtk_combo_box_new_text ();
        PangoContext* context = gtk_widget_get_pango_context (widget);
        PangoFontFamily** families;
        int n_families;
        pango_context_list_families (context, &families, &n_families);
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gint i = 0;
        while (i < n_families)
        {
            const gchar* font = pango_font_family_get_name (families[i]);
            gtk_combo_box_append_text (GTK_COMBO_BOX (widget), font);
            if (string && !strcmp (font, string))
                gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
            i++;
        }
        g_signal_connect (widget, "changed",
                          G_CALLBACK (proxy_combo_box_text_changed_cb), object);
        g_free (families);
    }
    else if (type == G_TYPE_PARAM_STRING)
    {
        widget = gtk_entry_new ();
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_entry_set_text (GTK_ENTRY (widget), string ? string : "");
        g_signal_connect (widget, "focus-out-event",
                          G_CALLBACK (proxy_entry_focus_out_event_cb), object);
    }
    else if (type == G_TYPE_PARAM_INT)
    {
        widget = gtk_spin_button_new_with_range (
            G_PARAM_SPEC_INT (pspec)->minimum,
            G_PARAM_SPEC_INT (pspec)->maximum, 1);
        gint value;
        g_object_get (object, property, &value, NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
        g_signal_connect (widget, "value-changed",
                          G_CALLBACK (proxy_spin_button_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_ENUM)
    {
        GEnumClass* enum_class = G_ENUM_CLASS (
            g_type_class_ref (pspec->value_type));
        widget = gtk_combo_box_new_text ();
        gint i = 0;
        while (i < enum_class->n_values)
        {
            const gchar* label = gettext (enum_class->values[i].value_nick);
            gtk_combo_box_append_text (GTK_COMBO_BOX (widget), label);
            i++;
        }
        gint value;
        g_object_get (object, property, &value, NULL);
        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), value);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (proxy_combo_box_changed_cb), object);
        g_type_class_unref (enum_class);
    }
    else
        widget = gtk_label_new (gettext (nick));
    g_free (string);

    gtk_widget_set_sensitive (widget, pspec->flags & G_PARAM_WRITABLE);

    g_object_set_data (G_OBJECT (widget), "property", (gchar*)property);

    return widget;
}

/**
 * katze_property_label:
 * @object: a #GObject
 * @property: the name of a property
 *
 * Create a label widget displaying the name of the specified object's property.
 *
 * Return value: a new label widget
 **/
GtkWidget*
katze_property_label (gpointer     object,
                      const gchar* property)
{
    g_return_val_if_fail (G_IS_OBJECT (object), NULL);
    GObjectClass* class = G_OBJECT_GET_CLASS (object);
    GParamSpec* pspec = g_object_class_find_property (class, property);
    if (!pspec)
    {
        g_warning (_("Property '%s' is invalid for %s"),
                   property, G_OBJECT_CLASS_NAME (class));
        return gtk_label_new (property);
    }
    const gchar* nick = g_param_spec_get_nick (pspec);
    GtkWidget* widget = gtk_label_new (nick);

    return widget;
}
