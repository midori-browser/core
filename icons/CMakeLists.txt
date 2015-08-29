# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

if (UNIX)
    EXECUTE_PROCESS(COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/hicolor)
endif ()

macro(install_icon name cat size)
    file(GLOB FILENAME RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${size}/${cat}/${name}.*")
    install(FILES ${FILENAME} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/${cat})
endmacro()

foreach(SIZE 16x16 22x22 24x24 scalable)
    install_icon(midori apps ${SIZE})
    install_icon(extension categories ${SIZE})
    install_icon(internet-news-reader status ${SIZE})
    install_icon(adblock-enabled status ${SIZE})
    install_icon(adblock-blocked status ${SIZE})
    install_icon(adblock-disabled status ${SIZE})
endforeach()

