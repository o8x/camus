#!/usr/bin/env bash
#
# Install the latest Camus release.
# Supported: macOS arm64 (.pkg) and Linux amd64 (.deb).
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/the-writer/camus/main/scripts/install.sh | bash

set -euo pipefail

case "$(uname -s)-$(uname -m)" in
	Darwin-arm64) ext="pkg" ;;
	Linux-x86_64 | Linux-amd64) ext="deb" ;;
	*)
		echo "error: unsupported platform '$(uname -s) $(uname -m)' (need macOS arm64 or Linux amd64)" >&2
		exit 1
		;;
esac

LATEST_URL="https://github.com/the-writer/camus/releases/latest/download/latest.json"
json="$(curl -fsSL "$LATEST_URL" </dev/null)" || {
	echo "error: failed to fetch $LATEST_URL" >&2
	exit 1
}

url="$(printf '%s\n' "$json" | grep -oE 'https://[^"]+\.'"$ext" | sed -n '1p')"
[ -n "$url" ] || {
	echo "error: no .$ext asset found in latest release" >&2
	exit 1
}

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
file="$tmp/$(basename "$url")"

echo "downloading: $url" >&2
curl -fL "$url" -o "$file" </dev/null

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"

echo "installing: $(basename "$file")" >&2
if [ "$ext" = "pkg" ]; then
	$SUDO installer -pkg "$file" -target /
else
	$SUDO dpkg -i "$file" || {
		$SUDO apt-get -f install -y
		$SUDO dpkg -i "$file"
	}
fi
