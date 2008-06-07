#!/bin/sh

echo "no" | glib-gettextize --force --copy
intltoolize --copy --force --automake
libtoolize --copy --force
aclocal
autoheader
autoconf
automake --add-missing --copy

echo "Now running the configure script"
./configure $*
