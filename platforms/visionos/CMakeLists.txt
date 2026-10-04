# xrlib - visionOS
# Copyright 2026 Rune Berg
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR "visionOS builds require CMake 3.28 or newer")
endif()

if(CMAKE_OSX_SYSROOT MATCHES "simulator")
    message(FATAL_ERROR "The vision-openxr SDK provides device frameworks, use the xros SDK")
endif()

option(BUILD_AS_STATIC "Build as static library" ON)
option(ENABLE_RENDERDOC "Enable renderdoc for render debugs" OFF)
set(VISION_OPENXR_SDK "" CACHE PATH "Public vision-openxr-sdk repo")
set(XRLIB_LIB_OUT "${CMAKE_BINARY_DIR}/lib/$<CONFIG>" CACHE PATH "Library output directory")
set(XRLIB_BIN_OUT "${CMAKE_BINARY_DIR}/bin/$<CONFIG>" CACHE PATH "Binary output directory")

if(VISION_OPENXR_SDK AND NOT TARGET VisionOpenXR::Runtime)
    if(NOT EXISTS "${VISION_OPENXR_SDK}/CMakeLists.txt")
        message(FATAL_ERROR "VISION_OPENXR_SDK must point to the public SDK repo")
    endif()

    set(VISION_OPENXR_BACKEND vulkan)
    add_subdirectory("${VISION_OPENXR_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/vision-openxr-sdk")
endif()

if(TARGET VisionOpenXR::Runtime AND TARGET VisionOpenXR::MoltenVK)
    get_target_property(XRLIB_OPENXR_LIBRARY VisionOpenXR::Runtime IMPORTED_LOCATION)
    get_target_property(Vulkan_LIBRARY VisionOpenXR::MoltenVK IMPORTED_LOCATION)
    get_target_property(Vulkan_INCLUDE_DIR VisionOpenXR::MoltenVK INTERFACE_INCLUDE_DIRECTORIES)
endif()

# Explicit runtime and Vulkan paths are also supported for embedded consumers
if(NOT EXISTS "${XRLIB_OPENXR_LIBRARY}" OR NOT EXISTS "${Vulkan_LIBRARY}" OR NOT EXISTS "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.h")
    message(FATAL_ERROR "Set VISION_OPENXR_SDK to a public SDK checkout with its Vulkan device frameworks and headers")
endif()

set(XRLIB_OPENXR_LIBRARY "${XRLIB_OPENXR_LIBRARY}" CACHE FILEPATH "External OpenXR runtime/loader binary" FORCE)
