# xrlib
# Copyright 2024,2025 Copyright Rune Berg 
# https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
# Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
# SPDX-License-Identifier: Apache-2.0
# 
# This work is the next iteration of OpenXRProvider (v1, v2)
# OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
# OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
# v1 & v2 licensed under MIT: https://opensource.org/license/mit

cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)


######################
# PROJECT DEFINITION #
######################

# Set project variables
set(XRLIB "xrlib")
project("${XRLIB}" VERSION 0.5.0.0)

# Set project directories
set(XRLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(XRLIB_SRC "${XRLIB_ROOT}/src")
set(XRLIB_INCLUDE "${XRLIB_ROOT}/include")
set(XRLIB_BIN_OUT "${XRLIB_ROOT}/bin")
set(XRLIB_LIB_OUT "${XRLIB_ROOT}/lib")
set(THIRD_PARTY "${XRLIB_ROOT}/third_party")
set(XRLIB_EXT_INCLUDE "${XRLIB_INCLUDE}/${XRLIB}/ext")
set(XRLIB_EXT_SRC "${XRLIB_SRC}/${XRLIB}/ext")

set(XRVK_INCLUDE "${XRLIB_INCLUDE}/xrvk")
set(XRVK_SRC "${XRLIB_SRC}/xrvk")
set(XRVK_SHADERS_SRC "${XRLIB_ROOT}/res/shaders/src")
set(XRVK_SHADERS_BIN "${XRLIB_ROOT}/res/shaders/bin")
set(XRVK_MODELS_SRC "${XRLIB_ROOT}/res/models/src")
set(XRVK_MODELS_BIN "${XRLIB_ROOT}/res/models/bin")

# Set project configuration files
set(XRLIB_CONFIG_IN "${XRLIB_ROOT}/project_config.h.in")
set(XRLIB_CONFIG_OUT "${XRLIB_INCLUDE}/project_config.h")

# Set project wide options
option(BUILD_AS_STATIC "Build as static library" OFF)
option(BUILD_SHADERS "Build shaders in resource directory" ON)
option(ENABLE_XRVK "Compile xrvk - pbr render module" ON)
option(ENABLE_RENDERDOC "Enable renderdoc for render debugs" ON) 
option(ENABLE_VULKAN_DEBUG "Enable vulkan debugging" OFF) 

# For windows, we need to override openxr's resource script, so define the rc files here which will be used during binary pre-build
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(SRC_RC "${CMAKE_CURRENT_SOURCE_DIR}/win_dll_metadata.rc")
    set(DST_RC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/openxr/src/loader/loader.rc")

    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${SRC_RC} ${DST_RC})
endif()

# Set config files
file(GLOB XRLIB_CONFIG
        "${XRLIB_ROOT}/.clang-format"
        "${XRLIB_ROOT}/.gitignore"
        "${XRLIB_ROOT}/README.md"
        "${XRLIB_ROOT}/LICENSE" 
        "${XRLIB_ROOT}/CMakeLists.txt" 
        "${XRLIB_ROOT}/*.h.in" 
	)

# Set CI files
file(GLOB XRLIB_CI
        "${XRLIB_ROOT}/.github/workflows/*.yml"
	)

# Set headers
file(GLOB_RECURSE XRLIB_HEADERS
        "${XRLIB_INCLUDE}/*.h*"
        "${XRLIB_INCLUDE}/{$XRLIB}/*.h*"
        "${XRLIB_SRC}/*.h*"
	)

file(GLOB_RECURSE XRVK_HEADERS
        "${XRVK_INCLUDE}/*.h*"
        "${XRVK_SRC}/*.h*"
	)

# Set source code
file(GLOB_RECURSE XRLIB_SOURCE
        "${XRLIB_SRC}/*.c*"
        "${XRLIB_SRC}/{$XRLIB}/*.c*"
	)

file(GLOB_RECURSE XRVK_SOURCE
        "${XRVK_SRC}/*.c*"
	)

# Exclude extensions (option)
macro(exclude_extension option_var file_name)
    if(${option_var})
        list(REMOVE_ITEM XRLIB_HEADERS "${XRLIB_EXT_INCLUDE}/${file_name}.hpp")
        list(REMOVE_ITEM XRLIB_SOURCE "${XRLIB_EXT_SRC}/${file_name}.cpp")
    endif()
endmacro()

option(EXCLUDE_KHR_VISIBILITY_MASK "EXCLUDE_KHR_VISIBILITY_MASK" OFF)
option(EXCLUDE_EXT_HAND_TRACKING "EXCLUDE_EXT_HAND_TRACKING" OFF)
option(EXCLUDE_EXT_HAND_INTERACTION "EXCLUDE_EXT_HAND_INTERACTION" OFF)
option(EXCLUDE_EXT_HAND_JOINTS_MOTION_RANGE "EXCLUDE_EXT_HAND_JOINTS_MOTION_RANGE" OFF)
option(EXCLUDE_FB_DISPLAY_REFRESH "EXCLUDE_FB_DISPLAY_REFRESH" OFF)
option(EXCLUDE_FB_PASSTHROUGH "EXCLUDE_FB_PASSTHROUGH" OFF) 
option(EXCLUDE_FB_TRIANGLE_MESH "EXCLUDE_FB_TRIANGLE_MESH" OFF)
option(EXCLUDE_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS "EXCLUDE_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS" OFF)

exclude_extension(EXCLUDE_KHR_VISIBILITY_MASK "KHR/visibility_mask")
exclude_extension(EXCLUDE_EXT_HAND_TRACKING "EXT/hand_tracking")
exclude_extension(EXCLUDE_EXT_HAND_INTERACTION "EXT/hand_interaction")
exclude_extension(EXCLUDE_EXT_HAND_JOINTS_MOTION_RANGE "EXT/hand_joints_motion_range")
exclude_extension(EXCLUDE_FB_DISPLAY_REFRESH "FB/display_refresh_rate")
exclude_extension(EXCLUDE_FB_PASSTHROUGH "FB/passthrough")
exclude_extension(EXCLUDE_FB_TRIANGLE_MESH "FB/triangle_mesh")
exclude_extension(EXCLUDE_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS "META/simultaneous_hands_and_controllers")

# Set shaders
file(GLOB_RECURSE XRVK_SHADERS
        "${XRVK_SHADERS_SRC}/*.frag"
        "${XRVK_SHADERS_SRC}/*.vert"
        "${XRVK_SHADERS_SRC}/*.tesc"
        "${XRVK_SHADERS_SRC}/*.tese"
        "${XRVK_SHADERS_SRC}/*.geom"
        "${XRVK_SHADERS_SRC}/*.comp"
	)

file(GLOB_RECURSE XRVK_SHADERS_INCLUDE
        "${XRVK_SHADERS_SRC}/*.glsl"
	)

# Compile shaders
if(NOT ANDROID AND BUILD_SHADERS)
    set(SHADER_OUTPUT_FILES)
    foreach(SHADER ${XRVK_SHADERS})
        # Get the filename with extension
        get_filename_component(SHADER_NAME ${SHADER} NAME)

        # Define the output .spv file path
        set(OUTPUT_FILE "${XRVK_SHADERS_BIN}/${SHADER_NAME}.spv") 

        # Add custom command to run glslc for each shader file
        add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND glslc ${SHADER} -o ${OUTPUT_FILE}
            COMMENT "Compiling ${SHADER} to ${OUTPUT_FILE}"
            VERBATIM
            DEPENDS ${SHADER}  # Check if the shader file has changed
        )

        # Collect all output files
        list(APPEND SHADER_OUTPUT_FILES ${OUTPUT_FILE})
    endforeach()

    # Create a custom target that depends on all shader output files
    add_custom_target(_compile_shaders ALL DEPENDS ${SHADER_OUTPUT_FILES})  
