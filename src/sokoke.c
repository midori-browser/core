/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "sokoke.h"

#include "debug.h"

#include <string.h>
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif
#include <gdk/gdkkeysyms.h>

void sokoke_combo_box_add_strings(GtkComboBox* combobox
 , const gchar* sLabelFirst, ...)
{
    // Add a number of strings to a combobox, terminated with NULL
    // This works only for text comboboxes
    va_list args;
    va_start(args, sLabelFirst);

    const gchar* sLabel;
    for(sLabel = sLabelFirst; sLabel; sLabel = va_arg(args, const gchar*))
        gtk_combo_box_append_text(combobox, sLabel);

    va_end(args);
}

void sokoke_radio_action_set_current_value(GtkRadioAction* action
 , gint current_value)
{
    // Activates the group member with the given value
    #if GTK_CHECK_VERSION(2, 10, 0)
    gtk_radio_action_set_current_value(action, current_value);
    #else
    // TODO: Implement this for older gtk
    UNIMPLEMENTED
    #endif
}

void sokoke_widget_set_visible(GtkWidget* widget, gboolean bVisibility)
{
    // Show or hide the widget
    if(bVisibility)
        gtk_widget_show(widget);
    else
        gtk_widget_hide(widget);
}

void sokoke_container_show_children(GtkContainer* container)
{
    // Show every child but not the container itself
    gtk_container_foreach(container, (GtkCallback)(gtk_widget_show_all), NULL);
}

void sokoke_widget_set_tooltip_text(GtkWidget* widget, const gchar* sText)
{
    #if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text(widget, sText);
    #else
    static GtkTooltips* tooltips;
    if(!tooltips)
        tooltips = gtk_tooltips_new();
    gtk_tooltips_set_tip(tooltips, widget, sText, NULL);
    #endif
}

void sokoke_tool_item_set_tooltip_text(GtkToolItem* toolitem, const gchar* sText)
{
    // TODO: Use 2.12 api if available
    GtkTooltips* tooltips = gtk_tooltips_new();
    gtk_tool_item_set_tooltip(toolitem, tooltips, sText, NULL);
}

void sokoke_widget_popup(GtkWidget* widget, GtkMenu* menu
 , GdkEventButton* event)
{
    // TODO: Provide a GtkMenuPositionFunc in case a keyboard invoked this
    int button, event_time;
    if(event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time();
    }

    if(!gtk_menu_get_attach_widget(menu))
        gtk_menu_attach_to_widget(menu, widget, NULL);
    gtk_menu_popup(menu, NULL, NULL, NULL, NULL, button, event_time);
}

enum
{
 SOKOKE_DESKTOP_UNKNOWN,
 SOKOKE_DESKTOP_XFCE
};

static guint sokoke_get_desktop(void)
{
    // Are we running in Xfce?
    gint iResult; gchar* stdout; gchar* stderr;
    gboolean bSuccess = g_spawn_command_line_sync(
     "xprop -root _DT_SAVE_MODE | grep -q xfce4"
     , &stdout, &stderr, &iResult, NULL);
    if(bSuccess && !iResult)
        return SOKOKE_DESKTOP_XFCE;

    return SOKOKE_DESKTOP_UNKNOWN;
}

gpointer sokoke_xfce_header_new(const gchar* sIcon, const gchar* sTitle)
{

    // Create an xfce header with icon and title
    // This returns NULL if the desktop is not xfce
    if(sokoke_get_desktop() == SOKOKE_DESKTOP_XFCE)
    {
        GtkWidget* entry = gtk_entry_new();
        gchar* sMarkup;
        GtkWidget* xfce_heading = gtk_event_box_new();
        gtk_widget_modify_bg(xfce_heading, GTK_STATE_NORMAL
         , &entry->style->base[GTK_STATE_NORMAL]);
        GtkWidget* hbox = gtk_hbox_new(FALSE, 12);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
        GtkWidget* icon = gtk_image_new_from_icon_name(sIcon, GTK_ICON_SIZE_DIALOG);
        gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
        GtkWidget* label = gtk_label_new(NULL);
        gtk_widget_modify_fg(label, GTK_STATE_NORMAL
         , &entry->style->text[GTK_STATE_NORMAL]);
        sMarkup = g_strdup_printf("<span size='large' weight='bold'>%s</span>", sTitle);
        gtk_label_set_markup(GTK_LABEL(label), sMarkup);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(xfce_heading), hbox);
        g_free(sMarkup);
        return xfce_heading;
    }
    return NULL;
}

