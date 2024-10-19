# xrlib
# Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
# Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)


######################
# PROJECT DEFINITION #
######################

# Set project variables
set(XRLIB "xrlib")
project("${XRLIB}" VERSION 1.0.0.0)

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

# Set project configuration files
set(XRLIB_CONFIG_IN "${XRLIB_ROOT}/project_config.h.in")
set(XRLIB_CONFIG_OUT "${XRLIB_INCLUDE}/project_config.h")

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
	)

file(GLOB_RECURSE XRVK_SOURCE
        "${XRVK_SRC}/*.c*"
	)

# Exclude extensions (option)
option(EXCLUDE_KHR_VISIBILITY_MASK "EXCLUDE_KHR_VISIBILITY_MASK" OFF) 
set(FILES_KHR_VISIBILITY_MASK "KHR/visibility_mask")

option(EXCLUDE_EXT_HAND_TRACKING "EXCLUDE_EXT_HAND_TRACKING" OFF) 
set (FILES_EXT_HAND_TRACKING  "EXT/hand_tracking" )

option(EXCLUDE_FB_DISPLAY_REFRESH "EXCLUDE_FB_DISPLAY_REFRESH" OFF) 
set (FILES_FB_DISPLAY_REFRESH "FB/display_refresh_rate" )

option(EXCLUDE_FB_PASSTHROUGH "EXCLUDE_FB_PASSTHROUGH" OFF) 
set (FILES_FB_PASSTHROUGH "FB/passthrough" "FB/triangle_mesh")

if (${EXCLUDE_KHR_VISIBILITY_MASK} STREQUAL "ON")
    foreach( f IN ITEMS ${FILES_KHR_VISIBILITY_MASK} )
        list(REMOVE_ITEM XRLIB_HEADERS "${XRLIB_EXT_INCLUDE}/${f}.hpp" )
        list(REMOVE_ITEM XRLIB_SOURCE "${XRLIB_EXT_SRC}/${f}.cpp" )
    endforeach()
endif()

if (${EXCLUDE_EXT_HAND_TRACKING})
    foreach(f IN ITEMS ${FILE_EXT_HAND_TRACKING})
        list(REMOVE_ITEM XRLIB_HEADERS "${XRLIB_EXT_INCLUDE}/${f}.hpp")
        list(REMOVE_ITEM XRLIB_SOURCE "${XRLIB_EXT_SRC}/${f}.cpp")
    endforeach()
endif()

if (${EXCLUDE_FB_DISPLAY_REFRESH})
    foreach(f IN ITEMS ${FILES_FB_DISPLAY_REFRESH} )
        list(REMOVE_ITEM XRLIB_HEADERS "${XRLIB_EXT_INCLUDE}/${f}.hpp")
        list(REMOVE_ITEM XRLIB_SOURCE "${XRLIB_EXT_SRC}/${f}.cpp")
    endforeach()
endif()

if (${EXCLUDE_FB_PASSTHROUGH})
    foreach(f IN ITEMS ${FILES_FB_PASSTHROUGH} )
        list(REMOVE_ITEM XRLIB_HEADERS "${XRLIB_EXT_INCLUDE}/${f}.hpp")
        list(REMOVE_ITEM XRLIB_SOURCE "${XRLIB_EXT_SRC}/${f}.cpp")
    endforeach()
endif()

# Set openxr
set(OPENXR_ROOT "${THIRD_PARTY}/openxr")
set(OPENXR_INCLUDE "${OPENXR_ROOT}/include")


#######################
# SET COMPILE OPTIONS #
#######################

# static build
option(BUILD_AS_STATIC "Build as static library" OFF)

# xrvk - pbr render module
option(ENABLE_XRVK "Compile xrvk - pbr render module" ON)


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

    if(NOT ANDROID)
        # Enable vulkan validation and debugging
        add_definitions(-DXRVK_VULKAN_VALIDATION_ENABLE -DXRVK_VULKAN_DEBUG_ENABLE )
        message(STATUS "[${XRLIB}] Debug build: Vulkan validation and debugging enabled.")
    endif()

else()
    # Disable assertions by defining NDEBUG
    add_definitions(-DNDEBUG)
    message(STATUS "[${XRLIB}] Release build: Assertions disabled.")
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

add_subdirectory(${OPENXR_ROOT})


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