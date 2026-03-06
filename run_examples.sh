#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build/examples"

# Override from environment if needed.
# Example:
#   CXX=g++-16 CXXFLAGS="-std=gnu++2c -O2 -Iinclude" ./run_examples.sh
if [[ -n "${CXX:-}" ]]; then
  : # use caller-supplied compiler
elif command -v g++-16 >/dev/null 2>&1; then
  CXX="g++-16"
elif command -v g++ >/dev/null 2>&1; then
  CXX="g++"
elif command -v clang++ >/dev/null 2>&1; then
  CXX="clang++"
else
  echo "No suitable C++ compiler found (tried g++-16, g++, clang++)." >&2
  exit 1
fi

if [[ -n "${CXXFLAGS:-}" ]]; then
  # shellcheck disable=SC2206
  FLAGS=(${CXXFLAGS})
else
  case "${CXX}" in
    g++*|*/g++*)
      FLAGS=(-std=gnu++2c -freflection -O2 -I"${ROOT_DIR}/include")
      ;;
    *)
      FLAGS=(-std=c++2c -O2 -I"${ROOT_DIR}/include")
      ;;
  esac
fi

mkdir -p "${BUILD_DIR}"

shopt -s nullglob
SOURCES=("${ROOT_DIR}"/examples/*.cpp)
if [[ ${#SOURCES[@]} -eq 0 ]]; then
  echo "No example .cpp files found in ${ROOT_DIR}/examples."
  exit 1
fi

for SRC in "${SOURCES[@]}"; do
  NAME="$(basename "${SRC}" .cpp)"
  OUT="${BUILD_DIR}/${NAME}"
  echo "Compiling ${NAME}..."
  "${CXX}" "${FLAGS[@]}" "${SRC}" -o "${OUT}"
done

echo "Compiled ${#SOURCES[@]} example(s) into ${BUILD_DIR}."
