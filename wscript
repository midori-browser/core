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

APPNAME = 'midori'
VERSION = VERSION_FULL = '0.5.2'
VERSION_SUFFIX = ' (%s)' % VERSION

try:
    if os.path.isdir ('.git'):
        git = Utils.cmd_output (['git', 'describe'], silent=True)
        if git:
            VERSION_FULL = git.strip ()
    elif os.path.isdir ('.bzr'):
        bzr = Utils.cmd_output (['bzr', 'revno'], silent=True)
        if bzr:
            VERSION_FULL = '%s~r%s' % (VERSION, bzr.strip ())
    else:
        folder = os.getcwd ()
        if VERSION in folder:
            VERSION_FULL = os.path.basename (folder)
    if VERSION in VERSION_FULL:
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
            given_major, given_minor, given_micro = given_version.split ('.', 2)
            if '.' in given_micro:
                given_micro, given_pico = given_micro.split ('.', 1)
        else:
            given_major, given_minor, given_micro = given_version
        return int(given_major) >  major or \
               int(given_major) == major and int(given_minor) >  minor or \
               int(given_major) == major and int(given_minor) == minor and int(given_micro) >= micro

    conf.check_message_custom ('release version', '', VERSION_FULL)

    conf.check_tool ('compiler_cc')
    conf.check_tool ('vala')
    conf.check_tool ('glib2')
    if not check_version (conf.env['VALAC_VERSION'], 0, 14, 0):
        Utils.pprint ('RED', 'Vala 0.14.0 or later is required.')
        sys.exit (1)

    if option_enabled ('nls'):
        conf.check_tool ('intltool')
        if not conf.env['INTLTOOL'] and conf.env['POCOM']:
            option_checkfatal ('nls', 'localization')
            conf.define ('ENABLE_NLS', 0)
        else:
            conf.define ('ENABLE_NLS', 1)
    else:
        conf.define ('ENABLE_NLS', 0)
        conf.check_message_custom ('nls', '', 'disabled')

    conf.find_program ('rsvg-convert', var='RSVG_CONVERT')

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
            pass
        else:
            option_checkfatal ('apidocs', 'API documentation')
    else:
        conf.check_message_custom ('gtk-doc', '', 'disabled')

    def check_pkg (name, version='', mandatory=True, var=None, args=''):
        if not var:
            var = name.split ('-')[0].upper ()
        ver_str = ['',' >= ' + version][version != '']
        def okmsg_ver (kw):
            return conf.check_cfg (modversion=name, uselibstore=var)
        conf.check_cfg (msg='Checking for ' + name + ver_str, okmsg=okmsg_ver,
            package=name, uselib_store=var, args='--cflags --libs ' + args,
            atleast_version=version, mandatory=mandatory)
        have = conf.env['HAVE_' + var] == 1
        conf.define (var + '_VERSION', ['No', conf.check_cfg (modversion=name,
            uselib_store=var, errmsg=name + ver_str + ' not found')][have])
        return have

    if option_enabled ('libnotify'):
        if not check_pkg ('libnotify', mandatory=False):
            option_checkfatal ('libnotify', 'notifications')
    else:
        conf.define ('LIBNOTIFY_VERSION', 'No')
        conf.check_message_custom ('libnotify', '', 'disabled')
    conf.define ('HAVE_LIBNOTIFY', [0,1][conf.env['LIBNOTIFY_VERSION'] != 'No'])

    if option_enabled ('granite'):
        check_pkg ('granite', '0.2', mandatory=False)
        granite = ['N/A', 'yes'][conf.env['HAVE_GRANITE'] == 1]
        if granite != 'yes':
            option_checkfatal ('granite', 'new notebook, pop-overs')
            conf.define ('GRANITE_VERSION', 'No')
        else:
            conf.env.append_value ('VALAFLAGS', '-D HAVE_GRANITE')
    else:
        conf.define ('GRANITE_VERSION', 'No')
        conf.check_message_custom ('granite', '', 'disabled')

    if option_enabled ('zeitgeist'):
        check_pkg ('zeitgeist-1.0', '0.3.14')
    else:
        conf.check_message_custom ('zeitgeist', '', 'disabled')

    conf.check (lib='m')
    check_pkg ('gmodule-2.0')
    check_pkg ('gio-2.0', '2.22.0')
    if check_version (conf.env['GIO_VERSION'], 2, 30, 0) \
        and check_version (conf.env['VALAC_VERSION'], 0, 16, 0):
        # Older Vala doesn't have GLib 2.30 bindings
        conf.env.append_value ('VALAFLAGS', '-D HAVE_GLIB_2_30')

    args = ''
    if Options.platform == 'win32':
        args = '--define-variable=target=win32'
        conf.env.append_value ('VALAFLAGS', '-D HAVE_WIN32')
    elif sys.platform != 'darwin':
        if sys.platform.startswith ('freebsd'):
            conf.env.append_value ('VALAFLAGS', '-D HAVE_FREEBSD')

        check_pkg ('x11')
        # Pass /usr/X11R6/include for OpenBSD
        conf.check (header_name='X11/extensions/scrnsaver.h',
                    includes='/usr/X11R6/include', mandatory=False)
        conf.check (lib='Xss', libpath='/usr/X11R6/lib', mandatory=False)

    have_gtk3 = option_enabled ('gtk3') or option_enabled ('webkit2') or option_enabled ('granite')
    if have_gtk3:
        check_pkg ('gtk+-3.0', '3.0.0', var='GTK', mandatory=False)
        check_pkg ('gcr-3', '2.32', mandatory=False)
        if option_enabled ('webkit2'):
            # 2.0.0 matches 1.11.91 API; 1.11.92 > 2.0.0
            check_pkg ('webkit2gtk-3.0', '1.11.91', var='WEBKIT', mandatory=False)
            if not conf.env['HAVE_WEBKIT']:
                Utils.pprint ('RED', 'WebKit2/ GTK+3 was not found.\n' \
                    'Pass --disable-webkit2 to build without WebKit2.')
                sys.exit (1)
            conf.define ('HAVE_WEBKIT2', 1)
            conf.env.append_value ('VALAFLAGS', '-D HAVE_WEBKIT2')
        else:
            check_pkg ('webkitgtk-3.0', '1.1.17', var='WEBKIT', mandatory=False)
        if not conf.env['HAVE_GTK'] or not conf.env['HAVE_WEBKIT']:
            Utils.pprint ('RED', 'GTK+3 was not found.\n' \
                'Pass --disable-gtk3 to build without GTK+3.')
            sys.exit (1)
        if check_version (conf.env['WEBKIT_VERSION'], 1, 5, 1):
            check_pkg ('javascriptcoregtk-3.0', '1.5.1', args=args)
        conf.env.append_value ('VALAFLAGS', '-D HAVE_GTK3')
        conf.env.append_value ('VALAFLAGS', '-D HAVE_OFFSCREEN')
        conf.env.append_value ('VALAFLAGS', '-D HAVE_DOM')
    else:
        check_pkg ('gtk+-2.0', '2.16.0', var='GTK')
        check_pkg ('webkit-1.0', '1.1.17', args=args)
        check_pkg ('gcr-3-gtk2', '2.32', mandatory=False)
        if check_version (conf.env['WEBKIT_VERSION'], 1, 5, 1):
            check_pkg ('javascriptcoregtk-1.0', '1.5.1', args=args)
        if check_version (conf.env['GTK_VERSION'], 2, 20, 0):
            conf.env.append_value ('VALAFLAGS', '-D HAVE_OFFSCREEN')
    conf.env['HAVE_GTK3'] = have_gtk3
    conf.env['HAVE_WEBKIT2'] = option_enabled ('webkit2')

    if option_enabled ('unique'):
        if have_gtk3: unique_pkg = 'unique-3.0'
        else: unique_pkg = 'unique-1.0'
        if not check_pkg (unique_pkg, '0.9', mandatory=False):
            option_checkfatal ('unique', 'single instance')
    else:
        conf.define ('UNIQUE_VERSION', 'No')
        conf.check_message_custom ('unique', '', 'disabled')
    conf.define ('HAVE_UNIQUE', [0,1][conf.env['UNIQUE_VERSION'] != 'No'])

    check_pkg ('libsoup-2.4', '2.27.90', var='LIBSOUP')
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 29, 91):
        conf.define ('HAVE_LIBSOUP_2_29_91', 1)
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 34, 0):
        conf.define ('HAVE_LIBSOUP_2_34_0', 1)
        conf.env.append_value ('VALAFLAGS', '-D HAVE_LIBSOUP_2_34_0')
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 37, 1):
        conf.define ('HAVE_LIBSOUP_2_37_1', 1)

    if check_version (conf.env['WEBKIT_VERSION'], 1, 3, 8):
        conf.env.append_value ('VALAFLAGS', '-D HAVE_WEBKIT_1_3_8')
    if check_version (conf.env['WEBKIT_VERSION'], 1, 3, 13):
        conf.env.append_value ('VALAFLAGS', '-D HAVE_WEBKIT_1_3_13')
    if check_version (conf.env['WEBKIT_VERSION'], 1, 8, 0):
        conf.env.append_value ('VALAFLAGS', '-D HAVE_WEBKIT_1_8_0')

    check_pkg ('libxml-2.0', '2.6')
    conf.undefine ('LIBXML_VERSION') # Defined in xmlversion.h
    check_pkg ('sqlite3', '3.6.19', var='SQLITE')

    # Store options in env, since 'Options' is not persistent
    if 'CC' in os.environ: conf.env['CC'] = os.environ['CC'].split()
    conf.env['addons'] = option_enabled ('addons')
    conf.env['tests'] = option_enabled ('tests')
    conf.env['docs'] = option_enabled ('docs')
    if 'LINGUAS' in os.environ: conf.env['LINGUAS'] = os.environ['LINGUAS']

    if not check_version (conf.env['GIO_VERSION'], 2, 26, 0):
        conf.env['addons'] = False
        Utils.pprint ('YELLOW', 'Glib < 2.26.0, disabling addons')

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
    major, minor, micro = VERSION.split ('.', 2)
    conf.define ('MIDORI_MAJOR_VERSION', int (major))
    conf.define ('MIDORI_MINOR_VERSION', int (minor))
    conf.define ('MIDORI_MICRO_VERSION', int (micro))

    conf.env.append_value ('CCFLAGS', '-DHAVE_CONFIG_H -include config.h'.split ())
    debug_level = Options.options.debug_level
    compiler = conf.env['CC_NAME']

    if debug_level == 'full':
        conf.define ('PACKAGE_VERSION', '%s (debug)' % VERSION_FULL)
        conf.env.append_value ('CCFLAGS', '-DMIDORI_VERSION_SUFFIX="%s (debug)"' % VERSION_SUFFIX)
    else:
        conf.define ('PACKAGE_VERSION', VERSION_FULL)
        conf.env.append_value ('CCFLAGS', '-DMIDORI_VERSION_SUFFIX="%s"' % VERSION_SUFFIX)
    conf.write_config_header ('config.h')

    if debug_level == 'debug':
        conf.env.append_value ('VALAFLAGS', '--debug --enable-deprecated'.split ())
        if compiler == 'gcc':
            conf.env.append_value ('CCFLAGS', '-O2 -g -Wall -Wno-deprecated-declarations'.split ())
    elif debug_level == 'full':
        conf.env.append_value ('VALAFLAGS', '--debug --enable-checking'.split ())
        if compiler == 'gcc':
            conf.env.append_value ('CCFLAGS',
                '-O2 -g -Wall -Wextra -DG_ENABLE_DEBUG '
                '-Waggregate-return -Wno-unused-parameter '
                '-Wno-missing-field-initializers '
                '-Wredundant-decls -Wmissing-noreturn '
                '-Wshadow -Wpointer-arith -Wcast-align '
                '-Winline -Wformat-security -fno-common '
                '-Winit-self -Wundef -Wdeclaration-after-statement '
                '-Wmissing-format-attribute -Wnested-externs'.split ())
    conf.env.append_value ('CCFLAGS', '-Wno-unused-variable -Wno-comment'.split ())

    if conf.env['UNIQUE_VERSION'] == '1.0.4':
        Utils.pprint ('RED', 'unique 1.0.4 found, this version is erroneous.')
        Utils.pprint ('RED', 'Please use an older or newer version.')
        sys.exit (1)
    if check_version (conf.env['LIBSOUP_VERSION'], 2, 33, 4) \
        and check_version (conf.env['GIO_VERSION'], 2, 32, 1) \
        and not check_version (conf.env['GIO_VERSION'], 2, 32, 3):
        Utils.pprint ('RED', 'libsoup >= 2.33.4 found with glib >= 2.32.1 < 2.32.3:')
        Utils.pprint ('RED', 'This combination breaks the download GUI.')
        Utils.pprint ('RED', 'See https://bugs.launchpad.net/midori/+bug/780133/comments/14')
        sys.exit (1)

