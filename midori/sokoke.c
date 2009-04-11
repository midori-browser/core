/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "sokoke.h"
#include "midori-stock.h"

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#if HAVE_LIBIDN
    #include <stringprep.h>
    #include <punycode.h>
    #include <idna.h>
#endif

static gchar*
sokoke_js_string_utf8 (JSStringRef js_string)
{
    size_t size_utf8;
    gchar* string_utf8;

    g_return_val_if_fail (js_string, NULL);

    size_utf8 = JSStringGetMaximumUTF8CStringSize (js_string);
    string_utf8 = g_new (gchar, size_utf8);
    JSStringGetUTF8CString (js_string, string_utf8, size_utf8);
    return string_utf8;
}

JSValueRef
sokoke_js_script_eval (JSContextRef js_context,
                       const gchar* script,
                       gchar**      exception)
{
    g_return_val_if_fail (js_context, FALSE);
    g_return_val_if_fail (script, FALSE);

    JSStringRef js_script = JSStringCreateWithUTF8CString (script);
    JSValueRef js_exception = NULL;
    JSValueRef js_value = JSEvaluateScript (js_context, js_script,
        JSContextGetGlobalObject (js_context), NULL, 0, &js_exception);
    if (!js_value && exception)
    {
        JSStringRef js_message = JSValueToStringCopy (js_context,
                                                      js_exception, NULL);
        *exception = sokoke_js_string_utf8 (js_message);
        JSStringRelease (js_message);
        js_value = JSValueMakeNull (js_context);
    }
    JSStringRelease (js_script);
    return js_value;
}

static void
error_dialog (const gchar* short_message,
              const gchar* detailed_message)
{
    GtkWidget* dialog = gtk_message_dialog_new (
        NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", short_message);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", detailed_message);
    gtk_widget_show (dialog);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy), dialog);


}

gboolean
sokoke_spawn_program (const gchar* command,
                      const gchar* argument)
{
    gchar* argument_escaped;
    gchar* command_ready;
    gchar** argv;
    GError* error;

    g_return_val_if_fail (command != NULL, FALSE);
    g_return_val_if_fail (argument != NULL, FALSE);

    argument_escaped = g_shell_quote (argument);
    if (strstr (command, "%s"))
        command_ready = g_strdup_printf (command, argument_escaped);
    else
        command_ready = g_strconcat (command, " ", argument_escaped, NULL);

    error = NULL;
    if (!g_shell_parse_argv (command_ready, NULL, &argv, &error))
    {
        error_dialog (_("Could not run external program."), error->message);
        g_error_free (error);
        g_free (command_ready);
        g_free (argument_escaped);
        return FALSE;
    }

    error = NULL;
    if (!g_spawn_async (NULL, argv, NULL,
        (GSpawnFlags)G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
        NULL, NULL, NULL, &error))
    {
        error_dialog (_("Could not run external program."), error->message);
        g_error_free (error);
    }

    g_strfreev (argv);
    g_free (command_ready);
    g_free (argument_escaped);
    return TRUE;
}

static gchar*
sokoke_idn_to_punycode (gchar* uri)
{
    #if HAVE_LIBIDN
    gchar* proto;
    gchar* hostname;
    gchar* path;
    char *s;
    uint32_t *q;
    int rc;
    gchar *result;

    if ((proto = g_utf8_strchr (uri, -1, ':')))
    {
        gulong offset;
        gchar* buffer;

        /* 'file' URIs don't have a hostname */
        if (!strcmp (proto, "file"))
            return uri;

        offset = g_utf8_pointer_to_offset (uri, proto);
        buffer = g_malloc0 (offset + 1);
        g_utf8_strncpy (buffer, uri, offset);
        proto = buffer;
    }

    path = NULL;
    if ((hostname = g_utf8_strchr (uri, -1, '/')))
    {
        if (hostname[1] == '/')
            hostname += 2;
        if ((path = g_utf8_strchr (hostname, -1, '/')))
        {
            gulong offset = g_utf8_pointer_to_offset (hostname, path);
            gchar* buffer = g_malloc0 (offset + 1);
            g_utf8_strncpy (buffer, hostname, offset);
            hostname = buffer;
        }
    }
    else
        hostname = g_strdup (uri);

    if (!(q = stringprep_utf8_to_ucs4 (hostname, -1, NULL)))
    {
        g_free (proto);
        g_free (hostname);
        return uri;
    }

    rc = idna_to_ascii_4z (q, &s, IDNA_ALLOW_UNASSIGNED);
    free (q);
    if (rc != IDNA_SUCCESS)
    {
        g_free (proto);
        g_free (hostname);
        return uri;
    }

    if (proto)
    {
        result = g_strconcat (proto, "://", s, path ? path : "", NULL);
        g_free (proto);
        if (path)
            g_free (hostname);
    }
    else
        result = g_strdup (s);
    g_free (uri);
    free (s);

    return result;
    #else
    return uri;
    #endif
}

