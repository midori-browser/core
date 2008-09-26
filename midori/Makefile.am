INCLUDES = \
    $(UNIQUE_CFLAGS)        \
    $(GIO_CFLAGS)           \
    $(GTK_CFLAGS)           \
    $(GTKSOURCEVIEW_CFLAGS) \
    $(WEBKIT_CFLAGS)        \
    -I../katze

AM_CFLAGS = -DMIDORI_LOCALEDIR=\""$(localedir)"\"

LDADD = \
    $(UNIQUE_LIBS)       \
    $(GIO_LIBS)          \
    $(GTK_LIBS)          \
    $(GTKSOURCEVIEW_LIBS)\
    $(WEBKIT_LIBS)       \
    $(INTLLIBS)          \
    ../katze/libkatze.la

bin_PROGRAMS = \
    midori

midori_SOURCES = \
    main.c                 midori-stock.h         \
    gtkiconentry.c         gtkiconentry.h         \
    midori-app.c           midori-app.h           \
    midori-browser.c       midori-browser.h       \
    midori-panel.c         midori-panel.h         \
    midori-addons.c        midori-addons.h        \
    midori-console.c       midori-console.h       \
    midori-view.c          midori-view.h          \
    midori-source.c        midori-source.h        \
    midori-websettings.c   midori-websettings.h   \
    midori-preferences.c   midori-preferences.h   \
    midori-searchentry.c   midori-searchentry.h   \
    midori-locationentry.c midori-locationentry.h \
    midori-locationaction.c midori-locationaction.h \
    gjs.c       gjs.h       \
    sokoke.c    sokoke.h    \
    compat.c    compat.h
