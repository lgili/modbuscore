#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: create_release_package.sh [--extra <path>] <build-dir> <staging-dir> <archive-path>

Installs the project from <build-dir> into <staging-dir>, then packs the staged
files into <archive-path>. Optional --extra arguments copy additional files or
directories into the staging directory prior to packaging. The archive format is
inferred from the file suffix:
  - *.tar.gz : POSIX tarball (gzip compressed)

All paths may be absolute or relative. Existing archives will be overwritten.
EOF
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
  usage
  exit 0
fi

extras=()
positional=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --extra)
      shift
      [[ $# -gt 0 ]] || { usage >&2; exit 2; }
      extras+=("$1")
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      positional+=("$1")
      ;;
  esac
  shift
done

if [[ ${#positional[@]} -ne 3 ]]; then
  usage >&2
  exit 2
fi

build_dir=${positional[0]}
staging_dir=${positional[1]}
archive_path=${positional[2]}

if [[ ! -d "${build_dir}" ]]; then
  echo "error: build directory '${build_dir}' not found" >&2
  exit 1
fi

rm -rf "${staging_dir}"
cmake --install "${build_dir}" --prefix "${staging_dir}"

for extra in "${extras[@]}"; do
  if [[ ! -e "${extra}" ]]; then
    echo "warning: extra path '${extra}' not found; skipping" >&2
    continue
  fi
  cp -a "${extra}" "${staging_dir}/"
done

archive_parent=$(dirname "${archive_path}")
mkdir -p "${archive_parent}"
archive_abs=$(cd "${archive_parent}" && pwd)/$(basename "${archive_path}")

case "${archive_path}" in
  *.tar.gz)
    tar_parent=$(dirname "${staging_dir}")
    tar_basename=$(basename "${staging_dir}")
    (cd "${tar_parent}" && tar -czf "${archive_abs}" "${tar_basename}")
    ;;
  *)
    echo "error: unsupported archive format '${archive_path}'" >&2
    exit 1
    ;;
esac