gchar*
sokoke_magic_uri (const gchar* uri,
                  KatzeArray*  search_engines)
{
    gchar* current_dir;
    gchar* result;
    gchar** parts;
    gchar* search;
    const gchar* search_uri;
    KatzeItem* item;

    g_return_val_if_fail (uri, NULL);
    g_return_val_if_fail (!search_engines ||
        katze_array_is_a (search_engines, KATZE_TYPE_ITEM), NULL);

    /* Just return if it's a javascript: or mailto: uri */
    if (g_str_has_prefix (uri, "javascript:")
     || g_str_has_prefix (uri, "mailto:")
     || g_str_has_prefix (uri, "data:"))
        return g_strdup (uri);
    /* Add file:// if we have a local path */
    if (g_path_is_absolute (uri))
        return g_strconcat ("file://", uri, NULL);
    /* Construct an absolute path if the file is relative */
    if (g_file_test (uri, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    {
        current_dir = g_get_current_dir ();
        result = g_strconcat ("file://", current_dir,
                              G_DIR_SEPARATOR_S, uri, NULL);
        g_free (current_dir);
        return result;
    }
    /* Do we have a protocol? */
    if (g_strstr_len (uri, 8, "://"))
        return sokoke_idn_to_punycode (g_strdup (uri));

    /* Do we have a domain, ip address or localhost? */
    if ((search = strchr (uri, ':')) && search[0] &&
        !g_ascii_isalpha (search[1]) && search[1] != ' ')
        if (!strchr (search, '.'))
            return sokoke_idn_to_punycode (g_strconcat ("http://", uri, NULL));
    if (!strcmp (uri, "localhost") || g_str_has_prefix (uri, "localhost/"))
        return g_strconcat ("http://", uri, NULL);
    parts = g_strsplit (uri, ".", 0);
    if (!search && parts[0] && parts[1])
    {
        if (!(parts[1][1] == '\0' && !g_ascii_isalpha (parts[1][0])))
            if (!strchr (parts[0], ' ') && !strchr (parts[1], ' '))
                if ((search = g_strconcat ("http://", uri, NULL)))
                {
                    g_strfreev (parts);
                    return sokoke_idn_to_punycode (search);
                }
    }
    g_strfreev (parts);
    /* We don't want to search? So return early. */
    if (!search_engines)
        return g_strdup (uri);
    search = NULL;
    search_uri = NULL;
    /* Do we have a keyword and a string? */
    parts = g_strsplit (uri, " ", 2);
    if (parts[0] && parts[1])
        if ((item = katze_array_find_token (search_engines, parts[0])))
        {
            search_uri = katze_item_get_uri (item);
            if (strstr (search_uri, "%s"))
                search = g_strdup_printf (search_uri, parts[1]);
            else
                search = g_strdup_printf ("%s%s", search_uri, parts[1]);
        }
    g_strfreev (parts);
    return search;
}

void
sokoke_combo_box_add_strings (GtkComboBox* combobox,
                              const gchar* label_first, ...)
{
    const gchar* label;

    /* Add a number of strings to a combobox, terminated with NULL
       This works only for text comboboxes */
    va_list args;
    va_start (args, label_first);

    for (label = label_first; label; label = va_arg (args, const gchar*))
        gtk_combo_box_append_text (combobox, label);

    va_end (args);
}

void sokoke_widget_set_visible (GtkWidget* widget, gboolean visible)
{
    /* Show or hide the widget */
    if (visible)
        gtk_widget_show (widget);
    else
        gtk_widget_hide (widget);
}

void
sokoke_container_show_children (GtkContainer* container)
{
    /* Show every child but not the container itself */
    gtk_container_foreach (container, (GtkCallback)(gtk_widget_show_all), NULL);
}

void
sokoke_widget_popup (GtkWidget*      widget,
                     GtkMenu*        menu,
                     GdkEventButton* event,
                     SokokeMenuPos   pos)
{
    katze_widget_popup (widget, menu, event, (KatzeMenuPos)pos);
}

typedef enum
{
    SOKOKE_DESKTOP_UNTESTED,
    SOKOKE_DESKTOP_XFCE,
    SOKOKE_DESKTOP_OSX,
    SOKOKE_DESKTOP_UNKNOWN
} SokokeDesktop;

static SokokeDesktop
sokoke_get_desktop (void)
{
    #if HAVE_OSX
    return SOKOKE_DESKTOP_OSX;
    #else
    static SokokeDesktop desktop = SOKOKE_DESKTOP_UNTESTED;
    if (G_UNLIKELY (desktop == SOKOKE_DESKTOP_UNTESTED))
    {
        /* Are we running in Xfce? */
        gint result;
        gchar *out = NULL;
        gboolean success = g_spawn_command_line_sync ("xprop -root _DT_SAVE_MODE",
            &out, NULL, &result, NULL);
        if (success && ! result && out != NULL && strstr (out, "xfce4") != NULL)
            desktop = SOKOKE_DESKTOP_XFCE;
        else
            desktop = SOKOKE_DESKTOP_UNKNOWN;
        g_free (out);
    }

    return desktop;
    #endif
}

/**
 * sokoke_xfce_header_new:
 * @icon: an icon name
 * @title: the title of the header
 *
 * Creates an Xfce style header *if* Xfce is running.
 *
 * Return value: A #GtkWidget or %NULL
 *
 * Since 0.1.2 @icon may be NULL, and a default is used.
 **/
GtkWidget*
sokoke_xfce_header_new (const gchar* icon,
                        const gchar* title)
{
    g_return_val_if_fail (title, NULL);

    /* Create an xfce header with icon and title
       This returns NULL if the desktop is not Xfce */
    if (sokoke_get_desktop () == SOKOKE_DESKTOP_XFCE)
    {
        GtkWidget* entry;
        gchar* markup;
        GtkWidget* xfce_heading;
        GtkWidget* hbox;
        GtkWidget* image;
        GtkWidget* label;
        GtkWidget* vbox;
        GtkWidget* separator;

        xfce_heading = gtk_event_box_new ();
        entry = gtk_entry_new ();
        gtk_widget_modify_bg (xfce_heading, GTK_STATE_NORMAL,
            &entry->style->base[GTK_STATE_NORMAL]);
        hbox = gtk_hbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
        if (icon)
            image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_DIALOG);
        else
            image = gtk_image_new_from_stock (GTK_STOCK_PREFERENCES,
                GTK_ICON_SIZE_DIALOG);
        gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
        label = gtk_label_new (NULL);
        gtk_widget_modify_fg (label, GTK_STATE_NORMAL
         , &entry->style->text[GTK_STATE_NORMAL]);
        markup = g_strdup_printf ("<span size='large' weight='bold'>%s</span>",
                                  title);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (xfce_heading), hbox);
        g_free (markup);
        gtk_widget_destroy (entry);

        vbox = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), xfce_heading, FALSE, FALSE, 0);

        separator = gtk_hseparator_new ();
        gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

        return vbox;
    }
    return NULL;
}

