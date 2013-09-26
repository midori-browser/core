# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

include(FindGettext)
if (GETTEXT_FOUND)
    set(LANGUAGES $ENV{LINGUAS})
    if (NOT LANGUAGES)
        file(GLOB POTFILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.po")
        string(REPLACE ".po" " " LANGUAGES ${POTFILES})
    endif ()
    message(STATUS "gettext found: ${LANGUAGES}")
    string(REPLACE " " ";" LANGUAGES ${LANGUAGES})
    foreach(LANG ${LANGUAGES})
        GETTEXT_PROCESS_PO_FILES(${LANG} ALL)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LANG}.gmo"
                DESTINATION "${CMAKE_INSTALL_FULL_LOCALEDIR}/${LANG}/LC_MESSAGES"
                RENAME "${GETTEXT_PACKAGE}.mo")
    endforeach ()
else ()
    message(STATUS "gettext not found")
endif ()
