/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-panel.h"

#include "midori-view.h"

#include "compat.h"
#include "marshal.h"
#include "sokoke.h"

#include <glib/gi18n.h>

struct _MidoriPanel
{
    GtkHBox parent_instance;

    GtkWidget* toolbar;
    GtkToolItem* button_align;
    GtkToolItem* button_detach;
    GtkWidget* toolbar_label;
    GtkWidget* frame;
    GtkWidget* toolbook;
    GtkWidget* notebook;
    GtkMenu*   menu;

    gboolean right_aligned;
};

struct _MidoriPanelClass
{
    GtkHBoxClass parent_class;

    /* Signals */
    gboolean
    (*close)                  (MidoriPanel*   panel);
};

G_DEFINE_TYPE (MidoriPanel, midori_panel, GTK_TYPE_HBOX)

enum
{
    PROP_0,

    PROP_SHADOW_TYPE,
    PROP_MENU,
    PROP_PAGE,
    PROP_RIGHT_ALIGNED,
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
midori_panel_close (MidoriPanel* panel)
{
    gtk_widget_hide (GTK_WIDGET (panel));
    return FALSE;
}

static void
midori_panel_class_init (MidoriPanelClass* class)
{
    GObjectClass* gobject_class;
    GParamFlags flags;

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
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_panel_finalize;
    gobject_class->set_property = midori_panel_set_property;
    gobject_class->get_property = midori_panel_get_property;

    class->close = midori_panel_close;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS;

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
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_PAGE,
                                     g_param_spec_int (
                                     "page",
                                     "Page",
                                     "The index of the current page",
                                     -1, G_MAXINT, -1,
                                     flags));

