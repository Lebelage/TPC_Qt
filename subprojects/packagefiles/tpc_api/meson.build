project(
  'TPC',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.7.0',
  default_options: [
    'cpp_std=c++23,c++latest',
    'default_library=static',
    'warning_level=3',
  ],
)

# The parent supplies these dependencies from its vcpkg installation.
eigen_dep = dependency('Eigen3')
open62541pp_dep = dependency('open62541pp')
stdexec_dep = dependency('stdexec')

tpc_inc = include_directories('include')
tpc_cpp = meson.get_compiler('cpp')
tpc_cpp_args = []
if tpc_cpp.get_id() == 'msvc'
  tpc_cpp_args += [
    '/permissive-',
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/Zc:externConstexpr',
    '/bigobj',
  ]
endif

tpc_system = static_library(
  'tpc_system',
  files(
    'src/system/tpc.cpp',
    'src/system/client/client.cpp',
    'src/system/client/frame_receiver.cpp',
    'src/system/client/subscription.cpp',
  ),
  dependencies: [eigen_dep, open62541pp_dep, stdexec_dep],
  include_directories: tpc_inc,
  cpp_args: tpc_cpp_args,
  install: false,
)

tpc_dep = declare_dependency(
  link_with: tpc_system,
  dependencies: [eigen_dep, open62541pp_dep, stdexec_dep],
  include_directories: tpc_inc,
)

meson.override_dependency('TPC', tpc_dep)
