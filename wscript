#! /usr/bin/env python
# WAF build script for midori
# This file is licensed under the terms of the expat license, see the file EXPAT.

import sys

# Waf version check, for global waf installs
try:
    from Constants import WAFVERSION
except:
    WAFVERSION='1.0.0'
if WAFVERSION[:3] != '1.5':
    print 'Incompatible Waf, use 1.5'
    sys.exit (1)

import Build
import Options
import Utils
import pproc as subprocess
import os
import UnitTest

major = 0
minor = 1
micro = 7

APPNAME = 'midori'
VERSION = str (major) + '.' + str (minor) + '.' + str (micro)

try:
    git = Utils.cmd_output (['git', 'rev-parse', '--short', 'HEAD'], silent=True)
    if git:
        VERSION = (VERSION + '-' + git).strip ()
except:
    pass

srcdir = '.'
blddir = '_build_'

def option_enabled (option):
    if getattr (Options.options, 'enable_' + option):
        return True
    if getattr (Options.options, 'disable_' + option):
        return False
    return True

def is_mingw (env):
    if 'CC' in env:
        cc = env['CC']
        if not isinstance (cc, str):
            cc = ''.join (cc)
        return cc.find ('mingw') != -1# or cc.find ('wine') != -1
    return False

def configure (conf):
    def option_checkfatal (option, desc):
        if hasattr (Options.options, 'enable_' + option):
            if getattr (Options.options, 'enable_' + option):
                Utils.pprint ('RED', desc + ' N/A')
                sys.exit (1)

    def dirname_default (dirname, default, defname=None):
        if getattr (Options.options, dirname) == '':
            dirvalue = default
        else:
            dirvalue = getattr (Options.options, dirname)
        if not defname:
            defname = dirname
        conf.define (defname, dirvalue)
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
            user_docs = 'N/A'
    else:
        user_docs = 'no '

    if option_enabled ('nls'):
        conf.check_tool ('intltool')
        if conf.env['INTLTOOL'] and conf.env['POCOM']:
            nls = 'yes'
        else:
            option_checkfatal ('nls', 'localization')
            nls = 'N/A'
    else:
        nls = 'no '
    conf.define ('ENABLE_NLS', [0,1][nls == 'yes'])

    # This is specific to cross compiling with mingw
    if is_mingw (conf.env) and Options.platform != 'win32':
        if not 'AR' in os.environ and not 'RANLIB' in os.environ:
            conf.env['AR'] = os.environ['CC'][:-3] + 'ar'
        Options.platform = 'win32'
        # Make sure we don't have -fPIC in the CCFLAGS
        conf.env["shlib_CCFLAGS"] = []
        # Adjust file naming
        conf.env["shlib_PATTERN"] = 'lib%s.dll'
        conf.env['program_PATTERN'] = '%s.exe'
        # Use Visual C++ compatible alignment
        conf.env.append_value ('CCFLAGS', '-mms-bitfields')
        conf.env['staticlib_LINKFLAGS'] = []

        Utils.pprint ('BLUE', 'Mingw recognized, assuming cross compile.')

    dirname_default ('LIBDIR', os.path.join (conf.env['PREFIX'], 'lib'))
    if conf.env['PREFIX'] == '/usr':
        dirname_default ('SYSCONFDIR', '/etc')
    else:
        dirname_default ('SYSCONFDIR', os.path.join (conf.env['PREFIX'], 'etc'))
    dirname_default ('DATADIR', os.path.join (conf.env['PREFIX'], 'share'),
    # Use MDATADIR because DATADIR is a constant in objidl.h on Windows
        'MDATADIR')
    conf.undefine ('DATADIR')
    dirname_default ('DOCDIR', os.path.join (conf.env['MDATADIR'], 'doc'))

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
            api_docs = 'N/A'
    else:
        api_docs = 'no '

    def check_pkg (name, version='', mandatory=True, var=None, args=''):
        if not var:
            var = name.split ('-')[0].upper ()
        conf.check_cfg (package=name, uselib_store=var, args='--cflags --libs ' + args,
            atleast_version=version, mandatory=mandatory)
        return conf.env['HAVE_' + var]

    if option_enabled ('unique'):
        check_pkg ('unique-1.0', '0.9', False)
        unique = ['N/A', 'yes'][conf.env['HAVE_UNIQUE'] == 1]
        if unique != 'yes':
            option_checkfatal ('unique', 'single instance')
    else:
        unique = 'no '
    conf.define ('HAVE_UNIQUE', [0,1][unique == 'yes'])

    if option_enabled ('libidn'):
        check_pkg ('libidn', '1.0', False)
        libidn = ['N/A','yes'][conf.env['HAVE_LIBIDN'] == 1]
        if libidn != 'yes':
            option_checkfatal ('libidn', 'international domain names')
    else:
        libidn = 'no '
    conf.define ('HAVE_LIBIDN', [0,1][libidn == 'yes'])

    if option_enabled ('sqlite'):
        check_pkg ('sqlite3', '3.0', False, var='SQLITE')
        sqlite = ['N/A','yes'][conf.env['HAVE_SQLITE'] == 1]
        if sqlite != 'yes':
            option_checkfatal ('sqlite', 'history database')
    else:
        sqlite = 'no '
    conf.define ('HAVE_SQLITE', [0,1][sqlite == 'yes'])

    conf.check (lib='m', mandatory=True)
    check_pkg ('gmodule-2.0', '2.8.0', False)
    check_pkg ('gthread-2.0', '2.8.0', False)
    check_pkg ('gio-2.0', '2.16.0')
    args = ''
    if Options.platform == 'win32':
        args = '--define-variable=target=win32'
    check_pkg ('gtk+-2.0', '2.10.0', var='GTK', args=args)
    check_pkg ('webkit-1.0', '1.1.1', args=args)
    conf.define ('HAVE_JSCORE', [0,1][not is_mingw (conf.env)])
    check_pkg ('libsoup-2.4', '2.25.2')
    conf.define ('HAVE_LIBSOUP_2_25_2', 1)
    check_pkg ('libxml-2.0', '2.6')

    if option_enabled ('hildon'):
        if check_pkg ('hildon-1', mandatory=False, var='HILDON'):
            check_pkg ('libosso', var='HILDON')
        hildon = ['N/A','yes'][conf.env['HAVE_HILDON'] == 1]
        if hildon != 'yes':
            option_checkfatal ('hildon', 'Maemo integration')
    else:
        hildon = 'no '
    conf.define ('HAVE_HILDON', [0,1][hildon == 'yes'])

    # Store options in env, since 'Options' is not persistent
    if 'CC' in os.environ: conf.env['CC'] = os.environ['CC'].split()
    conf.env['addons'] = option_enabled ('addons')
    conf.env['docs'] = option_enabled ('docs')

    conf.check (header_name='unistd.h')
    if not conf.env['HAVE_UNIQUE']:
        if Options.platform == 'win32':
            conf.check (lib='ws2_32')
        check_pkg ('openssl', mandatory=False)
        conf.define ('USE_SSL', [0,1][conf.env['HAVE_OPENSSL'] == 1])
        conf.define ('HAVE_NETDB_H', [0,1][conf.check (header_name='netdb.h')])
        conf.check (header_name='sys/wait.h')
        conf.check (header_name='sys/select.h')
        conf.check (function_name='inet_aton')
        conf.check (function_name='inet_addr')
    conf.define ('HAVE_OSX', int(sys.platform == 'darwin'))

    if conf.find_program ('rsvg-convert', var='RSVG_CONVERT'):
        icons = 'yes'
    else:
        icons = 'no '

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
    if compiler == 'gcc':
        if debug_level == 'debug':
            conf.env.append_value ('CCFLAGS', '-Wall -O0 -g'.split ())
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
                '-DG_ENABLE_DEBUG'.split ())
    elif debug_level != 'none':
            Utils.pprint ('RED', 'No debugging level support for ' + compiler)
            sys.exit (1)

    print '''
        Localization:        %(nls)s (intltool)
        Icon optimizations:  %(icons)s (rsvg-convert)
        Persistent history:  %(sqlite)s (sqlite3)

        IDN support:         %(libidn)s (libidn)
        User documentation:  %(user_docs)s (docutils)
        API documentation:   %(api_docs)s (gtk-doc)
        Maemo integration:   %(hildon)s (hildon)
        ''' % locals ()
    if unique == 'yes' and conf.check_cfg (modversion='unique-1.0') == '1.0.4':
        Utils.pprint ('RED', 'unique 1.0.4 found, this version is erroneous.')
        Utils.pprint ('RED', 'Please use an older or newer version.')

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
    add_enable_option ('libidn', 'international domain name support', group)
    add_enable_option ('sqlite', 'history database support', group)
    add_enable_option ('addons', 'building of extensions', group)
    add_enable_option ('hildon', 'Maemo integration', group)

