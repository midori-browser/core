/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __BROWSER_H__
#define __BROWSER_H__ 1

#include "global.h"

#include <gtk/gtk.h>

// -- Types

typedef struct _CBrowser
{
    // shared widgets
    GtkWidget* window;
    GtkActionGroup* actiongroup;
    // menus
    GtkWidget* menubar;
    GtkWidget* menu_bookmarks;
    GtkWidget* popup_bookmark;
    GtkWidget* menu_window;
    GtkWidget* popup_webView;
    GtkWidget* popup_element;
    GtkWidget* popup_editable;
    GtkWidget* throbber;
    // navibar
    GtkWidget* navibar;
    GtkWidget* newTab;
    GtkWidget* location_icon;
    GtkWidget* location;
    GtkWidget* webSearch;
    GtkWidget* closedTabs;
    GtkWidget* fullscreen;
    GtkWidget* bookmarkbar;
    // panels
    GtkWidget* panels;
    GtkWidget* panels_notebook;
    GtkWidget* panel_bookmarks;
    GtkWidget* panel_pageholder;
    GtkWidget* webViews;
    // findbox
    GtkWidget* findbox;
    GtkWidget* findbox_text;
    GtkToolItem* findbox_case;
    GtkToolItem* findbox_highlight;
    GtkWidget* statusbar;
    GtkWidget* progress;
    GtkWidget* icon_security;
    GtkWidget* icon_newsfeed;

    // view specific widgets
    GtkWidget* webView_menu;
    GtkWidget* webView_icon;
    GtkWidget* webView_name;
    GtkWidget* webView_close;
    GtkWidget* webView;

    // view specific values
    gboolean hasMenubar;
    gboolean hasToolbar;
    gboolean hasLocation;
    gboolean hasStatusbar;
    gchar* elementUri; // the element the mouse is hovering on
    gint loadedPercent; // -1 means "not loading"
    //UNDEFINED favicon;
    guint security;
    gchar* statusMessage; // message from a webView
    KatzeXbelItem* sessionItem;
} CBrowser;

enum
{
    SEARCH_COL_ICON,
    SEARCH_COL_TEXT,
    SEARCH_COL_N
};

// -- Declarations

void
on_action_window_new_activate(GtkAction*, CBrowser*);

void
on_action_tab_new_activate(GtkAction*, CBrowser*);

void
on_action_open_activate(GtkAction*, CBrowser*);

void
on_action_tab_close_activate(GtkAction*, CBrowser*);

void
on_action_window_close_activate(GtkAction*, CBrowser*);

void
on_action_quit_activate(GtkAction*, CBrowser*);

void
on_action_edit_activate(GtkAction*, CBrowser*);

void
on_action_cut_activate(GtkAction*, CBrowser*);

void
on_action_copy_activate(GtkAction*, CBrowser*);

void
on_action_paste_activate(GtkAction*, CBrowser*);

void
on_action_delete_activate(GtkAction*, CBrowser*);

void
on_action_selectAll_activate(GtkAction*, CBrowser*);

void
on_action_find_activate(GtkAction*, CBrowser*);

void
on_action_find_next_activate(GtkAction*, CBrowser*);

void
on_action_find_previous_activate(GtkAction*, CBrowser*);

void
on_action_preferences_activate(GtkAction*, CBrowser*);

void
on_action_toolbar_navigation_activate(GtkToggleAction*, CBrowser*);

void
on_action_toolbar_bookmarks_activate(GtkToggleAction*, CBrowser*);

void
on_action_panels_activate(GtkToggleAction*, CBrowser*);

void
on_action_toolbar_status_activate(GtkToggleAction*, CBrowser*);

void
on_action_refresh_stop_activate(GtkAction*, CBrowser*);

void
on_action_zoom_in_activate(GtkAction*, CBrowser*);

void
on_action_zoom_out_activate(GtkAction*, CBrowser*);

void
on_action_zoom_normal_activate(GtkAction*, CBrowser*);

void
on_action_source_view_activate(GtkAction*, CBrowser*);

void
on_action_fullscreen_activate(GtkAction*, CBrowser*);