GtkWidget*
sokoke_superuser_warning_new (void)
{
    /* Create a horizontal bar with a security warning
       This returns NULL if the user is no superuser */
    #if HAVE_UNISTD_H
    if (G_UNLIKELY (!geteuid ())) /* effective superuser? */
    {
        GtkWidget* hbox;
        GtkWidget* label;

        hbox = gtk_event_box_new ();
        gtk_widget_modify_bg (hbox, GTK_STATE_NORMAL,
                              &hbox->style->bg[GTK_STATE_SELECTED]);
        /* i18n: A superuser, or system administrator, may not be 'root' */
        label = gtk_label_new (_("Warning: You are using a superuser account!"));
        gtk_misc_set_padding (GTK_MISC (label), 0, 2);
        gtk_widget_modify_fg (GTK_WIDGET (label), GTK_STATE_NORMAL,
            &GTK_WIDGET (label)->style->fg[GTK_STATE_SELECTED]);
        gtk_widget_show (label);
        gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (label));
        gtk_widget_show (hbox);
        return hbox;
    }
    #endif
    return NULL;
}

GtkWidget*
sokoke_hig_frame_new (const gchar* title)
{
    /* Create a frame with no actual frame but a bold label and indentation */
    GtkWidget* frame = gtk_frame_new (NULL);
    gchar* title_bold = g_strdup_printf ("<b>%s</b>", title);
    GtkWidget* label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), title_bold);
    g_free (title_bold);
    gtk_frame_set_label_widget (GTK_FRAME (frame), label);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    return frame;
}

