#define XRLIB_NAME "xrlib"
#define XRLIB_VERSION_MAJOR 1
#define XRLIB_VERSION_MINOR 0
#define XRLIB_VERSION_PATCH 0

#ifdef XR_USE_PLATFORM_ANDROID
    #define XRLIB_ASSETS_FOLDER		""
#else
    #define XRLIB_ASSETS_FOLDER		"assets/"
    // #define XRVK_VULKAN_DEBUG2_ENABLE 1
    // #define XRVK_VULKAN_DEBUG3_ENABLE 1
#endif

#define XRLIB_SHADERS_FOLDER		XRLIB_ASSETS_FOLDER "shaders/"
#define XRLIB_MODELS_FOLDER		    XRLIB_ASSETS_FOLDER "models/"
#define XRLIB_TEXTURES_FOLDER		XRLIB_ASSETS_FOLDER "textures/"
