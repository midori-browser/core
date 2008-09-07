#! /usr/bin/env python
# WAF build script for midori

import Params
import pproc as subprocess
import Common
import platform

APPNAME = 'midori'
VERSION = '0.0.21'

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
            nls = 'yes'
            conf.define ('ENABLE_NLS', 1)
            conf.define ('MIDORI_LOCALEDIR', 'LOCALEDIR', 0)
        else:
            nls = 'not available'
    else:
        nls = 'no'
    conf.check_message_custom ('localization', 'support', nls)

    if Params.g_options.enable_update_po:
        conf.find_program ('intltool-update', var='INTLTOOL_UPDATE')
        if conf.env['INTLTOOL_UPDATE']:
            update_po = 'yes'
        else:
            update_po = 'not available'
    else:
        update_po = 'no'
    conf.check_message_custom ('localization file', 'updates', update_po)

    if Params.g_options.enable_api_docs:
        conf.find_program ('gtkdoc-scan', var='GTKDOC_SCAN')
        conf.find_program ('gtkdoc-mktmpl', var='GTKDOC_MKTMPL')
        conf.find_program ('gtkdoc-mkdb', var='GTKDOC_MKDB')
        conf.find_program ('gtkdoc-mkhtml', var='GTKDOC_MKHTML')
        if conf.env['GTKDOC_SCAN'] and conf.env['GTKDOC_MKTMPL'] \
            and conf.env['GTKDOC_MKDB'] and conf.env['GTKDOC_MKHTML']:
            api_docs = 'yes'
        else:
            api_docs = 'not available'
    else:
        api_docs = 'no'
    conf.check_message_custom ('generate', 'API documentation', api_docs)

    if not Params.g_options.disable_unique:
        conf.check_pkg ('unique-1.0', destvar='UNIQUE', vnum='0.9', mandatory=False)
        single_instance = ['not available','yes'][conf.env['HAVE_UNIQUE'] == 1]
    else:
        single_instance = 'no'
    conf.check_message_custom ('single instance', 'support', single_instance)

    if not Params.g_options.disable_gio:
        conf.check_pkg ('gio-2.0', destvar='GIO', vnum='2.16.0', mandatory=False)
        gio = ['not available','yes'][conf.env['HAVE_GIO'] == 1]
    else:
        gio = 'no'
    conf.check_message_custom ('GIO', 'support', gio)

    if gio == 'yes':
        if platform.system () != 'Windows':
            if not conf.find_program ('gvfs-open'):
                print '\tNote: There doesn\'t seem to be GVfs installed.'
                print '\t      The HTTP backend of GVfs is recommended for'
                print '\t      viewing source code and loading favicons.'

    conf.check_pkg ('gtk+-2.0', destvar='GTK', vnum='2.6.0', mandatory=True)
    conf.check_pkg ('gtksourceview-2.0', destvar='GTKSOURCEVIEW', vnum='2.0', mandatory=False)
    conf.check_pkg ('webkit-1.0', destvar='WEBKIT', vnum='0.1', mandatory=True)
    conf.check_pkg ('libxml-2.0', destvar='LIBXML', vnum='2.6', mandatory=True)

    conf.check_header ('unistd.h', 'HAVE_UNISTD_H')

    if conf.find_program ('rsvg-convert', var='RSVG_CONVERT'):
        icons = 'yes'
    else:
        icons = 'no'
    conf.check_message_custom ('icon optimization', 'support', icons)

    conf.define ('PACKAGE_VERSION', VERSION)
    conf.define ('PACKAGE_NAME', APPNAME)
    conf.define ('PACKAGE_BUGREPORT', 'http://www.twotoasts.de/bugs')
    conf.define ('GETTEXT_PACKAGE', APPNAME)

    conf.write_config_header ('config.h')
    conf.env.append_value ('CCFLAGS', '-DHAVE_CONFIG_H')

def set_options (opt):
    opt.tool_options ('compiler_cc')
    opt.tool_options ('intltool')

    opt.add_option ('--disable-nls', action='store_true', default=False,
        help='Disables native language support', dest='disable_nls')

    opt.add_option ('--disable-unique', action='store_true', default=False,
        help='Disables Unique support', dest='disable_unique')
    opt.add_option ('--disable-gio', action='store_true', default=False,
        help='Disables GIO support', dest='disable_gio')

    opt.add_option ('--enable-update-po', action='store_true', default=False,
        help='Enables localization file updates', dest='enable_update_po')
    opt.add_option ('--enable-api-docs', action='store_true', default=False,
        help='Enables API documentation', dest='enable_api_docs')

def build (bld):
    bld.add_subdirs ('katze midori icons')

    if bld.env ()['INTLTOOL']:
        bld.add_subdirs ('po')

    if bld.env ()['GTKDOC_SCAN'] and Params.g_commands['build']:
        bld.add_subdirs ('docs/api')

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
                uic = subprocess.Popen (['gtk-update-icon-cache', '-q', '-f', '-t', dir],
                                  stderr=subprocess.PIPE)
                if not uic.wait ():
                    Params.pprint ('YELLOW', "Updated Gtk icon cache.")
                    icon_cache_updated = True
            except:
                Params.pprint ('RED', "Failed to update icon cache.")
        if not icon_cache_updated:
            Params.pprint ('YELLOW', "Icon cache not updated. After install, run this:")
            Params.pprint ('YELLOW', "gtk-update-icon-cache -q -f -t %s" % dir)
