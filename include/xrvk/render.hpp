/* 
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <vector>
#include <array>
#include <filesystem>
#include <fstream>

#include <xrvk/primitive.hpp>

namespace xrlib
{
	#ifdef XR_USE_PLATFORM_ANDROID
		static std::vector< char > ReadBinaryFile( AAssetManager *assetManager, const std::string &sFilename )
		{
			AAsset *file = AAssetManager_open( assetManager, sFilename.c_str(), AASSET_MODE_BUFFER );
			if ( !file )
				LogError( "", "Unable to load binary file: %s", sFilename.c_str() );

			size_t fileLength = AAsset_getLength( file );
			char *fileContent = new char[ fileLength ];
			AAsset_read( file, fileContent, fileLength );
			AAsset_close( file );
			std::vector< char > vec( fileContent, fileContent + fileLength );

			return vec;
		}
	#else
		static std::vector< char > ReadBinaryFile( const std::string &sFilename )
		{
			std::ifstream file( sFilename, std::ios::ate | std::ios::binary );

			if ( !file.is_open() )
			{
				std::filesystem::path cwd = std::filesystem::current_path();
				LogError( "Unable to read file: %s (%s)", sFilename.c_str(), cwd.generic_string().c_str() );
				throw std::runtime_error( "failed to open file!" );
			}

			size_t fileSize = (size_t) file.tellg();
			std::vector< char > buffer( fileSize );

			file.seekg( 0 );
			file.read( buffer.data(), fileSize );

			file.close();
			assert( fileSize > 0 );
			return buffer;
		}
	#endif

	struct SGraphicsPipelineLayout
	{
		uint32_t size = 0;	
		VkPipelineLayout layout = VK_NULL_HANDLE;

		SGraphicsPipelineLayout( uint32_t unSize )
			: size( unSize ) {};

		~SGraphicsPipelineLayout() {};
	};

	struct SShader
	{
	   public: 
		SShader( std::string sFilename, std::string sEntrypoint = "main" ): 
			m_sFilename( sFilename ), 
			m_sEntrypoint( sEntrypoint )
		 {
			 assert( !sFilename.empty() );
			 assert( !sEntrypoint.empty() );
		 };

		~SShader();

		#ifdef XR_USE_PLATFORM_ANDROID
		VkPipelineShaderStageCreateInfo Init( 
			AAssetManager *assetManager,
			VkDevice vkLogicalDevice, 
			VkShaderStageFlagBits shaderStage,
			VkShaderModuleCreateFlags createFlags = 0, 
			void *pNext = nullptr );
		#else
		VkPipelineShaderStageCreateInfo Init( 
			VkDevice vkLogicalDevice, 
			VkShaderStageFlagBits shaderStage,
			VkShaderModuleCreateFlags createFlags = 0, 
			void *pNext = nullptr );
		#endif

		VkShaderModule GetShaderModule() { return m_vkShaderModule; }

	   private:
		std::string m_sFilename;
		std::string m_sEntrypoint;

		VkDevice m_vkLogicalDevice = VK_NULL_HANDLE;
		VkShaderModule m_vkShaderModule = VK_NULL_HANDLE;
	};

	struct SShaderSet
	{
	  public:
		SShaderSet( 
			std::string sVertexShaderFilename, 
			std::string sFragmentShaderFilename, 
			std::string sVertexShaderEntrypoint = "main", 
			std::string sFragmentShaderEntrypoint = "main" );

		~SShaderSet();

		SShader *vertexShader = nullptr;
		SShader *fragmentShader = nullptr;

		std::vector< VkPipelineShaderStageCreateInfo > stages;

		#ifdef XR_USE_PLATFORM_ANDROID
		void Init(
			AAssetManager *assetManager,
			VkDevice vkLogicalDevice,
			VkShaderStageFlagBits vertexShaderStage = VK_SHADER_STAGE_VERTEX_BIT,
			VkShaderStageFlagBits fragmentShaderStage = VK_SHADER_STAGE_FRAGMENT_BIT,
			VkShaderModuleCreateFlags vertexShaderCreateFlags = 0,
			VkShaderModuleCreateFlags fragmentShaderCreateFlags = 0,
			void *pVertexNext = nullptr,
			void *pFragmentNext = nullptr );
		#else
		void Init(
			VkDevice vkLogicalDevice,
			VkShaderStageFlagBits vertexShaderStage = VK_SHADER_STAGE_VERTEX_BIT,
			VkShaderStageFlagBits fragmentShaderStage = VK_SHADER_STAGE_FRAGMENT_BIT,
			VkShaderModuleCreateFlags vertexShaderCreateFlags = 0,
			VkShaderModuleCreateFlags fragmentShaderCreateFlags = 0,
			void *pVertexNext = nullptr,
			void *pFragmentNext = nullptr );
		#endif

		std::vector< VkVertexInputBindingDescription > vertexBindings;
		std::vector< VkVertexInputAttributeDescription > vertexAttributes;

	};

	struct SRenderFrameInfo
	{
	  public:
		float nearZ = 0.f;
		float farZ = 100.f;

		float minDepth = 0.f;
		float maxDepth = 1.f;

		uint32_t unCurrentSwapchainImage_Color = 0;
		uint32_t unCurrentSwapchainImage_Depth = 0;

		XrFrameState frameState { XR_TYPE_FRAME_STATE };
		XrViewState sharedEyeState { XR_TYPE_VIEW_STATE };
		XrCompositionLayerProjection projectionLayer { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
			
		XrVector3f eyeScale = { 1.0f, 1.0f, 1.0f };
		std::array< XrMatrix4x4f, 2 > arrEyeVPs = {};

		std::vector< XrMatrix4x4f > eyeProjectionMatrices = { XrMatrix4x4f(), XrMatrix4x4f() };
		std::vector< XrMatrix4x4f > eyeViewMatrices = { XrMatrix4x4f(), XrMatrix4x4f() };
		
		std::vector< XrOffset2Di > imageRectOffsets = { { 0, 0 }, { 0, 0 } }; 
		std::vector< VkClearValue > clearValues = { { 0.0f, 0.0f, 0.0f, 1.0f },{ 1.0f, 0 } };

		std::vector< XrCompositionLayerProjectionView > projectionLayerViews = {
			XrCompositionLayerProjectionView { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW },
			XrCompositionLayerProjectionView { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };

		std::vector< XrCompositionLayerBaseHeader * > frameLayers;

		std::vector< CDeviceBuffer * > vecStagingBuffers;

		void ClearStagingBuffers()
		{
			 for ( CDeviceBuffer *pStagingBuffer : vecStagingBuffers )
			 {
				 if ( pStagingBuffer == nullptr ) delete pStagingBuffer;
			 }

			 vecStagingBuffers.clear();
		}


		SRenderFrameInfo( float near, float far, float min = 0.f, float max = 1.f ) :
			nearZ ( near ),
			farZ ( far ), 
			minDepth( min ), 
			maxDepth( max)
		{
		};

		SRenderFrameInfo() 
		{ 
		};

		~SRenderFrameInfo() {};

	};

	class CStereoRender
	{
	  public:
		CStereoRender( CSession *pSession, VkFormat vkColorFormat, VkFormat vkDepthFormat );
		~CStereoRender();

		const uint32_t k_EyeCount = 2;
		const uint32_t k_unStereoViewMask = 0b00000011;
		const uint32_t k_unStereoConcurrentMask = 0b00000011;

		VkClearColorValue vkClearColor = { { 0.05f, 0.05f, 0.05f, 1.0f } };

		struct SMultiviewRenderTarget
		{
			VkImage vkColorTexture = VK_NULL_HANDLE;
			VkDescriptorImageInfo vkColorImageDescriptor { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

			VkImage vkDepthTexture = VK_NULL_HANDLE;
			VkImageView vkDepthImageView = VK_NULL_HANDLE;

			VkFramebuffer vkFrameBuffer = VK_NULL_HANDLE;

			VkCommandBuffer vkRenderCommandBuffer = VK_NULL_HANDLE;
			VkFence vkRenderCommandFence = VK_NULL_HANDLE;

			VkCommandBuffer vkTransferCommandBuffer = VK_NULL_HANDLE;
			VkFence vkTransferCommandFence = VK_NULL_HANDLE;

			void SetImageViewArray( std::array < VkImageView, 2 > &arrImageViews )
			{
				arrImageViews[ 0 ] = vkColorImageDescriptor.imageView;
				arrImageViews[ 1 ] = vkDepthImageView;
			}
		};

		std::vector< VkRenderPass > vecRenderPasses;

		XrResult Init( uint32_t unTextureFaceCount = 1, uint32_t unTextureMipCount = 1 );
		XrResult CreateSwapchains( uint32_t unFaceCount = 1, uint32_t unMipCount = 1 );
		XrResult CreateSwapchainImages( std::vector < XrSwapchainImageVulkan2KHR > &outSwapchainImages, XrSwapchain xrSwapchain );

		XrResult CreateMainRenderPass( VkRenderPass &outRenderPass );
		XrResult PrepareDefaultMultiviewRendering();
		XrResult AddDefaultMultiviewRenderPass();
		XrResult CreateDefaultMultiviewFramebuffers( VkRenderPass vkRenderPass );

		XrResult CreateMultiviewRenderTargets( 
			VkImageViewCreateFlags colorCreateFlags = 0, 
			VkImageViewCreateFlags depthCreateFlags = 0, 
			void *pColorNext = nullptr,
			void *pDepthNext = nullptr,
			VkAllocationCallbacks *pCallbacks = nullptr );
		XrResult CreateRenderTargetSamplers( VkSamplerCreateInfo &samplerCI, VkAllocationCallbacks *pCallbacks = nullptr );


		VkAttachmentDescription GenerateColorAttachmentDescription();
		VkAttachmentDescription GenerateDepthAttachmentDescription();
		VkSubpassDescription GenerateSubpassDescription();

		VkRenderPassMultiviewCreateInfo GenerateMultiviewCI();

		VkRenderPassCreateInfo GenerateRenderPassCI( 
			std::vector< VkAttachmentDescription > &vecAttachmentDescriptions,
			std::vector< VkSubpassDescription > &vecSubpassDescriptions, 
			std::vector < VkSubpassDependency > &vecSubpassDependencies,
			const VkRenderPassCreateFlags vkCreateFlags = 0,
			const void *pNext = nullptr );

		VkFramebufferCreateInfo GenerateMultiviewFrameBufferCI( 
			std::array< VkImageView, 2 > &arrImageViews,
			VkRenderPass vkRenderPass, 
			VkFramebufferCreateFlags vkCreateFlags = 0,
			const void *pNext = nullptr);

		VkSamplerCreateInfo GenerateImageSamplerCI( VkSamplerCreateFlags flags = 0, void *pNext = nullptr );

		VkPipelineColorBlendAttachmentState GenerateColorBlendAttachment();

		VkPipelineLayoutCreateInfo GeneratePipelineLayoutCI(
			std::vector < VkPushConstantRange > &vecPushConstantRanges,
			std::vector< VkDescriptorSetLayout > &vecDescriptorSetLayouts,
			VkPipelineLayoutCreateFlags createFlags = 0,
			void *pNext = nullptr );

		VkPipelineVertexInputStateCreateInfo GeneratePipelineStateCI_VertexInput( 
			std::vector < VkVertexInputBindingDescription > &vecVertexBindingDescriptions, 
			std::vector< VkVertexInputAttributeDescription > &vecVertexAttributeDescriptions,
			VkPipelineVertexInputStateCreateFlags createFlags = 0, 
			void *pNext = nullptr );

		VkPipelineInputAssemblyStateCreateInfo GeneratePipelineStateCI_Assembly( 
			VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VkBool32 primitiveRestartEnable = VK_FALSE,
			VkPipelineInputAssemblyStateCreateFlags createFlags = 0, 
			void *pNext = nullptr
		);

		VkPipelineTessellationStateCreateInfo GeneratePipelineStateCI_TesselationCI
		( 
			uint32_t patchControlPoints = 0,
			VkPipelineTessellationStateCreateFlags createFlags = 0,
			void *pNext = nullptr 															
		);

		VkPipelineViewportStateCreateInfo GeneratePipelineStateCI_ViewportCI
		( 
			std::vector < VkViewport > &vecViewports,
			std::vector < VkRect2D > &vecScissors,
			VkPipelineViewportStateCreateFlags createFlags = 0,
			void *pNext = nullptr
		);

		VkPipelineRasterizationStateCreateInfo GeneratePipelineStateCI_RasterizationCI
		( 
			VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
			VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
			VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE,
			float lineWidth = 1.0f,
			VkBool32 depthClampEnable = VK_FALSE,
			float depthBiasClamp = 0.f,
			VkBool32 depthBiasEnable = VK_FALSE,
			float depthBiasConstantFactor = 0.f,
			float depthBiasSlopeFactor = 0.f,
			VkBool32 rasterizerDiscardEnable = VK_FALSE,
			VkPipelineRasterizationStateCreateFlags createFlags = 0,
			void *pNext = nullptr
		);

		VkPipelineMultisampleStateCreateInfo GeneratePipelineStateCI_MultisampleCI
		( 
			VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			VkBool32 sampleShadingEnable = VK_FALSE, 
			float minSampleShading = 0.f,
			VkSampleMask *pSampleMask = nullptr,
			VkBool32 alphaToCoverageEnable = VK_FALSE,
			VkBool32 alphaToOneEnable = VK_FALSE,
			VkPipelineMultisampleStateCreateFlags createFlags = 0,
			void *pNext = nullptr
		);

		VkPipelineDepthStencilStateCreateInfo GeneratePipelineStateCI_DepthStencilCI
		( 
			VkBool32 depthTestEnable = VK_FALSE, //VK_TRUE,
			VkBool32 depthWriteEnable = VK_FALSE, //VK_TRUE, 
			VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			VkBool32 depthBoundsTestEnable = VK_FALSE,
			VkBool32 stencilTestEnable = VK_FALSE,
			VkStencilOpState front = {},
			VkStencilOpState back = {},
			float minDepthBounds = 0.f,
			float maxDepthBounds = 0.f,
			VkPipelineDepthStencilStateCreateFlags createFlags = 0,
			void *pNext = nullptr
		);

		VkPipelineColorBlendStateCreateInfo GeneratePipelineStateCI_ColorBlendCI
		( 
			std::vector< VkPipelineColorBlendAttachmentState > &vecColorBlenAttachmentStates, 
			VkBool32 logicOpEnable = VK_FALSE,
			VkLogicOp logicOp = VK_LOGIC_OP_CLEAR,
			VkPipelineColorBlendStateCreateFlags createFlags = 0,
			void *pNext = nullptr 
		);

		VkPipelineDynamicStateCreateInfo GeneratePipelineStateCI_DynamicStateCI
		( 
			std::vector< VkDynamicState > &vecDynamicStates,
			VkPipelineDynamicStateCreateFlags createFlags = 0,
			void *pNext = nullptr 
		);

		VkResult CreateGraphicsPipeline(
			VkPipeline &outPipeline,
			VkPipelineLayout pipelineLayout,
			VkRenderPass renderPass,
			std::vector < VkPipelineShaderStageCreateInfo > &vecShaderStages, 
			const VkPipelineVertexInputStateCreateInfo *pVertexInputCI,
			const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyCI,
			const VkPipelineTessellationStateCreateInfo *pTessellationCI,
			const VkPipelineViewportStateCreateInfo *pViewportStateCI,
			const VkPipelineRasterizationStateCreateInfo *pRasterizationCI,
			const VkPipelineMultisampleStateCreateInfo *pMultisampleCI,
			const VkPipelineDepthStencilStateCreateInfo *pDepthStencilCI,
			const VkPipelineColorBlendStateCreateInfo *pColorBlendCI,
			const VkPipelineDynamicStateCreateInfo *pDynamicStateCI,
			const VkPipelineCache pipelineCache = VK_NULL_HANDLE,
			const uint32_t unSubpassIndex = 0,
			const VkPipelineCreateFlags createFlags = 0,
			const void *pNext = nullptr,
			const VkAllocationCallbacks *pCallbacks = nullptr );

		VkResult CreateGraphicsPipeline_Primitives( 
			#ifdef XR_USE_PLATFORM_ANDROID
				AAssetManager *assetManager,
			#endif
			VkPipeline &outPipeline, 
			SGraphicsPipelineLayout &pipelineLayout, 
			VkRenderPass vkRenderPass, 
			std::string sVertexShaderFilename, 
			std::string sFragmentShaderFilename 
		);

		VkResult AddRenderPass( VkRenderPassCreateInfo *renderPassCI, VkAllocationCallbacks *pAllocator = nullptr );
		void RenderFrame( VkRenderPass renderPass, SGraphicsPipelineLayout pipelineLayout, SRenderFrameInfo &renderInfo, std::vector< CColoredPrimitive * > &primitives );

		void BeginDraw(
			const uint32_t unSwpachainImageIndex,
			std::vector< VkClearValue > &vecClearValues,
			const bool startCommandBufferRecording = true,
			const VkRenderPass renderpass = VK_NULL_HANDLE,
			const VkSubpassContents subpass = VK_SUBPASS_CONTENTS_INLINE );

		void SubmitDraw( 
			const uint32_t unSwpachainImageIndex, 
			std::vector< CDeviceBuffer* >&vecStagingBuffers,
			const uint32_t timeoutNs = 1000000000, 
			const VkCommandBufferResetFlags transferBufferResetFlags = 0,
			const VkCommandBufferResetFlags renderBufferResetFlags = 0 );

		void BeginBufferUpdates( const uint32_t unSwpachainImageIndex );
		void SubmitBufferUpdates( const uint32_t unSwpachainImageIndex );
		void CalculateViewMatrices( std::vector< XrMatrix4x4f > &outViewMatrices, const XrVector3f *eyeScale );

		const uint32_t GetTextureWidth() { return m_unTextureWidth; }
		const uint32_t GetTextureHeight() { return m_unTextureHeight; }

		const VkFormat GetColorFormat() { return m_vkColorFormat; }
		const VkFormat GetDepthFormat() { return m_vkDepthFormat; }
		const VkCommandPool GetCommandPool() { return m_vkRenderCommandPool; }

		VkAttachmentReference *GetColorAttachmentReference() { return &m_vkColorAttachmentReference; }
		VkAttachmentReference *GetDepthAttachmentReference() { return &m_vkDepthAttachmentReference; }

		const XrSwapchain GetColorSwapchain() { return m_xrColorSwapchain; }
		const XrSwapchain GetDepthSwapchain() { return m_xrDepthSwapchain; }

		std::vector< XrSwapchainImageVulkan2KHR > &GetSwapchainColorImages() { return m_vecSwapchainColorImages; }
		std::vector< XrSwapchainImageVulkan2KHR > &GetSwapchainDepthImages() { return m_vecSwapchainDepthImages; }
		std::vector< XrViewConfigurationView > &GetEyeConfigs() { return m_vecEyeConfigs; }
		std::vector< XrView > &GetEyeViews() { return m_vecEyeViews; }
		std::vector< SMultiviewRenderTarget > &GetMultiviewRenderTargets() { return m_vecMultiviewRenderTargets;  }

		XrSwapchainImageVulkan2KHR *GetSwapchainColorImage( uint32_t unIndex ) { return &m_vecSwapchainColorImages[ unIndex ]; }
		XrSwapchainImageVulkan2KHR *GetSwapchainDepthImage( uint32_t unIndex ) { return &m_vecSwapchainColorImages[ unIndex ]; }

		VkImage *GetColorTexture( uint32_t unIndex ) { return &m_vecSwapchainColorImages[ unIndex ].image; }
		VkImage *GetDepthTexture( uint32_t unIndex ) { return &m_vecSwapchainDepthImages[ unIndex ].image; }

		XrViewConfigurationView *GetEyeConfig( uint32_t unEye ) { return &m_vecEyeConfigs[ unEye ]; }
		XrView *GetEyeView( uint32_t unEye ) { return &m_vecEyeViews[ unEye ]; }

		CSession *GetAppSession() { return m_pSession; }
		CInstance *GetAppInstance() { return m_pSession->GetAppInstance(); }
		
		VkPhysicalDevice GetPhysicalDevice();
		VkDevice GetLogicalDevice();

		VkExtent2D GetTextureExtent() { return { m_unTextureWidth, m_unTextureHeight }; }
		XrExtent2Di GetTexutreExtent2Di() { return { (int32_t) m_unTextureWidth, (int32_t) m_unTextureHeight }; }

		const ELogLevel GetMinLogLevel() { return GetAppInstance()->GetMinLogLevel(); }

	  private:
		CSession *m_pSession = nullptr;

		uint32_t m_unTextureWidth = 0;
		uint32_t m_unTextureHeight = 0;

		VkFormat m_vkColorFormat = VK_FORMAT_UNDEFINED;
		VkFormat m_vkDepthFormat = VK_FORMAT_UNDEFINED;

		VkCommandPool m_vkRenderCommandPool = XR_NULL_HANDLE;
		VkCommandPool m_vkTransferCommandPool = XR_NULL_HANDLE;
		
		VkAttachmentReference m_vkColorAttachmentReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference m_vkDepthAttachmentReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		XrSwapchain m_xrColorSwapchain = XR_NULL_HANDLE;
		XrSwapchain m_xrDepthSwapchain = XR_NULL_HANDLE;

		std::vector< XrSwapchainImageVulkan2KHR > m_vecSwapchainColorImages;
		std::vector< XrSwapchainImageVulkan2KHR > m_vecSwapchainDepthImages;
		std::vector< SMultiviewRenderTarget > m_vecMultiviewRenderTargets;

		// Contains texture information such as recommended and max extents (width, height)
		std::vector< XrViewConfigurationView > m_vecEyeConfigs;

		// Contains fov and pose info for each view
		std::vector< XrView > m_vecEyeViews;

	};
}
