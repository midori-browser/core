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
proxy_toggle_button_toggled_cb (GtkToggleButton* button,
                                GObject*         object)
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

static void
proxy_combo_box_text_changed_cb (GtkComboBox* button,
                                 GObject*     object)
{
    gchar* text = gtk_combo_box_get_active_text (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, text, NULL);
}

static void
proxy_entry_activate_cb (GtkEntry* entry,
                         GObject*  object)
{
    const gchar* text = gtk_entry_get_text (entry);
    const gchar* property = g_object_get_data (G_OBJECT (entry), "property");
    g_object_set (object, property, text, NULL);
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

static void
proxy_spin_button_changed_cb (GtkSpinButton* button,
                              GObject*       object)
{
    GObjectClass* class = G_OBJECT_GET_CLASS (object);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    GParamSpec* pspec = g_object_class_find_property (class, property);
    if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_PARAM_INT)
    {
        gint value = gtk_spin_button_get_value_as_int (button);
        g_object_set (object, property, value, NULL);
    }
    else
    {
        gdouble value = gtk_spin_button_get_value (button);
        g_object_set (object, property, value, NULL);
    }
}

static void
proxy_combo_box_changed_cb (GtkComboBox* button,
                            GObject*     object)
{
    gint value = gtk_combo_box_get_active (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, value, NULL);
}

static void
proxy_object_notify_boolean_cb (GObject*    object,
                                GParamSpec* pspec,
                                GtkWidget*  proxy)
{
    const gchar* property = g_object_get_data (G_OBJECT (proxy), "property");
    gboolean value = katze_object_get_boolean (object, property);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy), value);
}

