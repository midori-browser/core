# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

set(EXTENSIONDIR "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}")
include_directories(
                    "${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/midori"
                    "${CMAKE_SOURCE_DIR}/katze"
                    ${DEPS_INCLUDE_DIRS}
                    ${OPTS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    ${OPTS_GTK_INCLUDE_DIRS}
                    ${CMAKE_BINARY_DIR}
                    "${CMAKE_BINARY_DIR}/midori"
                    )
file(GLOB EXTENSIONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
if (HALF_BRO_INCOM_WEBKIT2)
    list(REMOVE_ITEM EXTENSIONS
         "cookie-permissions"
         "addons.c"
         "formhistory"
         "external-download-manager.vala"
         "nojs"
         "nsplugin-manager.vala"
         )
endif ()

foreach(UNIT_SRC ${EXTENSIONS})
    string(FIND ${UNIT_SRC} ".c" UNIT_EXTENSION)
    if (UNIT_EXTENSION GREATER -1)
        string(REPLACE ".c" "" UNIT ${UNIT_SRC})
        add_library(${UNIT} MODULE ${UNIT_SRC})
        target_link_libraries(${UNIT}
                              ${LIBMIDORI}
                              )
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS ${CFLAGS}
                              )
        install(TARGETS ${UNIT}
            LIBRARY DESTINATION ${EXTENSIONDIR}
                )
    endif ()
endforeach ()

foreach(UNIT_SRC ${EXTENSIONS})
    string(FIND ${UNIT_SRC} "." UNIT_EXTENSION)
    if (UNIT_EXTENSION EQUAL -1)
        file(GLOB UNIT_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${UNIT_SRC}/*.c")
        file(GLOB UNIT_FILES_VALA RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${UNIT_SRC}/*.vala")
        if (UNIT_FILES_VALA)
            include(ValaPrecompile)
            vala_precompile(UNIT_SRC_C ${UNIT_SRC}
                ${UNIT_FILES_VALA}
            PACKAGES
                ${PKGS}
            OPTIONS
                ${VALAFLAGS}
            GENERATE_HEADER
                ${UNIT}
            CUSTOM_VAPIS
                ${EXTRA_VAPIS}
                "${CMAKE_SOURCE_DIR}/midori/midori.vapi"
                "${CMAKE_SOURCE_DIR}/katze/katze.vapi"
                "${CMAKE_BINARY_DIR}/midori/${LIBMIDORI}.vapi"
            )
            set(UNIT_FILES ${UNIT_FILES} ${UNIT_SRC_C})
        endif ()
        if (UNIT_FILES)
            add_library(${UNIT_SRC} MODULE ${UNIT_FILES})
            target_link_libraries(${UNIT_SRC}
                                  ${LIBMIDORI}
                                  )
            set_target_properties(${UNIT_SRC} PROPERTIES
                                  COMPILE_FLAGS ${VALA_CFLAGS}
                                  )
            install(TARGETS ${UNIT_SRC}
                    LIBRARY DESTINATION ${EXTENSIONDIR}
                    )
        endif ()
    endif ()
endforeach ()

foreach(UNIT_SRC ${EXTENSIONS})
    string(FIND ${UNIT_SRC} ".vala" UNIT_EXTENSION)
    if (UNIT_EXTENSION GREATER -1)
        string(REPLACE ".vala" "" UNIT ${UNIT_SRC})
        include(ValaPrecompile)
        vala_precompile(UNIT_SRC_C ${UNIT}
            ${UNIT_SRC}
        PACKAGES
            ${PKGS}
        OPTIONS
            ${VALAFLAGS}
        GENERATE_HEADER
            ${UNIT}
        CUSTOM_VAPIS
            ${EXTRA_VAPIS}
            "${CMAKE_SOURCE_DIR}/midori/midori.vapi"
            "${CMAKE_SOURCE_DIR}/katze/katze.vapi"
            "${CMAKE_BINARY_DIR}/midori/${LIBMIDORI}.vapi"
        )
        add_library(${UNIT} MODULE ${UNIT_SRC_C})
        target_link_libraries(${UNIT}
                              ${LIBMIDORI}
                              )
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS "${VALA_CFLAGS}"
                              )
        install(TARGETS ${UNIT}
                LIBRARY DESTINATION ${EXTENSIONDIR}
                )
    endif ()
endforeach ()
