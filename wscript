#! /usr/bin/env python
# WAF build script for midori

import Params
import subprocess

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
        # FIXME if we have intltool but not msgfmt the build breaks
        if conf.env['INTLTOOL']:
            nls = 'yes'
            conf.define ('ENABLE_NLS', 1)
            conf.define ('MIDORI_LOCALEDIR', 'LOCALEDIR', 0)
        else:
            nls = 'not available'
    else:
        nls = 'no'
    conf.check_message_custom ('localization', 'support', nls)

    conf.check_pkg ('gtk+-2.0', destvar='GTK', vnum='2.6.0')
    conf.check_pkg ('webkit-1.0', destvar='WEBKIT', vnum='0.1')
    conf.check_pkg ('libxml-2.0', destvar='LIBXML', vnum='2.6')
    conf.check_pkg ('libsexy', destvar='LIBSEXY', vnum='0.1')

    if conf.find_program ('convert', var='CONVERT'):
        icons = 'yes'
    else:
        icons = 'no'
    conf.check_message_custom ('icon optimization', 'support', icons)

    # FIXME we need numbers
    conf.define ('GTK_VER', '-')
    conf.define ('WEBKIT_VER', '-')
    conf.define ('LIBXML_VER', '-')
    conf.define ('LIBSEXY_VER', '-')

    conf.define ('PACKAGE_VERSION', VERSION)
    conf.define ('PACKAGE_NAME', APPNAME)
    conf.define ('PACKAGE_BUGREPORT', 'christian@twotoasts.de')
    conf.define ('GETTEXT_PACKAGE', APPNAME)

    conf.define ('SOKOKE_DEBUG_', '-')
    conf.write_config_header ('config.h')

def set_options(opt):
    opt.tool_options ('compiler_cc')
    opt.tool_options ('intltool')

    opt.add_option ('--disable-nls', action='store_true', default=False,
        help='Disables native language support', dest='disable_nls')

def build (bld):
    bld.add_subdirs ('katze src data')

    if bld.env ()['INTLTOOL']:
        bld.add_subdirs ('po')

    if bld.env ()['INTLTOOL']:
        obj = bld.create_obj ('intltool_in')
        obj.source  = 'midori.desktop.in'
        obj.destvar = 'PREFIX'
        obj.subdir  = 'share/applications'
        obj.flags   = '-d'
    else:
        # FIXME: process desktop.in without intltool
        Params.pprint ('BLUE', "File midori.desktop not generated")
    if bld.env ()['INTLTOOL']:
        install_files ('DATADIR', 'applications', 'midori.desktop')
