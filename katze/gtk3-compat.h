#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifndef H_GTK3_COMPAT_20110110
#define H_GTK3_COMPAT_20110110

#if !GTK_CHECK_VERSION (2, 24 ,0)
    #define gtk_combo_box_text_append_text gtk_combo_box_append_text
    #define gtk_combo_box_text_new gtk_combo_box_new_text
    #define gtk_combo_box_text_get_active_text gtk_combo_box_get_active_text
    #define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
    #define GtkComboBoxText GtkComboBox
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

#endif
