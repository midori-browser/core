#!/bin/sh

# Copyright (C) 2010-2011 Peter de Ridder <peter@xfce.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# See the file COPYING for the full license text.

# config variables
REPO_URL=http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_11.4
REPO_ARCH=noarch
DOWNLOAD_PATH=~/dev/mingw/packages/opensuse
BUILD_PATH=~/tmp/opensuse
INSTALL_PATH=~/dev/mingw/mingw32

if [[ "$1" == "update" ]]
then
  UPDATE=yes
fi

which xmlgrep 2> /dev/null && HAVE_XMLGREP=1 || HAVE_XMLGREP=0
if [[ "$HAVE_XMLGREP" == "0" ]]; then
    echo -e "\nPlease install xmlclitools http://robur.slu.se/jensl/xmlclitools\n"
    exit
fi

# create download and build directory
mkdir -p $DOWNLOAD_PATH
mkdir -p $BUILD_PATH

rm $DOWNLOAD_PATH/repomd.xml
wget -nc $REPO_URL/repodata/repomd.xml -P $DOWNLOAD_PATH || exit 1
OTHER_FILE=`xmlgrep -c -f $DOWNLOAD_PATH/repomd.xml repomd.data:type=other.location | sed 's/.*href="\([^"]*\)".*/\1/'`

wget -nc $REPO_URL/$OTHER_FILE -O $DOWNLOAD_PATH/other.xml.gz || exit 1
rm $DOWNLOAD_PATH/other.xml
#gunzip -N $DOWNLOAD_PATH/`basename $OTHER_FILE`
gunzip -N $DOWNLOAD_PATH/other.xml

rm packages.version
touch packages.version

# download all packages
while read line
do
  VERSION=`xmlgrep -c -f $DOWNLOAD_PATH/other.xml otherdata.package:name="$line":arch="$REPO_ARCH" | awk -F\< '{print $2}' |sed -e 'h' -e 's/^.*ver="\([^"]*\)".*$/\1/p' -e 'g' -e 's/^.*rel="\([^"]*\)".*$/\1/' | sed -e N -e 's/\n/-/' | sort -V -r | head -n 1`
  FILE=$line-$VERSION.$REPO_ARCH.rpm
  test "$UPDATE" == yes || echo $FILE >> packages.version
  if [ ! -f $DOWNLOAD_PATH/$FILE ]
  then
    test "$UPDATE" == yes && echo $FILE >> packages.version
    wget -nc $REPO_URL/$REPO_ARCH/$FILE -P $DOWNLOAD_PATH
  fi
done < packages.list

# convert and install packages
while read line
do
  pushd $BUILD_PATH
  # extract rpm
  rpm2cpio $DOWNLOAD_PATH/$line | cpio -i -d

  if [ -d $BUILD_PATH/usr/i686-pc-ming32/sys-root/mingw ]
  then
    # convert pkgconfig files
    if [ -d $BUILD_PATH/usr/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig ]
    then
      sed -i -e 's@^prefix=.*@prefix='$INSTALL_PATH'@' -e 's@/usr/i686-pc-mingw32/sys-root/mingw@${prefix}@' $BUILD_PATH/usr/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig/*.pc
    fi
    # install the package
    cp -rf $BUILD_PATH/usr/i686-pc-mingw32/sys-root/mingw/* $INSTALL_PATH/
  fi

  if [ -d $BUILD_PATH/usr/i686-w64-mingw32/sys-root/mingw ]
  then
    # convert pkgconfig files
    if [ -d $BUILD_PATH/usr/i686-w64-mingw32/sys-root/mingw/lib/pkgconfig ]
    then
      sed -i -e 's@^prefix=.*@prefix='$INSTALL_PATH'@' -e 's@/usr/i686-w64-mingw32/sys-root/mingw@${prefix}@' $BUILD_PATH/usr/i686-w64-mingw32/sys-root/mingw/lib/pkgconfig/*.pc
    fi
    # install the package
    cp -rf $BUILD_PATH/usr/i686-w64-mingw32/sys-root/mingw/* $INSTALL_PATH/
  fi

  # remove the extracted file
  rm -rf $BUILD_PATH/usr
  popd
done < packages.version

# remove build directory
rm -rf $BUILD_PATH
