#! /usr/bin/env sh
# Copyright 2012 Christian Dywan <christian@twotoasts.de>
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
echo Running 'licensecheck'
test -z $(which licensecheck) && echo ...SKIPPED: not installed && exit 0
test -n "$SRCDIR" && cd $SRCDIR
test -z "$BLDDIR" && BLDDIR=_build
find . \! -path './.*/*' -a \! -path "./$BLDDIR/*" -a \! -path "./_*/*" -a \! -path "./debian/*" | xargs licensecheck | grep UNKNOWN && exit 1
echo ...OK
