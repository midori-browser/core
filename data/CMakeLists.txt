# Copyright (C) 2013-2018 Christian Dywan <christian@twotoasts.de>

find_program (XMLLINT_EXECUTABLE xmllint)
if (NOT XMLLINT_EXECUTABLE)
    message(FATAL_ERROR "xmllint not found")
endif ()

include(FindIntltool)
if (NOT INTLTOOL_MERGE_FOUND)
    message(FATAL_ERROR "intltool-merge not found")
elseif (NOT INTLTOOL_UPDATE_FOUND)
    message(FATAL_ERROR "intltool-update not found")
endif ()

file(GLOB_RECURSE DATA_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
list(REMOVE_ITEM DATA_FILES "CMakeLists.txt")

foreach(FILE ${DATA_FILES})
    if (${FILE} MATCHES ".desktop")
        if (NOT WIN32)
            FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} CONTENT)
            # Escape CMake list-separator ";" first
            string(REPLACE ";" "\\;" CONTENT "${CONTENT}")
            string(REPLACE "Icon=${CMAKE_PROJECT_NAME}"
                           "Icon=${DESKTOP_ICON}"
                           CONTENT "${CONTENT}")
            FILE(WRITE ${FILE} ${CONTENT})
            string(REPLACE ".desktop.in" "" DESKTOP_ID ${FILE})
            INTLTOOL_MERGE_DESKTOP (${DESKTOP_ID} po)
        endif ()
    elseif (${FILE} MATCHES ".appdata.xml")
        if (NOT WIN32)
            string(REPLACE ".appdata.xml.in" "" DESKTOP_ID ${FILE})
            INTLTOOL_MERGE_APPDATA (${DESKTOP_ID} po)
        endif ()
    endif()

endforeach()