static void
proxy_object_notify_string_cb (GObject*    object,
                               GParamSpec* pspec,
                               GtkWidget*  proxy)
{
    const gchar* property = g_object_get_data (G_OBJECT (proxy), "property");
    gchar* value = katze_object_get_string (object, property);
    gtk_entry_set_text (GTK_ENTRY (proxy), value);
    g_free (value);
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
 * Since 0.1.2 strings without hints and booleans are truly synchronous
 * including property changes causing the proxy to be updated.
 *
 * Return value: a new widget
 **/
GtkWidget*
katze_property_proxy (gpointer     object,
                      const gchar* property,
                      const gchar* hint)
{
    GObjectClass* class;
    GParamSpec* pspec;
    GType type;
    const gchar* nick;
    const gchar* _hint;
    GtkWidget* widget;
    gchar* string;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);

    class = G_OBJECT_GET_CLASS (object);
    pspec = g_object_class_find_property (class, property);
    if (!pspec)
    {
        g_warning (_("Property '%s' is invalid for %s"),
                   property, G_OBJECT_CLASS_NAME (class));
        return gtk_label_new (property);
    }

    type = G_PARAM_SPEC_TYPE (pspec);
    nick = g_param_spec_get_nick (pspec);
    _hint = g_intern_string (hint);
    if (_hint == g_intern_string ("blurb"))
        nick = g_param_spec_get_blurb (pspec);
    string = NULL;
    if (type == G_TYPE_PARAM_BOOLEAN)
    {
        gchar* notify_property;
        gboolean toggled = katze_object_get_boolean (object, property);

        widget = gtk_check_button_new_with_label (gettext (nick));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), toggled);
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (proxy_toggle_button_toggled_cb), object);
        notify_property = g_strdup_printf ("notify::%s", property);
        g_signal_connect (object, notify_property,
            G_CALLBACK (proxy_object_notify_boolean_cb), widget);
        g_free (notify_property);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("file"))
    {
        string = katze_object_get_string (object, property);

        widget = gtk_file_chooser_button_new (_("Choose file"),
            GTK_FILE_CHOOSER_ACTION_OPEN);

        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget),
                                       string ? string : "");
        #if GTK_CHECK_VERSION (2, 12, 0)
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_file_file_set_cb), object);
        #else
        if (pspec->flags & G_PARAM_WRITABLE)
            g_signal_connect (widget, "selection-changed",
                              G_CALLBACK (proxy_file_file_set_cb), object);
        #endif
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("folder"))
    {
        string = katze_object_get_string (object, property);

        widget = gtk_file_chooser_button_new (_("Choose folder"),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget),
                                             string ? string : "");
        #if GTK_CHECK_VERSION (2, 12, 0)
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_folder_file_set_cb), object);
        #else
        if (pspec->flags & G_PARAM_WRITABLE)
            g_signal_connect (widget, "selection-changed",
                              G_CALLBACK (proxy_folder_file_set_cb), object);
        #endif
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("uri"))
    {
        string = katze_object_get_string (object, property);

        widget = gtk_file_chooser_button_new (_("Choose file"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (widget),
                                  string ? string : "");
        #if GTK_CHECK_VERSION (2, 12, 0)
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_uri_file_set_cb), object);
        #else
        if (pspec->flags & G_PARAM_WRITABLE)
            g_signal_connect (widget, "selection-changed",
                              G_CALLBACK (proxy_uri_file_set_cb), object);
        #endif
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == g_intern_string ("font"))
    {
        int n_families, i;
        PangoContext* context;
        PangoFontFamily** families;
        string = katze_object_get_string (object, property);

        widget = gtk_combo_box_new_text ();
        context = gtk_widget_get_pango_context (widget);
        pango_context_list_families (context, &families, &n_families);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        for (i = 0; i < n_families; i++)
        {
            const gchar* font = pango_font_family_get_name (families[i]);
            gtk_combo_box_append_text (GTK_COMBO_BOX (widget), font);
            if (string && !strcmp (font, string))
                gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
        }
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (
            gtk_combo_box_get_model (GTK_COMBO_BOX (widget))),
                                     0, GTK_SORT_ASCENDING);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (proxy_combo_box_text_changed_cb), object);
        g_free (families);
    }
    else if (type == G_TYPE_PARAM_STRING)
    {
        gchar* notify_property;

        widget = gtk_entry_new ();
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_entry_set_text (GTK_ENTRY (widget), string ? string : "");
        g_signal_connect (widget, "activate",
                          G_CALLBACK (proxy_entry_activate_cb), object);
        g_signal_connect (widget, "focus-out-event",
                          G_CALLBACK (proxy_entry_focus_out_event_cb), object);
        notify_property = g_strdup_printf ("notify::%s", property);
        g_signal_connect (object, notify_property,
            G_CALLBACK (proxy_object_notify_string_cb), widget);
        g_free (notify_property);
    }
    else if (type == G_TYPE_PARAM_FLOAT)
    {
        gfloat value = katze_object_get_float (object, property);

        widget = gtk_spin_button_new_with_range (
            G_PARAM_SPEC_FLOAT (pspec)->minimum,
            G_PARAM_SPEC_FLOAT (pspec)->maximum, 1);
        /* Keep it narrow, 5 + 2 digits are usually fine */
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5 + 2);
        gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 2);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
        g_signal_connect (widget, "value-changed",
                          G_CALLBACK (proxy_spin_button_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_INT)
    {
        gint value = katze_object_get_int (object, property);

        widget = gtk_spin_button_new_with_range (
            G_PARAM_SPEC_INT (pspec)->minimum,
            G_PARAM_SPEC_INT (pspec)->maximum, 1);
        /* Keep it narrow, 5 digits are usually fine */
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
        g_signal_connect (widget, "value-changed",
                          G_CALLBACK (proxy_spin_button_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_ENUM)
    {
        guint i;
        GEnumClass* enum_class = G_ENUM_CLASS (
            g_type_class_ref (pspec->value_type));
        gint value = katze_object_get_enum (object, property);

        widget = gtk_combo_box_new_text ();
        for (i = 0; i < enum_class->n_values; i++)
        {
            const gchar* label = gettext (enum_class->values[i].value_nick);
            gtk_combo_box_append_text (GTK_COMBO_BOX (widget), label);
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), value);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (proxy_combo_box_changed_cb), object);
        g_type_class_unref (enum_class);
    }
    else
        widget = gtk_label_new (gettext (nick));
    g_free (string);

    gtk_widget_set_tooltip_text (widget, g_param_spec_get_blurb (pspec));
    gtk_widget_set_sensitive (widget, pspec->flags & G_PARAM_WRITABLE);

    g_object_set_data_full (G_OBJECT (widget), "property",
                            g_strdup (property), g_free);

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
    GObjectClass* class;
    GParamSpec* pspec;
    const gchar* nick;
    GtkWidget* widget;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);

    class = G_OBJECT_GET_CLASS (object);
    pspec = g_object_class_find_property (class, property);
    if (!pspec)
    {
        g_warning (_("Property '%s' is invalid for %s"),
                   property, G_OBJECT_CLASS_NAME (class));
        return gtk_label_new (property);
    }
    nick = g_param_spec_get_nick (pspec);
    widget = gtk_label_new (nick);

    return widget;
}

