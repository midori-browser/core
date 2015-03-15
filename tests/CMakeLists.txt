# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

include(ContainTest)
add_custom_target(check COMMAND "env" "CTEST_OUTPUT_ON_FAILURE=1" "${CMAKE_CTEST_COMMAND}")

include_directories(
                    "${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/midori"
                    ${DEPS_INCLUDE_DIRS}
                    ${OPTS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    ${CMAKE_BINARY_DIR}
                    "${CMAKE_BINARY_DIR}/midori"
                    )
file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.vala)
foreach(UNIT_SRC ${TESTS})
    if (${UNIT_SRC} MATCHES "(.vala)$")
        string(REPLACE ".vala" "" UNIT ${UNIT_SRC})
        include(ValaPrecompile)
        vala_precompile(UNIT_SRC_C ${UNIT}
            ${UNIT_SRC}
        PACKAGES
            ${PKGS}
        OPTIONS
            ${VALAFLAGS}
        CUSTOM_VAPIS
            ${EXTRA_VAPIS}
            "${CMAKE_SOURCE_DIR}/midori/midori.vapi"
            "${CMAKE_BINARY_DIR}/midori/${LIBMIDORI}.vapi"
        )

        add_executable(${UNIT} ${UNIT_SRC_C})
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS "${VALA_CFLAGS}"
                              )
    else()
        string(REPLACE ".c" "" UNIT ${UNIT_SRC})
        add_executable(${UNIT} ${UNIT_SRC})
        set_target_properties(${UNIT} PROPERTIES
                              COMPILE_FLAGS ${CFLAGS}
                              )
    endif()

    target_link_libraries(${UNIT}
                          ${LIBMIDORI}
                          )
    add_test(NAME ${UNIT} COMMAND ${UNIT})
    contain_test (${UNIT} ${UNIT})
endforeach ()

file(GLOB TESTS ${CMAKE_BINARY_DIR}/extensions/lib*.so)
foreach(UNIT ${TESTS})
    string(REPLACE ${CMAKE_BINARY_DIR} "" TEST ${UNIT})
    string(REPLACE "/" "-" TEST ${TEST})
    string(REPLACE "-extensions-lib" "" TEST ${TEST})
    string(REPLACE ".so" "" TEST ${TEST})
    add_test(NAME ${TEST} COMMAND $<TARGET_FILE:extensions> -e ${UNIT})
    set(exe $<TARGET_FILE:extensions> -e ${UNIT})
    contain_test (${TEST} "${exe}")
endforeach()

file(GLOB TESTS *.sh)
foreach(UNIT_SRC ${TESTS})
    string(REPLACE ".sh" "" UNIT ${UNIT_SRC})
    add_test(NAME ${UNIT} COMMAND ${UNIT_SRC})
    file(RELATIVE_PATH BLDDIR ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    set_tests_properties(${UNIT} PROPERTIES
                         ENVIRONMENT "SRCDIR=${CMAKE_SOURCE_DIR};BLDDIR=${BLDDIR};"
                         )
endforeach ()
