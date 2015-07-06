# GIR.cmake
#
# Macros for building Gobject Introspection bindings for Midori API
find_program (GIR_SCANNER_BIN g-ir-scanner)
find_program (GIR_COMPILER_BIN g-ir-compiler)

if (GIR_SCANNER_BIN AND GIR_COMPILER_BIN)

    set (GIR_FOUND TRUE)
    set (GIR_VERSION "${MIDORI_MAJOR_VERSION}.${MIDORI_MINOR_VERSION}")
    macro (gir_build module namespace)
        add_custom_target ("g-ir-scanner_${module}" ALL
            ${GIR_SCANNER_BIN} -Imidori -I${CMAKE_SOURCE_DIR}/ -I${CMAKE_BINARY_DIR}/midori -I${CMAKE_SOURCE_DIR}/${module} -I${CMAKE_SOURCE_DIR}/toolbars -I.
                --header-only -n ${namespace} --identifier-prefix ${namespace}
                ${CMAKE_SOURCE_DIR}/${module}/${module}-*.c ${CMAKE_SOURCE_DIR}/${module}/${module}-*.h
                --pkg gtk+-2.0 --pkg webkit-1.0 --pkg gio-2.0 --pkg gobject-2.0
                --warn-all -iGObject-2.0 -iGLib-2.0 -iGtk-2.0
                --nsversion ${GIR_VERSION}
                -o ${CMAKE_CURRENT_BINARY_DIR}/${namespace}-${GIR_VERSION}.gir
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                DEPENDS ${CMAKE_PROJECT_NAME})
        add_custom_target ("g-ir-compiler_${module}" ALL
            ${GIR_COMPILER_BIN} ${CMAKE_CURRENT_BINARY_DIR}/${namespace}-${GIR_VERSION}.gir
                --output ${CMAKE_CURRENT_BINARY_DIR}/${namespace}-${GIR_VERSION}.typelib
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                DEPENDS g-ir-scanner_${module})

    endmacro (gir_build module namespace)

    macro (gir module namespace)
        gir_build (${module} ${namespace})

        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${namespace}-${GIR_VERSION}.gir"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gir-1.0/")
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${namespace}-${GIR_VERSION}.typelib"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/girepository-1.0/")
    endmacro (gir module)
endif ()
