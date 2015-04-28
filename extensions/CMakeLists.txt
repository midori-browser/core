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
         "tabs2one.c"
         )
endif ()

# FIXME: re-enable webmedia extension
# once we have working notifications on win
if (WIN32)
    list(REMOVE_ITEM EXTENSIONS "webmedia-now-playing.vala")
endif()

# FIXME: not stable enough for release
if (NOT REVISION)
    list(REMOVE_ITEM EXTENSIONS "tabs2one.c")
endif()

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
                --use-header="${CMAKE_PROJECT_NAME}-core.h"
            GENERATE_HEADER
                "${UNIT_SRC}"
            GENERATE_HEADER
                ${UNIT}
            CUSTOM_VAPIS
                ${EXTRA_VAPIS}
                "${CMAKE_SOURCE_DIR}/midori/midori.vapi"
                "${CMAKE_BINARY_DIR}/midori/${LIBMIDORI}.vapi"
            )
            set(UNIT_FILES ${UNIT_FILES} ${UNIT_SRC_C})
        endif ()
        if (UNIT_FILES)
            add_library(${UNIT_SRC} MODULE ${UNIT_FILES})
            target_link_libraries(${UNIT_SRC}
                                  ${LIBMIDORI}
                                  )
            install(TARGETS ${UNIT_SRC}
                    LIBRARY DESTINATION ${EXTENSIONDIR}
                    )
            # extensions with vala code get the lenient VALA_CFLAGS
            # others get the usual CFLAGS with -Wall and -Werror
	    if (UNIT_FILES_VALA)
		set_target_properties(${UNIT_SRC} PROPERTIES
				      COMPILE_FLAGS ${VALA_CFLAGS}
				      )
	    else ()
		set_target_properties(${UNIT_SRC} PROPERTIES
				      COMPILE_FLAGS ${CFLAGS}
				      )
	    endif ()
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
            --use-header="${CMAKE_PROJECT_NAME}-core.h"
        GENERATE_HEADER
            ${UNIT}
        CUSTOM_VAPIS
            ${EXTRA_VAPIS}
            "${CMAKE_SOURCE_DIR}/midori/midori.vapi"
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
