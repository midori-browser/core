/*
 Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __DEBUG_H__
#define __DEBUG_H__ 1

#include "config.h"

#include <glib/gprintf.h>

#if SOKOKE_DEBUG > 1
    #define UNIMPLEMENTED g_print(" * Unimplemented: %s\n", G_STRFUNC);
#else
    #define UNIMPLEMENTED ;
#endif

#endif /* !__DEBUG_H__ */
