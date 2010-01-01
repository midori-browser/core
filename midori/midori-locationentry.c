/*
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-locationentry.h"

#include "gtkiconentry.h"
#include "sokoke.h"
#include <gdk/gdkkeysyms.h>

struct _MidoriLocationEntry
{
    GtkComboBoxEntry parent_instance;
};

struct _MidoriLocationEntryClass
{
    GtkComboBoxEntryClass parent_class;
};

G_DEFINE_TYPE (MidoriLocationEntry,
    midori_location_entry, GTK_TYPE_COMBO_BOX_ENTRY)

static void
midori_location_entry_class_init (MidoriLocationEntryClass* class)
{

}

static void
midori_location_entry_init (MidoriLocationEntry* location_entry)
{
    GtkWidget* entry;
    #if HAVE_HILDON
    HildonGtkInputMode mode;
    #endif

    /* We want the widget to have appears-as-list applied */
    gtk_rc_parse_string ("style \"midori-location-entry-style\" {\n"
                         "  GtkComboBox::appears-as-list = 1\n }\n"
                         "widget_class \"*MidoriLocationEntry\" "
                         "style \"midori-location-entry-style\"\n");

    #if HAVE_HILDON
    entry = gtk_entry_new ();
    mode = hildon_gtk_entry_get_input_mode (GTK_ENTRY (entry));
    mode &= ~HILDON_GTK_INPUT_MODE_AUTOCAP;
    hildon_gtk_entry_set_input_mode (GTK_ENTRY (entry), mode);
    #else
    entry = gtk_icon_entry_new ();
    gtk_icon_entry_set_icon_from_stock (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_PRIMARY, GTK_STOCK_FILE);
    gtk_icon_entry_set_icon_highlight (GTK_ICON_ENTRY (entry),
         GTK_ICON_ENTRY_SECONDARY, TRUE);
    #endif
    gtk_widget_show (entry);
    gtk_container_add (GTK_CONTAINER (location_entry), entry);
}
