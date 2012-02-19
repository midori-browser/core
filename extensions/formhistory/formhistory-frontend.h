/*
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/

#ifndef __FORMHISTORY_FRONTEND_H__
#define __FORMHISTORY_FRONTEND_H__
#include <midori/midori.h>
#include <glib/gstdio.h>

#include "config.h"
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 1)
    #define FORMHISTORY_USE_GDOM 1
#else
    #define FORMHISTORY_USE_JS 1
#endif
#define MAXPASSSIZE 64

typedef struct
{
    sqlite3* db;
    #ifdef FORMHISTORY_USE_GDOM
    WebKitDOMElement* element;
    int completion_timeout;
    GtkTreeModel* completion_model;
    GtkWidget* treeview;
    GtkWidget* popup;
    gchar* oldkeyword;
    glong selection_index;
    #else
    gchar* jsforms;
    #endif
    gchar* master_password;
    int master_password_canceled;
} FormHistoryPriv;

typedef struct
{
    gchar* domain;
    gchar* form_data;
    FormHistoryPriv* priv;
} FormhistoryPasswordEntry;

FormHistoryPriv*
formhistory_private_new ();

void
formhistory_private_destroy (FormHistoryPriv *priv);

gboolean
formhistory_construct_popup_gui (FormHistoryPriv* priv);

void
formhistory_setup_suggestions (WebKitWebView*   web_view,
                               JSContextRef     js_context,
                               MidoriExtension* extension);

#ifdef FORMHISTORY_USE_GDOM
void
formhistory_suggestions_hide_cb (WebKitDOMElement* element,
                                 WebKitDOMEvent*   dom_event,
                                 FormHistoryPriv*  priv);
#endif

#endif