endif()

# Set openxr
set(OPENXR_ROOT "${THIRD_PARTY}/openxr")
set(OPENXR_INCLUDE "${OPENXR_ROOT}/include")


######################################
# SET PROJECT TECHNICAL REQUIREMENTS #
######################################

# C++ standard for this project
set(CPP_STD 20)
set(CMAKE_CXX_STANDARD ${CPP_STD})
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "[${XRLIB}] Project language set to C++ ${CPP_STD}")

# Check platform architecture
if(NOT PLATFORM)
	if(CMAKE_SIZEOF_VOID_P MATCHES 8)
	    set(PLATFORM 64)
	else()
        message(FATAL_ERROR "[${XRLIB}] ERROR: Only 64-bit platforms are supported.")
	endif()
endif()

# OS specific
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  add_definitions(-D_WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)
endif()

# Debug settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Assertions enabled
    message(STATUS "[${XRLIB}] Debug build: Assertions enabled.")
else()
    # Disable assertions by defining NDEBUG
    add_definitions(-DNDEBUG)
    message(STATUS "[${XRLIB}] Release build: Assertions disabled.")
endif()

# Advanced debug options (desktop only)
if(NOT ANDROID)
    if(ENABLE_RENDERDOC)
        add_definitions(-DRENDERDOC_ENABLE)
        message(STATUS "[${XRLIB}] DEBUG: Renderdoc frame capture enabled.")
    endif()

    if(ENABLE_VULKAN_DEBUG)
        add_definitions(-DXRVK_VULKAN_VALIDATION_ENABLE -DXRVK_VULKAN_DEBUG_ENABLE )
        message(STATUS "[${XRLIB}] DEBUG: Vulkan validation and debugging enabled.")
    endif()
