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
#include "midori-stock.h"
#include "sokoke.h"

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

int
main (int    argc,
      char** argv)
{
    midori_app_setup (argv);
    g_test_init (&argc, &argv, NULL);
    gtk_init_check (&argc, &argv);

    g_test_add_func ("/browser/create", browser_create);

    return g_test_run ();
}
