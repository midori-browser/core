#! /usr/bin/env python
# WAF build script for midori

import Params
import pproc as subprocess
import Common

APPNAME = 'midori'
VERSION = '0.0.18'

try:
    git = subprocess.Popen (['git', 'rev-parse', '--short', 'HEAD'],
                            stdout=subprocess.PIPE)
    if not git.wait ():
        VERSION = (VERSION + '-' + git.stdout.read ()).strip ()
except:
    pass

srcdir = '.'
blddir = '_build_'

def configure (conf):
    conf.check_tool ('compiler_cc')

    if not Params.g_options.disable_nls:
        conf.check_tool ('intltool')
        if conf.env['INTLTOOL'] and conf.env['POCOM']:
            conf.find_program ('intltool-update', var='INTLTOOL_UPDATE')
            nls = 'yes'
            conf.define ('ENABLE_NLS', 1)
            conf.define ('MIDORI_LOCALEDIR', 'LOCALEDIR', 0)
        else:
            nls = 'not available'
    else:
        nls = 'no'
    conf.check_message_custom ('localization', 'support', nls)

    conf.check_pkg ('gio-2.0', destvar='GIO', vnum='2.16.0', mandatory=False)
    conf.check_pkg ('gtk+-2.0', destvar='GTK', vnum='2.6.0', mandatory=True)
    conf.check_pkg ('gtksourceview-2.0', destvar='GTKSOURCEVIEW', vnum='2.0', mandatory=False)
    conf.check_pkg ('webkit-1.0', destvar='WEBKIT', vnum='0.1', mandatory=True)
    conf.check_pkg ('libxml-2.0', destvar='LIBXML', vnum='2.6', mandatory=True)

    conf.check_header ('unistd.h', 'HAVE_UNISTD_H')

    if conf.find_program ('convert', var='CONVERT'):
        icons = 'yes'
    else:
        icons = 'no'
    conf.check_message_custom ('icon optimization', 'support', icons)

    conf.define ('PACKAGE_VERSION', VERSION)
    conf.define ('PACKAGE_NAME', APPNAME)
    conf.define ('PACKAGE_BUGREPORT', 'http://software.twotoasts.de/bugs')
    conf.define ('GETTEXT_PACKAGE', APPNAME)

    conf.write_config_header ('config.h')
    conf.env.append_value ('CCFLAGS', '-DHAVE_CONFIG_H')

def set_options (opt):
    opt.tool_options ('compiler_cc')
    opt.tool_options ('intltool')

    opt.add_option ('--disable-nls', action='store_true', default=False,
        help='Disables native language support', dest='disable_nls')

def build (bld):
    bld.add_subdirs ('katze midori data')

    if bld.env ()['INTLTOOL']:
        bld.add_subdirs ('po')

    if bld.env ()['INTLTOOL']:
        obj = bld.create_obj ('intltool_in')
        obj.source   = 'midori.desktop.in'
        obj.inst_var = 'DATADIR'
        obj.inst_dir = 'applications'
        obj.flags    = '-d'
    else:
        # FIXME: process desktop.in without intltool
        Params.pprint ('BLUE', "File midori.desktop not generated")
    if bld.env ()['INTLTOOL']:
        install_files ('DATADIR', 'applications', 'midori.desktop')

def shutdown ():
    if Params.g_commands['install'] or Params.g_commands['uninstall']:
        dir = Common.path_install ('DATADIR', 'icons/hicolor')
        icon_cache_updated = False
        if not Params.g_options.destdir:
            # update the pixmap cache directory
            try:
                subprocess.call (['gtk-update-icon-cache', '-q', '-f', '-t', dir])
                Params.pprint ('YELLOW', "Updated Gtk icon cache.")
                icon_cache_updated = True
            except:
                Params.pprint ('RED', "Failed to update icon cache.")
        if not icon_cache_updated:
            Params.pprint ('YELLOW', "Icon cache not updated. After install, run this:")
            Params.pprint ('YELLOW', "gtk-update-icon-cache -q -f -t %s" % dir)