endif()


##################################
# BUILD THIRD PARTY DEPENDENCIES #
##################################

# Add cmake known packages
find_package(Vulkan)
if(NOT Vulkan_FOUND)
	message(FATAL_ERROR "[${XRLIB}] ERROR: Unable to find Vulkan library - install the Vulkan SDK from https://vulkan.lunarg.com/")
else()
	message(STATUS "[${XRLIB}] Vulkan library loaded: ${Vulkan_LIBRARY}")
endif()

# Add openxr
set(BUILD_WITH_SYSTEM_JSONCPP OFF CACHE BOOL "Force BUILD_WITH_SYSTEM_JSONCPP to OFF" FORCE)
add_subdirectory(${OPENXR_ROOT})
message(STATUS "[${XRLIB}] OpenXR library loaded: ${OPENXR_ROOT} with jsconcpp: ${BUILD_WITH_SYSTEM_JSONCPP}")

# Add TinyGLTF
set(TINYGLTF_USE_CPP14 ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

if(ANDROID)
    set(TINYGLTF_ANDROID_LOAD_FROM_ASSETS ON CACHE INTERNAL "" FORCE)
endif()

add_subdirectory(${THIRD_PARTY}/tinygltf)


#####################
# BINARY DEFINITION #
#####################

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(CI FILES ${XRLIB_CI})
source_group(config FILES ${XRLIB_CONFIG})

source_group(TREE ${XRLIB_INCLUDE} PREFIX include FILES ${XRLIB_HEADERS})
source_group(TREE ${XRLIB_SRC} PREFIX src FILES ${XRLIB_SOURCE})

source_group(TREE ${XRVK_INCLUDE} PREFIX xrvk_include FILES ${XRVK_HEADERS})
source_group(TREE ${XRVK_SRC} PREFIX xrvk_src FILES ${XRVK_SOURCE})
source_group(TREE ${XRVK_SHADERS_SRC} PREFIX xrvk_shaders FILES ${XRVK_SHADERS} ${XRVK_SHADERS_INCLUDE})

# Set project config header which contains this project's current version number
configure_file(${XRLIB_CONFIG_IN} ${XRLIB_CONFIG_OUT})
message(STATUS "[${XRLIB}] Project version is ${CMAKE_PROJECT_VERSION}")

# Assemble all source code (and optional modules/code) that will be compiled for the library
# See "Set Compile Options" section
set (ALLSOURCE 
      ${XRLIB_CONFIG}
      ${XRLIB_CI}
      ${XRLIB_HEADERS} 
      ${XRLIB_SOURCE} 
      $<TARGET_OBJECTS:openxr_loader>    
    )

if(ENABLE_XRVK)
    list(APPEND ALLSOURCE 
            ${XRVK_HEADERS} 
            ${XRVK_SOURCE} 
            ${XRVK_SHADERS}
            ${XRVK_SHADERS_INCLUDE}
    )

    add_compile_definitions(XRVK_ENABLED)
    message(STATUS "[${XRLIB}] Optional module included: xrvk (test with macro XRVK_ENABLED)")
endif()

# Add project header and source files to project (including statically linking openxr_loader object)
if(BUILD_AS_STATIC)
    add_library(${XRLIB} STATIC ${ALLSOURCE})
    message(STATUS "[${XRLIB}] Project library defined. Will build as: STATIC")
else()
    add_library(${XRLIB} SHARED ${ALLSOURCE})
    message(STATUS "[${XRLIB}] Project library defined. Will build as: SHARED")
endif()

# For shared library builds, set project version
if(NOT BUILD_AS_STATIC)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set_target_properties(${XRLIB} PROPERTIES VERSION ${PROJECT_VERSION})
    # else()
        # cmake 3.22 builds multiple copies with version and so version for desktops. commenting for now to avoid CI build duplicates.
        # set_target_properties(${XRLIB} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    endif()
endif()

# Set project public include headers
target_include_directories(${XRLIB} PUBLIC "${XRLIB_SRC}"
                                           "${XRLIB_INCLUDE}"
                                           "${XRLIB_ROOT}"
                                           "${THIRD_PARTY}"
                                           "${OPENXR_INCLUDE}"
                                           "${Vulkan_INCLUDE_DIRS}"
                           )
# For Android, add the native app glue NDK directory
if(ANDROID)
    message(STATUS "[${XRLIB}] Android NDK path: ${ANDROID_NDK}")
    
    # Add native app glue
    find_path(ANDROID_NATIVE_APP_GLUE NAMES android_native_app_glue.h PATHS "${ANDROID_NDK}/sources/android/native_app_glue" NO_DEFAULT_PATH)

    if(NOT ANDROID_NATIVE_APP_GLUE)
        message(WARNING "[${XRLIB}] WARNING: Unable to find native_app_glue in:  **${ANDROID_NDK}/sources/android/native_app_glue** setting manually.")
        set(ANDROID_NATIVE_APP_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")
    endif()

    target_include_directories(${XRLIB} PUBLIC ${ANDROID_NATIVE_APP_GLUE})
endif()

message(STATUS "[${XRLIB}] Public include directories defined.")

# Generate export headers
include(GenerateExportHeader)
generate_export_header(${XRLIB})

message(STATUS "[${XRLIB}] Export headers generated.")


###########################################
# LINK THIRD PARTY DEPENDENCIES TO BINARY #
###########################################

# Add external libraries
if(ANDROID)
    target_compile_definitions(${XRLIB} PRIVATE XR_USE_PLATFORM_ANDROID=1)    

    find_library(ANDROID_LIBRARY NAMES android)
    find_library(ANDROID_LOG_LIBRARY NAMES log)
    target_link_libraries(${XRLIB} PUBLIC 
                          ${Vulkan_LIBRARY}
                          ${ANDROID_LIBRARY} 
                          ${ANDROID_LOG_LIBRARY}
                         )
else()
    target_link_libraries(${XRLIB} PUBLIC 
                          ${Vulkan_LIBRARY}
                         )
endif()

    target_link_libraries(${XRLIB} PUBLIC 
                          tinygltf
                         )

message(STATUS "[${XRLIB}] Third party libraries linked.")


################
# BUILD BINARY #
################

# Set output directories
set_target_properties(${XRLIB} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${XRLIB_LIB_OUT}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${XRLIB_LIB_OUT}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${XRLIB_BIN_OUT}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${XRLIB_LIB_OUT}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${XRLIB_LIB_OUT}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${XRLIB_BIN_OUT}"
)

message(STATUS "[${XRLIB}] Current build/working directory is: ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "[${XRLIB}] Project archives will be built in: ${XRLIB_LIB_OUT}")
message(STATUS "[${XRLIB}] Project libraries will be built in: ${XRLIB_LIB_OUT}")
message(STATUS "[${XRLIB}] Project binaries will be built in: ${XRLIB_BIN_OUT}")

# Post-Build
add_custom_command(TARGET ${XRLIB} POST_BUILD

                    # Create output directories
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${XRLIB_LIB_OUT}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${XRLIB_BIN_OUT}"
                  )

                  
# Compile shaders only if we're not in an android build
if(NOT ANDROID AND BUILD_SHADERS)
    add_custom_command(TARGET ${XRLIB} POST_BUILD
                            COMMAND ${CMAKE_COMMAND} --build . --target _compile_shaders
                      )
endif()
