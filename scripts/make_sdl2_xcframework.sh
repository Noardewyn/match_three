#!/usr/bin/env bash
set -euo pipefail

# Build SDL2.xcframework (iOS device + simulator) from SDL's Xcode project.
#
# Usage:
#   ./scripts/make_sdl2_xcframework.sh [SDL_GIT_TAG] [OUTPUT_DIR]
# Example:
#   ./scripts/make_sdl2_xcframework.sh release-2.30.9 external/ios
#
# Env:
#   USE_XC_SCHEME=0   # skip official "xcFramework-iOS" scheme (always fallback)
#   USE_XCPRETTY=0    # disable xcpretty even if installed
#   DEPLOY_TARGET=12.0

SDL_TAG="${1:-release-2.30.9}"
OUT_DIR="${2:-external/ios}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${ROOT_DIR}/build/_sdl_xc"
SRC_DIR="${WORK_DIR}/SDL"
XCODEPROJ="${SRC_DIR}/Xcode/SDL/SDL.xcodeproj"
OUT_XC="${ROOT_DIR}/${OUT_DIR}/SDL2.xcframework"

USE_XC_SCHEME="${USE_XC_SCHEME:-1}"
DEPLOY_TARGET="${DEPLOY_TARGET:-12.0}"

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}" "${ROOT_DIR}/${OUT_DIR}" "${WORK_DIR}/logs"

uniq_rb() { echo "${WORK_DIR}/Result_${1}_$(date +%s)_$$.xcresult"; }

run_xcb() {
  local label="$1"; shift
  local log="${WORK_DIR}/logs/${label}.log"
  echo "---- xcodebuild ($label) -> ${log}"
  set +e
  if command -v xcpretty >/dev/null && [[ "${USE_XCPRETTY:-1}" == "1" ]]; then
    xcodebuild "$@" 2>&1 | tee "${log}" | xcpretty
    status=${PIPESTATUS[0]}
  else
    xcodebuild "$@" 2>&1 | tee "${log}"
    status=${PIPESTATUS[0]}
  fi
  set -e
  return "${status}"
}

echo "==> Cloning SDL (${SDL_TAG})"
git clone --depth 1 --branch "${SDL_TAG}" https://github.com/libsdl-org/SDL.git "${SRC_DIR}"

# 1) Try official xcFramework-iOS scheme
FOUND_XC=""
if [[ "${USE_XC_SCHEME}" == "1" ]]; then
  echo "==> Trying scheme: xcFramework-iOS (Release, iOS ${DEPLOY_TARGET}+)"
  DERIVED_XC="${WORK_DIR}/DerivedData_xc"
  rm -rf "${DERIVED_XC}"; mkdir -p "${DERIVED_XC}"

  if run_xcb "xcframework-ios" \
      -project "${XCODEPROJ}" \
      -scheme "xcFramework-iOS" \
      -configuration Release \
      IPHONEOS_DEPLOYMENT_TARGET="${DEPLOY_TARGET}" \
      ONLY_ACTIVE_ARCH=NO \
      BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
      -derivedDataPath "${DERIVED_XC}" \
      -resultBundlePath "$(uniq_rb xcframework)" \
      clean build; then
    FOUND_XC="$(/usr/bin/find "${DERIVED_XC}/Build/Products" -type d -name 'SDL2.xcframework' -print -quit || true)"
  fi
fi

if [[ -n "${FOUND_XC}" ]]; then
  rm -rf "${OUT_XC}"
  ditto "${FOUND_XC}" "${OUT_XC}"
  echo "==> Done: ${OUT_XC}"
  echo "Logs: ${WORK_DIR}/logs/xcframework-ios.log"
  exit 0
fi

# 2) Fallback: build Static Library-iOS for device + simulator, then create xcframework
echo "==> Fallback: Static Library-iOS (device + simulator) -> create xcframework"

copy_lib_from_dd() {
  local dd="$1" out="$2"
  local lib
  lib="$(/usr/bin/find "${dd}/Build/Products" -type f -name 'libSDL2.a' -print -quit || true)"
  [[ -n "${lib}" ]] || { echo "ERROR: libSDL2.a not found in ${dd}/Build/Products"; exit 1; }
  local bdir="${WORK_DIR}/${out}"
  rm -rf "${bdir}"; mkdir -p "${bdir}"
  ditto "${lib}" "${bdir}/libSDL2.a"
}

build_static () {
  local sdk="$1" dst="$2" out="$3"
  local dd="${WORK_DIR}/DerivedData_${out}"
  rm -rf "${dd}"; mkdir -p "${dd}"

  # Limit arch to arm64 for clarity; simulator: arm64 (Apple Silicon)
  local archs="arm64"
  if [[ "${sdk}" == "iphonesimulator" ]]; then archs="arm64"; fi

  run_xcb "static-${out}" \
    -project "${XCODEPROJ}" \
    -scheme "Static Library-iOS" \
    -configuration Release \
    -sdk "${sdk}" \
    -destination "${dst}" \
    IPHONEOS_DEPLOYMENT_TARGET="${DEPLOY_TARGET}" \
    ARCHS="${archs}" \
    ONLY_ACTIVE_ARCH=NO \
    BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
    -derivedDataPath "${dd}" \
    -resultBundlePath "$(uniq_rb ${out})" \
    clean build

  copy_lib_from_dd "${dd}" "${out}"
}

build_static "iphoneos"        "generic/platform=iOS"           "ios-device"
build_static "iphonesimulator" "generic/platform=iOS Simulator" "ios-sim"

DEVICE_LIB="${WORK_DIR}/ios-device/libSDL2.a"
SIM_LIB="${WORK_DIR}/ios-sim/libSDL2.a"
HEADERS_DIR="${SRC_DIR}/include"

echo "==> Creating SDL2.xcframework"
run_xcb "create-xc" -create-xcframework \
  -library "${DEVICE_LIB}" -headers "${HEADERS_DIR}" \
  -library "${SIM_LIB}"    -headers "${HEADERS_DIR}" \
  -output "${OUT_XC}"

echo "==> Success: ${OUT_XC}"
echo "Logs:"
ls -1 "${WORK_DIR}/logs"