    /**
    * MidoriWebSettings:right-aligned:
    *
    * Whether to align the panel on the right.
    *
    * Since: 0.1.3
    */
    g_object_class_install_property (gobject_class,
                                     PROP_RIGHT_ALIGNED,
                                     g_param_spec_boolean (
                                     "right-aligned",
                                     "Right aligned",
                                     "Whether the panel is aligned to the right",
                                     FALSE,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
midori_panel_button_close_clicked_cb (GtkWidget*   toolitem,
                                      MidoriPanel* panel)
{
    gboolean return_value;
    g_signal_emit (panel, signals[CLOSE], 0, &return_value);
}

static GtkToolItem*
midori_panel_construct_tool_item (MidoriPanel*    panel,
                                  MidoriViewable* viewable);

static GtkWidget*
_midori_panel_child_for_scrolled (MidoriPanel* panel,
                                  GtkWidget*   scrolled);

static gboolean
midori_panel_detached_window_delete_event_cb (GtkWidget*   window,
                                              GdkEvent*    event,
                                              MidoriPanel* panel)
{
    /* FIXME: The panel will not end up at its original position */
    /* FIXME: The menuitem may be mispositioned */
    GtkWidget* vbox = gtk_bin_get_child (GTK_BIN (window));
    GtkWidget* scrolled = g_object_get_data (G_OBJECT (window), "scrolled");
    GtkWidget* toolbar = g_object_get_data (G_OBJECT (scrolled), "panel-toolbar");
    GtkWidget* menuitem = g_object_get_data (G_OBJECT (scrolled), "panel-menuitem");
    GtkToolItem* toolitem;
    gint n;

    g_object_ref (toolbar);
    gtk_container_remove (GTK_CONTAINER (vbox), toolbar);
    gtk_container_add (GTK_CONTAINER (panel->toolbook), toolbar);
    g_object_unref (toolbar);
    g_object_ref (scrolled);
    gtk_container_remove (GTK_CONTAINER (vbox), scrolled);
    n = gtk_notebook_append_page (GTK_NOTEBOOK (panel->notebook), scrolled, NULL);
    g_object_unref (scrolled);
    toolitem = midori_panel_construct_tool_item (panel,
        MIDORI_VIEWABLE (_midori_panel_child_for_scrolled (panel, scrolled)));
    if (menuitem)
    {
        gtk_widget_show (menuitem);
        g_object_set_data (G_OBJECT (menuitem), "toolitem", toolitem);
    }
    midori_panel_set_current_page (panel, n);
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (toolitem), TRUE);
    return FALSE;
}

static void
midori_panel_widget_destroy_cb (GtkWidget* viewable,
                                GtkWidget* widget)
{
    gtk_widget_destroy (widget);
    g_signal_handlers_disconnect_by_func (
        viewable, midori_panel_widget_destroy_cb, widget);
}

static void
midori_panel_button_detach_clicked_cb (GtkWidget*   toolbutton,
                                       MidoriPanel* panel)
{
    /* FIXME: Use stock icon for window */
    /* FIXME: What happens when the browser is destroyed? */
    /* FIXME: What about multiple browsers? */
    /* FIXME: Should we remember if the child was detached? */
    gint n = midori_panel_get_current_page (panel);
    GtkToolItem* toolitem = gtk_toolbar_get_nth_item (
        GTK_TOOLBAR (panel->toolbar), n);
    const gchar* title = gtk_tool_button_get_label (GTK_TOOL_BUTTON (toolitem));
    GtkWidget* toolbar = gtk_notebook_get_nth_page (
        GTK_NOTEBOOK (panel->toolbook), n);
    GtkWidget* scrolled = gtk_notebook_get_nth_page (
        GTK_NOTEBOOK (panel->notebook), n);
    GtkWidget* menuitem = g_object_get_data (G_OBJECT (scrolled), "panel-menuitem");
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
    g_object_set_data (G_OBJECT (window), "scrolled", scrolled);
    gtk_window_set_title (GTK_WINDOW (window), title);
    gtk_window_set_default_size (GTK_WINDOW (window), 250, 400);
    gtk_window_set_transient_for (GTK_WINDOW (window),
        GTK_WINDOW (gtk_widget_get_toplevel (panel->notebook)));
    gtk_widget_show (vbox);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    if (menuitem)
        gtk_widget_hide (menuitem);
    g_signal_handlers_disconnect_by_func (
        _midori_panel_child_for_scrolled (panel, scrolled),
        midori_panel_widget_destroy_cb, toolitem);
    gtk_container_remove (GTK_CONTAINER (panel->toolbar), GTK_WIDGET (toolitem));
    g_object_ref (toolbar);
    gtk_container_remove (GTK_CONTAINER (panel->toolbook), toolbar);
    gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
    g_object_unref (toolbar);
    g_object_set_data (G_OBJECT (scrolled), "panel-toolbar", toolbar);
    g_object_ref (scrolled);
    gtk_container_remove (GTK_CONTAINER (panel->notebook), scrolled);
    gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
    g_object_unref (scrolled);
    midori_panel_set_current_page (panel, n > 0 ? n - 1 : 0);
    toolitem = gtk_toolbar_get_nth_item (GTK_TOOLBAR (panel->toolbar),
                                         n > 0 ? n - 1 : 0);
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (toolitem), TRUE);
    if (!gtk_notebook_get_n_pages (GTK_NOTEBOOK (panel->notebook)))
        gtk_widget_set_sensitive (toolbutton, FALSE);
    g_signal_connect (window, "delete-event",
        G_CALLBACK (midori_panel_detached_window_delete_event_cb), panel);
    gtk_widget_show (window);
}

static void
midori_panel_button_align_clicked_cb (GtkWidget*   toolitem,
                                      MidoriPanel* panel)
{
    midori_panel_set_right_aligned (panel, !panel->right_aligned);
}

static void
midori_panel_destroy_cb (MidoriPanel* panel)
{
    /* Destroy pages first, so they don't need special care */
    gtk_container_foreach (GTK_CONTAINER (panel->notebook),
                           (GtkCallback) gtk_widget_destroy, NULL);
}

static void
midori_panel_init (MidoriPanel* panel)
{
    GtkWidget* vbox;
    GtkWidget* labelbar;
    GtkToolItem* toolitem;

    panel->right_aligned = FALSE;

    /* Create the sidebar */
    panel->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (panel->toolbar), GTK_TOOLBAR_BOTH);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (panel->toolbar),
                               GTK_ICON_SIZE_BUTTON);
    gtk_toolbar_set_orientation (GTK_TOOLBAR (panel->toolbar),
                                 GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (panel), panel->toolbar, FALSE, FALSE, 0);
    gtk_widget_show_all (panel->toolbar);
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (panel), vbox, TRUE, TRUE, 0);

    /* Create the titlebar */
    labelbar = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (labelbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (labelbar), GTK_TOOLBAR_ICONS);
    toolitem = gtk_tool_item_new ();
    gtk_tool_item_set_expand (toolitem, TRUE);
    panel->toolbar_label = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (panel->toolbar_label), 0, 0.5);
    gtk_container_add (GTK_CONTAINER (toolitem), panel->toolbar_label);
    gtk_container_set_border_width (GTK_CONTAINER (toolitem), 6);
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_FULLSCREEN);
    gtk_widget_set_sensitive (GTK_WIDGET (toolitem), FALSE);
    panel->button_detach = toolitem;
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (toolitem),
        _("Detach chosen panel from the window"));
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (midori_panel_button_detach_clicked_cb), panel);
    #if HAVE_OSX
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, 0);
    #else
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    #endif
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (toolitem),
        _("Align sidepanel to the right"));
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (midori_panel_button_align_clicked_cb), panel);
    #if HAVE_OSX
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, 0);
    #else
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    #endif
    panel->button_align = toolitem;
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), _("Close panel"));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (toolitem), _("Close panel"));
    g_signal_connect (toolitem, "clicked",
        G_CALLBACK (midori_panel_button_close_clicked_cb), panel);
    #if HAVE_OSX
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, 0);
    #else
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    #endif
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

    g_signal_connect (panel, "destroy",
                      G_CALLBACK (midori_panel_destroy_cb), NULL);
}

