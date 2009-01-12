#! /usr/bin/env python
# WAF build script for midori
# This file is licensed under the terms of the expat license, see the file EXPAT.

import Build
import Options
import Utils
import pproc as subprocess
import sys
import os
import UnitTest

major = 0
minor = 1
micro = 1

APPNAME = 'midori'
VERSION = str (major) + '.' + str (minor) + '.' + str (micro)

try:
    git = subprocess.Popen (['git', 'rev-parse', '--short', 'HEAD'],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if not git.wait ():
        VERSION = (VERSION + '-' + git.stdout.read ()).strip ()
except:
    pass

srcdir = '.'
blddir = '_build_'

def option_enabled (option):
    if eval ('Options.options.enable_' + option):
        return True
    if eval ('Options.options.disable_' + option):
        return False
    return True

def configure (conf):
    def option_checkfatal (option, desc):
        if eval ('Options.options.enable_' + option):
                Utils.pprint ('RED', desc + ' not available')
                sys.exit (1)

    def dirname_default (dirname, default):
        if eval ('Options.options.' + dirname) == '':
            dirvalue = default
        else:
            dirvalue = eval ('Options.options.' + dirname)
        conf.define (dirname, dirvalue)
        return dirvalue

    conf.check_tool ('compiler_cc')
    conf.check_tool ('glib2')

    if option_enabled ('userdocs'):
        conf.find_program ('rst2html.py', var='RST2HTML')
        # debian renames the executable, check that as well :(
        if not conf.env['RST2HTML']:
            conf.find_program ('rst2html', var='RST2HTML')
        if conf.env['RST2HTML']:
            user_docs = 'yes'
        else:
            option_checkfatal ('userdocs', 'user documentation')
            user_docs = 'not available'
    else:
        user_docs = 'no'
    conf.check_message_custom ('generate', 'user documentation', user_docs)

    if option_enabled ('nls'):
        conf.check_tool ('intltool')
        if conf.env['INTLTOOL'] and conf.env['POCOM']:
            nls = 'yes'
            conf.define ('ENABLE_NLS', 1)
            conf.define ('MIDORI_LOCALEDIR', 'LOCALEDIR', 0)
        else:
            option_checkfatal ('nls', 'localization')
            nls = 'not available'
    else:
        nls = 'no'
    conf.check_message_custom ('localization', 'support', nls)

    dirname_default ('LIBDIR', os.path.join (conf.env['PREFIX'], 'lib'))
    dirname_default ('DATADIR', os.path.join (conf.env['PREFIX'], 'share'))
    dirname_default ('DOCDIR', os.path.join (conf.env['DATADIR'], 'doc'))

    if option_enabled ('apidocs'):
        conf.find_program ('gtkdoc-scan', var='GTKDOC_SCAN')
        conf.find_program ('gtkdoc-mktmpl', var='GTKDOC_MKTMPL')
        conf.find_program ('gtkdoc-mkdb', var='GTKDOC_MKDB')
        conf.find_program ('gtkdoc-mkhtml', var='GTKDOC_MKHTML')
        if conf.env['GTKDOC_SCAN'] and conf.env['GTKDOC_MKTMPL'] \
            and conf.env['GTKDOC_MKDB'] and conf.env['GTKDOC_MKHTML']:
            api_docs = 'yes'
        else:
            option_checkfatal ('apidocs', 'API documentation')
            api_docs = 'not available'
    else:
        api_docs = 'no'
    conf.check_message_custom ('generate', 'API documentation', api_docs)

    def check_pkg (name, version='', mandatory=True, var=None):
        if not var:
            var = name.split ('-')[0].upper ()
        conf.check_cfg (package=name, uselib_store=var, args='--cflags --libs',
            atleast_version=version, mandatory=mandatory)

    if option_enabled ('unique'):
        check_pkg ('unique-1.0', '0.9', False)
        single_instance = ['not available', 'yes'][conf.env['HAVE_UNIQUE'] == 1]
        if single_instance == 'yes':
            if conf.check_cfg (modversion='unique-1.0') == '1.0.4':
                Utils.pprint ('RED', 'unique 1.0.4 has a fatal bug.')
                Utils.pprint ('RED', 'Please use an older or newer version.')
    else:
        option_checkfatal ('unique', 'single instance')
        conf.define ('HAVE_UNIQUE', 0)
        single_instance = 'no'
    conf.check_message_custom ('single instance', 'support', single_instance)

    if option_enabled ('libsoup'):
        check_pkg ('libsoup-2.4', '2.23.1', False)
        check_pkg ('libsoup-2.4', '2.25.2', False, var='LIBSOUP_2_25_2')
        libsoup = ['not available','yes'][conf.env['HAVE_LIBSOUP'] == 1]
    else:
        option_checkfatal ('libsoup', 'libsoup')
        conf.define ('HAVE_LIBSOUP', 0)
        conf.define ('HAVE_LIBSOUP_2_25_2', 0)
        libsoup = 'no'
    conf.check_message_custom ('libsoup', 'support', libsoup)

    if option_enabled ('sqlite'):
        check_pkg ('sqlite3', '3.0', False, var='SQLITE')
        sqlite = ['not available','yes'][conf.env['HAVE_SQLITE'] == 1]
    else:
        option_checkfatal ('sqlite', 'history database')
        conf.define ('HAVE_SQLITE', 0)
        sqlite = 'no'
    conf.check_message_custom ('history database', 'support', sqlite)

    check_pkg ('gmodule-2.0', '2.8.0', False)
    check_pkg ('gthread-2.0', '2.8.0', False)
    check_pkg ('gio-2.0', '2.16.0', False)
    check_pkg ('gtk+-2.0', '2.10.0', var='GTK')
    check_pkg ('webkit-1.0', '0.1')
    check_pkg ('libxml-2.0', '2.6')

    conf.check (header_name='unistd.h')
    conf.define ('HAVE_OSX', int(sys.platform == 'darwin'))

    if conf.find_program ('rsvg-convert', var='RSVG_CONVERT'):
        icons = 'yes'
    else:
        icons = 'no'
    conf.check_message_custom ('icon optimization', 'support', icons)

    conf.define ('PACKAGE_VERSION', VERSION)
    conf.define ('PACKAGE_NAME', APPNAME)
    conf.define ('PACKAGE_BUGREPORT', 'http://www.twotoasts.de/bugs')
    conf.define ('GETTEXT_PACKAGE', APPNAME)

    conf.define ('MIDORI_MAJOR_VERSION', major)
    conf.define ('MIDORI_MINOR_VERSION', minor)
    conf.define ('MIDORI_MICRO_VERSION', micro)

    conf.write_config_header ('config.h')
    conf.env.append_value ('CCFLAGS', '-DHAVE_CONFIG_H')
    debug_level = Options.options.debug_level
    compiler = conf.env['CC_NAME']
    if debug_level == '':
        if compiler == 'gcc':
            debug_level = 'debug'
        else:
            debug_level = 'none'
    if debug_level != 'none':
        if compiler == 'gcc':
            if debug_level == 'debug':
                conf.env.append_value ('CCFLAGS', '-Wall -O0 -g')
            elif debug_level == 'full':
                # -Wdeclaration-after-statement
                # -Wmissing-declarations -Wmissing-prototypes
                # -Wwrite-strings
                conf.env.append_value ('CCFLAGS',
                    '-Wall -Wextra -O1 -g '
                    '-Waggregate-return -Wno-unused-parameter '
                    '-Wno-missing-field-initializers '
                    '-Wunsafe-loop-optimizations '
                    '-Wredundant-decls -Wmissing-noreturn '
                    '-Wshadow -Wpointer-arith -Wcast-align '
                    '-Winline -Wformat-security '
                    '-Winit-self -Wmissing-include-dirs -Wundef '
                    '-Wmissing-format-attribute -Wnested-externs '
                    '-DG_ENABLE_DEBUG')
            else:
                conf.env.append_value ('CCFLAGS', '-O2')
        else:
            Utils.pprint ('RED', 'No debugging level support for ' + compiler)
            sys.exit (1)

    print
    if single_instance == 'not available':
        print "Single instance support is unavailable in this build."
        print " Install libUnique and reconfigure to enable it."
        print
    if libsoup == 'not available':
        print "Icons, view source and Save as are unavailable in this build."
        print " Install libSoup and reconfigure to enable these features."
        print
    if sqlite == 'not available':
        print "History storage on disk is unavailable in this build."
        print " Install sqlite3 and reconfigure to enable it."
        print
    if user_docs == 'not available':
        print "User documentation is unavailable in this build."
        print " Install docutils and reconfigure to enable it."
        print
    if api_docs == 'not available':
        print "API documentation is unavailable in this build."
        print " Install gtk-doc and reconfigure to enable it."
        print

def set_options (opt):
    def add_enable_option (option, desc, group=None, disable=False):
        if group == None:
            group = opt
        option_ = option.replace ('-', '_')
        group.add_option ('--enable-' + option, action='store_true',
            default=False, help='Enable ' + desc, dest='enable_' + option_)
        group.add_option ('--disable-' + option, action='store_true',
            default=disable, help='Disable ' + desc, dest='disable_' + option_)

    opt.tool_options ('compiler_cc')
    opt.get_option_group ('--check-c-compiler').add_option('-d', '--debug-level',
        action = 'store', default = '',
        help = 'Specify the debugging level. [\'none\', \'debug\', \'full\']',
        choices = ['', 'none', 'debug', 'full'], dest = 'debug_level')
    opt.tool_options ('gnu_dirs')
    opt.parser.remove_option ('--oldincludedir')
    opt.parser.remove_option ('--htmldir')
    opt.parser.remove_option ('--dvidir')
    opt.parser.remove_option ('--pdfdir')
    opt.parser.remove_option ('--psdir')
    opt.tool_options ('intltool')
    opt.add_option ('--run', action='store_true', default=False,
        help='Run application after building it', dest='run')

    group = opt.add_option_group ('Localization and documentation', '')
    add_enable_option ('nls', 'native language support', group)
    group.add_option ('--update-po', action='store_true', default=False,
        help='Update localization files', dest='update_po')
    add_enable_option ('docs', 'informational text files', group)
    add_enable_option ('userdocs', 'user documentation', group)
    add_enable_option ('apidocs', 'API documentation', group, disable=True)

    group = opt.add_option_group ('Optional features', '')
    add_enable_option ('unique', 'single instance support', group)
    add_enable_option ('libsoup', 'icon and view source support', group)
    add_enable_option ('sqlite', 'history database support', group)
    add_enable_option ('addons', 'building of extensions', group)

def build (bld):
    def mkdir (path):
        if not os.access (path, os.F_OK):
            os.mkdir (path)

    def install_files (folder, destination, source):
        try:
            bld.install_files (folder, destination, source)
        except:
            pass

    bld.add_subdirs ('katze midori icons')

    if option_enabled ('addons'):
        bld.add_subdirs ('extensions')

    if option_enabled ('docs'):
        install_files ('DOCDIR', '/' + APPNAME + '/', \
            'AUTHORS ChangeLog COPYING EXPAT README TRANSLATE')

    if bld.env['RST2HTML']:
        # FIXME: Build only if needed
        if not os.access (blddir, os.F_OK):
            os.mkdir (blddir)
        if not os.access (blddir + '/docs', os.F_OK):
            os.mkdir (blddir + '/docs')
        if not os.access (blddir + '/docs/user', os.F_OK):
            os.mkdir (blddir + '/docs/user')
        os.chdir (blddir + '/docs/user')
        command = bld.env['RST2HTML'] + ' -stg ' + \
            '--stylesheet=../../../docs/user/midori.css ' + \
            '../../../docs/user/midori.txt ' + 'midori.html'
        Utils.exec_command (command)
        os.chdir ('../../..')
        install_files ('DOCDIR', '/midori/user/', blddir + '/docs/user/midori.html')

    if bld.env['INTLTOOL']:
        obj = bld.new_task_gen ('intltool_po')
        obj.podir = 'po'
        obj.appname = APPNAME

    if bld.env['GTKDOC_SCAN'] and Options.commands['build']:
        bld.add_subdirs ('docs/api')
        install_files ('DOCDIR', '/midori/api/', blddir + '/docs/api/*')

    if bld.env['INTLTOOL']:
        obj = bld.new_task_gen ('intltool_in')
        obj.source   = APPNAME + '.desktop.in'
        obj.install_path = '${DATADIR}/applications'
        obj.flags    = '-d'
        install_files ('DATADIR', 'applications', APPNAME + '.desktop')
    else:
        folder = os.path.dirname (bld.env['waf_config_files'][0])
        desktop = APPNAME + '.desktop'
        pre = open (desktop + '.in')
        after = open (folder + '/' + desktop, 'w')
        try:
            try:
                for line in pre:
                    if line != '':
                        if line[0] == '_':
                            after.write (line[1:])
                        else:
                            after.write (line)
                after.close ()
                Utils.pprint ('BLUE', desktop + '.in -> ' + desktop)
                install_files ('DATADIR', 'applications', folder + '/' + desktop)
            except:
                Utils.pprint ('BLUE', 'File ' + desktop + ' not generated')
        finally:
            pre.close ()

    if bld.env['RSVG_CONVERT']:
        mkdir (blddir + '/data')
        command = bld.env['RSVG_CONVERT'] + \
            ' -o ' + blddir + '/data/logo-shade.png ' + \
            srcdir + '/data/logo-shade.svg'
        if not Utils.exec_command (command):
            install_files ('DATADIR', APPNAME, blddir + '/data/logo-shade.png')
        else:
            Utils.pprint ('BLUE', "logo-shade could not be rasterized.")

    if Options.commands['check']:
        bld.add_subdirs ('tests')

def shutdown ():
    if Options.commands['install'] or Options.commands['uninstall']:
        dir = Build.bld.get_install_path ('${DATADIR}/icons/hicolor')
        icon_cache_updated = False
        if not Options.options.destdir:
            # update the pixmap cache directory
            try:
                command = 'gtk-update-icon-cache -q -f -t %s' % dir
                if not Utils.exec_command (command):
                    Utils.pprint ('YELLOW', "Updated Gtk icon cache.")
                    icon_cache_updated = True
            except:
                Utils.pprint ('RED', "Failed to update icon cache.")
        if not icon_cache_updated:
            Utils.pprint ('YELLOW', "Icon cache not updated. "
                                     "After install, run this:")
            Utils.pprint ('YELLOW', "gtk-update-icon-cache -q -f -t %s" % dir)

    elif Options.commands['check']:
        test = UnitTest.unit_test ()
        test.change_to_testfile_dir = True
        test.want_to_see_test_output = True
        test.want_to_see_test_error = True
        test.run ()
        test.print_results ()

    elif Options.options.update_po:
        os.chdir('./po')
        try:
            try:
                size_old = os.stat (APPNAME + '.pot').st_size
            except:
                size_old = 0
            subprocess.call (['intltool-update', '-p', '-g', APPNAME])
            size_new = os.stat (APPNAME + '.pot').st_size
            if size_new <> size_old:
                Utils.pprint ('YELLOW', "Updated po template.")
                try:
                    command = 'intltool-update -r -g %s' % APPNAME
                    Utils.exec_command (command)
                    Utils.pprint ('YELLOW', "Updated translations.")
                except:
                    Utils.pprint ('RED', "Failed to update translations.")
        except:
            Utils.pprint ('RED', "Failed to generate po template.")
            Utils.pprint ('RED', "Make sure intltool is installed.")
        os.chdir ('..')
    elif Options.options.run:
        folder = os.path.dirname (Build.bld.env['waf_config_files'][0])
        try:
            command = folder + os.sep + APPNAME + os.sep + APPNAME
            Utils.exec_command (command)
        except:
            Utils.pprint ('RED', "Failed to run application.")
