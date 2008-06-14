INCLUDES = \
    $(GTK_CFLAGS)     \
    $(WEBKIT_CFLAGS)  \
    $(LIBSEXY_CFLAGS) \
    -I../katze

AM_CFLAGS = -DMIDORI_LOCALEDIR=\""$(localedir)"\"

LDADD = \
    $(GTK_LIBS)          \
    $(WEBKIT_LIBS)       \
    $(LIBSEXY_LIBS)      \
    $(INTLLIBS)          \
    ../katze/libkatze.la

bin_PROGRAMS = \
    midori

midori_SOURCES = \
    main.c               main.h               \
    midori-webitem.c     midori-webitem.h     \
    midori-weblist.c     midori-weblist.h     \
    midori-app.c         midori-app.h         \
    midori-browser.c     midori-browser.h     \
    midori-panel.c       midori-panel.h       \
    midori-addons.c      midori-addons.h      \
    midori-console.c     midori-console.h     \
    midori-trash.c       midori-trash.h       \
    midori-webview.c     midori-webview.h     \
    midori-websettings.c midori-websettings.h \
    midori-preferences.c midori-preferences.h \
    webSearch.c webSearch.h \
    gjs.c       gjs.h       \
    sokoke.c    sokoke.h    \
    compat.c    compat.h