static void
midori_panel_finalize (GObject* object)
{
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
        katze_object_assign (panel->menu, g_value_dup_object (value));
        /* FIXME: Move existing items to the new menu */
        break;
    case PROP_PAGE:
        midori_panel_set_current_page (panel, g_value_get_int (value));
        break;
    case PROP_RIGHT_ALIGNED:
        midori_panel_set_right_aligned (panel, g_value_get_boolean (value));
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
    case PROP_RIGHT_ALIGNED:
        g_value_set_boolean (value, panel->right_aligned);
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

/**
 * midori_panel_set_compact:
 * @compact: %TRUE if the panel should be compact
 *
 * Determines if the panel should be compact.
 **/
void
midori_panel_set_compact (MidoriPanel* panel,
                          gboolean     compact)
{
    g_return_if_fail (MIDORI_IS_PANEL (panel));

    #if HAVE_HILDON
    compact = TRUE;
    #endif
    gtk_toolbar_set_style (GTK_TOOLBAR (panel->toolbar),
        compact ? GTK_TOOLBAR_ICONS : GTK_TOOLBAR_BOTH);
}

/**
 * midori_panel_set_right_aligned:
 * @right_aligned: %TRUE if the panel should be aligned to the right
 *
 * Determines if the panel should be right aligned.
 *
 * Since: 0.1.3
 **/
void
midori_panel_set_right_aligned (MidoriPanel* panel,
                                gboolean     right_aligned)
{
    GtkWidget* box;

    g_return_if_fail (MIDORI_IS_PANEL (panel));

    box = gtk_widget_get_parent (panel->toolbar);
    gtk_box_reorder_child (GTK_BOX (box), panel->toolbar,
        right_aligned ? -1 : 0);
    gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (panel->button_align),
        right_aligned ? GTK_STOCK_GO_BACK : GTK_STOCK_GO_FORWARD);
    panel->right_aligned = right_aligned;
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (panel->button_align),
        !panel->right_aligned ? _("Align sidepanel to the right")
            : _("Align sidepanel to the left"));
    g_object_notify (G_OBJECT (panel), "right-aligned");
}

