project(
  'TPC_Qt',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.7.0',
  default_options: [
    'cpp_std=c++23,c++latest',
    'warning_level=2',
  ],
)

subdir('meson')

qt_modules = ['Core', 'Concurrent', 'Gui', 'Qml', 'Quick', 'QuickControls2', 'Widgets']
qt_include_args = ['-I' + dependency_prefix / 'include/Qt6']
qt_link_args = []
qt_compile_args = get_option('debug') ? [] : ['-DQT_NO_DEBUG']
qt_libdir = dependency_prefix / (get_option('buildtype') == 'debug' ? 'debug/lib' : 'lib')
if host_machine.system() == 'darwin'
  qt_include_args += '-F' + qt_libdir
endif
foreach module : qt_modules
  qt_include_args += '-I' + dependency_prefix / 'include/Qt6' / ('Qt' + module)
  qt_compile_args += '-DQT_' + module.to_upper() + '_LIB'
  if host_machine.system() == 'darwin'
    framework = qt_libdir / ('Qt' + module + '.framework')
    if not fs.is_file(framework / ('Qt' + module))
      error('Missing vcpkg Qt framework: ' + framework)
    endif
    qt_link_args += framework / ('Qt' + module)
    qt_include_args += '-I' + framework / 'Headers'
  else
    suffix = get_option('buildtype') == 'debug' ? 'd' : ''
    library = qt_libdir / ('Qt6' + module + suffix + '.lib')
    if not fs.is_file(library)
      error('Missing vcpkg Qt library: ' + library)
    endif
    qt_link_args += library
  endif
endforeach
if host_machine.system() == 'windows'
  entrypoint = qt_libdir / ('Qt6EntryPoint' + suffix + '.lib')
  if not fs.is_file(entrypoint)
    error('Missing vcpkg Qt entry point: ' + entrypoint)
  endif
  qt_link_args += [entrypoint, 'shell32.lib']
endif
qt6_dep = declare_dependency(compile_args: qt_include_args + qt_compile_args, link_args: qt_link_args)

qt_build_rpath = dependency_prefix / (get_option('buildtype') == 'debug' ? 'debug/lib' : 'lib')
qt_install_rpath = host_machine.system() == 'darwin' ? '@executable_path/../Frameworks' : ''
nlohmann_json_dep = dependency('nlohmann_json', method: 'cmake',
  modules: ['nlohmann_json::nlohmann_json'], cmake_args: managed_cmake_args, required: true)

# Make the same restricted dependency search available to the TPC subproject.
foreach entry : [['Eigen3', 'Eigen3::Eigen'], ['open62541pp', 'open62541pp::open62541pp'], ['stdexec', 'STDEXEC::stdexec']]
  dep = dependency(entry[0], method: 'cmake', modules: [entry[1]],
    cmake_args: managed_cmake_args, required: true)
  meson.override_dependency(entry[0], dep)
endforeach

# Always use the pinned project source, independent of installed TPC packages.
tpc_dep = subproject('tpc_api').get_variable('tpc_dep')

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

moc_sources = []
foreach header : moc_headers
  moc_output = 'moc_' + fs.name(header) + '.cpp'
  moc_sources += custom_target('moc_' + fs.name(header),
    input: header, output: moc_output, depfile: moc_output + '.d',
    command: [moc] + qt_include_args + qt_compile_args + [
      '-DQT_NO_KEYWORDS', '-I' + meson.project_source_root() / 'src',
      '--output-dep-file', '@INPUT@', '-o', '@OUTPUT@'])
endforeach

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
  windeployqt = find_program(host_qt_bin / 'windeployqt.exe')
  if windeployqt.found()
    powershell = find_program('powershell.exe')
    deploy_windows = files('scripts/meson/deploy-windows.ps1')
    deploy_command = [
      powershell, '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
      '-File', deploy_windows,
      '-BuildDir', meson.project_build_root(),
      '-WinDeployQt', windeployqt.full_path(),
      '-QtPaths', qtpaths_path,
      '-HostPrefix', host_prefix,
      '-DependencyPrefix', dependency_prefix,
    ]
    run_target(
      'deploy',
      command: deploy_command,
      depends: tpc_qt,
    )
    if get_option('buildtype') == 'release'
      run_target('package', command: deploy_command + ['-Package', '-Archive'], depends: tpc_qt)
    endif
  endif
elif host_machine.system() == 'darwin'
  macdeployqt = find_program(deploy_tool_path)
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
    deploy_command = [
        deploy_macos,
        tpc_qt,
        bundle_info,
        meson.project_build_root() / 'TPC_Qt.app',
        meson.project_source_root() / 'qml',
        macdeployqt,
        get_option('buildtype'),
      ]
    run_target('deploy', command: deploy_command, depends: tpc_qt)
    if get_option('buildtype') == 'release'
      run_target('package', command: [find_program('scripts/meson/package-macos.sh'),
        meson.project_build_root() / 'TPC_Qt.app', target_arch, 'package'] + deploy_command, depends: tpc_qt)
    endif
  endif
endif