def build (bld):
    bld.add_subdirs ('katze midori icons')

    if bld.env['addons']:
        bld.add_subdirs ('extensions')

    bld.add_group ()

    if bld.env['docs']:
        bld.install_files ('${DOCDIR}/' + APPNAME + '/', \
            'AUTHORS ChangeLog COPYING EXPAT README TRANSLATE')

    # Install default configuration
    bld.install_files ('${SYSCONFDIR}/xdg/' + APPNAME + '/', 'data/search')

    if bld.env['RST2HTML']:
        # FIXME: Build only if needed
        Utils.check_dir (blddir)
        Utils.check_dir (blddir + '/docs')
        Utils.check_dir (blddir + '/docs/user')
        os.chdir (blddir + '/docs/user')
        command = bld.env['RST2HTML'] + ' -stg ' + \
            '--stylesheet=../../../docs/user/midori.css ' + \
            '../../../docs/user/midori.txt ' + 'midori.html'
        Utils.exec_command (command)
        os.chdir ('../../..')
        bld.install_files ('${DOCDIR}/midori/user/', blddir + '/docs/user/midori.html')

    if bld.env['INTLTOOL']:
        obj = bld.new_task_gen ('intltool_po')
        obj.podir = 'po'
        obj.appname = APPNAME

    if bld.env['GTKDOC_SCAN'] and Options.commands['build']:
        bld.add_subdirs ('docs/api')
        bld.install_files ('${DOCDIR}/midori/api/', blddir + '/docs/api/*')

    if bld.env['HAVE_HILDON']:
        appdir = '${MDATADIR}/applications/hildon'
        bld.install_files ('${MDATADIR}/dbus-1/services',
                           'data/com.nokia.' + APPNAME + '.service')
    else:
        appdir = '${MDATADIR}/applications'
    if bld.env['INTLTOOL']:
        obj = bld.new_task_gen ('intltool_in')
        obj.source = 'data/' + APPNAME + '.desktop.in'
        obj.install_path = appdir
        obj.flags  = '-d'
        bld.install_files (appdir, 'data/' + APPNAME + '.desktop')
    else:
        folder = os.path.abspath (blddir + '/default/data')
        Utils.check_dir (folder)
        desktop = APPNAME + '.desktop'
        pre = open ('data/' + desktop + '.in')
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
                bld.install_files (appdir, folder + '/' + desktop)
            except:
                Utils.pprint ('BLUE', 'File ' + desktop + ' not generated')
        finally:
            pre.close ()

    if bld.env['RSVG_CONVERT']:
        Utils.check_dir (blddir + '/data')
        command = bld.env['RSVG_CONVERT'] + \
            ' -o ' + blddir + '/data/logo-shade.png ' + \
            srcdir + '/data/logo-shade.svg'
        if not Utils.exec_command (command):
            bld.install_files ('${MDATADIR}/' + APPNAME + '/res', blddir + '/data/logo-shade.png')
        else:
            Utils.pprint ('BLUE', "logo-shade could not be rasterized.")
    bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/error.html')
    bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/speeddial-head.html')
    bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/speeddial.json')
    bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/mootools.js')

    if Options.commands['check']:
        bld.add_subdirs ('tests')

