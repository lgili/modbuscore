#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: install-from-archive.sh [--prefix <path>] <archive>

Extracts a ModbusCore release archive into the desired prefix (default: /usr/local)
and prints follow-up instructions for verifying CMake/pkg-config consumption.
EOF
}

prefix="/usr/local"
archive=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      shift
      [[ $# -gt 0 ]] || { echo "error: --prefix requires a value" >&2; exit 2; }
      prefix="$1"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      archive="$1"
      ;;
  esac
  shift
done

if [[ -z "${archive}" ]]; then
  usage >&2
  exit 2
fi

if [[ ! -f "${archive}" ]]; then
  echo "error: archive '${archive}' not found" >&2
  exit 1
fi

mkdir -p "${prefix}"

case "${archive}" in
  *.tar.gz)
    tar -xzf "${archive}" -C "${prefix}"
    ;;
  *)
    echo "error: unsupported archive format '${archive}'" >&2
    exit 1
    ;;
esac

cat <<EOF
✅ Installed archive into ${prefix}

Next steps:
  • Export CMAKE_PREFIX_PATH or pass -Dmodbuscore_DIR pointing at the extracted tree.
  • Verify pkg-config:    PKG_CONFIG_PATH=${prefix}/lib/pkgconfig pkg-config --modversion modbuscore
  • Verify CMake:         cmake -S examples/cmake-consume -B build -Dmodbuscore_DIR=${prefix}/lib/cmake/modbuscore
EOF
