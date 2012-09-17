/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-preferences.h"

#include "midori-app.h"
#include "midori-core.h"
#include "midori-platform.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>

#if WEBKIT_CHECK_VERSION (1, 3, 11)
    #define LIBSOUP_USE_UNSTABLE_REQUEST_API
    #include <libsoup/soup-cache.h>
#endif

#include <config.h>
#if HAVE_LIBNOTIFY
    #include <libnotify/notify.h>
#endif

struct _MidoriPreferences
{
    KatzePreferences parent_instance;

    gpointer settings;
};

G_DEFINE_TYPE (MidoriPreferences, midori_preferences, KATZE_TYPE_PREFERENCES);

enum
{
    PROP_0,

    PROP_SETTINGS
};

static void
midori_preferences_finalize (GObject* object);

static void
midori_preferences_set_property (GObject*      object,
                                 guint         prop_id,
                                 const GValue* value,
                                 GParamSpec*   pspec);

static void
midori_preferences_get_property (GObject*    object,
                                 guint       prop_id,
                                 GValue*     value,
                                 GParamSpec* pspec);

static void
midori_preferences_class_init (MidoriPreferencesClass* class)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_preferences_finalize;
    gobject_class->set_property = midori_preferences_set_property;
    gobject_class->get_property = midori_preferences_get_property;

    /**
     * MidoriPreferences:settings:
     *
     * The settings to proxy properties from.
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "Settings instance to provide properties",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
midori_preferences_init (MidoriPreferences* preferences)
{
    preferences->settings = NULL;
}

static void
midori_preferences_finalize (GObject* object)
{
    G_OBJECT_CLASS (midori_preferences_parent_class)->finalize (object);
}

static void
midori_preferences_set_property (GObject*      object,
                                 guint         prop_id,
                                 const GValue* value,
                                 GParamSpec*   pspec)
{
    MidoriPreferences* preferences = MIDORI_PREFERENCES (object);

    switch (prop_id)
    {
    case PROP_SETTINGS:
        midori_preferences_set_settings (preferences,
                                         g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_preferences_get_property (GObject*    object,
                                 guint       prop_id,
                                 GValue*     value,
                                 GParamSpec* pspec)
{
    MidoriPreferences* preferences = MIDORI_PREFERENCES (object);

    switch (prop_id)
    {
    case PROP_SETTINGS:
        g_value_set_object (value, preferences->settings);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_preferences_new:
 * @parent: the parent window
 * @settings: the settings
 *
 * Creates a new preferences dialog.
 *
 * Since 0.1.2 @parent may be %NULL.
 *
 * Return value: a new #MidoriPreferences
 **/
GtkWidget*
midori_preferences_new (GtkWindow*         parent,
                        MidoriWebSettings* settings)
{
    MidoriPreferences* preferences;

    g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);
    g_return_val_if_fail (MIDORI_IS_WEB_SETTINGS (settings), NULL);

    preferences = g_object_new (MIDORI_TYPE_PREFERENCES,
                                "transient-for", parent,
                                "settings", settings,
                                NULL);

    return GTK_WIDGET (preferences);
}

static void
midori_preferences_homepage_current_clicked_cb (GtkWidget*         button,
                                                MidoriWebSettings* settings)
{
    GtkWidget* preferences = gtk_widget_get_toplevel (button);
    GtkWidget* browser = katze_object_get_object (preferences, "transient-for");

    if (GTK_IS_WINDOW (browser))
    {
        gchar* uri = katze_object_get_string (browser, "uri");
        if (uri && *uri)
            g_object_set (settings, "homepage", uri, NULL);
        else
            g_object_set (settings, "homepage", "about:blank", NULL);
        g_free (uri);
    }
}

#if !HAVE_HILDON
static void
midori_preferences_notify_proxy_type_cb (MidoriWebSettings* settings,
                                         GParamSpec*        pspec,
                                         GtkWidget*         entry)
{
    MidoriProxy proxy_type = katze_object_get_enum (settings, "proxy-type");

    gtk_widget_set_sensitive (entry, proxy_type == MIDORI_PROXY_HTTP);
}
#endif

