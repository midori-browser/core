# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
# Work-around a bug in the included FindGettext fixed with 2.8.8
# See http://www.cmake.org/pipermail/cmake-commits/2012-February/012117.html
if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}" VERSION_LESS "2.8.8")
    cmake_policy(SET CMP0002 OLD)
endif ()
project(midori C)
add_definitions("-DPACKAGE_NAME=\"${CMAKE_PROJECT_NAME}\"")
add_definitions("-DPACKAGE_BUGREPORT=\"https://bugs.launchpad.net/midori\"")

set(VERSION 0.5.11)
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
    # All warnings are errors in development builds
    set(VALAFLAGS ${VALAFLAGS} --fatal-warnings)
    set(CFLAGS "${CFLAGS}")
endif ()
add_definitions("-DPACKAGE_VERSION=\"${VERSION}\"")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
# Disallow building during install to avoid permission problems
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY 1)

find_package(Vala REQUIRED)
vala_require("0.16.0")
set(VALAFLAGS ${VALAFLAGS}
    --enable-deprecated
    --debug
    )

include(GNUInstallDirs)
set(DATADIR ${CMAKE_INSTALL_FULL_DATADIR})
add_definitions("-DMDATADIR=\"${DATADIR}\"")
add_definitions("-DSYSCONFDIR=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}\"")
add_definitions("-DLIBDIR=\"${CMAKE_INSTALL_FULL_LIBDIR}\"")
add_definitions("-DDOCDIR=\"${CMAKE_INSTALL_FULL_DOCDIR}\"")

add_definitions("-DENABLE_NLS=1")
add_definitions("-DLOCALEDIR=\"${CMAKE_INSTALL_FULL_LOCALEDIR}\"")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h" "/* # generated file (stub) */")
add_definitions("-DHAVE_CONFIG_H=1")

find_file (UNISTD unistd.h)
if (UNISTD)
    add_definitions("-DHAVE_UNISTD_H")
endif ()

if (UNIX)
    find_file (SIGNAL signal.h)
    if (SIGNAL)
        add_definitions("-DHAVE_SIGNAL_H")
    endif ()

    find_file (EXEC_INFO execinfo.h)
    if (EXEC_INFO)
        set(VALAFLAGS ${VALAFLAGS} -D HAVE_EXECINFO_H)
    endif ()
endif ()

find_library (X11 X11)
if (X11)
    # Pass /usr/X11R6/include for OpenBSD
    find_file (SCRNSAVER X11/extensions/scrnsaver.h /usr/X11R6/include)
    find_library (XSS Xss /usr/lib/X11R6/lib)
    if (SCRNSAVER AND XSS)
        add_definitions("-DHAVE_X11_EXTENSIONS_SCRNSAVER_H")
        set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${XSS};${X11}")
    endif ()
endif ()

if (WIN32)
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_WIN32)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_FREEBSD)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_DRAGONFLY)
    set(DFLY 1)
endif ()

if (APPLE)
    add_definitions("-DHAVE_OSX=1")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_OSX)
else ()
    add_definitions("-DHAVE_OSX=0")
endif ()

find_package(PkgConfig)
pkg_check_modules(DEPS REQUIRED
                  libxml-2.0>=2.6
                  sqlite3>=3.6.19
                  gmodule-2.0
                  gio-2.0>=2.32.3
                  libsoup-gnome-2.4>=2.37.1
                  )
add_definitions("-DHAVE_LIBXML")
add_definitions("-DGIO_VERSION=\"${DEPS_gio-2.0_VERSION}\"")
add_definitions("-DLIBSOUP_VERSION=\"${DEPS_libsoup-gnome-2.4_VERSION}\"")
set(PKGS posix linux libxml-2.0 sqlite3 gmodule-2.0 gio-2.0 libsoup-2.4)
if (${DEPS_libsoup-gnome-2.4_VERSION} VERSION_GREATER "2.40.0")
    # valac 0.16 didn't have the bindings yet
    # For consistency we need to ensure C code makes the same assumptions
    if (${VALA_VERSION} VERSION_GREATER "0.17.0")
        add_definitions("-DHAVE_LIBSOUP_2_40_0")
        set(VALAFLAGS ${VALAFLAGS} -D HAVE_LIBSOUP_2_40_0)
    endif ()
