# FindIntltool.cmake
#
# Jim Nelson <jim@yorba.org>
# Copyright 2012-2013 Yorba Foundation
# Copyright (C) 2013 Christian Dywan

find_program (INTLTOOL_MERGE_EXECUTABLE intltool-merge)

if (INTLTOOL_MERGE_EXECUTABLE)
    set (INTLTOOL_MERGE_FOUND TRUE)
    macro (INTLTOOL_MERGE_DESKTOP desktop_id po_dir)
        add_custom_target ("${desktop_id}.desktop" ALL
            ${INTLTOOL_MERGE_EXECUTABLE} --desktop-style ${CMAKE_SOURCE_DIR}/${po_dir}
                ${CMAKE_CURRENT_SOURCE_DIR}/${desktop_id}.desktop.in ${desktop_id}.desktop
        )
        install (FILES ${CMAKE_CURRENT_BINARY_DIR}/"${desktop_id}.desktop" DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
    endmacro (INTLTOOL_MERGE_DESKTOP desktop_id po_dir)
endif ()
