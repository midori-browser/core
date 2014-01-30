/*
 Copyright (C) 2010-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori/midori-history.h"

#include <glib/gi18n-lib.h>
#include <midori/midori-core.h>

static void
midori_history_clear_cb (KatzeArray* array,
                         sqlite3*    db)
{
    GError* error = NULL;
    MidoriHistoryDatabase* database = midori_history_database_new (NULL, &error);
    if (error == NULL)
        midori_history_database_clear (database, 0, &error);
    if (error != NULL)
    {
        g_printerr (_("Failed to clear history: %s\n"), error->message);
        g_error_free (error);
    }
    g_object_unref (database);
}

KatzeArray*
midori_history_new (char** errmsg)
{
    MidoriHistoryDatabase* database;
    GError* error = NULL;
    sqlite3* db;
    KatzeArray* array;

    g_return_val_if_fail (errmsg != NULL, NULL);

    database = midori_history_database_new (NULL, &error);
    if (error != NULL)
    {
        *errmsg = g_strdup (error->message);
        g_error_free (error);
        return NULL;
    }

    db = midori_database_get_db (MIDORI_DATABASE (database));
    g_return_val_if_fail (db != NULL, NULL);

    array = katze_array_new (KATZE_TYPE_ARRAY);
    g_object_set_data (G_OBJECT (array), "db", db);
    g_signal_connect (array, "clear",
                      G_CALLBACK (midori_history_clear_cb), db);
    return array;
}

void
midori_history_on_quit (KatzeArray*        array,
                        MidoriWebSettings* settings)
{
    gint max_history_age = katze_object_get_int (settings, "maximum-history-age");
    GError* error = NULL;
    MidoriHistoryDatabase* database = midori_history_database_new (NULL, &error);
    if (error == NULL)
        midori_history_database_clear (database, max_history_age, &error);
    if (error != NULL)
    {
        /* i18n: Couldn't remove items that are older than n days */
        g_printerr (_("Failed to remove old history items: %s\n"), error->message);
        g_error_free (error);
    }
    g_object_unref (database);
}