void
sokoke_widget_set_pango_font_style (GtkWidget* widget,
                                    PangoStyle style)
{
    /* Conveniently change the pango font style
       For some reason we need to reset if we actually want the normal style */
    if (style == PANGO_STYLE_NORMAL)
        gtk_widget_modify_font (widget, NULL);
    else
    {
        PangoFontDescription* font_description = pango_font_description_new ();
        pango_font_description_set_style (font_description, PANGO_STYLE_ITALIC);
        gtk_widget_modify_font (widget, font_description);
        pango_font_description_free (font_description);
    }
}

static gboolean
sokoke_on_entry_focus_in_event (GtkEntry*      entry,
                                GdkEventFocus* event,
                                gpointer       userdata)
{
    gint has_default = GPOINTER_TO_INT (
        g_object_get_data (G_OBJECT (entry), "sokoke_has_default"));
    if (has_default)
    {
        gtk_entry_set_text (entry, "");
        g_object_set_data (G_OBJECT (entry), "sokoke_has_default",
                           GINT_TO_POINTER (0));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_NORMAL);
    }
    return FALSE;
}

static gboolean
sokoke_on_entry_focus_out_event (GtkEntry*      entry,
                                 GdkEventFocus* event,
                                 gpointer       userdata)
{
    const gchar* text = gtk_entry_get_text (entry);
    if (text && !*text)
    {
        const gchar* default_text = (const gchar*)g_object_get_data (
            G_OBJECT (entry), "sokoke_default_text");
        gtk_entry_set_text (entry, default_text);
        g_object_set_data (G_OBJECT (entry),
                           "sokoke_has_default", GINT_TO_POINTER (1));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_ITALIC);
    }
    return FALSE;
}

void
sokoke_entry_set_default_text (GtkEntry*    entry,
                               const gchar* default_text)
{
    /* Note: The default text initially overwrites any previous text */
    gchar* old_value = g_object_get_data (G_OBJECT (entry),
                                          "sokoke_default_text");
    if (!old_value)
    {
        g_object_set_data (G_OBJECT (entry), "sokoke_has_default",
                           GINT_TO_POINTER (1));
        sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                            PANGO_STYLE_ITALIC);
        gtk_entry_set_text (entry, default_text);
    }
    else if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (entry)))
    {
        gint has_default = GPOINTER_TO_INT (
            g_object_get_data (G_OBJECT (entry), "sokoke_has_default"));
        if (has_default)
        {
            gtk_entry_set_text (entry, default_text);
            sokoke_widget_set_pango_font_style (GTK_WIDGET (entry),
                                                PANGO_STYLE_ITALIC);
        }
    }
    g_object_set_data (G_OBJECT (entry), "sokoke_default_text",
                       (gpointer)default_text);
    g_signal_connect (entry, "focus-in-event",
        G_CALLBACK (sokoke_on_entry_focus_in_event), NULL);
    g_signal_connect (entry, "focus-out-event",
        G_CALLBACK (sokoke_on_entry_focus_out_event), NULL);
}

gchar*
sokoke_key_file_get_string_default (GKeyFile*    key_file,
                                    const gchar* group,
                                    const gchar* key,
                                    const gchar* default_value,
                                    GError**     error)
{
    gchar* value = g_key_file_get_string (key_file, group, key, error);
    return value == NULL ? g_strdup (default_value) : value;
}

gint
sokoke_key_file_get_integer_default (GKeyFile*    key_file,
                                     const gchar* group,
                                     const gchar* key,
                                     const gint   default_value,
                                     GError**     error)
{
    if (!g_key_file_has_key (key_file, group, key, NULL))
        return default_value;
    return g_key_file_get_integer (key_file, group, key, error);
}

gdouble
sokoke_key_file_get_double_default (GKeyFile*     key_file,
                                    const gchar*  group,
                                    const gchar*  key,
                                    const gdouble default_value,
                                    GError**      error)
{
    if (!g_key_file_has_key (key_file, group, key, NULL))
        return default_value;
    return g_key_file_get_double (key_file, group, key, error);
}

gboolean
sokoke_key_file_get_boolean_default (GKeyFile*      key_file,
                                     const gchar*   group,
                                     const gchar*   key,
                                     const gboolean default_value,
                                     GError**       error)
{
    if (!g_key_file_has_key (key_file, group, key, NULL))
        return default_value;
    return g_key_file_get_boolean (key_file, group, key, error);
}

