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
    print ('Incompatible Waf, use 1.5')
    sys.exit (1)

import Build
import Options
import Utils
import pproc as subprocess
import os
try:
    import UnitTest
except:
    import unittestw as UnitTest
import Task
from TaskGen import extension, feature, taskgen
import misc
from Configure import find_program_impl

major = 0
minor = 4
micro = 6

APPNAME = 'midori'
VERSION = VERSION_FULL = str (major) + '.' + str (minor) + '.' + str (micro)
VERSION_SUFFIX = ' (%s)' % VERSION

try:
    if os.path.isdir ('.git'):
        git = Utils.cmd_output (['git', 'describe'], silent=True)
        if git:
            VERSION_FULL = git.strip ()
            VERSION_SUFFIX = VERSION_FULL.replace (VERSION, '')
except:
    pass

srcdir = '.'
blddir = '_build' # recognized by ack

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

def is_win32 (env):
    return is_mingw (env) or Options.platform == 'win32'

# Compile Win32 res files to (resource) object files
def rc_file(self, node):
    rctask = self.create_task ('winrc')
    rctask.set_inputs (node)
    rctask.set_outputs (node.change_ext ('.rc.o'))
    self.compiled_tasks.append (rctask)
rc_file = extension ('.rc')(rc_file)
Task.simple_task_type ('winrc', '${WINRC} -o${TGT} ${SRC}', color='BLUE',
    before='cc cxx', shell=False)

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

    def check_version (given_version, major, minor, micro):
        if '.' in given_version:
            given_major, given_minor, given_micro = given_version.split ('.')
        else:
            given_major, given_minor, given_micro = given_version
        return int(given_major) >  major or \
               int(given_major) == major and int(given_minor) >  minor or \
               int(given_major) == major and int(given_minor) == minor and int(given_micro) >= micro

    conf.check_tool ('compiler_cc')
    conf.check_tool ('vala')
    conf.check_tool ('glib2')
    if not check_version (conf.env['VALAC_VERSION'], 0, 14, 0):
        Utils.pprint ('RED', 'Vala 0.14.0 or later is required.')
        sys.exit (1)

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

    if conf.find_program ('rsvg-convert', var='RSVG_CONVERT'):
        icons = 'yes'
    else:
        icons = 'no '

    if is_win32 (conf.env):
        conf.find_program ('windres', var='WINRC')
        conf.env['platform'] = 'win32'

    # This is specific to cross compiling with mingw
    if is_mingw (conf.env) and Options.platform != 'win32':
        if not 'AR' in os.environ and not 'RANLIB' in os.environ:
            conf.env['AR'] = os.environ['CC'][:-3] + 'ar'
        if conf.find_program (os.environ['CC'][:-3] + 'windres', var='WINRC'):
            os.environ['WINRC'] = os.environ['CC'][:-3] + 'windres'
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
    if not APPNAME in conf.env['DOCDIR']:
        conf.env['DOCDIR'] += '/' + APPNAME
        conf.define ('DOCDIR', conf.env['DOCDIR'])

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
        if option_enabled('gtk3'): unique_pkg = 'unique-3.0'
        else: unique_pkg = 'unique-1.0'
        check_pkg (unique_pkg, '0.9', False)
        unique = ['N/A', 'yes'][conf.env['HAVE_UNIQUE'] == 1]
        if unique != 'yes':
            option_checkfatal ('unique', 'single instance')
            conf.define ('UNIQUE_VERSION', 'No')
        else:
            conf.define ('UNIQUE_VERSION', conf.check_cfg (modversion=unique_pkg))
    else:
        unique = 'no '
        conf.define ('UNIQUE_VERSION', 'No')
    conf.define ('HAVE_UNIQUE', [0,1][unique == 'yes'])

    if option_enabled ('libnotify'):
        check_pkg ('libnotify', mandatory=False)
        libnotify = ['N/A','yes'][conf.env['HAVE_LIBNOTIFY'] == 1]
        if libnotify != 'yes':
            option_checkfatal ('libnotify', 'notifications')
            conf.define ('LIBNOTIFY_VERSION', 'No')
        else:
            conf.define ('LIBNOTIFY_VERSION', conf.check_cfg (modversion='libnotify'))
    else:
        libnotify = 'no '
        conf.define ('LIBNOTIFY_VERSION', 'No')
    conf.define ('HAVE_LIBNOTIFY', [0,1][libnotify == 'yes'])

    if option_enabled ('granite'):
        if not option_enabled ('gtk3'):
            if getattr (Options.options, 'enable_granite'):
                Utils.pprint ('RED', 'Granite requires --enable-gtk3')
                sys.exit (1)
            else:
                granite = 'no (requires --enable-gtk3)'
        else:
            check_pkg ('granite', '0.1', False)
            granite = ['N/A', 'yes'][conf.env['HAVE_GRANITE'] == 1]
        if granite != 'yes':
            option_checkfatal ('granite', 'new notebook, pop-overs')
            conf.define ('GRANITE_VERSION', 'No')
        else:
            conf.define ('GRANITE_VERSION', conf.check_cfg (modversion='granite'))
    else:
        granite = 'no '
        conf.define ('GRANITE_VERSION', 'No')

    conf.check (lib='m', mandatory=True)
    check_pkg ('gmodule-2.0', '2.8.0', False)
    check_pkg ('gthread-2.0', '2.8.0', False)
    check_pkg ('gio-2.0', '2.22.0')
    args = ''
    if Options.platform == 'win32':
        args = '--define-variable=target=win32'
    elif sys.platform != 'darwin':
        check_pkg ('x11')
        # Pass /usr/X11R6/include for OpenBSD
        conf.check (header_name='X11/extensions/scrnsaver.h',
                    includes='/usr/X11R6/include', mandatory=False)
        conf.check (lib='Xss', libpath='/usr/X11R6/lib', mandatory=False)
    if option_enabled ('gtk3'):
        check_pkg ('gtk+-3.0', '3.0.0', var='GTK', mandatory=False)
        check_pkg ('webkitgtk-3.0', '1.1.17', var='WEBKIT', mandatory=False)
        if not conf.env['HAVE_GTK'] or not conf.env['HAVE_WEBKIT']:
            Utils.pprint ('RED', 'GTK+3 was not found.\n' \
                'Pass --disable-gtk3 to build without GTK+3.')
            sys.exit (1)
        if check_version (conf.check_cfg (modversion='webkitgtk-3.0'), 1, 5, 1):
            check_pkg ('javascriptcoregtk-3.0', '1.5.1', args=args)
        conf.env.append_value ('VALAFLAGS', '-D HAVE_GTK3')
    else:
        check_pkg ('gtk+-2.0', '2.10.0', var='GTK')
        check_pkg ('webkit-1.0', '1.1.17', args=args)
        if check_version (conf.check_cfg (modversion='webkit-1.0'), 1, 5, 1):
            check_pkg ('javascriptcoregtk-1.0', '1.5.1', args=args)
    conf.env['HAVE_GTK3'] = option_enabled ('gtk3')
    check_pkg ('libsoup-2.4', '2.27.90')
    conf.define ('LIBSOUP_VERSION', conf.check_cfg (modversion='libsoup-2.4'))
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 29, 3):
        conf.define ('HAVE_LIBSOUP_2_29_3', 1)
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 29, 91):
        conf.define ('HAVE_LIBSOUP_2_29_91', 1)
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 33, 4):
        conf.define ('HAVE_LIBSOUP_2_33_4', 1)
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 37, 1):
        conf.define ('HAVE_LIBSOUP_2_37_1', 1)
    check_pkg ('libxml-2.0', '2.6')
    check_pkg ('sqlite3', '3.0', True, var='SQLITE')

    if option_enabled ('hildon'):
        if check_pkg ('hildon-1', mandatory=False, var='HILDON'):
            check_pkg ('libosso', var='HILDON')
            check_pkg ('hildon-1', '2.2', var='HILDON_2_2')
            check_pkg ('hildon-fm-2', var='HILDON_FM')
        hildon = ['N/A','yes'][conf.env['HAVE_HILDON'] == 1]
        if hildon != 'yes':
            option_checkfatal ('hildon', 'Maemo integration')
    else:
        hildon = 'no '
    conf.define ('HAVE_HILDON', [0,1][hildon == 'yes'])

    # Store options in env, since 'Options' is not persistent
    if 'CC' in os.environ: conf.env['CC'] = os.environ['CC'].split()
    conf.env['addons'] = option_enabled ('addons')
    conf.env['tests'] = option_enabled ('tests')
    conf.env['docs'] = option_enabled ('docs')
    if 'LINGUAS' in os.environ: conf.env['LINGUAS'] = os.environ['LINGUAS']

    conf.check (header_name='unistd.h')
    if not conf.env['HAVE_UNIQUE']:
        if Options.platform == 'win32':
            conf.check (lib='ws2_32')
        conf.define ('HAVE_NETDB_H', [0,1][conf.check (header_name='netdb.h')])
        conf.check (header_name='sys/wait.h')
        conf.check (header_name='sys/select.h')
        conf.check (function_name='inet_aton', header_name='sys/types.h sys/socket.h netinet/in.h arpa/inet.h')
        conf.check (function_name='inet_addr', header_name='sys/types.h sys/socket.h netinet/in.h arpa/inet.h')
    conf.define ('HAVE_OSX', int(sys.platform == 'darwin'))
    if Options.platform == 'win32':
        conf.env.append_value ('LINKFLAGS', '-mwindows')
        conf.env.append_value ('program_LINKFLAGS', ['-Wl,--out-implib=default/midori/libmidori.a', '-Wl,--export-all-symbols'])
    else:
        conf.check (header_name='signal.h')

    conf.define ('PACKAGE_NAME', APPNAME)
    conf.define ('PACKAGE_BUGREPORT', 'https://bugs.launchpad.net/midori')
    conf.define ('GETTEXT_PACKAGE', APPNAME)

    conf.define ('MIDORI_VERSION', VERSION)
    conf.define ('MIDORI_MAJOR_VERSION', major)
    conf.define ('MIDORI_MINOR_VERSION', minor)
    conf.define ('MIDORI_MICRO_VERSION', micro)

    conf.env.append_value ('CCFLAGS', '-DHAVE_CONFIG_H -include config.h'.split ())
    debug_level = Options.options.debug_level
    compiler = conf.env['CC_NAME']
    if debug_level != '' and compiler != 'gcc':
        Utils.pprint ('RED', 'No debugging level support for ' + compiler)
        sys.exit (1)
    elif debug_level == '':
        debug_level = 'debug'

    if debug_level == 'full':
        conf.define ('PACKAGE_VERSION', '%s (debug)' % VERSION_FULL)
        conf.env.append_value ('CCFLAGS', '-DMIDORI_VERSION_SUFFIX="%s (debug)"' % VERSION_SUFFIX)
    else:
        conf.define ('PACKAGE_VERSION', VERSION_FULL)
        conf.env.append_value ('CCFLAGS', '-DMIDORI_VERSION_SUFFIX="%s"' % VERSION_SUFFIX)
    conf.write_config_header ('config.h')

    if compiler == 'gcc':
        if debug_level == 'none':
            if 'CCFLAGS' in os.environ:
                conf.env.append_value ('CCFLAGS', os.environ['CCFLAGS'].split ())
            else:
                conf.env.append_value ('CCFLAGS', '-DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS -DG_DISABLE_ASSERT'.split ())
        elif debug_level == 'debug':
            conf.env.append_value ('CCFLAGS', '-Wall -O0 -g'.split ())
        elif debug_level == 'full':
            # -Wdeclaration-after-statement
            # -Wmissing-declarations -Wmissing-prototypes
            # -Wwrite-strings -Wunsafe-loop-optimizations -Wmissing-include-dirs
            conf.env.append_value ('CCFLAGS',
                '-Wall -Wextra -O1 -g '
                '-Waggregate-return -Wno-unused-parameter '
                '-Wno-missing-field-initializers '
                '-Wredundant-decls -Wmissing-noreturn '
                '-Wshadow -Wpointer-arith -Wcast-align '
                '-Winline -Wformat-security -fno-common '
                '-Winit-self -Wundef -Wdeclaration-after-statement '
                '-Wmissing-format-attribute -Wnested-externs '
            # -DGSEAL_ENABLE
                '-DG_ENABLE_DEBUG -DG_DISABLE_DEPRECATED '
                '-DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED '
                '-DGTK_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED '
                '-DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE'.split ())
    if debug_level == 'full':
        conf.env.append_value ('VALAFLAGS', '--debug --enable-checking'.split ())
    elif debug_level == 'debug':
        conf.env.append_value ('VALAFLAGS', '--debug'.split ())
    elif debug_level == 'none':
        conf.env.append_value ('VALAFLAGS', '--disable-assert')
    conf.env.append_value ('VALAFLAGS', '--enable-deprecated')
    print ('''
        Localization:        %(nls)s (intltool)
        Icon optimizations:  %(icons)s (rsvg-convert)
        Notifications:       %(libnotify)s (libnotify)

        API documentation:   %(api_docs)s (gtk-doc)
        ''' % locals ())
    if unique == 'yes' and conf.check_cfg (modversion='unique-1.0') == '1.0.4':
        Utils.pprint ('RED', 'unique 1.0.4 found, this version is erroneous.')
        Utils.pprint ('RED', 'Please use an older or newer version.')

