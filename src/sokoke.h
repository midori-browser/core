/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __SOKOKE_H__
#define __SOKOKE_H__ 1

#include <gtk/gtk.h>

// Many themes need this hack for small toolbars to work
#define GTK_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_BUTTON

void
sokoke_combo_box_add_strings(GtkComboBox* combobox, const gchar* sLabelFirst, ...);

void
sokoke_radio_action_set_current_value(GtkRadioAction* action, gint current_value);

void
sokoke_widget_set_visible(GtkWidget* widget, gboolean bVisibility);

void
sokoke_container_show_children(GtkContainer* container);

void
sokoke_widget_set_tooltip_text(GtkWidget* widget, const gchar* sText);

void
sokoke_tool_item_set_tooltip_text(GtkToolItem* toolitem, const gchar* sText);

void
sokoke_widget_popup(GtkWidget* widget, GtkMenu* menu, GdkEventButton* event);

gpointer
sokoke_xfce_header_new(const gchar* sIcon, const gchar* sTitle);

gpointer
sokoke_superuser_warning_new(void);

GtkWidget*
sokoke_hig_frame_new(const gchar* sLabel);

void
sokoke_widget_set_pango_font_style(GtkWidget* widget, PangoStyle style);

void
sokoke_entry_set_default_text(GtkEntry* entry, const gchar* sDefaultText);

gchar*
sokoke_key_file_get_string_default(GKeyFile* key_file
 , const gchar* group_name, const gchar* key, const gchar* def, GError* *error);

gint
sokoke_key_file_get_integer_default(GKeyFile* key_file
 , const gchar* group_name, const gchar* key, const gint def, GError* *error);

gboolean
sokoke_key_file_save_to_file(GKeyFile* key_file
 , const gchar* file, GError* *error);

void
sokoke_widget_get_text_size(GtkWidget* widget, const gchar* sText
 , gint* w, gint* h);

void
sokoke_menu_item_set_accel(GtkMenuItem* menuitem, const gchar* sPath
 , const gchar* sKey, GdkModifierType accel_mods);

gboolean
sokoke_entry_can_undo(GtkEntry* entry);

gboolean
sokoke_entry_can_redo(GtkEntry* entry);

void
sokoke_entry_undo(GtkEntry* entry);

void
sokoke_entry_redo(GtkEntry* entry);

gboolean
sokoke_entry_get_can_undo(GtkEntry* entry);

void
sokoke_entry_set_can_undo(GtkEntry* entry, gboolean bCanUndo);

#endif /* !__SOKOKE_H__ */