static void
midori_panel_menu_item_activate_cb (GtkWidget*   widget,
                                    MidoriPanel* panel)
{
    GtkWidget* child;
    GtkToggleToolButton* toolitem;
    guint n;

    child = g_object_get_data (G_OBJECT (widget), "page");
    toolitem = g_object_get_data (G_OBJECT (widget), "toolitem");

    if (toolitem)
    {
        /* Unsetting the button before setting it ensures that
           it will emit signals even if it was active before */
        gtk_toggle_tool_button_set_active (toolitem, FALSE);
        gtk_toggle_tool_button_set_active (toolitem, TRUE);
    }
    else
    {
        n = midori_panel_page_num (panel, child);
        midori_panel_set_current_page (panel, n);
        g_signal_emit (panel, signals[SWITCH_PAGE], 0, n);
        gtk_widget_show (GTK_WIDGET (panel));
    }
}

static void
midori_panel_viewable_destroy_cb (GtkWidget*   viewable,
                                  MidoriPanel* panel)
{
    gint i = gtk_notebook_page_num (GTK_NOTEBOOK (panel->notebook),
                g_object_get_data (G_OBJECT (viewable), "parent"));
    if (i > -1)
        gtk_notebook_remove_page (GTK_NOTEBOOK (panel->notebook), i);
    g_signal_handlers_disconnect_by_func (
        viewable, midori_panel_viewable_destroy_cb, panel);
}

static GtkToolItem*
midori_panel_construct_tool_item (MidoriPanel*    panel,
                                  MidoriViewable* viewable)
{
    const gchar* label = midori_viewable_get_label (viewable);
    const gchar* stock_id = midori_viewable_get_stock_id (viewable);
    GtkToolItem* toolitem;
    GtkWidget* image;

    toolitem = gtk_radio_tool_button_new_from_stock (NULL, stock_id);
    g_object_set (toolitem, "group",
        gtk_toolbar_get_nth_item (GTK_TOOLBAR (panel->toolbar), 0), NULL);
    image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (toolitem), image);
    if (label)
    {
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem), label);
        gtk_widget_set_tooltip_text (GTK_WIDGET (toolitem), label);
    }
    g_object_set_data (G_OBJECT (toolitem), "page", viewable);
    g_signal_connect (toolitem, "clicked",
                      G_CALLBACK (midori_panel_menu_item_activate_cb), panel);
    gtk_widget_show_all (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (panel->toolbar), toolitem, -1);
    g_signal_connect (viewable, "destroy",
                      G_CALLBACK (midori_panel_widget_destroy_cb), toolitem);

    if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (panel->notebook)))
        gtk_widget_set_sensitive (GTK_WIDGET (panel->button_detach), TRUE);

    return toolitem;
}

/**
 * midori_panel_append_page:
 * @panel: a #MidoriPanel
 * @viewable: a viewable widget
 * @toolbar: a toolbar widget, or %NULL
 * @stock_id: a stock ID
 * @label: a string to use as the label
 *
 * Appends a new page to the panel. If @toolbar is specified it will
 * be packed above @viewable.
 *
 * Since 0.1.3 destroying the @viewable implicitly removes
 * the page including the menu and eventual toolbar.
 *
 * In the case of an error, -1 is returned.
 *
 * Return value: the index of the new page, or -1
 **/
