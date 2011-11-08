/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori.h"

static void
browser_create (void)
{
    MidoriApp* app;
    MidoriBrowser* browser;
    GtkActionGroup* action_group;
    GList* actions;

    app = midori_app_new ();
    browser = midori_app_create_browser (app);
    gtk_widget_destroy (GTK_WIDGET (browser));

    app = midori_app_new ();
    browser = midori_app_create_browser (app);
    action_group = midori_browser_get_action_group (browser);
    actions = gtk_action_group_list_actions (action_group);
    while (actions)
    {
        GtkAction* action = actions->data;
        if (g_strcmp0 (gtk_action_get_name (action), "WindowClose"))
            if (g_strcmp0 (gtk_action_get_name (action), "EncodingCustom"))
                if (g_strcmp0 (gtk_action_get_name (action), "AddSpeedDial"))
                    if (g_strcmp0 (gtk_action_get_name (action), "PrivateBrowsing"))
                        if (g_strcmp0 (gtk_action_get_name (action), "AddDesktopShortcut"))
                            gtk_action_activate (action);
        actions = g_list_next (actions);
    }
    g_list_free (actions);
    gtk_widget_destroy (GTK_WIDGET (browser));
    g_object_unref (app);
}

static void
browser_tooltips (void)
{
    MidoriBrowser* browser;
    GtkActionGroup* action_group;
    GList* actions;
    gchar* toolbar;
    guint errors = 0;

    browser = midori_browser_new ();
    action_group = midori_browser_get_action_group (browser);
    actions = gtk_action_group_list_actions (action_group);
    toolbar = g_strjoinv (" ", (gchar**)midori_browser_get_toolbar_actions (browser));

    while (actions)
    {
        GtkAction* action = actions->data;
        const gchar* name = gtk_action_get_name (action);

        if (strstr ("CompactMenu Location Separator", name))
        {
            actions = g_list_next (actions);
            continue;
        }

        if (strstr (toolbar, name) != NULL)
        {
            if (!gtk_action_get_tooltip (action))
            {
                printf ("'%s' can be toolbar item but tooltip is unset\n", name);
                errors++;
            }
        }
        else
        {
            if (gtk_action_get_tooltip (action))
            {
                printf ("'%s' is no toolbar item but tooltip is set\n", name);
                errors++;
            }
        }
        actions = g_list_next (actions);
    }
    g_free (toolbar);
    g_list_free (actions);
    gtk_widget_destroy (GTK_WIDGET (browser));

    if (errors)
        g_error ("Tooltip errors");
}

int
main (int    argc,
      char** argv)
{
    midori_app_setup (argv);
    g_object_set_data (G_OBJECT (webkit_get_default_session ()),
                       "midori-session-initialized", (void*)1);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/browser/create", browser_create);
    g_test_add_func ("/browser/tooltips", browser_tooltips);

    return g_test_run ();
}
