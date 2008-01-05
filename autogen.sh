#!/bin/sh

libtoolize --copy --force
aclocal
autoheader
autoconf
automake --add-missing --copy