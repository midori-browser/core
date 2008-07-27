AUTOMAKE_OPTIONS = gnu

SUBDIRS = katze midori po icons

desktopdir = $(datadir)/applications
desktop_in_files = midori.desktop
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES = \
    midori.desktop \
    intltool-extract \
    intltool-merge \
    intltool-update

EXTRA_DIST = \
    HACKING \
    TODO \
    midori.desktop.in \
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in
