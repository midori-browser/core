# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

set(LIBMIDORI_VERSION 0.5.5)
set(LIBMIDORI_SOVERSION 0)

file(GLOB MIDORI_VALA_SOURCE ../katze/*.vala *.vala ../panels/*.vala)
file(GLOB LIBMIDORI_SOURCE ../katze/*.c ../panels/*.c ../toolbars/*.c midori-*.vala midori-*.c sokoke.c)
include(GLibHelpers)
add_glib_marshal(LIBMIDORI_SOURCE marshal ${CMAKE_PROJECT_NAME}_cclosure_marshal "midori.h")
set(MIDORI_SOURCE
    main.c
    )

include(ValaPrecompile)
vala_precompile(LIBMIDORI_SOURCE_C ${CMAKE_PROJECT_NAME}
    ${MIDORI_VALA_SOURCE}
PACKAGES
    ${PKGS}
OPTIONS
    ${VALAFLAGS}
CUSTOM_VAPIS
    ${EXTRA_VAPIS}
GENERATE_VAPI
    "${CMAKE_PROJECT_NAME}-core"
GENERATE_HEADER
    "${CMAKE_PROJECT_NAME}-core"
)

add_library("${LIBMIDORI}-vala" STATIC ${LIBMIDORI_SOURCE_C})
target_link_libraries("${LIBMIDORI}-vala"
                      ${DEPS_LIBRARIES}
                      ${OPTS_LIBRARIES}
                      ${DEPS_GTK_LIBRARIES}
                      )
set_target_properties("${LIBMIDORI}-vala" PROPERTIES
                      COMPILE_FLAGS "${VALA_CFLAGS}"
                      POSITION_INDEPENDENT_CODE ON
                      )

add_library(${LIBMIDORI} SHARED ${LIBMIDORI_SOURCE})
include_directories(
                    ${CMAKE_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    "${CMAKE_SOURCE_DIR}/katze"
                    "${CMAKE_SOURCE_DIR}/toolbars"
                    ${CMAKE_BINARY_DIR}
                    ${DEPS_INCLUDE_DIRS}
                    ${OPTS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    )
target_link_libraries(${LIBMIDORI}
                      "${LIBMIDORI}-vala"
                      ${DEPS_LIBRARIES}
                      ${OPTS_LIBRARIES}
                      ${DEPS_GTK_LIBRARIES}
                      )
set_target_properties(${LIBMIDORI} PROPERTIES
                      COMPILE_FLAGS ${CFLAGS}
                      SOVERSION 1
                      VERSION 0.0.0
                      )
if (WIN32)
    install(TARGETS ${LIBMIDORI}
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
else ()
    install(TARGETS ${LIBMIDORI}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
endif ()

if (WIN32)
    add_executable(${CMAKE_PROJECT_NAME} WIN32 main.c "${CMAKE_SOURCE_DIR}/data/midori.rc")
else()
    add_executable(${CMAKE_PROJECT_NAME} main.c)
endif()

target_link_libraries(${CMAKE_PROJECT_NAME}
                      ${LIBMIDORI})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
                      COMPILE_FLAGS ${CFLAGS}
                      )
install(TARGETS ${CMAKE_PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