gpointer sokoke_superuser_warning_new(void)
{
    // Create a horizontal bar with a security warning
    // This returns NULL if the user is no superuser
    #ifdef HAVE_UNISTD_H
    if(G_UNLIKELY(!geteuid())) // effective superuser?
    {
        GtkWidget* hbox = gtk_event_box_new();
        gtk_widget_modify_bg(hbox, GTK_STATE_NORMAL
         , &hbox->style->bg[GTK_STATE_SELECTED]);
        GtkWidget* label = gtk_label_new("Warning: You are using the superuser account!");
        gtk_misc_set_padding(GTK_MISC(label), 0, 2);
        gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL
         , &GTK_WIDGET(label)->style->fg[GTK_STATE_SELECTED]);
        gtk_container_add(GTK_CONTAINER(hbox), GTK_WIDGET(label));
        return hbox;
    }
    #endif
    return NULL;
}

GtkWidget* sokoke_hig_frame_new(const gchar* sLabel)
{
    // Create a frame with no actual frame but a bold label and indentation
    GtkWidget* frame = gtk_frame_new(NULL);
    gchar* sLabelBold = g_strdup_printf("<b>%s</b>", sLabel);
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), sLabelBold);
    g_free(sLabelBold);
    gtk_frame_set_label_widget(GTK_FRAME(frame), label);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    return frame;
}

void sokoke_widget_set_pango_font_style(GtkWidget* widget, PangoStyle style)
{
    // Conveniently change the pango font style
    // For some reason we need to reset if we actually want the normal style
    if(style == PANGO_STYLE_NORMAL)
        gtk_widget_modify_font(widget, NULL);
    else
    {
        PangoFontDescription* pangofontdesc = pango_font_description_new();
        pango_font_description_set_style(pangofontdesc, PANGO_STYLE_ITALIC);
        gtk_widget_modify_font(widget, pangofontdesc);
        pango_font_description_free(pangofontdesc);
    }
}

static gboolean sokoke_on_entry_focus_in_event(GtkEntry* entry, GdkEventFocus *event
 , gpointer userdata)
{
    gboolean bDefaultText = (gboolean)g_object_get_data(G_OBJECT(entry)
     , "sokoke_bDefaultText");
    if(bDefaultText)
    {
        gtk_entry_set_text(entry, "");
        g_object_set_data(G_OBJECT(entry), "sokoke_bDefaultText", (gpointer)FALSE);
        sokoke_widget_set_pango_font_style(GTK_WIDGET(entry), PANGO_STYLE_NORMAL);
    }
    return FALSE;
}

static gboolean sokoke_on_entry_focus_out_event(GtkEntry* entry, GdkEventFocus* event
 , gpointer userdata)
{
    const gchar* sText = gtk_entry_get_text(entry);
    if(sText[0] == '\0')
    {
        const gchar* sDefaultText = (const gchar*)g_object_get_data(
         G_OBJECT(entry), "sokoke_sDefaultText");
        gtk_entry_set_text(entry, sDefaultText);
        g_object_set_data(G_OBJECT(entry), "sokoke_bDefaultText", (gpointer)TRUE);
        sokoke_widget_set_pango_font_style(GTK_WIDGET(entry), PANGO_STYLE_ITALIC);
    }
    return FALSE;
}

void sokoke_entry_set_default_text(GtkEntry* entry, const gchar* sDefaultText)
{
    // Note: The default text initially overwrites any previous text
    gchar* sOldValue = g_object_get_data(G_OBJECT(entry), "sokoke_sDefaultText");
    if(!sOldValue)
    {
        g_object_set_data(G_OBJECT(entry), "sokoke_bDefaultText", (gpointer)TRUE);
        sokoke_widget_set_pango_font_style(GTK_WIDGET(entry), PANGO_STYLE_ITALIC);
        gtk_entry_set_text(entry, sDefaultText);
    }
    g_object_set_data(G_OBJECT(entry), "sokoke_sDefaultText", (gpointer)sDefaultText);
    g_signal_connect(entry, "focus-in-event"
     , G_CALLBACK(sokoke_on_entry_focus_in_event), NULL);
    g_signal_connect(entry, "focus-out-event"
     , G_CALLBACK(sokoke_on_entry_focus_out_event), NULL);
}

gchar* sokoke_key_file_get_string_default(GKeyFile* key_file
 , const gchar* group_name, const gchar* key, const gchar* def, GError* *error)
{
    gchar* value = g_key_file_get_string(key_file, group_name, key, error);
    return value == NULL ? g_strdup(def) : value;
}

