# Copyright (C) 2013 Olivier Duchateau

include (GtkDoc)

if (GTKDOC_FOUND)
    list (APPEND MODULES "katze" "midori")
    foreach (MOD ${MODULES})
        if (EXISTS "${CMAKE_SOURCE_DIR}/${MOD}")
            gtkdoc (${MOD})
        endif ()
    endforeach ()
else ()
    message (FATAL_ERROR "gtk-doc not found")
endif ()
