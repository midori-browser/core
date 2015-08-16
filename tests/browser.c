/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori.h"

#ifndef HAVE_WEBKIT2
static void
browser_create (void)
{
    MidoriApp* app;
    MidoriWebSettings* settings;
    MidoriBrowser* browser;
    gchar* temporary_downloads;
    gchar* temporary_filename;
    GtkWidget* view;
    GFile* file;
    gchar* uri;
    gchar* filename;
    gchar* filename2;

    midori_test_log_set_fatal_handler_for_icons ();

    app = midori_app_new (NULL);
    settings = midori_web_settings_new ();
    g_object_set (app, "settings", settings, NULL);
    browser = midori_app_create_browser (app);
    file = g_file_new_for_commandline_arg ("./data/about.css");
    uri = g_file_get_uri (file);
    g_object_unref (file);
    view = midori_browser_add_uri (browser, uri);

    midori_test_set_dialog_response (GTK_RESPONSE_OK);
    temporary_downloads = midori_paths_make_tmp_dir ("saveXXXXXX");
    temporary_filename = g_build_filename (temporary_downloads, "test.html", NULL);
    midori_test_set_file_chooser_filename (temporary_filename);
    midori_settings_set_download_folder (MIDORI_SETTINGS (settings), temporary_downloads);
    midori_browser_save_uri (browser, MIDORI_VIEW (view), NULL);

    filename = midori_view_save_source (MIDORI_VIEW (view), NULL, NULL, FALSE);
    filename2 = g_filename_from_uri (uri, NULL, NULL);
    g_assert_cmpstr (filename, ==, filename2);
    g_free (filename);
    g_free (filename2);

    /* View source for local file: should NOT use temporary file */
    view = midori_browser_add_uri (browser, uri);
    midori_browser_set_current_tab (browser, view);
    g_assert_cmpstr (uri, ==, midori_browser_get_current_uri (browser));
    g_free (uri);
    g_free (temporary_downloads);
    g_free (temporary_filename);

    gtk_widget_destroy (GTK_WIDGET (browser));
    g_object_unref (settings);
    g_object_unref (app);
}
#endif

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

static void
browser_site_data (void)
{
    typedef struct
    {
        const gchar* url;
        MidoriSiteDataPolicy policy;
    } PolicyItem;

    static const PolicyItem items[] = {
    { "google.com", MIDORI_SITE_DATA_BLOCK },
    { "facebook.com", MIDORI_SITE_DATA_BLOCK },
    { "bugzilla.gnome.org", MIDORI_SITE_DATA_PRESERVE },
    { "bugs.launchpad.net", MIDORI_SITE_DATA_ACCEPT },
    };

    const gchar* rules = "-google.com,-facebook.com,!bugzilla.gnome.org,+bugs.launchpad.net";
    MidoriWebSettings* settings = g_object_new (MIDORI_TYPE_WEB_SETTINGS,
        "site-data-rules", rules, NULL);

    guint i;
    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        MidoriSiteDataPolicy policy = midori_web_settings_get_site_data_policy (
            settings, items[i].url);
        if (policy != items[i].policy)
            g_error ("Match '%s' yields %d but %d expected",
                     items[i].url, policy, items[i].policy);
    }
    g_object_unref (settings);
}

static void
browser_block_uris (void)
{
    MidoriWebSettings* settings = g_object_new (MIDORI_TYPE_WEB_SETTINGS, NULL);
    gchar* pattern = katze_object_get_string (settings, "block-uris");
    g_object_set (settings, "block-uris", NULL, NULL);
    g_object_set (settings, "block-uris", "", NULL);
    g_object_set (settings, "block-uris", "^(?!.*?(gmail|mail\\.google|accounts\\.google)).*", NULL);
    g_free (pattern);
    g_object_unref (settings);
}

static void
browser_appmenu_visibility (void)
{
    MidoriApp* app = midori_app_new (NULL);
    MidoriBrowser* browser = midori_app_create_browser (app);
    GtkToolItem* appmenu = midori_window_get_tool_item (MIDORI_WINDOW (browser), "CompactMenu");
    gboolean menubar_visible;
    gboolean appmenu_visible;

    midori_test_log_set_fatal_handler_for_icons ();

    g_object_get (appmenu, "visible", &appmenu_visible, NULL);
    g_object_get (browser, "show-menubar", &menubar_visible, NULL);
    g_assert (menubar_visible == !appmenu_visible);

    g_object_set (browser, "show-menubar", !menubar_visible, NULL);

    g_object_get (appmenu, "visible", &appmenu_visible, NULL);
    g_object_get (browser, "show-menubar", &menubar_visible, NULL);
    g_assert (menubar_visible == !appmenu_visible);

    g_object_set (browser, "show-menubar", TRUE, NULL);

    g_object_get (appmenu, "visible", &appmenu_visible, NULL);
    g_object_get (browser, "show-menubar", &menubar_visible, NULL);
    g_assert (menubar_visible && !appmenu_visible);

    g_object_set (browser, "show-menubar", FALSE, NULL);

    g_object_get (appmenu, "visible", &appmenu_visible, NULL);
    g_object_get (browser, "show-menubar", &menubar_visible, NULL);
    g_assert (!menubar_visible && appmenu_visible);

    gtk_widget_destroy (GTK_WIDGET (browser));
    g_object_unref (app);
}

int
main (int    argc,
      char** argv)
{
    g_test_init (&argc, &argv, NULL);
    midori_app_setup (&argc, &argv, NULL);
    midori_paths_init (MIDORI_RUNTIME_MODE_NORMAL, NULL);

    #ifndef HAVE_WEBKIT2
    g_object_set_data (G_OBJECT (webkit_get_default_session ()),
                       "midori-session-initialized", (void*)1);
    g_test_add_func ("/browser/create", browser_create);
    #endif                   

    g_test_add_func ("/browser/tooltips", browser_tooltips);
    g_test_add_func ("/browser/site_data", browser_site_data);
    g_test_add_func ("/browser/block_uris", browser_block_uris);
    g_test_add_func ("/browser/appmenu_visibility", browser_appmenu_visibility);

    return g_test_run ();
}
