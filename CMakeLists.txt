cmake_minimum_required(VERSION 3.22)
project(match_three LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer static libs on desktop for simpler distribution
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# --- Add custom config type for multi-config generators (VS/Xcode)
# This adds "ReleaseNoOptim" alongside Debug/Release/RelWithDebInfo/MinSizeRel.
if (CMAKE_CONFIGURATION_TYPES)
  list(APPEND CMAKE_CONFIGURATION_TYPES ReleaseNoOptim)
  list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "" FORCE)
endif()

# --- Compiler flags for ReleaseNoOptim across compilers
# Goal: release-like defines (NDEBUG) + debug symbols + no optimizations + frame pointers.
string(TOUPPER "ReleaseNoOptim" _CFG)
if (MSVC)
  set(CMAKE_C_FLAGS_${_CFG}   "${CMAKE_C_FLAGS_${_CFG}} /Od /Zi /DNDEBUG /Oy-")
  set(CMAKE_CXX_FLAGS_${_CFG} "${CMAKE_CXX_FLAGS_${_CFG}} /Od /Zi /DNDEBUG /Oy-")
else()
  set(CMAKE_C_FLAGS_${_CFG}   "${CMAKE_C_FLAGS_${_CFG}} -O0 -g -DNDEBUG -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS_${_CFG} "${CMAKE_CXX_FLAGS_${_CFG}} -O0 -g -DNDEBUG -fno-omit-frame-pointer")
endif()
unset(_CFG)

include(FetchContent)

# SDL2 for desktop (Windows/macOS/Linux) via FetchContent.
# On Android/iOS we will wire SDL differently (native builds).
if (NOT ANDROID AND NOT IOS)
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9 # stable SDL2 tag; adjust when upgrading
  )
  FetchContent_MakeAvailable(SDL2)
endif()

add_executable(match_three
  src/main.cpp
)

# Platform defines for conditional code if needed later
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_compile_definitions(match_three PRIVATE PLATFORM_ANDROID=1)
elseif (IOS)
  target_compile_definitions(match_three PRIVATE PLATFORM_IOS=1)
elseif (WIN32)
  target_compile_definitions(match_three PRIVATE PLATFORM_WINDOWS=1)
else()
  target_compile_definitions(match_three PRIVATE PLATFORM_DESKTOP=1)
endif()

# Link SDL2 when available (desktop path)
if (TARGET SDL2::SDL2)
  target_link_libraries(match_three PRIVATE SDL2::SDL2)
elseif (TARGET SDL2::SDL2-static)
  target_link_libraries(match_three PRIVATE SDL2::SDL2-static)
endif()

# Windows: build as a GUI app (no console window)
if (WIN32)
  # Tell the linker we are a GUI subsystem binary (no console)
  set_target_properties(match_three PROPERTIES WIN32_EXECUTABLE YES)
  # SDL2main provides WinMain that forwards to SDL_main/main
  if (TARGET SDL2::SDL2main)
    target_link_libraries(match_three PRIVATE SDL2::SDL2main)
  endif()
endif()


# iOS: mark as a GUI app bundle (no console)
if (IOS)
  set_target_properties(match_three PROPERTIES
    MACOSX_BUNDLE YES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.matchthree
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.example.matchthree
  )
endif()
