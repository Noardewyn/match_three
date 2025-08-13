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
if (APPLE)
  if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_IOS TRUE)
  elseif (DEFINED CMAKE_OSX_SYSROOT AND CMAKE_OSX_SYSROOT MATCHES "iphoneos|iphonesimulator")
    set(IS_IOS TRUE)
  endif ()
endif ()
message(STATUS "Platform check: IS_IOS=${IS_IOS}; CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}; CMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")

# -----------------------------------------------------------------------------
# Extra configuration: ReleaseNoOptim (RelNoOpt = NDEBUG + symbols, no opts)
# -----------------------------------------------------------------------------
if (CMAKE_CONFIGURATION_TYPES)
  list(APPEND CMAKE_CONFIGURATION_TYPES ReleaseNoOptim)
  list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "" FORCE)

  string(TOUPPER "ReleaseNoOptim" _CFG)
  if (MSVC)
    set(CMAKE_C_FLAGS_${_CFG}   "${CMAKE_C_FLAGS_${_CFG}} /Od /Zi /DNDEBUG /Oy-")
    set(CMAKE_CXX_FLAGS_${_CFG} "${CMAKE_CXX_FLAGS_${_CFG}} /Od /Zi /DNDEBUG /Oy-")
  else ()
    set(CMAKE_C_FLAGS_${_CFG}   "${CMAKE_C_FLAGS_${_CFG}} -O0 -g -DNDEBUG -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_${_CFG} "${CMAKE_CXX_FLAGS_${_CFG}} -O0 -g -DNDEBUG -fno-omit-frame-pointer")
  endif ()
  unset(_CFG)
endif ()

# -----------------------------------------------------------------------------
# SDL2 dependency
# - Desktop: build from source via FetchContent
# - iOS: link prebuilt SDL2.xcframework / SDL2_ttf.xcframework (do NOT build from source)
# -----------------------------------------------------------------------------
if (NOT IS_IOS AND NOT ANDROID)
  include(FetchContent)

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

  FetchContent_Declare(
    SDL2_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-2.24.0
    CMAKE_ARGS
      -DSDLTTF_VENDORED=ON
      -DSDL2TTF_VENDORED=ON
      -DBUILD_SHARED_LIBS=OFF
      -DSDL2_DIR=${SDL2_BINARY_DIR}
  )
  FetchContent_MakeAvailable(SDL2_ttf)
else ()
  message(STATUS "Skipping SDL FetchContent on this platform (ANDROID=${ANDROID}, IS_IOS=${IS_IOS})")
endif ()

# -----------------------------------------------------------------------------
# App target (code only; assets are added later via target_sources)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES
    "src/*.cpp"
)

add_executable(match_three
    ${GAME_SOURCES}
)

