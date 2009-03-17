/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "midori.h"
#include "midori-bookmarks.h"
#include "sokoke.h"

#define pspec_is_writable(pspec) (pspec->flags & G_PARAM_WRITABLE \
    && !(pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)))

static void
properties_object_get_set (GObject* object)
{
    GObjectClass* class;
    GParamSpec** pspecs;
    guint i, n_properties;

    class = G_OBJECT_GET_CLASS (object);
    pspecs = g_object_class_list_properties (class, &n_properties);
    for (i = 0; i < n_properties; i++)
    {
        GParamSpec *pspec = pspecs[i];
        GType type = G_PARAM_SPEC_TYPE (pspec);
        const gchar* property = g_param_spec_get_name (pspec);
        void* value = NULL;

        /* Skip properties of parent classes */
        if (pspec->owner_type != G_OBJECT_TYPE (object))
            continue;

        g_object_get (object, property, &value, NULL);
        if (type == G_TYPE_PARAM_BOOLEAN)
        {
            gboolean current_value = value ? TRUE : FALSE;
            gboolean default_value = G_PARAM_SPEC_BOOLEAN (pspec)->default_value;
            if (current_value != default_value)
                g_error ("Set %s.%s to default (%d), but returned '%d'",
                    G_OBJECT_TYPE_NAME (object), property,
                    G_PARAM_SPEC_BOOLEAN (pspec)->default_value, current_value);
            if (pspec_is_writable (pspec))
            {
                g_object_set (object, property, !default_value, NULL);
                g_object_get (object, property, &current_value, NULL);
                if (current_value == default_value)
                    g_error ("Set %s.%s to non-default (%d), but returned '%d'",
                        G_OBJECT_TYPE_NAME (object), property,
                        !G_PARAM_SPEC_BOOLEAN (pspec)->default_value, current_value);
                g_object_set (object, property, default_value, NULL);
                g_object_get (object, property, &current_value, NULL);
                if (current_value != default_value)
                    g_error ("Set %s.%s to default again (%d), but returned '%d'",
                        G_OBJECT_TYPE_NAME (object), property,
                        G_PARAM_SPEC_BOOLEAN (pspec)->default_value, current_value);
            }
        }
        else if (type == G_TYPE_PARAM_STRING)
        {
            g_free (value);
            if (pspec_is_writable (pspec))
            {
                g_object_set (object, property,
                    G_PARAM_SPEC_STRING (pspec)->default_value, NULL);
                g_object_get (object, property, &value, NULL);
                if (g_strcmp0 (value, G_PARAM_SPEC_STRING (pspec)->default_value))
                    g_error ("Set %s.%s to %s, but returned '%s'",
                        G_OBJECT_TYPE_NAME (object), property,
                            G_PARAM_SPEC_STRING (pspec)->default_value, (gchar*)value);
                g_free (value);
            }
        }
        else if (type == G_TYPE_PARAM_ENUM)
        {
            GEnumClass* enum_class = G_ENUM_CLASS (
                g_type_class_ref (pspec->value_type));
            gint j;

            if (pspec_is_writable (pspec))
            {
                g_object_set (object, property,
                    G_PARAM_SPEC_ENUM (pspec)->default_value, NULL);
                for (j = enum_class->minimum; j < enum_class->maximum; j++)
                {
                    GEnumValue* enum_value = g_enum_get_value (enum_class, j);
                    if (!enum_value)
                        g_error ("%s.%s has no value %d",
                            G_OBJECT_TYPE_NAME (object), property, j);
                    GEnumValue* enum_value_ = g_enum_get_value_by_name (enum_class,
                        enum_value->value_name);
                    if (!enum_value)
                        g_error ("%s.%s has no value '%s'",
                            G_OBJECT_TYPE_NAME (object), property, enum_value->value_name);
                    g_assert_cmpint (enum_value->value, ==, enum_value_->value);
                    g_object_set (object, property, j, NULL);
                }
            }

            g_type_class_unref (enum_class);
        }
    }
  g_free (pspecs);
}

static void
properties_object_test (gconstpointer object)
{
    if (GTK_IS_OBJECT (object))
        g_object_ref_sink ((GObject*)object);

    properties_object_get_set ((GObject*)object);

    if (GTK_IS_OBJECT (object))
        gtk_object_destroy (GTK_OBJECT (object));
    g_object_unref ((GObject*)object);
}

static void
properties_type_test (gconstpointer type)
{
    GObject* object;

    object = g_object_new ((GType)type, NULL);
    properties_object_test ((gconstpointer)object);
}

GtkWidget*
midori_dummy_viewable_new (const gchar* stock_id,
                           const gchar* label,
                           GtkWidget*   toolbar);

GtkWidget* dummy_viewable_new (void)
{
    GtkWidget* toolbar = gtk_toolbar_new ();
    return midori_dummy_viewable_new (GTK_STOCK_ABOUT, "Test", toolbar);
}

int
main (int    argc,
      char** argv)
{
    /* libSoup uses threads, therefore if WebKit is built with libSoup
       or Midori is using it, we need to initialize threads. */
    if (!g_thread_supported ()) g_thread_init (NULL);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);
    sokoke_register_stock_items ();

    g_test_add_data_func ("/properties/app",
        (gconstpointer)MIDORI_TYPE_APP, properties_type_test);
    g_test_add_data_func ("/properties/browser",
        (gconstpointer)MIDORI_TYPE_BROWSER, properties_type_test);
    g_test_add_data_func ("/properties/extension",
        (gconstpointer)MIDORI_TYPE_EXTENSION, properties_type_test);
    g_test_add_data_func ("/properties/location-action",
        (gconstpointer)MIDORI_TYPE_LOCATION_ACTION, properties_type_test);
    g_test_add_data_func ("/properties/location-entry",
        (gconstpointer)MIDORI_TYPE_LOCATION_ENTRY, properties_type_test);
    g_test_add_data_func ("/properties/panel",
        (gconstpointer)MIDORI_TYPE_PANEL, properties_type_test);
    g_test_add_data_func ("/properties/preferences",
        (gconstpointer)MIDORI_TYPE_PREFERENCES, properties_type_test);
    g_test_add_data_func ("/properties/search-action",
        (gconstpointer)MIDORI_TYPE_SEARCH_ACTION, properties_type_test);
    g_test_add_data_func ("/properties/view",
        (gconstpointer)MIDORI_TYPE_VIEW, properties_type_test);
    g_test_add_data_func ("/properties/viewable",
        (gconstpointer)dummy_viewable_new (), properties_object_test);
    g_test_add_data_func ("/properties/web-settings",
        (gconstpointer)MIDORI_TYPE_WEB_SETTINGS, properties_type_test);

    return g_test_run ();
}
