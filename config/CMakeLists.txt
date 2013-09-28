# Copyright (C) 2013 Olivier Duchateau

set (SYSCONFDIR ${CMAKE_INSTALL_FULL_SYSCONFDIR})
set (XDG_CONFIG_DIR "xdg/${CMAKE_PROJECT_NAME}")

file (GLOB_RECURSE CONFIG_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
list (REMOVE_ITEM CONFIG_FILES "CMakeLists.txt")

foreach (FILE ${CONFIG_FILES})
    string (FIND ${FILE} "adblock" ADBLOCK_CONF)
    if (ADBLOCK_CONF GREATER -1)
        string (REPLACE "config" "" dirname ${FILE})
        install (FILES ${FILE}
            DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${XDG_CONFIG_DIR}/${dirname}")
        message("${CMAKE_PROJECT_NAME}")
    else ()
        install (FILES ${FILE}
            DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${XDG_CONFIG_DIR}")
        message("${FILE}")
    endif ()
endforeach ()
