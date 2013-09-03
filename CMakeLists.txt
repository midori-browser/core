# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
project(midori C)
add_definitions("-DPACKAGE_NAME=\"${CMAKE_PROJECT_NAME}\"")
add_definitions("-DPACKAGE_BUGREPORT=\"https://bugs.launchpad.net/midori\"")

set(VERSION 0.5.5)
add_definitions("-DMIDORI_VERSION_SUFFIX=\"${VERSION}\"")
 
string(REPLACE "." ";" VERSION_LIST ${VERSION})
LIST(GET VERSION_LIST 0 MIDORI_MAJOR_VERSION)
add_definitions("-DMIDORI_MAJOR_VERSION=${MIDORI_MAJOR_VERSION}")
LIST(GET VERSION_LIST 1 MIDORI_MINOR_VERSION)
add_definitions("-DMIDORI_MINOR_VERSION=${MIDORI_MINOR_VERSION}")
LIST(GET VERSION_LIST 2 MIDORI_MICRO_VERSION)
add_definitions("-DMIDORI_MICRO_VERSION=${MIDORI_MICRO_VERSION}")

execute_process(COMMAND "bzr" "revno"
                OUTPUT_VARIABLE REVISION
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if (REVISION)
    set(VERSION "${VERSION}~r${REVISION}")
endif ()
add_definitions("-DPACKAGE_VERSION=\"${VERSION}\"")

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

find_file (UNISTD unistd.h)
if (UNISTD)
    add_definitions("-DHAVE_UNISTD_H")
endif ()

if (UNIX)
    find_file (SIGNAL signal.h)
    if (SIGNAL)
        add_definitions("-DHAVE_SIGNAL_H")
    endif ()
endif ()

find_library (X11 X11)
if (X11)
    # Pass /usr/X11R6/include for OpenBSD
    find_file (SCRNSAVER X11/extensions/scrnsaver.h /usr/X11R6/include)
    find_library (XSS Xss /usr/lib/X11R6/lib)
    if (SCRNSAVER AND XSS)
        add_definitions("-DHAVE_X11_EXTENSIONS_SCRNSAVER_H")
        set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${XSS}")
    endif ()
endif ()

if (WIN32)
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_WIN32")
endif ()

string(FIND ${CMAKE_SYSTEM_NAME} "FreeBSD" FREEBSD)
if (FREEBSD GREATER -1)
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_FREEBSD")
endif ()

if (APPLE)
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_OSX")
endif ()

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

if (WIN32)
    add_definitions("-DLIBNOTIFY_VERSION=\"No\"")
else ()
    pkg_check_modules(NOTIFY REQUIRED libnotify)
    add_definitions("-DLIBNOTIFY_VERSION=\"${NOTIFY_VERSION}\"")
    set(OPTS_INCLUDE_DIRS "${OPTS_INCLUDE_DIRS};${NOTIFY_INCLUDE_DIRS}")
    set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${NOTIFY_LIBRARIES}")
    set(PKGS ${PKGS} libnotify)
endif ()

option(USE_GTK3 "Use GTK+3" OFF)
option(HALF_BRO_INCOM_WEBKIT2 "Serve as a guniea pig" OFF)
option(USE_ZEITGEIST "Zeitgeist history integration" ON)
option(USE_GRANITE "Fancy notebook and pop-overs" OFF)

if (USE_GRANITE)
    if (NOT USE_GTK3 AND NOT HALF_BRO_INCOM_WEBKIT2)
        set(USE_GTK3 ON)
    endif ()

    pkg_check_modules(GRANITE granite>=0.2)
    set(OPTS_INCLUDE_DIRS "${OPTS_INCLUDE_DIRS};${GRANITE_INCLUDE_DIRS}")
    set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${GRANITE_LIBRARIES}")
    add_definitions("-DHAVE_GRANITE")
    add_definitions("-DGRANITE_VERSION=\"${GRANITE_VERSION}\"")
    set(PKGS ${PKGS} granite)
else ()
    add_definitions("-DGRANITE_VERSION=\"No\"")
endif()

if (USE_ZEITGEIST)
    pkg_check_modules(ZEITGEIST zeitgeist-1.0>=0.3.14)
    set(OPTS_INCLUDE_DIRS "${OPTS_INCLUDE_DIRS};${ZEITGEIST_INCLUDE_DIRS}")
    set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${ZEITGEIST_LIBRARIES}")
    add_definitions("-DHAVE_ZEITGEIST")
    set(PKGS ${PKGS} zeitgeist-1.0)
endif()

if (USE_GTK3)
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-3.0>=3.0.0
                      webkitgtk-3.0>=1.8.3
                      javascriptcoregtk-3.0
                      )
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-3.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkitgtk-3.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-3.0)
    set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkitgtk-3.0.vapi")
    # Always pass >1 module, otherwise we get different variable names
    pkg_check_modules(OPTS_GTK
                      gio-2.0
                      gcr-3>=2.32
                      )
    add_definitions("-DGCR_VERSION=\"${OPTS_GTK_gcr-3_VERSION}\"")
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_GTK3")
elseif (HALF_BRO_INCOM_WEBKIT2)
    # Note: WebKitGTK+ 2.0.0 matches 1.11.91; 1.11.92 > 2.0.0
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-3.0>=3.0.0
                      webkit2gtk-3.0>=1.11.91
                      javascriptcoregtk-3.0
                      )
                  add_definitions("-DHAVE_WEBKIT2")
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-3.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkit2gtk-3.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-3.0)
    set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkit2gtk-3.0.vapi")
    # Always pass >1 module, otherwise we get different variable names
    pkg_check_modules(OPTS_GTK
                      gio-2.0
                      gcr-3>=2.32
                      )
    add_definitions("-DGCR_VERSION=\"${OPTS_GTK_gcr-3_VERSION}\"")
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_GTK3")
    set(VALAFLAGS ${VALAFLAGS} "-D;HAVE_WEBKIT2")
else ()
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-2.0>=2.24.0
                      webkit-1.0>=1.8.3
                      javascriptcoregtk-1.0
                      )
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-2.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkit-1.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-2.0)
    set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkitgtk-3.0.vapi")
    add_definitions("-DGCR_VERSION=\"No\"")
endif ()

find_package(Vala REQUIRED)
vala_require("0.16.0")
set(VALAFLAGS ${VALAFLAGS}
    --enable-deprecated
    --debug
    )

# dh_translations detects this if there's no variable used
set (GETTEXT_PACKAGE "midori")
add_definitions("-DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\"")

set(CFLAGS "-w -Wno-deprecated-declarations -g")
set(LIBMIDORI "${CMAKE_PROJECT_NAME}-core")

# CMake provides no uninstall target by design
add_custom_target (uninstall
    COMMAND "xargs" "rm" "-v" "<" "install_manifest.txt")

add_subdirectory (midori)
add_subdirectory (po)
add_subdirectory (icons)
add_subdirectory (data)
enable_testing()
add_subdirectory (tests)
add_subdirectory (extensions)
