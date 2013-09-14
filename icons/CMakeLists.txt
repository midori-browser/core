# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

include(FindConvert)
if (NOT CONVERT_FOUND)
    message(FATAL_ERROR "rsvg-convert not found")
endif ()

macro(install_icon name cat size)
    file(GLOB FILENAME RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${size}/${name}.*")
    string(FIND ${FILENAME} ".svg" EXTENSION)
    if (EXTENSION GREATER -1)
        string(SUBSTRING ${FILENAME} 0 ${EXTENSION} BASENAME)
        SVG2PNG(${BASENAME} ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/${cat})
    else ()
        install(FILES ${FILENAME} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/${cat})
    endif ()
endmacro()

foreach(SIZE 16x16 22x22 scalable)
    install_icon(midori apps ${SIZE})
    install_icon(extension categories ${SIZE})
    install_icon(internet-news-reader status ${SIZE})
endforeach()
