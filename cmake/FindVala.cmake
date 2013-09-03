# Copyright (C) 2013 Christian Dywan <christian@twotoasts.de>

find_program(VALA_EXECUTABLE NAMES valac)
if (NOT VALA_NOTFOUND)
    execute_process(COMMAND ${VALA_EXECUTABLE} "--version" OUTPUT_VARIABLE "VALA_VERSION")
    string(REPLACE "Vala " "" VALA_VERSION ${VALA_VERSION})
    string(STRIP ${VALA_VERSION} VALA_VERSION)
else ()
    message(FATAL_ERROR "valac not found")
endif ()

macro(vala_require VALA_REQUIRED)
    if (${VALA_VERSION} VERSION_GREATER ${VALA_REQUIRED} OR ${VALA_VERSION} VERSION_EQUAL ${VALA_REQUIRED})
        message(STATUS "valac ${VALA_VERSION} found")
    else ()
        message(FATAL_ERROR "valac >= ${VALA_REQUIRED} or later required")
    endif ()
endmacro(vala_require)
