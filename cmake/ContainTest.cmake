# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

include(ParseArguments)

macro(contain_test test_name executable)
    parse_arguments(ARGS "test_name;executable" "" ${ARGN})
    set(TEST_ENV "")
    foreach(VARIABLE XDG_CACHE_HOME XDG_CONFIG_HOME XDG_DATA_HOME XDG_RUNTIME_DIR TMPDIR)
        set(CONTAINER "${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/${VARIABLE}")
        set(TEST_ENV "${TEST_ENV}${VARIABLE}=${CONTAINER};")
    endforeach()

    add_dependencies(check contain-${test_name})

    set_tests_properties(${test_name} PROPERTIES
                         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                         TIMEOUT 42
                         ENVIRONMENT "${TEST_ENV}"
                         )
    add_custom_target("contain-${test_name}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/XDG_CACHE_HOME
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/XDG_CONFIG_HOME
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/XDG_DATA_HOME
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/XDG_RUNTIME_DIR
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-folders/TMPDIR
        )

    string(REPLACE ${executable} ";" " " executable)
    add_custom_target("gdb-${test_name}"
        COMMAND env ${TEST_ENV} gdb
        --batch -ex 'set print thread-events off'
        -ex 'run' -ex 'bt'
        --args ${executable}
        DEPENDS "contain-${test_name}"
        )

    add_custom_target("valgrind-${test_name}"
        COMMAND env ${TEST_ENV} valgrind
        -q --leak-check=no --num-callers=4
        --show-possibly-lost=no
        --undef-value-errors=yes
        --track-origins=yes
        ${executable}
        DEPENDS "contain-${test_name}"
        )

    add_custom_target("callgrind-${test_name}"
        COMMAND env ${TEST_ENV} valgrind
        --tool=callgrind
        --callgrind-out-file=${UNIT}.callgrind
        ${executable}
        DEPENDS "contain-${test_name}"
        )
endmacro(contain_test)