def set_options (opt):
    def is_maemo (): return os.path.exists ('/etc/osso-af-init/')

    def add_enable_option (option, desc, group=None, disable=False):
        if group == None:
            group = opt
        option_ = option.replace ('-', '_')
        group.add_option ('--enable-' + option, action='store_true', default=False,
            help='Enable ' + desc + ' [Default: ' + str (not disable) + ']',
            dest='enable_' + option_)
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
    add_enable_option ('apidocs', 'API documentation', group, disable=True)

    group = opt.add_option_group ('Optional features', '')
    add_enable_option ('unique', 'single instance support', group, disable=is_win32 (os.environ))
    add_enable_option ('libnotify', 'notification support', group)
    add_enable_option ('granite', 'new notebook, pop-overs', group)
    add_enable_option ('addons', 'building of extensions', group)
    add_enable_option ('tests', 'building of tests', group, disable=True)
    add_enable_option ('hildon', 'Maemo integration', group, disable=not is_maemo ())
    add_enable_option ('gtk3', 'GTK+3 and WebKitGTK+3 support', group, disable=True)

    # Provided for compatibility
    opt.add_option ('--build', help='Ignored')
    opt.add_option ('--disable-maintainer-mode', help='Ignored')

# Taken from Geany's wscript, modified to support LINGUAS variable
def write_linguas_file (self):
    linguas = ''
    # Depending on Waf version, getcwd() is different
    if os.path.exists ('./po'):
        podir = './po'
    else:
        podir = '../po'
    if 'LINGUAS' in Build.bld.env:
        files = Build.bld.env['LINGUAS']
        for f in files.split (' '):
            if os.path.exists (podir + '/' + f + '.po'):
                linguas += f + ' '
    else:
        files = os.listdir (podir)
        for f in files:
            if f.endswith ('.po'):
                linguas += '%s ' % f[:-3]
    f = open (podir + '/LINGUAS', 'w')
    f.write ('# This file is autogenerated. Do not edit.\n%s\n' % linguas)
    f.close ()