void
on_action_back_activate(GtkAction*, CBrowser*);

void
on_action_forward_activate(GtkAction*, CBrowser*);

void
on_action_home_activate(GtkAction*, CBrowser*);

void
on_action_location_activate(GtkAction*, CBrowser*);

void
on_action_webSearch_activate(GtkAction*, CBrowser*);

void
on_action_openInPanel_activate(GtkAction*, CBrowser*);

void
on_menu_tabsClosed_activate(GtkWidget*, CBrowser*);

void
on_menu_tabsClosed_item_activate(GtkWidget*, CBrowser*);

void
on_action_tabsClosed_clear_activate(GtkAction*, CBrowser*);

void
on_action_tabsClosed_undo_activate(GtkAction*, CBrowser*);

void
on_action_link_tab_new_activate(GtkAction*, CBrowser*);

void
on_action_link_tab_current_activate(GtkAction*, CBrowser*);

void
on_action_link_window_new_activate(GtkAction*, CBrowser*);

void
on_action_link_saveWith_activate(GtkAction*, CBrowser*);

void
on_action_link_copy_activate(GtkAction*, CBrowser*);

void
on_action_bookmarkOpen_activate(GtkAction*, CBrowser*);

void
on_action_bookmarkOpenTab_activate(GtkAction*, CBrowser*);

void
on_action_bookmarkOpenWindow_activate(GtkAction*, CBrowser*);

void
on_action_bookmarkEdit_activate(GtkAction*, CBrowser*);

void
on_action_bookmarkDelete_activate(GtkAction*, CBrowser*);

void
on_menu_bookmarks_item_activate(GtkWidget*, CBrowser*);

void
on_action_bookmark_new_activate(GtkAction*, CBrowser*);

void
on_action_manageSearchEngines_activate(GtkAction*, CBrowser*);

void
on_action_tab_previous_activate(GtkAction*, CBrowser*);

void
on_action_tab_next_activate(GtkAction*, CBrowser*);

void
on_action_about_activate(GtkAction*, CBrowser*);

gboolean
on_location_key_down(GtkWidget*, GdkEventKey*, CBrowser*);

CBrowser*
browser_new(CBrowser*);

// -- Action definitions

