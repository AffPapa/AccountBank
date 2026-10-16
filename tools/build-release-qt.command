#!/bin/bash
# Build a separate, static arm64 Release Qt without modifying the Debug tree.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
repo_root="$(cd "$script_dir/.." && pwd -P)"
workspace_root="$(cd "$repo_root/.." && pwd -P)"

if [[ "$workspace_root" != /Users/dumay/Desktop/PaSEO ]]; then
  echo 'This script is restricted to /Users/dumay/Desktop/PaSEO.' >&2
  exit 1
fi

qt_source="$workspace_root/Libraries/qt_6.11.2"
release_source="$workspace_root/Libraries/qt_6.11.2-release-source"
release_build="$workspace_root/Libraries/qt_6.11.2-release-build"
release_prefix="$workspace_root/Libraries/local/Qt-6.11.2-Release-arm64"
local_prefix="$workspace_root/Libraries/local"

usage() {
  echo "Usage: $0 [--check | --build]"
  echo '  --check  Validate inputs and show paths without writing (default).'
  echo '  --build  Snapshot the current patched Qt source, configure, build, install.'
}

mode="${1:---check}"
if [[ $# -gt 1 || ( "$mode" != --check && "$mode" != --build ) ]]; then
  usage >&2
  exit 2
fi

for command_name in cmake git python3; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required tool is missing: $command_name" >&2
    exit 1
  fi
done

if [[ ! -f "$qt_source/configure" || ! -f "$qt_source/qtbase/cmake/FindLCMS2.cmake" ]]; then
  echo 'Qt source or the local LCMS2 CMake helper is missing.' >&2
  exit 1
fi

for module in . qtbase qtimageformats qtshadertools qtsvg; do
  if [[ "$(git -C "$qt_source/$module" rev-parse --is-inside-work-tree)" != true ]]; then
    echo "Qt module is not a Git checkout: $module" >&2
    exit 1
  fi
done

for destination in "$release_source" "$release_build" "$release_prefix"; do
  if [[ -e "$destination" || -L "$destination" ]]; then
    echo "Refusing to overwrite existing Release path: $destination" >&2
    exit 1
  fi
done

jobs="${ACCOUNTBANK_QT_RELEASE_JOBS:-8}"
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
  echo 'ACCOUNTBANK_QT_RELEASE_JOBS must be a positive integer.' >&2
  exit 2
fi

echo "Qt source:  $qt_source"
echo "Snapshot:   $release_source"
echo "Build tree: $release_build"
echo "Install:    $release_prefix"
echo "Jobs:       $jobs"

if [[ "$mode" == --check ]]; then
  echo 'Ready. Run with --build to create the isolated Release Qt.'
  exit 0
fi

# Reserve all destinations with exclusive mkdir calls before copying or
# configuring. Even a failed build cannot silently reuse an older Release.
mkdir "$release_source" "$release_build" "$release_prefix"

# Git's tracked-file lists include local source patches while omitting the
# substantial generated Debug outputs. Copy the one untracked helper needed by
# the current qtbase LCMS2 patch explicitly.
python3 - "$qt_source" "$release_source" <<'PY'
import os
import pathlib
import shutil
import subprocess
import sys

source = pathlib.Path(sys.argv[1])
destination = pathlib.Path(sys.argv[2])

for module in ('.', 'qtbase', 'qtimageformats', 'qtshadertools', 'qtsvg'):
    module_source = source / module
    module_destination = destination / module
    paths = subprocess.check_output(
        ['git', '-C', str(module_source), 'ls-files', '-z']
    ).split(b'\0')
    for raw in paths:
        if not raw:
            continue
        relative = pathlib.Path(os.fsdecode(raw))
        item_source = module_source / relative
        item_destination = module_destination / relative
        if not os.path.lexists(item_source):
            continue  # Preserve tracked deletions in the working tree.
        if item_source.is_dir() and not item_source.is_symlink():
            continue  # A superproject submodule entry is copied below.
        item_destination.parent.mkdir(parents=True, exist_ok=True)
        if item_source.is_symlink():
            item_destination.symlink_to(os.readlink(item_source))
        else:
            shutil.copy2(item_source, item_destination)

helper = pathlib.Path('qtbase/cmake/FindLCMS2.cmake')
shutil.copy2(source / helper, destination / helper)
PY

cd "$release_build"
"$release_source/configure" \
  -prefix "$release_prefix" \
  -release -opensource -confirm-license -static -no-framework \
  -opengl desktop -no-openssl -securetransport -system-webp \
  -I "$local_prefix/include" \
  -no-feature-futimens -no-feature-brotli -no-feature-cxx17_filesystem \
  -submodules qtbase,qtimageformats,qtshadertools,qtsvg \
  -nomake examples -nomake tests -platform macx-clang \
  -- \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  "-DCMAKE_PREFIX_PATH=$local_prefix" \
  -DQT_NO_HANDLE_APPLE_SINGLE_ARCH_CROSS_COMPILING=ON \
  -DQT_SYNC_HEADERS_AT_CONFIGURE_TIME=ON

cmake --build "$release_build" --parallel "$jobs"
cmake --install "$release_build"

if [[ ! -f "$release_prefix/lib/libQt6OpenGLWidgets.a" ]]; then
  echo 'Release Qt installation is incomplete: OpenGLWidgets archive missing.' >&2
  exit 1
fi

echo "Release Qt installed: $release_prefix"
