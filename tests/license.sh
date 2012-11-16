#! /usr/bin/env sh
# Copyright 2012 Christian Dywan <christian@twotoasts.de>
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
echo Running 'licensecheck'
test -n "$SRCDIR" && cd $SRCDIR
find . \! -path './.waf*/*' -a \! -path './_build/*' | xargs licensecheck | grep UNKNOWN && exit 1
echo ...OK
