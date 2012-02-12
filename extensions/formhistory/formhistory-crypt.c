/*
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>
 Copyright (C) 2009-2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
*/
#include <string.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <glib.h>
#define BUF_SIZE 256

typedef unsigned char uc;

const char *iv ="12345678";

gchar*
formhistory_encrypt (const gchar*   input,
                     const gchar*   key)
{
    int outlen=0, auxlen=0;
    EVP_CIPHER_CTX ctx;
    size_t inputSize;
    char buff[BUF_SIZE];

    if (!input)
        return NULL;

    inputSize = strlen (input);
    memset (&buff[0], '\0', BUF_SIZE);

    EVP_CIPHER_CTX_init (&ctx);
    EVP_EncryptInit (&ctx, EVP_bf_cbc (), (unsigned char*)key, (unsigned char*)iv);

    if (EVP_EncryptUpdate (&ctx, (uc*)&buff, &outlen, (uc*)input, inputSize) != 1)
        return NULL;
    if (EVP_EncryptFinal (&ctx, (uc*)&buff + outlen, &auxlen) != 1)
        return NULL;

    outlen += auxlen;
    EVP_CIPHER_CTX_cleanup (&ctx);
    return g_base64_encode ((const guchar*)&buff, outlen);
}

gchar*
formhistory_decrypt (const gchar*   b64input,
                     const gchar*   key)
{
    int outlen=0, auxlen=0;
    EVP_CIPHER_CTX ctx;
    char buff[BUF_SIZE];
    guchar* input;
    size_t inputSize;

    if (!b64input)
        return NULL;

    input = g_base64_decode (b64input, &inputSize);
    memset (&buff, 0, BUF_SIZE);

    EVP_CIPHER_CTX_init (& ctx);
    EVP_DecryptInit (& ctx, EVP_bf_cbc(), (unsigned char*)key, (uc*)iv);

    if (EVP_DecryptUpdate (& ctx, (uc*)&buff, &outlen, (uc*)input, inputSize) != 1)
        return NULL;
    if (EVP_DecryptFinal (& ctx, (uc*)&buff + outlen, &auxlen) != 1)
        return NULL;

    outlen += auxlen;
    g_free (input);
    EVP_CIPHER_CTX_cleanup (&ctx);
    return g_strndup (buff, outlen);
}
