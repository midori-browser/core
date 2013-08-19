# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

macro(install_icon name cat size)
    file(GLOB FILENAME RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${size}/${name}.*")
    install(FILES ${FILENAME} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/${cat})
endmacro()

foreach(SIZE "16x16;22x22;scalable")
    install_icon(midori apps ${SIZE})
    install_icon(extension categories ${SIZE})
    install_icon(internet-news-reader status ${SIZE})
endforeach()
