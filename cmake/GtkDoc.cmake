# GtkDoc.cmake
#
# Macros for building Midori API documentation.
# Copyright (C) 2013 Olivier Duchateau

find_program (GTKDOC_SCAN_BIN gtkdoc-scan)
find_program (GTKDOC_MKDB_BIN gtkdoc-mkdb)
find_program (GTKDOC_MKHTML_BIN gtkdoc-mkhtml)
find_program (GTKDOC_MKTMPL_BIN gtkdoc-mktmpl)

if (GTKDOC_SCAN_BIN AND GTKDOC_MKTMPL_BIN AND GTKDOC_MKDB_BIN
        AND GTKDOC_MKHTML_BIN)

    set (GTKDOC_FOUND TRUE)

    macro (gtkdoc_build module)
        message("gtkdoc: module ${module}")
        # file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${module}")
        add_custom_target ("gtkdoc-scan_${module}" ALL
            ${GTKDOC_SCAN_BIN} --module=${module}
                --source-dir="${CMAKE_SOURCE_DIR}/${module}"
                --output-dir="${CMAKE_CURRENT_BINARY_DIR}/${module}"
                --rebuild-sections --rebuild-types
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

        add_custom_target ("gtkdoc-tmpl_${module}" ALL
            ${GTKDOC_MKTMPL_BIN} --module=${module}
            --output-dir="${CMAKE_CURRENT_BINARY_DIR}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${module}"
            DEPENDS "gtkdoc-scan_${module}")

        add_custom_target ("gtkdoc-docbook_${module}" ALL
            ${GTKDOC_MKDB_BIN} --module=${module}
                --output-dir="xml"
                --source-dir="${CMAKE_SOURCE_DIR}/${module}"
                --source-suffixes=c,h --output-format=xml
                --default-includes=${module}/${module}.h
                --sgml-mode --main-sgml-file=${module}.sgml
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${module}"
            DEPENDS "gtkdoc-tmpl_${module}")

        # Keep this target alone, otherwise build fails
        add_custom_target ("gtkdoc-html_${module}" ALL
            ${GTKDOC_MKHTML_BIN} ${module}
            "${CMAKE_CURRENT_BINARY_DIR}/${module}/${module}.sgml"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${module}/html"
            DEPENDS "gtkdoc-docbook_${module}")

    endmacro (gtkdoc_build module)

    macro (gtkdoc module)
        file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${module}/html")
        gtkdoc_build (${module})

        set (DOC_DIR "html/midori-${MIDORI_MAJOR_VERSION}-${MIDORI_MINOR_VERSION}")
        install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${module}/html/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/${DOC_DIR}/${module}"
            PATTERN "html/*"
            PATTERN "index.sgml" EXCLUDE)
    endmacro (gtkdoc module)
endif ()