#if HAVE_OSX
static void
midori_preferences_toolbutton_clicked_cb (GtkWidget* toolbutton,
                                          GtkWidget* page)
{
    gpointer notebook = g_object_get_data (G_OBJECT (toolbutton), "notebook");
    guint n = gtk_notebook_page_num (notebook, page);
    gtk_notebook_set_current_page (notebook, n);
}
#endif

static inline void
midori_preferences_add_toolbutton (GtkWidget*   toolbar,
                                   GtkWidget**  toolbutton,
                                   const gchar* icon,
                                   const gchar* label,
                                   GtkWidget*   page)
{
#if HAVE_OSX
    *toolbutton = GTK_WIDGET (*toolbutton ? gtk_radio_tool_button_new_from_widget (
        GTK_RADIO_TOOL_BUTTON (*toolbutton)) : gtk_radio_tool_button_new (NULL));
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (*toolbutton), label);
    gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (*toolbutton), icon);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (*toolbutton), -1);
    g_signal_connect (*toolbutton, "clicked",
        G_CALLBACK (midori_preferences_toolbutton_clicked_cb), page);
#endif
}

#if 0
static void
midori_preferences_list_dicts_cb (const gchar* lang_tag,
                                  const gchar* provider_name,
                                  const gchar* provider_desc,
                                  const gchar* provider_file,
                                  GList**      dicts)
{
    *dicts = g_list_prepend (*dicts, (gchar*)lang_tag);
}

static GList*
midori_preferences_get_spell_languages (void)
{
    GList* dicts = NULL;
    gpointer broker = enchant_broker_init ();
    enchant_broker_list_dicts (broker, (GCallback)midori_preferences_list_dicts_cb, &dicts);
    enchant_broker_free (broker);
    return dicts;
}
#endif

/**
 * midori_preferences_set_settings:
 * @settings: the settings
 *
 * Assigns a settings instance to a preferences dialog.
 *
 * Note: This must not be called more than once.
 *
 * Since 0.1.2 this is equal to setting #MidoriPreferences:settings:.
 **/
