# xrlib
[![android-latest](https://github.com/1runeberg/xrlib/actions/workflows/android_builds.yml/badge.svg)](https://github.com/1runeberg/xrlib/actions/workflows/android_builds.yml)&nbsp;&nbsp;[![ubuntu-latest](https://github.com/1runeberg/xrlib/actions/workflows/ubuntu_builds.yml/badge.svg)](https://github.com/1runeberg/xrlib/actions/workflows/ubuntu_builds.yml)&nbsp;&nbsp;[![windows-latest](https://github.com/1runeberg/xrlib/actions/workflows/windows_builds.yml/badge.svg)](https://github.com/1runeberg/xrlib/actions/workflows/windows_builds.yml)

A modern OpenXR wrapper library written in C++20 that streamlines XR development while maintaining OpenXR's full capabilities. Built with native Vulkan support, it abstracts complex API interactions without sacrificing performance or flexibility. 

The library features an optional physically-based rendering (PBR) engine specifically optimized for mixed reality applications, enabling developers to create high-fidelity immersive experiences with minimal boilerplate code.

Developers can also quickly bootstrap immersive experiences by inheriting from `XrApp` (link to demos), or leverage the library's individual components for custom implementations.

## Demos
Explore xrlib's capabilities through a collection of example applications available at [xrlib-demos](https://github.com/1runeberg/xrlib-demos). These demos showcase:

- [checkxr](https://github.com/1runeberg/xrlib-demos/tree/main/demo-01_checkxr) - Runtime capability querying and inspection
- [displayxr](https://github.com/1runeberg/xrlib-demos/tree/main/demo-02_displayxr) - Basic XR visualization and rendering
- [passthroughxr](https://github.com/1runeberg/xrlib-demos/tree/main/demo-03_passthroughxr) - Meta Quest Passthrough implementation 
- [handtrackingxr](https://github.com/1runeberg/xrlib-demos/tree/main/demo-04_handtrackingxr) - Hand tracking visualization
- [inputxr](https://github.com/1runeberg/xrlib-demos/tree/main/demo-05_inputxr) - Full-featured input handling, multi-threading via xrlib's xr dynamic thread pool manager, and PBR rendering
- [interactionsxr](https://github.com/1runeberg/xrlib-demos/tree/main/demo-05_interactionsxr) - Demonstrates the EXT Hand Interaction extension, including passthrough and simultaneous hand/controller tracking, and system properties querying.

Each demo provides clear, practical implementation examples designed to highlight specific xrlib features. Desktop (Windows, Linux) and Android builds are supported for all demos.

## Building

### Prerequisites

1. Required Tools
    - CMake 3.22 or higher
    - C++20 compatible compiler
    - Vulkan SDK (from [https://vulkan.lunarg.com/](https://vulkan.lunarg.com/))
    - glslc (Vulkan shader compiler, included in Vulkan SDK)

2. Optional Tools
    - RenderDoc (if building with debug features)

### Building the Library

1. Clone the Repository
    ```bash
    git clone [repository-url]
    cd xrlib
    ```

2. Configure Build Options

The following CMake options are available:

#### Basic Options
    - `BUILD_AS_STATIC`: Build as static library (default: OFF)
    - `BUILD_SHADERS`: Build shaders in resource directory (default: ON)
    - `ENABLE_XRVK`: Compile xrvk - PBR render module (default: ON)

#### Debug Options (Desktop only)
    - `ENABLE_RENDERDOC`: Enable RenderDoc for render debugging (default: ON)
    - `ENABLE_VULKAN_DEBUG`: Enable Vulkan debugging (default: OFF)

#### Extension Exclusion Options
    - `EXCLUDE_KHR_VISIBILITY_MASK`: Exclude KHR visibility mask extension (default: OFF)
    - `EXCLUDE_EXT_HAND_TRACKING`: Exclude hand tracking extension (default: OFF)
    - `EXCLUDE_FB_DISPLAY_REFRESH`: Exclude display refresh rate extension (default: OFF)
    - `EXCLUDE_FB_PASSTHROUGH`: Exclude passthrough extension (default: OFF)

3. Configure and Build

#### Windows
    ```bash
    # Create build directory
    mkdir build
    cd build

    # Configure with CMake
    cmake ..

    # Build
    cmake --build . --config Release
    ```

#### Linux
    ```bash
    # Create build directory
    mkdir build
    cd build

    # Configure with CMake
    cmake ..

    # Build
    make
    ```

#### Android
Additional requirements:
    - Android NDK
    - Android native app glue

    ```bash
    # Create build directory
    mkdir build
    cd build

    # Configure with CMake (adjust paths as needed)
    cmake .. -DANDROID=ON -DANDROID_NDK=/path/to/ndk

    # Build
    cmake --build .
    ```

4. Output Locations

After successful build, you'll find the outputs in:
    - Binaries: `./bin/`
    - Libraries: `./lib/`
    - Shader binaries (if enabled): `./res/shaders/bin/`

## Advanced Configuration

### Custom Build Configuration Example
```bash
cmake .. \
    -DBUILD_AS_STATIC=ON \
    -DENABLE_VULKAN_DEBUG=ON \
    -DEXCLUDE_FB_PASSTHROUGH=ON
```

### Debug Build
```bash
# Configure debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build . --config Debug
```

## Verification

To verify your build:
1. Check that the library files exist in the `lib` directory
2. If building with shaders enabled, verify shader `.spv` files exist in `res/shaders/bin`
3. For debug builds, ensure debug symbols are present

## Troubleshooting

### Common Issues

1. **Vulkan SDK Not Found**
   - Ensure Vulkan SDK is installed
   - Verify `VULKAN_SDK` environment variable is set correctly

2. **Shader Compilation Fails**
   - Verify `glslc` is in your PATH
   - Check shader source files in `res/shaders/src`

3. **Android Build Issues**
   - Verify NDK path is correct
   - Ensure native app glue is available in NDK

## Development

When developing with the library:
    - Header files are in `include` directory
    - Source files are in `src` directory
    - Shader sources are in `res/shaders/src`
    - Build artifacts are placed in `bin` and `lib` directories