typedef struct
{
     GtkWidget* widget;
     KatzeMenuPos position;
} KatzePopupInfo;

static void
katze_widget_popup_position_menu (GtkMenu*  menu,
                                  gint*     x,
                                  gint*     y,
                                  gboolean* push_in,
                                  gpointer  user_data)
{
    gint wx, wy;
    gint menu_width;
    GtkRequisition menu_req;
    GtkRequisition widget_req;
    KatzePopupInfo* info = user_data;
    GtkWidget* widget = info->widget;
    gint widget_height;

    /* Retrieve size and position of both widget and menu */
    if (GTK_WIDGET_NO_WINDOW (widget))
    {
        gdk_window_get_position (widget->window, &wx, &wy);
        wx += widget->allocation.x;
        wy += widget->allocation.y;
    }
    else
        gdk_window_get_origin (widget->window, &wx, &wy);
    gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);
    gtk_widget_size_request (widget, &widget_req);
    menu_width = menu_req.width;
    widget_height = widget_req.height; /* Better than allocation.height */

    /* Calculate menu position */
    if (info->position == KATZE_MENU_POSITION_CURSOR)
        ; /* Do nothing? */
    else if (info->position == KATZE_MENU_POSITION_RIGHT)
    {
        *x = wx + widget->allocation.width - menu_width;
        *y = wy + widget_height;
    } else if (info->position == KATZE_MENU_POSITION_LEFT)
    {
        *x = wx;
        *y = wy + widget_height;
    }

    *push_in = TRUE;
}

/**
 * katze_widget_popup:
 * @widget: a widget
 * @menu: the menu to popup
 * @event: a button event, or %NULL
 * @pos: the preferred positioning
 *
 * Pops up the given menu relative to @widget. Use this
 * instead of writing custom positioning functions.
 *
 * Return value: a new label widget
 **/
void
katze_widget_popup (GtkWidget*      widget,
                    GtkMenu*        menu,
                    GdkEventButton* event,
                    KatzeMenuPos    pos)
{
    int button, event_time;
    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    if (!gtk_menu_get_attach_widget (menu))
        gtk_menu_attach_to_widget (menu, widget, NULL);


    if (widget)
    {
        KatzePopupInfo info = { widget, pos };
        gtk_menu_popup (menu, NULL, NULL,
                        katze_widget_popup_position_menu, &info,
                        button, event_time);
    }
    else
        gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, event_time);
}

/**
 * katze_image_menu_item_new_ellipsized:
 * @label: a label or %NULL
 *
 * Creates an image menu item where the label is
 * reasonably ellipsized for you.
 *
 * Return value: a new label widget
 **/
