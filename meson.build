project('midori', 'vala', 'c',
        version: '0.6.0',
        license: 'LGPLv2.1+',
        default_options:['buildtype=debugoptimized'])

revision = run_command('git', 'describe', '--tags').stdout().strip()
if revision != ''
    revision = '~r' + revision
    add_project_arguments(['--fatal-warnings'],
                          language: 'vala')
else
    add_project_arguments(['--disable-assert'],
                          language: 'vala')
endif

add_project_arguments(['--vapidir', join_paths(meson.current_source_dir(), 'vapi')],
                      ['--pkg=config'],
                      ['--target-glib=2.48'],
                      ['--enable-deprecated'],
                      language: 'vala')
add_project_arguments(['-DGCR_API_SUBJECT_TO_CHANGE'],
                      language: 'c')

gnome = import('gnome')
windows = import('windows')
i18n = import('i18n')

add_global_arguments([
    '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name())
    ],
    language: 'c',
)

asresources = gnome.compile_resources(
  'as-resources', 'gresource.xml',
  source_dir: '.',
  c_name: 'as'
)

if host_machine.system() == 'windows'
    winres = windows.compile_resources(
        'main.rc'
    )
endif

conf_data = configuration_data()
conf_data.set('CORE_VERSION', meson.project_version() + revision)
conf_data.set('PROJECT_NAME', meson.project_name())
conf_data.set('PROJECT_BUGS', 'https://github.com/midori-browser/core/issues')
conf_data.set('PROJECT_WEBSITE', 'https://www.midori-browser.org')
conf_data.set('DATADIR', get_option('datadir'))
conf_data.set('CMAKE_INSTALL_FULL_SYSCONFDIR', get_option('sysconfdir'))
conf_data.set('CMAKE_INSTALL_FULL_LIBDIR', get_option('libdir'))
conf_data.set('CMAKE_INSTALL_FULL_DOCDIR', join_paths(get_option('datadir'), 'doc'))
conf_data.set('CMAKE_INSTALL_FULL_LOCALEDIR', get_option('localedir'))
config_header = configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration: conf_data,
)

# subdir('core')
potfiles = run_command('cat', 'po/POTFILES.in').stdout().strip().split('\n')
sources = []
foreach source : potfiles
    if source.startswith('core/') and source.endswith('.vala')
        sources += source
    endif
endforeach
executable(
    meson.project_name(),
    sources,
    asresources,
    config_header,
    dependencies: [
        dependency('sqlite3', version: '>=3.6.19'),
        dependency('glib-2.0', version: '>=2.48.0'),
        dependency('libsoup-2.4', version: '>=2.48.0'),
        dependency('gtk+-3.0', version: '>=3.12.0'),
        dependency('gcr-ui-3', version: '>=2.32'),
        dependency('libpeas-gtk-1.0'),
        dependency('webkit2gtk-4.0'),
        dependency('javascriptcoregtk-4.0'),
    ],
    gui_app: true,
    install: true
)

# subdir('web')
# XXX

# subdir('data')
icon_sizes = ['16x16', '22x22']
foreach size : icon_sizes
    install_data(
        join_paths('icons', size, 'apps', meson.project_name() + '.png'),
        install_dir: join_paths(get_option('datadir'), 'icons/hicolor', size, 'apps'),
    )
endforeach
install_data(
    join_paths('icons/scalable/apps', meson.project_name() + '.svg'),
    install_dir: join_paths(get_option('datadir'), 'icons/hicolor', 'scalable/apps'),
)
i18n.merge_file(
    input: join_paths('data', meson.project_name() + '.desktop.in'),
    output: meson.project_name() + '.desktop',
    po_dir: join_paths(meson.source_root(), 'po'),
    type: 'desktop',
    install: true,
    install_dir: join_paths(get_option('datadir'), 'applications')
)
i18n.merge_file(
    input: join_paths('data', meson.project_name() + '.appdata.xml.in'),
    output: meson.project_name() + '.appdata.xml',
    po_dir: join_paths(meson.source_root(), 'po'),
    install: true,
    install_dir: join_paths(get_option('datadir'), 'metainfo')
)

# subdir('po')
i18n.gettext(meson.project_name(), preset: 'glib')
