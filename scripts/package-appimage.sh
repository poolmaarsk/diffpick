#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 BUILD_DIR VERSION" >&2
    exit 2
fi

build_dir="$(realpath "$1")"
version="$2"
work_dir="$build_dir/appimage"
appdir="$work_dir/AppDir"
tools_dir="$work_dir/tools"
output_dir="$build_dir/package"

case "$work_dir" in
    "$build_dir"/*) ;;
    *) echo "Refusing to stage outside the build directory." >&2; exit 1 ;;
esac

rm -rf "$appdir"
mkdir -p "$appdir" "$tools_dir" "$output_dir"

DESTDIR="$appdir" cmake --install "$build_dir" --prefix /usr

download_tool() {
    local destination="$1"
    local url="$2"
    if [[ ! -x "$destination" ]]; then
        curl --fail --location --retry 3 --output "$destination.tmp" "$url"
        chmod +x "$destination.tmp"
        mv "$destination.tmp" "$destination"
    fi
}

linuxdeploy="$tools_dir/linuxdeploy-x86_64.AppImage"
qt_plugin="$tools_dir/linuxdeploy-plugin-qt-x86_64.AppImage"
download_tool "$linuxdeploy" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
download_tool "$qt_plugin" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"

qmake_path="$(command -v qmake6 || command -v qmake || true)"
if [[ -z "$qmake_path" ]]; then
    echo "qmake was not found; set up the Qt development environment first." >&2
    exit 1
fi

qt_prefix="$($qmake_path -query QT_INSTALL_PREFIX)"
qt_plugins="$($qmake_path -query QT_INSTALL_PLUGINS)"
license_destination="$appdir/usr/share/doc/diffpick/QT-LICENSE-LGPL-3.0.txt"
mkdir -p "$(dirname "$license_destination")"
for license_file in \
    "$qt_prefix/LICENSES/LGPL-3.0-only.txt" \
    "$qt_prefix/share/licenses/qt6-base/LGPL-3.0-only.txt" \
    /usr/share/common-licenses/LGPL-3; do
    if [[ -f "$license_file" ]]; then
        cp "$license_file" "$license_destination"
        break
    fi
done
if [[ ! -f "$license_destination" ]]; then
    echo "The Qt LGPL v3 license text was not found." >&2
    exit 1
fi

output="$output_dir/diffpick-linux-x86_64.AppImage"
rm -f "$output"

export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$qmake_path"
wayland_plugins=()
for plugin in libqwayland-egl.so libqwayland-generic.so libqwayland.so; do
    if [[ -f "$qt_plugins/platforms/$plugin" ]]; then
        wayland_plugins+=("$plugin")
    fi
done
if [[ ${#wayland_plugins[@]} -eq 0 ]]; then
    echo "No Qt Wayland platform plugin was found." >&2
    exit 1
fi
export EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${wayland_plugins[*]}")"
for plugin_dir in wayland-shell-integration wayland-decoration-client \
                  wayland-graphics-integration-client; do
    if [[ -d "$qt_plugins/$plugin_dir" ]]; then
        mkdir -p "$appdir/usr/plugins/$plugin_dir"
        cp -a "$qt_plugins/$plugin_dir/." "$appdir/usr/plugins/$plugin_dir/"
    fi
done
export LDAI_OUTPUT="$output"
export LINUXDEPLOY_OUTPUT_VERSION="$version"
"$linuxdeploy" --appdir "$appdir" --plugin qt --output appimage

test -x "$output"
echo "Created $output"
