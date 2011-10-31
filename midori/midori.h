/*
 Copyright (C) 2008-2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_H__
#define __MIDORI_H__

#include "midori-app.h"
#include "midori-array.h"
#include "midori-browser.h"
#include "midori-extension.h"
#include "midori-locationaction.h"
#include "midori-panel.h"
#include "midori-preferences.h"
#include "midori-searchaction.h"
#include "midori-view.h"
#include "midori-viewable.h"
#include "midori-websettings.h"
#include "midori-platform.h"
#include <midori/midori-core.h> /* Vala API */

/* For convenience, include localization header */
#include <glib/gi18n-lib.h>

#define MIDORI_CHECK_VERSION(major, minor, micro) \
  (MIDORI_MAJOR_VERSION > (major) || \
  (MIDORI_MAJOR_VERSION == (major) && MIDORI_MINOR_VERSION > (minor)) || \
  (MIDORI_MAJOR_VERSION == (major) && MIDORI_MINOR_VERSION == (minor) && \
  MIDORI_MICRO_VERSION >= (micro)))

#endif /* __MIDORI_H__ */
