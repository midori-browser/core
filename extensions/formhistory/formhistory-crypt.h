/*
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/

#ifndef __FORMHISTORY_CRYPT_H__
#define __FORMHISTORY_CRYPT_H__

typedef struct
{
    sqlite3* db;
    gchar* domain;
    gchar* form_data;
} FormhistoryPasswordEntry;

gchar*
formhistory_encrypt (const gchar* input, unsigned char* key);

gchar*
formhistory_decrypt (const gchar* b64input, unsigned char* key);

#endif