// TODO: Fill in a good description for each 'hm?'
static const GtkActionEntry entries[] = {
 { "File", NULL, "_File" },
 { "WindowNew", STOCK_WINDOW_NEW
 , NULL, "<Ctrl>n"
 , "Open a new window", G_CALLBACK(on_action_window_new_activate) },
 { "TabNew", STOCK_TAB_NEW
 , NULL, "<Ctrl>t"
 , "Open a new tab", G_CALLBACK(on_action_tab_new_activate) },
 { "Open", GTK_STOCK_OPEN
 , NULL, "<Ctrl>o"
 , "Open a file", G_CALLBACK(on_action_open_activate) },
 { "SaveAs", GTK_STOCK_SAVE_AS
 , NULL, "<Ctrl>s"
 , "Save to a file", NULL/*G_CALLBACK(on_action_saveas_activate)*/ },
 { "TabClose", STOCK_TAB_CLOSE
 , NULL, "<Ctrl>w"
 , "Close the current tab", G_CALLBACK(on_action_tab_close_activate) },
 { "WindowClose", STOCK_WINDOW_CLOSE
 , NULL, "<Ctrl><Shift>w"
 , "Close this window", G_CALLBACK(on_action_window_close_activate) },
 { "PageSetup", GTK_STOCK_PROPERTIES
 , "Pa_ge Setup", ""
 , "hm?", NULL/*G_CALLBACK(on_action_page_setup_activate)*/ },
 { "PrintPreview", GTK_STOCK_PRINT_PREVIEW
 , NULL, ""
 , "hm?", NULL/*G_CALLBACK(on_action_print_preview_activate)*/ },
 { "Print", GTK_STOCK_PRINT
 , NULL, "<Ctrl>p"
 , "hm?", NULL/*G_CALLBACK(on_action_print_activate)*/ },
 { "Quit", GTK_STOCK_QUIT
 , NULL, "<Ctrl>q"
 , "Quit the application", G_CALLBACK(on_action_quit_activate) },

 { "Edit", NULL, "_Edit", NULL, NULL, G_CALLBACK(on_action_edit_activate) },
 { "Undo", GTK_STOCK_UNDO
 , NULL, "<Ctrl>z"
 , "Undo the last modification", NULL/*G_CALLBACK(on_action_undo_activate)*/ },
 { "Redo", GTK_STOCK_REDO
 , NULL, "<Ctrl><Shift>z"
 , "Redo the last modification", NULL/*G_CALLBACK(on_action_redo_activate)*/ },
 { "Cut", GTK_STOCK_CUT
 , NULL, "<Ctrl>x"
 , "Cut the selected text", G_CALLBACK(on_action_cut_activate) },
 { "Copy", GTK_STOCK_COPY
 , NULL, "<Ctrl>c"
 , "Copy the selected text", G_CALLBACK(on_action_copy_activate) },
 { "Copy_", GTK_STOCK_COPY
 , NULL, "<Ctrl>c"
 , "Copy the selected text", G_CALLBACK(on_action_copy_activate) },
 { "Paste", GTK_STOCK_PASTE
 , NULL, "<Ctrl>v"
 , "Paste text from the clipboard", G_CALLBACK(on_action_paste_activate) },
 { "Delete", GTK_STOCK_DELETE
 , NULL, NULL
 , "Delete the selected text", G_CALLBACK(on_action_delete_activate) },
 { "SelectAll", GTK_STOCK_SELECT_ALL
 , NULL, "<Ctrl>a"
 , "Selected all text", G_CALLBACK(on_action_selectAll_activate) },
 { "FormFill", STOCK_FORM_FILL
 , NULL, ""
 , "hm?", NULL/*G_CALLBACK(on_action_formfill_activate)*/ },
 { "Find", GTK_STOCK_FIND
 , NULL, "<Ctrl>f"
 , "hm?", G_CALLBACK(on_action_find_activate) },
 { "FindNext", GTK_STOCK_GO_FORWARD
 , "Find _Next", "<Ctrl>g"
 , "hm?", G_CALLBACK(on_action_find_next_activate) },
 { "FindPrevious", GTK_STOCK_GO_BACK
 , "Find _Previous", "<Ctrl><Shift>g"
 , "hm?", G_CALLBACK(on_action_find_previous_activate) },
 { "FindQuick", GTK_STOCK_FIND
 , "_Quick Find", "period"
 , "hm?", NULL/*G_CALLBACK(on_action_find_quick_activate)*/ },
 { "ManageSearchEngines", GTK_STOCK_PROPERTIES
 , "_Manage Search Engines", "<Ctrl><Alt>s"
 , "hm?", G_CALLBACK(on_action_manageSearchEngines_activate) }, 
 { "Preferences", GTK_STOCK_PREFERENCES
 , NULL, "<Ctrl><Alt>p"
 , "hm?", G_CALLBACK(on_action_preferences_activate) },

 { "View", NULL, "_View" },
 { "Toolbars", NULL, "_Toolbars" },
 { "Refresh", GTK_STOCK_REFRESH
 , NULL, "<Ctrl>r"
 , "Refresh the current page", G_CALLBACK(on_action_refresh_stop_activate) },
 // TODO: Is appointment-new a good choice?
 // TODO: What if it isn't available?
 { "RefreshEvery", "appointment-new"
 , "Refresh _Every...", ""
 , "Refresh the current page", G_CALLBACK(on_action_refresh_stop_activate) },
 { "Stop", GTK_STOCK_STOP
 , NULL, "Escape"
 , "Stop loading of the current page", G_CALLBACK(on_action_refresh_stop_activate) },
 { "RefreshStop", GTK_STOCK_REFRESH
 , NULL, ""
 , NULL, G_CALLBACK(on_action_refresh_stop_activate) },
 { "ZoomIn", GTK_STOCK_ZOOM_IN
 , NULL, "<Ctrl>plus"
 , "hm?", G_CALLBACK(on_action_zoom_in_activate) },
 { "ZoomOut", GTK_STOCK_ZOOM_OUT
 , NULL, "<Ctrl>minus"
 , "hm?", G_CALLBACK(on_action_zoom_out_activate) },
 { "ZoomNormal", GTK_STOCK_ZOOM_100
 , NULL, "<Ctrl>0"
 , "hm?", G_CALLBACK(on_action_zoom_normal_activate) },
 { "BackgroundImage", STOCK_IMAGE
 , "_Background Image", ""
 , "hm?", NULL/*G_CALLBACK(on_action_background_image_activate)*/ },
 { "SourceView", STOCK_SOURCE_VIEW
 , NULL, ""
 , "hm?", /*G_CALLBACK(on_action_source_view_activate)*/ },
 { "SelectionSourceView", STOCK_SOURCE_VIEW
 , "View Selection Source", ""
 , "hm?", NULL/*G_CALLBACK(on_action_selection_source_view_activate)*/ },
 { "Properties", GTK_STOCK_PROPERTIES
 , NULL, ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_activate)*/ },
 { "Fullscreen", GTK_STOCK_FULLSCREEN
 , NULL, "F11"
 , "Toggle fullscreen view", G_CALLBACK(on_action_fullscreen_activate) },

 { "Go", NULL, "_Go" },
 { "Back", GTK_STOCK_GO_BACK
 , NULL, "<Alt>Left"
 , "hm?", G_CALLBACK(on_action_back_activate) },
 { "Forward", GTK_STOCK_GO_FORWARD
 , NULL, "<Alt>Right"
 , "hm?", G_CALLBACK(on_action_forward_activate) },
 { "Home", STOCK_HOMEPAGE
 , NULL, "<Alt>Home"
 , "hm?", G_CALLBACK(on_action_home_activate) },
 { "Location", GTK_STOCK_JUMP_TO
 , "Location...", "<Ctrl>l"
 , "hm?", G_CALLBACK(on_action_location_activate) },
 { "Websearch", GTK_STOCK_FIND
 , "Websearch...", "<Ctrl><Shift>f"
 , "hm?", G_CALLBACK(on_action_webSearch_activate) },
 { "OpenInPageholder", GTK_STOCK_JUMP_TO
 , "Open in Page_holder...", ""
 , "hm?", G_CALLBACK(on_action_openInPanel_activate) },
 { "TabsClosed", STOCK_USER_TRASH
 , "Closed Tabs", ""
 , "hm?", NULL },
 { "TabsClosedClear", GTK_STOCK_CLEAR
 , "Clear List of Closed Tabs", ""
 , "hm?", G_CALLBACK(on_action_tabsClosed_clear_activate) },
 { "UndoTabClose", GTK_STOCK_UNDELETE
 , "Undo Close Tab", ""
 , "hm?", G_CALLBACK(on_action_tabsClosed_undo_activate) },
 { "LinkTabNew", STOCK_TAB_NEW
 , "Open Link in New Tab", ""
 , "hm?", G_CALLBACK(on_action_link_tab_new_activate) },
 { "LinkTabCurrent", NULL
 , "Open Link in Current Tab", ""
 , "hm?", G_CALLBACK(on_action_link_tab_current_activate) },
 { "LinkWindowNew", STOCK_WINDOW_NEW
 , "Open Link in New Window", ""
 , "hm?", G_CALLBACK(on_action_link_window_new_activate) },
 { "LinkBookmarkNew", STOCK_BOOKMARK_NEW
 , NULL, ""
 , "Bookmark this link", NULL/*G_CALLBACK(on_action_link_bookmark_activate)*/ },
 { "LinkSaveAs", GTK_STOCK_SAVE
 , "Save Destination as...", ""
 , "Save destination to a file", NULL/*G_CALLBACK(on_action_link_saveas_activate)*/ },
 { "LinkSaveWith", STOCK_DOWNLOADS
 , "Download Destination", ""
 , "Save destination with the chosen download manager", G_CALLBACK(on_action_link_saveWith_activate) },
 { "LinkCopy", GTK_STOCK_COPY
 , "Copy Link Address", ""
 , "Copy the link address to the clipboard", G_CALLBACK(on_action_link_copy_activate) },
 { "SelectionLinksNewTabs", NULL
 , "Open Selected Links in Tabs", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "SelectionTextTabNew", STOCK_TAB_NEW
 , "Open <Selection> in New Tab", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "SelectionTextTabCurrent", NULL
 , "Open <Selection> in Current Tab", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "SelectionTextWindowNew", STOCK_WINDOW_NEW
 , "Open <Selection> in New Qindow", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "SelectionSearch", GTK_STOCK_FIND
 , "Search for <Selection>", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "SelectionSearchWith", GTK_STOCK_FIND
 , "Search for <Selection> with...", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "ImageViewTabNew", STOCK_TAB_NEW
 , "View Image in New Tab", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "ImageViewTabCurrent", NULL
 , "View image in current tab", ""
 , "hm?", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "ImageSaveAs", GTK_STOCK_SAVE
 , "Save Image as...", ""
 , "Save image to a file", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "ImageSaveWith", STOCK_DOWNLOADS
 , "Download Image", ""
 , "Save image with the chosen download manager", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },
 { "ImageCopy", GTK_STOCK_COPY
 , "Copy Image Address", ""
 , "Copy the image address to the clipboard", NULL/*G_CALLBACK(on_action_properties_selection_activate)*/ },

 { "Bookmarks", NULL, "_Bookmarks" },
 { "BookmarkNew", STOCK_BOOKMARK_NEW
 , NULL, "<Ctrl>d"
 , "hm?", G_CALLBACK(on_action_bookmark_new_activate) },
 { "BookmarksManage", STOCK_BOOKMARKS
 , "_Manage Bookmarks", "<Ctrl>b"
 , "hm?", NULL/*G_CALLBACK(on_action_bookmarks_manage_activate)*/ },
 { "BookmarkOpen", GTK_STOCK_OPEN
 , NULL, ""
 , "hm?", G_CALLBACK(on_action_bookmarkOpen_activate) },
 { "BookmarkOpenTab", STOCK_TAB_NEW
 , "Open in New _Tab", ""
 , "hm?", G_CALLBACK(on_action_bookmarkOpenTab_activate) },
 { "BookmarkOpenWindow", STOCK_WINDOW_NEW
 , "Open in New _Window", ""
 , "hm?", G_CALLBACK(on_action_bookmarkOpenWindow_activate) },
 { "BookmarkEdit", GTK_STOCK_EDIT
 , NULL, ""
 , "hm?", G_CALLBACK(on_action_bookmarkEdit_activate) },
 { "BookmarkDelete", GTK_STOCK_DELETE
 , NULL, ""
 , "hm?", G_CALLBACK(on_action_bookmarkDelete_activate) },

 { "Tools", NULL, "_Tools" },

 { "Window", NULL, "_Window" },
 { "SessionLoad", GTK_STOCK_REVERT_TO_SAVED
 , "_Load Session", ""
 , "hm?", NULL/*G_CALLBACK(on_action_session_load_activate)*/ },
 { "SessionSave", GTK_STOCK_SAVE_AS
 , "_Save Session", ""
 , "hm?", NULL/*G_CALLBACK(on_action_session_save_activate)*/ },
 { "TabPrevious", GTK_STOCK_GO_BACK
 , "_Previous Tab", "<Ctrl>Page_Up"
 , "hm?", G_CALLBACK(on_action_tab_previous_activate) },
 { "TabNext", GTK_STOCK_GO_FORWARD
 , "_Next Tab", "<Ctrl>Page_Down"
 , "hm?", G_CALLBACK(on_action_tab_next_activate) },
 { "TabOverview", NULL
 , "Tab _Overview", ""
 , "hm?", NULL/*G_CALLBACK(on_action_tab_overview_activate)*/ },

 { "Help", NULL, "_Help" },
 { "HelpContents", GTK_STOCK_HELP
 , "_Contents", "F1"
 , "hm?", NULL/*G_CALLBACK(on_action_help_contents_activate)*/ },
 { "About", GTK_STOCK_ABOUT
 , NULL, ""
 , "hm?", G_CALLBACK(on_action_about_activate) },
 };
 static const guint entries_n = G_N_ELEMENTS(entries);

