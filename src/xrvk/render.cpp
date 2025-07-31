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


#ifdef XR_USE_PLATFORM_ANDROID
	#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ENABLE_WEBP
#include <tinygltf/tiny_gltf.h>

#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
	#define RENDERDOC_FRAME_SAMPLES 5
	#include <xrvk/renderdoc_api.h>
	#include <Windows.h>

	RENDERDOC_API_1_1_2 *rdoc_api = NULL;
#endif

#include <xrvk/render.hpp>

namespace xrlib
{
	SShader::~SShader() 
	{
		if ( m_vkLogicalDevice != VK_NULL_HANDLE && m_vkShaderModule != VK_NULL_HANDLE )
			vkDestroyShaderModule( m_vkLogicalDevice, m_vkShaderModule, nullptr );
	}

	#ifdef XR_USE_PLATFORM_ANDROID
		VkPipelineShaderStageCreateInfo SShader::Init(
			AAssetManager *assetManager,
			VkDevice vkLogicalDevice, 
			VkShaderStageFlagBits shaderStage,
			VkShaderModuleCreateFlags createFlags,
			void *pNext )
	#else
	VkPipelineShaderStageCreateInfo SShader::Init( 
		VkDevice vkLogicalDevice, 
		VkShaderStageFlagBits shaderStage, 
		VkShaderModuleCreateFlags createFlags, 
		void *pNext ) 
	#endif
	{ 
		assert( vkLogicalDevice != VK_NULL_HANDLE );
		assert( shaderStage != 0 );

        m_vkLogicalDevice = vkLogicalDevice;

		// Read shader from disk
		#ifdef XR_USE_PLATFORM_ANDROID	
			auto spirvShaderCode = ReadBinaryFile( assetManager, m_sFilename );
		#else
			auto spirvShaderCode = ReadBinaryFile( m_sFilename );
		#endif

		// Create shader module
		VkShaderModuleCreateInfo shaderModuleCI { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		shaderModuleCI.pNext = pNext;
		shaderModuleCI.flags = createFlags;
		shaderModuleCI.codeSize = spirvShaderCode.size();
		shaderModuleCI.pCode = reinterpret_cast< const uint32_t * >( spirvShaderCode.data() );

		if ( vkCreateShaderModule( vkLogicalDevice, &shaderModuleCI, nullptr, &m_vkShaderModule ) != VK_SUCCESS )
			throw std::runtime_error( "failed to create shader module!" );

		// Assemble shader stage
		VkPipelineShaderStageCreateInfo shaderStageCI { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		shaderStageCI.pNext = nullptr;
		shaderStageCI.flags = 0;
		shaderStageCI.stage = shaderStage;
		shaderStageCI.module = m_vkShaderModule;
		shaderStageCI.pName = m_sEntrypoint.c_str();

		return shaderStageCI; 
	}

	SShaderSet::SShaderSet( 
		std::string sVertexShaderFilename, 
		std::string sFragmentShaderFilename, 
		std::string sVertexShaderEntrypoint, 
		std::string sFragmentShaderEntrypoint ) 
	{
		assert( !sVertexShaderFilename.empty() );
		assert( !sFragmentShaderFilename.empty() );
		assert( !sVertexShaderEntrypoint.empty() );
		assert( !sFragmentShaderEntrypoint.empty() );

		vertexShader = new SShader( sVertexShaderFilename, sVertexShaderEntrypoint );
		fragmentShader = new SShader( sFragmentShaderFilename, sFragmentShaderEntrypoint );
	}

	SShaderSet::~SShaderSet() 
	{
		if ( vertexShader )
			delete vertexShader;

		if ( fragmentShader )
			delete fragmentShader;
	}

	#ifdef XR_USE_PLATFORM_ANDROID

	void SShaderSet::Init(
		AAssetManager *assetManager,
		VkDevice vkLogicalDevice,
		VkShaderStageFlagBits vertexShaderStage,
		VkShaderStageFlagBits fragmentShaderStage,
		VkShaderModuleCreateFlags vertexShaderCreateFlags,
		VkShaderModuleCreateFlags fragmentShaderCreateFlags,
		void *pVertexNext,
		void *pFragmentNext )
	{
		stages.clear();

		stages.push_back( vertexShader->Init( assetManager, vkLogicalDevice, vertexShaderStage, vertexShaderCreateFlags, pVertexNext ) );
		stages.push_back( fragmentShader->Init( assetManager, vkLogicalDevice, fragmentShaderStage, fragmentShaderCreateFlags, pFragmentNext ) );
	}

	#else

	void SShaderSet::Init(
		VkDevice vkLogicalDevice,
		VkShaderStageFlagBits vertexShaderStage,
		VkShaderStageFlagBits fragmentShaderStage,
		VkShaderModuleCreateFlags vertexShaderCreateFlags,
		VkShaderModuleCreateFlags fragmentShaderCreateFlags,
		void *pVertexNext,
		void *pFragmentNext )
	{ 
		#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
			if ( HMODULE mod = GetModuleHandleA( "renderdoc.dll" ) )
			{
				pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) GetProcAddress( mod, "RENDERDOC_GetAPI" );
				int ret = RENDERDOC_GetAPI( eRENDERDOC_API_Version_1_1_2, (void **) &rdoc_api );
				assert( ret == 1 );
			}
		#endif

		stages.clear();

		stages.push_back( vertexShader->Init( vkLogicalDevice, vertexShaderStage, vertexShaderCreateFlags, pVertexNext ) );
		stages.push_back( fragmentShader->Init( vkLogicalDevice, fragmentShaderStage, fragmentShaderCreateFlags, pFragmentNext ) );
	}

	#endif

	CStereoRender::CStereoRender( CSession *pSession, VkFormat vkColorFormat, VkFormat vkDepthFormat )
		: m_pSession( pSession), 
		  m_vkColorFormat( vkColorFormat ),
		  m_vkDepthFormat( vkDepthFormat ) 
	{
		// Validate session
		assert( pSession );
		assert( pSession->GetXrSession() != XR_NULL_HANDLE );

		// Set asset manager for tinygltf (android only)
		#ifdef XR_USE_PLATFORM_ANDROID
				tinygltf::asset_manager = pSession->GetAppInstance()->GetAssetManager();
		#endif

		// Validate session's current view configuration
		assert( pSession->xrViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO );

		// Validate texture formats
		std::vector< int64_t > supportedFormats;
		assert ( XR_UNQUALIFIED_SUCCESS( pSession->GetSupportedTextureFormats( supportedFormats ) ) );

		bool bFoundColor = false;
		bool bFoundDepth = false;

		for ( auto &supportedFormat : supportedFormats )
		{
			if ( bFoundColor && bFoundDepth )
				break;

			if ( !bFoundColor && ( (VkFormat) supportedFormat == vkColorFormat ) )
				bFoundColor = true;

			if ( !bFoundDepth && ( (VkFormat) supportedFormat == vkDepthFormat ) )
				bFoundDepth = true;
		}

		assert( bFoundColor && bFoundDepth );
		assert( !pSession->GetVulkan()->IsDepthFormat( vkColorFormat ) );
		assert( pSession->GetVulkan()->IsDepthFormat( vkDepthFormat ) );
	};

	CStereoRender::~CStereoRender() 
	{
		if ( GetLogicalDevice() != VK_NULL_HANDLE )
		{
			// Destroy render views
			for ( auto &renderTarget : m_vecMultiviewRenderTargets )
			{
				if ( renderTarget.vkColorImageDescriptor.imageView != VK_NULL_HANDLE )
					vkDestroyImageView( GetLogicalDevice(), renderTarget.vkColorImageDescriptor.imageView, nullptr );

				if ( renderTarget.vkColorImageDescriptor.imageView != VK_NULL_HANDLE )
					vkDestroyImageView( GetLogicalDevice(), renderTarget.vkColorImageDescriptor.imageView, nullptr );

				if ( renderTarget.vkDepthImageView != VK_NULL_HANDLE )
					vkDestroyImageView(GetLogicalDevice(), renderTarget.vkDepthImageView, nullptr );
			}

			// Destroy render pass
			for ( auto renderPass : vecRenderPasses )
			{
				if ( renderPass != VK_NULL_HANDLE )
					vkDestroyRenderPass( GetLogicalDevice(), renderPass, nullptr );
			}

			// Destroy samplers, frame buffers and fences
			for ( auto &renderTarget : m_vecMultiviewRenderTargets )
			{
				if ( renderTarget.vkColorImageDescriptor.sampler != VK_NULL_HANDLE )
					vkDestroySampler( GetLogicalDevice(), renderTarget.vkColorImageDescriptor.sampler, nullptr );

				if ( renderTarget.vkFrameBuffer != VK_NULL_HANDLE )
					vkDestroyFramebuffer( GetLogicalDevice(), renderTarget.vkFrameBuffer, nullptr );

				if ( renderTarget.vkRenderCommandFence != VK_NULL_HANDLE )
					vkDestroyFence( GetLogicalDevice(), renderTarget.vkRenderCommandFence, nullptr );
			}
		}

		if ( m_xrColorSwapchain != XR_NULL_HANDLE )
			xrDestroySwapchain( m_xrColorSwapchain );

		if ( m_xrDepthSwapchain != XR_NULL_HANDLE )
			xrDestroySwapchain( m_xrDepthSwapchain );

		if ( m_vkTransferCommandPool != VK_NULL_HANDLE )
			vkDestroyCommandPool( GetLogicalDevice(), m_vkTransferCommandPool, nullptr );

		if ( m_vkRenderCommandPool != VK_NULL_HANDLE )
			vkDestroyCommandPool( GetLogicalDevice(), m_vkRenderCommandPool, nullptr );
	}

	XrResult CStereoRender::Init( uint32_t unTextureFaceCount, uint32_t unTextureMipCount ) 
	{ 
		// Check if openxr session is valid
		if ( m_pSession->GetXrSession() == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Create swapchains
		XR_RETURN_ON_ERROR( CreateSwapchains( unTextureFaceCount, unTextureMipCount ) );
		XR_RETURN_ON_ERROR( CreateSwapchainImages( m_vecSwapchainColorImages, m_xrColorSwapchain ) );
		XR_RETURN_ON_ERROR( CreateSwapchainImages( m_vecSwapchainDepthImages, m_xrDepthSwapchain ) );

		// Create command pools
		VkCommandPoolCreateInfo commandPoolCI { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		commandPoolCI.queueFamilyIndex = GetAppSession()->GetVulkan()->GetVkQueueIndex_GraphicsFamily();
		commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VkResult result = vkCreateCommandPool( GetLogicalDevice(), &commandPoolCI, nullptr, &m_vkRenderCommandPool );
		assert( result == VK_SUCCESS );

		commandPoolCI.queueFamilyIndex = GetAppSession()->GetVulkan()->GetVkQueueIndex_TransferFamily();
		commandPoolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		result = vkCreateCommandPool( GetLogicalDevice(), &commandPoolCI, nullptr, &m_vkTransferCommandPool );
		assert( result == VK_SUCCESS );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateSwapchains( uint32_t unFaceCount, uint32_t unMipCount ) 
	{ 
		assert( m_pSession->GetVulkan()->GetVkPhysicalDevice() != VK_NULL_HANDLE );

		// Update eye view configurations
		uint32_t unEyeConfigNum = 0;
		m_vecEyeConfigs.clear();

		// Get number of configuration views for this session
		XR_RETURN_ON_ERROR( xrEnumerateViewConfigurationViews( 
			GetAppInstance()->GetXrInstance(), 
			GetAppInstance()->GetXrSystemId(), 
			XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			unEyeConfigNum, 
			&unEyeConfigNum, 
			nullptr ) );

		if ( unEyeConfigNum != k_EyeCount )
			return XR_ERROR_VALIDATION_FAILURE;

		// Update eye view configurations
		m_vecEyeConfigs.resize( unEyeConfigNum, { XR_TYPE_VIEW_CONFIGURATION_VIEW } );
		XR_RETURN_ON_ERROR( xrEnumerateViewConfigurationViews( 
			GetAppInstance()->GetXrInstance(), 
			GetAppInstance()->GetXrSystemId(), 
			XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			unEyeConfigNum, 
			&unEyeConfigNum, 
			m_vecEyeConfigs.data() ) );


		// Create eye views that will contain up to date fov, pose, etc
		m_vecEyeViews.resize( unEyeConfigNum, { XR_TYPE_VIEW } );

		// Validate image array support
		VkPhysicalDeviceProperties vkPhysicalDeviceProps;
		vkGetPhysicalDeviceProperties( m_pSession->GetVulkan()->GetVkPhysicalDevice(), &vkPhysicalDeviceProps );
			
		if ( vkPhysicalDeviceProps.limits.maxImageArrayLayers < k_EyeCount )
		{
			LogError( "", "Device does not support image arrays." );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		// Create color swapchain
		XrSwapchainCreateInfo xrSwapchainCreateInfo { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		xrSwapchainCreateInfo.format = m_vkColorFormat;
		xrSwapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		xrSwapchainCreateInfo.arraySize = k_EyeCount;
		xrSwapchainCreateInfo.width = m_unTextureWidth = m_vecEyeConfigs[ 0 ].recommendedImageRectWidth;
		xrSwapchainCreateInfo.height = m_unTextureHeight = m_vecEyeConfigs[ 0 ].recommendedImageRectHeight;
		xrSwapchainCreateInfo.mipCount = unMipCount;
		xrSwapchainCreateInfo.faceCount = unFaceCount;
		xrSwapchainCreateInfo.sampleCount = m_vecEyeConfigs[ 0 ].recommendedSwapchainSampleCount;

		XR_RETURN_ON_ERROR( xrCreateSwapchain( m_pSession->GetXrSession(), &xrSwapchainCreateInfo, &m_xrColorSwapchain ) );

		// Create depth swapchain
		xrSwapchainCreateInfo.format = m_vkDepthFormat;
		xrSwapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		XR_RETURN_ON_ERROR( xrCreateSwapchain( m_pSession->GetXrSession(), &xrSwapchainCreateInfo, &m_xrDepthSwapchain ) );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateSwapchainImages( std::vector< XrSwapchainImageVulkan2KHR > &outSwapchainImages, XrSwapchain xrSwapchain ) 
	{ 
		// Validate swpachain
		if ( xrSwapchain == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Determine number of swapchain images generated by the runtime
		uint32_t unNumOfSwapchainImages = 0;
		XR_RETURN_ON_ERROR( xrEnumerateSwapchainImages( xrSwapchain, unNumOfSwapchainImages, &unNumOfSwapchainImages, nullptr ) );

		// Create swapchain images
		outSwapchainImages.clear();
		outSwapchainImages.resize( unNumOfSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR } );

		XR_RETURN_ON_ERROR ( xrEnumerateSwapchainImages(
			xrSwapchain,
			unNumOfSwapchainImages,
			&unNumOfSwapchainImages, 
			reinterpret_cast< XrSwapchainImageBaseHeader * >( outSwapchainImages.data() ) ) );

		// Log swapchain creation 
		if ( CheckLogLevelVerbose( GetMinLogLevel() ) )
			LogVerbose( "", "Swapchain created with %i images.", unNumOfSwapchainImages );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateRenderPass( VkRenderPass &outRenderPass, bool bUseVisMask ) 
	{ 
		// Setup render pass artifacts - render targets
		XR_RETURN_ON_ERROR( InitRendering_Multiview() );

		// Create render pass
		XR_RETURN_ON_ERROR( CreateRenderPass_Multiview( bUseVisMask ) );
		outRenderPass = vecRenderPasses.back();

		// Create frame buffers for this render pass
		XR_RETURN_ON_ERROR( CreateFramebuffers_Multiview( outRenderPass ) );

		return XR_SUCCESS;
	}

	XrResult CStereoRender::InitRendering_Multiview() 
	{ 
		XR_RETURN_ON_ERROR( CreateRenderTargets_Multiview() );

		VkSamplerCreateInfo samplerCI = GenerateImageSamplerCI();
		XR_RETURN_ON_ERROR( CreateRenderTargetSamplers( samplerCI ) ); 

		return XR_SUCCESS; 
	}

	XrResult CStereoRender::CreateRenderTargets_Multiview( 
		VkImageViewCreateFlags colorCreateFlags, 
		VkImageViewCreateFlags depthCreateFlags, 
		void *pColorNext,
		void *pDepthNext,
		VkAllocationCallbacks *pCallbacks ) 
	{ 
		// Call order validation
		if ( m_vecSwapchainColorImages.empty() || m_vecSwapchainDepthImages.empty() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( m_vkRenderCommandPool == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Swapchain image count
		uint32_t unSwapchainImageCount = (uint32_t) m_vecSwapchainColorImages.size();
		if ( unSwapchainImageCount != (uint32_t) m_vecSwapchainDepthImages.size() )
		{
			LogError( "", "Error creating multiview render targets: color & depth swpachain must be of equal length!" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		// Create render targets
		for ( uint32_t i = 0; i < unSwapchainImageCount; i++ )
		{
			m_vecMultiviewRenderTargets.push_back( {} );

			// Create msaa color image, custom settings for multiview/MSAA
			VkImageCreateInfo msaaImageCI { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			msaaImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			msaaImageCI.imageType = VK_IMAGE_TYPE_2D;
			msaaImageCI.format = m_vkColorFormat;
			msaaImageCI.extent.width = m_unTextureWidth;
			msaaImageCI.extent.height = m_unTextureHeight;
			msaaImageCI.extent.depth = 1;
			msaaImageCI.mipLevels = 1;
			msaaImageCI.arrayLayers = k_EyeCount;		 
			msaaImageCI.samples = VK_SAMPLE_COUNT_2_BIT; 
			msaaImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			msaaImageCI.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			msaaImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			msaaImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VkImage msaaImage;
			VkDeviceMemory msaaMemory;
			VK_CHECK_RESULT( vkCreateImage( GetLogicalDevice(), &msaaImageCI, nullptr, &msaaImage ) );

			// Get memory requirements and allocate
			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements( GetLogicalDevice(), msaaImage, &memRequirements );

			VkMemoryAllocateInfo allocInfo {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = vkutils::FindMemoryTypeWithFallback( GetPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

			VK_CHECK_RESULT( vkAllocateMemory( GetLogicalDevice(), &allocInfo, nullptr, &msaaMemory ) );
			VK_CHECK_RESULT( vkBindImageMemory( GetLogicalDevice(), msaaImage, msaaMemory, 0 ) );

			m_vecMultiviewRenderTargets.back().vkMSAAColorTexture = msaaImage;

			// Runtime provided image becomes msaa resolve target
			m_vecMultiviewRenderTargets.back().vkColorTexture = m_vecSwapchainColorImages[ i ].image;

			// Create MSAA image view
			VkImageViewCreateInfo msaaViewCI { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			msaaViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			msaaViewCI.image = msaaImage;
			msaaViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; 
			msaaViewCI.format = m_vkColorFormat;
			msaaViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			msaaViewCI.subresourceRange.baseMipLevel = 0;
			msaaViewCI.subresourceRange.levelCount = 1;
			msaaViewCI.subresourceRange.baseArrayLayer = 0;
			msaaViewCI.subresourceRange.layerCount = k_EyeCount; 

			VK_CHECK_RESULT( vkCreateImageView( GetLogicalDevice(), &msaaViewCI, nullptr, &m_vecMultiviewRenderTargets.back().vkMSAAColorView ) );

			// Color image view (image from openxr runtime, will be used as the msaa resolve image)
			VkImageViewCreateInfo imageViewCI { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			imageViewCI.pNext = pColorNext;
			imageViewCI.flags = colorCreateFlags;
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			imageViewCI.format = m_vkColorFormat;

			imageViewCI.subresourceRange = {};
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCI.subresourceRange.baseMipLevel = 0;
			imageViewCI.subresourceRange.levelCount = 1;
			imageViewCI.subresourceRange.baseArrayLayer = 0;
			imageViewCI.subresourceRange.layerCount = k_EyeCount;

			imageViewCI.image = m_vecMultiviewRenderTargets.back().vkColorTexture;
			
			VkResult result = vkCreateImageView( 
				GetLogicalDevice(), 
				&imageViewCI, 
				pCallbacks, 
				&m_vecMultiviewRenderTargets.back().vkColorImageDescriptor.imageView );
			assert( result == VK_SUCCESS );

			// Create MSAA depth image
			VkImageCreateInfo msaaDepthCI { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			msaaDepthCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			msaaDepthCI.imageType = VK_IMAGE_TYPE_2D;
			msaaDepthCI.format = m_vkDepthFormat;
			msaaDepthCI.extent.width = m_unTextureWidth;
			msaaDepthCI.extent.height = m_unTextureHeight;
			msaaDepthCI.extent.depth = 1;
			msaaDepthCI.mipLevels = 1;
			msaaDepthCI.arrayLayers = k_EyeCount;
			msaaDepthCI.samples = VK_SAMPLE_COUNT_2_BIT;
			msaaDepthCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			msaaDepthCI.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			msaaDepthCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			msaaDepthCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VkImage msaaDepthImage;
			VkDeviceMemory msaaDepthMemory;
			VK_CHECK_RESULT( vkCreateImage( GetLogicalDevice(), &msaaDepthCI, nullptr, &msaaDepthImage ) );

			// Get memory requirements and allocate
			VkMemoryRequirements depthMemRequirements;
			vkGetImageMemoryRequirements( GetLogicalDevice(), msaaDepthImage, &depthMemRequirements );

			VkMemoryAllocateInfo depthAllocInfo {};
			depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			depthAllocInfo.allocationSize = depthMemRequirements.size;
			depthAllocInfo.memoryTypeIndex = vkutils::FindMemoryTypeWithFallback( GetPhysicalDevice(), depthMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

			VK_CHECK_RESULT( vkAllocateMemory( GetLogicalDevice(), &depthAllocInfo, nullptr, &msaaDepthMemory ) );
			VK_CHECK_RESULT( vkBindImageMemory( GetLogicalDevice(), msaaDepthImage, msaaDepthMemory, 0 ) );

			m_vecMultiviewRenderTargets.back().vkMSAADepthTexture = msaaDepthImage;

			// Create MSAA depth view
			VkImageViewCreateInfo msaaDepthViewCI { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			msaaDepthViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			msaaDepthViewCI.image = msaaDepthImage;
			msaaDepthViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			msaaDepthViewCI.format = m_vkDepthFormat;
			msaaDepthViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			msaaDepthViewCI.subresourceRange.baseMipLevel = 0;
			msaaDepthViewCI.subresourceRange.levelCount = 1;
			msaaDepthViewCI.subresourceRange.baseArrayLayer = 0;
			msaaDepthViewCI.subresourceRange.layerCount = k_EyeCount;

			VK_CHECK_RESULT( vkCreateImageView( GetLogicalDevice(), &msaaDepthViewCI, nullptr, &m_vecMultiviewRenderTargets.back().vkMSAADepthView ) );

			// Depth image view (image from openxr runtime, will be used as the msaa resolve image)
			m_vecMultiviewRenderTargets.back().vkDepthTexture = m_vecSwapchainDepthImages[ i ].image;

			imageViewCI.pNext = NULL;
			imageViewCI.flags = depthCreateFlags;
			imageViewCI.format = m_vkDepthFormat;
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageViewCI.image = m_vecMultiviewRenderTargets.back().vkDepthTexture;

			result = vkCreateImageView( 
				GetLogicalDevice(), 
				&imageViewCI, 
				pCallbacks, 
				&m_vecMultiviewRenderTargets.back().vkDepthImageView );
			assert( result == VK_SUCCESS );

			// Command buffers
			VkCommandBufferAllocateInfo commandBufferAlloc { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			commandBufferAlloc.pNext = nullptr;
			commandBufferAlloc.commandPool = m_vkRenderCommandPool;
			commandBufferAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAlloc.commandBufferCount = 1;
			result = vkAllocateCommandBuffers( GetLogicalDevice(), &commandBufferAlloc, &m_vecMultiviewRenderTargets.back().vkRenderCommandBuffer );
			assert( result == VK_SUCCESS );

			commandBufferAlloc.commandPool = m_vkTransferCommandPool;
			result = vkAllocateCommandBuffers( GetLogicalDevice(), &commandBufferAlloc, &m_vecMultiviewRenderTargets.back().vkTransferCommandBuffer );
			assert( result == VK_SUCCESS );

			// Fences
			VkFenceCreateInfo fenceCI { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			result = vkCreateFence( GetLogicalDevice(), &fenceCI, nullptr, &m_vecMultiviewRenderTargets.back().vkRenderCommandFence );
			assert( result == VK_SUCCESS );

			result = vkCreateFence( GetLogicalDevice(), &fenceCI, nullptr, &m_vecMultiviewRenderTargets.back().vkTransferCommandFence );
			assert( result == VK_SUCCESS );
		}

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateRenderTargetSamplers( VkSamplerCreateInfo &samplerCI, VkAllocationCallbacks *pCallbacks ) 
	{ 
		if ( m_vecMultiviewRenderTargets.size() != m_vecSwapchainColorImages.size() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( samplerCI.sType != VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO )
			return XR_ERROR_VALIDATION_FAILURE;

		for ( auto &renderTarget : m_vecMultiviewRenderTargets )
		{
			VkResult result = vkCreateSampler( GetLogicalDevice(), &samplerCI, pCallbacks, &renderTarget.vkColorImageDescriptor.sampler );
			assert( result == VK_SUCCESS );
		}

		return XR_SUCCESS; 
	}

	VkResult CStereoRender::CreateDescriptorPool( 
		VkDescriptorPool &outPool, 
		std::vector< VkDescriptorPoolSize > &poolSizes, 
		uint32_t maxSets, 
		VkDescriptorPoolCreateFlags flags,
		void *pNext ) 
	{ 
		VkDescriptorPoolCreateInfo poolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			pNext,
			flags,
			maxSets,
			static_cast<uint32_t>( poolSizes.size() ), 
			poolSizes.data() };

		return vkCreateDescriptorPool( GetLogicalDevice(), &poolInfo, nullptr, &outPool );
	}

	XrResult CStereoRender::CreateRenderPass_Multiview( bool bUseVisMask )
	{
		if ( m_vecMultiviewRenderTargets.size() != m_vecSwapchainColorImages.size() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		m_bUseVisMask = bUseVisMask && m_pSession->GetVulkan()->IsStencilFormat( m_vkDepthFormat );

		// Define Attachment Descriptions (Color, Depth-Stencil)
		std::vector< VkAttachmentDescription2 > vecAttachmentDescriptions;

		// Color attachment (MSAA - shader output)
		VkAttachmentDescription2 colorAttachment = GenerateColorAttachmentDescription();
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.samples = VK_SAMPLE_COUNT_2_BIT;
		vecAttachmentDescriptions.push_back( colorAttachment );

		// Resolve color attachment (runtime image - resolve target)
		VkAttachmentDescription2 resolveColorAttachment = GenerateColorAttachmentDescription();
		resolveColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		vecAttachmentDescriptions.push_back( resolveColorAttachment );

		// Depth/stencil attachment (MSAA - shader output)
		VkAttachmentDescription2 depthStencilAttachment = GenerateDepthAttachmentDescription();
		depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthStencilAttachment.samples = VK_SAMPLE_COUNT_2_BIT;
		vecAttachmentDescriptions.push_back( depthStencilAttachment );

		// Resolve depth attachment (runtime image - resolve target)
		VkAttachmentDescription2 resolveDepthStencilAttachment = GenerateDepthAttachmentDescription();
		resolveDepthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		vecAttachmentDescriptions.push_back( resolveDepthStencilAttachment );

		// Define Subpass Descriptions
		std::vector< VkSubpassDescription2 > vecSubpassDescriptions;

		if ( m_bUseVisMask ) // If visibility mask is enabled, create a stencil subpass
		{
			// Left eye stencil setup (no color output, writes to stencil buffer)
			VkAttachmentReference2 stencilReference = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2 };
			stencilReference.attachment = 2; // Index of depth-stencil attachment
			stencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription2 stencilSubpass = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2 };
			stencilSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			stencilSubpass.colorAttachmentCount = 0;					// No color output
			stencilSubpass.pDepthStencilAttachment = &stencilReference; // Use depth-stencil attachment for stencil
			stencilSubpass.viewMask = k_unStereoViewMask;
			vecSubpassDescriptions.push_back( stencilSubpass );

			// Right eye stencil setup
			vecSubpassDescriptions.push_back( stencilSubpass );
		}

		// Main subpass (depth & color render)
		VkAttachmentReference2 colorReference = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2 };
		colorReference.attachment = 0; // Index of color attachment (msaa)
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference2 resolveColorReference = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2 };
		resolveColorReference.attachment = 1; // Index of color resolve attachment
		resolveColorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference2 depthStencilReference = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2 };
		depthStencilReference.attachment = 2; // Index of depth attachment (msaa)
		depthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference2 resolveDepthStencilReference = { VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2 };
		resolveDepthStencilReference.attachment = 3; // Index of depth resolve attachment
		resolveDepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescriptionDepthStencilResolve depthStencilResolve = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE };
		depthStencilResolve.pNext = nullptr;												
		depthStencilResolve.depthResolveMode = VK_RESOLVE_MODE_MIN_BIT;				
		depthStencilResolve.stencilResolveMode = VK_RESOLVE_MODE_NONE;			
		depthStencilResolve.pDepthStencilResolveAttachment = &resolveDepthStencilReference; // Reference to the resolve attachment

		VkSubpassDescription2 mainSubpass = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2 };
		mainSubpass.pNext = &depthStencilResolve;
		mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		mainSubpass.colorAttachmentCount = 1;
		mainSubpass.pColorAttachments = &colorReference;			  // Reference to color attachment
		mainSubpass.pResolveAttachments = &resolveColorReference;	  // Reference to color resolve attachment
		mainSubpass.pDepthStencilAttachment = &depthStencilReference; // Reference to depth stencil attachment
		mainSubpass.viewMask = k_unStereoViewMask;

		vecSubpassDescriptions.push_back( mainSubpass );

		// Define Subpass Dependencies
		std::vector< VkSubpassDependency2 > vecSubpassDependencies;

		if ( m_bUseVisMask ) 
		{
			// External dependency before the render pass (from VK_SUBPASS_EXTERNAL to subpass 0)
			vecSubpassDependencies.push_back( { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 } );
			vecSubpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().dstSubpass = 0; // First subpass (stencil setup)
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Stencil write
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Internal dependency (between left eye stencil subpass and right eye stencil subpass)
			vecSubpassDependencies.push_back( { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 } );
			vecSubpassDependencies.back().srcSubpass = 0;																			  // First subpass (stencil setup)
			vecSubpassDependencies.back().dstSubpass = 1;																			  // Second subpass (main rendering)
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;									  // Write to depth/stencil
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								  // Color output stage
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;								  // Stencil write
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Color attachment read/write
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Internal dependency (between right eye stencil subpass and main rendering subpass)
			vecSubpassDependencies.push_back( { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 } );
			vecSubpassDependencies.back().srcSubpass = 1;																			  // Second stencil subpass (right eye)
			vecSubpassDependencies.back().dstSubpass = 2;																			  // Main rendering subpass
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;									  // Write to depth/stencil
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								  // Color output stage
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;								  // Stencil write
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Color attachment read/write
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// External dependency after the render pass (from main rendering subpass to VK_SUBPASS_EXTERNAL)
			vecSubpassDependencies.push_back( { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 } );
			vecSubpassDependencies.back().srcSubpass = 2; // Last subpass (main rendering)
			vecSubpassDependencies.back().dstSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								  // Final color output stage
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;										  // Synchronize with external operations
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Color write
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;												  // Read after the render pass
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}
		else // Dependencies for a single subpass
		{
			// External dependency before the render pass
			vecSubpassDependencies.push_back( { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 } );
			vecSubpassDependencies.back().srcSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().dstSubpass = 0;	// Single subpass
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Wait for memory to be ready
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;	 // Color output and depth/stencil tests
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Read memory before render pass
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Read/write for color and depth/stencil
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// External dependency after the render pass
			vecSubpassDependencies.push_back( { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2 } );
			vecSubpassDependencies.back().srcSubpass = 0; // Single subpass
			vecSubpassDependencies.back().dstSubpass = VK_SUBPASS_EXTERNAL;
			vecSubpassDependencies.back().srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;	 // Color output and depth/stencil tests
			vecSubpassDependencies.back().dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Synchronize with external operations
			vecSubpassDependencies.back().srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Read/write for color and depth/stencil
			vecSubpassDependencies.back().dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Read after the render pass
			vecSubpassDependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		// Create the Render Pass
		VkRenderPassCreateInfo2 renderPassCI = GenerateRenderPassCI( vecAttachmentDescriptions, vecSubpassDescriptions, vecSubpassDependencies );

		// (A) Add Multiview to Render Pass (legacy, for vulkan 1.1, values are now in subpassdescription and renderpass ci)
		//VkRenderPassMultiviewCreateInfo multiviewCI = GenerateMultiviewCI();
		//renderPassCI.pNext = &multiviewCI;

		// (B) Adjustments to multiview if vismask is enabled (legacy, for vulkan 1.1, values are now in subpassdescription and renderpass ci)
		//if ( m_bUseVisMask )
		//{
		//	std::array< uint32_t, 3 > subpassViewMasks = {
		//		this->k_unStereoViewMask,
		//		this->k_unStereoViewMask,
		//		this->k_unStereoViewMask,
		//	};
		//	multiviewCI.pViewMasks = subpassViewMasks.data();
		//	multiviewCI.subpassCount = 3;
		//}

		// Create the Render Pass
		VkResult result = AddRenderPass( &renderPassCI );
		if ( !VK_CHECK_SUCCESS( result ) )
		{
			LogError( LOG_CATEGORY_DEFAULT, "Error creating render pass!" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		return XR_SUCCESS;
	}

	XrResult CStereoRender::CreateFramebuffers_Multiview( VkRenderPass vkRenderPass ) 
	{ 
		assert( vkRenderPass != VK_NULL_HANDLE );

		if ( m_vecMultiviewRenderTargets.empty() )
			return XR_ERROR_CALL_ORDER_INVALID;

		if ( GetLogicalDevice() == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		for ( auto &renderTarget : m_vecMultiviewRenderTargets )
		{
			std::array< VkImageView, 4 > arrImageViews { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
			renderTarget.SetImageViewArray( arrImageViews );

			VkFramebufferCreateInfo frameBufferCI = GenerateMultiviewFrameBufferCI( arrImageViews, vkRenderPass );
			VkResult result = vkCreateFramebuffer( GetLogicalDevice(), &frameBufferCI, nullptr, &renderTarget.vkFrameBuffer);
			assert( result == VK_SUCCESS );
		}

		return XR_SUCCESS; 
	}

	VkAttachmentDescription2 CStereoRender::GenerateColorAttachmentDescription() 
	{ 
		VkAttachmentDescription2 outDesc { VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2 };
		outDesc.format = m_vkColorFormat;
		outDesc.samples = VK_SAMPLE_COUNT_2_BIT;
		outDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		outDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		outDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		outDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		outDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		outDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		return outDesc;
	}

	VkAttachmentDescription2 CStereoRender::GenerateDepthAttachmentDescription()
	{
		VkAttachmentDescription2 outDesc { VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2 };
		outDesc.format = m_vkDepthFormat;
		outDesc.samples = VK_SAMPLE_COUNT_2_BIT;
		outDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

		if ( m_bUseVisMask )
		{
			// Stencil operations are required when using a visibility mask.
			outDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			outDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			outDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		else
		{
			// No stencil operations needed in this case
			outDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			outDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			outDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 
		}

		outDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		outDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		return outDesc;
	}

	VkSubpassDescription CStereoRender::GenerateSubpassDescription() 
	{ 
		VkSubpassDescription outDesc {};
		outDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		outDesc.pDepthStencilAttachment = GetDepthAttachmentReference();
		outDesc.colorAttachmentCount = 1;
		outDesc.pColorAttachments = GetColorAttachmentReference();

		return outDesc;
	}

	VkRenderPassMultiviewCreateInfo CStereoRender::GenerateMultiviewCI() 
	{ 
		VkRenderPassMultiviewCreateInfo multiviewCI { VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
		multiviewCI.pNext = nullptr;
		multiviewCI.subpassCount = 1;
		
		multiviewCI.pViewMasks = &this->k_unStereoViewMask;
		multiviewCI.correlationMaskCount = 1;
		multiviewCI.pCorrelationMasks = &this->k_unStereoConcurrentMask;

		multiviewCI.dependencyCount = 0;
		multiviewCI.pViewOffsets = nullptr;

		return multiviewCI;
	}

	VkRenderPassCreateInfo2
		CStereoRender::GenerateRenderPassCI( 
			std::vector< VkAttachmentDescription2 > &vecAttachmentDescriptions, 
			std::vector< VkSubpassDescription2 > &vecSubpassDescriptions, 
			std::vector< VkSubpassDependency2 > &vecSubpassDependencies,
			const VkRenderPassCreateFlags vkCreateFlags,
			const void *pNext ) 
	{ 
		VkRenderPassCreateInfo2 renderPassCI { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2 };
		renderPassCI.pNext = pNext;
		renderPassCI.flags = vkCreateFlags;
		renderPassCI.attachmentCount = vecAttachmentDescriptions.empty() ? 0 : (uint32_t) vecAttachmentDescriptions.size();
		renderPassCI.pAttachments = vecAttachmentDescriptions.empty() ? nullptr : vecAttachmentDescriptions.data();
		renderPassCI.subpassCount = vecSubpassDescriptions.empty() ? 0 : (uint32_t) vecSubpassDescriptions.size();
		renderPassCI.pSubpasses = vecSubpassDescriptions.empty() ? nullptr : vecSubpassDescriptions.data();
		renderPassCI.dependencyCount = vecSubpassDependencies.empty() ? 0 : (uint32_t) vecSubpassDependencies.size();
		renderPassCI.pDependencies = vecSubpassDependencies.empty() ? nullptr : vecSubpassDependencies.data();
		renderPassCI.correlatedViewMaskCount = 1;
		renderPassCI.pCorrelatedViewMasks = &this->k_unStereoConcurrentMask;

		return renderPassCI;
	}

	VkFramebufferCreateInfo	CStereoRender::GenerateMultiviewFrameBufferCI( 
			std::array< VkImageView, 4 > &arrImageViews,
			VkRenderPass vkRenderPass, 
			VkFramebufferCreateFlags vkCreateFlags, 	
			const void *pNext ) 
	{ 
		assert( vkRenderPass != VK_NULL_HANDLE );

		VkFramebufferCreateInfo frameBufferCI { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		frameBufferCI.pNext = pNext;
		frameBufferCI.flags = vkCreateFlags;
		frameBufferCI.width = m_unTextureWidth;
		frameBufferCI.height = m_unTextureHeight;
		frameBufferCI.renderPass = vkRenderPass;
		frameBufferCI.attachmentCount = (uint32_t) arrImageViews.size();
		frameBufferCI.pAttachments = arrImageViews.data();
		frameBufferCI.layers = 1; // must be one when using multiview

		return frameBufferCI;
	}

	VkSamplerCreateInfo CStereoRender::GenerateImageSamplerCI( VkSamplerCreateFlags flags, void *pNext ) 
	{ 
		VkSamplerCreateInfo samplerCI { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCI.pNext = pNext;
		samplerCI.flags = flags;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = samplerCI.addressModeU;
		samplerCI.addressModeW = samplerCI.addressModeU;
		samplerCI.mipLodBias = 0.0f;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.0f;
		samplerCI.anisotropyEnable = VK_TRUE;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		return samplerCI;
	}

	VkPipelineColorBlendAttachmentState CStereoRender::GenerateColorBlendAttachment( bool bEnableAlphaBlending ) 
	{ 
		VkPipelineColorBlendAttachmentState colorBlendAttachment {};
		colorBlendAttachment.blendEnable = bEnableAlphaBlending ? VK_TRUE : VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = bEnableAlphaBlending ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = bEnableAlphaBlending ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		return colorBlendAttachment;
	}

	VkPipelineLayoutCreateInfo CStereoRender::GeneratePipelineLayoutCI(
		std::vector< VkPushConstantRange > &vecPushConstantRanges,
		std::vector< VkDescriptorSetLayout > &vecDescriptorSetLayouts,
		VkPipelineLayoutCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineLayoutCreateInfo pipelineCI { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineCI.pNext = pNext;
		pipelineCI.flags = createFlags;
		pipelineCI.pushConstantRangeCount = vecPushConstantRanges.empty() ? 0 : (uint32_t) vecPushConstantRanges.size();
		pipelineCI.pPushConstantRanges = vecPushConstantRanges.empty() ? nullptr : vecPushConstantRanges.data();
		pipelineCI.setLayoutCount = vecDescriptorSetLayouts.empty() ? 0 : (uint32_t) vecDescriptorSetLayouts.size();
		pipelineCI.pSetLayouts = vecDescriptorSetLayouts.empty() ? nullptr : vecDescriptorSetLayouts.data();

		return pipelineCI;
	}

	VkPipelineVertexInputStateCreateInfo CStereoRender::GeneratePipelineStateCI_VertexInput(
		std::vector< VkVertexInputBindingDescription > &vecVertexBindingDescriptions,
		std::vector< VkVertexInputAttributeDescription > &vecVertexAttributeDescriptions,
		VkPipelineVertexInputStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineVertexInputStateCreateInfo outVertexInputCI { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		outVertexInputCI.pNext = pNext;
		outVertexInputCI.flags = createFlags;
		outVertexInputCI.vertexBindingDescriptionCount = vecVertexBindingDescriptions.empty() ? 0 : (uint32_t) vecVertexBindingDescriptions.size();
		outVertexInputCI.pVertexBindingDescriptions = vecVertexBindingDescriptions.empty() ? nullptr : vecVertexBindingDescriptions.data();
		outVertexInputCI.vertexAttributeDescriptionCount = vecVertexAttributeDescriptions.empty() ? 0 : (uint32_t) vecVertexAttributeDescriptions.size();
		outVertexInputCI.pVertexAttributeDescriptions = vecVertexAttributeDescriptions.empty() ? nullptr : vecVertexAttributeDescriptions.data();

		return outVertexInputCI;
	}

	VkPipelineInputAssemblyStateCreateInfo CStereoRender::GeneratePipelineStateCI_Assembly( 
		VkPrimitiveTopology topology, 
		VkBool32 primitiveRestartEnable, 
		VkPipelineInputAssemblyStateCreateFlags createFlags, 
		void *pNext )
	{
		VkPipelineInputAssemblyStateCreateInfo outInputAssemblyCI { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		outInputAssemblyCI.pNext = pNext;
		outInputAssemblyCI.flags = createFlags;
		outInputAssemblyCI.topology = topology;
		outInputAssemblyCI.primitiveRestartEnable = primitiveRestartEnable;
		
		return outInputAssemblyCI;
	}

	VkPipelineTessellationStateCreateInfo CStereoRender::GeneratePipelineStateCI_TesselationCI( 
		uint32_t patchControlPoints, 
		VkPipelineTessellationStateCreateFlags createFlags, 
		void *pNext )
	{
		VkPipelineTessellationStateCreateInfo tesselationCI { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		tesselationCI.pNext = pNext;
		tesselationCI.flags = createFlags;
		tesselationCI.patchControlPoints = patchControlPoints;
		
		return tesselationCI;
	}

	VkPipelineViewportStateCreateInfo CStereoRender::GeneratePipelineStateCI_ViewportCI( 
		std::vector< VkViewport > &vecViewports, 
		std::vector< VkRect2D > &vecScissors, 
		VkPipelineViewportStateCreateFlags createFlags, void *pNext )
	{
		VkPipelineViewportStateCreateInfo viewportCI { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportCI.pNext = pNext;
		viewportCI.flags = createFlags;
		viewportCI.viewportCount = vecViewports.empty() ? 0 : (uint32_t) vecViewports.size();
		viewportCI.pViewports = vecViewports.empty() ? nullptr : vecViewports.data();
		viewportCI.scissorCount = vecScissors.empty() ? 0 : (uint32_t) vecScissors.size();
		viewportCI.pScissors = vecScissors.empty() ? nullptr : vecScissors.data();

		return viewportCI;
	}

	VkPipelineRasterizationStateCreateInfo CStereoRender::GeneratePipelineStateCI_RasterizationCI(
		VkPolygonMode polygonMode,
		VkCullModeFlags cullMode,
		VkFrontFace frontFace,
		float lineWidth,
		VkBool32 depthClampEnable,
		float depthBiasClamp,
		VkBool32 depthBiasEnable,
		float depthBiasConstantFactor,
		float depthBiasSlopeFactor,
		VkBool32 rasterizerDiscardEnable,
		VkPipelineRasterizationStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineRasterizationStateCreateInfo rasterizationCI { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationCI.pNext = pNext;
		rasterizationCI.flags = createFlags;
		rasterizationCI.polygonMode = polygonMode;
		rasterizationCI.cullMode = cullMode;
		rasterizationCI.frontFace = frontFace;
		rasterizationCI.lineWidth = lineWidth;
		rasterizationCI.depthClampEnable = depthClampEnable;
		rasterizationCI.depthBiasEnable = depthBiasEnable;
		rasterizationCI.depthBiasClamp = depthBiasClamp;
		rasterizationCI.depthBiasConstantFactor = depthBiasConstantFactor;
		rasterizationCI.depthBiasSlopeFactor = depthBiasSlopeFactor;
		rasterizationCI.rasterizerDiscardEnable = rasterizerDiscardEnable;

		return rasterizationCI;
	}

	VkPipelineMultisampleStateCreateInfo CStereoRender::GeneratePipelineStateCI_MultisampleCI(
		VkSampleCountFlagBits rasterizationSamples,
		VkBool32 sampleShadingEnable,
		float minSampleShading,
		VkSampleMask *pSampleMask,
		VkBool32 alphaToCoverageEnable,
		VkBool32 alphaToOneEnable,
		VkPipelineMultisampleStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineMultisampleStateCreateInfo multisampleCI { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampleCI.pNext = pNext;
		multisampleCI.flags = createFlags;
		multisampleCI.rasterizationSamples = rasterizationSamples;
		multisampleCI.sampleShadingEnable = sampleShadingEnable;
		multisampleCI.minSampleShading = minSampleShading;
		multisampleCI.pSampleMask = pSampleMask;
		multisampleCI.alphaToCoverageEnable = alphaToCoverageEnable;
		multisampleCI.alphaToOneEnable = alphaToOneEnable;

		return multisampleCI;
	}

	VkPipelineDepthStencilStateCreateInfo CStereoRender::GeneratePipelineStateCI_DepthStencilCI(
		VkBool32 depthTestEnable,
		VkBool32 depthWriteEnable,
		VkCompareOp depthCompareOp,
		VkBool32 depthBoundsTestEnable,
		VkBool32 stencilTestEnable,
		VkStencilOpState front,
		VkStencilOpState back,
		float minDepthBounds,
		float maxDepthBounds,
		VkPipelineDepthStencilStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineDepthStencilStateCreateInfo depthStencilCI { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencilCI.pNext = pNext;
		depthStencilCI.flags = createFlags;
		depthStencilCI.depthTestEnable = depthTestEnable;
		depthStencilCI.depthWriteEnable = depthWriteEnable;
		depthStencilCI.depthCompareOp = depthCompareOp;
		depthStencilCI.depthBoundsTestEnable = depthBoundsTestEnable;
		depthStencilCI.stencilTestEnable = stencilTestEnable;
		depthStencilCI.front = front;
		depthStencilCI.back = back;
		depthStencilCI.minDepthBounds = minDepthBounds;
		depthStencilCI.maxDepthBounds = maxDepthBounds;
		
		return depthStencilCI;
	}

	VkPipelineColorBlendStateCreateInfo CStereoRender::GeneratePipelineStateCI_ColorBlendCI(
		std::vector< VkPipelineColorBlendAttachmentState > &vecColorBlendAttachmentStates,
		VkBool32 logicOpEnable,
		VkLogicOp logicOp,
		VkPipelineColorBlendStateCreateFlags createFlags,
		void *pNext )
	{
		VkPipelineColorBlendStateCreateInfo colorBlendCI { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendCI.pNext = pNext;
		colorBlendCI.flags = createFlags;
		colorBlendCI.attachmentCount = vecColorBlendAttachmentStates.empty() ? 0 : (uint32_t)vecColorBlendAttachmentStates.size();
		colorBlendCI.pAttachments = vecColorBlendAttachmentStates.empty() ? nullptr : vecColorBlendAttachmentStates.data();
		colorBlendCI.logicOpEnable = logicOpEnable;
		colorBlendCI.logicOp = logicOp;
		
		return colorBlendCI;
	}

	VkPipelineDynamicStateCreateInfo CStereoRender::GeneratePipelineStateCI_DynamicStateCI( 
		std::vector< VkDynamicState > &vecDynamicStates, 
		VkPipelineDynamicStateCreateFlags createFlags, 
		void *pNext )
	{
		VkPipelineDynamicStateCreateInfo dynamicStateCI { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStateCI.pNext = pNext;
		dynamicStateCI.flags = createFlags;
		dynamicStateCI.dynamicStateCount = vecDynamicStates.empty() ? 0 : (uint32_t) vecDynamicStates.size();
		dynamicStateCI.pDynamicStates = vecDynamicStates.empty() ? nullptr : vecDynamicStates.data();

		return dynamicStateCI;
	}

	VkResult CStereoRender::CreateGraphicsPipeline(
		VkPipelineLayout &outPipelineLayout,
		VkPipeline &outPipeline,
		VkRenderPass renderPass,
		std::vector< VkPipelineShaderStageCreateInfo > &vecShaderStages,
		const VkPipelineVertexInputStateCreateInfo *pVertexInputCI,
		const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyCI,
		const VkPipelineTessellationStateCreateInfo *pTessellationCI,
		const VkPipelineViewportStateCreateInfo *pViewportStateCI,
		const VkPipelineRasterizationStateCreateInfo *pRasterizationCI,
		const VkPipelineMultisampleStateCreateInfo *pMultisampleCI,
		const VkPipelineDepthStencilStateCreateInfo *pDepthStencilCI,
		const VkPipelineColorBlendStateCreateInfo *pColorBlendCI,
		const VkPipelineDynamicStateCreateInfo *pDynamicStateCI,
		const VkPipelineCache pipelineCache,
		const uint32_t unSubpassIndex,
		const VkPipelineCreateFlags createFlags,
		const void *pNext,
		const VkAllocationCallbacks *pCallbacks )
	{
		assert( GetLogicalDevice() != VK_NULL_HANDLE );

		VkGraphicsPipelineCreateInfo pipelineCI { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineCI.pNext = pNext;
		pipelineCI.flags = createFlags;

		pipelineCI.layout = outPipelineLayout;
		pipelineCI.renderPass = renderPass;
		pipelineCI.subpass = unSubpassIndex;

		pipelineCI.stageCount = (uint32_t) vecShaderStages.size();
		pipelineCI.pStages = vecShaderStages.data();
		pipelineCI.pVertexInputState = pVertexInputCI;
		pipelineCI.pInputAssemblyState = pInputAssemblyCI;
		pipelineCI.pTessellationState = pTessellationCI;
		pipelineCI.pViewportState = pViewportStateCI;
		pipelineCI.pRasterizationState = pRasterizationCI;
		pipelineCI.pMultisampleState = pMultisampleCI;
		pipelineCI.pDepthStencilState = pDepthStencilCI;
		pipelineCI.pColorBlendState = pColorBlendCI;
		pipelineCI.pDynamicState = pDynamicStateCI;

		return vkCreateGraphicsPipelines( GetLogicalDevice(), pipelineCache, 1, &pipelineCI, pCallbacks, &outPipeline );
	}

	VkResult CStereoRender::CreateGraphicsPipeline_Stencil(
		#ifdef XR_USE_PLATFORM_ANDROID
				AAssetManager *assetManager,
		#endif
		uint32_t unSubpassIndex,
		VkPipelineLayout &outPipelineLayout,
		VkPipeline &outPipeline,
		VkRenderPass vkRenderPass,
		std::string sVertexShaderFilename,
		std::string sFragmentShaderFilename )
	{
		assert( GetLogicalDevice() != VK_NULL_HANDLE );
		assert( vkRenderPass != VK_NULL_HANDLE );
		assert( !sVertexShaderFilename.empty() );
		assert( !sFragmentShaderFilename.empty() );

		// Create descriptor set layout for joint matrices
		VkDescriptorSetLayoutBinding jointMatricesBinding = {
			0,								   // binding
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType
			1,								   // descriptorCount
			VK_SHADER_STAGE_VERTEX_BIT,		   // stageFlags
			nullptr							   // pImmutableSamplers
		};

		VkDescriptorSetLayoutCreateInfo dsLayoutCI = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &jointMatricesBinding };

		VkDescriptorSetLayout jointMatricesSetLayout;
		vkCreateDescriptorSetLayout( GetLogicalDevice(), &dsLayoutCI, nullptr, &jointMatricesSetLayout );

		// Now include this in your pipeline layout creation
		std::vector< VkDescriptorSetLayout > vecLayouts = { jointMatricesSetLayout };

		// Create pipeline layout with push constant definitions for the shader
		std::vector< VkPushConstantRange > vecPCRs = { { VK_SHADER_STAGE_VERTEX_BIT, 0, k_pcrSize } };

		VkPipelineLayoutCreateInfo pipelineLayoutCI = GeneratePipelineLayoutCI( vecPCRs, vecLayouts );
		VkResult result = vkCreatePipelineLayout( GetLogicalDevice(), &pipelineLayoutCI, nullptr, &outPipelineLayout );
		if ( !VK_CHECK_SUCCESS( result ) )
			return result;

		// Setup shader set
		SShaderSet *pShaders = new SShaderSet( sVertexShaderFilename, sFragmentShaderFilename );

		#ifdef XR_USE_PLATFORM_ANDROID
				pShaders->Init( assetManager, GetLogicalDevice() );
		#else
				pShaders->Init( GetLogicalDevice() );
		#endif

		// Define shader vertex inputs
		pShaders->vertexBindings.push_back( { 0, sizeof( XrVector2f ), VK_VERTEX_INPUT_RATE_VERTEX } );
		pShaders->vertexAttributes.push_back( { 0, 0, VK_FORMAT_R32G32_SFLOAT } );

		auto vertexInputCI = GeneratePipelineStateCI_VertexInput( pShaders->vertexBindings, pShaders->vertexAttributes );

		// Define viewport
		std::vector< VkViewport > vecViewports { { 0.0f, 0.0f, (float) GetTextureWidth(), (float) GetTextureHeight(), 0.0f, 1.0f } }; // Will invert y after projection
		std::vector< VkRect2D > vecScissors { { { 0, 0 }, GetTextureExtent() } };

		auto viewportCI = GeneratePipelineStateCI_ViewportCI( vecViewports, vecScissors );

		// Define other pipeline states
		auto assemblyCI = GeneratePipelineStateCI_Assembly();
		auto multisampleCI = GeneratePipelineStateCI_MultisampleCI();
		auto depthStencilCI = GeneratePipelineStateCI_DepthStencilCI();

		auto rasterizationCI = GeneratePipelineStateCI_RasterizationCI();
		rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationCI.cullMode = VK_CULL_MODE_NONE;

		// Configure Depth
		if ( m_pSession->GetVulkan()->IsDepthFormat( m_vkDepthFormat ) )
		{
			// Enable depth testing and writing
			depthStencilCI.depthTestEnable = VK_FALSE;			
			depthStencilCI.depthWriteEnable = VK_FALSE;			
			depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

			// Enable stencil test
			depthStencilCI.stencilTestEnable = VK_TRUE;

			// Stencil op state
			depthStencilCI.front.failOp = VK_STENCIL_OP_KEEP;
			depthStencilCI.front.passOp = VK_STENCIL_OP_REPLACE;  
			depthStencilCI.front.depthFailOp = VK_STENCIL_OP_KEEP;
			depthStencilCI.front.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencilCI.front.compareMask = 0xFF;			   // Test all bits
			depthStencilCI.front.writeMask = 0xFF;				   // Write all bits to stencil buffer
			depthStencilCI.front.reference = 1;					   

			depthStencilCI.back = depthStencilCI.front;			   // Use the same configuration for back face

			// Depth bounds - no testing to save a bit of overhead
			depthStencilCI.depthBoundsTestEnable = VK_FALSE;
		}

		// Create graphics pipeline
		result = CreateGraphicsPipeline(
			outPipelineLayout, 
			outPipeline, 
			vkRenderPass, 
			pShaders->stages, 
			&vertexInputCI, 
			&assemblyCI, 
			nullptr, 
			&viewportCI, 
			&rasterizationCI, 
			&multisampleCI, 
			&depthStencilCI, 
			nullptr,
			nullptr,
			VK_NULL_HANDLE,
			unSubpassIndex); 

		if ( !VK_CHECK_SUCCESS( result ) )
			return result;

		// Cleanup
		delete pShaders;
		return VK_SUCCESS;
	}

	VkResult CStereoRender::CreateGraphicsPipeline_Stencils(
		#ifdef XR_USE_PLATFORM_ANDROID
				AAssetManager *assetManager,
		#endif
		VkPipelineLayout &outPipelineLayout,
		std::vector< VkPipeline > &outPipelines,
		VkRenderPass vkRenderPass,
		std::vector< std::string > vecVertexShaders,
		std::vector< std::string > vecFragmentShaders )
	{
		assert( vecVertexShaders.size() == vecFragmentShaders.size() );
		VkResult result = VK_SUCCESS;

		outPipelines.clear();
		for ( uint32_t i = 0; i < vecVertexShaders.size(); i++ )
		{
			outPipelines.push_back( VK_NULL_HANDLE );
			result = CreateGraphicsPipeline_Stencil(
			#ifdef XR_USE_PLATFORM_ANDROID
				assetManager,
			#endif
				i,
				outPipelineLayout,
				outPipelines[ i ],
				vkRenderPass,
				vecVertexShaders[ i ],
				vecFragmentShaders[ i ] );

			if ( !VK_CHECK_SUCCESS( result ) )
				break;
		}

		return result;
	}

	VkResult CStereoRender::CreateGraphicsPipeline_Primitives( 
		#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
		#endif
		VkPipelineLayout &outPipelineLayout,
		VkPipeline &outPipeline,
		VkRenderPass vkRenderPass,
		std::string sVertexShaderFilename,
		std::string sFragmentShaderFilename ) 
	{ 
		assert( GetLogicalDevice() && vkRenderPass && !sVertexShaderFilename.empty() && !sFragmentShaderFilename.empty() );

		// Create pipeline layout if needed
		if ( outPipelineLayout == VK_NULL_HANDLE )
		{
			std::vector< VkPushConstantRange > pcRanges = { GetEyeMatricesPushConstant() };
			std::vector< VkDescriptorSetLayout > vecDescriptorSetLayouts;
			VkPipelineLayoutCreateInfo layoutCI = GeneratePipelineLayoutCI( pcRanges, vecDescriptorSetLayouts );
			VK_CHECK_RESULT( vkCreatePipelineLayout( GetLogicalDevice(), &layoutCI, nullptr, &outPipelineLayout ) );
		}

		// Setup shader set with primitive vertex attributes
		SShaderSet *pShaders = new SShaderSet( sVertexShaderFilename, sFragmentShaderFilename );
		SetupPrimitiveVertexAttributes( *pShaders );

		#ifdef XR_USE_PLATFORM_ANDROID
			pShaders->Init( assetManager, GetLogicalDevice() );
		#else
			pShaders->Init( GetLogicalDevice() );
		#endif

		// Create pipeline state
		SPipelineStateInfo state = CreateDefaultPipelineState( 
			pShaders->vertexBindings, 
			pShaders->vertexAttributes, 
			GetTextureWidth(), 
			GetTextureHeight() );
		state.rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;

		ConfigureDepthStencil( state.depthStencil, m_bUseVisMask, m_vkDepthFormat );

		// Create pipeline
		VkResult result = CreateGraphicsPipeline(
			outPipelineLayout,
			outPipeline,
			vkRenderPass,
			pShaders->stages,
			&state.vertexInput,
			&state.assembly,
			nullptr,
			&state.viewport,
			&state.rasterization,
			&state.multisample,
			&state.depthStencil,
			&state.colorBlend,
			&state.dynamicState,
			VK_NULL_HANDLE,
			m_bUseVisMask ? 2 : 0 );

		delete pShaders;
		return result;
	}

	VkResult CStereoRender::CreateGraphicsPipeline_PBR( 		
	#ifdef XR_USE_PLATFORM_ANDROID
		AAssetManager *assetManager,
	#endif 
		SPipelines &outPipelines, 
		uint32_t &outPipelineIndex,
		CRenderInfo *pRenderInfo,
		uint32_t unPbrDescriptorPoolCount,
		VkRenderPass vkRenderPass, 
		std::string sVertexShaderFilename, 
		std::string sFragmentShaderFilename,
		bool bCreateAsMainPBRPipeline )
	{
		assert( pRenderInfo && unPbrDescriptorPoolCount > 0 );

		SShaderSet *pShaderSet = new SShaderSet( sVertexShaderFilename, sFragmentShaderFilename );
		SetupPBRVertexAttributes( *pShaderSet );

		#ifdef XR_USE_PLATFORM_ANDROID
			pShaderSet->Init( assetManager, GetLogicalDevice() );
		#else
			pShaderSet->Init( GetLogicalDevice() );
		#endif

		if ( bCreateAsMainPBRPipeline )
		{
			VK_CHECK_RESULT( SetupPBRDescriptors( outPipelines, pRenderInfo, unPbrDescriptorPoolCount ) );
			outPipelines.pbrLayout = pRenderInfo->AddNewLayout();
		}

		outPipelineIndex = pRenderInfo->AddNewPipeline();
		if ( bCreateAsMainPBRPipeline )
		{
			outPipelines.pbr = outPipelineIndex;
		}

		std::vector< VkDescriptorSetLayout > layouts = { pRenderInfo->pDescriptors->GetDescriptorSetLayout( outPipelines.pbrFragmentDescriptorLayout ), pRenderInfo->pDescriptors->GetDescriptorSetLayout( pRenderInfo->lightingLayoutId ) };

		SPipelineCreationParams params { 
			.renderPass = vkRenderPass, 
			.useVisMask = m_bUseVisMask, 
			.depthFormat = m_vkDepthFormat, 
			.subpassIndex = static_cast< uint32_t >( m_bUseVisMask ? 2 : 0 ) 
		};

		return CreateBasePipeline( 
			pRenderInfo->vecPipelineLayouts[ outPipelines.pbrLayout ], 
			pRenderInfo->vecGraphicsPipelines[ outPipelineIndex ], 
			pShaderSet, 
			params, 
			layouts );
	}

	VkResult CStereoRender::CreateGraphicsPipeline_CustomPBR(
		#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
		#endif 
		SPipelines &outPipelines,
		uint32_t &outPipelineIndex,
		CRenderInfo *pRenderInfo,
		VkRenderPass vkRenderPass,
		const std::string &sVertexShaderFilename,
		const std::string &sFragmentShaderFilename,
		SPipelineStateInfo &pipelineState,
		bool bCreateAsMainPBRPipeline,
		uint32_t unDescriptorPoolCount )
	{
		assert( pRenderInfo && !sVertexShaderFilename.empty() && !sFragmentShaderFilename.empty() );

		SShaderSet *pShaderSet = new SShaderSet( sVertexShaderFilename, sFragmentShaderFilename );
		SetupPBRVertexAttributes( *pShaderSet );

		#ifdef XR_USE_PLATFORM_ANDROID
			pShaderSet->Init( assetManager, GetLogicalDevice() );
		#else
			pShaderSet->Init( GetLogicalDevice() );
		#endif

		pipelineState.vertexInput = GeneratePipelineStateCI_VertexInput( pShaderSet->vertexBindings, pShaderSet->vertexAttributes ); 

		std::vector< VkDescriptorSetLayout > layouts;
		if ( bCreateAsMainPBRPipeline )
		{
			VK_CHECK_RESULT( SetupPBRDescriptors( outPipelines, pRenderInfo, unDescriptorPoolCount ) );
			layouts.push_back( pRenderInfo->pDescriptors->GetDescriptorSetLayout( outPipelines.pbrFragmentDescriptorLayout ) );

			outPipelines.pbrLayout = pRenderInfo->AddNewLayout();
		}

		outPipelineIndex = pRenderInfo->AddNewPipeline();
		if ( bCreateAsMainPBRPipeline )
		{
			outPipelines.pbr = outPipelineIndex;
		}

		return CreateGraphicsPipeline(
			pRenderInfo->vecPipelineLayouts[ outPipelines.pbrLayout ],
			pRenderInfo->vecGraphicsPipelines[ outPipelineIndex ],
			vkRenderPass,
			pShaderSet->stages,
			&pipelineState.vertexInput,
			&pipelineState.assembly,
			nullptr,
			&pipelineState.viewport,
			&pipelineState.rasterization,
			&pipelineState.multisample,
			&pipelineState.depthStencil,
			&pipelineState.colorBlend,
			&pipelineState.dynamicState,
			VK_NULL_HANDLE,
			m_bUseVisMask ? 2 : 0 );
	}

	VkResult CStereoRender::CreateGraphicsPipeline(
		VkPipelineLayout &outLayout, 
		VkPipeline &outPipeline, 
		VkRenderPass vkRenderPass, 
		std::vector< VkDescriptorSetLayout > &vecLayouts,
		SShaderSet *pShaderSet_WillBeDestroyed ) 
	{ 
		assert( GetLogicalDevice() != VK_NULL_HANDLE && vkRenderPass != VK_NULL_HANDLE && pShaderSet_WillBeDestroyed );

		// Create pipeline layout if needed
		if ( outLayout == VK_NULL_HANDLE && !vecLayouts.empty() )
		{
			std::vector< VkPushConstantRange > vecPCRs = { GetEyeMatricesPushConstant() };
			VkPipelineLayoutCreateInfo layoutCI = GeneratePipelineLayoutCI( vecPCRs, vecLayouts );
			VK_CHECK_RESULT( vkCreatePipelineLayout( GetLogicalDevice(), &layoutCI, nullptr, &outLayout ) );
		}

		// Create default pipeline state
		SPipelineStateInfo state = CreateDefaultPipelineState( 
			pShaderSet_WillBeDestroyed->vertexBindings, 
			pShaderSet_WillBeDestroyed->vertexAttributes, 
			GetTextureWidth(), 
			GetTextureHeight() );

		// Configure depth-stencil state
		ConfigureDepthStencil( state.depthStencil, m_bUseVisMask, m_vkDepthFormat );

		// Create pipeline
		VkResult result = CreateGraphicsPipeline(
			outLayout,
			outPipeline,
			vkRenderPass,
			pShaderSet_WillBeDestroyed->stages,
			&state.vertexInput,
			&state.assembly,
			nullptr,
			&state.viewport,
			&state.rasterization,
			&state.multisample,
			&state.depthStencil,
			&state.colorBlend,
			&state.dynamicState,
			VK_NULL_HANDLE,
			static_cast< uint32_t > ( m_bUseVisMask ? 2 : 0 ) );

		delete pShaderSet_WillBeDestroyed;
		return result;
	}

	SPipelineStateInfo CStereoRender::CreateDefaultPipelineState( 
		std::vector< VkVertexInputBindingDescription > &bindings, 
		std::vector< VkVertexInputAttributeDescription > &attributes, 
		uint32_t textureWidth, 
		uint32_t textureHeight )
	{
		SPipelineStateInfo info = CreateDefaultPipelineState( textureWidth, textureHeight );
		info.vertexInput = GeneratePipelineStateCI_VertexInput( bindings, attributes ); 
		return info;
	}

	SPipelineStateInfo CStereoRender::CreateDefaultPipelineState( uint32_t textureWidth, uint32_t textureHeight ) 
	{ 
		SPipelineStateInfo info;

		info.viewports = { { 0.0f, 0.0f, (float) textureWidth, (float) textureHeight, 0.0f, 1.0f } };
		info.scissors = { { { 0, 0 }, { textureWidth, textureHeight } } };

		info.viewport = GeneratePipelineStateCI_ViewportCI( info.viewports, info.scissors );
		info.colorBlendAttachments = { GenerateColorBlendAttachment() };
		info.colorBlend = GeneratePipelineStateCI_ColorBlendCI( info.colorBlendAttachments );
		info.dynamicState = GeneratePipelineStateCI_DynamicStateCI( info.dynamicStates );
		info.assembly = GeneratePipelineStateCI_Assembly();
		info.rasterization = GeneratePipelineStateCI_RasterizationCI();
		info.rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		info.multisample = GeneratePipelineStateCI_MultisampleCI();
		info.depthStencil = GeneratePipelineStateCI_DepthStencilCI();

		return info;
	}

	void CStereoRender::ConfigureDepthStencil( 
		VkPipelineDepthStencilStateCreateInfo &outDepthStencilCI, 
		bool bUseVisMask, 
		VkFormat depthFormat ) 
	{
		if ( m_pSession->GetVulkan()->IsDepthFormat( depthFormat ) )
		{
			outDepthStencilCI.depthTestEnable = VK_TRUE;
			outDepthStencilCI.depthWriteEnable = VK_TRUE;
			outDepthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS;
			outDepthStencilCI.stencilTestEnable = bUseVisMask ? VK_TRUE : VK_FALSE;
			outDepthStencilCI.depthBoundsTestEnable = VK_FALSE;

			if ( bUseVisMask )
			{
				VkStencilOpState stencilState { 
					.failOp = VK_STENCIL_OP_KEEP, 
					.passOp = VK_STENCIL_OP_KEEP, 
					.depthFailOp = VK_STENCIL_OP_KEEP, 
					.compareOp = VK_COMPARE_OP_NOT_EQUAL, 
					.compareMask = 0xFF,	// Use all stencil bits
					.writeMask = 0x00,		// Disable writes to stencil bits
					.reference = 1 };

				outDepthStencilCI.front = stencilState;
				outDepthStencilCI.back = stencilState;
			}
		}
	}

	VkResult CStereoRender::SetupPBRDescriptors( 
		SPipelines &outPipelines, 
		CRenderInfo *pRenderInfo, 
		uint32_t poolCount ) 
	{ 

		std::vector< SDescriptorBinding > pbrBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT } };

		VK_CHECK_RESULT( pRenderInfo->pDescriptors->CreateDescriptorSetLayout( outPipelines.pbrFragmentDescriptorLayout, pbrBindings ) );

		VK_CHECK_RESULT( pRenderInfo->pDescriptors->CreateDescriptorPool( outPipelines.pbrFragmentDescriptorPool, outPipelines.pbrFragmentDescriptorLayout, poolCount ) );

		// Setup lighting descriptors
		VkDescriptorPoolSize lightingPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 };

		VkDescriptorPoolCreateInfo lightingPoolInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &lightingPoolSize };

		VK_CHECK_RESULT( pRenderInfo->pDescriptors->CreateDescriptorPool( pRenderInfo->lightingPoolId, lightingPoolInfo ) );

		std::vector< SDescriptorBinding > lightingBindings = { { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT } };

		VK_CHECK_RESULT( pRenderInfo->pDescriptors->CreateDescriptorSetLayout( pRenderInfo->lightingLayoutId, lightingBindings ) );

		pRenderInfo->SetupSceneLighting();

		return VK_SUCCESS;
	}

	void CStereoRender::SetupPrimitiveVertexAttributes( SShaderSet &shaderSet ) 
	{
		// Vertex data binding
		shaderSet.vertexBindings = { { 0, sizeof( SColoredVertex ), VK_VERTEX_INPUT_RATE_VERTEX }, { 1, sizeof( XrMatrix4x4f ), VK_VERTEX_INPUT_RATE_INSTANCE } };

		// Vertex attributes
		uint32_t sizeFloat = sizeof( float );
		shaderSet.vertexAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( SColoredVertex, position ) },
			{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof( SColoredVertex, color ) },
			{ 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
			{ 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeFloat },
			{ 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 8 * sizeFloat },
			{ 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 12 * sizeFloat } };
	}

	void CStereoRender::SetupPBRVertexAttributes( SShaderSet &shaderSet ) 
	{
		shaderSet.vertexBindings = { { 0, sizeof( SMeshVertex ), VK_VERTEX_INPUT_RATE_VERTEX }, { 1, sizeof( XrMatrix4x4f ), VK_VERTEX_INPUT_RATE_INSTANCE } };

		shaderSet.vertexAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( SMeshVertex, position ) },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( SMeshVertex, normal ) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof( SMeshVertex, tangent ) },
			{ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( SMeshVertex, uv0 ) },
			{ 4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( SMeshVertex, uv1 ) },
			{ 5, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( SMeshVertex, color0 ) },
			{ 6, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof( SMeshVertex, joints ) },
			{ 7, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof( SMeshVertex, weights ) },
			{ 8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
			{ 9, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 16 },
			{ 10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 32 },
			{ 11, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 48 } };
	}

	VkResult CStereoRender::CreateBasePipeline( 
		VkPipelineLayout &outLayout, 
		VkPipeline &outPipeline, 
		SShaderSet *pShaderSet, 
		const SPipelineCreationParams &params, 
		std::vector< VkDescriptorSetLayout > &layouts ) 
	{ 
		
		if ( outLayout == VK_NULL_HANDLE && !layouts.empty() )
		{
			std::vector< VkPushConstantRange > pcRanges = { GetEyeMatricesPushConstant() };
			VkPipelineLayoutCreateInfo layoutCI = GeneratePipelineLayoutCI( pcRanges, layouts );
			VK_CHECK_RESULT( vkCreatePipelineLayout( GetLogicalDevice(), &layoutCI, nullptr, &outLayout ) );
		}

		SPipelineStateInfo state = CreateDefaultPipelineState( 
			pShaderSet->vertexBindings, 
			pShaderSet->vertexAttributes, 
			GetTextureWidth(), 
			GetTextureHeight() );

		ConfigureDepthStencil( state.depthStencil, params.useVisMask, params.depthFormat );

		return CreateGraphicsPipeline(
			outLayout,
			outPipeline,
			params.renderPass,
			pShaderSet->stages,
			&state.vertexInput,
			&state.assembly,
			nullptr,
			&state.viewport,
			&state.rasterization,
			&state.multisample,
			&state.depthStencil,
			&state.colorBlend,
			&state.dynamicState,
			VK_NULL_HANDLE,
			params.subpassIndex );
	}

	VkResult CStereoRender::AddRenderPass( VkRenderPassCreateInfo2 *renderPassCI, VkAllocationCallbacks *pAllocator ) 
	{ 
		assert( renderPassCI );

		vecRenderPasses.push_back( VK_NULL_HANDLE );

		VkResult result = vkCreateRenderPass2( GetLogicalDevice(), renderPassCI, nullptr, &vecRenderPasses.back() ); 
		if ( result != VK_SUCCESS )
		{
			vecRenderPasses.pop_back();
			return result;
		}

		return VK_SUCCESS;
	}

	void CStereoRender::RenderFrame( 
		const VkRenderPass renderPass, 
		CRenderInfo *pRenderInfo, 
		std::vector< CPlane2D * > &stencils ) 
	{ 
		if( StartRenderFrame( pRenderInfo ) )
			EndRenderFrame( renderPass, pRenderInfo, stencils );
	}

	bool CStereoRender::StartRenderFrame( CRenderInfo* pRenderInfo ) 
	{
		if ( !XR_UNQUALIFIED_SUCCESS( m_pSession->StartFrame( &pRenderInfo->state.frameState ) ) )
			return false;

		return true;
	}

	void CStereoRender::EndRenderFrame( const VkRenderPass renderPass, CRenderInfo *pRenderInfo, std::vector< CPlane2D * > &stencils ) 
	{
		auto &state = pRenderInfo->state;

		// Render frame
		if ( state.frameState.shouldRender )
		{
#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
			if ( rdoc_api && rdoc_api->GetNumCaptures() < RENDERDOC_FRAME_SAMPLES )
			{
				rdoc_api->StartFrameCapture( RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE( m_pSession->GetVulkan()->GetVkInstance() ), NULL );
				LogDebug( "RENDERDOC", "Renderdoc capture started for frame: %i", rdoc_api->GetNumCaptures() + 1 );
			}
#endif

			// Update eye view pose, fov, etc
			m_pSession->UpdateEyeStates( m_vecEyeViews, state.eyeProjectionMatrices, &state.sharedEyeState, &state.frameState, m_pSession->GetAppSpace(), state.nearZ, state.farZ );

			// DRAW CALLS
			if ( state.sharedEyeState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT )
			{
				// Update Hmd pose
				m_pSession->UpdateHmdPose( state.frameState.predictedDisplayTime + state.frameState.predictedDisplayPeriod );
				m_pSession->GetHmdPose( state.hmdPose );

				// Acquire swapchain images
				m_pSession->AcquireFrameImage( &state.unCurrentSwapchainImage_Color, &state.unCurrentSwapchainImage_Depth, GetColorSwapchain(), GetDepthSwapchain() );

				// Wait for swapchain images. This ensures that the openxr runtime is finish with them before we submit the draw commands to the gpu
				m_pSession->WaitForFrameImage( GetColorSwapchain(), GetDepthSwapchain() );

				// Update frame layers (eye projection views)
				for ( uint32_t i = 0; i < k_EyeCount; i++ )
				{
					state.projectionLayerViews[ i ].pose = m_vecEyeViews[ i ].pose;
					state.projectionLayerViews[ i ].fov = m_vecEyeViews[ i ].fov;
					state.projectionLayerViews[ i ].subImage.swapchain = GetColorSwapchain();
					state.projectionLayerViews[ i ].subImage.imageArrayIndex = i;
					state.projectionLayerViews[ i ].subImage.imageRect.offset = { 0, 0 };
					state.projectionLayerViews[ i ].subImage.imageRect.extent = GetTexutreExtent2Di();

					XrCompositionLayerDepthInfoKHR depthInfo { XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR };
					depthInfo.subImage.swapchain = GetDepthSwapchain();
					depthInfo.subImage.imageArrayIndex = i;
					depthInfo.subImage.imageRect.offset = state.imageRectOffsets[ i ];
					depthInfo.subImage.imageRect.extent = GetTexutreExtent2Di();
					depthInfo.minDepth = state.minDepth;
					depthInfo.maxDepth = state.maxDepth;
					depthInfo.nearZ = state.nearZ;
					depthInfo.farZ = state.farZ;

					state.projectionLayerViews[ i ].next = &depthInfo;
				}

				// Calculate view projection matrices for each eye
				CalculateViewMatrices( state.eyeViewMatrices, &state.eyeScale );

				XrMatrix4x4f_Multiply( &state.eyeVPs[ k_Left ], &state.eyeProjectionMatrices[ k_Left ], &state.eyeViewMatrices[ k_Left ] );
				XrMatrix4x4f_Multiply( &state.eyeVPs[ k_Right ], &state.eyeProjectionMatrices[ k_Right ], &state.eyeViewMatrices[ k_Right ] );

				// Begin draw commands for rendering
				BeginDraw( state.unCurrentSwapchainImage_Color, state.clearValues, true, renderPass );

				// Draw vismask (if activated)
				if ( m_bUseVisMask && stencils.size() == 2 )
				{
					for ( size_t eyeIndex = 0; eyeIndex < stencils.size(); ++eyeIndex )
					{
						// Push constants for the vertex shader
						vkCmdPushConstants( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, pRenderInfo->stencilLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, k_pcrSize, state.eyeProjectionMatrices.data() );

						// Set stencil reference to write to the stencil mask
						vkCmdSetStencilReference( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1 );

						// Bind the vismask pipeline
						vkCmdBindPipeline( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pRenderInfo->stencilPipelines[ eyeIndex ] );

						// Bind vismask vertices and indices for the current eye
						VkDeviceSize offsets[] = { 0 };

						// Bind the index buffer for the current eye stencil
						vkCmdBindIndexBuffer( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, stencils[ eyeIndex ]->GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16 );

						// Bind the vertex buffer for the current eye stencil
						vkCmdBindVertexBuffers( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, 0, 1, stencils[ eyeIndex ]->GetVertexBuffer()->GetVkBufferPtr(), offsets );

						// Draw the stencil for the current eye
						vkCmdDrawIndexed(
							GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer,
							static_cast< uint32_t >( stencils[ eyeIndex ]->GetIndices()->size() ), // Number of indices to draw
							1,																	   // Instance count
							0,																	   // First index
							0,																	   // Vertex offset
							0																	   // First instance
						);

						if ( eyeIndex == 0 )
						{
							// Transition to the right eye subpass
							vkCmdNextSubpass( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_SUBPASS_CONTENTS_INLINE );
						}
					}

					// Transition to the next subpass for main rendering
					vkCmdNextSubpass( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, VK_SUBPASS_CONTENTS_INLINE );
				}

				// Copy model matrices to gpu buffer
				state.ClearStagingBuffers();
				{
					// Begin buffer recording to gpu
					BeginBufferUpdates( state.unCurrentSwapchainImage_Color );

					// Update asset buffers
					XrTime renderTime = state.frameState.predictedDisplayTime + state.frameState.predictedDisplayPeriod;

					for ( auto &renderable : pRenderInfo->vecRenderables )
					{
						if ( !renderable->isVisible )
							continue;

						// Update matrices (for each instance)
						for ( uint32_t i = 0; i < renderable->instances.size(); i++ )
							renderable->UpdateModelMatrix( i, m_pSession->GetAppSpace(), renderTime );

						// Add to render
						state.vecStagingBuffers.push_back( renderable->UpdateInstancesBuffer( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkTransferCommandBuffer ) );
					}

					// Submit to gpu
					SubmitBufferUpdates( state.unCurrentSwapchainImage_Color );
				}

				//  Main rendering subpass: Draw render assets
				for ( auto &renderable : pRenderInfo->vecRenderables )
				{
					if ( renderable->isVisible )
						renderable->Draw( GetMultiviewRenderTargets().at( state.unCurrentSwapchainImage_Color ).vkRenderCommandBuffer, *pRenderInfo );
				}
				
				// Submit draw calls to gpu - this will also clear the staging buffers (if any)
				SubmitDraw( state.unCurrentSwapchainImage_Color, state.vecStagingBuffers );

				// Release the swapchian image to let the openxr runtime know we're through with it
				m_pSession->ReleaseFrameImage( GetColorSwapchain(), GetDepthSwapchain() );
			}

			// Assemble frame layers

			// Pre application layers
			if ( !state.preAppFrameLayers.empty() )
			{
				for ( XrCompositionLayerBaseHeader *layer : state.preAppFrameLayers )
					state.frameLayers.push_back( layer );
			}

			// Application layers
			state.projectionLayer.next = nullptr;
			state.projectionLayer.layerFlags = state.compositionLayerFlags;

			state.projectionLayer.space = m_pSession->GetAppSpace();
			state.projectionLayer.viewCount = (uint32_t) state.projectionLayerViews.size();
			state.projectionLayer.views = state.projectionLayerViews.data();

			state.frameLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( &state.projectionLayer ) );

			// Post application layers
			if ( !state.postAppFrameLayers.empty() )
			{
				for ( XrCompositionLayerBaseHeader *layer : state.postAppFrameLayers )
					state.frameLayers.push_back( layer );
			}
		}

		// End frame
		m_pSession->EndFrame( &state.frameState, state.frameLayers, state.environmentBlendMode );
		state.frameLayers.clear();

		#if defined( _WIN32 ) && defined( RENDERDOC_ENABLE )
			if ( rdoc_api && rdoc_api->GetNumCaptures() < RENDERDOC_FRAME_SAMPLES )
			{
				if ( rdoc_api->EndFrameCapture( RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE( m_pSession->GetVulkan()->GetVkInstance() ), NULL ) )
					LogDebug( "RENDERDOC", "Renderdoc capture ended for frame: %i", rdoc_api->GetNumCaptures() );
				else
					LogError( "RENDERDOC", "FAILED FRAME CAPTURE for frame: %i", rdoc_api->GetNumCaptures() + 1 );
			}
		#endif
	}

	void CStereoRender::BeginDraw(
		const uint32_t unSwpachainImageIndex,
		std::vector< VkClearValue > &vecClearValues,
		const bool startCommandBufferRecording,
		const VkRenderPass renderpass,
		const VkSubpassContents subpass)
	{
		// @todo - debug assert swapchain image index vs size of render targets

		if ( startCommandBufferRecording )
		{
			// Set command buffer to recording
			VkCommandBufferBeginInfo cmdBeginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			vkBeginCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer, &cmdBeginInfo );
			// @todo assert on VkResult for debug only
		}

		if ( renderpass != VK_NULL_HANDLE )
		{
			// Set render pass begin info
			VkRenderPassBeginInfo renderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.clearValueCount = (uint32_t) vecClearValues.size();
			renderPassBeginInfo.pClearValues = vecClearValues.data();
			renderPassBeginInfo.renderPass = renderpass;
			renderPassBeginInfo.framebuffer = m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkFrameBuffer;
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = GetTextureExtent();

			// Start render pass
			vkCmdBeginRenderPass( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer, &renderPassBeginInfo, subpass );
		}
	}

	void CStereoRender::SubmitDraw(
		const uint32_t unSwpachainImageIndex,
		std::vector< CDeviceBuffer * > &vecStagingBuffers,
		const uint32_t timeoutNs,
		const VkCommandBufferResetFlags transferBufferResetFlags, const VkCommandBufferResetFlags renderBufferResetFlags ) 
	{
		// End render recording
		vkCmdEndRenderPass( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer );
		vkEndCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer );

		// Wait for any data transfer operations to finish
		// @todo start recording for next frame
		vkWaitForFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandFence, VK_TRUE, timeoutNs );
		vkResetFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandFence );
		vkResetCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer, transferBufferResetFlags );

		// Clear/free staging buffer memory
		for ( CDeviceBuffer *pStagingBuffer : vecStagingBuffers )
			delete pStagingBuffer;

		vecStagingBuffers.clear();

		// Execute render commands (requires exclusive access to vkQueue)
		// safest after wait swapchain image
		VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer;
		vkQueueSubmit( GetAppSession()->GetVulkan()->GetVkQueue_Graphics(), 1, &submitInfo, m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandFence );

		// Wait for rendering to finish
		// @todo move to separate thread - or at start unSwapchainIndex from prior frame
		vkWaitForFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandFence, VK_TRUE, timeoutNs );
		vkResetFences( GetLogicalDevice(), 1, &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandFence );
		vkResetCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkRenderCommandBuffer, renderBufferResetFlags );
	}

	void CStereoRender::BeginBufferUpdates( const uint32_t unSwpachainImageIndex ) 
	{ 
		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer, &beginInfo );
	}

	void CStereoRender::SubmitBufferUpdates( const uint32_t unSwpachainImageIndex ) 
	{
		vkEndCommandBuffer( m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer );

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandBuffer;

		vkQueueSubmit( GetAppSession()->GetVulkan()->GetVkQueue_Transfer(), 1, &submitInfo, m_vecMultiviewRenderTargets[ unSwpachainImageIndex ].vkTransferCommandFence );
	}

	void CStereoRender::CalculateViewMatrices( std::array< XrMatrix4x4f, 2 > &outViewMatrices, const XrVector3f *eyeScale ) 
	{
		std::vector< XrMatrix4x4f > eyeViews;
		eyeViews.resize( outViewMatrices.size() );
		for ( uint32_t i = 0; i < outViewMatrices.size(); i++ )
		{
			XrMatrix4x4f_CreateTranslationRotationScale( &eyeViews[ i ], &m_vecEyeViews[ i ].pose.position, &m_vecEyeViews[ i ].pose.orientation, eyeScale );
			XrMatrix4x4f_InvertRigidBody( &outViewMatrices[ i ], &eyeViews[ i ] );
		}
	}

	VkDescriptorPool CStereoRender::CreateDescriptorPool( 
		const std::vector< VkDescriptorPoolSize > &poolSizes, 
		uint32_t maxSets, 
		VkDescriptorPoolCreateFlags flags, 
		const void *pNext )
	{
		// Create descriptor pool create info
		VkDescriptorPoolCreateInfo poolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			pNext,
			flags,
			maxSets,
			static_cast< uint32_t >( poolSizes.size() ),
			poolSizes.data() };

		// Create descriptor pool
		VkDescriptorPool descriptorPool;
		if ( vkCreateDescriptorPool( GetLogicalDevice(), &poolInfo, nullptr, &descriptorPool ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create descriptor pool!" );
		}

		return descriptorPool;
	}

	VkDescriptorSet CStereoRender::UpdateDescriptorSets(
		VkDescriptorPool descriptorPool,
		std::vector< VkDescriptorSetLayout > &setLayouts,
		const std::vector< VkDescriptorBufferInfo > &bufferInfos,
		const std::vector< VkDescriptorImageInfo > &imageInfos,
		const std::vector< VkBufferView > &texelBufferViews,
		const std::vector< VkDescriptorType > &descriptorTypes,
		const std::vector< uint32_t > &bindings,
		const void *pNextAllocate,
		const void *pNextWrite )
	{
		// Allocate descriptor set
		VkDescriptorSetAllocateInfo allocInfo = { 
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 
			pNextAllocate, 
			descriptorPool,
			static_cast< uint32_t >( setLayouts.size() ), 
			setLayouts.data() };

		VkDescriptorSet descriptorSet;
		if ( vkAllocateDescriptorSets( GetLogicalDevice(), &allocInfo, &descriptorSet ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to allocate descriptor sets!" );
		}

		// Prepare write descriptor sets
		std::vector< VkWriteDescriptorSet > descriptorWrites;
		descriptorWrites.reserve( descriptorTypes.size() );

		for ( size_t i = 0; i < descriptorTypes.size(); ++i )
		{
			VkWriteDescriptorSet writeSet = {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				pNextWrite,
				descriptorSet,
				bindings[ i ],
				0,
				1,
				descriptorTypes[ i ],
				descriptorTypes[ i ] == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ? &imageInfos[ i ] : nullptr,
				descriptorTypes[ i ] == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? &bufferInfos[ i ] : nullptr,
				descriptorTypes[ i ] == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ? &texelBufferViews[ i ] : nullptr };
			descriptorWrites.push_back( writeSet );
		}

		// Update descriptor set
		vkUpdateDescriptorSets( GetLogicalDevice(), static_cast< uint32_t >( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );

		return descriptorSet;
	}

	VkPhysicalDevice CStereoRender::GetPhysicalDevice() 
	{ 
		return m_pSession->GetVulkan()->GetVkPhysicalDevice(); 
	}

	VkDevice CStereoRender::GetLogicalDevice() 
	{ 
		return m_pSession->GetVulkan()->GetVkLogicalDevice(); 
	}

} // namespace xrlib