GtkWidget*
katze_image_menu_item_new_ellipsized (const gchar* label)
{
    GtkWidget* menuitem;
    GtkWidget* label_widget;

    menuitem = gtk_image_menu_item_new ();
    label_widget = gtk_label_new (label);
    /* FIXME: Should text direction be respected here? */
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.0);
    gtk_label_set_max_width_chars (GTK_LABEL (label_widget), 50);
    gtk_label_set_ellipsize (GTK_LABEL (label_widget), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_show (label_widget);
    gtk_container_add (GTK_CONTAINER (menuitem), label_widget);

    return menuitem;
}

/**
 * katze_pixbuf_new_from_buffer:
 * @buffer: Buffer with image data
 * @length: Length of the buffer
 * @mime_type: a MIME type, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new #GdkPixbuf out of the specified buffer.
 *
 * You can specify a MIME type if looking at the buffer
 * is not enough to determine the right type.
 *
 * Return value: A newly-allocated #GdkPixbuf
 **/
GdkPixbuf*
katze_pixbuf_new_from_buffer (const guchar* buffer,
                              gsize         length,
                              const gchar*  mime_type,
                              GError**      error)
{
    /* Proposed for inclusion in GdkPixbuf
       See http://bugzilla.gnome.org/show_bug.cgi?id=74291 */
    GdkPixbufLoader* loader;
    GdkPixbuf* pixbuf;

    g_return_val_if_fail (buffer != NULL, NULL);
    g_return_val_if_fail (length > 0, NULL);

    if (mime_type)
    {
        loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, error);
        if (!loader)
            return NULL;
    }
    else
        loader = gdk_pixbuf_loader_new ();
    if (!gdk_pixbuf_loader_write (loader, buffer, length, error))
    {
        g_object_unref (loader);
        return NULL;
    }
    if (!gdk_pixbuf_loader_close (loader, error))
    {
        g_object_unref (loader);
        return NULL;
    }

    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    g_object_ref (pixbuf);
    g_object_unref (loader);
    return pixbuf;
}

/**
 * katze_object_has_property:
 * @object: a #GObject
 * @property: the name of the property
 *
 * Determine if @object has a property with the specified name.
 *
 * Return value: a boolean
 *
 * Since: 0.1.2
 **/
gboolean
katze_object_has_property (gpointer     object,
                           const gchar* property)
{
    GObjectClass* class;

    g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

    class = G_OBJECT_GET_CLASS (object);
    return g_object_class_find_property (class, property) != NULL;
}

/**
 * katze_object_get_boolean:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the boolean value of the specified property.
 *
 * Return value: a boolean
 **/
gboolean
katze_object_get_boolean (gpointer     object,
                          const gchar* property)
{
    gboolean value = FALSE;

    g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_int:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the integer value of the specified property.
 *
 * Return value: an integer
 **/
gint
katze_object_get_int (gpointer     object,
                      const gchar* property)
{
    gint value = -1;

    g_return_val_if_fail (G_IS_OBJECT (object), -1);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_float:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the float value of the specified property.
 *
 * Return value: a float
 **/
gfloat
katze_object_get_float (gpointer     object,
                        const gchar* property)
{
    gfloat value = -1.0f;

    g_return_val_if_fail (G_IS_OBJECT (object), -1.0f);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_enum:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the enum value of the specified property.
 *
 * Return value: an enumeration
 **/
gint
katze_object_get_enum (gpointer     object,
                       const gchar* property)
{
    gint value = -1;

    g_return_val_if_fail (G_IS_OBJECT (object), -1);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_string:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the string value of the specified property.
 *
 * Return value: a newly allocated string
 **/
gchar*
katze_object_get_string (gpointer     object,
                         const gchar* property)
{
    gchar* value = NULL;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_object:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the object value of the specified property.
 *
 * Return value: an object
 **/
gpointer
katze_object_get_object (gpointer     object,
                         const gchar* property)
{
    GObject* value = NULL;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}
