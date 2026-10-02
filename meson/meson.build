fs = import('fs')
vcpkg_root = get_option('vcpkg_root')
if vcpkg_root == ''
  if build_machine.system() == 'windows'
    env_probe = run_command(find_program('powershell.exe'), '-NoLogo', '-NoProfile',
      '-Command', '[Environment]::GetEnvironmentVariable("VCPKG_ROOT")', check: true)
  else
    env_probe = run_command(find_program('sh'), '-c', 'printf %s "$VCPKG_ROOT"', check: true)
  endif
  vcpkg_root = env_probe.stdout().strip()
endif
vcpkg_name = build_machine.system() == 'windows' ? 'vcpkg.exe' : 'vcpkg'
vcpkg = find_program(vcpkg_name, required: false)
if not vcpkg.found() and vcpkg_root != ''
  vcpkg = find_program(vcpkg_root / vcpkg_name)
elif not vcpkg.found()
  error('vcpkg was not found in PATH. Install it or set VCPKG_ROOT.')
endif

# A package manager may install only the vcpkg executable. vcpkg still needs
# its scripts and ports checkout. Use an explicitly supplied checkout when
# available; otherwise keep one in the ignored project build directory.
managed_vcpkg_root = false
if vcpkg_root == ''
  executable_root = fs.parent(fs.parent(vcpkg.full_path()))
  if fs.exists(executable_root / '.vcpkg-root')
    vcpkg_root = executable_root
  else
    vcpkg_root = meson.project_source_root() / '.build/vcpkg-root'
    managed_vcpkg_root = true
  endif
endif

if managed_vcpkg_root
  manifest_text = fs.read('../vcpkg.json')
  baseline_parts = manifest_text.split('"builtin-baseline"')
  if baseline_parts.length() != 2
    error('vcpkg.json must contain exactly one builtin-baseline.')
  endif
  baseline = baseline_parts[1].split('"')[1]

  git = find_program('git')
  if not fs.is_dir(vcpkg_root / '.git')
    message('Preparing the vcpkg checkout in ' + vcpkg_root)
    run_command(git, 'init', vcpkg_root, check: true)
  endif
  checkout = run_command(git, '-C', vcpkg_root, 'rev-parse', 'HEAD', check: false)
  if checkout.returncode() != 0 or checkout.stdout().strip() != baseline
    message('Downloading vcpkg registry files for baseline ' + baseline)
    run_command(git, '-C', vcpkg_root, 'fetch', '--depth=1',
      'https://github.com/microsoft/vcpkg.git', baseline, check: true)
    run_command(git, '-C', vcpkg_root, 'checkout', '--detach', 'FETCH_HEAD', check: true)
  endif
endif

if not fs.exists(vcpkg_root / '.vcpkg-root')
  error('VCPKG_ROOT is not a vcpkg checkout: ' + vcpkg_root)
endif
vcpkg_env = environment()
vcpkg_env.set('VCPKG_ROOT', vcpkg_root)

if host_machine.cpu_family() == 'aarch64'
  target_arch = 'arm64'
  vcpkg_arch = 'arm64'
elif host_machine.cpu_family() == 'x86_64'
  target_arch = 'x86_64'
  vcpkg_arch = 'x64'
else
  error('Supported target architectures: x86_64 and aarch64')
endif
if host_machine.system() == 'darwin'
  vcpkg_platform = 'osx'
elif host_machine.system() == 'windows'
  vcpkg_platform = 'windows'
else
  error('Supported target systems: macOS and Windows')
endif
host_arch = build_machine.cpu_family() == 'aarch64' ? 'arm64' : 'x64'
host_platform = build_machine.system() == 'darwin' ? 'osx' : 'windows'
triplet = vcpkg_arch + '-' + vcpkg_platform
host_triplet = host_arch + '-' + host_platform
install_root = meson.project_source_root() / '.build/meson-vcpkg' / triplet
dependency_prefix = install_root / triplet
host_prefix = install_root / host_triplet
# Track dependency inputs so changes trigger automatic reconfiguration.
configure_file(input: '../vcpkg.json', output: 'vcpkg-manifest.json', copy: true)
foreach input : ['triplets/arm64-osx.cmake', 'triplets/x64-osx.cmake', 'fix-qt-tools.cmake', '../scripts/meson/fix-qt-tools-macos.py']
  configure_file(input: input, output: fs.name(input), copy: true)
endforeach
message('Installing manifest dependencies with vcpkg (' + triplet + ')')
install = run_command(vcpkg, 'install',
  '--x-manifest-root=' + meson.project_source_root(),
  '--x-install-root=' + install_root,
  '--overlay-triplets=' + meson.current_source_dir() / 'triplets',
  '--triplet=' + triplet, '--host-triplet=' + host_triplet,
  env: vcpkg_env, check: false)
if install.returncode() != 0
  error('vcpkg install failed:\n' + install.stdout() + install.stderr())
endif

managed_cmake_args = [
  '-DCMAKE_PREFIX_PATH=' + dependency_prefix,
  '-DCMAKE_FIND_ROOT_PATH=' + dependency_prefix,
  '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY',
  '-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY',
  '-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY',
  '-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER',
  '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
  '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF',
]
host_qt_bin = host_prefix / 'tools/Qt6/bin'
tool_suffix = build_machine.system() == 'windows' ? '.exe' : ''
moc = find_program(host_qt_bin / ('moc' + tool_suffix))
rcc = find_program(host_qt_bin / ('rcc' + tool_suffix))
qt_data = configuration_data()
qt_data.set('PREFIX', dependency_prefix.replace('\\', '/'))
qt_data.set('HOST_PREFIX', host_prefix.replace('\\', '/'))
qt_data.set('CONFIG_PREFIX', get_option('buildtype') == 'debug' ? 'debug/' : '')
qt_conf = configure_file(input: 'qt.conf.in', output: 'qt.conf', configuration: qt_data)
if build_machine.system() == 'darwin'
  deploy_tool = configure_file(input: host_qt_bin / 'macdeployqt', output: 'macdeployqt', copy: true)
  deploy_tool_path = meson.current_build_dir() / 'macdeployqt'
  run_command(find_program('chmod'), '+x', deploy_tool_path, check: true)
  # The copied tool uses this build's qt.conf and the host's release libraries.
  rpaths = run_command(find_program('otool'), '-l', deploy_tool_path, check: true).stdout()
  if not rpaths.contains(host_prefix / 'lib')
    run_command(find_program('install_name_tool'), '-add_rpath', host_prefix / 'lib', deploy_tool_path, check: true)
  endif
  run_command(find_program('codesign'), '--force', '--sign', '-', deploy_tool_path, check: true)
elif build_machine.system() == 'windows'
  qtpaths_file = configure_file(input: host_qt_bin / 'qtpaths.exe', output: 'qtpaths.exe', copy: true)
  qtpaths_path = meson.current_build_dir() / 'qtpaths.exe'
endif
summary({
  'vcpkg': vcpkg.full_path(),
  'vcpkg root': vcpkg_root,
  'Target triplet': triplet,
  'Dependencies': dependency_prefix,
}, section: 'Managed dependencies')
