cmake_minimum_required(VERSION 3.24)

# -----------------------------------------------------------------------------
# Project
# -----------------------------------------------------------------------------
project(match_three LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer static libs on desktop for simpler distribution
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# -----------------------------------------------------------------------------
# Robust iOS detection (Xcode sometimes keeps CMAKE_SYSTEM_NAME as 'Darwin')
# -----------------------------------------------------------------------------
set(IS_IOS FALSE)
if(APPLE)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_IOS TRUE)
  elseif(DEFINED CMAKE_OSX_SYSROOT AND CMAKE_OSX_SYSROOT MATCHES "iphoneos|iphonesimulator")
    set(IS_IOS TRUE)
  endif()
endif()
message(STATUS "Platform check: IS_IOS=${IS_IOS}; CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}; CMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")

# -----------------------------------------------------------------------------
# Extra configuration: ReleaseNoOptim (RelNoOpt = NDEBUG + symbols, no opts)
# -----------------------------------------------------------------------------
if (CMAKE_CONFIGURATION_TYPES)  # multi-config generators (VS/Xcode)
  list(APPEND CMAKE_CONFIGURATION_TYPES ReleaseNoOptim)
  list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "" FORCE)

  string(TOUPPER "ReleaseNoOptim" _CFG)
  if (MSVC)
    set(CMAKE_C_FLAGS_${_CFG}   "${CMAKE_C_FLAGS_${_CFG}} /Od /Zi /DNDEBUG /Oy-")
    set(CMAKE_CXX_FLAGS_${_CFG} "${CMAKE_CXX_FLAGS_${_CFG}} /Od /Zi /DNDEBUG /Oy-")
  else()
    set(CMAKE_C_FLAGS_${_CFG}   "${CMAKE_C_FLAGS_${_CFG}} -O0 -g -DNDEBUG -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_${_CFG} "${CMAKE_CXX_FLAGS_${_CFG}} -O0 -g -DNDEBUG -fno-omit-frame-pointer")
  endif()
  unset(_CFG)
endif()

# -----------------------------------------------------------------------------
# SDL2 dependency
# - Desktop: build from source via FetchContent
# - iOS: link prebuilt SDL2.xcframework (do NOT build from source)
# - Android: to be wired later
# -----------------------------------------------------------------------------
if (NOT IS_IOS AND NOT ANDROID)
  include(FetchContent)

  # Keep SDL lean (no tests/installer targets)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON  CACHE BOOL "" FORCE)
  set(SDL_TEST   OFF CACHE BOOL "" FORCE)
  set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9
  )
  FetchContent_MakeAvailable(SDL2)
else()
  message(STATUS "Skipping SDL FetchContent on this platform (ANDROID=${ANDROID}, IS_IOS=${IS_IOS})")
endif()

# -----------------------------------------------------------------------------
# App target
# -----------------------------------------------------------------------------
add_executable(match_three
  src/main.cpp
  src/game.cpp
  src/board.cpp
  src/renderer.cpp
  src/input.cpp
  src/animation.cpp
  src/visuals.cpp
)

# Guard: on iOS we must NOT have SDL CMake targets (we link xcframework instead)
if (IS_IOS)
  if (TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static OR TARGET SDL2::SDL2main)
    message(FATAL_ERROR "Found SDL2 CMake targets on iOS. You are accidentally building SDL from source. Fix the FetchContent guard.")
  endif()
endif()

