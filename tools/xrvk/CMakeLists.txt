# xrvk host asset tools
# Copyright 2026 Rune Berg
# Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
# SPDX-License-Identifier: Apache-2.0

# Added by xrlib desktop builds, or directly by an app's host asset build
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(TARGET xrvk-tools)
    return()
endif()

project(xrvk-tools LANGUAGES C CXX)

set(XRVK_TOOLS_XRLIB "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# The xrvk headers need Vulkan types, the tool doesn't load Vulkan
find_path(XRVK_TOOLS_VULKAN_INCLUDE vulkan/vulkan.h HINTS "${Vulkan_INCLUDE_DIR}" "$ENV{VULKAN_SDK}/include" REQUIRED)

# Host asset builds don't configure xrlib, so they bring in its glTF dependencies here
if(NOT TARGET fastgltf)
    set(BUILD_SHARED_LIBS OFF)
    set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_INSTALL OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${XRVK_TOOLS_XRLIB}/third_party/simdjson" "${CMAKE_CURRENT_BINARY_DIR}/simdjson")
    add_subdirectory("${XRVK_TOOLS_XRLIB}/third_party/fastgltf" "${CMAKE_CURRENT_BINARY_DIR}/fastgltf")
endif()

add_executable(xrvk-tools
    main.cpp
    prepare_environment.cpp
    prepare_gltf.cpp
    tool_utils.cpp
    tools.hpp
    "${XRVK_TOOLS_XRLIB}/src/xrvk/environment_bake.cpp"
    "${XRVK_TOOLS_XRLIB}/src/xrvk/night_grid.cpp"
)

target_compile_features(xrvk-tools PRIVATE cxx_std_20)
target_include_directories(xrvk-tools PRIVATE
    "${XRVK_TOOLS_XRLIB}/include"
    "${XRVK_TOOLS_XRLIB}"
    "${XRVK_TOOLS_XRLIB}/third_party"
    "${XRVK_TOOLS_XRLIB}/third_party/openxr/include"
    "${XRVK_TOOLS_VULKAN_INCLUDE}"
)

target_link_libraries(xrvk-tools PRIVATE fastgltf::fastgltf simdjson::simdjson)

if(WIN32)
    target_compile_definitions(xrvk-tools PRIVATE NOMINMAX)
endif()

# Keep the tool beside xrlib's binaries when built as part of xrlib
if(XRLIB_BIN_OUT)
    set_target_properties(xrvk-tools PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${XRLIB_BIN_OUT}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${XRLIB_BIN_OUT}"
    )
endif()
