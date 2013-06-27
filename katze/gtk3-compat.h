/*
 Copyright (C) 2011-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifndef H_GTK3_COMPAT_20110110
#define H_GTK3_COMPAT_20110110

G_BEGIN_DECLS

#if GTK_CHECK_VERSION (3, 2, 0) && defined (GTK_DISABLE_DEPRECATED)
    #define GTK_TYPE_VBOX GTK_TYPE_BOX
    #define GtkVBox GtkBox
    #define GtkVBoxClass GtkBoxClass
    #define gtk_vbox_new(hmg,spc) g_object_new (GTK_TYPE_BOX, \
        "homogeneous", hmg, "spacing", spc, \
        "orientation", GTK_ORIENTATION_VERTICAL, NULL)
    #define GTK_TYPE_HBOX GTK_TYPE_BOX
    #define GtkHBox GtkBox
    #define GtkHBoxClass GtkBoxClass
    #define gtk_hbox_new(hmg,spc) g_object_new (GTK_TYPE_BOX, \
        "homogeneous", hmg, "spacing", spc, \
        "orientation", GTK_ORIENTATION_HORIZONTAL, NULL)
    #define gtk_hseparator_new() g_object_new (GTK_TYPE_SEPARATOR, NULL)
    #define gtk_hpaned_new() g_object_new (GTK_TYPE_PANED, NULL)
    #define gtk_vpaned_new() g_object_new (GTK_TYPE_PANED, \
        "orientation", GTK_ORIENTATION_VERTICAL, NULL)
    /* FIXME */
    #define gtk_widget_render_icon(wdgt, stk, sz, dtl) \
        gtk_widget_render_icon_pixbuf(wdgt, stk, sz)
    #define gtk_widget_size_request(wdgt, req) \
        gtk_widget_get_preferred_size(wdgt, req, NULL)
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
    #define GTK_DIALOG_NO_SEPARATOR 0
#endif

#if !GTK_CHECK_VERSION (3, 2, 0)
    void gtk_entry_set_placeholder_text (GtkEntry* entry, const gchar* text);
    const gchar* gtk_entry_get_placeholder_text (GtkEntry* entry);
#endif

#ifndef GDK_KEY_Return
    #define GDK_KEY_0 GDK_0
    #define GDK_KEY_BackSpace GDK_BackSpace
    #define GDK_KEY_space GDK_space
    #define GDK_KEY_F5 GDK_F5
    #define GDK_KEY_KP_Equal GDK_KP_Equal
    #define GDK_KEY_KP_Enter GDK_KP_Enter
    #define GDK_KEY_KP_Left GDK_KP_Left
    #define GDK_KEY_KP_Right GDK_KP_Right
    #define GDK_KEY_KP_Delete GDK_KP_Delete
    #define GDK_KEY_KP_Down GDK_KP_Down
    #define GDK_KEY_KP_Up GDK_KP_Up
    #define GDK_KEY_KP_Divide GDK_KP_Divide
    #define GDK_KEY_Tab GDK_Tab
    #define GDK_KEY_ISO_Left_Tab GDK_ISO_Left_Tab
    #define GDK_KEY_equal GDK_equal
    #define GDK_KEY_ISO_Enter GDK_ISO_Enter
    #define GDK_KEY_Left GDK_Left
    #define GDK_KEY_Right GDK_Right
    #define GDK_KEY_Escape GDK_Escape
    #define GDK_KEY_Page_Up GDK_Page_Up
    #define GDK_KEY_Page_Down GDK_Page_Down
    #define GDK_KEY_Delete GDK_Delete
    #define GDK_KEY_Down GDK_Down
    #define GDK_KEY_Up GDK_Up
    #define GDK_KEY_B GDK_B
    #define GDK_KEY_H GDK_H
    #define GDK_KEY_J GDK_J
    #define GDK_KEY_Return GDK_Return
#endif

#ifdef GDK_WINDOWING_X11
    #include <gdk/gdkx.h>
    #ifndef GDK_IS_X11_DISPLAY
        #define GDK_IS_X11_DISPLAY(display) TRUE
    #endif
#endif

G_END_DECLS

#endif
