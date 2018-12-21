# FindIntltool.cmake
#
# Jim Nelson <jim@yorba.org>
# Copyright 2012-2013 Yorba Foundation
# Copyright (C) 2013-2018 Christian Dywan

find_program (INTLTOOL_MERGE_EXECUTABLE intltool-merge)
if (INTLTOOL_MERGE_EXECUTABLE)
    macro (INTLTOOL_MERGE_DESKTOP_LIKE target po_dir)
        add_custom_target (${target} ALL
            COMMAND ${INTLTOOL_MERGE_EXECUTABLE} --desktop-style ${CMAKE_SOURCE_DIR}/${po_dir}
                ${CMAKE_CURRENT_SOURCE_DIR}/${target}.in ${target}
            DEPENDS ${target}.in
        )
    endmacro (INTLTOOL_MERGE_DESKTOP_LIKE target po_dir)
    macro (INTLTOOL_MERGE_DESKTOP desktop_id po_dir)
        INTLTOOL_MERGE_DESKTOP_LIKE ("${desktop_id}.desktop" ${po_dir})
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${desktop_id}.desktop"
                 DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    endmacro (INTLTOOL_MERGE_DESKTOP desktop_id po_dir)
    macro (INTLTOOL_MERGE_APPDATA desktop_id po_dir)
        add_custom_target (${desktop_id}.appdata.xml ALL
            COMMAND ${INTLTOOL_MERGE_EXECUTABLE} --xml-style ${CMAKE_SOURCE_DIR}/${po_dir}
                ${CMAKE_CURRENT_SOURCE_DIR}/${desktop_id}.appdata.xml.in ${desktop_id}.appdata.xml
            DEPENDS ${desktop_id}.appdata.xml.in
        )
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${desktop_id}.appdata.xml"
                 DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
    endmacro (INTLTOOL_MERGE_APPDATA desktop_id po_dir)
else ()
    message(FATAL_ERROR "intltool-merge not found")
endif ()

find_program (INTLTOOL_UPDATE_EXECUTABLE intltool-update)
if (INTLTOOL_UPDATE_EXECUTABLE)
    add_custom_target (pot
        COMMAND ${INTLTOOL_UPDATE_EXECUTABLE} "-p" "-g" ${GETTEXT_PACKAGE}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/po"
        )
else ()
    message(FATAL_ERROR "intltool-update not found")
endif ()
