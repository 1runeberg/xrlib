/* 
 * Copyright 2024,2025 Copyright Rune Berg 
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#include <xrlib/vulkan.hpp>
#include <xrlib/common.hpp>
#include <xrlib/utility_functions.hpp>


namespace xrlib
{
	CVulkan::CVulkan( CSession *pSession ) : 
		m_pSession( pSession)
	{ 
		assert( pSession );
	}

	CVulkan::~CVulkan() 
	{
		// Destroy logical device
		if ( m_vkDevice )
			vkDestroyDevice( m_vkDevice, nullptr );
		if ( m_vkInstance )
			vkDestroyInstance( m_vkInstance, nullptr );
	}

	XrResult CVulkan::Init( 
		VkSurfaceKHR *pSurface, 
		void *pVkInstanceNext, 
		void *pXrVkInstanceNext, 
		void *pVkLogicalDeviceNext, 
		void *pXrLogicalDeviceNext ) 
	{ 
		// Check if there's a valid openxr instance
		if ( GetAppInstance()->GetXrInstance() == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Call *required* function GetVulkanGraphicsRequirements
		//     This returns the min-and-max vulkan api level required by the active runtime
		XR_RETURN_ON_ERROR( GetVulkanGraphicsRequirements() );

		// Define app info for vulkan
		VkApplicationInfo vkApplicationInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		vkApplicationInfo.pApplicationName = GetAppInstance()->GetAppName();
		vkApplicationInfo.applicationVersion = static_cast< uint32_t >( GetAppInstance()->GetAppVersion() );
		vkApplicationInfo.pEngineName = XRLIB_NAME;
		vkApplicationInfo.engineVersion = XR_MAKE_VERSION32( XRLIB_VERSION_MAJOR, XRLIB_VERSION_MINOR, XRLIB_VERSION_PATCH );
		vkApplicationInfo.apiVersion = VK_API_VERSION_1_2;

		// Add vulkan layers and extensions the library needs
		for ( auto validationLayer : m_vecValidationLayers )
			vecLayers.push_back( validationLayer );

		for ( auto validationExtension : m_vecValidationExtensions )
			vecExtensions.push_back( validationExtension );

		#ifdef XRVK_VULKAN_DEBUG_ENABLE
				vecExtensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		#endif

		#ifdef XRVK_VULKAN_DEBUG2_ENABLE
				vecExtensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
				vecExtensions.push_back( VK_EXT_DEBUG_MARKER_EXTENSION_NAME );
		#endif

		#ifdef XRVK_VULKAN_DEBUG3_ENABLE
				vecExtensions.push_back( VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME );
		#endif

		// Create vulkan instance
		VkInstanceCreateInfo vkInstanceCreateInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
		vkInstanceCreateInfo.enabledLayerCount = (uint32_t) vecLayers.size();
		vkInstanceCreateInfo.ppEnabledLayerNames = vecLayers.empty() ? nullptr : vecLayers.data();
		vkInstanceCreateInfo.enabledExtensionCount = (uint32_t) vecExtensions.size();
		vkInstanceCreateInfo.ppEnabledExtensionNames = vecExtensions.empty() ? nullptr : vecExtensions.data();

		XrVulkanInstanceCreateInfoKHR xrVulkanInstanceCreateInfo { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
		xrVulkanInstanceCreateInfo.next = pXrVkInstanceNext;
		xrVulkanInstanceCreateInfo.systemId = GetAppInstance()->GetXrSystemId();
		xrVulkanInstanceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
		xrVulkanInstanceCreateInfo.vulkanCreateInfo = &vkInstanceCreateInfo;
		xrVulkanInstanceCreateInfo.vulkanAllocator = nullptr;

		// ... validation
		VkDebugUtilsMessengerCreateInfoEXT vkDebugCreateInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		vkDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		vkDebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		vkDebugCreateInfo.pfnUserCallback = Callback_Debug;
		
		// ... next chain
		vkDebugCreateInfo.pNext = pVkInstanceNext;
		vkInstanceCreateInfo.pNext = pVkInstanceNext;
#ifdef XRVK_VULKAN_DEBUG_ENABLE
		vkInstanceCreateInfo.pNext = &vkDebugCreateInfo;
#endif

		VkResult vkResult = VK_SUCCESS;
		XrResult xrResult = CreateVkInstance( vkResult, GetAppInstance()->GetXrInstance(), &xrVulkanInstanceCreateInfo );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) || vkResult != VK_SUCCESS )
		{
			LogError( "Error creating vulkan instance with openxr result (%s) and vulkan result (%i)", XrEnumToString( xrResult ), (int32_t) vkResult );
			return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
		}
		
		LogInfo( XRLIB_NAME, "Vulkan instance successfully created." );

		// Get the vulkan physical device (gpu) used by the runtime
		xrResult = GetVulkanGraphicsPhysicalDevice();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Create vulkan logical device
		xrResult = CreateVulkanLogicalDevice( pSurface, pVkLogicalDeviceNext, pXrLogicalDeviceNext );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult CVulkan::GetVulkanGraphicsRequirements() 
	{
		if ( GetAppInstance()->IsExtensionEnabled( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) )
		{
			PFN_xrGetVulkanGraphicsRequirements2KHR getRequirements = nullptr;
			XR_RETURN_ON_ERROR( xrGetInstanceProcAddr( GetAppInstance()->GetXrInstance(), "xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast< PFN_xrVoidFunction * >( &getRequirements ) ) );
			return getRequirements( GetAppInstance()->GetXrInstance(), GetAppInstance()->GetXrSystemId(), &m_xrGraphicsRequirements );
		}
		// Get the vulkan graphics requirements (min/max vulkan api version, etc) of the runtime
		PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( GetAppInstance()->GetXrInstance(), xrGetVulkanGraphicsRequirementsKHR ) );

		// We'll retrieve the Vulkan1 requirements and use it as the min/max vulkan api
		XrGraphicsRequirementsVulkanKHR vulkan1Requirements { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
		XR_RETURN_ON_ERROR( xrGetVulkanGraphicsRequirementsKHR( GetAppInstance()->GetXrInstance(), GetAppInstance()->GetXrSystemId(), &vulkan1Requirements ) );
		m_xrGraphicsRequirements.next = vulkan1Requirements.next;
		m_xrGraphicsRequirements.minApiVersionSupported = vulkan1Requirements.minApiVersionSupported;
		m_xrGraphicsRequirements.maxApiVersionSupported = vulkan1Requirements.maxApiVersionSupported;

		return XR_SUCCESS;
	}

	XrResult CVulkan::CreateVkInstance( VkResult &outVkResult, const XrInstance xrInstance, const XrVulkanInstanceCreateInfoKHR *xrVulkanInstanceCreateInfo ) 
	{
		if ( GetAppInstance()->IsExtensionEnabled( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) )
		{
			PFN_xrCreateVulkanInstanceKHR createInstance = nullptr;
			XR_RETURN_ON_ERROR( xrGetInstanceProcAddr( xrInstance, "xrCreateVulkanInstanceKHR", reinterpret_cast< PFN_xrVoidFunction * >( &createInstance ) ) );
			return createInstance( xrInstance, xrVulkanInstanceCreateInfo, &m_vkInstance, &outVkResult );
		}
		// Check vulkan extensions required by the runtime
		PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( xrInstance, xrGetVulkanInstanceExtensionsKHR ) );	
		uint32_t unExtNum = 0;
		XR_RETURN_ON_ERROR( xrGetVulkanInstanceExtensionsKHR( xrInstance, xrVulkanInstanceCreateInfo->systemId, 0, &unExtNum, nullptr ) );

		// Check number of extensions required by the runtime
		std::vector< char > vecRuntimeExtensionNames( unExtNum );
		XR_RETURN_ON_ERROR( xrGetVulkanInstanceExtensionsKHR( xrInstance, xrVulkanInstanceCreateInfo->systemId, unExtNum, &unExtNum, vecRuntimeExtensionNames.data() ) );

		// Combine runtime and app extensions
		std::vector< const char * > vecRuntimeExtensions = ConvertDelimitedCharArray( vecRuntimeExtensionNames.data(), ' ' );
		for ( uint32_t i = 0; i < vecRuntimeExtensions.size(); ++i )
		{
			vecExtensions.push_back( vecRuntimeExtensions[ i ] );
		}

		VkInstanceCreateInfo vkInstanceCreateInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		memcpy( &vkInstanceCreateInfo, xrVulkanInstanceCreateInfo->vulkanCreateInfo, sizeof( vkInstanceCreateInfo ) );
		vkInstanceCreateInfo.enabledExtensionCount = (uint32_t) vecExtensions.size();
		vkInstanceCreateInfo.ppEnabledExtensionNames = vecExtensions.empty() ? nullptr : vecExtensions.data();

		auto pfnCreateInstance = (PFN_vkCreateInstance) xrVulkanInstanceCreateInfo->pfnGetInstanceProcAddr( nullptr, "vkCreateInstance" );
		outVkResult = pfnCreateInstance( &vkInstanceCreateInfo, xrVulkanInstanceCreateInfo->vulkanAllocator, &m_vkInstance );

		return XR_SUCCESS; 
	}

	XrResult CVulkan::GetVulkanGraphicsPhysicalDevice() 
	{ 
		assert( m_vkInstance != VK_NULL_HANDLE );

		if ( GetAppInstance()->IsExtensionEnabled( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) )
		{
			PFN_xrGetVulkanGraphicsDevice2KHR getDevice = nullptr;
			XR_RETURN_ON_ERROR( xrGetInstanceProcAddr( GetAppInstance()->GetXrInstance(), "xrGetVulkanGraphicsDevice2KHR", reinterpret_cast< PFN_xrVoidFunction * >( &getDevice ) ) );
			XrVulkanGraphicsDeviceGetInfoKHR info { XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR };
			info.systemId = GetAppInstance()->GetXrSystemId();
			info.vulkanInstance = m_vkInstance;
			return getDevice( GetAppInstance()->GetXrInstance(), &info, &m_vkPhysicalDevice );
		}
		PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( GetAppInstance()->GetXrInstance(), xrGetVulkanGraphicsDeviceKHR ) );
		XR_RETURN_ON_ERROR( xrGetVulkanGraphicsDeviceKHR( GetAppInstance()->GetXrInstance(), GetAppInstance()->GetXrSystemId(), m_vkInstance, &m_vkPhysicalDevice ) );
		return XR_SUCCESS;
	}

	XrResult CVulkan::CreateVulkanLogicalDevice( VkSurfaceKHR *pSurface, void *pVkLogicalDeviceNext, void *pXrLogicalDeviceNext ) 
	{ 
		assert( m_vkPhysicalDevice != VK_NULL_HANDLE );

		// Setup device queues ( one for graphics and one for presentation to a surface/mirror if available)
		VkDeviceQueueCreateInfo vkDeviceQueueCI_Graphics { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriority_Graphics = 1.f; // highest priority for rendering to hmd
		vkDeviceQueueCI_Graphics.queueCount = 1;
		vkDeviceQueueCI_Graphics.pQueuePriorities = &fQueuePriority_Graphics;

		VkDeviceQueueCreateInfo vkDeviceQueueCI_Transfer { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriority_Transfer = 0.5f; // mid priority for data transfers
		vkDeviceQueueCI_Transfer.queueCount = 1;
		vkDeviceQueueCI_Transfer.pQueuePriorities = &fQueuePriority_Transfer;

		VkDeviceQueueCreateInfo vkDeviceQueueCI_Present { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriority_Present = 0.f; // lowest priority for rendering to a mirror/surface
		vkDeviceQueueCI_Present.queueCount = 1;
		vkDeviceQueueCI_Present.pQueuePriorities = &fQueuePriority_Present;

		// Retrieve all queue families for the physical device
		uint32_t unQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( m_vkPhysicalDevice, &unQueueFamilyCount, nullptr );
		std::vector< VkQueueFamilyProperties > vecQueueFamilyProps( unQueueFamilyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( m_vkPhysicalDevice, &unQueueFamilyCount, vecQueueFamilyProps.data() );

		bool bGraphicsFamilyFound = false;
		bool bPresentFamilyFound = false;
		bool bTransferFamilyFound = false;

		for ( uint32_t i = 0; i < unQueueFamilyCount; ++i )
		{
			// Find an appropriate graphics queue
			if ( !bGraphicsFamilyFound && ( ( vecQueueFamilyProps[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0u ) )
			{
				m_vkQueueIndex_GraphicsFamily = vkDeviceQueueCI_Graphics.queueFamilyIndex = i;
				bGraphicsFamilyFound = true;
			}

			// Find an appropriate transfer queue
			if ( !bTransferFamilyFound && ( ( vecQueueFamilyProps[ i ].queueFlags & VK_QUEUE_TRANSFER_BIT ) != 0u ) )
			{
				m_vkQueueIndex_TransferFamily = vkDeviceQueueCI_Transfer.queueFamilyIndex = i;
				bTransferFamilyFound = true;
			}

			// Find an appropriate present queue
			if ( !bPresentFamilyFound && pSurface )
			{
				vkGetPhysicalDeviceSurfaceSupportKHR( m_vkPhysicalDevice, i, *pSurface, &m_vkSupportsSurfacePresent );
				if ( m_vkSupportsSurfacePresent )
				{
					m_vkQueueIndex_PresentFamily = vkDeviceQueueCI_Present.queueFamilyIndex = i;
					bPresentFamilyFound = true;
				}
			}
			else
			{
				m_vkSupportsSurfacePresent = VK_FALSE;
				if ( bGraphicsFamilyFound && bTransferFamilyFound )
					break;
			}

			if ( bGraphicsFamilyFound && bPresentFamilyFound && bTransferFamilyFound )
				break;
		}

		#if defined( _WIN32 )
			vecLogicalDeviceExtensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
		#endif

		// Setup logical device
			VkPhysicalDeviceFeatures supportedFeatures {};
			vkGetPhysicalDeviceFeatures( m_vkPhysicalDevice, &supportedFeatures );
			vkPhysicalDeviceFeatures.samplerAnisotropy = supportedFeatures.samplerAnisotropy;
			// VkPhysicalDeviceFeatures2 physical_features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
			// physical_features2.features.samplerAnisotropy = VK_TRUE;
			// physical_features2.features.multiViewport = VK_TRUE;
			// vkGetPhysicalDeviceFeatures2( m_SharedState.vkPhysicalDevice, &physical_features2 );

			//// log warning(s) if feature(s) is/are not supported
			// if ( physical_features2.features.samplerAnisotropy == VK_FALSE || m_SharedState.vkPhysicalDeviceFeatures.samplerAnisotropy == VK_FALSE )
			//{
			//	LogWarning( "xrvk", "features.samplerAnisotropy is not available in this gpu, application may not run or render as intended." );
			//  }

			// Add multiview (required)
			vkPhysicalDeviceFeatures.multiViewport = supportedFeatures.multiViewport;
			vkPhysicalDeviceFeatures.sampleRateShading = supportedFeatures.sampleRateShading;
			vecLogicalDeviceExtensions.push_back( VK_KHR_MULTIVIEW_EXTENSION_NAME );

			std::vector< VkDeviceQueueCreateInfo > vecDeviceQueueCIs;
			vecDeviceQueueCIs.push_back( vkDeviceQueueCI_Graphics );

			bool bTransferIsSameAsGraphics = vkDeviceQueueCI_Graphics.queueFamilyIndex == vkDeviceQueueCI_Transfer.queueFamilyIndex;
			if ( !bTransferIsSameAsGraphics )
				vecDeviceQueueCIs.push_back( vkDeviceQueueCI_Transfer );

			bool bPresentIsUnique = ( vkDeviceQueueCI_Graphics.queueFamilyIndex != vkDeviceQueueCI_Present.queueFamilyIndex ) && ( vkDeviceQueueCI_Transfer.queueFamilyIndex != vkDeviceQueueCI_Present.queueFamilyIndex );

			if ( m_vkSupportsSurfacePresent && bPresentIsUnique )
				vecDeviceQueueCIs.push_back( vkDeviceQueueCI_Present );

			VkDeviceCreateInfo vkLogicalDeviceCI { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			vkLogicalDeviceCI.pNext = pVkLogicalDeviceNext; //&physical_features2;
			vkLogicalDeviceCI.queueCreateInfoCount = (uint32_t) vecDeviceQueueCIs.size();
			vkLogicalDeviceCI.pQueueCreateInfos = vecDeviceQueueCIs.data();
			vkLogicalDeviceCI.enabledLayerCount = 0;
			vkLogicalDeviceCI.ppEnabledLayerNames = nullptr;
			vkLogicalDeviceCI.enabledExtensionCount = (uint32_t) vecLogicalDeviceExtensions.size();
			vkLogicalDeviceCI.ppEnabledExtensionNames = vecLogicalDeviceExtensions.empty() ? nullptr : vecLogicalDeviceExtensions.data();
			vkLogicalDeviceCI.pEnabledFeatures = &vkPhysicalDeviceFeatures;

			XrVulkanDeviceCreateInfoKHR xrVulkanDeviceCreateInfo { XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
			xrVulkanDeviceCreateInfo.next = pXrLogicalDeviceNext;
			xrVulkanDeviceCreateInfo.systemId = GetAppInstance()->GetXrSystemId();
			xrVulkanDeviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
			xrVulkanDeviceCreateInfo.vulkanCreateInfo = &vkLogicalDeviceCI;
			xrVulkanDeviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
			xrVulkanDeviceCreateInfo.vulkanAllocator = nullptr;

			// Create logical device
			VkResult vkResult = VK_SUCCESS;
			XrResult xrResult = CreateVkDevice( vkResult, &xrVulkanDeviceCreateInfo );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) || vkResult != VK_SUCCESS )
			{
				LogError( XRLIB_NAME, "Error creating vulkan device with openxr result (%s) and vulkan result (%i)", xrlib::XrEnumToString( xrResult ), (int32_t) vkResult );
				return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
			}

		LogInfo( XRLIB_NAME, "Vulkan (logical) device successfully created." );

		// Get device queue(s)
		vkGetDeviceQueue( m_vkDevice, m_vkQueueIndex_GraphicsFamily, 0, &m_vkQueue_Graphics );
		
		if ( !bTransferIsSameAsGraphics )
		{
			vkGetDeviceQueue( m_vkDevice, m_vkQueueIndex_TransferFamily, 0, &m_vkQueue_Transfer );
		}
		else
		{
			m_vkQueue_Transfer = m_vkQueue_Graphics;
		}
		

		if ( m_vkSupportsSurfacePresent )
		{
			if ( !bPresentIsUnique )
				m_vkQueue_Present = m_vkQueue_Graphics;
			else
				vkGetDeviceQueue( m_vkDevice, m_vkQueueIndex_PresentFamily, 0, &m_vkQueue_Present );
		}

		// Create graphics binding that we will use to create an openxr session
		m_xrGraphicsBinding.instance = m_vkInstance;
		m_xrGraphicsBinding.physicalDevice = m_vkPhysicalDevice;
		m_xrGraphicsBinding.device = m_vkDevice;
		m_xrGraphicsBinding.queueFamilyIndex = m_vkQueueIndex_GraphicsFamily;
		m_xrGraphicsBinding.queueIndex = m_vkQueueIndex_Graphics;

		return XR_SUCCESS;
	}

	XrResult CVulkan::CreateVkDevice( VkResult &outVkResult, const XrVulkanDeviceCreateInfoKHR *xrVulkanDeviceCreateInfo ) 
	{ 
		assert( xrVulkanDeviceCreateInfo );
		assert( m_vkInstance != VK_NULL_HANDLE );
		assert( m_vkPhysicalDevice != VK_NULL_HANDLE );

		if ( GetAppInstance()->IsExtensionEnabled( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) )
		{
			PFN_xrCreateVulkanDeviceKHR createDevice = nullptr;
			XR_RETURN_ON_ERROR( xrGetInstanceProcAddr( GetAppInstance()->GetXrInstance(), "xrCreateVulkanDeviceKHR", reinterpret_cast< PFN_xrVoidFunction * >( &createDevice ) ) );
			return createDevice( GetAppInstance()->GetXrInstance(), xrVulkanDeviceCreateInfo, &m_vkDevice, &outVkResult );
		}
		PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( GetAppInstance()->GetXrInstance(), xrGetVulkanDeviceExtensionsKHR ) );

		// Get number of device extensions needed by the runtime
		uint32_t unDeviceExtensionsNum = 0;
		XR_RETURN_ON_ERROR( xrGetVulkanDeviceExtensionsKHR( GetAppInstance()->GetXrInstance(), xrVulkanDeviceCreateInfo->systemId, 0, &unDeviceExtensionsNum, nullptr ) );

		std::vector< char > vecRuntimeLogicalDeviceExtensionNames( unDeviceExtensionsNum );
		XR_RETURN_ON_ERROR( xrGetVulkanDeviceExtensionsKHR(
			GetAppInstance()->GetXrInstance(), xrVulkanDeviceCreateInfo->systemId, unDeviceExtensionsNum, &unDeviceExtensionsNum, vecRuntimeLogicalDeviceExtensionNames.data() ) );

		// Merge runtime and app's required extensions
		std::vector< const char * > vecRuntimeLogicalDeviceExtensions = ConvertDelimitedCharArray( vecRuntimeLogicalDeviceExtensionNames.data(), ' ' );
		for ( uint32_t i = 0; i < vecRuntimeLogicalDeviceExtensions.size(); ++i )
		{
			vecLogicalDeviceExtensions.push_back( vecRuntimeLogicalDeviceExtensions[ i ] );
		}

		memcpy( &vkPhysicalDeviceFeatures, xrVulkanDeviceCreateInfo->vulkanCreateInfo->pEnabledFeatures, sizeof( vkPhysicalDeviceFeatures ) );

		#if !defined( XR_USE_PLATFORM_ANDROID )
			// Mute validation error with Meta PC runtime
			VkPhysicalDeviceFeatures metaRequiredFeatures {};
			vkGetPhysicalDeviceFeatures( m_vkPhysicalDevice, &metaRequiredFeatures );
			if ( metaRequiredFeatures.shaderStorageImageMultisample == VK_TRUE )
			{
				vkPhysicalDeviceFeatures.shaderStorageImageMultisample = VK_TRUE;
			}
		#endif

		VkDeviceCreateInfo vkDeviceCreateInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		memcpy( &vkDeviceCreateInfo, xrVulkanDeviceCreateInfo->vulkanCreateInfo, sizeof( vkDeviceCreateInfo ) );
		vkDeviceCreateInfo.pEnabledFeatures = &vkPhysicalDeviceFeatures;
		vkDeviceCreateInfo.enabledExtensionCount = (uint32_t) vecLogicalDeviceExtensions.size();
		vkDeviceCreateInfo.ppEnabledExtensionNames = vecLogicalDeviceExtensions.empty() ? nullptr : vecLogicalDeviceExtensions.data();

		auto pfnCreateDevice = (PFN_vkCreateDevice) xrVulkanDeviceCreateInfo->pfnGetInstanceProcAddr( m_vkInstance, "vkCreateDevice" );
		outVkResult = pfnCreateDevice( m_vkPhysicalDevice, &vkDeviceCreateInfo, xrVulkanDeviceCreateInfo->vulkanAllocator, &m_vkDevice );

		return XR_SUCCESS;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL CVulkan::Callback_Debug(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData )
	{
		if ( messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
		{
			LogDebug( "Vulkan", "%s", pCallbackData->pMessage );
		}

		return VK_FALSE;
	}

	const bool CVulkan::IsDepthFormat( VkFormat vkFormat ) 
	{ 
		if ( vkFormat == VK_FORMAT_D16_UNORM || 
			 vkFormat == VK_FORMAT_X8_D24_UNORM_PACK32 || 
			 vkFormat == VK_FORMAT_D32_SFLOAT || 
			 vkFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
			 vkFormat == VK_FORMAT_D24_UNORM_S8_UINT || 
			 vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT )
		{
			return true;
		}

		return false;
	}

	const bool CVulkan::IsStencilFormat( VkFormat vkFormat )
	{
		if ( vkFormat == VK_FORMAT_D16_UNORM_S8_UINT || vkFormat == VK_FORMAT_D24_UNORM_S8_UINT || vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT )
		{
			return true;
		}

		return false;
	}


}