write_linguas_file = feature ('intltool_po')(write_linguas_file)

def build (bld):
    bld.add_group ()

    bld.add_subdirs ('midori icons')

    if bld.env['addons']:
        bld.add_subdirs ('extensions')

    bld.add_group ()

    if bld.env['docs']:
        bld.install_files ('${DOCDIR}/', \
            'AUTHORS COPYING ChangeLog EXPAT README data/faq.html data/faq.css')

    # Install default configuration
    bld.install_files ('${SYSCONFDIR}/xdg/' + APPNAME + '/', 'data/search')

    if bld.env['INTLTOOL']:
        obj = bld.new_task_gen ('intltool_po')
        obj.podir = 'po'
        obj.appname = APPNAME

    if bld.env['GTKDOC_SCAN'] and Options.commands['build']:
        bld.add_subdirs ('docs/api')
        bld.install_files ('${DOCDIR}/api/', blddir + '/docs/api/*')

    for desktop in [APPNAME + '.desktop', APPNAME + '-private.desktop']:
        if is_win32 (bld.env):
            break
        if bld.env['HAVE_HILDON']:
            appdir = '${MDATADIR}/applications/hildon'
            bld.install_files ('${MDATADIR}/dbus-1/services',
                               'data/com.nokia.' + APPNAME + '.service')
        else:
            appdir = '${MDATADIR}/applications'
        if bld.env['INTLTOOL']:
            obj = bld.new_task_gen ('intltool_in')
            obj.source = 'data/' + desktop + '.in'
            obj.install_path = appdir
            obj.flags  = ['-d', '-c']
            bld.install_files (appdir, 'data/' + desktop)
        else:
            folder = os.path.abspath (blddir + '/default/data')
            Utils.check_dir (folder)
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

    for res_file in ['about.css', 'error.html', 'close.png']:
        bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/' + res_file)
    bld.install_as ( \
        '${MDATADIR}/' + APPNAME + '/res/speeddial-head-%s.html' % VERSION, \
        'data/speeddial-head.html')

    if bld.env['addons']:
        bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/autosuggestcontrol.js')
        bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/autosuggestcontrol.css')

        if 1:
            extensions = os.listdir ('data/extensions')
            for extension in extensions:
                source = 'data/extensions/' + extension +  '/config'
                if os.path.exists (source):
                    bld.install_files ('${SYSCONFDIR}/xdg/' + APPNAME + \
                                       '/extensions/' + extension, source)

    if Options.commands['check'] or bld.env['tests']:
        bld.add_subdirs ('tests')

    if Options.commands['clean']:
        distclean ()

def check (ctx):
    # The real work happens in shutdown ()
    pass

def distclean ():
    if os.path.exists ('po/LINGUAS'):
        os.remove ('po/LINGUAS')
    if os.path.exists ('po/midori.pot'):
        os.remove ('po/midori.pot')

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
            if size_new != size_old:
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
            nls = 'MIDORI_NLSPATH=' + relfolder + os.sep + 'po'
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
            print (command)
            Utils.exec_command (command)
        except Exception:
            msg = sys.exc_info()[1] # Python 2/3 compatibility
            Utils.pprint ('RED', "Failed to run application: " + str (msg))
