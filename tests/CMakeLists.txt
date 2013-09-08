# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

include(ContainTest)
add_custom_target(check COMMAND "env" "CTEST_OUTPUT_ON_FAILURE=1" "${CMAKE_CTEST_COMMAND}")

include_directories(
                    "${CMAKE_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/midori"
                    ${DEPS_INCLUDE_DIRS}
                    ${OPTS_INCLUDE_DIRS}
                    ${DEPS_GTK_INCLUDE_DIRS}
                    ${OPTS_GTK_INCLUDE_DIRS}
                    ${CMAKE_BINARY_DIR}
                    "${CMAKE_BINARY_DIR}/midori"
                    )
file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c)
foreach(UNIT_SRC ${TESTS})
    string(REPLACE ".c" "" UNIT ${UNIT_SRC})
    add_executable(${UNIT} ${UNIT_SRC})
    add_test(NAME ${UNIT} COMMAND ${UNIT})
    contain_test(${UNIT})
    target_link_libraries(${UNIT}
                          ${LIBMIDORI}
                          )
    set_target_properties(${UNIT} PROPERTIES
                          COMPILE_FLAGS ${CFLAGS}
                          )
endforeach ()

file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.vala)
foreach(UNIT_SRC ${TESTS})
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
        "${CMAKE_SOURCE_DIR}/katze/katze.vapi"
        "${CMAKE_BINARY_DIR}/midori/${LIBMIDORI}.vapi"
    )
    add_executable(${UNIT} ${UNIT_SRC_C})
    add_test(NAME ${UNIT} COMMAND ${UNIT})
    contain_test(${UNIT})
    target_link_libraries(${UNIT}
                          ${LIBMIDORI}
                          )
    set_target_properties(${UNIT} PROPERTIES
                          COMPILE_FLAGS "${VALA_CFLAGS}"
                          )
endforeach ()

file(GLOB TESTS *.sh)
foreach(UNIT_SRC ${TESTS})
    string(REPLACE ".sh" "" UNIT ${UNIT_SRC})
    add_test(NAME ${UNIT} COMMAND ${UNIT_SRC})
    file(RELATIVE_PATH BLDDIR ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    set_tests_properties(${UNIT} PROPERTIES
                         ENVIRONMENT "SRCDIR=${CMAKE_SOURCE_DIR};BLDDIR=${BLDDIR};"
                         )
endforeach ()
