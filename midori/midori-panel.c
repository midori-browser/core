/*
 Copyright (C) 2007-2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-panel.h"

#include "midori-view.h"
#include "sokoke.h"
#include "compat.h"

#include <glib/gi18n.h>

struct _MidoriPanel
{
    GtkHBox parent_instance;

    GtkWidget* toolbar;
    GtkWidget* toolbar_label;
    GtkWidget* frame;
    GtkWidget* toolbook;
    GtkWidget* notebook;
    GSList*    group;
    GtkMenu*   menu;
};

G_DEFINE_TYPE (MidoriPanel, midori_panel, GTK_TYPE_HBOX)

enum
{
    PROP_0,

    PROP_SHADOW_TYPE,
    PROP_MENU,
    PROP_PAGE
};

enum {
    CLOSE,
    SWITCH_PAGE,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_panel_finalize (GObject* object);

static void
midori_panel_set_property (GObject*      object,
                           guint         prop_id,
                           const GValue* value,
                           GParamSpec*   pspec);

static void
midori_panel_get_property (GObject*    object,
                           guint       prop_id,
                           GValue*     value,
                           GParamSpec* pspec);

static gboolean
midori_panel_close_cb (MidoriPanel* panel)
{
    gtk_widget_hide (GTK_WIDGET (panel));
    return FALSE;
}

static void
midori_cclosure_marshal_BOOLEAN__VOID (GClosure*     closure,
                                       GValue*       return_value,
                                       guint         n_param_values,
                                       const GValue* param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data)
{
    typedef gboolean(*GMarshalFunc_BOOLEAN__VOID) (gpointer  data1,
                                                   gpointer  data2);
    register GMarshalFunc_BOOLEAN__VOID callback;
    register GCClosure* cc = (GCClosure*) closure;
    register gpointer data1, data2;
    gboolean v_return;

    g_return_if_fail (return_value != NULL);
    g_return_if_fail (n_param_values == 1);

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_BOOLEAN__VOID) (marshal_data
        ? marshal_data : cc->callback);
    v_return = callback (data1, data2);
    g_value_set_boolean (return_value, v_return);
}

static void
midori_panel_class_init (MidoriPanelClass* class)
{

    signals[CLOSE] = g_signal_new (
        "close",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriPanelClass, close),
        g_signal_accumulator_true_handled,
        NULL,
        midori_cclosure_marshal_BOOLEAN__VOID,
        G_TYPE_BOOLEAN, 0);

    signals[SWITCH_PAGE] = g_signal_new (
        "switch-page",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriPanelClass, switch_page),
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    class->close = midori_panel_close_cb;

    GObjectClass* gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_panel_finalize;
    gobject_class->set_property = midori_panel_set_property;
    gobject_class->get_property = midori_panel_get_property;

    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    g_object_class_install_property (gobject_class,
                                     PROP_SHADOW_TYPE,
                                     g_param_spec_enum (
                                     "shadow-type",
                                     "Shadow Type",
                                     "Appearance of the shadow around each panel",
                                     GTK_TYPE_SHADOW_TYPE,
                                     GTK_SHADOW_NONE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_MENU,
                                     g_param_spec_object (
                                     "menu",
                                     "Menu",
                                     "Menu to hold panel items",
                                     GTK_TYPE_MENU,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PAGE,
                                     g_param_spec_int (
                                     "page",
                                     "Page",
                                     "The index of the current page",
                                     -1, G_MAXINT, -1,
                                     flags));
}

static void
midori_panel_button_close_clicked_cb (GtkWidget*   toolitem,
                                      MidoriPanel* panel)
{
    gboolean return_value;
    g_signal_emit (panel, signals[CLOSE], 0, &return_value);
}

static void
midori_panel_init (MidoriPanel* panel)
{
    /* Create the sidebar */
    panel->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (panel->toolbar), GTK_TOOLBAR_BOTH);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (panel->toolbar),
                               GTK_ICON_SIZE_BUTTON);
    gtk_toolbar_set_orientation (GTK_TOOLBAR (panel->toolbar),
                                 GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (panel), panel->toolbar, FALSE, FALSE, 0);
    gtk_widget_show_all (panel->toolbar);
    GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (panel), vbox, TRUE, TRUE, 0);

    /* Create the titlebar */
    GtkWidget* labelbar = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (labelbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (labelbar), GTK_TOOLBAR_ICONS);
    GtkToolItem* toolitem = gtk_tool_item_new ();
    gtk_tool_item_set_expand (toolitem, TRUE);
    panel->toolbar_label = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (panel->toolbar_label), 0, 0.5);
    gtk_container_add (GTK_CONTAINER (toolitem), panel->toolbar_label);
    gtk_container_set_border_width (GTK_CONTAINER (toolitem), 6);
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("Close panel"));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (toolitem), _("Close panel"));
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (midori_panel_button_close_clicked_cb), panel);
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    gtk_box_pack_start (GTK_BOX (vbox), labelbar, FALSE, FALSE, 0);
    gtk_widget_show_all (vbox);

    /* Create the toolbook */
    panel->toolbook = gtk_notebook_new ();
    gtk_notebook_set_show_border (GTK_NOTEBOOK (panel->toolbook), FALSE);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (panel->toolbook), FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), panel->toolbook, FALSE, FALSE, 0);
    gtk_widget_show (panel->toolbook);

    /* Create the notebook */
    panel->notebook = gtk_notebook_new ();
    gtk_notebook_set_show_border (GTK_NOTEBOOK (panel->notebook), FALSE);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (panel->notebook), FALSE);
    panel->frame = gtk_frame_new (NULL);
    gtk_container_add (GTK_CONTAINER (panel->frame), panel->notebook);
    gtk_box_pack_start (GTK_BOX (vbox), panel->frame, TRUE, TRUE, 0);
    gtk_widget_show_all (panel->frame);
}