static const GtkToggleActionEntry toggle_entries[] = {
 { "PrivateBrowsing", NULL
 , "P_rivate Browsing", ""
 , "hm?", NULL/*G_CALLBACK(on_action_private_browsing_activate)*/
 , FALSE },
 { "WorkOffline", GTK_STOCK_DISCONNECT
 , "_Work Offline", ""
 , "hm?", NULL/*G_CALLBACK(on_action_work_offline_activate)*/
 , FALSE },

 { "ToolbarNavigation", NULL
 , "_Navigationbar", ""
 , "hm?", G_CALLBACK(on_action_toolbar_navigation_activate)
 , FALSE },
 { "Panels", NULL
 , "_Panels", "F9"
 , "hm?", G_CALLBACK(on_action_panels_activate)
 , FALSE },
 { "ToolbarBookmarks", NULL
 , "_Bookmarkbar", ""
 , "hm?", G_CALLBACK(on_action_toolbar_bookmarks_activate)
 , FALSE },
 { "ToolbarDownloads", NULL
 , "_Downloadbar", ""
 , "hm?", NULL/*G_CALLBACK(on_action_toolbar_downloads_activate)*/
 , FALSE },
 { "ToolbarStatus", NULL
 , "_Statusbar", ""
 , "hm?", G_CALLBACK(on_action_toolbar_status_activate)
 , FALSE },
 { "RefreshEveryEnable", NULL
 , "_Enabled", ""
 , "hm?", NULL/*G_CALLBACK(on_action_reloadevery_enable_activate)*/
 , FALSE },
 { "ReloadEveryActive", NULL
 , "_Active", ""
 , "hm?", NULL/*G_CALLBACK(on_action_reloadevery_active_activate)*/
 , FALSE },
 };
 static const guint toggle_entries_n = G_N_ELEMENTS(toggle_entries);

