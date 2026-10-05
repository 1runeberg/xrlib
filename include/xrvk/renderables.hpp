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

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include <xrvk/buffer.hpp>
#include <xrvk/descriptors.hpp>
#include <xrvk/environment.hpp>
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

	// Frame order for renderables, material pipelines pick Opaque or Transparent from each section's alpha mode
	// Queues set draw order only, each pipeline keeps its own depth and blend state
	enum class ERenderQueue : uint8_t
	{
		Background,	 // List order before scene geometry, pipelines usually skip depth writes
		Opaque,		 // With opaque material sections
		Transparent, // Sorted back to front with blended material sections
		PostScene	 // List order after scene geometry, the default for custom pipelines
	};

	class CRenderable;

	// A whole renderable or one material section, in the order EndRenderFrame records it
	struct SQueuedDraw
	{
		CRenderable *pRenderable;
		uint32_t section;  // UINT32_MAX draws the whole renderable
		uint32_t instance; // UINT32_MAX draws every instance
		float depth;	   // View distance, only Transparent draws are sorted by it
		ERenderQueue queue;
	};

	// Stable, so draws keep list order within Background, Opaque and PostScene
	inline void SortQueuedDraws( std::vector< SQueuedDraw > &draws )
	{
		std::stable_sort( draws.begin(), draws.end(), []( const SQueuedDraw &a, const SQueuedDraw &b )
		{
			if ( a.queue != b.queue )
				return a.queue < b.queue;

			return a.queue == ERenderQueue::Transparent && a.depth > b.depth;
		} );
	}

	struct CRenderInfo;
	class CRenderable
	{
	  public:
		bool isVisible = true;
		ERenderQueue renderQueue = ERenderQueue::PostScene; // Custom pipelines only
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

		// pRenderInfo must outlive the renderable, it retires replaced buffers until the GPU is done with them
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

		// Interfaces, InitBuffers is safe between frames as replaced buffers are retired rather than freed
		virtual void Reset() = 0;
		virtual VkResult InitBuffers( bool bReset = false ) = 0;
		virtual void Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo  ) = 0;

		// Allocation failure leaves instances unchanged
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

		// Selects the frame in flight being recorded and copies CPU-side data into its buffers
		// EndRenderFrame calls this for visible renderables once that frame's earlier GPU work has completed
		virtual VkResult UpdateFrameBuffers( uint32_t unFrameIndex );
		
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

		// The instance buffer holds a copy of the matrices per frame in flight
		// Bind the frame being recorded at GetInstanceBufferOffset
		CDeviceBuffer *GetInstanceBuffer() { return m_pInstanceBuffer.get(); }
		VkDeviceSize GetInstanceBufferOffset() const { return m_unInstanceFrameSize * m_unFrameIndex; }

		XrMatrix4x4f *GetModelMatrix( uint32_t unInstanceIndex = 0, bool bRefresh = false );
		XrMatrix4x4f *GetUpdatedModelMatrix( uint32_t unInstanceIndex = 0 ) { return GetModelMatrix( unInstanceIndex, true ); }
	  
	protected:
		CSession *m_pSession = nullptr;
		CRenderInfo *m_pRenderInfo = nullptr;
		CDeviceBuffer *m_pIndexBuffer = nullptr;
		CDeviceBuffer *m_pVertexBuffer = nullptr;

		std::unique_ptr< CDeviceBuffer > m_pInstanceBuffer;
		VkDeviceSize m_unInstanceFrameSize = 0;
		uint32_t m_unFramesInFlight = 1;
		uint32_t m_unFrameIndex = 0;

		uint32_t AllFrameBits() const { return ( 1u << m_unFramesInFlight ) - 1; }

		// Hands a buffer to the render info, which frees it once no frame in flight can still read it
		void RetireBuffer( CDeviceBuffer *&pBuffer );
		void RetireBuffer( std::unique_ptr< CDeviceBuffer > &pBuffer );

		// Keeps a buffer that already fits instanceMatrices, otherwise retires it and creates a new one
		VkResult InitInstanceBuffers();
		VkResult UpdateInstancesBuffer();
		void BindInstanceBuffer( VkCommandBuffer commandBuffer );

		// Interfaces, DeleteBuffers frees immediately so it's for destructors only
		virtual void DeleteBuffers() = 0;

	  private:
		VkResult CreateInstanceBuffer( std::unique_ptr< CDeviceBuffer > &outBuffer, const std::vector< XrMatrix4x4f > &matrices );
	};

	class CRenderInfo
	{
	  public:

		// Frames in flight let the CPU record a frame while the GPU renders earlier ones
		explicit CRenderInfo( CSession* pSession, uint32_t unFramesInFlight = 2 );
		~CRenderInfo();

		static constexpr uint32_t k_unMaxFramesInFlight = 8;
		uint32_t GetFramesInFlight() const { return m_unFramesInFlight; }

		// Per-frame strides that keep descriptor offsets aligned
		VkDeviceSize GetUniformStride( VkDeviceSize unSize ) const { return ( unSize + m_unUniformOffsetAlignment - 1 ) / m_unUniformOffsetAlignment * m_unUniformOffsetAlignment; }
		VkDeviceSize GetStorageStride( VkDeviceSize unSize ) const { return ( unSize + m_unStorageOffsetAlignment - 1 ) / m_unStorageOffsetAlignment * m_unStorageOffsetAlignment; }

		// Creates a mapped host-coherent buffer with unStride bytes per frame in flight
		// Each frame's section is zeroed, then seeded from pData when given
		VkResult CreateFrameBuffer( std::unique_ptr< CDeviceBuffer > &outBuffer, VkBufferUsageFlags usageFlags, VkDeviceSize unStride, const void *pData = nullptr, VkDeviceSize unSize = 0 );

		// Frees the buffer once every frame in flight has finished with it
		void RetireBuffer( std::unique_ptr< CDeviceBuffer > pBuffer );

		// Selects the frame in flight being recorded and refreshes its scene lighting and environment
		// Also frees retired buffers once every frame has passed, EndRenderFrame calls this after the fence wait
		VkResult UpdateFrameBuffers( uint32_t unFrameIndex );

		// For renderables
		std::vector< VkPipelineLayout > vecPipelineLayouts;
		std::vector< VkPipeline > vecGraphicsPipelines;

		// Variants indexed by blend, double-sided and mirrored-instance bits
		std::unordered_map< uint32_t, std::array< uint32_t, 8 > > materialPipelines;
		std::vector< CRenderable * > vecRenderables;

		uint16_t AddNewLayout( VkPipelineLayout layout = VK_NULL_HANDLE );
		uint16_t AddNewPipeline( VkPipeline pipeline = VK_NULL_HANDLE );
		uint32_t AddNewRenderable( CRenderable *renderable );

		// For stencil ops
		VkPipelineLayout stencilLayout = VK_NULL_HANDLE;
		std::vector< VkPipeline > stencilPipelines;

		// For global scene lighting, the CPU-side pSceneLighting is copied to each frame's section of the buffer
		uint32_t lightingPoolId = 0;
		uint32_t lightingLayoutId = 0;
		SSceneLighting *pSceneLighting = nullptr;
		std::unique_ptr< CDeviceBuffer > pSceneLightingBuffer;
		std::vector< VkDescriptorSet > vecSceneLightingDescriptors; // One per frame in flight

		void SetupSceneLighting();
		VkDescriptorSet GetSceneLightingDescriptor() const { return vecSceneLightingDescriptors[ state.unFrameIndex ]; }

		// Safe between frames, each frame in flight rebinds the images once its earlier GPU work has completed
		// Shared ownership keeps images alive, nullptr disables IBL without changing direct/ambient light
		VkResult SetEnvironment( std::shared_ptr< CEnvironmentLighting > environment, float intensity = 1.f, float rotation = 0.f );

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
			uint32_t unFrameIndex = 0; // Frame in flight being recorded

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
		CSession *m_pSession = nullptr;
		uint32_t m_unFramesInFlight = 2;
		VkDeviceSize m_unUniformOffsetAlignment = 1;
		VkDeviceSize m_unStorageOffsetAlignment = 1;

		// Retired buffers are freed once each frame in flight has passed its fence
		struct SRetiredBuffer
		{
			uint32_t flgFrames;
			std::unique_ptr< CDeviceBuffer > pBuffer;
		};
		std::vector< SRetiredBuffer > m_vecRetiredBuffers;

		SSceneLighting m_sceneLighting {};
		VkDeviceSize m_unSceneLightingStride = 0;

		// Replaced environments stay alive until every frame has rebound the current one
		uint32_t m_flgDirtyEnvironmentFrames = 0;
		std::shared_ptr< CEnvironmentLighting > m_pDefaultEnvironment;
		std::shared_ptr< CEnvironmentLighting > m_pEnvironment;
		std::vector< std::shared_ptr< CEnvironmentLighting > > m_vecRetiredEnvironments;
	};

} // namespace xrlib
