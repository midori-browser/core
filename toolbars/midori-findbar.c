/*
 Copyright (C) 2008-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-findbar.h"

#include "midori-browser.h"
#include "midori-platform.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "config.h"

struct _MidoriFindbar
{
    GtkToolbar parent_instance;
    GtkWidget* find_text;
    GtkToolItem* previous;
    GtkToolItem* next;
    GtkToolItem* find_case;
    GtkToolItem* find_close;
    gboolean find_typing;
};

struct _MidoriFindbarClass
{
    GtkToolbarClass parent_class;
};

G_DEFINE_TYPE (MidoriFindbar, midori_findbar, GTK_TYPE_TOOLBAR);

static void
midori_findbar_class_init (MidoriFindbarClass* class)
{
    /* Nothing to do */
}

static void
midori_findbar_set_icon (MidoriFindbar*       findbar,
                         GtkEntryIconPosition icon_pos,
                         const gchar*         icon_name)
{
    if (icon_name != NULL)
    {
        gchar* symbolic_icon_name = g_strconcat (icon_name, "-symbolic", NULL);
        gtk_entry_set_icon_from_gicon (GTK_ENTRY (findbar->find_text), icon_pos,
            g_themed_icon_new_with_default_fallbacks (symbolic_icon_name));
        g_free (symbolic_icon_name);
    }
    else
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (findbar->find_text),
                                                icon_pos, NULL);
}

static void
midori_findbar_done (MidoriFindbar* findbar)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (findbar));
    GtkWidget* view = midori_browser_get_current_tab (browser);

    midori_tab_unmark_text_matches (MIDORI_TAB (view));
    gtk_widget_hide (GTK_WIDGET (findbar));
    findbar->find_typing = FALSE;
    gtk_widget_grab_focus (view);
}

static gboolean
midori_findbar_find_key_press_event_cb (MidoriFindbar* findbar,
                                        GdkEventKey*   event)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        midori_findbar_done (findbar);
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Return
          && (event->state & GDK_SHIFT_MASK))
    {
        midori_findbar_continue (findbar, FALSE);
        return TRUE;
    }

    return FALSE;
}

static gboolean
midori_findbar_case_sensitive (MidoriFindbar* findbar)
{
    /* Smart case while typing: foo or fOO lowercase, Foo or FOO uppercase */
    if (findbar->find_typing)
    {
        const gchar* text = gtk_entry_get_text (GTK_ENTRY (findbar->find_text));
        return g_unichar_isupper (g_utf8_get_char (text));
    }
    return gtk_toggle_tool_button_get_active (
        GTK_TOGGLE_TOOL_BUTTON (findbar->find_case));
}

void
midori_findbar_continue (MidoriFindbar* findbar,
                         gboolean       forward)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (findbar));
    const gchar* text;
    gboolean case_sensitive;
    GtkWidget* view;

    if (!(view = midori_browser_get_current_tab (browser)))
        return;

    text = gtk_entry_get_text (GTK_ENTRY (findbar->find_text));
    case_sensitive = midori_findbar_case_sensitive (findbar);
    midori_tab_find (MIDORI_TAB (view), text, case_sensitive, forward);
}

/**
 * midori_findbar_get_text:
 * @findbar: #MidoriFindbar
 *
 * Returns: the text typed in the entry
 *
 * Since: 0.4.5
 **/
const gchar*
midori_findbar_get_text (MidoriFindbar* findbar)
{
    g_return_val_if_fail (MIDORI_IS_FINDBAR (findbar), NULL);

    return gtk_entry_get_text (GTK_ENTRY (findbar->find_text));
}

void
midori_findbar_invoke (MidoriFindbar* findbar,
                       const gchar*   selected_text)
{
    if (gtk_widget_get_visible (GTK_WIDGET (findbar)))
        gtk_widget_grab_focus (GTK_WIDGET (findbar->find_text));
    else
    {
        midori_findbar_set_icon (findbar, GTK_ENTRY_ICON_PRIMARY, STOCK_EDIT_FIND);
        gtk_widget_show (GTK_WIDGET (findbar->find_case));
        gtk_widget_show (GTK_WIDGET (findbar->find_close));
        if (selected_text != NULL)
            gtk_entry_set_text (GTK_ENTRY (findbar->find_text), selected_text);
        gtk_widget_show (GTK_WIDGET (findbar));
        gtk_widget_grab_focus (GTK_WIDGET (findbar->find_text));
    }
}

static void
midori_findbar_next_activate_cb (GtkWidget*     entry,
                                 MidoriFindbar* findbar)
{
    midori_findbar_continue (findbar, TRUE);
}

static void
midori_findbar_previous_clicked_cb (GtkWidget*     entry,
                                    MidoriFindbar* findbar)
{
    midori_findbar_continue (findbar, FALSE);
}

static void
midori_findbar_button_close_clicked_cb (GtkWidget*     widget,
                                        MidoriFindbar* findbar)
{
    midori_findbar_done (findbar);
}

static void
midori_findbar_preedit_changed_cb (GtkWidget*     entry,
                                   const gchar*   preedit,
                                   MidoriFindbar* findbar)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (entry);
    GtkWidget* view = midori_browser_get_current_tab (browser);
    midori_tab_unmark_text_matches (MIDORI_TAB (view));
    if (g_utf8_strlen (preedit, -1) >= 1)
    {
        gboolean case_sensitive = midori_findbar_case_sensitive (findbar);
        midori_findbar_set_icon (findbar, GTK_ENTRY_ICON_SECONDARY, STOCK_EDIT_CLEAR);
        midori_tab_find (MIDORI_TAB (view), preedit, case_sensitive, TRUE);
    }
    else
        midori_findbar_set_icon (findbar, GTK_ENTRY_ICON_SECONDARY, NULL);
}

