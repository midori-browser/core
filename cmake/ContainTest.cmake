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

    add_custom_target("gdb-${test_name}"
        COMMAND env ${TEST_ENV} gdb
        --batch -ex 'set print thread-events off'
        -ex 'run' -ex 'bt'
        ${CMAKE_BINARY_DIR}/tests/${UNIT}
        )

    add_custom_target("valgrind-${test_name}"
        COMMAND env ${TEST_ENV} valgrind
        -q --leak-check=no --num-callers=4
        --show-possibly-lost=no
        --undef-value-errors=yes
        --track-origins=yes
        ${CMAKE_BINARY_DIR}/tests/${UNIT}
        )

    add_custom_target("callgrind-${test_name}"
        COMMAND env ${TEST_ENV} valgrind
        --tool=callgrind
        --callgrind-out-file=${UNIT}.callgrind
        ${CMAKE_BINARY_DIR}/tests/${UNIT}
        )
endmacro(contain_test)
