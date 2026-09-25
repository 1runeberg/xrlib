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


#pragma once

#include <vector>
#include <array>
#include <cfloat>

#include <xrvk/renderables.hpp>
#include <xrvk/primitive.hpp>
#include <xrvk/mesh.hpp>
#include <xrvk/gltf.hpp>

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

	struct SPipelineStateInfo
	{
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineViewportStateCreateInfo viewport;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineRasterizationStateCreateInfo rasterization;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkPipelineInputAssemblyStateCreateInfo assembly;
		VkPipelineMultisampleStateCreateInfo multisample;
		VkPipelineDepthStencilStateCreateInfo depthStencil;

		// Store vectors to maintain lifetime of data referenced by create infos
		std::vector< VkViewport > viewports;
		std::vector< VkRect2D > scissors;
		std::vector< VkPipelineColorBlendAttachmentState > colorBlendAttachments;
		std::vector< VkDynamicState > dynamicStates;
	};

	struct SPipelineCreationParams
	{
		VkRenderPass renderPass;
		bool useVisMask;
		VkFormat depthFormat;
		uint32_t subpassIndex;
	};

	struct SPipelines
	{
		uint16_t primitiveLayout = 0;
		uint16_t pbrLayout = 0;

		uint32_t primitives = 0;
		uint32_t pbr = 0;
		uint32_t sky = 0;
		uint32_t floor = 0;

		uint32_t pbrFragmentDescriptorLayout = 0;
		uint32_t pbrFragmentDescriptorPool = 0;
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

	class CStereoRender
	{
	  public:
		CStereoRender( CSession *pSession, VkFormat vkColorFormat, VkFormat vkDepthFormat );
		~CStereoRender();

#pragma region CONSTS

		const uint32_t k_EyeCount = 2;
		const uint32_t k_unStereoViewMask = 0b00000011;
		const uint32_t k_unStereoConcurrentMask = 0b00000011;

#pragma endregion CONSTS

#pragma region TYPES

		struct SMultiviewRenderTarget
		{
			 VkImage vkMSAAColorTexture = VK_NULL_HANDLE;
			 VkDeviceMemory vkMSAAColorMemory = VK_NULL_HANDLE; ///< Owned allocation, freed after its image.
			 VkImageView vkMSAAColorView = VK_NULL_HANDLE;

			 VkImage vkColorTexture = VK_NULL_HANDLE;
			 VkDescriptorImageInfo vkColorImageDescriptor { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

			 VkImage vkMSAADepthTexture = VK_NULL_HANDLE;
			 VkDeviceMemory vkMSAADepthMemory = VK_NULL_HANDLE; ///< Owned allocation, freed after its image.
			 VkImageView vkMSAADepthView = VK_NULL_HANDLE;

			 VkImage vkDepthTexture = VK_NULL_HANDLE;
			 VkImageView vkDepthImageView = VK_NULL_HANDLE;

			 VkFramebuffer vkFrameBuffer = VK_NULL_HANDLE;

			 VkCommandBuffer vkRenderCommandBuffer = VK_NULL_HANDLE;
			 VkFence vkRenderCommandFence = VK_NULL_HANDLE;

			 VkCommandBuffer vkTransferCommandBuffer = VK_NULL_HANDLE;
			 VkFence vkTransferCommandFence = VK_NULL_HANDLE;

			 void SetImageViewArray( std::array< VkImageView, 4 > &arrImageViews )
			 {
				 arrImageViews[ 0 ] = vkMSAAColorView;					// For msaa rendering (color)
				 arrImageViews[ 1 ] = vkColorImageDescriptor.imageView; // As msaa resolve target, color texture from openxr runtime
				 arrImageViews[ 2 ] = vkMSAADepthView;					// For msaa rendering (depth)
				 arrImageViews[ 3 ] = vkDepthImageView;					// As msaa resolve target, depth texture from openxr runtime
			 }
		};

#pragma endregion TYPES

		XrResult Init( uint32_t unTextureFaceCount = 1, uint32_t unTextureMipCount = 1 );
		XrResult CreateSwapchains( uint32_t unFaceCount = 1, uint32_t unMipCount = 1 );
		XrResult CreateSwapchainImages( std::vector< XrSwapchainImageVulkan2KHR > &outSwapchainImages, XrSwapchain xrSwapchain );

		XrResult CreateRenderPass( VkRenderPass &outRenderPass, bool bWithStencilSubpass );
		XrResult InitRendering_Multiview();
		XrResult CreateRenderPass_Multiview( bool bUseVisMask );
		XrResult CreateFramebuffers_Multiview( VkRenderPass vkRenderPass );

		XrResult CreateRenderTargets_Multiview( VkImageViewCreateFlags colorCreateFlags = 0, VkImageViewCreateFlags depthCreateFlags = 0, void *pColorNext = nullptr, void *pDepthNext = nullptr, VkAllocationCallbacks *pCallbacks = nullptr );
		XrResult CreateRenderTargetSamplers( VkSamplerCreateInfo &samplerCI, VkAllocationCallbacks *pCallbacks = nullptr );

		VkResult CreateDescriptorPool( VkDescriptorPool &outPool, std::vector< VkDescriptorPoolSize > &poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags flags = 0, void *pNext = nullptr );

		VkAttachmentDescription2 GenerateColorAttachmentDescription();
		VkAttachmentDescription2 GenerateDepthAttachmentDescription();
		VkSubpassDescription GenerateSubpassDescription();

		VkRenderPassMultiviewCreateInfo GenerateMultiviewCI();

		VkRenderPassCreateInfo2 GenerateRenderPassCI(
			std::vector< VkAttachmentDescription2 > &vecAttachmentDescriptions,
			std::vector< VkSubpassDescription2 > &vecSubpassDescriptions,
			std::vector< VkSubpassDependency2 > &vecSubpassDependencies,
			const VkRenderPassCreateFlags vkCreateFlags = 0,
			const void *pNext = nullptr );

		VkFramebufferCreateInfo GenerateMultiviewFrameBufferCI( std::array< VkImageView, 4 > &arrImageViews, VkRenderPass vkRenderPass, VkFramebufferCreateFlags vkCreateFlags = 0, const void *pNext = nullptr );

		VkSamplerCreateInfo GenerateImageSamplerCI( VkSamplerCreateFlags flags = 0, void *pNext = nullptr );

		VkPipelineColorBlendAttachmentState GenerateColorBlendAttachment( bool bEnableAlphaBlending = false );

		VkPipelineLayoutCreateInfo
			GeneratePipelineLayoutCI( std::vector< VkPushConstantRange > &vecPushConstantRanges, std::vector< VkDescriptorSetLayout > &vecDescriptorSetLayouts, VkPipelineLayoutCreateFlags createFlags = 0, void *pNext = nullptr );

		VkPipelineVertexInputStateCreateInfo GeneratePipelineStateCI_VertexInput(
			std::vector< VkVertexInputBindingDescription > &vecVertexBindingDescriptions,
			std::vector< VkVertexInputAttributeDescription > &vecVertexAttributeDescriptions,
			VkPipelineVertexInputStateCreateFlags createFlags = 0,
			void *pNext = nullptr );

		VkPipelineInputAssemblyStateCreateInfo
			GeneratePipelineStateCI_Assembly( VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkBool32 primitiveRestartEnable = VK_FALSE, VkPipelineInputAssemblyStateCreateFlags createFlags = 0, void *pNext = nullptr );

		VkPipelineTessellationStateCreateInfo GeneratePipelineStateCI_TesselationCI( uint32_t patchControlPoints = 0, VkPipelineTessellationStateCreateFlags createFlags = 0, void *pNext = nullptr );

		VkPipelineViewportStateCreateInfo GeneratePipelineStateCI_ViewportCI( std::vector< VkViewport > &vecViewports, std::vector< VkRect2D > &vecScissors, VkPipelineViewportStateCreateFlags createFlags = 0, void *pNext = nullptr );

		VkPipelineRasterizationStateCreateInfo GeneratePipelineStateCI_RasterizationCI(
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
			void *pNext = nullptr );

		VkPipelineMultisampleStateCreateInfo GeneratePipelineStateCI_MultisampleCI(
			VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_2_BIT,
			VkBool32 sampleShadingEnable = VK_TRUE,
			float minSampleShading = 0.25f,
			VkSampleMask *pSampleMask = nullptr,
			VkBool32 alphaToCoverageEnable = VK_FALSE,
			VkBool32 alphaToOneEnable = VK_FALSE,
			VkPipelineMultisampleStateCreateFlags createFlags = 0,
			void *pNext = nullptr );

		VkPipelineDepthStencilStateCreateInfo GeneratePipelineStateCI_DepthStencilCI(
			VkBool32 depthTestEnable = VK_TRUE,
			VkBool32 depthWriteEnable = VK_TRUE,
			VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS,
			VkBool32 depthBoundsTestEnable = VK_FALSE,
			VkBool32 stencilTestEnable = VK_FALSE,
			VkStencilOpState front = {},
			VkStencilOpState back = {},
			float minDepthBounds = 0.f,
			float maxDepthBounds = 0.f,
			VkPipelineDepthStencilStateCreateFlags createFlags = 0,
			void *pNext = nullptr );

		VkPipelineColorBlendStateCreateInfo GeneratePipelineStateCI_ColorBlendCI(
			std::vector< VkPipelineColorBlendAttachmentState > &vecColorBlenAttachmentStates,
			VkBool32 logicOpEnable = VK_FALSE,
			VkLogicOp logicOp = VK_LOGIC_OP_CLEAR,
			VkPipelineColorBlendStateCreateFlags createFlags = 0,
			void *pNext = nullptr );

		VkPipelineDynamicStateCreateInfo GeneratePipelineStateCI_DynamicStateCI( std::vector< VkDynamicState > &vecDynamicStates, VkPipelineDynamicStateCreateFlags createFlags = 0, void *pNext = nullptr );

		VkResult CreateGraphicsPipeline(
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
			const VkPipelineCache pipelineCache = VK_NULL_HANDLE,
			const uint32_t unSubpassIndex = 0,
			const VkPipelineCreateFlags createFlags = 0,
			const void *pNext = nullptr,
			const VkAllocationCallbacks *pCallbacks = nullptr );

		VkResult CreateGraphicsPipeline_Stencil(
#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
#endif
			uint32_t unSubpassIndex,
			VkPipelineLayout &outPipelineLayout,
			VkPipeline &outPipeline,
			VkRenderPass vkRenderPass,
			std::string sVertexShaderFilename,
			std::string sFragmentShaderFilename );

		VkResult CreateGraphicsPipeline_Stencils(
#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
#endif
			VkPipelineLayout &outPipelineLayout,
			std::vector< VkPipeline > &outPipelines,
			VkRenderPass vkRenderPass,
			std::vector< std::string > vecVertexShaders,
			std::vector< std::string > vecFragmentShaders );

		VkResult CreateGraphicsPipeline_Primitives(
		#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
		#endif
			VkPipelineLayout &outPipelineLayout,
			VkPipeline &outPipeline,
			VkRenderPass vkRenderPass,
			std::string sVertexShaderFilename,
			std::string sFragmentShaderFilename );

		VkResult CreateGraphicsPipeline_PBR(
		#ifdef XR_USE_PLATFORM_ANDROID
			AAssetManager *assetManager,
		#endif
			SPipelines &outPipelines,
			uint32_t &outPipelineIndex,
			CRenderInfo* pRenderInfo,
			uint32_t unPbrDescriptorPoolCount,
			VkRenderPass vkRenderPass,
			std::string sVertexShaderFilename,
			std::string sFragmentShaderFilename,
			bool bCreateAsMainPBRPipeline );

		VkResult CreateGraphicsPipeline_CustomPBR(
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
			uint32_t unDescriptorPoolCount);

		VkResult CreateGraphicsPipeline( 
			VkPipelineLayout &outLayout, 
			VkPipeline &outPipeline, 
			VkRenderPass vkRenderPass, 
			std::vector< VkDescriptorSetLayout > &vecLayouts, 
			SShaderSet *pShaderSet_WillBeDestroyed );

		SPipelineStateInfo CreateDefaultPipelineState( 
			std::vector< VkVertexInputBindingDescription > &bindings, 
			std::vector< VkVertexInputAttributeDescription > &attributes, 
			uint32_t textureWidth, 
			uint32_t textureHeight );

		SPipelineStateInfo CreateDefaultPipelineState( 
			uint32_t textureWidth, 
			uint32_t textureHeight );

		void ConfigureDepthStencil( 
			VkPipelineDepthStencilStateCreateInfo &outDepthStencilCI, 
			bool bUseVisMask, 
			VkFormat depthFormat );

		VkResult SetupPBRDescriptors( 
			SPipelines &outPipelines, 
			CRenderInfo *pRenderInfo, 
			uint32_t poolCount );

		void SetupPrimitiveVertexAttributes( SShaderSet &shaderSet );
		void SetupPBRVertexAttributes( SShaderSet &shaderSet );

		VkResult CreateBasePipeline( 
			VkPipelineLayout &outLayout, 
			VkPipeline &outPipeline, 
			SShaderSet *pShaderSet, 
			const SPipelineCreationParams &params, 
			std::vector< VkDescriptorSetLayout > &layouts );

		VkResult AddRenderPass( VkRenderPassCreateInfo2 *renderPassCI, VkAllocationCallbacks *pAllocator = nullptr );
		/** @brief Render a complete XR frame and report wait/begin/end failures.
		 * @param[in] renderPass Renderer-owned pass compatible with its targets.
		 * @param[in] pRenderInfo Borrowed scene and per-frame state.
		 * @param[in] stencils Optional borrowed visibility-mask geometry.
		 * @return Frame transport result; failure must not be counted as a presented frame.
		 */
		XrResult RenderFrame( const VkRenderPass renderPass, CRenderInfo *pRenderInfo, std::vector< CPlane2D * > &stencils );
		bool StartRenderFrame( CRenderInfo *pRenderInfo );
		/** @brief Render and end a frame previously begun by StartRenderFrame.
		 * @param[in] renderPass Renderer-owned pass compatible with its targets.
		 * @param[in] pRenderInfo Borrowed scene and begun-frame state.
		 * @param[in] stencils Optional borrowed visibility-mask geometry.
		 * @return The xrEndFrame result, including validation or native presentation failure.
		 */
		XrResult EndRenderFrame( const VkRenderPass renderPass, CRenderInfo *pRenderInfo, std::vector< CPlane2D * > &stencils );

		void BeginDraw(
			const uint32_t unSwpachainImageIndex,
			std::vector< VkClearValue > &vecClearValues,
			const bool startCommandBufferRecording = true,
			const VkRenderPass renderpass = VK_NULL_HANDLE,
			const VkSubpassContents subpass = VK_SUBPASS_CONTENTS_INLINE );

		void SubmitDraw(
			const uint32_t unSwpachainImageIndex,
			std::vector< CDeviceBuffer * > &vecStagingBuffers,
			const uint32_t timeoutNs = 1000000000,
			const VkCommandBufferResetFlags transferBufferResetFlags = 0,
			const VkCommandBufferResetFlags renderBufferResetFlags = 0 );

		void BeginBufferUpdates( const uint32_t unSwpachainImageIndex );
		void SubmitBufferUpdates( const uint32_t unSwpachainImageIndex );
		void CalculateViewMatrices( std::array< XrMatrix4x4f, 2 > &outViewMatrices, const XrVector3f *eyeScale );

		VkDescriptorPool CreateDescriptorPool( 
			const std::vector< VkDescriptorPoolSize > &poolSizes, 
			uint32_t maxSets, 
			VkDescriptorPoolCreateFlags flags = 0, 
			const void *pNext = nullptr );

		VkDescriptorSet UpdateDescriptorSets(
			VkDescriptorPool descriptorPool,
			std::vector< VkDescriptorSetLayout > &setLayouts,
			const std::vector< VkDescriptorBufferInfo > &bufferInfos,
			const std::vector< VkDescriptorImageInfo > &imageInfos,
			const std::vector< VkBufferView > &texelBufferViews,
			const std::vector< VkDescriptorType > &descriptorTypes,
			const std::vector< uint32_t > &bindings,
			const void *pNextAllocate = nullptr,
			const void *pNextWrite = nullptr );

		template < typename VertexType > 
		void FillVertexAttributes( 
			std::vector< VkVertexInputAttributeDescription > &outVecAttributes, 
			uint32_t binding, 
			uint32_t locStart, 
			std::initializer_list< size_t > excludeOffsets = {} ) 
		{
			using Vertex = VertexType;

			if constexpr ( std::is_same_v< Vertex, SMeshVertex > )
			{
				const size_t attributeOffsets[] = { 
					offsetof( Vertex, position ), 
					offsetof( Vertex, normal ), 
					offsetof( Vertex, uv0 ), 
					offsetof( Vertex, color0 ), 
					offsetof( Vertex, joints ), 
					offsetof( Vertex, weights ) };

				const VkFormat attributeFormats[] = { 
					VK_FORMAT_R32G32B32_SFLOAT, 
					VK_FORMAT_R32G32B32_SFLOAT, 
					VK_FORMAT_R32G32_SFLOAT, 
					VK_FORMAT_R32G32B32_SFLOAT, 
					VK_FORMAT_R32G32B32A32_UINT, 
					VK_FORMAT_R32G32B32A32_SFLOAT };

				// Generate and skip attribs if offset is in exclude list
				for ( size_t i = 0; i < std::size( attributeOffsets ); ++i )
					if ( std::find( excludeOffsets.begin(), excludeOffsets.end(), attributeOffsets[ i ] ) == excludeOffsets.end() )
						outVecAttributes.push_back( { 
							locStart++, 
							binding, 
							attributeFormats[ i ], 
							static_cast<uint32_t> ( attributeOffsets[ i ] ) } );

				return;
			}

			if constexpr ( std::is_same_v< Vertex, XrMatrix4x4f > )
			{
				outVecAttributes.push_back( { locStart, binding, VK_FORMAT_R32G32B32A32_SFLOAT, 0 } );
				outVecAttributes.push_back( { locStart + 1, binding, VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof( float ) } );
				outVecAttributes.push_back( { locStart + 2, binding, VK_FORMAT_R32G32B32A32_SFLOAT, 8 * sizeof( float ) } );
				outVecAttributes.push_back( { locStart + 3, binding, VK_FORMAT_R32G32B32A32_SFLOAT, 12 * sizeof( float ) } );

				return;
			}
		}

#pragma region GETTERS

		const VkPushConstantRange GetEyeMatricesPushConstant() 
		{ 
			return VkPushConstantRange 
			{ 
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, 
				.offset = 0, 
				.size = k_pcrSize
			}; 
		}

		const bool GetUseVisMask() { return m_bUseVisMask; }

		const uint32_t GetTextureWidth() { return m_unTextureWidth; }
		const uint32_t GetTextureHeight() { return m_unTextureHeight; }

		const VkFormat GetColorFormat() { return m_vkColorFormat; }
		const VkFormat GetDepthFormat() { return m_vkDepthFormat; }
		const VkCommandPool GetCommandPool() { return m_vkRenderCommandPool; }
		const VkCommandPool GetTransferPool() { return m_vkTransferCommandPool; }

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

		# pragma endregion GETTERS

		
		# pragma region PUBLIC_VARS

		std::vector< VkRenderPass > vecRenderPasses;
		VkClearColorValue vkClearColor = { { 0.05f, 0.05f, 0.05f, 1.0f } };

		#pragma endregion PUBLIC_VARS


	  private:
		CSession *m_pSession = nullptr;
		bool m_bUseVisMask = false;

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
