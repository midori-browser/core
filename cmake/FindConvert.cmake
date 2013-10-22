# Copyright (C) 2013 Christian Dywan
# Copyright (C) 2013 Olivier Duchateau

find_program (RSVG_CONVERT rsvg-convert)

if (RSVG_CONVERT)
    set (CONVERT_FOUND TRUE)
    macro (SVG2PNG filename install_destination)
        string(REPLACE "/" "_" target ${filename})
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${filename}")
        add_custom_target ("${target}.png" ALL
            ${RSVG_CONVERT} --keep-aspect-ratio --format=png "${CMAKE_CURRENT_SOURCE_DIR}/${filename}.svg"
                --output "${CMAKE_CURRENT_BINARY_DIR}/${filename}.png"
        )
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${filename}.png"
            DESTINATION ${install_destination})
    endmacro (SVG2PNG filename)
endif ()

