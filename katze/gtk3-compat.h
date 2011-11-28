#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifndef H_GTK3_COMPAT_20110110
#define H_GTK3_COMPAT_20110110

G_BEGIN_DECLS

#if GTK_CHECK_VERSION (3, 2, 0)
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

#if !GLIB_CHECK_VERSION (2, 32, 0)
    #define G_SOURCE_REMOVE   FALSE
    #define G_SOURCE_CONTINUE TRUE
#endif

#if !GLIB_CHECK_VERSION (2, 30, 0)
    #define g_format_size(sz) g_format_size_for_display ((goffset)sz)
#endif

#if !GTK_CHECK_VERSION (2, 14, 0)
    #define gtk_dialog_get_content_area(dlg) dlg->vbox
    #define gtk_dialog_get_action_area(dlg) dlg->action_area
    #define gtk_widget_get_window(wdgt) wdgt->window
    #define gtk_adjustment_get_page_size(adj) adj->page_size
    #define gtk_adjustment_get_upper(adj) adj->upper
    #define gtk_adjustment_get_lower(adj) adj->lower
    #define gtk_adjustment_get_value(adj) adj->value
#endif

#if !GTK_CHECK_VERSION (2, 16, 0)
    #define GTK_ACTIVATABLE GTK_WIDGET
    #define gtk_activatable_get_related_action gtk_widget_get_action
    #define gtk_menu_item_set_label(menuitem, label) \
        gtk_label_set_label (GTK_LABEL (GTK_BIN (menuitem)->child), \
                             label ? label : "");
    #define gtk_image_menu_item_set_always_show_image(menuitem, yesno) ()
#endif

#if !GTK_CHECK_VERSION (2, 18, 0)
    #define gtk_widget_is_toplevel(widget) GTK_WIDGET_TOPLEVEL (widget)
    #define gtk_widget_has_focus(widget) GTK_WIDGET_HAS_FOCUS (widget)
    #define gtk_widget_get_visible(widget) GTK_WIDGET_VISIBLE (widget)
    #define gtk_widget_get_sensitive(widget) GTK_WIDGET_IS_SENSITIVE (widget)
    #define gtk_widget_set_can_focus(widget,flag) \
        GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS)
    #define gtk_widget_get_allocation(wdgt, alloc) *alloc = wdgt->allocation
    #define gtk_widget_get_has_window(wdgt) !GTK_WIDGET_NO_WINDOW (wdgt)
    #define gtk_widget_get_allocation(wdgt, alloc) *alloc = wdgt->allocation
    #define gtk_widget_set_window(wdgt, wndw) wdgt->window = wndw
    #define gtk_widget_is_drawable GTK_WIDGET_DRAWABLE
    #define gtk_widget_get_drawable GTK_WIDGET_VISIBLE
    #define gtk_widget_set_has_window(wdgt, wnd) \
        if (wnd) GTK_WIDGET_UNSET_FLAGS (wdgt, GTK_NO_WINDOW); \
        else GTK_WIDGET_SET_FLAGS (wdgt, GTK_NO_WINDOW)
#endif

#if !GTK_CHECK_VERSION (2, 20, 0)
    #define gtk_widget_get_realized(widget) GTK_WIDGET_REALIZED (widget)
    #define gtk_widget_set_realized(wdgt, real) \
        if (real) GTK_WIDGET_SET_FLAGS (wdgt, GTK_REALIZED); \
        else GTK_WIDGET_UNSET_FLAGS (wdgt, GTK_REALIZED)
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
    #define GTK_DIALOG_NO_SEPARATOR 0
#endif

#if !GTK_CHECK_VERSION (3, 2, 0) && defined (HAVE_HILDON_2_2)
    #define gtk_entry_set_placeholder_text hildon_gtk_entry_set_placeholder_text
#elif !GTK_CHECK_VERSION (3, 2, 0)
    #define gtk_entry_set_placeholder_text sokoke_entry_set_default_text
#endif

#if !GTK_CHECK_VERSION(2, 12, 0)

void
gtk_widget_set_has_tooltip             (GtkWidget*         widget,
                                        gboolean           has_tooltip);

void
gtk_widget_set_tooltip_text            (GtkWidget*         widget,
                                        const gchar*       text);

void
gtk_tool_item_set_tooltip_text         (GtkToolItem*       toolitem,
                                        const gchar*       text);

#endif

#if !GTK_CHECK_VERSION (2, 24 ,0)
    #define gtk_combo_box_text_append_text gtk_combo_box_append_text
    #define gtk_combo_box_text_new gtk_combo_box_new_text
    #define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
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

G_END_DECLS

#endif
