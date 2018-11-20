# Copyright (C) 2013-2018 Christian Dywan <christian@twotoasts.de>

cmake_minimum_required(VERSION 3.2)

# dh_translations detects this if there's no variable used
set (GETTEXT_PACKAGE "midori")
add_definitions("-DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\"")
project(${GETTEXT_PACKAGE} C)
set(PROJECT_BUGS https://github.com/midori-browser/core/issues)
set(PROJECT_WEBSITE https://www.midori-browser.org)
set(CORE_VERSION 6.0)

execute_process(COMMAND "git" "describe" "--tags"
                OUTPUT_VARIABLE REVISION
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if (REVISION)
    set(CORE_VERSION "${REVISION}")
    # All warnings are errors in development builds
    set(VALAFLAGS ${VALAFLAGS} --fatal-warnings)
else ()
    # No runtime type checks
    set(VALAFLAGS ${VALAFLAGS} --disable-assert)
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
# Disallow building during install to avoid permission problems
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY 1)

include(FindIntltool)

find_package(Vala REQUIRED)
vala_require("0.30.0")
set(VALAFLAGS ${VALAFLAGS}
    --debug
    --target-glib=2.48
    --enable-deprecated
    )

include(GNUInstallDirs)
set(DATADIR ${CMAKE_INSTALL_FULL_DATADIR})
set(PLUGINDIR "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}" CACHE STRING "Location for system-wide plugins")

find_package(PkgConfig)
pkg_check_modules(DEPS REQUIRED
                  sqlite3>=3.6.19
                  gio-2.0>=2.48.0
                  libsoup-2.4>=2.48.0
                  )
set(PKGS sqlite3 gio-2.0 libsoup-2.4)

pkg_check_modules(DEPS_GTK REQUIRED
                  gtk+-3.0>=3.12.0
                  webkit2gtk-4.0>=2.16.6
                  gcr-ui-3>=2.32
                  libpeas-gtk-1.0
                  )
set(PKGS ${PKGS} gtk+-3.0 libsoup-2.4 gcr-ui-3 libpeas-gtk-1.0)
set(EXTRA_VAPIS
    ${CMAKE_SOURCE_DIR}/vapi/config.vapi
    ${CMAKE_SOURCE_DIR}/vapi/webkit2gtk-4.0.vapi
    ${CMAKE_SOURCE_DIR}/vapi/javascriptcoregtk-4.0.vapi)

add_definitions("-DGCR_API_SUBJECT_TO_CHANGE")
set(CFLAGS "${CFLAGS} -g")

# Explicitly add -fPIC for older toolchains
set(VALA_CFLAGS "-g -fPIC -w -Werror")

set(LIBCORE "${CMAKE_PROJECT_NAME}-core")
set(CORE_USER_AGENT_VERSION "Midori/6")

configure_file(config.h.in config.h @ONLY)

# CMake provides no uninstall target by design
add_custom_target (uninstall
    COMMAND "xargs" "rm" "-v" "<" "install_manifest.txt")

install(FILES COPYING README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(DESKTOP_ICON "${CMAKE_PROJECT_NAME}" CACHE STRING "The Icon value to be used in the .desktop file")

add_subdirectory (core)
add_subdirectory (web)
add_subdirectory (extensions)
enable_testing()
add_subdirectory (tests)
add_subdirectory (po)
add_subdirectory (icons)
add_subdirectory (data)