static const GtkRadioActionEntry refreshevery_entries[] = {
 { "RefreshEvery30", NULL
 , "30 seconds", ""
 , "Refresh Every _30 Seconds", 30 },
 { "RefreshEvery60", NULL
 , "60 seconds", ""
 , "Refresh Every _60 Seconds", 60 },
 { "RefreshEvery300", NULL
 , "5 minutes", ""
 , "Refresh Every _5 Minutes", 300 },
 { "RefreshEvery900", NULL
 , "15 minutes", ""
 , "Refresh Every _15 Minutes", 900 },
 { "RefreshEvery1800", NULL
 , "30 minutes", ""
 , "Refresh Every 3_0 Minutes", 1800 },
 { "RefreshEveryCustom", NULL
 , "Custom...", ""
 , "Refresh by a _Custom Period", 0 },
 };
 static const guint refreshevery_entries_n = G_N_ELEMENTS(refreshevery_entries);

static const GtkRadioActionEntry panel_entries[] = {
 { "PanelDownloads", STOCK_DOWNLOADS
 , NULL, ""
 , "hm?", 0 },
 { "PanelBookmarks", STOCK_BOOKMARKS
 , "_Bookmarks", ""
 , "hm?", 1 },
 { "PanelConsole", STOCK_CONSOLE
 , NULL, ""
 , "hm?", 2 },
 { "PanelExtensions", STOCK_EXTENSIONS
 , NULL, ""
 , "hm?", 3 },
 { "PanelHistory", STOCK_HISTORY
 , "_History", ""
 , "hm?", 4 },
 // TODO: We want a better icon here, but which one?
 { "PanelTabs", STOCK_TAB_NEW
 , "_Tabs", ""
 , "hm?", 5 },
 // TODO: We probably want another icon here
 { "PanelPageholder", GTK_STOCK_CONVERT
 , "_Pageholder", ""
 , "hm?", 6 },
 };
 static const guint panel_entries_n = G_N_ELEMENTS(panel_entries);

#endif /* !__BROWSER_H__ */