# -----------------------------------------------------------------------------
# iOS bundle (uses prebuilt SDL2.xcframework)
# -----------------------------------------------------------------------------
if (IS_IOS)
  enable_language(OBJC)  # we compile platform/ios/SDLMain.m

  # Info.plist (required)
  set(_plist "${CMAKE_SOURCE_DIR}/platform/ios/Info.plist")
  if (NOT EXISTS "${_plist}")
    message(FATAL_ERROR "Missing Info.plist at ${_plist}")
  endif()

  # Optional Launch Screen storyboard
  set(_launch "${CMAKE_SOURCE_DIR}/platform/ios/LaunchScreen.storyboard")
  if (EXISTS "${_launch}")
    set_source_files_properties(${_launch} PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources
      XCODE_EXPLICIT_FILE_TYPE "file.storyboard"
    )
    target_sources(match_three PRIVATE ${_launch})
  endif()

  set_target_properties(match_three PROPERTIES
    MACOSX_BUNDLE YES
    MACOSX_BUNDLE_INFO_PLIST ${_plist}
  )

  # Path to prebuilt SDL2.xcframework
  set(SDL2_XCFRAMEWORK_DIR "${CMAKE_SOURCE_DIR}/external/ios/SDL2.xcframework" CACHE PATH "Path to SDL2.xcframework")
  if (NOT EXISTS "${SDL2_XCFRAMEWORK_DIR}")
    message(FATAL_ERROR "SDL2.xcframework not found at ${SDL2_XCFRAMEWORK_DIR}")
  endif()

  # Build narrow header search paths from available slices (device + simulator)
  set(_HDRS "")
  foreach(p
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/SDL2.framework/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/SDL2.framework/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/SDL2.framework/Headers"
  )
    if (EXISTS "${p}")
      list(APPEND _HDRS "${p}")
    endif()
  endforeach()
  if (_HDRS)
    list(JOIN _HDRS " " _HDRS_JOINED)
    set_target_properties(match_three PROPERTIES
      XCODE_ATTRIBUTE_HEADER_SEARCH_PATHS "${_HDRS_JOINED}"
    )
  endif()

  # Prefer framework-style xcframework if present; else library-style (.a) per-SDK
  set(_fw_dev "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/SDL2.framework")
  set(_fw_sim_a "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/SDL2.framework")
  set(_fw_sim_b "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/SDL2.framework")

  if (EXISTS "${_fw_dev}" OR EXISTS "${_fw_sim_a}" OR EXISTS "${_fw_sim_b}")
    set_target_properties(match_three PROPERTIES
      XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${SDL2_XCFRAMEWORK_DIR}/ios-arm64 ${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator ${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator"
      XCODE_ATTRIBUTE_OTHER_LDFLAGS
        "-ObjC -framework SDL2 -framework UIKit -framework Foundation -framework CoreGraphics -framework QuartzCore -framework GameController -framework CoreMotion -framework AVFoundation -framework AVFAudio -framework AudioToolbox -framework CoreAudio -framework CoreHaptics -framework CoreBluetooth -framework Metal -framework OpenGLES"
    )
  else()
    set(_lib_dev "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/libSDL2.a")
    set(_lib_sim "")
    if (EXISTS "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/libSDL2.a")
      set(_lib_sim "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/libSDL2.a")
    elseif (EXISTS "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/libSDL2.a")
      set(_lib_sim "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/libSDL2.a")
    endif()
    if (NOT EXISTS "${_lib_dev}" OR NOT EXISTS "${_lib_sim}")
      message(FATAL_ERROR "SDL2 static slices not found in ${SDL2_XCFRAMEWORK_DIR}")
    endif()

    # Device flags
    set_target_properties(match_three PROPERTIES
      XCODE_ATTRIBUTE_OTHER_LDFLAGS[sdk=iphoneos*]
        "-ObjC -force_load ${_lib_dev} -framework UIKit -framework Foundation -framework CoreGraphics -framework QuartzCore -framework GameController -framework CoreMotion -framework AVFoundation -framework AVFAudio -framework AudioToolbox -framework CoreAudio -framework CoreHaptics -framework CoreBluetooth -framework Metal -framework OpenGLES"
    )
    # Simulator flags
    set_target_properties(match_three PROPERTIES
      XCODE_ATTRIBUTE_OTHER_LDFLAGS[sdk=iphonesimulator*]
        "-ObjC -force_load ${_lib_sim} -framework UIKit -framework Foundation -framework CoreGraphics -framework QuartzCore -framework GameController -framework CoreMotion -framework AVFoundation -framework AVFAudio -framework AudioToolbox -framework CoreAudio -framework CoreHaptics -framework CoreBluetooth -framework Metal -framework OpenGLES"
    )
  endif()

  # Help SDL headers choose UIKit path (avoid Cocoa includes)
  target_compile_definitions(match_three PRIVATE SDL_VIDEO_DRIVER_UIKIT=1 SDL_VIDEO_DRIVER_COCOA=0)
endif()

# -----------------------------------------------------------------------------
# Platform defines for conditional compilation
# -----------------------------------------------------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_compile_definitions(match_three PRIVATE PLATFORM_ANDROID=1)
elseif (IS_IOS)
  target_compile_definitions(match_three PRIVATE PLATFORM_IOS=1)
elseif (WIN32)
  target_compile_definitions(match_three PRIVATE PLATFORM_WINDOWS=1)
else()
  target_compile_definitions(match_three PRIVATE PLATFORM_DESKTOP=1)
endif()

# -----------------------------------------------------------------------------
# SDL link (desktop only). iOS is handled by xcframework above.
# -----------------------------------------------------------------------------
if (NOT IS_IOS)
  if (TARGET SDL2::SDL2)
    target_link_libraries(match_three PRIVATE SDL2::SDL2)
    target_include_directories(match_three PRIVATE
      $<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>
    )
  elseif (TARGET SDL2::SDL2-static)
    target_link_libraries(match_three PRIVATE SDL2::SDL2-static)
    target_include_directories(match_three PRIVATE
      $<TARGET_PROPERTY:SDL2::SDL2-static,INTERFACE_INCLUDE_DIRECTORIES>
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Windows: GUI subsystem (no console) + SDL2main
# -----------------------------------------------------------------------------
if (WIN32)
  set_target_properties(match_three PROPERTIES WIN32_EXECUTABLE YES)
  if (TARGET SDL2::SDL2main)
    target_link_libraries(match_three PRIVATE SDL2::SDL2main)
  endif()
endif()

# -----------------------------------------------------------------------------
# Warnings
# -----------------------------------------------------------------------------
if (MSVC)
  target_compile_options(match_three PRIVATE /W4 /permissive-)
else()
  target_compile_options(match_three PRIVATE -Wall -Wextra -Wpedantic)
endif()
