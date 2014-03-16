# Copyright (C) 2013 Olivier Duchateau

include (GIR)

if (GIR_FOUND)
    list (APPEND MODULES "Katze" "Midori")
    foreach (MOD ${MODULES})
        string(TOLOWER ${MOD} LOWER_MOD)
        if (EXISTS "${CMAKE_SOURCE_DIR}/${LOWER_MOD}")
            gir (${LOWER_MOD} ${MOD})
        endif ()
    endforeach ()
else ()
    message (FATAL_ERROR "g-ir-scanner not found")
endif ()
