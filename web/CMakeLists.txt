# Copyright (C) 2013-2018 Christian Dywan <christian@twotoasts.de>

include_directories(
                    "${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/core"
                    ${DEPS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    ${CMAKE_BINARY_DIR}
                    "${CMAKE_BINARY_DIR}/core"
                    )
file(GLOB EXTENSIONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.vala)
foreach(UNIT_SRC ${EXTENSIONS})
    if (${UNIT_SRC} MATCHES "(.vala)$")
        string(REPLACE ".vala" "" UNIT ${UNIT_SRC})
        include(ValaPrecompile)
        vala_precompile(UNIT_SRC_C ${UNIT}
            ${UNIT_SRC}
            ${CMAKE_SOURCE_DIR}/core/loggable.vala
            ${CMAKE_SOURCE_DIR}/core/plugins.vala
            ${CMAKE_SOURCE_DIR}/core/settings.vala
        PACKAGES
            libsoup-2.4 libpeas-1.0
        OPTIONS
            ${VALAFLAGS}
        CUSTOM_VAPIS
            ${CMAKE_SOURCE_DIR}/vapi/config.vapi
            ${CMAKE_SOURCE_DIR}/vapi/webkit2gtk-web-extension-4.0.vapi
            ${CMAKE_SOURCE_DIR}/vapi/javascriptcoregtk-4.0.vapi
        )

        add_library(${UNIT} MODULE ${UNIT_SRC_C})
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS "${VALA_CFLAGS}"
                              )
    else()
        string(REPLACE ".c" "" UNIT ${UNIT_SRC})
        add_library(${UNIT} MODULE ${UNIT_SRC})
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS ${CFLAGS}
                              )
    endif()
    target_link_libraries(${UNIT}
                          ${DEPS_LIBRARIES}
                          ${DEPS_GTK_LIBRARIES}
                          )
    install(TARGETS ${UNIT}
            LIBRARY DESTINATION ${PLUGINDIR}
            )
endforeach ()
