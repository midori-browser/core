#!/bin/sh

# Copyright (C) 2010-2011 Peter de Ridder <peter@xfce.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# See the file COPYING for the full license text.

export MINGW_PREFIX=~/dev/mingw/mingw32
export PATH=$MINGW_PREFIX/bin:$PATH
export PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig
export PKG_CONFIG_LIBDIR=
export MINGW_BUILD=`gcc -dumpmachine`
export MINGW_TARGET=i386-mingw32

find_compiler ()
{
    export CC=$( find /usr/bin -iname *mingw32*gcc )
}

find_compiler
if [ "$CC" != "" ]; then
    CC=${CC} ./configure --prefix=$MINGW_PREFIX $@
else
    echo "Cannot find cross-complier! Please install mingw version of gcc compiler."
fi
