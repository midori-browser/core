# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
project(midori C)
add_definitions("-DPACKAGE_NAME=\"${CMAKE_PROJECT_NAME}\"")
add_definitions("-DPACKAGE_BUGREPORT=\"https://bugs.launchpad.net/midori\"")
set(VERSION 0.5.5)
add_definitions("-DPACKAGE_VERSION=\"${VERSION}\"")
add_definitions("-DMIDORI_VERSION_SUFFIX=\"${VERSION}\"")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
# Disallow building during install to avoid permission problems
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY 1)

include(GNUInstallDirs)
set(DATADIR ${CMAKE_INSTALL_FULL_DATADIR})
add_definitions("-DMDATADIR=\"${DATADIR}\"")
add_definitions("-DSYSCONFDIR=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}\"")
add_definitions("-DLIBDIR=\"${CMAKE_INSTALL_FULL_LIBDIR}\"")
add_definitions("-DDOCDIR=\"${CMAKE_INSTALL_FULL_DOCDIR}\"")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h" "/* # generated file (stub) */")

find_package(PkgConfig)
pkg_check_modules(DEPS REQUIRED
                  libxml-2.0>=2.6
                  sqlite3>=3.6.19
                  gmodule-2.0
                  gio-2.0>=2.32.3
                  libsoup-gnome-2.4>=2.27.90
                  )
add_definitions("-DHAVE_LIBXML")
add_definitions("-DGIO_VERSION=\"${DEPS_gio-2.0_VERSION}\"")
add_definitions("-DLIBSOUP_VERSION=\"${DEPS_libsoup-gnome-2.4_VERSION}\"")
set(PKGS posix linux libxml-2.0 sqlite3 gmodule-2.0 gio-2.0 libsoup-2.4)
if (${DEPS_libsoup-gnome-2.4_VERSION} VERSION_GREATER "2.34.0")
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_LIBSOUP_2_34_0")
endif ()
pkg_check_modules(OPTS
                  libnotify
                  zeitgeist-1.0>=0.3.14
                  )
add_definitions("-DLIBNOTIFY_VERSION=\"${OPTS_libnotify_VERSION}\"")
add_definitions("-DGRANITE_VERSION=\"${OPTS_granite_VERSION}\"")
set(PKGS ${PKGS} libnotify zeitgeist-1.0)
option(USE_GTK3 "Use GTK+3" OFF)
if (USE_GTK3)
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-3.0>=3.0.0
                      webkitgtk-3.0>=1.8.3
                      javascriptcoregtk-3.0
                      )
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-3.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkitgtk-3.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-3.0)
    pkg_check_modules(OPTS_GTK
                      gcr-3>=2.32
                      unique-3.0>=0.9
                      )
    if (OPTS_GTK_unique-3.0_VERSION)
        add_definitions("-DHAVE_UNIQUE")
        add_definitions("-DUNIQUE_VERSION=\"${OPTS_GTK_unique-3.0_VERSION}\"")
    endif ()
    add_definitions("-DGCR_VERSION=\"${OPTS_GTK_gcr-3_VERSION}\"")
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_GTK3")
endif ()

find_package(Vala REQUIRED)
vala_require("0.16.0")
set(VALAFLAGS ${VALAFLAGS}
    --enable-deprecated
    --debug
    )

set(GETTEXT_PACKAGE ${CMAKE_PROJECT_NAME})
add_definitions("-DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\"")

set(CFLAGS "-w -Wno-deprecated-declarations")
set(LIBMIDORI "${CMAKE_PROJECT_NAME}-core")

add_subdirectory (midori)
add_subdirectory (po)
add_subdirectory (data)
add_subdirectory (extensions)
