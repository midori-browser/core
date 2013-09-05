# FindIntltool.cmake
#
# Jim Nelson <jim@yorba.org>
# Copyright 2012-2013 Yorba Foundation
# Copyright (C) 2013 Christian Dywan

find_program (INTLTOOL_MERGE_EXECUTABLE intltool-merge)
find_program (INTLTOOL_UPDATE_EXECUTABLE intltool-update)

if (INTLTOOL_MERGE_EXECUTABLE)
    set (INTLTOOL_MERGE_FOUND TRUE)
    macro (INTLTOOL_MERGE_DESKTOP desktop_id po_dir)
        add_custom_target ("${desktop_id}.desktop" ALL
            ${INTLTOOL_MERGE_EXECUTABLE} --desktop-style ${CMAKE_SOURCE_DIR}/${po_dir}
                ${CMAKE_CURRENT_SOURCE_DIR}/${desktop_id}.desktop.in ${desktop_id}.desktop
        )
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${desktop_id}.desktop"
                 DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    endmacro (INTLTOOL_MERGE_DESKTOP desktop_id po_dir)
    macro (INTLTOOL_MERGE_APPDATA desktop_id po_dir)
        add_custom_target ("${desktop_id}.appdata.xml" ALL
            ${INTLTOOL_MERGE_EXECUTABLE} --xml-style ${CMAKE_SOURCE_DIR}/${po_dir}
                ${CMAKE_CURRENT_SOURCE_DIR}/${desktop_id}.appdata.xml.in ${desktop_id}.appdata.xml
        )
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${desktop_id}.appdata.xml"
                 DESTINATION "${CMAKE_INSTALL_PREFIX}/share/appdata")
         endmacro (INTLTOOL_MERGE_APPDATA desktop_id po_dir)
endif ()

if (INTLTOOL_UPDATE_EXECUTABLE)
    set (INTLTOOL_UPDATE_FOUND TRUE)
    add_custom_target (pot
        COMMAND ${INTLTOOL_UPDATE_EXECUTABLE} "-p" "-g" ${GETTEXT_PACKAGE}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/po"
        )
endif ()