gboolean
sokoke_key_file_save_to_file (GKeyFile*    key_file,
                              const gchar* filename,
                              GError**     error)
{
    gchar* data;
    FILE* fp;

    data = g_key_file_to_data (key_file, NULL, error);
    if (!data)
        return FALSE;

    if (!(fp = fopen (filename, "w")))
    {
        *error = g_error_new (G_FILE_ERROR, G_FILE_ERROR_ACCES,
                              _("Writing failed."));
        return FALSE;
    }
    fputs (data, fp);
    fclose (fp);
    g_free (data);
    return TRUE;
}

void
sokoke_widget_get_text_size (GtkWidget*   widget,
                             const gchar* text,
                             gint*        width,
                             gint*        height)
{
    PangoLayout* layout = gtk_widget_create_pango_layout (widget, text);
    pango_layout_get_pixel_size (layout, width, height);
    g_object_unref (layout);
}

/**
 * sokoke_action_create_popup_menu_item:
 * @action: a #GtkAction
 *
 * Creates a menu item from an action, just like
 * gtk_action_create_menu_item(), but it won't
 * display an accelerator.
 *
 * Note: This menu item is not a proxy and will
 *       not reflect any changes to the action.
 *
 * Return value: a new #GtkMenuItem
 **/
GtkWidget*
sokoke_action_create_popup_menu_item (GtkAction* action)
{
    GtkWidget* menuitem;
    GtkWidget* icon;
    gchar* label;
    gchar* stock_id;
    gchar* icon_name;
    gboolean sensitive;
    gboolean visible;

    g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

    g_object_get (action,
                  "label", &label,
                  "stock-id", &stock_id,
                  "icon-name", &icon_name,
                  "sensitive", &sensitive,
                  "visible", &visible,
                  NULL);
    if (GTK_IS_TOGGLE_ACTION (action))
    {
        menuitem = gtk_check_menu_item_new_with_mnemonic (label);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
            gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
    }
    else if (stock_id)
    {
        if (label)
        {
            menuitem = gtk_image_menu_item_new_with_mnemonic (label);
            icon = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
        else
            menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
    }
    else
    {
        menuitem = gtk_image_menu_item_new_with_mnemonic (label);
        if (icon_name)
        {
            icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), icon);
        }
    }
    gtk_widget_set_sensitive (menuitem, sensitive);
    sokoke_widget_set_visible (menuitem, visible);
    g_signal_connect_swapped (menuitem, "activate",
                              G_CALLBACK (gtk_action_activate), action);

    return menuitem;
}

/**
 * sokoke_image_menu_item_new_ellipsized:
 * @label: the text of the menu item
 *
 * Creates a new #GtkImageMenuItem containing an ellipsized label.
 *
 * Return value: a new #GtkImageMenuItem
 **/
GtkWidget*
sokoke_image_menu_item_new_ellipsized (const gchar* label)
{
    return katze_image_menu_item_new_ellipsized (label);
}

/**
 * sokoke_time_t_to_julian:
 * @timestamp: a time_t timestamp value
 *
 * Calculates a unix timestamp to a julian day value.
 *
 * Return value: an integer.
 **/
gint64
sokoke_time_t_to_julian (const time_t* timestamp)
{
    GDate* date;
    gint64 julian;

    date = g_date_new ();

    g_date_set_time_t (date, *timestamp);
    julian = (gint64)g_date_get_julian (date);

    g_date_free (date);

    return julian;
}

/**
 * sokoke_register_stock_items:
 *
 * Registers several custom stock items used throughout Midori.
 **/
