/*
 Copyright (C) 2007-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori-panel.h"

#include "midori-browser.h"
#include "midori-platform.h"
#include "midori-view.h"

#include "marshal.h"

#include <glib/gi18n.h>

#include "config.h"

struct _MidoriPanel
{
    GtkHBox parent_instance;

    GtkWidget* labelbar;
    GtkWidget* toolbar;
    GtkToolItem* button_align;
    GtkWidget* toolbar_label;
    GtkWidget* frame;
    GtkWidget* toolbook;
    GtkWidget* notebook;
    GtkActionGroup* action_group;

    gboolean show_titles;
    gboolean show_controls;
    gboolean right_aligned;
    gboolean open_panels_in_windows;
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
    PROP_ACTION_GROUP,
    PROP_PAGE,
    PROP_SHOW_TITLES,
    PROP_SHOW_CONTROLS,
    PROP_RIGHT_ALIGNED,
    PROP_OPEN_PANELS_IN_WINDOWS,
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

    /**
     * MidoriWebSettings:action-group:
     *
     * This is the action group the panel will add actions
     * corresponding to pages to.
     *
     * Since: 0.2.1
     */
    g_object_class_install_property (gobject_class,
                                     PROP_ACTION_GROUP,
                                     g_param_spec_object (
                                     "action-group",
                                     "Action Group",
                                     "The action group the panel will add actions to",
                                     GTK_TYPE_ACTION_GROUP,
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
     * MidoriWebSettings:show-titles:
     *
     * Whether to show panel titles.
     *
     * Deprecated: 0.2.3
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_TITLES,
                                     g_param_spec_boolean (
                                     "show-titles",
                                     "Show Titles",
                                     "Whether to show panel titles",
                                     TRUE,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * MidoriWebSettings:show-controls:
     *
     * Whether to show operating controls.
     *
     * Since: 0.1.9
     *
     * Deprecated: 0.3.0
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_CONTROLS,
                                     g_param_spec_boolean (
                                     "show-controls",
                                     "Show Controls",
                                     "Whether to show operating controls",
                                     TRUE,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

    /**
     * MidoriWebSettings:open-panels-in-windows:
     *
     * Whether to open panels in separate windows.
     *
     * Since: 0.2.2
     */
    g_object_class_install_property (gobject_class,
                                     PROP_OPEN_PANELS_IN_WINDOWS,
                                     g_param_spec_boolean (
                                     "open-panels-in-windows",
                                     "Open panels in windows",
        "Whether to open panels in standalone windows by default",
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

static void
midori_panel_widget_destroy_cb (GtkWidget* viewable,
                                GtkWidget* widget)
{
    gtk_widget_destroy (widget);
    g_signal_handlers_disconnect_by_func (
        viewable, midori_panel_widget_destroy_cb, widget);
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

    panel->action_group = NULL;
    panel->show_titles = TRUE;
    panel->show_controls = TRUE;
    panel->right_aligned = FALSE;

    /* Create the sidebar */
    panel->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (panel->toolbar), GTK_ICON_SIZE_BUTTON);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (panel->toolbar), FALSE);
    gtk_widget_show_all (panel->toolbar);
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (panel), vbox, TRUE, TRUE, 0);
    gtk_box_pack_end (GTK_BOX (vbox), panel->toolbar, FALSE, FALSE, 0);

    /* Create the titlebar */
    labelbar = gtk_toolbar_new ();
    katze_widget_add_class (labelbar, "secondary-toolbar");
    panel->labelbar = labelbar;
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (labelbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (labelbar), GTK_TOOLBAR_ICONS);
    toolitem = gtk_tool_item_new ();
    gtk_tool_item_set_expand (toolitem, TRUE);
    panel->toolbar_label = gtk_label_new (NULL);
    gtk_label_set_ellipsize (GTK_LABEL (panel->toolbar_label), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment (GTK_MISC (panel->toolbar_label), 0, 0.5);
    gtk_container_add (GTK_CONTAINER (toolitem), panel->toolbar_label);
    gtk_container_set_border_width (GTK_CONTAINER (toolitem), 6);
    gtk_toolbar_insert (GTK_TOOLBAR (labelbar), toolitem, -1);
    toolitem = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (toolitem),
        _("Align sidepanel to the right"));
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

    /* Create the notebook */
    panel->notebook = gtk_notebook_new ();
    katze_widget_add_class (panel->notebook, "content-view");
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
    case PROP_ACTION_GROUP:
        katze_object_assign (panel->action_group, g_value_dup_object (value));
        break;
    case PROP_PAGE:
        midori_panel_set_current_page (panel, g_value_get_int (value));
        break;
    case PROP_SHOW_TITLES:
        panel->show_titles = g_value_get_boolean (value);
        /* Ignore */
        break;
    case PROP_SHOW_CONTROLS:
        panel->show_controls = g_value_get_boolean (value);
        break;
    case PROP_RIGHT_ALIGNED:
        midori_panel_set_right_aligned (panel, g_value_get_boolean (value));
        break;
    case PROP_OPEN_PANELS_IN_WINDOWS:
        panel->open_panels_in_windows = g_value_get_boolean (value);
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
    case PROP_ACTION_GROUP:
        g_value_set_object (value, panel->action_group);
        break;
    case PROP_PAGE:
        g_value_set_int (value, midori_panel_get_current_page (panel));
        break;
    case PROP_SHOW_TITLES:
        g_value_set_boolean (value, panel->show_titles);
        break;
    case PROP_SHOW_CONTROLS:
        g_value_set_boolean (value, panel->show_controls);
        break;
    case PROP_RIGHT_ALIGNED:
        g_value_set_boolean (value, panel->right_aligned);
        break;
    case PROP_OPEN_PANELS_IN_WINDOWS:
        g_value_set_boolean (value, panel->open_panels_in_windows);
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
 * Return value: (transfer full): a new #MidoriPanel
 **/
GtkWidget*
midori_panel_new (void)
{
    MidoriPanel* panel = g_object_new (MIDORI_TYPE_PANEL,
                                       NULL);

    return GTK_WIDGET (panel);
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
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (panel->button_align),
        !panel->right_aligned ? _("Align sidepanel to the right")
            : _("Align sidepanel to the left"));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (panel->button_align),
        !panel->right_aligned ? _("Align sidepanel to the right")
            : _("Align sidepanel to the left"));
    g_object_notify (G_OBJECT (panel), "right-aligned");
}

static void
midori_panel_viewable_destroy_cb (GtkWidget*   viewable,
                                  MidoriPanel* panel)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
    gint n_pages;
    gchar* action_name;
    GtkAction* action;
    gint i;

    i = gtk_notebook_page_num (GTK_NOTEBOOK (panel->notebook),
                g_object_get_data (G_OBJECT (viewable), "parent"));
    if (i > -1)
        gtk_notebook_remove_page (GTK_NOTEBOOK (panel->notebook), i);
    g_signal_handlers_disconnect_by_func (
        viewable, midori_panel_viewable_destroy_cb, panel);

    n_pages = midori_panel_get_n_pages (panel);
    if (n_pages > 0 && browser && !g_object_get_data (G_OBJECT (browser), "midori-browser-destroyed"))
        midori_panel_set_current_page (panel, (n_pages-1 > i) ? i : n_pages - 1);

    action_name = g_strconcat ("PanelPage",
        midori_viewable_get_stock_id (MIDORI_VIEWABLE (viewable)), NULL);
    action = gtk_action_group_get_action (panel->action_group, action_name);
    g_free (action_name);

    gtk_action_group_remove_action (panel->action_group, action);
    g_object_unref (G_OBJECT (action));
}

static GtkToolItem*
midori_panel_construct_tool_item (MidoriPanel*    panel,
                                  MidoriViewable* viewable)
{
    GtkAction* action;
    GtkWidget* toolitem;

    action = g_object_get_data (G_OBJECT (viewable), "midori-panel-action");
    toolitem = gtk_action_create_tool_item (action);
    g_object_set_data (G_OBJECT (toolitem), "page", viewable);
    gtk_toolbar_insert (GTK_TOOLBAR (panel->toolbar), GTK_TOOL_ITEM (toolitem), -1);
    g_signal_connect (viewable, "destroy",
                      G_CALLBACK (midori_panel_widget_destroy_cb), toolitem);

    return GTK_TOOL_ITEM (toolitem);
}

static void
midori_panel_action_activate_cb (GtkRadioAction* action,
                                 MidoriPanel*    panel)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (panel));
    GtkActionGroup* actions = midori_browser_get_action_group (browser);
    GtkAction* panel_action = gtk_action_group_get_action (actions, "Panel");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (panel_action), TRUE);
    GtkWidget* viewable = g_object_get_data (G_OBJECT (action), "viewable");
    gint n = midori_panel_page_num (panel, viewable);

    midori_panel_set_current_page (panel, n);
    g_signal_emit (panel, signals[SWITCH_PAGE], 0, n);
    gtk_widget_show (GTK_WIDGET (panel));
}

