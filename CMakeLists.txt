cmake_minimum_required(VERSION 3.21)
project(SafeFlightN1 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

# Stamped into the plugin's description string so a bug report names a build.
# Release builds pass the tag; a working copy reports the project version.
set(SFN1_VERSION "${PROJECT_VERSION}" CACHE STRING "Version reported by the plugin")

# Universal by default; overridable so a single-arch configure can produce a
# compile database clang-tidy can read (it rejects multi-arch compiler jobs).
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    endif()
endif()

set(SFN1_SOURCES
    src/plugin.cpp
    src/plugin_paths.cpp
    src/ini.cpp
    src/window.cpp
    src/segment_display.cpp
    src/layout.cpp
    src/textures.cpp
    src/sim_inputs.cpp
    src/aircraft_gate.cpp
    src/n1_computer.cpp
    src/n1_tables.cpp
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/misc/cpp/imgui_stdlib.cpp
    lib/ImgWindow/ImgWindow.cpp
    lib/ImgWindow/ImgFontAtlas.cpp
)

add_library(SafeFlightN1 MODULE ${SFN1_SOURCES})

# X-Plane looks for the plugin under a per-platform folder inside the plugin
# directory, and expects the .xpl suffix on every platform.
if(APPLE)
    set(SFN1_PLATFORM_DIR "mac_x64")
elseif(WIN32)
    set(SFN1_PLATFORM_DIR "win_x64")
else()
    set(SFN1_PLATFORM_DIR "lin_x64")
endif()

set(SFN1_DIST_DIR "${CMAKE_SOURCE_DIR}/dist/SafeFlightN1")

set_target_properties(SafeFlightN1 PROPERTIES
    PREFIX ""
    SUFFIX ".xpl"
    OUTPUT_NAME "SafeFlightN1"
    LIBRARY_OUTPUT_DIRECTORY "${SFN1_DIST_DIR}/${SFN1_PLATFORM_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${SFN1_DIST_DIR}/${SFN1_PLATFORM_DIR}"
)

# MSVC appends the per-config subdirectory unless every configuration is pinned.
foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set_target_properties(SafeFlightN1 PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${SFN1_DIST_DIR}/${SFN1_PLATFORM_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${SFN1_DIST_DIR}/${SFN1_PLATFORM_DIR}"
    )
endforeach()

target_include_directories(SafeFlightN1 PRIVATE
    src
    lib/imgui
    lib/imgui/misc/cpp
    lib/ImgWindow
    lib/stb
    SDK/CHeaders/XPLM
)

target_compile_definitions(SafeFlightN1 PRIVATE
    XPLM200 XPLM210 XPLM300 XPLM301 XPLM400 XPLM410 XPLM420
    SFN1_VERSION="${SFN1_VERSION}"
)

if(APPLE)
    target_compile_definitions(SafeFlightN1 PRIVATE APL=1 IBM=0 LIN=0 GL_SILENCE_DEPRECATION)
elseif(WIN32)
    # NOMINMAX: SystemGL.h pulls in windows.h, whose min/max macros would
    # otherwise shadow the std:: templates this code uses.
    target_compile_definitions(SafeFlightN1 PRIVATE
        APL=0 IBM=1 LIN=0 NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_definitions(SafeFlightN1 PRIVATE APL=0 IBM=0 LIN=1)
endif()

if(MSVC)
    target_compile_options(SafeFlightN1 PRIVATE /W4 /O2 /permissive-)
else()
    target_compile_options(SafeFlightN1 PRIVATE
        -fvisibility=hidden
        -Wall -Wextra -Wshadow -Wno-unused
        -O2
    )
endif()

if(APPLE)
    target_link_options(SafeFlightN1 PRIVATE
        "-F${CMAKE_SOURCE_DIR}/SDK/Libraries/Mac"
        "SHELL:-framework XPLM"
        "SHELL:-framework OpenGL"
        "SHELL:-framework Cocoa"
    )
elseif(WIN32)
    target_link_libraries(SafeFlightN1 PRIVATE
        "${CMAKE_SOURCE_DIR}/SDK/Libraries/Win/XPLM_64.lib"
        opengl32
    )
else()
    # Linux ships no import library: the SDK's symbols are left undefined and
    # resolved by X-Plane at dlopen time (SDK/README.txt). The runtime libraries
    # are linked statically so the plugin does not require a newer libstdc++
    # than the user's distribution provides.
    find_package(OpenGL REQUIRED)
    target_link_libraries(SafeFlightN1 PRIVATE OpenGL::GL)
    target_link_options(SafeFlightN1 PRIVATE -static-libstdc++ -static-libgcc)
endif()

# Ship the N1 schedule CSVs and the faceplate art alongside the plugin.
# A standalone ALL target so dist stays current even when the .xpl is up to date.
add_custom_target(ship_payload ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "${SFN1_DIST_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SFN1_DIST_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/faceplate.png"
        "${CMAKE_SOURCE_DIR}/assets/knob.png"
        "${CMAKE_SOURCE_DIR}/assets/layout.json"
        "${SFN1_DIST_DIR}/assets"
)
add_dependencies(ship_payload SafeFlightN1)
