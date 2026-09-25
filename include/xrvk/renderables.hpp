/* 
 * Copyright 2024-26 Rune Berg
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

#include <filesystem>
#include <fstream>

#include <vector>
#include <array>
#include <cstdint>
#include <limits>

#include <xrvk/buffer.hpp>
#include <xrvk/descriptors.hpp>
#include <xrvk/lighting.hpp>


namespace xrlib
{
	static const uint32_t k_pcrSize = sizeof( XrMatrix4x4f ) * 2;

	struct SInstanceState
	{
		XrSpace space = XR_NULL_HANDLE;
		XrPosef pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } };
		XrVector3f scale = { 1.f, 1.f, 1.f };

		SInstanceState( XrVector3f defaultScale = { 1.f, 1.f, 1.f } )
			: scale( defaultScale )
		{
		}

		SInstanceState( XrSpace defaultSpace = XR_NULL_HANDLE, XrVector3f defaultScale = { 1.f, 1.f, 1.f } )
			: space( defaultSpace )
			, scale( defaultScale )
		{
		}

		~SInstanceState() {}
	};

	struct CRenderInfo;
	class CRenderable
	{
	  public:
		bool isVisible = true;
		uint16_t pipelineLayoutIndex = 0;
		uint16_t graphicsPipelineIndex = 0;
		uint32_t descriptorLayoutIndex = 0;

		std::vector< VkDescriptorSet > vertexDescriptors;
		std::vector< SInstanceState > instances;
		std::vector< XrMatrix4x4f > instanceMatrices;

		const VkDeviceSize instanceOffsets[ 4 ] = { 0, 4 * sizeof( float ), 8 * sizeof( float ), 12 * sizeof( float ) };

		// Shader descriptor buffers
		CDeviceBuffer *pVertexDescriptorsBuffer = nullptr;
		CDeviceBuffer *pFragmentDescriptorsBuffer = nullptr;

		CRenderable( 
			CSession* pSession, 
			CRenderInfo* pRenderInfo,
			uint16_t pipelineLayoutIdx, 
			uint16_t graphicsPipelineIdx, 
			uint32_t descriptorLayoutIdx = ( std::numeric_limits< uint32_t >::max )(),
			bool bIsVisible = true, 
			XrVector3f xrScale = { 1.f, 1.f, 1.f }, 
			XrSpace xrSpace = XR_NULL_HANDLE );
		
		virtual ~CRenderable();

		// Interfaces
		virtual void Reset() = 0;
		virtual VkResult InitBuffers( bool bReset = false ) = 0;
		virtual void Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo  ) = 0;

		uint32_t AddInstance( uint32_t unCount, XrVector3f scale = { 1.f, 1.f, 1.f } );

		VkResult InitBuffer(
			CDeviceBuffer *pBuffer,
			VkBufferUsageFlags usageFlags,
			VkDeviceSize unSize,
			void *pData,
			VkMemoryPropertyFlags memPropFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VkAllocationCallbacks *pCallbacks = nullptr );

		VkResult InitInstancesBuffer(
			CDeviceBuffer *pBuffer,
			VkBufferUsageFlags usageFlags,
			VkDeviceSize unSize,
			void *pData,
			VkMemoryPropertyFlags memPropFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VkAllocationCallbacks *pCallbacks = nullptr );

		CDeviceBuffer *UpdateInstancesBuffer( VkCommandBuffer transferCmdBuffer );
		
		void ResetScale( float x, float y, float z, uint32_t unInstanceIndex = 0 );
		void ResetScale( float fScale, uint32_t unInstanceIndex = 0 );
		void Scale( float fPercent, uint32_t unInstanceIndex = 0 );

		void UpdateModelMatrix( uint32_t unInstanceIndex = 0, XrSpace baseSpace = XR_NULL_HANDLE, XrTime time = 0, bool bForceUpdate = false );

		XrVector3f *GetPosition( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].pose.position; }
		XrQuaternionf *GetOrientation( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].pose.orientation; }
		XrVector3f *GetScale( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].scale; }
		XrPosef *GetPose( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].pose; }

		[[nodiscard]] uint32_t GetInstanceCount() const { return (uint32_t) instances.size(); }
		CDeviceBuffer *GetIndexBuffer() { return m_pIndexBuffer; }
		CDeviceBuffer *GetVertexBuffer() { return m_pVertexBuffer; }
		CDeviceBuffer *GetInstanceBuffer() { return m_pInstanceBuffer; }

		XrMatrix4x4f *GetModelMatrix( uint32_t unInstanceIndex = 0, bool bRefresh = false );
		XrMatrix4x4f *GetUpdatedModelMatrix( uint32_t unInstanceIndex = 0 ) { return GetModelMatrix( unInstanceIndex, true ); }
	  
	protected:
		CSession *m_pSession = nullptr;
		CDeviceBuffer *m_pIndexBuffer = nullptr;
		CDeviceBuffer *m_pVertexBuffer = nullptr;
		CDeviceBuffer *m_pInstanceBuffer = nullptr;

		// Interfaces
		virtual void DeleteBuffers() = 0;
	};

	class CRenderInfo
	{
	  public:

		explicit CRenderInfo( CSession* pSession );
		~CRenderInfo();

		// For renderables
		std::vector< VkPipelineLayout > vecPipelineLayouts;
		std::vector< VkPipeline > vecGraphicsPipelines;
		std::vector< CRenderable * > vecRenderables;

		uint16_t AddNewLayout( VkPipelineLayout layout = VK_NULL_HANDLE );
		uint16_t AddNewPipeline( VkPipeline pipeline = VK_NULL_HANDLE );
		uint32_t AddNewRenderable( CRenderable *renderable );

		// For stencil ops
		VkPipelineLayout stencilLayout = VK_NULL_HANDLE;
		std::vector< VkPipeline > stencilPipelines;

		// For global scene lighting
		uint32_t lightingPoolId = 0;
		uint32_t lightingLayoutId = 0;
		CDeviceBuffer *pSceneLightingBuffer = nullptr;
		SSceneLighting *pSceneLighting = nullptr;
		VkDescriptorSet sceneLightingDescriptor = VK_NULL_HANDLE;

		void SetupSceneLighting();

		// For descriptor management
		CDescriptorManager *pDescriptors = nullptr;

		struct SFrameState
		{
			float nearZ = 0.1f;
			float farZ = 10000.f;

			float minDepth = 0.f;
			float maxDepth = 1.f;

			uint32_t unCurrentSwapchainImage_Color = 0;
			uint32_t unCurrentSwapchainImage_Depth = 0;

			XrFrameState frameState { XR_TYPE_FRAME_STATE };
			XrViewState sharedEyeState { XR_TYPE_VIEW_STATE };
			XrCompositionLayerProjection projectionLayer { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
			XrCompositionLayerFlags compositionLayerFlags = 0;
			XrEnvironmentBlendMode environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

			XrVector3f eyeScale = { 1.0f, 1.0f, 1.0f };
			XrPosef hmdPose;

			std::array< XrMatrix4x4f, 2 > eyeVPs;
			std::array< XrMatrix4x4f, 2 > eyeProjectionMatrices;
			std::array< XrMatrix4x4f, 2 > eyeViewMatrices;

			std::vector< XrOffset2Di > imageRectOffsets = { { 0, 0 }, { 0, 0 } };
			std::vector< VkClearValue > clearValues;

			std::vector< XrCompositionLayerProjectionView > projectionLayerViews = { 
				XrCompositionLayerProjectionView { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }, 
				XrCompositionLayerProjectionView { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };

			std::vector< XrCompositionLayerBaseHeader * > frameLayers;
			std::vector< XrCompositionLayerBaseHeader * > preAppFrameLayers;
			std::vector< XrCompositionLayerBaseHeader * > postAppFrameLayers;

			std::vector< CDeviceBuffer * > vecStagingBuffers;

			void ClearStagingBuffers()
			{
				for ( CDeviceBuffer *pStagingBuffer : vecStagingBuffers )
				{
					if ( pStagingBuffer == nullptr )
						delete pStagingBuffer;
				}

				vecStagingBuffers.clear();
			}

			SFrameState( float near, float far, float min = 0.f, float max = 1.f )
				: nearZ( near )
				, farZ( far )
				, minDepth( min )
				, maxDepth( max )
			{
				XrPosef_CreateIdentity( &hmdPose );

				clearValues.assign( 4, {} );
				clearValues[ 0 ].color = { 0.0f, 0.0f, 0.0f, 1.0f }; // Color MSAA
				clearValues[ 1 ].color = { 0.0f, 0.0f, 0.0f, 1.0f }; // Color resolve
				clearValues[ 2 ].depthStencil = { 1.0f, 0u };		 // Depth/stencil MSAA
				clearValues[ 3 ].depthStencil = { 1.0f, 0u };		 // Depth/stencil resolve
			};

			SFrameState() 
			{ 
				XrPosef_CreateIdentity( &hmdPose ); 

				clearValues.assign( 4, {} );
				clearValues[ 0 ].color = { 0.0f, 0.0f, 0.0f, 1.0f }; // Color MSAA
				clearValues[ 1 ].color = { 0.0f, 0.0f, 0.0f, 1.0f }; // Color resolve
				clearValues[ 2 ].depthStencil = { 1.0f, 0u };		 // Depth/stencil MSAA
				clearValues[ 3 ].depthStencil = { 1.0f, 0u };		 // Depth/stencil resolve
			};

			~SFrameState() = default;
		}state;

	  private:
		VkDevice m_device = VK_NULL_HANDLE;

	};

} // namespace xrlib
