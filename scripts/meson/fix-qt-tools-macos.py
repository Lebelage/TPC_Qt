#!/usr/bin/env python3
"""Repair Qt tools moved by vcpkg without flattening framework install names."""
from pathlib import Path
import subprocess
import sys

prefix = Path(sys.argv[1])
# Qt installs executables in bin; vcpkg moves them to tools/Qt6/bin.
# Library/plugin install names remain valid, but executable rpaths need repair.
rpath = '@loader_path/../../../lib'
tools = prefix / 'tools/Qt6/bin'
if not tools.is_dir():
    sys.exit(0)
for tool in tools.iterdir():
    if not tool.is_file() or tool.is_symlink():
        continue
    header = subprocess.run(['otool', '-hv', str(tool)], capture_output=True, text=True)
    if header.returncode or 'EXECUTE' not in header.stdout:
        continue
    paths = subprocess.check_output(['otool', '-l', str(tool)], text=True)
    if f'path {rpath} (offset ' in paths:
        continue
    subprocess.run(['install_name_tool', '-add_rpath', rpath, str(tool)], check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(tool)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
