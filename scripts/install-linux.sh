#!/usr/bin/env bash
# Install TownsQt (Tsugaru_QT) on Linux via cmake --install (m88-qt style).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CMAKE_SOURCE="${CMAKE_SOURCE:-$REPO_ROOT/src}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
PREFIX="${PREFIX:-}"
DESTDIR="${DESTDIR:-}"
DO_BUILD=0
USE_SUDO=0

usage() {
	cat <<'EOF'
Usage: scripts/install-linux.sh [options]

Install TownsQt (Tsugaru_QT) built under build/townsqt/.

Options:
  --user          Install to ~/.local (bin, share, applications)
  --system        Install to /usr (uses sudo)
  --prefix PATH   Install prefix (default: /usr with --system, else ~/.local)
  --build-dir DIR CMake build directory (default: ./build)
  --build         Build TownsQt before install if missing
  --DESTDIR PATH  Staging root for packaging (e.g. /tmp/townsqt-root)
  -h, --help      Show this help

Examples:
  ./scripts/install-linux.sh --build --user
  ./scripts/install-linux.sh --system
  DESTDIR=/tmp/stage ./scripts/install-linux.sh --system

Installed layout (prefix /usr):
  bin/Tsugaru_QT
  share/applications/townsqt.desktop
  share/icons/hicolor/*/apps/townsqt.png
  share/townsqt/translations/townsqt_*.json  (de/fr/es/ko/zh; en/ja embedded)
  share/doc/townsqt/BUILD_LINUX.md

UI language: TOWNSQT_LANG or system locale.
Translations dir override: TOWNSQT_TRANSLATIONS_DIR
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	--user)
		PREFIX="${HOME}/.local"
		shift
		;;
	--system)
		PREFIX="/usr"
		USE_SUDO=1
		shift
		;;
	--prefix)
		PREFIX="$2"
		shift 2
		;;
	--build-dir)
		BUILD_DIR="$2"
		shift 2
		;;
	--build)
		DO_BUILD=1
		shift
		;;
	--DESTDIR)
		DESTDIR="$2"
		shift 2
		;;
	-h|--help)
		usage
		exit 0
		;;
	*)
		echo "Unknown option: $1" >&2
		usage >&2
		exit 1
		;;
	esac
done

if [[ -z "$PREFIX" ]]; then
	PREFIX="${HOME}/.local"
fi

TOWNSQT_BIN="${BUILD_DIR}/townsqt/Tsugaru_QT"

if [[ ! -x "$TOWNSQT_BIN" ]]; then
	if [[ "$DO_BUILD" -eq 1 ]]; then
		echo "Building TownsQt in ${BUILD_DIR}..."
		cmake -S "$CMAKE_SOURCE" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
		cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target TownsQt
	else
		echo "TownsQt binary not found: ${TOWNSQT_BIN}" >&2
		echo "Build first, or re-run with --build" >&2
		exit 1
	fi
fi

INSTALL_ARGS=(--install "$BUILD_DIR" --prefix "$PREFIX")

run_install() {
	if [[ -n "$DESTDIR" ]]; then
		DESTDIR="$DESTDIR" cmake "${INSTALL_ARGS[@]}"
	else
		cmake "${INSTALL_ARGS[@]}"
	fi
}

echo "Installing TownsQt to prefix: ${PREFIX}"
if [[ -n "$DESTDIR" ]]; then
	echo "DESTDIR staging: ${DESTDIR}"
fi

if [[ "$USE_SUDO" -eq 1 && "$(id -u)" -ne 0 ]]; then
	sudo env DESTDIR="$DESTDIR" cmake "${INSTALL_ARGS[@]}"
else
	run_install
fi

echo
echo "Done."
echo "  Binary: ${PREFIX}/bin/Tsugaru_QT"
if [[ "$PREFIX" == "${HOME}/.local" ]]; then
	echo "  Ensure ~/.local/bin is in your PATH."
fi
if [[ -n "$DESTDIR" ]]; then
	echo "  Staged under: ${DESTDIR}${PREFIX}"
fi
