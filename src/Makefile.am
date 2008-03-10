INCLUDES = \
    $(GTK_CFLAGS)     \
    $(WEBKIT_CFLAGS)  \
    $(LIBSEXY_CFLAGS) \
    -I../katze

LDADD = \
    $(GTK_LIBS)          \
    $(WEBKIT_LIBS)       \
    $(LIBSEXY_LIBS)      \
    ../katze/libkatze.la

bin_PROGRAMS = \
    midori

midori_SOURCES = \
    main.c      main.h      \
    midori-browser.c     midori-browser.h     \
    midori-panel.c       midori-panel.h       \
    midori-trash.c       midori-trash.h       \
    midori-webview.c     midori-webview.h     \
    midori-websettings.c midori-websettings.h \
    prefs.c     prefs.h     \
    webSearch.c webSearch.h \
    helpers.c   helpers.h   \
    sokoke.c    sokoke.h    \
    conf.c      conf.h      \
    search.c    search.h    \
    global.h                \
    ui.h                    \
    debug.h