/**
 * midori_panel_append_page:
 * @panel: a #MidoriPanel
 * @viewable: a viewable widget
 *
 * Appends a new page to the panel. If @toolbar is specified it will
 * be packed above @viewable.
 *
 * Since 0.1.3 destroying the @viewable implicitly removes
 * the page including the menu and eventual toolbar.
 *
 * Since 0.2.1 a hidden @viewable will not be shown in the panel.
 *
 * Since 0.2.1 an action with an accelerator is created implicitly.
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
    #if !GTK_CHECK_VERSION (3, 0, 0)
    GObjectClass* gobject_class;
    #endif
    GtkWidget* widget;
    GtkWidget* toolbar;
    GtkToolItem* toolitem;
    guint n;
    gchar* action_name;
    GtkAction* action;

    g_return_val_if_fail (MIDORI_IS_PANEL (panel), -1);
    g_return_val_if_fail (MIDORI_IS_VIEWABLE (viewable), -1);

    #if GTK_CHECK_VERSION (3, 2, 0)
    if (GTK_IS_ORIENTABLE (viewable))
        gtk_orientable_set_orientation (GTK_ORIENTABLE (viewable),
                                        GTK_ORIENTATION_VERTICAL);
    #endif

    if (GTK_IS_SCROLLED_WINDOW (viewable))
        scrolled = (GtkWidget*)viewable;
    else
    {
        scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_widget_set_can_focus (scrolled, TRUE);
        gtk_widget_show (scrolled);
        #if GTK_CHECK_VERSION (3, 0, 0)
        if (GTK_IS_SCROLLABLE (viewable))
        #else
        gobject_class = G_OBJECT_GET_CLASS (viewable);
        if (GTK_WIDGET_CLASS (gobject_class)->set_scroll_adjustments_signal)
        #endif
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
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), FALSE);
    gtk_widget_show (toolbar);
    gtk_container_add (GTK_CONTAINER (panel->toolbook), toolbar);
    g_signal_connect (viewable, "destroy",
                      G_CALLBACK (midori_panel_widget_destroy_cb), toolbar);

    n = midori_panel_get_n_pages (panel) - 1;
    /* FIXME: Use something better than the stock ID */
    action_name = g_strconcat ("PanelPage",
        midori_viewable_get_stock_id (viewable), NULL);
    action = (GtkAction*)gtk_radio_action_new (action_name,
        midori_viewable_get_label (viewable),
        midori_viewable_get_label (viewable),
        midori_viewable_get_stock_id (viewable), n);
    g_object_set_data (G_OBJECT (action), "viewable", viewable);
    g_signal_connect (action, "activate",
        G_CALLBACK (midori_panel_action_activate_cb), panel);
    if (panel->action_group)
    {
        GtkWidget* toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        GSList* groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
        gtk_action_set_accel_group (action, g_slist_nth_data (groups, 0));
        gtk_action_group_add_action_with_accel (panel->action_group,
                                                action, NULL);
        gtk_action_connect_accelerator (action);
    }
    if (n > 0)
        g_object_set (action, "group", g_object_get_data (
            G_OBJECT (midori_panel_get_nth_page (panel, 0)),
            "midori-panel-action"), NULL);
    g_object_set_data (G_OBJECT (viewable), "midori-panel-action", action);
    g_free (action_name);

    g_object_set_data (G_OBJECT (viewable), "parent", scrolled);
    toolitem = midori_panel_construct_tool_item (panel, viewable);
    g_signal_connect (viewable, "destroy",
                      G_CALLBACK (midori_panel_viewable_destroy_cb), panel);

    if (!gtk_widget_get_visible (GTK_WIDGET (viewable)))
    {
        gtk_widget_hide (scrolled);
        gtk_widget_hide (GTK_WIDGET (toolitem));
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
 * Return value: (transfer none): the child widget of the new page, or %NULL
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
 *
 * Since 0.2.1 switching to hidden pages fails silently.
 **/
void
midori_panel_set_current_page (MidoriPanel* panel,
                               gint         n)
{
    GtkWidget* viewable;

    g_return_if_fail (MIDORI_IS_PANEL (panel));

    if ((viewable = midori_panel_get_nth_page (panel, n)))
    {
        GtkWidget* toolbar;
        GList* items;
        const gchar* label;

        if (!gtk_widget_get_visible (viewable))
            return;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (panel->toolbook), n);
        toolbar = gtk_notebook_get_nth_page (GTK_NOTEBOOK (panel->toolbook), n);
        items = gtk_container_get_children (GTK_CONTAINER (toolbar));
        sokoke_widget_set_visible (panel->toolbook, items != NULL);
        g_list_free (items);
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

/* Private function, used by MidoriBrowser */
void
midori_panel_set_toolbar_style (MidoriPanel*    panel,
                                GtkToolbarStyle style)
{
    g_return_if_fail (MIDORI_IS_PANEL (panel));

    gtk_toolbar_set_style (GTK_TOOLBAR (panel->toolbar), style);
}