static void
midori_panel_finalize (GObject* object)
{
    MidoriPanel* panel = MIDORI_PANEL (object);

    if (panel->menu)
    {
        /* FIXME: Remove all menu items */
    }

    G_OBJECT_CLASS (midori_panel_parent_class)->finalize (object);
}

static void
midori_panel_set_property (GObject*      object,
                           guint         prop_id,
                           const GValue* value,
                           GParamSpec*   pspec)
{
    MidoriPanel* panel = MIDORI_PANEL (object);

    switch (prop_id)
    {
    case PROP_SHADOW_TYPE:
        gtk_frame_set_shadow_type (GTK_FRAME (panel->frame),
                                   g_value_get_enum (value));
        break;
    case PROP_MENU:
        katze_object_assign (panel->menu, g_value_get_object (value));
        /* FIXME: Move existing items to the new menu */
        break;
    case PROP_PAGE:
        midori_panel_set_current_page (panel, g_value_get_int (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_panel_get_property (GObject*    object,
                           guint       prop_id,
                           GValue*     value,
                           GParamSpec* pspec)
{
    MidoriPanel* panel = MIDORI_PANEL (object);

    switch (prop_id)
    {
    case PROP_SHADOW_TYPE:
        g_value_set_enum (value,
            gtk_frame_get_shadow_type (GTK_FRAME (panel->frame)));
        break;
    case PROP_MENU:
        g_value_set_object (value, panel->menu);
        break;
    case PROP_PAGE:
        g_value_set_int (value, midori_panel_get_current_page (panel));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_panel_new:
 *
 * Creates a new empty panel.
 *
 * Return value: a new #MidoriPanel
 **/
GtkWidget*
midori_panel_new (void)
{
    MidoriPanel* panel = g_object_new (MIDORI_TYPE_PANEL,
                                       NULL);

    return GTK_WIDGET (panel);
}

static void
midori_panel_menu_item_activate_cb (GtkWidget*   widget,
                                    MidoriPanel* panel)
{
    GtkWidget* child;
    GtkToolItem* toolitem;
    guint n;

    child = g_object_get_data (G_OBJECT (widget), "page");
    toolitem = g_object_get_data (G_OBJECT (widget), "toolitem");

    if (toolitem)
    {
        /* Unset the button before setting it ensures that
           it will emit signals even if it was active before */
        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (toolitem), FALSE);
        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (toolitem), TRUE);
    }
    else
    {
        n = midori_panel_page_num (panel, child);
        midori_panel_set_current_page (panel, n);
        g_signal_emit (panel, signals[SWITCH_PAGE], 0, n);
        gtk_widget_show (GTK_WIDGET (panel));
    }
}

/**
 * midori_panel_append_page:
 * @panel: a #MidoriPanel
 * @child: the child widget
 * @toolbar: a toolbar widget, or %NULL
 * @stock_id: a stock ID
 * @label: a string to use as the label
 *
 * Appends a new page to the panel. If @toolbar is specified it will
 * be packed above @child.
 *
 * In the case of an error, -1 is returned.
 *
 * Return value: the index of the new page, or -1
 **/
gint
midori_panel_append_page (MidoriPanel* panel,
                          GtkWidget*   child,
                          GtkWidget*   toolbar,
                          const gchar* stock_id,
                          const gchar* label)
{
    GtkWidget* scrolled;
    GtkWidget* widget;
    GObjectClass* gobject_class;
    guint n;
    GtkToolItem* toolitem;
    GtkWidget* image;
    GtkWidget* menuitem;

    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);
    g_return_val_if_fail (GTK_IS_WIDGET (child), -1);
    g_return_val_if_fail (!toolbar || GTK_IS_WIDGET (toolbar), -1);
    g_return_val_if_fail (stock_id != NULL, -1);
    g_return_val_if_fail (label != NULL, -1);

    if (GTK_IS_SCROLLED_WINDOW (child))
        scrolled = child;
    else
    {
        scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        GTK_WIDGET_SET_FLAGS (scrolled, GTK_CAN_FOCUS);
        gtk_widget_show (scrolled);
        gobject_class = G_OBJECT_GET_CLASS (child);
        if (GTK_WIDGET_CLASS (gobject_class)->set_scroll_adjustments_signal)
            widget = child;
        else
        {
            widget = gtk_viewport_new (NULL, NULL);
            gtk_widget_show (widget);
            gtk_container_add (GTK_CONTAINER (widget), child);
        }
        gtk_container_add (GTK_CONTAINER (scrolled), widget);
    }
    gtk_container_add (GTK_CONTAINER (panel->notebook), scrolled);

    if (!toolbar)
        toolbar = gtk_event_box_new ();
    gtk_widget_show (toolbar);
    gtk_container_add (GTK_CONTAINER (panel->toolbook), toolbar);

    n = midori_panel_page_num (panel, scrolled);

    g_object_set_data (G_OBJECT (child), "label", (gchar*)label);

    toolitem = gtk_radio_tool_button_new (panel->group);
    panel->group = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (
                                                   toolitem));
    image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), label);
    g_object_set_data (G_OBJECT (toolitem), "page", child);
    g_signal_connect (toolitem, "clicked",
                      G_CALLBACK (midori_panel_menu_item_activate_cb), panel);
    gtk_widget_show_all (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (panel->toolbar), toolitem, -1);

    if (panel->menu)
    {
        menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
        gtk_widget_show (menuitem);
        g_object_set_data (G_OBJECT (menuitem), "page", child);
        g_object_set_data (G_OBJECT (menuitem), "toolitem", toolitem);
        g_signal_connect (menuitem, "activate",
                          G_CALLBACK (midori_panel_menu_item_activate_cb),
                          panel);
        gtk_menu_shell_append (GTK_MENU_SHELL (panel->menu), menuitem);
    }

    return n;
}

