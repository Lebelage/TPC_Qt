project(
  'TPC_Qt',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.7.0',
  default_options: [
    'cpp_std=c++latest',
    'warning_level=2',
  ],
)

qt6 = import('qt6')

qt6_dep = dependency(
  'qt6',
  modules: [
    'Core',
    'Concurrent',
    'Gui',
    'Qml',
    'Quick',
    'QuickControls2',
    'Widgets',
  ],
  main: host_machine.system() == 'windows',
)

qt_build_rpath = ''
qt_install_rpath = ''
if host_machine.system() == 'darwin'
  qmake = find_program('qmake6', 'qmake')
  qt_build_rpath = run_command(
    qmake,
    '-query',
    'QT_INSTALL_LIBS',
    check: true,
  ).stdout().strip()
  qt_install_rpath = '@executable_path/../Frameworks'
endif

nlohmann_json_dep = dependency(
  'nlohmann_json',
  method: 'cmake',
  modules: ['nlohmann_json::nlohmann_json'],
)

# Prefer an installed TPC package. If it is unavailable, Meson uses the pinned
# TPC_API wrap and its local Meson overlay from subprojects/packagefiles.
tpc_dep = dependency(
  'TPC',
  method: 'cmake',
  modules: ['TPC::TPC'],
  fallback: ['tpc_api', 'tpc_dep'],
)

project_inc = include_directories('src')
cpp = meson.get_compiler('cpp')
project_cpp_args = ['-DQT_NO_KEYWORDS']
if cpp.get_id() == 'msvc'
  project_cpp_args += [
    '/permissive-',
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/Zc:externConstexpr',
    '/bigobj',
  ]
endif

app_sources = files(
  'src/main.cpp',
  'src/application/application_context.cpp',
  'src/services/event_dispatcher/event_dispatcher.cpp',
  'src/services/field_slice/field_slice_service.cpp',
  'src/services/file_dialog/file_dialog_service.cpp',
  'src/services/file_worker/file_worker.cpp',
  'src/services/settings_holder/settings_holder.cpp',
  'src/services/tpc_service/tpc_service.cpp',
  'src/view/FieldSliceItem.cpp',
  'src/viewmodel/FieldVisualizationViewModel.cpp',
  'src/viewmodel/MainViewModel.cpp',
  'src/viewmodel/controls/BottomBarViewModel.cpp',
  'src/viewmodel/controls/SettingsTabViewModel.cpp',
  'src/viewmodel/controls/WorkspaceTabViewModel.cpp',
)

moc_headers = files(
  'src/models/tables/sensors_table_model.hpp',
  'src/view/FieldSliceItem.hpp',
  'src/viewmodel/FieldVisualizationViewModel.hpp',
  'src/viewmodel/MainViewModel.hpp',
  'src/viewmodel/controls/BottomBarViewModel.hpp',
  'src/viewmodel/controls/SettingsTabViewModel.hpp',
  'src/viewmodel/controls/WorkspaceTabViewModel.hpp',
)

moc_sources = qt6.compile_moc(
  headers: moc_headers,
  dependencies: qt6_dep,
  include_directories: project_inc,
)

subdir('qml')

tpc_qt = executable(
  'TPC_Qt',
  sources: [app_sources, moc_sources, tpc_qml_module],
  dependencies: [qt6_dep, tpc_dep, nlohmann_json_dep],
  include_directories: project_inc,
  cpp_args: project_cpp_args,
  win_subsystem: 'windows',
  build_rpath: qt_build_rpath,
  install_rpath: qt_install_rpath,
  install: true,
)

if host_machine.system() == 'windows'
  windeployqt = find_program('windeployqt', required: false)
  if windeployqt.found()
    run_target(
      'deploy',
      command: [
        windeployqt,
        '--release',
        '--no-translations',
        '--qmldir', meson.project_source_root() / 'qml',
        tpc_qt,
      ],
    )
  endif
elif host_machine.system() == 'darwin'
  macdeployqt = find_program('macdeployqt', required: false)
  if macdeployqt.found()
    bundle_data = configuration_data()
    bundle_data.set('BUNDLE_IDENTIFIER', 'org.tpc.controller')
    bundle_data.set('BUNDLE_VERSION', meson.project_version())
    bundle_data.set('EXECUTABLE_NAME', 'TPC_Qt')
    bundle_info = configure_file(
      input: 'meson/macos/Info.plist.in',
      output: 'TPC_Qt-Info.plist',
      configuration: bundle_data,
    )
    deploy_macos = find_program('scripts/meson/deploy-macos.sh')
    run_target(
      'deploy',
      command: [
        deploy_macos,
        tpc_qt,
        bundle_info,
        meson.project_build_root() / 'TPC_Qt.app',
        meson.project_source_root() / 'qml',
        macdeployqt,
      ],
    )
  endif
endif
