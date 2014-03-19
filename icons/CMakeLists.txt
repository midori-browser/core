# Copyright (C) 2013-2014 Christian Dywan <christian@twotoasts.de>

file(GLOB SIZES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
list(REMOVE_ITEM SIZES "CMakeLists.txt")
foreach(SIZE ${SIZES})
    file(GLOB ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${SIZE} *)
    foreach(ICON ${ICONS})
        install(FILES ${ICON} DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/icons/${size})
    endforeach()
endforeach()
