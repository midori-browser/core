#! /bin/sh

# Copyright (C) 2010-2011 Peter de Ridder <peter@xfce.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# See the file COPYING for the full license text.

temp_file_new=`mktemp`
temp_file_old=`mktemp`

while [ "$1" ]
do
  echo $1 >> $temp_file_new
  shift
done

while [ "x`sha1sum - < $temp_file_new`" != "x`sha1sum - < $temp_file_old`" ]
do
  files=`cat $temp_file_new $temp_file_old | sort | uniq -u`
  cp $temp_file_new $temp_file_old
  strings $files 2> /dev/null | grep \\.dll | cat - $temp_file_old | sort | uniq > $temp_file_new
done

cat $temp_file_new

rm $temp_file_new $temp_file_old

