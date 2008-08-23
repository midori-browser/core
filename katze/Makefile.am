INCLUDES = \
    $(GTK_CFLAGS)    \
    $(LIBXML_CFLAGS)

noinst_LTLIBRARIES = \
    libkatze.la

libkatze_la_LIBADD = \
    $(GTK_LIBS)      \
    $(INTLLIBS)      \
    $(LIBXML_LIBS)

libkatze_la_SOURCES = \
    katze.h                           \
    katze-throbber.c katze-throbber.h \
    katze-utils.c    katze-utils.h    \
    katze-item.c     katze-item.h     \
    katze-weblist.c  katze-weblist.h  \
    katze-xbel.c     katze-xbel.h
