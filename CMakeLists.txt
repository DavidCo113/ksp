cmake_minimum_required(VERSION 3.14)
project(KyroSpades C)

set(KYROSPADES_MAJOR 0)
set(KYROSPADES_MINOR 1)
set(KYROSPADES_PATCH 5)

option(ENABLE_TOUCH "Enable touch support including ingame overlay" OFF)
option(ENABLE_ANDROID_FILE "Use SDL's file functions" OFF)
option(ENABLE_OPENGLES "Build for OpenGL ES" OFF)
option(ENABLE_SOUND "Enable sound support using OpenAL" ON)
option(ENABLE_RPC "Enable Discord Rich Presence support" OFF)

# On macOS GLFW has known issues (Ctrl+Shift combos dropped, cursor warping in
# relative mode).  Default to the SDL backend there; use GLFW everywhere else.
# Both options can still be overridden explicitly on the cmake command line.
if(APPLE)
        option(ENABLE_SDL  "Build against SDL backend"   ON)
        option(ENABLE_GLFW "Build against GLFW3 backend" OFF)
        message(STATUS "macOS detected: defaulting to SDL backend")
else()
        option(ENABLE_SDL  "Build against SDL backend"   OFF)
        option(ENABLE_GLFW "Build against GLFW3 backend" ON)
endif()

if((ENABLE_ANDROID_FILE OR ENABLE_TOUCH OR ENABLE_OPENGLES) AND NOT ENABLE_SDL)
        message(FATAL_ERROR "Enable SDL to use ENABLE_ANDROID_FILE, ENABLE_TOUCH or ENABLE_OPENGLES")
endif()
if(ENABLE_SDL AND ENABLE_GLFW)
        message(FATAL_ERROR "Only one backend must be selected")
endif()
if(NOT ENABLE_SDL AND NOT ENABLE_GLFW)
        message(FATAL_ERROR "Enable SDL or GLFW")
endif()

message(STATUS "Compiler is ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
                add_compile_options(-Ofast)
        else()
                add_compile_options(-Ofast -g -march=native)
        endif()
endif()

# Strip absolute build paths from debug info so the compiled binary does not
# leak the CI/dev machine layout (e.g. /home/runner/work/KyroSpades/...).
# -ffile-prefix-map also remaps __FILE__ macros, so assert/error messages
# show clean relative paths instead of the build host's filesystem.
add_compile_options(
        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
        "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=."
)
if(DEFINED ENV{CI_PROJECT_DIR})
        add_compile_options(
                "-ffile-prefix-map=$ENV{CI_PROJECT_DIR}=."
                "-fdebug-prefix-map=$ENV{CI_PROJECT_DIR}=."
        )
endif()

add_subdirectory(src)
