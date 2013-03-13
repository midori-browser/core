#! /usr/bin/env sh
# Copyright 2013 Christian Dywan <christian@twotoasts.de>
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
echo Validating .desktop files
test -z $(which desktop-file-validate) && echo ...SKIPPED: not installed && return 0
test -z "$SRCDIR" && SRCDIR=$PWD
test -z "$BLDDIR" && BLDDIR=_build
cd "$SRCDIR/$BLDDIR/default/data"
ERRORS=0
for i in $(ls | GREP_OPTIONS= grep .desktop); do
    for j in $(desktop-file-validate $i | grep -v 'unregistered value "Pantheon"' | tr ' ' '_'); do
        ERRORS=1
        echo $j | tr '_' ' '
    done
done
test "$ERRORS" = 1 && echo ...FAILED && exit 1
echo ...OK
