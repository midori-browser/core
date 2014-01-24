#! /usr/bin/env sh
# Copyright 2012 Christian Dywan <christian@twotoasts.de>
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
echo Checking POTFILES.in for completeness
test -n "$SRCDIR" && cd $SRCDIR
test -z "$BLDDIR" && BLDDIR=_build
for i in $(find . -regextype posix-egrep \! -regex "./($BLDDIR|_.+|debian|tests)/.+" -a -regex './[^.]+.+[.](vala|c)'); do
    grep -q $(basename $i) po/POTFILES.in || FILES="$FILES$i\n"
done
test -n "$FILES" && echo "$FILES...FAILED"
test -z "$FILES" && echo "...OK"
