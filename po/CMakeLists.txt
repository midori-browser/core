# Copyright (C) 2013-2018 Christian Dywan <christian@twotoasts.de>

include(FindGettext)
if (GETTEXT_FOUND)
    file(GLOB POTFILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.po")
    string(REPLACE ".po" ";" ALL_LANGUAGES ${POTFILES})

    # if LINGUAS is set, we need to filter its values to languages
    # actually supported
    # NB: LINGUAS="" is distinct from unset, and means 'install no l10n'
    if(DEFINED ENV{LINGUAS})
        set(LINGUAS $ENV{LINGUAS})
        string(REPLACE " " ";" LINGUAS "${LINGUAS}")
        set(LANGUAGES)
        foreach(LANG ${LINGUAS})
            list(FIND ALL_LANGUAGES ${LANG} LANG_IDX)
            if(NOT LANG_IDX EQUAL -1)
                list(APPEND LANGUAGES ${LANG})
            endif()
        endforeach()
    else()
        set(LANGUAGES ${ALL_LANGUAGES})
    endif()

    message(STATUS "gettext found: ${LANGUAGES}")
    if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}" VERSION_LESS "2.8.8")
        GETTEXT_CREATE_TRANSLATIONS("${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.pot" ALL ${POTFILES})
    else()
        foreach(LANG ${LANGUAGES})
            GETTEXT_PROCESS_PO_FILES(${LANG} ALL PO_FILES ${LANG}.po)
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LANG}.gmo"
                    DESTINATION "${CMAKE_INSTALL_FULL_LOCALEDIR}/${LANG}/LC_MESSAGES"
                    RENAME "${GETTEXT_PACKAGE}.mo")
            endforeach ()
    endif()
else ()
    message(STATUS "gettext not found")
endif ()
