# Explicit resource URLs work identically on Windows and macOS.
tpc_qml_module = custom_target('qml_resources',
  input: 'tpc-qml.qrc', output: 'qrc_tpc_qml.cpp',
  command: [rcc, '--name', 'tpc_qml', '@INPUT@', '-o', '@OUTPUT@'],
  depend_files: files('qmldir', 'Main.qml', 'controls/qmldir',
    'controls/BottomBar.qml', 'controls/FieldVisualization.qml',
    'controls/Settings.qml', 'controls/Workspace.qml', 'styles/qmldir',
    'styles/StyledButton.qml', 'styles/StyledTextField.qml'))
