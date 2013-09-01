# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

include(ParseArguments)

macro(contain_test test_name)
    parse_arguments(ARGS "TEST" "" ${ARGN})
    set(TEST_ENV "")
    foreach(VARIABLE XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_HOME XDG_RUNTIME_DIR TMPDIR)
        set(CONTAINER "${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/${VARIABLE}")
        file(MAKE_DIRECTORY ${CONTAINER})
        set(TEST_ENV "${TEST_ENV}${VARIABLE}=${CONTAINER};")
    endforeach()
    set_tests_properties(${test_name} PROPERTIES
                         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                         TIMEOUT 42
                         ENVIRONMENT "${TEST_ENV}"
                         )
endmacro(contain_test)
