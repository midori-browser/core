This file is licensed under the terms of the LGPL 2.1, see the file COPYING.

        FILES

README.txt              This file, explaining how to build Midori.
midori-0.3.3.nsi        The NSIS installer creation script.

packages.list           The suse mingw packages required to build Midori.
x86-mingw32-opensuse/   Install and update script for the opensuse repository.

makedist/               Scripts to create distribution package
			from cross-compiled binaries and libs.

crossconfig.sh          Setup environment for cross build and run
			configure script for cross build.


        INTRODUCTION

This document will explain how to get Midori compiling for windows, hopefully.


        REQUIREMENTS

A working tool chain with the mingw compiler is required. This can either be
native in Windows or a cross-chain.
This tool chain must have the following applications and all applications
needed to build Midori:
- basename, for x86-mingw32-opensuse
- bash, for the shell scripts
- cat, for makedist.midori
- cp, for x86-mingw32-opensuse and makedist.midori
- cpio, for x86-mingw32-opensuse
- echo, for x86-mingw32-opensuse and makedist.midori
- find, for makedist.midori
- git, to get the development sources
- grep, for makedist.midori
- gunzip, for x86-mingw32-opensuse
- head, for x86-mingw32-opensuse
- ls, for makedist.midori
- mkdir, for x86-mingw32-opensuse and makedist.midori
- mktemp, for makedist.midori
- pwd, for makedist.midori
- rm, for x86-mingw32-opensuse and makedist.midori
- rmdir, for makedist.midori
- rpm2cpio, for x86-mingw32-opensuse
- sed, for x86-mingw32-opensuse
- sha1sum, for makedist.midori
- sort, for x86-mingw32-opensuse and makedist.midori
- strings, for makedist.midori
- tar, to unpack the scripts
- touch, for x86-mingw32-opensuse
- uniq, for makedist.midori
- wget, for x86-mingw32-opensuse
- xmlgrep, for x86-mingw32-opensuse
- zip, for makedist.midori


        PREPARATIONS

Install all the packages needed by Midori and its dependencies. There are two
ways to do this. Automatically install the dependencies from opensuse.org or
manually install them.
To automatically install the packages use the scripts and change configuration
in header of install.sh file where needed. And run ./install.sh to install all
necessary packages.
This script can be also invoked with "update" argument ./install.sh update
to update the installed packages to newer versions.
To install the dependencies manually install the packages in packages.list.
This are all the packages needed by Midori and its dependencies.

Get the Midori sources. Either use git to get them or download a snapshot or a
release. See http://git.xfce.org/apps/midori or
http://archive.xfce.org/src/apps/midori/


        BUILDING

Read the instruction in the source tree. For cross compiling the script
crossconfig.sh might help.


        DISTRIBUTION

To create a Windows installer for Midori you need NSIS. See
http://nsis.sourceforge.net/Main_Page
Use the makedist.midori script to get all files needed for distribution. Add
the version number as argument to makedist.midori to create a version number
in the distribution package. eg. ./makedist.midori -0.3.3
To complete the installer unzip the file created by the makedist script.
Compile midori-0.3.3.nsi with NSIS.

