# Copyright (C) 2013-2018 Christian Dywan <christian@twotoasts.de>

set(LIBCORE_VERSION ${CORE_VERSION})
set(LIBCORE_SOVERSION 0)

file(GLOB LIBCORE_SOURCE *.vala)
list(REMOVE_ITEM LIBCORE_SOURCE "main.vala")
set(MAIN_SOURCE
    main.vala
    )

include(ValaPrecompile)
vala_precompile(LIBCORE_SOURCE_C ${LIBCORE}
    ${LIBCORE_SOURCE}
PACKAGES
    ${PKGS}
OPTIONS
    ${VALAFLAGS}
    --gresources="${CMAKE_SOURCE_DIR}/gresource.xml"
CUSTOM_VAPIS
    ${EXTRA_VAPIS}
GENERATE_VAPI
    "${LIBCORE}"
GENERATE_HEADER
    "${LIBCORE}"
)

vala_precompile(MAIN_SOURCE_C ${CMAKE_PROJECT_NAME}
    ${MAIN_SOURCE}
PACKAGES
    ${PKGS}
OPTIONS
    ${VALAFLAGS}
CUSTOM_VAPIS
    ${EXTRA_VAPIS}
GENERATE_VAPI
    "${CMAKE_PROJECT_NAME}"
GENERATE_HEADER
    "${CMAKE_PROJECT_NAME}"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.c
                   COMMENT "Built resource"
                   COMMAND glib-compile-resources
                   --sourcedir ${CMAKE_SOURCE_DIR}
                   --generate-source
                   --target ${CMAKE_CURRENT_BINARY_DIR}/resources.c
                   ${CMAKE_SOURCE_DIR}/gresource.xml)

add_library("${LIBCORE}" SHARED ${LIBCORE_SOURCE_C} ${CMAKE_CURRENT_BINARY_DIR}/resources.c)
target_link_libraries("${LIBCORE}"
                      ${DEPS_LIBRARIES}
                      ${DEPS_GTK_LIBRARIES}
                      )
set_target_properties("${LIBCORE}" PROPERTIES
                      COMPILE_FLAGS "${VALA_CFLAGS}"
                      POSITION_INDEPENDENT_CODE ON
                      COMPILE_FLAGS ${VALA_CFLAGS}
                      SOVERSION ${LIBCORE_SOVERSION}
                      VERSION ${LIBCORE_VERSION}
                      )

include_directories(
                    ${CMAKE_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_BINARY_DIR}
                    ${DEPS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    )
if (WIN32)
    install(TARGETS ${LIBCORE}
            LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
else ()
    install(TARGETS ${LIBCORE}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (WIN32)
    add_executable(${CMAKE_PROJECT_NAME} WIN32 ${MAIN_SOURCE_C} "${CMAKE_SOURCE_DIR}/data/main.rc")
else()
    add_executable(${CMAKE_PROJECT_NAME} ${MAIN_SOURCE_C})
endif()

target_link_libraries(${CMAKE_PROJECT_NAME}
                      ${LIBCORE})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
                      COMPILE_FLAGS ${VALA_CFLAGS}
                      )
install(TARGETS ${CMAKE_PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