target_include_directories(match_three
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -----------------------------------------------------------------------------
# Assets (fonts, images, etc.)
# - On iOS: embed into the .app bundle under Resources/assets/<relative_path>.
# - On desktop: copy the whole assets directory next to the executable.
#   We strictly ignore hidden files/dirs (any path segment starting with '.').
# -----------------------------------------------------------------------------
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
set(APP_ASSETS "")

if (EXISTS "${ASSETS_DIR}")
  file(GLOB_RECURSE _ALL_ASSETS RELATIVE "${ASSETS_DIR}" "${ASSETS_DIR}/*")

  foreach(_rel IN LISTS _ALL_ASSETS)
    # Skip anything where any path segment starts with a dot (e.g., .DS_Store, .svn, etc.)
    if (_rel MATCHES "(^|/)\\.")
      continue()
    endif ()

    set(_src "${ASSETS_DIR}/${_rel}")
    if (IS_DIRECTORY "${_src}")
      continue()
    endif ()

    list(APPEND APP_ASSETS "${_src}")

    if (IS_IOS)
      # Preserve subdirectory structure under Resources/assets/<rel_dir>
      get_filename_component(_rel_dir "${_rel}" DIRECTORY)
      if (_rel_dir STREQUAL "")
        set(_loc "Resources/assets")
      else ()
        set(_loc "Resources/assets/${_rel_dir}")
      endif ()
      set_source_files_properties("${_src}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_loc}")
    endif ()
  endforeach()
endif ()

# Guard: on iOS we must NOT have SDL CMake targets (we link xcframework instead)
if (IS_IOS)
  if (TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static OR TARGET SDL2::SDL2main)
    message(FATAL_ERROR "Found SDL2 CMake targets on iOS. You are accidentally building SDL from source. Fix the FetchContent guard.")
  endif ()
endif ()

# -----------------------------------------------------------------------------
# iOS bundle (uses prebuilt SDL2.xcframework + SDL2_ttf.xcframework)
# -----------------------------------------------------------------------------
if (IS_IOS)
  enable_language(OBJC)

  set(_plist "${CMAKE_SOURCE_DIR}/platform/ios/Info.plist")
  if (NOT EXISTS "${_plist}")
    message(FATAL_ERROR "Missing Info.plist at ${_plist}")
  endif ()

  set(_launch "${CMAKE_SOURCE_DIR}/platform/ios/LaunchScreen.storyboard")
  if (EXISTS "${_launch}")
    set_source_files_properties(${_launch} PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources
      XCODE_EXPLICIT_FILE_TYPE "file.storyboard"
    )
    target_sources(match_three PRIVATE ${_launch})
  endif ()

  set_target_properties(match_three PROPERTIES
    MACOSX_BUNDLE YES
    MACOSX_BUNDLE_INFO_PLIST ${_plist}
  )

  # Add filtered assets into bundle
  if (APP_ASSETS)
    target_sources(match_three PRIVATE ${APP_ASSETS})
  endif ()

  # Paths to prebuilt xcframeworks
  set(SDL2_XCFRAMEWORK_DIR     "${CMAKE_SOURCE_DIR}/external/ios/SDL2.xcframework"     CACHE PATH "Path to SDL2.xcframework")
  set(SDL2_TTF_XCFRAMEWORK_DIR "${CMAKE_SOURCE_DIR}/external/ios/SDL2_ttf.xcframework" CACHE PATH "Path to SDL2_ttf.xcframework")

  if (NOT EXISTS "${SDL2_XCFRAMEWORK_DIR}")
    message(FATAL_ERROR "SDL2.xcframework not found at ${SDL2_XCFRAMEWORK_DIR}")
  endif ()
  if (NOT EXISTS "${SDL2_TTF_XCFRAMEWORK_DIR}")
    message(FATAL_ERROR "SDL2_ttf.xcframework not found at ${SDL2_TTF_XCFRAMEWORK_DIR}")
  endif ()

  # Header search paths for xcframeworks (multiple layouts)
  set(_HDRS "")
  foreach(p
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/SDL2.framework/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/SDL2.framework/Headers"
    "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/SDL2.framework/Headers"
    "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64/Headers"
    "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/Headers"
    "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator/Headers"
    "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64/SDL2_ttf.framework/Headers"
    "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/SDL2_ttf.framework/Headers"
    "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator/SDL2_ttf.framework/Headers"
  )
    if (EXISTS "${p}")
      list(APPEND _HDRS "${p}")
    endif ()
  endforeach()
  if (_HDRS)
    list(JOIN _HDRS " " _HDRS_JOINED)
    set_target_properties(match_three PROPERTIES
      XCODE_ATTRIBUTE_HEADER_SEARCH_PATHS "${_HDRS_JOINED}"
    )
  endif ()

  # Compose link flags for device/simulator
  set(_FW_SEARCH_PATHS "")
  set(_LDFLAGS_OS  "-ObjC")
  set(_LDFLAGS_SIM "-ObjC")

  set(_SYSFW
    "-framework UIKit -framework Foundation -framework CoreGraphics -framework QuartzCore -framework GameController -framework CoreMotion -framework AVFoundation -framework AVFAudio -framework AudioToolbox -framework CoreAudio -framework CoreHaptics -framework CoreBluetooth -framework Metal -framework OpenGLES"
  )

  # SDL2: framework preferred; else static
  set(_sdl2_fw_dev   "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/SDL2.framework")
  set(_sdl2_fw_sim_a "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/SDL2.framework")
  set(_sdl2_fw_sim_b "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/SDL2.framework")
  if (EXISTS "${_sdl2_fw_dev}" OR EXISTS "${_sdl2_fw_sim_a}" OR EXISTS "${_sdl2_fw_sim_b}")
    list(APPEND _FW_SEARCH_PATHS
      "${SDL2_XCFRAMEWORK_DIR}/ios-arm64"
      "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator"
      "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator"
    )
    set(_LDFLAGS_OS  "${_LDFLAGS_OS} -framework SDL2")
    set(_LDFLAGS_SIM "${_LDFLAGS_SIM} -framework SDL2")
  else ()
    set(_sdl2_lib_dev "${SDL2_XCFRAMEWORK_DIR}/ios-arm64/libSDL2.a")
    set(_sdl2_lib_sim "")
    if (EXISTS "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/libSDL2.a")
      set(_sdl2_lib_sim "${SDL2_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/libSDL2.a")
    elseif (EXISTS "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/libSDL2.a")
      set(_sdl2_lib_sim "${SDL2_XCFRAMEWORK_DIR}/ios-arm64-simulator/libSDL2.a")
    endif ()
    if (EXISTS "${_sdl2_lib_dev}" AND EXISTS "${_sdl2_lib_sim}")
      set(_LDFLAGS_OS  "${_LDFLAGS_OS} -force_load ${_sdl2_lib_dev}")
      set(_LDFLAGS_SIM "${_LDFLAGS_SIM} -force_load ${_sdl2_lib_sim}")
    else ()
      message(FATAL_ERROR "SDL2.xcframework: neither framework nor static slices found")
    endif ()
  endif ()

  # SDL2_ttf: framework preferred; else static fallback (combined first)
  set(_ttf_fw_dev   "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64/SDL2_ttf.framework")
  set(_ttf_fw_sim_a "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/SDL2_ttf.framework")
  set(_ttf_fw_sim_b "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator/SDL2_ttf.framework")
  set(_ttf_has_fw FALSE)
  if (EXISTS "${_ttf_fw_dev}" OR EXISTS "${_ttf_fw_sim_a}" OR EXISTS "${_ttf_fw_sim_b}")
    set(_ttf_has_fw TRUE)
    list(APPEND _FW_SEARCH_PATHS
      "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64"
      "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator"
      "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator"
    )
    set(_LDFLAGS_OS  "${_LDFLAGS_OS} -framework SDL2_ttf")
    set(_LDFLAGS_SIM "${_LDFLAGS_SIM} -framework SDL2_ttf")
  endif ()

  if (NOT _ttf_has_fw)
    set(_combined_dev "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64/libSDL2_ttf_combined_os.a")
    set(_combined_sim "")
    if (EXISTS "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/libSDL2_ttf_combined_sim.a")
      set(_combined_sim "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator/libSDL2_ttf_combined_sim.a")
    elseif (EXISTS "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator/libSDL2_ttf_combined_sim.a")
      set(_combined_sim "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator/libSDL2_ttf_combined_sim.a")
    endif ()

    if (EXISTS "${_combined_dev}" AND EXISTS "${_combined_sim}")
      set(_LDFLAGS_OS  "${_LDFLAGS_OS} -force_load ${_combined_dev}")
      set(_LDFLAGS_SIM "${_LDFLAGS_SIM} -force_load ${_combined_sim}")
    else ()
      set(_xcf_dev_dir "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64")
      set(_xcf_sim_dir "")
      if (EXISTS "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator")
        set(_xcf_sim_dir "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64_x86_64-simulator")
      elseif (EXISTS "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator")
        set(_xcf_sim_dir "${SDL2_TTF_XCFRAMEWORK_DIR}/ios-arm64-simulator")
      endif ()
      if (NOT (EXISTS "${_xcf_dev_dir}" AND EXISTS "${_xcf_sim_dir}"))
        message(FATAL_ERROR "SDL2_ttf.xcframework: device/simulator slices not found")
      endif ()

      file(GLOB _dev_libs "${_xcf_dev_dir}/*.a")
      file(GLOB _sim_libs "${_xcf_sim_dir}/*.a")

      unset(_SIM_MAP)
      foreach(_lib ${_sim_libs})
        get_filename_component(_name "${_lib}" NAME)
        set(_SIM_MAP_${_name} "${_lib}")
      endforeach()

      set(_required_pairs 0)
      foreach(_lib ${_dev_libs})
        get_filename_component(_name "${_lib}" NAME)
        set(_simlib "${_SIM_MAP_${_name}}")
        if (EXISTS "${_simlib}")
          set(_LDFLAGS_OS  "${_LDFLAGS_OS} -force_load ${_lib}")
          set(_LDFLAGS_SIM "${_LDFLAGS_SIM} -force_load ${_simlib}")
          if (_name STREQUAL "libSDL2_ttf.a" OR _name MATCHES "^libfreetype.*\\.a$")
            math(EXPR _required_pairs "${_required_pairs}+1")
          endif ()
        endif ()
      endforeach()

      if (_required_pairs LESS 2)
        message(FATAL_ERROR "SDL2_ttf.xcframework: required libs not found (need at least SDL2_ttf and freetype).")
      endif ()
    endif ()
  endif ()

  # Always link zlib (needed by freetype) and Apple system frameworks
  set(_LDFLAGS_OS  "${_LDFLAGS_OS} -lz ${_SYSFW}")
  set(_LDFLAGS_SIM "${_LDFLAGS_SIM} -lz ${_SYSFW}")

  if (_FW_SEARCH_PATHS)
    list(JOIN _FW_SEARCH_PATHS " " _FW_PATHS_JOINED)
    set_target_properties(match_three PROPERTIES
      XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${_FW_PATHS_JOINED}"
    )
  endif ()

  set_target_properties(match_three PROPERTIES
    XCODE_ATTRIBUTE_OTHER_LDFLAGS[sdk=iphoneos*]        "${_LDFLAGS_OS}"
    XCODE_ATTRIBUTE_OTHER_LDFLAGS[sdk=iphonesimulator*] "${_LDFLAGS_SIM}"
  )

  target_compile_definitions(match_three PRIVATE SDL_VIDEO_DRIVER_UIKIT=1 SDL_VIDEO_DRIVER_COCOA=0)
  target_compile_definitions(match_three PRIVATE USE_SDL_TTF=1)
endif ()

# -----------------------------------------------------------------------------
# Platform defines for conditional compilation
# -----------------------------------------------------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_compile_definitions(match_three PRIVATE PLATFORM_ANDROID=1)
elseif (IS_IOS)
  target_compile_definitions(match_three PRIVATE PLATFORM_IOS=1)
elseif (WIN32)
  target_compile_definitions(match_three PRIVATE PLATFORM_WINDOWS=1)
else ()
  target_compile_definitions(match_three PRIVATE PLATFORM_DESKTOP=1)
endif ()

# -----------------------------------------------------------------------------
# SDL link (desktop only). iOS is handled by xcframework logic above.
# Also copy assets folder next to the executable for desktop.
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
  endif ()

  if (TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(match_three PRIVATE SDL2_ttf::SDL2_ttf)
  elseif (TARGET SDL2_ttf)
    target_link_libraries(match_three PRIVATE SDL2_ttf)
  elseif (TARGET SDL2_ttf-static)
    target_link_libraries(match_three PRIVATE SDL2_ttf-static)
  else ()
    message(FATAL_ERROR "SDL2_ttf CMake target not found. Ensure SDL_ttf FetchContent succeeded.")
  endif ()

  target_compile_definitions(match_three PRIVATE USE_SDL_TTF=1)

  if (EXISTS "${ASSETS_DIR}")
    add_custom_command(TARGET match_three POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${ASSETS_DIR}"
              "$<TARGET_FILE_DIR:match_three>/assets"
      COMMENT "Copying assets to output directory"
    )
  endif ()
endif ()

# -----------------------------------------------------------------------------
# Windows: GUI subsystem (no console) + SDL2main
# -----------------------------------------------------------------------------
if (WIN32)
  set_target_properties(match_three PROPERTIES WIN32_EXECUTABLE YES)
  if (TARGET SDL2::SDL2main)
    target_link_libraries(match_three PRIVATE SDL2::SDL2main)
  endif ()
endif ()

# -----------------------------------------------------------------------------
# Warnings
# -----------------------------------------------------------------------------
if (MSVC)
  target_compile_options(match_three PRIVATE /W4 /permissive-)
else ()
  target_compile_options(match_three PRIVATE -Wall -Wextra -Wpedantic)
endif ()
