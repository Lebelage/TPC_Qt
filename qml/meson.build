tpc_qml_module = []

tpc_qml_module += qt6.qml_module(
  'TPC',
  version: '1.0',
  qml_sources: files('Main.qml'),
  dependencies: qt6_dep,
  include_directories: project_inc,
)

tpc_qml_module += qt6.qml_module(
  'TPC.controls',
  version: '1.0',
  qml_sources: files(
    'controls/BottomBar.qml',
    'controls/FieldVisualization.qml',
    'controls/Settings.qml',
    'controls/Workspace.qml',
  ),
  dependencies: qt6_dep,
  include_directories: project_inc,
)

tpc_qml_module += qt6.qml_module(
  'TPC.styles',
  version: '1.0',
  qml_sources: files(
    'styles/StyledButton.qml',
    'styles/StyledTextField.qml',
  ),
  dependencies: qt6_dep,
  include_directories: project_inc,
)
