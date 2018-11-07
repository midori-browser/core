# Copyright (C) 2013-2018 Christian Dywan <christian@twotoasts.de>

set(EXTENSIONDIR "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}")
include_directories(
                    "${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/core"
                    ${DEPS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    ${CMAKE_BINARY_DIR}
                    "${CMAKE_BINARY_DIR}/core"
                    )

file(GLOB EXTENSIONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
foreach(UNIT_SRC ${EXTENSIONS})
    # Extension sources may be in folders
    string(FIND ${UNIT_SRC} "." UNIT_EXTENSION)
    if (UNIT_EXTENSION EQUAL -1)
        set(UNIT ${UNIT_SRC})
        file(GLOB UNIT_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${UNIT_SRC}/*.vala")
    elseif (${UNIT_SRC} MATCHES "(.vala)$")
        string(REPLACE ".vala" "" UNIT ${UNIT_SRC})
    endif ()

    if ("${UNIT_SRC}" MATCHES "(.vala)$")
        include(ValaPrecompile)
        vala_precompile(UNIT_SRC_C ${UNIT}
            ${UNIT_SRC}
        PACKAGES
            ${PKGS}
        OPTIONS
            ${VALAFLAGS}
            --gresources="${CMAKE_SOURCE_DIR}/gresource.xml"
        CUSTOM_VAPIS
            ${CMAKE_BINARY_DIR}/core/${LIBCORE}.vapi
            ${EXTRA_VAPIS}
        )

        add_library(${UNIT} MODULE ${UNIT_SRC_C})
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS "${VALA_CFLAGS}"
                              )
    else ()
        continue()
    endif ()
    target_link_libraries(${UNIT}
                          ${DEPS_LIBRARIES}
                          ${DEPS_GTK_LIBRARIES}
                          ${LIBCORE}
                          )
    install(TARGETS ${UNIT}
            LIBRARY DESTINATION ${EXTENSIONDIR}
            )
    set(MANIFEST "${UNIT}.plugin")
    INTLTOOL_MERGE_DESKTOP_LIKE (${MANIFEST} po)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MANIFEST}
            DESTINATION ${EXTENSIONDIR}
            )
endforeach ()