static void
midori_findbar_text_changed_cb (GtkWidget*     entry,
                                MidoriFindbar* findbar)
{
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (entry));
    midori_findbar_preedit_changed_cb (entry, text, findbar);
}

static gboolean
midori_findbar_text_focus_out_event_cb (GtkWidget*     entry,
                                        GdkEventFocus* event,
                                        MidoriFindbar* findbar)
{
    if (findbar->find_typing)
        midori_findbar_done (findbar);
    return FALSE;
}

static void
midori_findbar_init (MidoriFindbar* findbar)
{
    GtkToolItem* toolitem;

    gtk_widget_set_name (GTK_WIDGET (findbar), "MidoriFindbar");
    katze_widget_add_class (GTK_WIDGET (findbar), "bottom-toolbar");
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (findbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (findbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (findbar), FALSE);
    g_signal_connect (findbar, "key-press-event",
        G_CALLBACK (midori_findbar_find_key_press_event_cb), NULL);

    toolitem = gtk_tool_item_new ();
    gtk_container_set_border_width (GTK_CONTAINER (toolitem), 6);
    gtk_container_add (GTK_CONTAINER (toolitem),
        /* i18n: A panel at the bottom, to search text in pages */
        gtk_label_new_with_mnemonic (_("_Inline Find:")));
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), toolitem, -1);
    findbar->find_text = sokoke_search_entry_new (NULL);
    g_signal_connect (findbar->find_text, "activate",
        G_CALLBACK (midori_findbar_next_activate_cb), findbar);
    g_signal_connect (findbar->find_text, "preedit-changed",
        G_CALLBACK (midori_findbar_preedit_changed_cb), findbar);
    g_signal_connect (findbar->find_text, "changed",
        G_CALLBACK (midori_findbar_text_changed_cb), findbar);
    g_signal_connect (findbar->find_text, "focus-out-event",
        G_CALLBACK (midori_findbar_text_focus_out_event_cb), findbar);
    toolitem = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (toolitem), findbar->find_text);
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), toolitem, -1);
    findbar->previous = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
    g_signal_connect (findbar->previous, "clicked",
        G_CALLBACK (midori_findbar_previous_clicked_cb), findbar);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (findbar->previous), _("Previous"));
    gtk_tool_item_set_is_important (findbar->previous, TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), findbar->previous, -1);
    findbar->next = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
    g_signal_connect (findbar->next, "clicked",
        G_CALLBACK (midori_findbar_next_activate_cb), findbar);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (findbar->next), _("Next"));
    gtk_tool_item_set_is_important (findbar->next, TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), findbar->next, -1);
    findbar->find_case = gtk_toggle_tool_button_new_from_stock (GTK_STOCK_SPELL_CHECK);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (findbar->find_case), _("Match Case"));
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (findbar->find_case), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), findbar->find_case, -1);
    toolitem = gtk_separator_tool_item_new ();
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (toolitem), FALSE);
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (toolitem), TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), toolitem, -1);
    findbar->find_close = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (findbar->find_close),
                               _("Close Findbar"));
    g_signal_connect (findbar->find_close, "clicked",
        G_CALLBACK (midori_findbar_button_close_clicked_cb), findbar);
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), findbar->find_close, -1);
    gtk_container_foreach (GTK_CONTAINER (findbar),
                           (GtkCallback)(gtk_widget_show_all), NULL);
}

void
midori_findbar_set_can_find (MidoriFindbar* findbar,
                             gboolean       can_find)
{
    gtk_widget_set_sensitive (GTK_WIDGET (findbar->next), can_find);
    gtk_widget_set_sensitive (GTK_WIDGET (findbar->previous), can_find);
}

void
midori_findbar_search_text (MidoriFindbar* findbar,
                            GtkWidget*     view,
                            gboolean       found,
                            const gchar*   typing)
{
    const gchar* text;
    gboolean case_sensitive;

    midori_findbar_set_icon (findbar, GTK_ENTRY_ICON_PRIMARY, found ? STOCK_EDIT_FIND : STOCK_STOP);

    if (typing)
    {
        gint position = -1;

        findbar->find_typing = TRUE;
        gtk_widget_hide (GTK_WIDGET (findbar->find_case));
        gtk_widget_hide (GTK_WIDGET (findbar->find_close));
        if (!gtk_widget_get_visible (GTK_WIDGET (findbar)))
            gtk_entry_set_text (GTK_ENTRY (findbar->find_text), "");
        gtk_widget_show (GTK_WIDGET (findbar));
        gtk_widget_grab_focus (findbar->find_text);
        gtk_editable_insert_text (GTK_EDITABLE (findbar->find_text),
                                 typing, -1, &position);
        gtk_editable_set_position (GTK_EDITABLE (findbar->find_text), -1);
    }
    if (gtk_widget_get_visible (GTK_WIDGET (findbar)) && !typing)
    {
        text = gtk_entry_get_text (GTK_ENTRY (findbar->find_text));
        case_sensitive = midori_findbar_case_sensitive (findbar);
        midori_tab_find (MIDORI_TAB (view), text, case_sensitive, TRUE);
    }
}

void
midori_findbar_set_close_button_left (MidoriFindbar* findbar,
                                      gboolean       close_button_left)
{
    g_object_ref (findbar->find_close);
    gtk_container_remove (GTK_CONTAINER (findbar),
                          GTK_WIDGET (findbar->find_close));
    gtk_toolbar_insert (GTK_TOOLBAR (findbar), findbar->find_close,
        close_button_left ? 0 : -1);
    g_object_unref (findbar->find_close);
}

