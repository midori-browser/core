/*
 Copyright (C) 2008-2012 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-extension.h"

#include <katze/katze.h>
#include "midori-platform.h"
#include "midori-core.h"
#include <glib/gi18n.h>

G_DEFINE_TYPE (MidoriExtension, midori_extension, G_TYPE_OBJECT);

struct _MidoriExtensionPrivate
{
    gchar* stock_id;
    gchar* name;
    gchar* description;
    gboolean use_markup;
    gchar* version;
    gchar* authors;
    gchar* website;
    gchar* key;

    MidoriApp* app;
    gint active;
    gchar* config_dir;
    GList* lsettings;
    GHashTable* settings;
    GKeyFile* key_file;
};

typedef struct
{
    gchar* name;
    GType type;
    gboolean default_value;
    gboolean value;
} MESettingBoolean;

typedef struct
{
    gchar* name;
    GType type;
    gint default_value;
    gint value;
} MESettingInteger;

typedef struct
{
    gchar* name;
    GType type;
    gchar* default_value;
    gchar* value;
} MESettingString;

typedef struct
{
    gchar* name;
    GType type;
    gchar** default_value;
    gchar** value;
    gsize default_length;
    gsize length;
} MESettingStringList;

void me_setting_free (gpointer setting)
{
    MESettingString* string_setting = (MESettingString*)setting;
    MESettingStringList* strlist_setting = (MESettingStringList*)setting;

    if (string_setting->type == G_TYPE_STRING)
    {
        g_free (string_setting->name);
        g_free (string_setting->default_value);
        g_free (string_setting->value);
    }
    if (strlist_setting->type == G_TYPE_STRV)
    {
        g_free (strlist_setting->name);
        g_strfreev (strlist_setting->default_value);
        g_strfreev (strlist_setting->value);
    }
}

#define midori_extension_can_install_setting(extension, name) \
    if (extension->priv->active > 0) \
    { \
        g_critical ("%s: Settings have to be installed before " \
                    "the extension is activated.", G_STRFUNC); \
        return; \
    } \
    if (g_hash_table_lookup (extension->priv->settings, name)) \
    { \
        g_critical ("%s: A setting with the name '%s' is already installed.", \
                    G_STRFUNC, name); \
        return; \
    }

#define me_setting_install(stype, _name, gtype, _default_value, _value) \
    setting = g_new (stype, 1); \
    setting->name = _name; \
    setting->type = gtype; \
    setting->default_value = _default_value; \
    setting->value = _value; \
    g_hash_table_insert (extension->priv->settings, setting->name, setting); \
    extension->priv->lsettings = g_list_prepend \
        (extension->priv->lsettings, setting);

#define me_setting_type(setting, gtype, rreturn) \
if (!setting) { \
g_critical ("%s: There is no setting with the name '%s' installed.", G_STRFUNC, name); \
rreturn; } \
if (setting->type != gtype) { \
g_critical ("%s: The setting '%s' is not a string.", G_STRFUNC, name); \
rreturn; }

enum
{
    PROP_0,

    PROP_STOCK_ID,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_USE_MARKUP,
    PROP_VERSION,
    PROP_AUTHORS,
    PROP_WEBSITE,
    PROP_KEY
};

enum {
    ACTIVATE,
    DEACTIVATE,
    OPEN_PREFERENCES,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_extension_finalize (GObject* object);

static void
midori_extension_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec);

static void
midori_extension_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec);

static void
midori_extension_class_init (MidoriExtensionClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

    signals[ACTIVATE] = g_signal_new (
        "activate",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        MIDORI_TYPE_APP);

    signals[DEACTIVATE] = g_signal_new (
        "deactivate",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0,
        G_TYPE_NONE);

    /**
     * MidoriExtension::open-preferences:
     *
     * The preferences of the extension should be opened.
     *
     * Since: 0.4.0
     */
     signals[OPEN_PREFERENCES] = g_signal_new (
        "open-preferences",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0,
        G_TYPE_NONE);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_extension_finalize;
    gobject_class->set_property = midori_extension_set_property;
    gobject_class->get_property = midori_extension_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS;

    g_object_class_install_property (gobject_class,
                                     PROP_STOCK_ID,
                                     g_param_spec_string (
                                     "stock-id",
                                     "Stock ID",
                                     "An optional icon stock ID",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string (
                                     "name",
                                     "Name",
                                     "The name of the extension",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string (
                                     "description",
                                     "Description",
                                     "The description of the extension",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_USE_MARKUP,
                                     g_param_spec_boolean (
                                     "use-markup",
                                     "Use Markup",
                                     "Whether to use Pango markup",
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_VERSION,
                                     g_param_spec_string (
                                     "version",
                                     "Version",
                                     "The version of the extension",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_AUTHORS,
                                     g_param_spec_string (
                                     "authors",
                                     "Authors",
                                     "The authors of the extension",
                                     NULL,
                                     flags));

    /**
     * MidoriExtension:website:
     *
     * The website of the extension.
     *
     * Since: 0.1.8
     */
    g_object_class_install_property (gobject_class,
                                     PROP_WEBSITE,
                                     g_param_spec_string (
                                     "website",
                                     "Website",
                                     "The website of the extension",
                                     NULL,
                                     flags));

    /**
     * MidoriExtension:key:
     *
     * The extension key.
     * Needed if there is more than one extension object in a single module.
     *
     * Since: 0.4.5
     */
    g_object_class_install_property (gobject_class,
                                     PROP_KEY,
                                     g_param_spec_string (
                                     "key",
                                     "Key",
                                     "The extension key",
                                     NULL,
                                     flags));

    g_type_class_add_private (class, sizeof (MidoriExtensionPrivate));
}

static void
midori_extension_activate_cb (MidoriExtension* extension,
                              MidoriApp*       app)
{
    GList* lsettings;

    g_return_if_fail (MIDORI_IS_APP (app));

    lsettings = g_list_first (extension->priv->lsettings);

    /* If a configuration directory was requested before activation we
       assume we should load and save settings. This is a detail that
       extension writers shouldn't worry about. */
    extension->priv->key_file = lsettings && extension->priv->config_dir
        ? g_key_file_new () :  NULL;
    if (extension->priv->key_file)
    {
        gchar* config_file;
        GError* error = NULL;

        config_file = g_build_filename (extension->priv->config_dir, "config", NULL);
        if (!g_key_file_load_from_file (extension->priv->key_file, config_file,
                                        G_KEY_FILE_KEEP_COMMENTS, &error))
        {
            if (error->code == G_FILE_ERROR_NOENT)
            {
                gchar* filename = g_object_get_data (G_OBJECT (extension), "filename");
                katze_assign (config_file,
                    midori_paths_get_extension_preset_filename (filename, "config"));
                g_key_file_load_from_file (extension->priv->key_file, config_file,
                                           G_KEY_FILE_KEEP_COMMENTS, NULL);
            }
            else
                printf (_("The configuration of the extension '%s' couldn't be loaded: %s\n"),
                        extension->priv->name, error->message);
            g_error_free (error);
        }
        g_free (config_file);
    }

    while (lsettings)
    {
        MESettingString* setting = (MESettingString*)lsettings->data;

        if (setting->type == G_TYPE_BOOLEAN)
        {
            MESettingBoolean* setting_ = (MESettingBoolean*)setting;
            if (extension->priv->key_file
             && g_key_file_has_key (extension->priv->key_file, "settings", setting_->name, NULL))
                setting_->value = g_key_file_get_boolean (extension->priv->key_file,
                    "settings", setting->name, NULL);
            else
                setting_->value = setting_->default_value;
        }
        else if (setting->type == G_TYPE_INT)
        {
            MESettingInteger* setting_ = (MESettingInteger*)setting;
            if (extension->priv->key_file
             && g_key_file_has_key (extension->priv->key_file, "settings", setting_->name, NULL))
                setting_->value = g_key_file_get_integer (extension->priv->key_file,
                    "settings", setting_->name, NULL);
            else
                setting_->value = setting_->default_value;
        }
        else if (setting->type == G_TYPE_STRING)
        {
            if (extension->priv->key_file)
            {
                setting->value = g_key_file_get_string (
                    extension->priv->key_file, "settings", setting->name, NULL);
                if (setting->value == NULL)
                    setting->value = setting->default_value;
            }
            else
                setting->value = g_strdup (setting->default_value);
        }
        else if (setting->type == G_TYPE_STRV)
        {
            MESettingStringList* setting_ = (MESettingStringList*)setting;
            if (extension->priv->key_file)
            {
                setting_->value = g_key_file_get_string_list (extension->priv->key_file,
                    "settings", setting->name, &setting_->length, NULL);
                if (setting_->value == NULL)
                {
                    setting_->value = g_strdupv (setting_->default_value);
                    setting_->length = setting_->default_length;
                }
            }
            else
                setting_->value = g_strdupv (setting_->default_value);
        }
        else
            g_assert_not_reached ();

        lsettings = g_list_next (lsettings);
    }

    extension->priv->app = g_object_ref (app);
    extension->priv->active = 1;
    /* FIXME: Disconnect all signal handlers */
}

static void
midori_extension_init (MidoriExtension* extension)
{
    extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension,
        MIDORI_TYPE_EXTENSION, MidoriExtensionPrivate);

    extension->priv->app = NULL;
    extension->priv->active = 0;
    extension->priv->config_dir = NULL;
    extension->priv->lsettings = NULL;
    extension->priv->settings = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, me_setting_free);
    extension->priv->key_file = NULL;

    g_signal_connect (extension, "activate",
        G_CALLBACK (midori_extension_activate_cb), NULL);
}

static void
midori_extension_finalize (GObject* object)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    katze_object_assign (extension->priv->app, NULL);
    katze_assign (extension->priv->stock_id, NULL);
    katze_assign (extension->priv->name, NULL);
    katze_assign (extension->priv->description, NULL);
    katze_assign (extension->priv->version, NULL);
    katze_assign (extension->priv->authors, NULL);
    katze_assign (extension->priv->website, NULL);
    katze_assign (extension->priv->key, NULL);

    katze_assign (extension->priv->config_dir, NULL);
    g_list_free (extension->priv->lsettings);
    g_hash_table_destroy (extension->priv->settings);
    if (extension->priv->key_file)
        g_key_file_free (extension->priv->key_file);
}

static void
midori_extension_set_property (GObject*      object,
                               guint         prop_id,
                               const GValue* value,
                               GParamSpec*   pspec)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    switch (prop_id)
    {
    case PROP_STOCK_ID:
        katze_assign (extension->priv->stock_id, g_value_dup_string (value));
        break;
    case PROP_NAME:
        katze_assign (extension->priv->name, g_value_dup_string (value));
        break;
    case PROP_DESCRIPTION:
        katze_assign (extension->priv->description, g_value_dup_string (value));
        break;
    case PROP_USE_MARKUP:
        extension->priv->use_markup = g_value_get_boolean (value);
        break;
    case PROP_VERSION:
    {
        /* Don't show version suffix if it matches the running Midori */
        const gchar* version = g_value_get_string (value);
        if (version && g_str_has_suffix (version, MIDORI_VERSION_SUFFIX))
            katze_assign (extension->priv->version,
                g_strndup (version,
                           strlen (version) - strlen (MIDORI_VERSION_SUFFIX)));
        /* No version suffix at all, must be 0.4.1 or 0.4.1 git */
        else if (version && !strchr (version, '-') && !strchr (version, '('))
            katze_assign (extension->priv->version,
                g_strconcat (version, " (0.4.1)", NULL));
        else
            katze_assign (extension->priv->version, g_strdup (version));
        break;
    }
    case PROP_AUTHORS:
        katze_assign (extension->priv->authors, g_value_dup_string (value));
        break;
    case PROP_WEBSITE:
        katze_assign (extension->priv->website, g_value_dup_string (value));
        break;
    case PROP_KEY:
        katze_assign (extension->priv->key, g_value_dup_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_extension_get_property (GObject*    object,
                               guint       prop_id,
                               GValue*     value,
                               GParamSpec* pspec)
{
    MidoriExtension* extension = MIDORI_EXTENSION (object);

    switch (prop_id)
    {
    case PROP_STOCK_ID:
        g_value_set_string (value, extension->priv->stock_id);
        break;
    case PROP_NAME:
        g_value_set_string (value, extension->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, extension->priv->description);
        break;
    case PROP_USE_MARKUP:
        g_value_set_boolean (value, extension->priv->use_markup);
        break;
    case PROP_VERSION:
        g_value_set_string (value, extension->priv->version);
        break;
    case PROP_AUTHORS:
        g_value_set_string (value, extension->priv->authors);
        break;
    case PROP_WEBSITE:
        g_value_set_string (value, extension->priv->website);
        break;
    case PROP_KEY:
        g_value_set_string (value, extension->priv->key);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

void
midori_extension_load_from_folder (MidoriApp* app,
                                   gchar**    keys,
                                   gboolean   activate)
{
    if (!g_module_supported ())
        return;

    gchar* extension_path = midori_paths_get_lib_path (PACKAGE_NAME);
    if (!extension_path)
        return;

    if (activate)
    {
        gint i = 0;
        const gchar* filename;
        while (keys && (filename = keys[i++]))
            midori_extension_activate_gracefully (app, extension_path, filename, activate);
        /* FIXME need proper stock extension mechanism */
        GObject* extension = midori_extension_activate_gracefully (app, extension_path, "libtransfers." G_MODULE_SUFFIX, activate);
        g_assert (extension != NULL);
    }
    else
    {
        GDir* extension_dir = g_dir_open (extension_path, 0, NULL);
        g_return_if_fail (extension_dir != NULL);
        const gchar* filename;
        while ((filename = g_dir_read_name (extension_dir)))
            midori_extension_activate_gracefully (app, extension_path, filename, activate);
        g_dir_close (extension_dir);
    }

    g_free (extension_path);
}

GObject*
midori_extension_load_from_file (const gchar* extension_path,
                                 const gchar* filename,
                                 gboolean     activate,
                                 gboolean     test)
{
    gchar* fullname;
    GModule* module;
    typedef GObject* (*extension_init_func)(void);
    extension_init_func extension_init;
    static GHashTable* modules = NULL;
    GObject* extension;

    g_return_val_if_fail (extension_path != NULL, NULL);
    g_return_val_if_fail (filename != NULL, NULL);

    if (strchr (filename, '/'))
    {
        gchar* clean = g_strndup (filename, strchr (filename, '/') - filename);
        fullname = g_build_filename (extension_path, clean, NULL);
        g_free (clean);
    }
    else
        fullname = g_build_filename (extension_path, filename, NULL);

    /* Ignore files which don't have the correct suffix */
    if (!g_str_has_suffix (fullname, G_MODULE_SUFFIX))
    {
        g_free (fullname);
        return NULL;
    }

    module = g_module_open (fullname, G_MODULE_BIND_LOCAL);
    g_free (fullname);

    /* GModule detects repeated loading but exposes no API to check it.
       Skip any modules that were loaded before. */
    if (modules == NULL)
        modules = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
    if ((extension = g_hash_table_lookup (modules, module)))
        return extension;

    if (module && g_module_symbol (module, "extension_init",
                                   (gpointer) &extension_init))
    {
        typedef void (*extension_test_func)(void);
        extension_test_func extension_test;
        if ((extension = extension_init ()))
        {
            if (test && g_module_symbol (module, "extension_test", (gpointer) &extension_test))
                extension_test ();
            g_object_set_data_full (G_OBJECT (extension), "filename", g_strdup (filename), g_free);
            g_hash_table_insert (modules, module, extension);
        }
    }

    return extension;
}

GObject*
midori_extension_activate_gracefully (MidoriApp*   app,
                                      const gchar* extension_path,
                                      const gchar* filename,
                                      gboolean     activate)
{
    GObject* extension = midori_extension_load_from_file (extension_path, filename, activate, FALSE);

    midori_extension_activate (extension, filename, activate, app);
    if (!extension && g_module_error () != NULL)
    {
        KatzeArray* extensions = katze_object_get_object (app, "extensions");
        extension = g_object_new (MIDORI_TYPE_EXTENSION,
                                  "name", filename,
                                  "description", g_module_error (),
                                  NULL);
        g_warning ("%s", g_module_error ());
        katze_array_add_item (extensions, extension);
        g_object_unref (extensions);
        g_object_unref (extension);
        return NULL;
    }
    else
        return extension;
}

static void
midori_extension_add_to_list (MidoriApp*       app,
                              MidoriExtension* extension,
                              const gchar*     filename)
{
    g_return_if_fail (MIDORI_IS_APP (app));
    g_return_if_fail (filename != NULL);
    KatzeArray* extensions = katze_object_get_object (app, "extensions");
    g_return_if_fail (KATZE_IS_ARRAY (extensions));
    if (katze_array_get_item_index (extensions, extension) >= 0)
        return;
    /* FIXME need proper stock extension mechanism */
    if (!strcmp (filename, "libtransfers." G_MODULE_SUFFIX))
        return;

    katze_array_add_item (extensions, extension);
    g_object_unref (extensions);

    if (midori_paths_is_readonly ())
        return;

    /* Signal that we want the extension to load and save */
    if (midori_extension_is_prepared (extension))
    {
        g_warn_if_fail (extension->priv->config_dir == NULL);
        extension->priv->config_dir = midori_paths_get_extension_config_dir (filename);
    }
}

void
midori_extension_activate (GObject*     extension,
                           const gchar* filename,
                           gboolean     activate,
                           MidoriApp*   app)
{
    if (MIDORI_IS_EXTENSION (extension))
    {
        if (filename != NULL)
            midori_extension_add_to_list (app, MIDORI_EXTENSION (extension), filename);
        if (activate && !midori_extension_is_active (MIDORI_EXTENSION (extension)))
            g_signal_emit_by_name (extension, "activate", app);
    }
    else if (KATZE_IS_ARRAY (extension))
    {
        gboolean success = FALSE;
        MidoriExtension* extension_item;
        KATZE_ARRAY_FOREACH_ITEM (extension_item, KATZE_ARRAY (extension))
            if (MIDORI_IS_EXTENSION (extension_item))
            {
                gchar* key = extension_item->priv->key;
                g_return_if_fail (key != NULL);
                if (filename != NULL && strchr (filename, '/'))
                {
                    gchar* clean = g_strndup (filename, strchr (filename, '/') - filename);
                    g_object_set_data_full (G_OBJECT (extension_item), "filename", clean, g_free);
                    midori_extension_add_to_list (app, extension_item, clean);
                }
                else if (filename != NULL)
                {
                    midori_extension_add_to_list (app, extension_item, filename);
                    g_object_set_data_full (G_OBJECT (extension_item), "filename", g_strdup (filename), g_free);
                }
                if (activate && !midori_extension_is_active (MIDORI_EXTENSION (extension_item))
                 && filename && strstr (filename, key))
                {
                    g_signal_emit_by_name (extension_item, "activate", app);
                    success = TRUE;
                }
            }
        /* Passed a multi extension w/o key or non-existing key */
        g_warn_if_fail (!activate || success);
    }
}

/**
 * midori_extension_is_prepared:
 * @extension: a #MidoriExtension
 *
 * Determines if @extension is prepared for use, for instance
 * by ensuring that all required values are set and that it
 * is actually activatable.
 *
 * Return value: %TRUE if @extension is ready for use
 **/
gboolean
midori_extension_is_prepared (MidoriExtension* extension)
{
    g_return_val_if_fail (MIDORI_IS_EXTENSION (extension), FALSE);

    if (extension->priv->name && extension->priv->description
        && extension->priv->version && extension->priv->authors
        && g_signal_has_handler_pending (extension, signals[ACTIVATE], 0, FALSE))
        return TRUE;
    return FALSE;
}

/**
 * midori_extension_has_preferences:
 * @extension: a #MidoriExtension
 *
 * Determines if @extension has preferences.
 *
 * Return value: %TRUE if @extension has preferences
 **/
gboolean
midori_extension_has_preferences (MidoriExtension* extension)
{
    g_return_val_if_fail (MIDORI_IS_EXTENSION (extension), FALSE);

    return g_signal_has_handler_pending (extension, signals[OPEN_PREFERENCES], 0, FALSE);
}

/**
 * midori_extension_is_active:
 * @extension: a #MidoriExtension
 *
 * Determines if @extension is active.
 *
 * Return value: %TRUE if @extension is active
 *
 * Since: 0.1.2
 **/
gboolean
midori_extension_is_active (MidoriExtension* extension)
{
    g_return_val_if_fail (MIDORI_IS_EXTENSION (extension), FALSE);

    return extension->priv->active > 0;
}

/**
 * midori_extension_deactivate:
 * @extension: a #MidoriExtension
 *
 * Attempts to deactivate @extension.
 **/
void
midori_extension_deactivate (MidoriExtension* extension)
{
    g_return_if_fail (midori_extension_is_active (extension));

    g_signal_emit (extension, signals[DEACTIVATE], 0);
    extension->priv->active = 0;
    katze_object_assign (extension->priv->app, NULL);
}

/**
 * midori_extension_get_app:
 * @extension: a #MidoriExtension
 *
 * Retrieves the #MidoriApp the extension belongs to. The
 * extension has to be active.
 *
 * Return value: the #MidoriApp instance
 *
 * Since 0.1.6
 **/
MidoriApp*
midori_extension_get_app (MidoriExtension* extension)
{
    g_return_val_if_fail (midori_extension_is_active (extension), NULL);

    return extension->priv->app;
}

/**
 * midori_extension_get_config_dir:
 * @extension: a #MidoriExtension
 *
 * Retrieves the path to a directory reserved for configuration
 * files specific to the extension.
 *
 * If settings are installed on the extension, they will be
 * loaded from and saved to a file "config" in this path.
 *
 * Return value: a path, such as ~/.config/midori/extensions/name
 **/
const gchar*
midori_extension_get_config_dir (MidoriExtension* extension)
{

    g_return_val_if_fail (midori_extension_is_prepared (extension), NULL);

    return extension->priv->config_dir;
}

/**
 * midori_extension_install_boolean:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @default_value: the default value
 *
 * Installs a boolean that can be used to conveniently
 * store user configuration.
 *
 * Note that all settings have to be installed before
 * the extension is activated.
 *
 * Since: 0.1.3
 **/
void
midori_extension_install_boolean (MidoriExtension* extension,
                                  const gchar*     name,
                                  gboolean         default_value)
{
    MESettingBoolean* setting;

    g_return_if_fail (midori_extension_is_prepared (extension));
    midori_extension_can_install_setting (extension, name);

    me_setting_install (MESettingBoolean, g_strdup (name), G_TYPE_BOOLEAN,
                        default_value, FALSE);
}

static void
midori_extension_save_settings (MidoriExtension *extension)
{
    GError* error = NULL;
    gchar* config_file = g_build_filename (extension->priv->config_dir, "config", NULL);
    katze_mkdir_with_parents (extension->priv->config_dir, 0700);
    sokoke_key_file_save_to_file (extension->priv->key_file, config_file, &error);
    if (error)
    {
        printf (_("The configuration of the extension '%s' couldn't be saved: %s\n"),
                extension->priv->name, error->message);
        g_error_free (error);
    }
    g_free (config_file);
}

/**
 * midori_extension_get_boolean:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 *
 * Retrieves the value of the specified setting.
 *
 * Since: 0.1.3
 **/
gboolean
midori_extension_get_boolean (MidoriExtension* extension,
                              const gchar*     name)
{
    MESettingBoolean* setting;

    g_return_val_if_fail (midori_extension_is_prepared (extension), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_BOOLEAN, return FALSE);

    return setting->value;
}

/**
 * midori_extension_set_boolean:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @value: the new value
 *
 * Assigns a new value to the specified setting.
 *
 * Since: 0.1.3
 **/
void
midori_extension_set_boolean (MidoriExtension* extension,
                              const gchar*     name,
                              gboolean         value)
{
    MESettingBoolean* setting;

    g_return_if_fail (midori_extension_is_active (extension));
    g_return_if_fail (name != NULL);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_BOOLEAN, return);

    setting->value = value;
    if (extension->priv->key_file)
    {
        g_key_file_set_boolean (extension->priv->key_file,
                                "settings", name, value);
        midori_extension_save_settings (extension);
    }
}



/**
 * midori_extension_install_integer:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @default_value: the default value
 *
 * Installs an integer that can be used to conveniently
 * store user configuration.
 *
 * Note that all settings have to be installed before
 * the extension is activated.
 *
 * Since: 0.1.3
 **/
void
midori_extension_install_integer (MidoriExtension* extension,
                                  const gchar*     name,
                                  gint             default_value)
{
    MESettingInteger* setting;

    g_return_if_fail (midori_extension_is_prepared (extension));
    midori_extension_can_install_setting (extension, name);

    me_setting_install (MESettingInteger, g_strdup (name), G_TYPE_INT,
                        default_value, 0);
}

/**
 * midori_extension_get_integer:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 *
 * Retrieves the value of the specified setting.
 *
 * Since: 0.1.3
 **/
gint
midori_extension_get_integer (MidoriExtension* extension,
                              const gchar*     name)
{
    MESettingInteger* setting;

    g_return_val_if_fail (midori_extension_is_prepared (extension), 0);
    g_return_val_if_fail (name != NULL, 0);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_INT, return 0);

    return setting->value;
}

/**
 * midori_extension_set_integer:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @value: the new value
 *
 * Assigns a new value to the specified setting.
 *
 * Since: 0.1.3
 **/
void
midori_extension_set_integer (MidoriExtension* extension,
                              const gchar*     name,
                              gint             value)
{
    MESettingInteger* setting;

    g_return_if_fail (midori_extension_is_active (extension));
    g_return_if_fail (name != NULL);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_INT, return);

    setting->value = value;
    if (extension->priv->key_file)
    {
        g_key_file_set_integer (extension->priv->key_file,
                                "settings", name, value);
        midori_extension_save_settings (extension);
    }
}

/**
 * midori_extension_install_string:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @default_value: the default value
 *
 * Installs a string that can be used to conveniently
 * store user configuration.
 *
 * Note that all settings have to be installed before
 * the extension is activated.
 *
 * Since: 0.1.3
 **/
void
midori_extension_install_string (MidoriExtension* extension,
                                 const gchar*     name,
                                 const gchar*     default_value)
{
    MESettingString* setting;

    g_return_if_fail (midori_extension_is_prepared (extension));
    midori_extension_can_install_setting (extension, name);

    me_setting_install (MESettingString, g_strdup (name), G_TYPE_STRING,
                        g_strdup (default_value), NULL);
}

/**
 * midori_extension_get_string:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 *
 * Retrieves the value of the specified setting.
 *
 * Since: 0.1.3
 **/
const gchar*
midori_extension_get_string (MidoriExtension* extension,
                             const gchar*     name)
{
    MESettingString* setting;

    g_return_val_if_fail (midori_extension_is_prepared (extension), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_STRING, return NULL);

    return setting->value;
}

/**
 * midori_extension_set_string:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @value: the new value
 *
 * Assigns a new value to the specified setting.
 *
 * Since: 0.1.3
 **/
void
midori_extension_set_string (MidoriExtension* extension,
                             const gchar*     name,
                             const gchar*     value)
{
    MESettingString* setting;

    g_return_if_fail (midori_extension_is_active (extension));
    g_return_if_fail (name != NULL);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_STRING, return);

    katze_assign (setting->value, g_strdup (value));
    if (extension->priv->key_file)
    {
        g_key_file_set_string (extension->priv->key_file,
                                "settings", name, value);
        midori_extension_save_settings (extension);
    }
}

/**
 * midori_extension_install_string_list:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @default_value: the default value
 *
 * Installs a string list that can be used to conveniently
 * store user configuration.
 *
 * Note that all settings have to be installed before
 * the extension is activated.
 *
 * Since: 0.1.7
 **/
void
midori_extension_install_string_list (MidoriExtension* extension,
                                      const gchar*     name,
                                      gchar**          default_value,
                                      gsize            default_length)
{
    MESettingStringList* setting;

    g_return_if_fail (midori_extension_is_prepared (extension));
    midori_extension_can_install_setting (extension, name);

    me_setting_install (MESettingStringList, g_strdup (name), G_TYPE_STRV,
                        g_strdupv (default_value), NULL);

    setting->default_length = default_length;
}

/**
 * midori_extension_get_string_list:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @length: return location to store number of strings, or %NULL
 *
 * Retrieves the value of the specified setting.
 *
 * Return value: a newly allocated NULL-terminated list of strings,
 *     should be freed with g_strfreev()
 *
 * Since: 0.1.7
 **/
gchar**
midori_extension_get_string_list (MidoriExtension* extension,
                                  const gchar*     name,
                                  gsize*           length)
{
    MESettingStringList* setting;

    g_return_val_if_fail (midori_extension_is_prepared (extension), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_STRV, return NULL);

    if (length)
        *length = setting->length;

    return g_strdupv (setting->value);
}

/**
 * midori_extension_set_string_list:
 * @extension: a #MidoriExtension
 * @name: the name of the setting
 * @value: the new value
 * @length: number of strings in @value, or G_MAXSIZE
 *
 * Assigns a new value to the specified setting.
 *
 * Since: 0.1.7
 **/
void
midori_extension_set_string_list (MidoriExtension* extension,
                                  const gchar*     name,
                                  gchar**          value,
                                  gsize            length)
{
    MESettingStringList* setting;

    g_return_if_fail (midori_extension_is_active (extension));
    g_return_if_fail (name != NULL);

    setting = g_hash_table_lookup (extension->priv->settings, name);

    me_setting_type (setting, G_TYPE_STRV, return);

    katze_strv_assign (setting->value, g_strdupv (value));
    setting->length = length;

    if (extension->priv->key_file)
    {
        g_key_file_set_string_list (extension->priv->key_file,
                                    "settings", name, (const gchar**)value, length);
        midori_extension_save_settings (extension);
    }
}
