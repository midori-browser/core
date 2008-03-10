/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __PREFS_H__
#define __PREFS_H__ 1

#include "midori-browser.h"

#include <gtk/gtk.h>

// -- Types

typedef struct
{
    MidoriBrowser* browser;
    GtkWidget* userStylesheetUri;
    GtkWidget* treeview;
    GtkWidget* combobox;
    GtkWidget* add;
} CPrefs;

enum
{
    PROTOCOLS_COL_NAME,
    PROTOCOLS_COL_COMMAND,
    PROTOCOLS_COL_N
};

// -- Declarations

GtkWidget*
prefs_preferences_dialog_new(MidoriBrowser*);

#endif /* !__PREFS_H__ */
