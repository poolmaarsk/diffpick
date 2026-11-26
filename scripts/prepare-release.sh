#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 || ! "$1" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Usage: $0 vX.Y.Z WINDOWS_ZIP LINUX_APPIMAGE" >&2
    exit 2
fi

tag="$1"
version="${tag#v}"
windows_zip="$2"
linux_appimage="$3"
if [[ "${windows_zip##*/}" != "diffpick-$version-windows-x64.zip" ||
      "${linux_appimage##*/}" != "diffpick-$version-linux-x86_64.AppImage" ||
      ! -s "$windows_zip" || ! -s "$linux_appimage" ]]; then
    echo "Expected both diffpick packages for $tag." >&2
    exit 1
fi

release="$(gh api --paginate 'repos/{owner}/{repo}/releases?per_page=100' |
    jq -sc --arg tag "$tag" 'map(.[] | select(.tag_name == $tag)) | first // null')"

if [[ "$release" == null ]]; then
    gh release create "$tag" "$windows_zip" "$linux_appimage" \
        --verify-tag --draft --title "diffpick $version" --generate-notes \
        --notes "[Code signing policy](https://github.com/$GH_REPO/blob/$tag/docs/code-signing.md)"
    exit 0
fi

if [[ "$(jq -r '.draft' <<< "$release")" != true ]]; then
    echo "Release $tag is already published."
    exit 0
fi

for package in "$windows_zip" "$linux_appimage"; do
    if jq -e --arg name "${package##*/}" 'any(.assets[]; .name == $name and .state == "uploaded")' \
        <<< "$release" > /dev/null; then
        continue
    fi

    gh release upload "$tag" "$package"
done