endif ()
if (${DEPS_libsoup-gnome-2.4_VERSION} VERSION_GREATER "2.48.0")
    add_definitions("-DHAVE_LIBSOUP_2_48_0")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_LIBSOUP_2_48_0)
endif ()

if (${DEPS_gio-2.0_VERSION} VERSION_GREATER "2.40.0" OR WIN32)
    add_definitions("-DLIBNOTIFY_VERSION=\"No\"")
else ()
    pkg_check_modules(NOTIFY REQUIRED libnotify)
    add_definitions("-DLIBNOTIFY_VERSION=\"${NOTIFY_VERSION}\"")
    add_definitions("-DHAVE_LIBNOTIFY")
    set(OPTS_INCLUDE_DIRS "${OPTS_INCLUDE_DIRS};${NOTIFY_INCLUDE_DIRS}")
    set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${NOTIFY_LIBRARIES}")
    set(PKGS ${PKGS} libnotify)
endif ()

option(USE_GTK3 "Use GTK+3" OFF)
option(HALF_BRO_INCOM_WEBKIT2 "Serve as a guniea pig" OFF)
option(USE_ZEITGEIST "Zeitgeist history integration" ON)
option(USE_GRANITE "Fancy notebook and pop-overs" OFF)
option(USE_APIDOCS "API documentation" OFF)
option(USE_GIR "Generate GObject Introspection bindings" OFF)
option(EXTRA_WARNINGS "Additional compiler warnings" OFF)

# GTK+3 is implied here, whether set or not
if (USE_GRANITE OR HALF_BRO_INCOM_WEBKIT2)
    set(USE_GTK3 ON)
endif ()

if (USE_GRANITE)
    pkg_check_modules(GRANITE granite>=0.2)
    set(OPTS_INCLUDE_DIRS "${OPTS_INCLUDE_DIRS};${GRANITE_INCLUDE_DIRS}")
    set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${GRANITE_LIBRARIES}")
    add_definitions("-DHAVE_GRANITE")
    add_definitions("-DGRANITE_VERSION=\"${GRANITE_VERSION}\"")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_GRANITE)
    set(PKGS ${PKGS} granite)
else ()
    add_definitions("-DGRANITE_VERSION=\"No\"")
endif()

if (USE_ZEITGEIST)
    pkg_check_modules(ZEITGEIST zeitgeist-2.0>=0.3.14)
    set(OPTS_INCLUDE_DIRS "${OPTS_INCLUDE_DIRS};${ZEITGEIST_INCLUDE_DIRS}")
    set(OPTS_LIBRARIES "${OPTS_LIBRARIES};${ZEITGEIST_LIBRARIES}")
    add_definitions("-DHAVE_ZEITGEIST")
    set(PKGS ${PKGS} zeitgeist-2.0)
endif()

if (WIN32)
    add_definitions("-DGCR_VERSION=\"No\"")
else ()
    if (USE_GTK3)
        pkg_check_modules(GCR REQUIRED gcr-3>=2.32)
    else ()
        pkg_check_modules(GCR REQUIRED gcr-base-3>=2.32)
    endif ()
    add_definitions("-DGCR_VERSION=\"${GCR_VERSION}\"")
    add_definitions("-DHAVE_GCR")
    set(OPTS_INCLUDE_DIRS ${OPTS_INCLUDE_DIRS} ${GCR_INCLUDE_DIRS})
    set(OPTS_LIBRARIES ${OPTS_LIBRARIES} ${GCR_LIBRARIES})
endif ()

