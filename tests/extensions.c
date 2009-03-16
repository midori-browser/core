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

const gpointer magic = (gpointer)0xdeadbeef;

static void
extension_activate_cb (MidoriExtension* extension,
                       MidoriApp*       app)
{
    g_object_set_data (G_OBJECT (extension), "activated", magic);
}

static void
extension_deactivate_cb (MidoriExtension* extension)
{
    g_object_set_data (G_OBJECT (extension), "deactivated", magic);
}

static void
extension_create (void)
{
    MidoriExtension* extension;

    extension = g_object_new (MIDORI_TYPE_EXTENSION, NULL);
    g_assert (!midori_extension_is_prepared (extension));
    g_object_set (extension, "name", "TestExtension",
                             "version", "1.0", NULL);
    g_assert (!midori_extension_is_prepared (extension));
    g_object_set (extension, "description", "Nothing but a test.",
                             "authors", "John Doe", NULL);
    /* FIXME: We should require to connect to "activate"
    g_assert (!midori_extension_is_prepared (extension)); */
    g_signal_connect (extension, "activate",
                      G_CALLBACK (extension_activate_cb), NULL);
    g_assert (midori_extension_is_prepared (extension));
    g_assert (!midori_extension_is_active (extension));
    g_signal_emit_by_name (extension, "activate", NULL);
    g_assert (midori_extension_is_active (extension));
    g_assert (g_object_get_data (G_OBJECT (extension), "activated") == magic);
    g_signal_connect (extension, "deactivate",
                      G_CALLBACK (extension_deactivate_cb), NULL);
    midori_extension_deactivate (extension);
    g_assert (!midori_extension_is_active (extension));
    g_assert (g_object_get_data (G_OBJECT (extension), "deactivated") == magic);
}

static MidoriExtension*
extension_mock_object (void)
{
    MidoriExtension* extension;

    extension = g_object_new (MIDORI_TYPE_EXTENSION,
                              "name", "TestExtension",
                              "version", "1.0",
                              "description", "Nothing but a test.",
                              "authors", "John Doe",
                              NULL);
    return extension;
}

static void
extension_settings (void)
{
    MidoriExtension* extension;
    gboolean nihilist;
    gint age;
    const gchar* lastname;

    extension = extension_mock_object ();
    midori_extension_install_boolean (extension, "nihilist", TRUE);
    nihilist = midori_extension_get_boolean (extension, "nihilist");
    g_assert (!nihilist);
    g_signal_connect (extension, "activate",
                      G_CALLBACK (extension_activate_cb), NULL);
    g_signal_emit_by_name (extension, "activate", NULL);
    nihilist = midori_extension_get_boolean (extension, "nihilist");
    g_assert (nihilist);
    midori_extension_set_boolean (extension, "nihilist", FALSE);
    nihilist = midori_extension_get_boolean (extension, "nihilist");
    g_assert (!nihilist);
    midori_extension_deactivate (extension);

    extension = extension_mock_object ();
    midori_extension_install_integer (extension, "age", 88);
    age = midori_extension_get_integer (extension, "age");
    g_assert_cmpint (age, ==, 0);
    g_signal_connect (extension, "activate",
                      G_CALLBACK (extension_activate_cb), NULL);
    g_signal_emit_by_name (extension, "activate", NULL);
    age = midori_extension_get_integer (extension, "age");
    g_assert_cmpint (age, ==, 88);
    midori_extension_set_integer (extension, "age", 66);
    age = midori_extension_get_integer (extension, "age");
    g_assert_cmpint (age, ==, 66);
    midori_extension_deactivate (extension);

    extension = extension_mock_object ();
    midori_extension_install_string (extension, "lastname", "Thomas Mann");
    lastname = midori_extension_get_string (extension, "lastname");
    g_assert_cmpstr (lastname, ==, NULL);
    g_signal_connect (extension, "activate",
                      G_CALLBACK (extension_activate_cb), NULL);
    g_signal_emit_by_name (extension, "activate", NULL);
    lastname = midori_extension_get_string (extension, "lastname");
    g_assert_cmpstr (lastname, ==, "Thomas Mann");
    midori_extension_set_string (extension, "lastname", "Theodor Fontane");
    lastname = midori_extension_get_string (extension, "lastname");
    g_assert_cmpstr (lastname, ==, "Theodor Fontane");
    midori_extension_deactivate (extension);
}

int
main (int    argc,
      char** argv)
{
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/extensions/create", extension_create);
    g_test_add_func ("/extensions/settings", extension_settings);

    return g_test_run ();
}