gint
midori_panel_append_page (MidoriPanel*    panel,
                          MidoriViewable* viewable)
{
    GtkWidget* scrolled;
    GObjectClass* gobject_class;
    GtkWidget* widget;
    GtkWidget* toolbar;
    const gchar* label;
    const gchar* stock_id;
    GtkToolItem* toolitem;
    GtkWidget* menuitem;
    guint n;

    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);
    g_return_val_if_fail (MIDORI_IS_VIEWABLE (viewable), -1);

    if (GTK_IS_SCROLLED_WINDOW (viewable))
        scrolled = (GtkWidget*)viewable;
    else
    {
        scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        GTK_WIDGET_SET_FLAGS (scrolled, GTK_CAN_FOCUS);
        gtk_widget_show (scrolled);
        gobject_class = G_OBJECT_GET_CLASS (viewable);
        if (GTK_WIDGET_CLASS (gobject_class)->set_scroll_adjustments_signal)
            widget = (GtkWidget*)viewable;
        else
        {
            widget = gtk_viewport_new (NULL, NULL);
            gtk_widget_show (widget);
            gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (viewable));
        }
        gtk_container_add (GTK_CONTAINER (scrolled), widget);
    }
    gtk_container_add (GTK_CONTAINER (panel->notebook), scrolled);

    toolbar = midori_viewable_get_toolbar (viewable);
    gtk_widget_show (toolbar);
    gtk_container_add (GTK_CONTAINER (panel->toolbook), toolbar);
    g_signal_connect (viewable, "destroy",
                      G_CALLBACK (midori_panel_widget_destroy_cb), toolbar);

    n = midori_panel_page_num (panel, scrolled);
    label = midori_viewable_get_label (viewable);
    stock_id = midori_viewable_get_stock_id (viewable);

    toolitem = midori_panel_construct_tool_item (panel, viewable);

    if (panel->menu)
    {
        menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);
        gtk_widget_show (menuitem);
        g_object_set_data (G_OBJECT (menuitem), "page", viewable);
        g_object_set_data (G_OBJECT (menuitem), "toolitem", toolitem);
        g_signal_connect (menuitem, "activate",
                          G_CALLBACK (midori_panel_menu_item_activate_cb),
                          panel);
        gtk_menu_shell_append (GTK_MENU_SHELL (panel->menu), menuitem);
        g_object_set_data (G_OBJECT (scrolled), "panel-menuitem", menuitem);
        g_signal_connect (viewable, "destroy",
                          G_CALLBACK (midori_panel_widget_destroy_cb), menuitem);
    }

    g_object_set_data (G_OBJECT (viewable), "parent", scrolled);
    g_signal_connect (viewable, "destroy",
                      G_CALLBACK (midori_panel_viewable_destroy_cb), panel);

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
    GtkWidget* scrolled;

    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);
    g_return_val_if_fail (GTK_IS_WIDGET (child), -1);

    scrolled = _midori_panel_scrolled_for_child (panel, child);
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
 *
 * Since 0.1.8 the "page" property is notifying changes.
 **/
void
midori_panel_set_current_page (MidoriPanel* panel,
                               gint         n)
{
    GtkWidget* viewable;

    g_return_if_fail (MIDORI_IS_PANEL (panel));

    if ((viewable = midori_panel_get_nth_page (panel, n)))
    {
        const gchar* label;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->toolbook), n);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->notebook), n);
        label = midori_viewable_get_label (MIDORI_VIEWABLE (viewable));
        g_object_set (panel->toolbar_label, "label", label, NULL);
        g_object_notify (G_OBJECT (panel), "page");
    }
}

typedef struct
{
    GtkAlignment parent_instance;

    gchar* label;
    gchar* stock_id;
    GtkWidget* toolbar;
} MidoriDummyViewable;

typedef struct
{
    GtkAlignmentClass parent_class;
}  MidoriDummyViewableClass;

GType midori_dummy_viewable_get_type (void) G_GNUC_CONST;
#define MIDORI_TYPE_DUMMY_VIEWABLE (midori_dummy_viewable_get_type ())
#define MIDORI_DUMMY_VIEWABLE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
    MIDORI_TYPE_DUMMY_VIEWABLE, MidoriDummyViewable))
#define MIDORI_IS_DUMMY_VIEWABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_DUMMY_VIEWABLE))

static void
midori_dummy_viewable_iface_init (MidoriViewableIface* iface);

static void
midori_dummy_viewable_finalize (GObject* object);

