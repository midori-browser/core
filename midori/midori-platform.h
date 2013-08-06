/*
 Copyright (C) 2010-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_PLATFORM_H__
#define __MIDORI_PLATFORM_H__ 1

#include "midori/midori-stock.h"
#include "katze/gtk3-compat.h"
#include "midori/sokoke.h"

/* Common behavior modifiers */
#define MIDORI_MOD_NEW_WINDOW(state) (state & GDK_SHIFT_MASK)
#define MIDORI_MOD_NEW_TAB(state) (state & GDK_CONTROL_MASK)
#define MIDORI_MOD_BACKGROUND(state) (state & GDK_SHIFT_MASK)
#define MIDORI_MOD_SCROLL(state) (state & GDK_CONTROL_MASK)

#ifdef GDK_WINDOWING_QUARTZ
    #define MIDORI_EVENT_CONTEXT_MENU(evt) \
        ((evt && evt->button == 3) \
        || (evt && evt->button == 1 && (evt->state & GDK_CONTROL_MASK)))
#else
    #define MIDORI_EVENT_CONTEXT_MENU(evt) \
        (evt && evt->button == 3)
#endif

#define MIDORI_EVENT_NEW_TAB(evt) \
    (evt != NULL \
     && ((((GdkEventButton*)evt)->button == 1 \
       && MIDORI_MOD_NEW_TAB(((GdkEventButton*)evt)->state)) \
     || (((GdkEventButton*)evt)->button == 2)))

#endif /* !__MIDORI_PLATFORM_H__ */