gint sokoke_key_file_get_integer_default(GKeyFile* key_file
 , const gchar* group_name, const gchar* key, const gint def, GError** error)
{
    if(!g_key_file_has_key(key_file, group_name, key, NULL))
        return def;
    return g_key_file_get_integer(key_file, group_name, key, error);
}

gboolean sokoke_key_file_save_to_file(GKeyFile* key_file
 , const gchar* sFilename, GError** error)
{
    gchar* sData = g_key_file_to_data(key_file, NULL, error);
    if(!sData)
        return FALSE;
    FILE* fp;
    if(!(fp = fopen(sFilename, "w")))
    {
        *error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_ACCES
         , "Writing failed.");
        return FALSE;
    }
    fputs(sData, fp);
    fclose(fp);
    g_free(sData);
    return TRUE;
}

void sokoke_widget_get_text_size(GtkWidget* widget, const gchar* sText
 , gint* w, gint* h)
{
    PangoLayout* layout = gtk_widget_create_pango_layout(widget, sText);
    pango_layout_get_pixel_size(layout, w, h);
    g_object_unref(layout);
}

void sokoke_menu_item_set_accel(GtkMenuItem* menuitem, const gchar* sPath
 , const gchar* sKey, GdkModifierType accel_mods)
{
    if(sPath && *sPath)
    {
        gchar* path = g_strconcat("<", g_get_prgname(), ">/", sPath, NULL);
        gtk_menu_item_set_accel_path(GTK_MENU_ITEM(menuitem), path);
        guint keyVal = sKey ? gdk_keyval_from_name(sKey) : 0;
        gtk_accel_map_add_entry(path, keyVal, accel_mods);
        g_free(path);
    }
}

gboolean sokoke_entry_can_undo(GtkEntry* entry)
{
    // TODO: Can we undo the last input?
    return FALSE;
}

gboolean sokoke_entry_can_redo(GtkEntry* entry)
{
    // TODO: Can we redo the last input?
    return FALSE;
}

void sokoke_entry_undo(GtkEntry* entry)
{
    // TODO: Implement undo
    UNIMPLEMENTED
}

void sokoke_entry_redo(GtkEntry* entry)
{
    // TODO: Implement redo
    UNIMPLEMENTED
}

static gboolean sokoke_on_undo_entry_key_down(GtkEntry* widget, GdkEventKey* event
 , gpointer userdata)
{
    switch(event->keyval)
    {
    case GDK_Undo:
        sokoke_entry_undo(widget);
        return FALSE;
    case GDK_Redo:
        sokoke_entry_redo(widget);
        return FALSE;
    default:
        return FALSE;
    }
}

static void sokoke_on_undo_entry_populate_popup(GtkEntry* entry, GtkMenu* menu
 , gpointer userdata)
{
    // Enhance the entry's menu with undo and redo items.
    GtkWidget* menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_prepend((GtkMenuShell*)menu, menuitem);
    gtk_widget_show(menuitem);
    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_REDO, NULL);
    g_signal_connect(menuitem, "activate", G_CALLBACK(sokoke_entry_redo), userdata);
    gtk_widget_set_sensitive(menuitem, sokoke_entry_can_redo(entry));
    gtk_menu_shell_prepend((GtkMenuShell*)menu, menuitem);
    gtk_widget_show(menuitem);
    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_UNDO, NULL);
    g_signal_connect(menuitem, "activate", G_CALLBACK(sokoke_entry_undo), userdata);
    gtk_widget_set_sensitive(menuitem, sokoke_entry_can_undo(entry));
    gtk_menu_shell_prepend((GtkMenuShell*)menu, menuitem);
    gtk_widget_show(menuitem);
}

gboolean sokoke_entry_get_can_undo(GtkEntry* entry)
{
    // TODO: Is this entry undo enabled?
    return FALSE;
}

void sokoke_entry_set_can_undo(GtkEntry* entry, gboolean bCanUndo)
{
    if(bCanUndo)
    {
        g_signal_connect(entry, "key-press-event"
         , G_CALLBACK(sokoke_on_undo_entry_key_down), NULL);
        g_signal_connect(entry, "populate-popup"
         , G_CALLBACK(sokoke_on_undo_entry_populate_popup), NULL);
    }
    else
    {
        ; // TODO: disconnect signal
        UNIMPLEMENTED
    }
}