def shutdown ():
    if Options.commands['install'] or Options.commands['uninstall']:
        dir = Build.bld.get_install_path ('${MDATADIR}/icons/hicolor')
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
        folder = os.path.abspath (blddir + '/default')
        try:
            relfolder = folder
            if not is_mingw (Build.bld.env):
                relfolder = os.path.relpath (folder)
        except:
            pass
        try:
            ext = 'MIDORI_EXTENSION_PATH=' + relfolder + os.sep + 'extensions'
            nls = 'NLSPATH=' + relfolder + os.sep + 'po'
            lang = os.environ['LANG']
            try:
                for lang in os.listdir (folder + os.sep + 'po'):
                    if lang[3:] == 'mo':
                        lang = lang[:-3]
                    else:
                        continue
                    Utils.check_dir (folder + os.sep + 'po' + os.sep + lang)
                    Utils.check_dir (folder + os.sep + 'po' + os.sep + lang + \
                        os.sep + 'LC_MESSAGES')
                    os.symlink (folder + os.sep + 'po' + os.sep + lang + '.mo',
                        folder + os.sep + 'po' + os.sep + lang + os.sep + \
                        'LC_MESSAGES' + os.sep + APPNAME + '.mo')
            except:
                pass
            command = ext + ' ' + nls + ' '
            if is_mingw (Build.bld.env):
                # This works only if everything is installed to that prefix
                os.chdir (Build.bld.env['PREFIX'] + os.sep + 'bin')
                command += ' wine cmd /k "PATH=%PATH%;' + Build.bld.env['PREFIX'] + os.sep + 'bin' + ' && ' + APPNAME + '.exe"'
            else:
                command += ' ' + relfolder + os.sep + APPNAME + os.sep + APPNAME
            print command
            Utils.exec_command (command)
        except Exception, msg:
            Utils.pprint ('RED', "Failed to run application: " + str (msg))
