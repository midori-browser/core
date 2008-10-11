#!/bin/sh
# This file is licensed under the terms of the expat license, see the file EXPAT.

echo "no" | glib-gettextize --force --copy
intltoolize --copy --force --automake
libtoolize --copy --force || glibtoolize --copy --force
aclocal
autoheader
autoconf
automake --add-missing --copy

echo "Now running the configure script"
./configure $*