def set_options (opt):
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
        action = 'store', default = 'debug',
        help = 'Specify the debugging level. [\'none\', \'debug\', \'full\']',
        choices = ['none', 'debug', 'full'], dest = 'debug_level')
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
    add_enable_option ('granite', 'new notebook, pop-overs', group, disable=True)
    add_enable_option ('addons', 'building of extensions', group)
    add_enable_option ('tests', 'install tests', group, disable=True)
    add_enable_option ('gtk3', 'GTK+3 and WebKitGTK+3 support', group, disable=True)
    add_enable_option ('webkit2', 'WebKit2 support', group, disable=True)
    add_enable_option ('zeitgeist', 'Zeitgeist history integration', group, disable=is_win32 (os.environ))

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

    for res_file in ['about.css', 'error.html', 'close.png', 'gtk3.css', 'speeddial-head.html']:
        bld.install_files ('${MDATADIR}/' + APPNAME + '/res', 'data/' + res_file)

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
        def reset_xdg_dirs ():
            import tempfile, shutil
            base = os.path.join (tempfile.gettempdir (), 'midori-test', '%s')
            if os.path.exists (base % ''):
                shutil.rmtree (base % '')
            for x in ['XDG_CONFIG_HOME', 'XDG_CACHE_HOME', 'XDG_DATA_HOME', 'XDG_RUNTIME_DIR', 'TMPDIR']:
                os.environ[x] = (base % x).lower ()
                Utils.check_dir (os.environ[x])

        def subprocess_popen_timeout (args, stdout=None, stderr=None):
            import threading, signal
            def t_kill ():
                Utils.pprint ('RED', 'timed out')
                os.kill (pp.pid, signal.SIGABRT)
            t = threading.Timer (int(os.environ.get ('MIDORI_TIMEOUT', '42')), t_kill)
            t.start ()
            if is_mingw (Build.bld.env):
                args.insert (0, 'wine')
            cwd = Build.bld.env['PREFIX'] + os.sep + 'bin'
            pp = subprocess.Popen (args, cwd=cwd, stdout=stdout, stderr=stderr)
            if stdout is None:
                (out, err) = pp.communicate ()
                t.cancel ()
            return pp

        # Avoid i18n-related false failures
        os.environ['LC_ALL'] = 'C'
        os.environ['UNIQUE_BACKEND'] = 'bacon'
        if is_mingw (Build.bld.env):
            os.environ['MIDORI_EXEC_PATH'] = Build.bld.env['PREFIX']
        test = UnitTest.unit_test ()

        reset_xdg_dirs ()
        if True:
            test.unit_test_results = {}
            for obj in Build.bld.all_task_gen:
                if getattr (obj, 'unit_test', '') and 'cprogram' in obj.features:
                    if 'MIDORI_UNITS' in os.environ and not obj.target.split('-')[1] in os.environ['MIDORI_UNITS']:
                        continue
                    output = obj.path
                    filename = os.path.join (output.abspath (obj.env), obj.target)
                    srcdir = output.abspath ()
                    label = os.path.join (output.bldpath (obj.env), obj.target)
                    test.unit_tests[label] = (filename, srcdir)

            Utils.pprint ('GREEN', 'Running the unit tests')
            for label in test.unit_tests.allkeys:
                filename = test.unit_tests[label][0]
                test.unit_test_results[label] = 0
                try:
                    if is_mingw (Build.bld.env):
                        filename += '.exe'
                    args = [filename]
                    pp = subprocess_popen_timeout (args)
                    test.unit_test_results[label] = int (pp.returncode == 0)
                    if not test.unit_test_results[label]:
                        test.num_tests_failed += 1
                except OSError:
                    msg = sys.exc_info()[1] # Python 2/3 compatibility
                    Utils.pprint ('RED', '%s: %s' % (args, msg))
                    test.num_tests_err += 1
                except KeyboardInterrupt:
                    pass
        else:
            test.want_to_see_test_output = True
            test.want_to_see_test_error = True
            test.run ()

        reset_xdg_dirs ()
        for label in test.unit_tests.allkeys:
            if not test.unit_test_results[label]:
                Utils.pprint ('YELLOW', label + '...FAILED')
                filename = test.unit_tests[label][0]
                try:
                    if is_mingw (Build.bld.env):
                        filename += '.exe'
                    args = ['gdb', '--batch', '-ex', 'set print thread-events off', '-ex', 'run', '-ex', 'bt', filename]
                    pp = subprocess_popen_timeout (args)
                except OSError:
                    Utils.pprint ('RED', 'Install gdb to see backtraces')
                except KeyboardInterrupt:
                    pass
            else:
                Utils.pprint ('GREEN', label + '.......OK')
                filename = test.unit_tests[label][0]
                if is_mingw (Build.bld.env):
                    filename += '.exe'
                if os.environ.get ('MIDORI_TEST') == 'valgrind':
                    args = ['valgrind', '-q', '--leak-check=no', '--num-callers=4', '--show-possibly-lost=no', '--undef-value-errors=yes', '--track-origins=yes', filename]
                elif os.environ.get ('MIDORI_TEST') == 'callgrind':
                    args = ['valgrind', '--tool=callgrind', '--callgrind-out-file=%s.callgrind' % filename, filename]
                else:
                    continue
                try:
                    pp = subprocess_popen_timeout (args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    skip = False
                    for line in iter(pp.stdout.readline, ''):
                        if line[:2] != '==':
                            continue
                        if line == '':
                            skip = False
                        elif 'Conditional jump or move' in line:
                            skip = True
                        elif 'Uninitialised value was created by a stack allocation' in line:
                            skip = True
                        elif not skip:
                            sys.stdout.write (line[9:])
                except OSError:
                    Utils.pprint ('YELLOW', 'Install valgrind to perform memory checks')
                except KeyboardInterrupt:
                    pass

        if not 'MIDORI_UNITS' in os.environ:
            Utils.pprint ('BLUE', 'Set MIDORI_UNITS to select a subset of test cases')
        if not 'MIDORI_TEST' in os.environ:
            Utils.pprint ('BLUE', 'Set MIDORI_TEST to "valgrind" or "callgrind" to perform memory checks')
        if not 'MIDORI_TIMEOUT' in os.environ:
            Utils.pprint ('BLUE', 'Set MIDORI_TIMEOUT to set the maximum test runtime (default: 42)')
        # if test.num_tests_failed > 0 or test.num_tests_err > 0:
        #     sys.exit (1)

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
            command = nls + ' '
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
