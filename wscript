#! /usr/bin/env python
# WAF build script for midori
# This file is licensed under the terms of the expat license, see the file EXPAT.

import Params
import pproc as subprocess
import Common
import sys
import os

APPNAME = 'midori'
VERSION = '0.1.0'

try:
    git = subprocess.Popen (['git', 'rev-parse', '--short', 'HEAD'],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if not git.wait ():
        VERSION = (VERSION + '-' + git.stdout.read ()).strip ()
except:
    pass

srcdir = '.'
blddir = '_build_'

def configure (conf):
    conf.check_tool ('compiler_cc')

    if not Params.g_options.disable_docs:
        conf.find_program ('rst2html.py', var='RST2HTML')
        # debian renames the executable, check that as well :(
        if not conf.env['RST2HTML']:
            conf.find_program ('rst2html', var='RST2HTML')
        if conf.env['RST2HTML']:
            docs = 'yes'
        else:
            docs = 'not available'
    else:
        docs = 'no'
    conf.check_message_custom ('generate', 'user documentation', docs)

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

    # We support building without intltool
    # Therefore datadir may not have been defined
    if not conf.is_defined ('DATADIR'):
        if Params.g_options.datadir != '':
            conf.define ('DATADIR', Params.g_options.datadir)
        else:
            conf.define ('DATADIR', os.path.join (conf.env['PREFIX'], 'share'))

    if Params.g_options.docdir == '':
        docdir =  "%s/doc" % conf.env['DATADIR']
    else:
        docdir = Params.g_options.docdir
    conf.define ('DOCDIR', docdir)

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

    if not Params.g_options.disable_libsoup:
        conf.check_pkg ('libsoup-2.4', destvar='LIBSOUP', mandatory=False)
        libsoup = ['not available','yes'][conf.env['HAVE_LIBSOUP'] == 1]
    else:
        libsoup = 'no'
    conf.check_message_custom ('libsoup', 'support', libsoup)

    if not Params.g_options.disable_sqlite:
        conf.check_pkg ('sqlite3', destvar='SQLITE', vnum='3.0', mandatory=False)
        sqlite = ['not available','yes'][conf.env['HAVE_SQLITE'] == 1]
    else:
        sqlite = 'no'
    conf.check_message_custom ('history database', 'support', sqlite)

    conf.check_pkg ('gio-2.0', destvar='GIO', vnum='2.16.0', mandatory=False)
    conf.check_pkg ('gtk+-2.0', destvar='GTK', vnum='2.10.0', mandatory=True)
    conf.check_pkg ('webkit-1.0', destvar='WEBKIT', vnum='0.1', mandatory=True)
    conf.check_pkg ('libxml-2.0', destvar='LIBXML', vnum='2.6', mandatory=True)

    conf.check_header ('unistd.h', 'HAVE_UNISTD_H')
    if sys.platform == 'darwin':
        conf.define ('HAVE_OSX', 1)

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
    opt.add_option ('--docdir', type='string', default='',
        help='documentation root', dest='docdir')
    opt.add_option ('--disable-docs', action='store_true', default=False,
        help='Disables user documentation', dest='disable_docs')

    opt.add_option ('--disable-nls', action='store_true', default=False,
        help='Disables native language support', dest='disable_nls')

    opt.add_option ('--disable-unique', action='store_true', default=False,
        help='Disables Unique support', dest='disable_unique')
    opt.add_option ('--disable-libsoup', action='store_true', default=False,
        help='Disables libsoup support', dest='disable_libsoup')
    opt.add_option ('--disable-sqlite', action='store_true', default=False,
        help='Disables sqlite support', dest='disable_sqlite')

    opt.add_option ('--enable-update-po', action='store_true', default=False,
        help='Enables localization file updates', dest='enable_update_po')
    opt.add_option ('--enable-api-docs', action='store_true', default=False,
        help='Enables API documentation', dest='enable_api_docs')

def build (bld):
    def mkdir (path):
        if not os.access (path, os.F_OK):
            os.mkdir (path)

    def _install_files (folder, destination, source):
        try:
            install_files (folder, destination, source)
        except:
            pass

    bld.add_subdirs ('katze midori icons')

    install_files ('DOCDIR', '/' + APPNAME + '/', \
        'AUTHORS ChangeLog COPYING EXPAT README TRANSLATE')

    if bld.env ()['RST2HTML']:
        # FIXME: Build only if needed
        if not os.access (blddir, os.F_OK):
            os.mkdir (blddir)
        if not os.access (blddir + '/docs', os.F_OK):
            os.mkdir (blddir + '/docs')
        if not os.access (blddir + '/docs/user', os.F_OK):
            os.mkdir (blddir + '/docs/user')
        os.chdir (blddir + '/docs/user')
        subprocess.call ([bld.env ()['RST2HTML'], '-stg',
            '--stylesheet=../../../docs/user/midori.css',
            '../../../docs/user/midori.txt',
            'midori.html',])
        os.chdir ('../../..')
        install_files ('DOCDIR', '/midori/user/', blddir + '/docs/user/midori.html')

    if bld.env ()['INTLTOOL']:
        bld.add_subdirs ('po')

    if bld.env ()['GTKDOC_SCAN'] and Params.g_commands['build']:
        bld.add_subdirs ('docs/api')

    if bld.env ()['INTLTOOL']:
        obj = bld.create_obj ('intltool_in')
        obj.source   = APPNAME + '.desktop.in'
        obj.inst_var = 'DATADIR'
        obj.inst_dir = 'applications'
        obj.flags    = '-d'
    else:
        # FIXME: process desktop.in without intltool
        Params.pprint ('BLUE', "File " + APPNAME + ".desktop not generated")
    if bld.env ()['INTLTOOL']:
        install_files ('DATADIR', 'applications', APPNAME + '.desktop')

    if bld.env ()['RSVG_CONVERT']:
        mkdir (blddir + '/data')
        convert = subprocess.Popen ([bld.env ()['RSVG_CONVERT'],
            '-o', blddir + '/data/logo-shade.png',
            srcdir + '/data/logo-shade.svg'],
            stderr=subprocess.PIPE)
        if not convert.wait ():
            _install_files ('DATADIR', APPNAME,
                            blddir + '/data/logo-shade.png')
        else:
            Params.pprint ('BLUE', "logo-shade could not be rasterized.")

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
