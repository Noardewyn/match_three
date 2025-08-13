#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Build SDL2_ttf.xcframework for iOS (device + simulator).
# Steps:
#   - Build a minimal SDL2 SDK (to provide SDL2Config.cmake for SDL2_ttf).
#   - Fetch vendored freetype/harfbuzz (SDL_ttf 2.24.x requires external/download.sh).
#   - Build SDL2_ttf for device and simulator.
#   - Collect required/optional static libs per platform.
#   - MERGE per-platform libs into ONE combined static library via libtool -static.
#   - Create xcframework from exactly TWO libraries (OS+SIM), passing headers only for SDL2_ttf.
#
# Required in the final combined lib: SDL2_ttf, freetype
# Optional (added if found): harfbuzz, brotli(dec/common/enc), png, bz2, graphite2
#
# Notes:
# - We do NOT pass -headers for deps to avoid xcodebuild arg issues.
# - Your app still links zlib (-lz) from the system (keep it in CMake).
# -----------------------------------------------------------------------------

SDL2_TAG="${SDL2_TAG:-release-2.30.9}"
SDL2_TTF_TAG="${SDL2_TTF_TAG:-release-2.24.0}"
IOS_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET:-13.0}"
SKIP_IF_UP_TO_DATE="${SKIP_IF_UP_TO_DATE:-1}"

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WORK_DIR="${ROOT_DIR}/_build/sdl2_ttf"
OUT_XCF="${ROOT_DIR}/external/ios/SDL2_ttf.xcframework"
STAMP="${WORK_DIR}/.stamp-${SDL2_TAG}-${SDL2_TTF_TAG}-ios${IOS_DEPLOYMENT_TARGET}"

echo "==> SDL2:         ${SDL2_TAG}"
echo "==> SDL2_ttf:     ${SDL2_TTF_TAG}"
echo "==> iOS target:   ${IOS_DEPLOYMENT_TARGET}"
echo "==> Work dir:     ${WORK_DIR}"
echo "==> Output:       ${OUT_XCF}"

command -v cmake >/dev/null || { echo "ERROR: cmake is required"; exit 1; }
command -v xcodebuild >/dev/null || { echo "ERROR: xcodebuild is required"; exit 1; }
command -v curl >/dev/null || { echo "ERROR: curl is required"; exit 1; }
command -v libtool >/dev/null || { echo "ERROR: libtool is required"; exit 1; }

# Fast path (skip if up-to-date)
if [ "${SKIP_IF_UP_TO_DATE}" = "1" ] && [ -d "${OUT_XCF}" ] && [ -f "${STAMP}" ]; then
  echo "==> Up-to-date: ${OUT_XCF} (SKIP_IF_UP_TO_DATE=1)."
  exit 0
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
pushd "${WORK_DIR}" >/dev/null

# -----------------------------------------------------------------------------
# 1) Download sources
# -----------------------------------------------------------------------------
curl -L -o sdl2.tar.gz     "https://github.com/libsdl-org/SDL/archive/refs/tags/${SDL2_TAG}.tar.gz"
curl -L -o sdl2_ttf.tar.gz "https://github.com/libsdl-org/SDL_ttf/archive/refs/tags/${SDL2_TTF_TAG}.tar.gz"
tar -xzf sdl2.tar.gz
tar -xzf sdl2_ttf.tar.gz

SDL2_SRC="$(find . -maxdepth 1 -type d -name 'SDL-*' | head -n1)"
TTF_SRC="$(find . -maxdepth 1 -type d -name 'SDL_ttf-*' | head -n1)"
[ -n "${SDL2_SRC}" ] || { echo "ERROR: SDL2 sources not found"; exit 1; }
[ -n "${TTF_SRC}" ]  || { echo "ERROR: SDL2_ttf sources not found"; exit 1; }
SDL2_SRC="$(cd "${SDL2_SRC}" && pwd)"
TTF_SRC="$(cd "${TTF_SRC}" && pwd)"

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
find_one () { find "$1" -name "$2" -print -quit 2>/dev/null || true; }

build_sdl2_ios () {
  local PLATFORM="$1" SYSROOT="$2" ARCHS="$3"
  local BDIR="${WORK_DIR}/build_sdl2_${PLATFORM}"
  local SDKP="${WORK_DIR}/sdk_sdl2_${PLATFORM}"
  cmake -S "${SDL2_SRC}" -B "${BDIR}" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="${SYSROOT}" \
    -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL_TEST=OFF -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_DISABLE_INSTALL=OFF \
    -DCMAKE_INSTALL_PREFIX="${SDKP}"
  cmake --build "${BDIR}" --config Release --parallel
  cmake --install "${BDIR}" --config Release
}

build_ttf_ios () {
  local PLATFORM="$1" SYSROOT="$2" ARCHS="$3" SDL2_DIR_IN="$4"
  local BDIR="${WORK_DIR}/build_ttf_${PLATFORM}"
  cmake -S "${TTF_SRC}" -B "${BDIR}" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="${SYSROOT}" \
    -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DSDL2_DIR="${SDL2_DIR_IN}" \
    -DSDLTTF_VENDORED=ON \
    -DSDL2TTF_VENDORED=ON
  cmake --build "${BDIR}" --config Release --parallel
}

# -----------------------------------------------------------------------------
# 2) Build & install minimal SDL2 SDK (device + simulator)
# -----------------------------------------------------------------------------
build_sdl2_ios "OS" "iphoneos" "arm64"
build_sdl2_ios "SIMULATOR" "iphonesimulator" "arm64;x86_64"
SDL2_DIR_OS="${WORK_DIR}/sdk_sdl2_OS/lib/cmake/SDL2"
SDL2_DIR_SIM="${WORK_DIR}/sdk_sdl2_SIMULATOR/lib/cmake/SDL2"
[ -f "${SDL2_DIR_OS}/SDL2Config.cmake" ]  || { echo "ERROR: Missing SDL2Config.cmake (OS)"; exit 1; }
[ -f "${SDL2_DIR_SIM}/SDL2Config.cmake" ] || { echo "ERROR: Missing SDL2Config.cmake (SIM)"; exit 1; }