if (HALF_BRO_INCOM_WEBKIT2)
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-3.0>=3.10.0
                      webkit2gtk-4.0>=2.3.91
                      )
    add_definitions("-DHAVE_WEBKIT2")
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-3.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkit2gtk-4.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-3.0)
    # set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkit2gtk-web-extension-4.0.vapi")
    set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkit2gtk-4.0.vapi")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_GTK3)
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_WEBKIT2)
        set(VALAFLAGS ${VALAFLAGS} -D HAVE_WEBKIT2_3_91)
elseif (USE_GTK3)
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-3.0>=3.10.0
                      webkitgtk-3.0>=1.8.1
                      javascriptcoregtk-3.0
                      )
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-3.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkitgtk-3.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-3.0)
    set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkitgtk-3.0.vapi")
    set(VALAFLAGS ${VALAFLAGS} -D HAVE_GTK3)
else ()
    pkg_check_modules(DEPS_GTK REQUIRED
                      gtk+-2.0>=2.24.0
                      webkit-1.0>=1.8.1
                      javascriptcoregtk-1.0
                      )
    add_definitions("-DGTK_VERSION=\"${DEPS_GTK_gtk+-2.0_VERSION}\"")
    add_definitions("-DWEBKIT_VERSION=\"${DEPS_GTK_webkit-1.0_VERSION}\"")
    set(PKGS ${PKGS} gtk+-2.0)
    set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/midori/webkitgtk-3.0.vapi")
endif ()
set(EXTRA_VAPIS ${EXTRA_VAPIS} "${CMAKE_SOURCE_DIR}/katze/katze.vapi")

# dh_translations detects this if there's no variable used
set (GETTEXT_PACKAGE "midori")
add_definitions("-DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\"")

set(CFLAGS "${CFLAGS} -Wall -Wundef -Wno-deprecated-declarations -g")

if (EXTRA_WARNINGS)
    LIST(APPEND EXTRA_CFLAGS_LIST
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-comment
        -Waggregate-return
        -Wredundant-decls
        -Wshadow -Wpointer-arith -Wcast-align
        -Winline -Wformat-security -fno-common
        -Winit-self -Wundef
        -Wnested-externs
    )
    string(REPLACE ";" " " EXTRA_CFLAGS "${EXTRA_CFLAGS_LIST}")
    set(CFLAGS "${CFLAGS} ${EXTRA_CFLAGS}")
else ()
    if (REVISION)
        set(CFLAGS "${CFLAGS} -Werror")
    endif()
endif ()

# Explicitly add -fPIC for older toolchains
set(VALA_CFLAGS "-g -fPIC")

# With compiler versions that can, enable exactly the non-spurious warnings
# in Vala-generated C, otherwise disable warnings
if ((CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER "5.0.0")
    OR (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER "3.0.0"))
    set(VALA_CFLAGS "${VALA_CFLAGS} -Werror=implicit-function-declaration")
    set(VALA_CFLAGS "${VALA_CFLAGS} -Wno-incompatible-pointer-types")
    set(VALA_CFLAGS "${VALA_CFLAGS} -Wno-discarded-qualifiers")
    set(VALA_CFLAGS "${VALA_CFLAGS} -Wno-deprecated-declarations")
else ()
    set(VALA_CFLAGS "${VALA_CFLAGS} -w")
endif ()

set(LIBMIDORI "${CMAKE_PROJECT_NAME}-core")

# CMake provides no uninstall target by design
add_custom_target (uninstall
    COMMAND "xargs" "rm" "-v" "<" "install_manifest.txt")

install(FILES AUTHORS COPYING ChangeLog EXPAT README DESTINATION ${CMAKE_INSTALL_DOCDIR})

add_subdirectory (midori)
add_subdirectory (extensions)
enable_testing()
add_subdirectory (tests)
add_subdirectory (po)
add_subdirectory (icons)
add_subdirectory (data)
add_subdirectory (config)

if (USE_APIDOCS)
    add_subdirectory (docs/api)
endif ()
if (USE_GIR)
    add_subdirectory (gir)
endif ()