/**
 * midori_panel_get_current_page:
 * @panel: a #MidoriPanel
 *
 * Retrieves the index of the currently selected page.
 *
 * If @panel has no children, -1 is returned.
 *
 * Return value: the index of the current page, or -1
 **/
gint
midori_panel_get_current_page (MidoriPanel* panel)
{
    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);

    return gtk_notebook_get_current_page (GTK_NOTEBOOK (panel->notebook));
}

static GtkWidget*
_midori_panel_child_for_scrolled (MidoriPanel* panel,
                                  GtkWidget*   scrolled)
{
    GtkWidget* child;

    /* This is a lazy hack, we should have a way of determining
       whether the scrolled is the actual child. */
    if (MIDORI_IS_VIEW (scrolled))
        return scrolled;
    child = gtk_bin_get_child (GTK_BIN (scrolled));
    if (GTK_IS_VIEWPORT (child))
        child = gtk_bin_get_child (GTK_BIN (child));
    return child;
}

/**
 * midori_panel_get_nth_page:
 * @panel: a #MidoriPanel
 *
 * Retrieves the child widget of the nth page.
 *
 * If @panel has no children, %NULL is returned.
 *
 * Return value: the child widget of the new page, or %NULL
 **/
GtkWidget*
midori_panel_get_nth_page (MidoriPanel* panel,
                           guint        page_num)
{
    GtkWidget* scrolled;

    g_return_val_if_fail (MIDORI_IS_PANEL (panel), NULL);

    scrolled = gtk_notebook_get_nth_page (
        GTK_NOTEBOOK (panel->notebook), page_num);
    if (scrolled)
        return _midori_panel_child_for_scrolled (panel, scrolled);
    return NULL;
}