void
midori_preferences_set_settings (MidoriPreferences* preferences,
                                 MidoriWebSettings* settings)
{
    GtkWidget* header;
    GtkWindow* parent;
    const gchar* icon_name;
    KatzePreferences* _preferences;
    GtkWidget* label;
    GtkWidget* button;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_PREFERENCES (preferences));
    g_return_if_fail (MIDORI_IS_WEB_SETTINGS (settings));

    g_return_if_fail (!preferences->settings);

    preferences->settings = settings;

    g_object_get (preferences, "transient-for", &parent, NULL);
    icon_name = parent ? gtk_window_get_icon_name (parent) : NULL;
    if ((header = sokoke_xfce_header_new (icon_name,
        gtk_window_get_title (GTK_WINDOW (preferences)))))
    {
        GtkWidget* vbox = gtk_dialog_get_content_area (GTK_DIALOG (preferences));
        gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
        gtk_widget_show_all (header);
    }
    _preferences = KATZE_PREFERENCES (preferences);

    #define PAGE_NEW(__icon, __label) \
     katze_preferences_add_category (_preferences, __label, __icon)
    #define FRAME_NEW(__label) \
     katze_preferences_add_group (_preferences, __label)
    #define FILLED_ADD(__widget) \
     katze_preferences_add_widget (_preferences, __widget, "filled")
    #define INDENTED_ADD(__widget) \
     katze_preferences_add_widget (_preferences, __widget, "indented")
    #define SPANNED_ADD(__widget) \
     katze_preferences_add_widget (_preferences, __widget, "spanned")
    /* Page "General" */
    if (!midori_paths_is_readonly ())
    {
    PAGE_NEW (GTK_STOCK_HOME, _("Startup"));
    FRAME_NEW (NULL);
    label = katze_property_label (settings, "load-on-startup");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "load-on-startup", NULL);
    SPANNED_ADD (button);
    label = gtk_label_new (_("Homepage:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "homepage", "address");
    SPANNED_ADD (entry);
    if (parent && katze_object_has_property (parent, "uri"))
    {
        #if 0
        button = gtk_button_new_with_mnemonic (_("Use _current page"));
        #else
        label = gtk_label_new (NULL);
        INDENTED_ADD (label);
        button = gtk_button_new_with_label (_("Use current page as homepage"));
        #endif
        g_signal_connect (button, "clicked",
            G_CALLBACK (midori_preferences_homepage_current_clicked_cb), settings);
        SPANNED_ADD (button);
    }
    }

    /* Page "Appearance" */
    PAGE_NEW (GTK_STOCK_SELECT_FONT, _("Fonts"));
    FRAME_NEW (NULL);
    #if !HAVE_HILDON
    label = gtk_label_new (_("Proportional Font Family"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "default-font-family", "font");
    gtk_widget_set_tooltip_text (button, _("The default font family used to display text"));
    SPANNED_ADD (button);
    entry = katze_property_proxy (settings, "default-font-size", NULL);
    gtk_widget_set_tooltip_text (entry, _("The default font size used to display text"));
    SPANNED_ADD (entry);
    label = gtk_label_new (_("Fixed-width Font Family"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "monospace-font-family", "font-monospace");
    gtk_widget_set_tooltip_text (button, _("The font family used to display fixed-width text"));
    SPANNED_ADD (button);
    entry = katze_property_proxy (settings, "default-monospace-font-size", NULL);
    gtk_widget_set_tooltip_text (entry, _("The font size used to display fixed-width text"));
    SPANNED_ADD (entry);
    label = gtk_label_new (_("Minimum Font Size"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "minimum-font-size", NULL);
    gtk_widget_set_tooltip_text (entry, _("The minimum font size used to display text"));
    SPANNED_ADD (entry);
    button = katze_property_proxy (settings, "enforce-font-family", NULL);
    INDENTED_ADD (button);
    #endif
    label = gtk_label_new (_("Preferred Encoding"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "preferred-encoding", "custom-default-encoding");
    SPANNED_ADD (button);

    /* Page "Behavior" */
    PAGE_NEW (GTK_STOCK_SELECT_COLOR, _("Behavior"));
    FRAME_NEW (NULL);
    #if !HAVE_HILDON
    button = katze_property_proxy (settings, "auto-load-images", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Load images automatically"));
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "enable-spell-checking", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable Spell Checking"));
    SPANNED_ADD (button);
    /* Disable spell check option if there are no enchant modules */
    {
        gchar* enchant_path = midori_paths_get_lib_path ("enchant");
        if (enchant_path == NULL)
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
            gtk_widget_set_sensitive (button, FALSE);
        }
        else
            g_free (enchant_path);
    }
    button = katze_property_proxy (settings, "enable-scripts", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable scripts"));
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "enable-plugins", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable Netscape plugins"));
    gtk_widget_set_sensitive (button, midori_web_settings_has_plugin_support ());
    SPANNED_ADD (button);
    #endif
    button = katze_property_proxy (settings, "zoom-text-and-images", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Zoom Text and Images"));
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "javascript-can-open-windows-automatically", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Allow scripts to open popups"));
    gtk_widget_set_tooltip_text (button, _("Whether scripts are allowed to open popup windows automatically"));
    SPANNED_ADD (button);
    if (katze_widget_has_touchscreen_mode (parent ?
        GTK_WIDGET (parent) : GTK_WIDGET (preferences)))
    {
        button = katze_property_proxy (settings, "kinetic-scrolling", NULL);
        gtk_button_set_label (GTK_BUTTON (button), _("Kinetic scrolling"));
        gtk_widget_set_tooltip_text (button, _("Whether scrolling should kinetically move according to speed"));
    }
    else
    {
        button = katze_property_proxy (settings, "middle-click-opens-selection", NULL);
        gtk_button_set_label (GTK_BUTTON (button), _("Middle click opens Selection"));
        gtk_widget_set_tooltip_text (button, _("Load an address from the selection via middle click"));
    }
    INDENTED_ADD (button);
    if (katze_object_has_property (settings, "enable-webgl"))
    {
        button = katze_property_proxy (settings, "enable-webgl", NULL);
        gtk_button_set_label (GTK_BUTTON (button), _("Enable WebGL support"));
        SPANNED_ADD (button);
    }
    #ifndef G_OS_WIN32
    button = katze_property_proxy (settings, "flash-window-on-new-bg-tabs", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Flash window on background tabs"));
    INDENTED_ADD (button);
    #endif

    FRAME_NEW (NULL);
    button = gtk_label_new (_("Preferred languages"));
    gtk_misc_set_alignment (GTK_MISC (button), 0.0, 0.5);
    INDENTED_ADD (button);
    entry = katze_property_proxy (settings, "preferred-languages", "languages");
    gtk_widget_set_tooltip_text (entry, _("A comma separated list of languages preferred for rendering multilingual webpages, for example \"de\", \"ru,nl\" or \"en-us;q=1.0, fr-fr;q=0.667\""));
    SPANNED_ADD (entry);
    label = gtk_label_new (_("Save downloaded files to:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "download-folder", "folder");
    SPANNED_ADD (button);

    /* Page "Interface" */
    PAGE_NEW (GTK_STOCK_CONVERT, _("Browsing"));
    #if !HAVE_HILDON
    if (!g_getenv ("DESKTOP_SESSION"))
    {
        FRAME_NEW (NULL);
        INDENTED_ADD (katze_property_label (settings, "toolbar-style"));
        button = katze_property_proxy (settings, "toolbar-style", NULL);
        SPANNED_ADD (button);
    }
    #endif
    FRAME_NEW (NULL);
    label = katze_property_label (settings, "open-new-pages-in");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "open-new-pages-in", NULL);
    SPANNED_ADD (button);
    button = katze_property_proxy (settings, "close-buttons-on-tabs", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Close Buttons on Tabs"));
    INDENTED_ADD (button);
    #ifndef HAVE_GRANITE
    button = katze_property_proxy (settings, "always-show-tabbar", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Always Show Tabbar"));
    SPANNED_ADD (button);
    #endif
    button = katze_property_proxy (settings, "open-tabs-next-to-current", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Open Tabs next to Current"));
    gtk_widget_set_tooltip_text (button, _("Whether to open new tabs next to the current tab or after the last one"));
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "open-tabs-in-the-background", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Open tabs in the background"));
    SPANNED_ADD (button);

    INDENTED_ADD (gtk_label_new (NULL));
    label = gtk_label_new (_("Text Editor"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "text-editor", "application-text/plain");
    SPANNED_ADD (entry);
    label = gtk_label_new (_("News Aggregator"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "news-aggregator", "application-News");
    SPANNED_ADD (entry);

    /* Page "Network" */
    PAGE_NEW (GTK_STOCK_NETWORK, _("Network"));
    FRAME_NEW (NULL);
    label = gtk_label_new (_("Proxy server"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "proxy-type", NULL);
    SPANNED_ADD (button);
    label = gtk_label_new (_("Hostname"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "http-proxy", "address");
    SPANNED_ADD (entry);
    g_signal_connect (settings, "notify::proxy-type",
        G_CALLBACK (midori_preferences_notify_proxy_type_cb), entry);
    midori_preferences_notify_proxy_type_cb (settings, NULL, entry);
    label = gtk_label_new (_("Port"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "http-proxy-port", NULL);
    SPANNED_ADD (entry);
    g_signal_connect (settings, "notify::proxy-type",
        G_CALLBACK (midori_preferences_notify_proxy_type_cb), entry);
    midori_preferences_notify_proxy_type_cb (settings, NULL, entry);
    #if WEBKIT_CHECK_VERSION (1, 3, 11)
    if (soup_session_get_feature (webkit_get_default_session (), SOUP_TYPE_CACHE))
    {
        label = gtk_label_new (_("Web Cache"));
        gtk_widget_set_tooltip_text (label, _("The maximum size of cached pages on disk"));
        INDENTED_ADD (label);
        button = katze_property_proxy (settings, "maximum-cache-size", NULL);
        gtk_widget_set_tooltip_text (button, _("The maximum size of cached pages on disk"));
        SPANNED_ADD (button);
        label = gtk_label_new (_("MB"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        SPANNED_ADD (label);
    }
    #endif
    label = katze_property_label (settings, "identify-as");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "identify-as", "custom-user-agent");
    SPANNED_ADD (button);
}
