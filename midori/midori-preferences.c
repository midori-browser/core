/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-preferences.h"

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#include "sokoke.h"
#include "compat.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>

struct _MidoriPreferences
{
    KatzePreferences parent_instance;
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
     *
     * Since 0.2.1 the property is only writable.
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "Settings instance to provide properties",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
midori_preferences_init (MidoriPreferences* preferences)
{
    /* Nothing to do */
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
    switch (prop_id)
    {
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
        g_object_set (settings, "homepage", uri, NULL);
        g_free (uri);
    }
}

static void
midori_preferences_notify_preferred_encoding_cb (MidoriWebSettings* settings,
                                                 GParamSpec*        pspec,
                                                 GtkWidget*         entry)
{
    MidoriPreferredEncoding preferred_encoding;

    preferred_encoding = katze_object_get_enum (settings, "preferred-encoding");
    gtk_widget_set_sensitive (entry, preferred_encoding == MIDORI_ENCODING_CUSTOM);
}

#if !HAVE_HILDON
static void
midori_preferences_notify_auto_detect_proxy_cb (MidoriWebSettings* settings,
                                                GParamSpec*        pspec,
                                                GtkWidget*         entry)
{
    MidoriIdentity auto_detect_proxy = katze_object_get_enum (settings,
                                                              "auto-detect-proxy");

    gtk_widget_set_sensitive (entry, !auto_detect_proxy);
}
#endif

static void
midori_preferences_notify_identify_as_cb (MidoriWebSettings* settings,
                                          GParamSpec*        pspec,
                                          GtkWidget*         entry)
{
    MidoriIdentity identify_as = katze_object_get_enum (settings, "identify-as");

    gtk_widget_set_sensitive (entry, identify_as == MIDORI_IDENT_CUSTOM);
}

#if HAVE_OSX
static void
midori_preferences_help_clicked_cb (GtkWidget* button,
                                    GtkDialog* dialog)
{
    gtk_dialog_response (dialog, GTK_RESPONSE_HELP);
}

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

/**
 * midori_preferences_set_settings:
 * @settings: the settings
 *
 * Assigns a settings instance to a preferences dialog.
 *
 * Note: This must not be called more than once.
 *
 * Since 0.1.2 this is equal to setting #MidoriPreferences:settings:.
 *
 * Since 0.2.1 this can be called multiple times.
 **/
void
midori_preferences_set_settings (MidoriPreferences* preferences,
                                 MidoriWebSettings* settings)
{
    GtkWidget* header;
    GtkWindow* parent;
    const gchar* icon_name;
    #if WEBKIT_CHECK_VERSION (1, 1, 15) || HAVE_HILDON
    GtkSettings* gtk_settings;
    #endif
    KatzePreferences* _preferences;
    GtkWidget* label;
    GtkWidget* button;
    GtkWidget* entry;

    g_return_if_fail (MIDORI_IS_PREFERENCES (preferences));
    g_return_if_fail (MIDORI_IS_WEB_SETTINGS (settings));

    gtk_container_foreach (GTK_CONTAINER (GTK_DIALOG (preferences)->vbox), (GtkCallback)gtk_widget_destroy, NULL);

    g_object_get (preferences, "transient-for", &parent, NULL);
    icon_name = parent ? gtk_window_get_icon_name (parent) : NULL;
    if ((header = sokoke_xfce_header_new (icon_name,
        gtk_window_get_title (GTK_WINDOW (preferences)))))
    {
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (preferences)->vbox),
            header, FALSE, FALSE, 0);
        gtk_widget_show_all (header);
    }
    #if WEBKIT_CHECK_VERSION (1, 1, 15) || HAVE_HILDON
    gtk_settings = parent ?
        gtk_widget_get_settings (GTK_WIDGET (parent)) : gtk_settings_get_default ();
    #endif
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
    PAGE_NEW (GTK_STOCK_HOME, _("General"));
    FRAME_NEW (_("Startup"));
    label = katze_property_label (settings, "load-on-startup");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "load-on-startup", NULL);
    SPANNED_ADD (button);
    label = katze_property_label (settings, "homepage");
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "homepage", NULL);
    SPANNED_ADD (entry);
    if (parent && katze_object_has_property (parent, "uri"))
    {
        button = gtk_button_new ();
        label = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (button), label);
        gtk_widget_set_tooltip_text (button, _("Use current page as homepage"));
        g_signal_connect (button, "clicked",
            G_CALLBACK (midori_preferences_homepage_current_clicked_cb), settings);
        SPANNED_ADD (button);
    }
    button = katze_property_proxy (settings, "show-crash-dialog", NULL);
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "speed-dial-in-new-tabs", NULL);
    SPANNED_ADD (button);
    FRAME_NEW (_("Transfers"));
    #if !HAVE_HILDON
    label = katze_property_label (settings, "download-folder");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "download-folder", "folder");
    SPANNED_ADD (button);
    label = katze_property_proxy (settings, "ask-for-destination-folder", NULL);
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "notify-transfer-completed", NULL);
    /* FIXME: Disable the option if notifications presumably cannot be sent
      gtk_widget_set_sensitive (button, FALSE); */
    SPANNED_ADD (button);
    #endif

    /* Page "Appearance" */
    PAGE_NEW (GTK_STOCK_SELECT_FONT, _("Appearance"));
    FRAME_NEW (_("Font settings"));
    #if !HAVE_HILDON
    label = gtk_label_new (_("Default Font Family"));
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "default-font-family", "font");
    gtk_widget_set_tooltip_text (button, _("The default font family used to display text"));
    SPANNED_ADD (button);
    entry = katze_property_proxy (settings, "default-font-size", NULL);
    gtk_widget_set_tooltip_text (entry, _("The default font size used to display text"));
    SPANNED_ADD (entry);
    label = gtk_label_new (_("Fixed-width Font Family"));
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "monospace-font-family", "font-monospace");
    gtk_widget_set_tooltip_text (button, _("The font family used to display fixed-width text"));
    SPANNED_ADD (button);
    entry = katze_property_proxy (settings, "default-monospace-font-size", NULL);
    gtk_widget_set_tooltip_text (entry, _("The font size used to display fixed-width text"));
    SPANNED_ADD (entry);
    label = gtk_label_new (_("Minimum Font Size"));
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "minimum-font-size", NULL);
    gtk_widget_set_tooltip_text (entry, _("The minimum font size used to display text"));
    SPANNED_ADD (entry);
    #endif
    label = katze_property_label (settings, "preferred-encoding");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "preferred-encoding", NULL);
    SPANNED_ADD (button);
    entry = katze_property_proxy (settings, "default-encoding", NULL);
    gtk_widget_set_tooltip_text (entry, _("The character encoding to use by default"));
    g_signal_connect (settings, "notify::preferred-encoding",
        G_CALLBACK (midori_preferences_notify_preferred_encoding_cb), entry);
    midori_preferences_notify_preferred_encoding_cb (settings, NULL, entry);
    SPANNED_ADD (entry);

    /* Page "Behavior" */
    PAGE_NEW (GTK_STOCK_SELECT_COLOR, _("Behavior"));
    FRAME_NEW (_("Features"));
    #if !HAVE_HILDON
    button = katze_property_proxy (settings, "auto-load-images", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Load images automatically"));
    gtk_widget_set_tooltip_text (button, _("Load and display images automatically"));
    INDENTED_ADD (button);
    #endif
    #if WEBKIT_CHECK_VERSION (1, 1, 15) || HAVE_HILDON
    if (katze_object_get_boolean (gtk_settings, "gtk-touchscreen-mode"))
        button = katze_property_proxy (settings, "kinetic-scrolling", NULL);
    else
    {
        button = katze_property_proxy (settings, "auto-shrink-images", NULL);
        gtk_button_set_label (GTK_BUTTON (button), _("Shrink images automatically"));
        gtk_widget_set_tooltip_text (button, _("Automatically shrink standalone images to fit"));
    }
    #else
    button = katze_property_proxy (settings, "middle-click-opens-selection", NULL);
    #endif
    SPANNED_ADD (button);
    #if !HAVE_HILDON
    button = katze_property_proxy (settings, "enable-scripts", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable scripts"));
    gtk_widget_set_tooltip_text (button, _("Enable embedded scripting languages"));
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "enable-plugins", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable Netscape plugins"));
    gtk_widget_set_tooltip_text (button, _("Enable embedded Netscape plugin objects"));
    SPANNED_ADD (button);
    button = katze_property_proxy (settings, "enforce-96-dpi", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enforce 96 dots per inch"));
    gtk_widget_set_tooltip_text (button, _("Enforce a video dot density of 96 DPI"));
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "enable-developer-extras", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable developer tools"));
    gtk_widget_set_tooltip_text (button, _("Enable special extensions for developers"));
    SPANNED_ADD (button);
    #endif
    button = katze_property_proxy (settings, "zoom-text-and-images", NULL);
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "find-while-typing", NULL);
    SPANNED_ADD (button);
    #if WEBKIT_CHECK_VERSION (1, 1, 6)
    FRAME_NEW (_("Spell Checking"));
    button = katze_property_proxy (settings, "enable-spell-checking", NULL);
    gtk_button_set_label (GTK_BUTTON (button), _("Enable Spell Checking"));
    gtk_widget_set_tooltip_text (button, _("Enable spell checking while typing"));
    INDENTED_ADD (button);
    entry = katze_property_proxy (settings, "spell-checking-languages", NULL);
    /* i18n: The example should be adjusted to contain a good local default */
    gtk_widget_set_tooltip_text (entry, _("A comma separated list of languages to be used for spell checking, for example \"en_GB,de_DE\""));
    SPANNED_ADD (entry);
    #endif

    /* Page "Interface" */
    PAGE_NEW (GTK_STOCK_CONVERT, _("Interface"));
    FRAME_NEW (_("Navigationbar"));
    #if !HAVE_HILDON
    INDENTED_ADD (katze_property_label (settings, "toolbar-style"));
    button = katze_property_proxy (settings, "toolbar-style", NULL);
    SPANNED_ADD (button);
    button = katze_property_proxy (settings, "progress-in-location", NULL);
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "search-engines-in-completion", NULL);
    SPANNED_ADD (button);
    #endif
    FRAME_NEW (_("Browsing"));
    label = katze_property_label (settings, "open-new-pages-in");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "open-new-pages-in", NULL);
    SPANNED_ADD (button);
    label = katze_property_label (settings, "open-external-pages-in");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "open-external-pages-in", NULL);
    SPANNED_ADD (button);
    #if !HAVE_HILDON
    button = katze_property_proxy (settings, "always-show-tabbar", NULL);
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "close-buttons-on-tabs", NULL);
    SPANNED_ADD (button);
    #endif
    button = katze_property_proxy (settings, "open-tabs-next-to-current", NULL);
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "open-tabs-in-the-background", NULL);
    SPANNED_ADD (button);

    #if !HAVE_HILDON
    /* Page "Applications" */
    PAGE_NEW (GTK_STOCK_CONVERT, _("Applications"));
    FRAME_NEW (_("External applications"));
    label = katze_property_label (settings, "text-editor");
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "text-editor", "application-text/plain");
    SPANNED_ADD (entry);
    label = katze_property_label (settings, "download-manager");
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "download-manager", "application-Network");
    SPANNED_ADD (entry);
    label = katze_property_label (settings, "news-aggregator");
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "news-aggregator", "application-Network");
    SPANNED_ADD (entry);
    #endif

    /* Page "Network" */
    PAGE_NEW (GTK_STOCK_NETWORK, _("Network"));
    FRAME_NEW (_("Network"));
    #if !HAVE_HILDON
    label = katze_property_label (settings, "http-proxy");
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "http-proxy", NULL);
    SPANNED_ADD (entry);
    button = katze_property_proxy (settings, "auto-detect-proxy", NULL);
    FILLED_ADD (button);
    g_signal_connect (settings, "notify::auto-detect-proxy",
        G_CALLBACK (midori_preferences_notify_auto_detect_proxy_cb), entry);
    midori_preferences_notify_auto_detect_proxy_cb (settings, NULL, entry);
    #endif
    label = katze_property_label (settings, "identify-as");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "identify-as", NULL);
    SPANNED_ADD (button);
    entry = katze_property_proxy (settings, "ident-string", NULL);
    g_signal_connect (settings, "notify::identify-as",
        G_CALLBACK (midori_preferences_notify_identify_as_cb), entry);
    midori_preferences_notify_identify_as_cb (settings, NULL, entry);
    SPANNED_ADD (entry);

    /* Page "Privacy" */
    PAGE_NEW (GTK_STOCK_INDEX, _("Privacy"));
    FRAME_NEW (_("Web Cookies"));
    label = katze_property_label (settings, "accept-cookies");
    INDENTED_ADD (label);
    button = katze_property_proxy (settings, "accept-cookies", NULL);
    SPANNED_ADD (button);
    button = katze_property_proxy (settings, "original-cookies-only", NULL);
    INDENTED_ADD (button);
    label = katze_property_label (settings, "maximum-cookie-age");
    INDENTED_ADD (label);
    entry = katze_property_proxy (settings, "maximum-cookie-age", NULL);
    SPANNED_ADD (entry);
    label = gtk_label_new (_("days"));
    SPANNED_ADD (label);
    FRAME_NEW (_("History"));
    button = katze_property_proxy (settings, "remember-last-visited-pages", NULL);
    INDENTED_ADD (button);
    button = katze_property_proxy (settings, "maximum-history-age", NULL);
    SPANNED_ADD (button);
    label = gtk_label_new (_("days"));
    SPANNED_ADD (label);
    button = katze_property_proxy (settings, "remember-last-downloaded-files", NULL);
    INDENTED_ADD (button);
}