/**
 * midori_panel_get_n_pages:
 * @panel: a #MidoriPanel
 *
 * Retrieves the number of pages contained in the panel.
 *
 * Return value: the number of pages
 **/
guint
midori_panel_get_n_pages (MidoriPanel* panel)
{
    g_return_val_if_fail (MIDORI_IS_PANEL (panel), 0);

    return gtk_notebook_get_n_pages (GTK_NOTEBOOK (panel->notebook));
}

static GtkWidget*
_midori_panel_scrolled_for_child (MidoriPanel* panel,
                                  GtkWidget*   child)
{
    GtkWidget* scrolled;

    /* This is a lazy hack, we should have a way of determining
       whether the scrolled is the actual child. */
    if (MIDORI_IS_VIEW (child))
        return child;
    scrolled = gtk_widget_get_parent (GTK_WIDGET (child));
    if (GTK_IS_VIEWPORT (scrolled))
        scrolled = gtk_widget_get_parent (scrolled);
    return scrolled;
}

/**
 * midori_panel_page_num:
 * @panel: a #MidoriPanel
 *
 * Retrieves the index of the page associated to @widget.
 *
 * If @panel has no children, -1 is returned.
 *
 * Return value: the index of page associated to @widget, or -1
 **/
gint
midori_panel_page_num (MidoriPanel* panel,
                       GtkWidget*   child)
{
    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);

    GtkWidget* scrolled = _midori_panel_scrolled_for_child (panel, child);
    return gtk_notebook_page_num (GTK_NOTEBOOK (panel->notebook), scrolled);
}

/**
 * midori_panel_set_current_page:
 * @panel: a #MidoriPanel
 * @n: index of the page to switch to, or -1 to mean the last page
 *
 * Switches to the page with the given index.
 *
 * The child must be visible, otherwise the underlying GtkNotebook will
 * silently ignore the attempt to switch the page.
 **/
void
midori_panel_set_current_page (MidoriPanel* panel,
                               gint         n)
{
    g_return_if_fail (MIDORI_IS_PANEL (panel));

    gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->toolbook), n);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->notebook), n);
    GtkWidget* child = midori_panel_get_nth_page (panel, n);
    if (child)
    {
        const gchar* label = g_object_get_data (G_OBJECT (child), "label");
        g_object_set (panel->toolbar_label, "label", label, NULL);
    }
}