void
sokoke_register_stock_items (void)
{
    typedef struct
    {
        const gchar* stock_id;
        const gchar* label;
        GdkModifierType modifier;
        guint keyval;
        const gchar* fallback;
    } FatStockItem;
    GtkIconSource* icon_source;
    GtkIconSet* icon_set;
    GtkIconFactory* factory = gtk_icon_factory_new ();
    gsize i;

    static FatStockItem items[] =
    {
        { STOCK_EXTENSION, NULL, 0, 0, GTK_STOCK_CONVERT },
        { STOCK_NEWS_FEED, NULL, 0, 0, GTK_STOCK_INDEX },
        { STOCK_SCRIPT, NULL, 0, 0, GTK_STOCK_EXECUTE },
        { STOCK_STYLE, NULL, 0, 0, GTK_STOCK_SELECT_COLOR },
        { STOCK_TRANSFER, NULL, 0, 0, GTK_STOCK_SAVE },

        { STOCK_BOOKMARK,       N_("_Bookmark"), 0, 0, GTK_STOCK_FILE },
        { STOCK_BOOKMARKS,      N_("_Bookmarks"), 0, 0, GTK_STOCK_DIRECTORY },
        { STOCK_BOOKMARK_ADD,   N_("_Add Bookmark"), 0, 0, GTK_STOCK_ADD },
        { STOCK_CONSOLE,        N_("_Console"), 0, 0, GTK_STOCK_DIALOG_WARNING },
        { STOCK_EXTENSIONS,     N_("_Extensions"), 0, 0, GTK_STOCK_CONVERT },
        { STOCK_HISTORY,        N_("_History"), 0, 0, GTK_STOCK_SORT_ASCENDING },
        { STOCK_HOMEPAGE,       N_("_Homepage"), 0, 0, GTK_STOCK_HOME },
        { STOCK_SCRIPTS,        N_("_Userscripts"), 0, 0, GTK_STOCK_EXECUTE },
        { STOCK_STYLES,         N_("User_styles"), 0, 0, GTK_STOCK_SELECT_COLOR },
        { STOCK_TAB_NEW,        N_("New _Tab"), 0, 0, GTK_STOCK_ADD },
        { STOCK_TRANSFERS,      N_("_Transfers"), 0, 0, GTK_STOCK_SAVE },
        { STOCK_PLUGINS,        N_("P_lugins"), 0, 0, GTK_STOCK_CONVERT },
        { STOCK_USER_TRASH,     N_("_Closed Tabs and Windows"), 0, 0, "gtk-undo-ltr" },
        { STOCK_WINDOW_NEW,     N_("New _Window"), 0, 0, GTK_STOCK_ADD },
    };

    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        icon_set = gtk_icon_set_new ();
        icon_source = gtk_icon_source_new ();
        if (items[i].fallback)
        {
            gtk_icon_source_set_icon_name (icon_source, items[i].fallback);
            items[i].fallback = NULL;
            gtk_icon_set_add_source (icon_set, icon_source);
        }
        gtk_icon_source_set_icon_name (icon_source, items[i].stock_id);
        gtk_icon_set_add_source (icon_set, icon_source);
        gtk_icon_source_free (icon_source);
        gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
        gtk_icon_set_unref (icon_set);
    }
    gtk_stock_add ((GtkStockItem*)items, G_N_ELEMENTS (items));
    gtk_icon_factory_add_default (factory);
    g_object_unref (factory);
}

/**
 * sokoke_set_config_dir:
 * @new_config_dir: an absolute path, or %NULL
 *
 * Retrieves and/ or sets the base configuration folder.
 *
 * Return value: the configuration folder, or %NULL
 **/
const gchar*
sokoke_set_config_dir (const gchar* new_config_dir)
{
    static gchar* config_dir = NULL;

    if (config_dir)
        return config_dir;

    if (!new_config_dir)
        config_dir = g_build_filename (g_get_user_config_dir (),
                                       PACKAGE_NAME, NULL);
    else
    {
        g_return_val_if_fail (g_path_is_absolute (new_config_dir), NULL);
        katze_assign (config_dir, g_strdup (new_config_dir));
    }

    return config_dir;
}

/**
 * sokoke_remove_path:
 * @path: an absolute path
 * @ignore_errors: keep removing even if an error occurred
 *
 * Removes the file at @path or the folder including any
 * child folders and files if @path is a folder.
 *
 * If @ignore_errors is %TRUE and @path is a folder with
 * children, one of which can't be removed, remaining
 * children will be deleted nevertheless
 * If @ignore_errors is %FALSE and @path is a folder, the
 * removal process will cancel immediately.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
sokoke_remove_path (const gchar* path,
                    gboolean     ignore_errors)
{
    GDir* dir = g_dir_open (path, 0, NULL);
    const gchar* name;

    if (!dir)
        return g_remove (path) == 0;

    while ((name = g_dir_read_name (dir)))
    {
        gchar* sub_path = g_build_filename (path, name, NULL);
        if (!sokoke_remove_path (sub_path, ignore_errors) && !ignore_errors)
            return FALSE;
        g_free (sub_path);
    }

    g_dir_close (dir);
    g_rmdir (path);
    return TRUE;
}