# -----------------------------------------------------------------------------
# 3) Vendored deps for SDL2_ttf (2.24.x requires running download.sh)
# -----------------------------------------------------------------------------
if [ -x "${TTF_SRC}/external/download.sh" ]; then
  (cd "${TTF_SRC}/external" && bash ./download.sh)
else
  chmod +x "${TTF_SRC}/external/download.sh" || true
  (cd "${TTF_SRC}/external" && bash ./download.sh)
fi

# -----------------------------------------------------------------------------
# 4) Build SDL2_ttf (device + simulator)
# -----------------------------------------------------------------------------
build_ttf_ios "OS" "iphoneos" "arm64" "${SDL2_DIR_OS}"
build_ttf_ios "SIMULATOR" "iphonesimulator" "arm64;x86_64" "${SDL2_DIR_SIM}"

# -----------------------------------------------------------------------------
# 5) Collect per-platform libs and MERGE them into single archives
# -----------------------------------------------------------------------------
# Required libs (must exist)
LIB_TTF_OS="$(find_one "${WORK_DIR}/build_ttf_OS"         'libSDL2_ttf.a')";  [ -f "${LIB_TTF_OS}" ]  || { echo "ERROR: libSDL2_ttf.a (OS) not found"; exit 1; }
LIB_TTF_SIM="$(find_one "${WORK_DIR}/build_ttf_SIMULATOR" 'libSDL2_ttf.a')";  [ -f "${LIB_TTF_SIM}" ] || { echo "ERROR: libSDL2_ttf.a (SIM) not found"; exit 1; }
LIB_FT_OS="$(find_one "${WORK_DIR}/build_ttf_OS"          'libfreetype*.a')"; [ -f "${LIB_FT_OS}" ]   || { echo "ERROR: libfreetype.a (OS) not found"; exit 1; }
LIB_FT_SIM="$(find_one "${WORK_DIR}/build_ttf_SIMULATOR"  'libfreetype*.a')"; [ -f "${LIB_FT_SIM}" ]  || { echo "ERROR: libfreetype.a (SIM) not found"; exit 1; }

# Optional globs (include if found on a given platform)
OPTIONAL_GLOBS=(
  'libbrotlicommon*.a' 'libbrotlidec*.a' 'libbrotlienc*.a'  # brotli first
  'libgraphite2*.a'
  'libharfbuzz*.a'
  'libpng*.a'
  'libbz2*.a'
)

# Build per-platform arrays in a safe order
OS_LIBS=( "${LIB_TTF_OS}" "${LIB_FT_OS}" )
SIM_LIBS=( "${LIB_TTF_SIM}" "${LIB_FT_SIM}" )

for glob in "${OPTIONAL_GLOBS[@]}"; do
  oslib="$(find_one "${WORK_DIR}/build_ttf_OS" "${glob}")"
  [ -n "${oslib}" ] && [ -f "${oslib}" ] && OS_LIBS+=( "${oslib}" )
  simlib="$(find_one "${WORK_DIR}/build_ttf_SIMULATOR" "${glob}")"
  [ -n "${simlib}" ] && [ -f "${simlib}" ] && SIM_LIBS+=( "${simlib}" )
done

# Merge into single libs per platform
COMBINED_OS="${WORK_DIR}/libSDL2_ttf_combined_os.a"
COMBINED_SIM="${WORK_DIR}/libSDL2_ttf_combined_sim.a"

echo "==> Merging OS libs into ${COMBINED_OS}"
printf '   %q\n' "${OS_LIBS[@]}"
/usr/bin/libtool -static -o "${COMBINED_OS}" "${OS_LIBS[@]}"

echo "==> Merging SIM libs into ${COMBINED_SIM}"
printf '   %q\n' "${SIM_LIBS[@]}"
/usr/bin/libtool -static -o "${COMBINED_SIM}" "${SIM_LIBS[@]}"

[ -f "${COMBINED_OS}" ]  || { echo "ERROR: failed to create ${COMBINED_OS}"; exit 1; }
[ -f "${COMBINED_SIM}" ] || { echo "ERROR: failed to create ${COMBINED_SIM}"; exit 1; }

# -----------------------------------------------------------------------------
# 6) Prepare headers and create xcframework (exactly two libraries)
# -----------------------------------------------------------------------------
HDR_DIR="${WORK_DIR}/ttf_headers"
rm -rf "${HDR_DIR}"; mkdir -p "${HDR_DIR}"

# Public API header for SDL_ttf
if [ -f "${TTF_SRC}/include/SDL_ttf.h" ]; then
  cp -v "${TTF_SRC}/include/SDL_ttf.h" "${HDR_DIR}/"
elif [ -f "${TTF_SRC}/SDL_ttf.h" ]; then
  cp -v "${TTF_SRC}/SDL_ttf.h" "${HDR_DIR}/"
else
  echo "ERROR: SDL_ttf.h not found"; exit 1
fi

echo "==> Create xcframework"
rm -rf "${OUT_XCF}"
xcodebuild -create-xcframework \
  -library "${COMBINED_OS}"  -headers "${HDR_DIR}" \
  -library "${COMBINED_SIM}" -headers "${HDR_DIR}" \
  -output "${OUT_XCF}"

# Stamp
mkdir -p "$(dirname "${STAMP}")"
date > "${STAMP}"

popd >/dev/null
echo "==> Done: ${OUT_XCF}"