G_DEFINE_TYPE_WITH_CODE (MidoriDummyViewable, midori_dummy_viewable,
                         GTK_TYPE_ALIGNMENT,
                         G_IMPLEMENT_INTERFACE (MIDORI_TYPE_VIEWABLE,
                                                midori_dummy_viewable_iface_init));

static void
midori_dummy_viewable_class_init (MidoriDummyViewableClass* class)
{
    GObjectClass* gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = midori_dummy_viewable_finalize;
}

static const gchar*
midori_dummy_viewable_get_label (MidoriViewable* viewable)
{
    return MIDORI_DUMMY_VIEWABLE (viewable)->label;
}

static const gchar*
midori_dummy_viewable_get_stock_id (MidoriViewable* viewable)
{
    return MIDORI_DUMMY_VIEWABLE (viewable)->stock_id;
}

static GtkWidget*
midori_dummy_viewable_get_toolbar (MidoriViewable* viewable)
{
    return MIDORI_DUMMY_VIEWABLE (viewable)->toolbar;
}

static void
midori_dummy_viewable_iface_init (MidoriViewableIface* iface)
{
    iface->get_stock_id = midori_dummy_viewable_get_stock_id;
    iface->get_label = midori_dummy_viewable_get_label;
    iface->get_toolbar = midori_dummy_viewable_get_toolbar;
}

static void
midori_dummy_viewable_init (MidoriDummyViewable* viewable)
{
    viewable->stock_id = NULL;
    viewable->label = NULL;
    viewable->toolbar = NULL;
}

static void
midori_dummy_viewable_finalize (GObject* object)
{
    MidoriDummyViewable* viewable = MIDORI_DUMMY_VIEWABLE (object);

    katze_assign (viewable->stock_id, NULL);
    katze_assign (viewable->label, NULL);

    G_OBJECT_CLASS (midori_dummy_viewable_parent_class)->finalize (object);
}

/* static */ GtkWidget*
midori_dummy_viewable_new (const gchar* stock_id,
                           const gchar* label,
                           GtkWidget*   toolbar)
{
    GtkWidget* viewable = g_object_new (MIDORI_TYPE_DUMMY_VIEWABLE, NULL);

    MIDORI_DUMMY_VIEWABLE (viewable)->stock_id = g_strdup (stock_id);
    MIDORI_DUMMY_VIEWABLE (viewable)->label = g_strdup (label);
    MIDORI_DUMMY_VIEWABLE (viewable)->toolbar = toolbar;

    return viewable;
}

/**
 * midori_panel_append_widget:
 * @panel: a #MidoriPanel
 * @widget: the child widget
 * @stock_id: a stock ID
 * @label: a string to use as the label, or %NULL
 * @toolbar: a toolbar widget, or %NULL
 *
 * Appends an arbitrary widget to the panel by wrapping it
 * in a #MidoriViewable created on the fly.
 *
 * Actually implementing #MidoriViewable instead of using
 * this convenience is recommended.
 *
 * Since 0.1.3 destroying the @widget implicitly removes
 * the page including the menu and eventual toolbar.
 *
 * In the case of an error, -1 is returned.
 *
 * Return value: the index of the new page, or -1
 **/
gint
midori_panel_append_widget (MidoriPanel* panel,
                            GtkWidget*   widget,
                            const gchar* stock_id,
                            const gchar* label,
                            GtkWidget*   toolbar)
{
    GtkWidget* viewable;

    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);
    g_return_val_if_fail (GTK_IS_WIDGET (widget), -1);

    g_return_val_if_fail (stock_id != NULL, -1);
    g_return_val_if_fail (!toolbar || GTK_IS_WIDGET (toolbar), -1);

    viewable = midori_dummy_viewable_new (stock_id, label, toolbar);
    gtk_widget_show (viewable);
    gtk_container_add (GTK_CONTAINER (viewable), widget);
    g_signal_connect (widget, "destroy",
                      G_CALLBACK (midori_panel_widget_destroy_cb), viewable);
    return midori_panel_append_page (panel, MIDORI_VIEWABLE (viewable));
}
