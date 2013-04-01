/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori.h"

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
        guint j;

        /* Skip properties of parent classes */
        if (pspec->owner_type != G_OBJECT_TYPE (object))
            continue;

        /* Verify that the ID is unique */
        if (pspecs[i]->owner_type == G_OBJECT_TYPE (object))
        for (j = 0; j < n_properties; j++)
            if (i != j && pspecs[j]->owner_type == G_OBJECT_TYPE (object))
                if (pspec->param_id == pspecs[j]->param_id)
                    g_error ("Duplicate ID %d of %s and %s",
                        pspec->param_id,
                        g_param_spec_get_name (pspec),
                        g_param_spec_get_name (pspecs[j]));
    }
  g_free (pspecs);
}

static void
properties_object_test (gconstpointer object)
{
    if (GTK_IS_WIDGET (object))
        g_object_ref_sink ((GObject*)object);

    properties_object_get_set ((GObject*)object);

    if (GTK_IS_WIDGET (object))
        gtk_widget_destroy (GTK_WIDGET (object));
    g_object_unref ((GObject*)object);
}

static void
properties_type_test (gconstpointer type)
{
    GObject* object;

    midori_test_log_set_fatal_handler_for_icons ();
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
    g_test_init (&argc, &argv, NULL);
    midori_app_setup (&argc, &argv, NULL);
    midori_paths_init (MIDORI_RUNTIME_MODE_PRIVATE, NULL);

    #ifndef HAVE_WEBKIT2
    g_object_set_data (G_OBJECT (webkit_get_default_session ()),
                       "midori-session-initialized", (void*)1);
    #endif

    g_test_add_data_func ("/properties/app",
        (gconstpointer)MIDORI_TYPE_APP, properties_type_test);
    g_test_add_data_func ("/properties/browser",
        (gconstpointer)MIDORI_TYPE_BROWSER, properties_type_test);
    g_test_add_data_func ("/properties/extension",
        (gconstpointer)MIDORI_TYPE_EXTENSION, properties_type_test);
    g_test_add_data_func ("/properties/location-action",
        (gconstpointer)MIDORI_TYPE_LOCATION_ACTION, properties_type_test);
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
