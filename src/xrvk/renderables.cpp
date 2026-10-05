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


#include <xrvk/renderables.hpp>

namespace xrlib
{

	CRenderable::CRenderable( 
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: 
			m_pSession( pSession ),
			m_pRenderInfo( pRenderInfo ),
			pipelineLayoutIndex( pipelineLayoutIdx ), 
			graphicsPipelineIndex( graphicsPipelineIdx ), 
			descriptorLayoutIndex( descriptorLayoutIdx ),
			isVisible( bIsVisible )
	{
		assert( pSession );
		if ( !pRenderInfo )
			throw std::invalid_argument( "Renderables need a CRenderInfo" );

		m_unFramesInFlight = pRenderInfo->GetFramesInFlight();

		// Fill descriptors (if any)
		if ( descriptorLayoutIdx < ( std::numeric_limits< uint32_t >::max )() )
			vertexDescriptors = pRenderInfo->pDescriptors->GetDescriptorSets( descriptorLayoutIdx );

		// Pre-fill first instance
		instances.push_back( SInstanceState { xrSpace, xrScale } );
		instanceMatrices.push_back( XrMatrix4x4f() );
		XrMatrix4x4f_CreateTranslationRotationScale( &instanceMatrices[ 0 ], GetPosition( 0 ), GetOrientation( 0 ), GetScale( 0 ) );
	}

	CRenderable::~CRenderable() 
	{ 
		if ( pFragmentDescriptorsBuffer )
			delete pFragmentDescriptorsBuffer;

		if ( pVertexDescriptorsBuffer )
			delete pVertexDescriptorsBuffer;
	}

	uint32_t CRenderable::AddInstance( uint32_t unCount, XrVector3f scale )
	{
		if ( unCount == 0 )
			return GetInstanceCount();

		auto newInstances = instances;
		auto newMatrices = instanceMatrices;
		for ( uint32_t i = 0; i < unCount; ++i )
		{
			newInstances.emplace_back( scale );
			XrMatrix4x4f matrix;
			XrMatrix4x4f_CreateTranslationRotationScale( &matrix, &newInstances.back().pose.position, &newInstances.back().pose.orientation, &scale );
			newMatrices.push_back( matrix );
		}

		std::unique_ptr< CDeviceBuffer > newBuffer;
		if ( CreateInstanceBuffer( newBuffer, newMatrices ) != VK_SUCCESS )
			throw std::runtime_error( "Failed to grow instance buffer" );

		RetireBuffer( m_pInstanceBuffer );
		m_pInstanceBuffer = std::move( newBuffer );
		m_unInstanceFrameSize = sizeof( XrMatrix4x4f ) * newMatrices.size();
		instances.swap( newInstances );
		instanceMatrices.swap( newMatrices );

		return GetInstanceCount();
	}

	VkResult CRenderable::InitBuffer( CDeviceBuffer *pBuffer, VkBufferUsageFlags usageFlags, VkDeviceSize unSize, void *pData, VkMemoryPropertyFlags memPropFlags, VkAllocationCallbacks *pCallbacks )
	{
		assert( pBuffer );
		return pBuffer->Init( usageFlags, memPropFlags, unSize, pData, true, pCallbacks );
	}

	VkResult CRenderable::InitInstancesBuffer( CDeviceBuffer *pBuffer, VkBufferUsageFlags usageFlags, VkDeviceSize unSize, void *pData, VkMemoryPropertyFlags memPropFlags, VkAllocationCallbacks *pCallbacks )
	{
		assert( pBuffer );
		return pBuffer->Init( usageFlags, memPropFlags, unSize, pData, true, pCallbacks );
	}

	void CRenderable::RetireBuffer( CDeviceBuffer *&pBuffer )
	{
		if ( !pBuffer )
			return;

		m_pRenderInfo->RetireBuffer( std::unique_ptr< CDeviceBuffer >( pBuffer ) );
		pBuffer = nullptr;
	}

	void CRenderable::RetireBuffer( std::unique_ptr< CDeviceBuffer > &pBuffer )
	{
		if ( pBuffer )
			m_pRenderInfo->RetireBuffer( std::move( pBuffer ) );
	}

	VkResult CRenderable::InitInstanceBuffers()
	{
		const VkDeviceSize frameSize = sizeof( XrMatrix4x4f ) * instanceMatrices.size();
		if ( m_pInstanceBuffer && m_unInstanceFrameSize == frameSize )
			return VK_SUCCESS;

		RetireBuffer( m_pInstanceBuffer );
		m_unInstanceFrameSize = 0;
		if ( instanceMatrices.empty() )
			return VK_SUCCESS;

		VK_CHECK_RETURN( CreateInstanceBuffer( m_pInstanceBuffer, instanceMatrices ) );
		m_unInstanceFrameSize = frameSize;
		return VK_SUCCESS;
	}

	VkResult CRenderable::CreateInstanceBuffer( std::unique_ptr< CDeviceBuffer > &outBuffer, const std::vector< XrMatrix4x4f > &matrices )
	{
		const VkDeviceSize frameSize = sizeof( XrMatrix4x4f ) * matrices.size();
		return m_pRenderInfo->CreateFrameBuffer( outBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, frameSize, matrices.data(), frameSize );
	}

	VkResult CRenderable::UpdateInstancesBuffer()
	{
		if ( instanceMatrices.empty() )
			return VK_SUCCESS;

		const VkDeviceSize size = sizeof( XrMatrix4x4f ) * instanceMatrices.size();
		if ( !m_pInstanceBuffer || size > m_unInstanceFrameSize )
			return VK_ERROR_INITIALIZATION_FAILED;

		std::memcpy( static_cast< uint8_t * >( m_pInstanceBuffer->GetMappedData() ) + GetInstanceBufferOffset(), instanceMatrices.data(), size );
		return VK_SUCCESS;
	}

	void CRenderable::BindInstanceBuffer( VkCommandBuffer commandBuffer )
	{
		const VkDeviceSize offset = GetInstanceBufferOffset();
		vkCmdBindVertexBuffers( commandBuffer, 1, 1, m_pInstanceBuffer->GetVkBufferPtr(), &offset );
	}

	VkResult CRenderable::UpdateFrameBuffers( uint32_t unFrameIndex )
	{
		if ( unFrameIndex >= m_unFramesInFlight )
			return VK_ERROR_INITIALIZATION_FAILED;

		m_unFrameIndex = unFrameIndex;
		return UpdateInstancesBuffer();
	}

	void CRenderable::ResetScale( float x, float y, float z, uint32_t unInstanceIndex )
	{
		// @todo - debug assert. ideally no checks here other than debug assert for perf
		instances[ unInstanceIndex ].scale = { x, y, z };
	}

	void CRenderable::ResetScale( float fScale, uint32_t unInstanceIndex )
	{
		// @todo - debug assert. ideally no checks here other than debug assert for perf
		instances[ unInstanceIndex ].scale = { fScale, fScale, fScale };
	}

	void CRenderable::Scale( float fPercent, uint32_t unInstanceIndex )
	{
		// @todo - debug assert. ideally no checks here other than debug assert for perf
		instances[ unInstanceIndex ].scale.x *= fPercent;
		instances[ unInstanceIndex ].scale.y *= fPercent;
		instances[ unInstanceIndex ].scale.z *= fPercent;
	}

	void CRenderable::UpdateModelMatrix( uint32_t unInstanceIndex, XrSpace baseSpace, XrTime time, bool bForceUpdate )
	{
		if ( instances[ unInstanceIndex ].space != XR_NULL_HANDLE && baseSpace != XR_NULL_HANDLE )
		{
			XrSpaceLocation spaceLocation { XR_TYPE_SPACE_LOCATION };
			if ( XR_UNQUALIFIED_SUCCESS( xrLocateSpace( instances[ unInstanceIndex ].space, baseSpace, time, &spaceLocation ) ) )
			{
				if ( spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT )
					instances[ unInstanceIndex ].pose.orientation = spaceLocation.pose.orientation;

				if ( spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT )
					instances[ unInstanceIndex ].pose.position = spaceLocation.pose.position;
			}
		}

		XrMatrix4x4f_CreateTranslationRotationScale( &instanceMatrices[ unInstanceIndex ], &instances[ unInstanceIndex ].pose.position, &instances[ unInstanceIndex ].pose.orientation, &instances[ unInstanceIndex ].scale );
	}

	XrMatrix4x4f *CRenderable::GetModelMatrix( uint32_t unInstanceIndex, bool bRefresh )
	{
		if ( bRefresh )
		{
			XrMatrix4x4f_CreateTranslationRotationScale( &instanceMatrices[ unInstanceIndex ], &instances[ unInstanceIndex ].pose.position, &instances[ unInstanceIndex ].pose.orientation, &instances[ unInstanceIndex ].scale );
		}

		return &instanceMatrices[ unInstanceIndex ];
	}

	CRenderInfo::CRenderInfo( CSession *pSession, uint32_t unFramesInFlight ) 
	{ 
		assert( pSession );

		if ( unFramesInFlight == 0 || unFramesInFlight > k_unMaxFramesInFlight )
			throw std::invalid_argument( "Unsupported frames in flight count" );

		m_pSession = pSession;
		m_device = pSession->GetVulkan()->GetVkLogicalDevice();
		m_unFramesInFlight = unFramesInFlight;
		pDescriptors = new CDescriptorManager( pSession );

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties( pSession->GetVulkan()->GetVkPhysicalDevice(), &properties );
		m_unUniformOffsetAlignment = ( std::max )( VkDeviceSize( 1 ), properties.limits.minUniformBufferOffsetAlignment );
		m_unStorageOffsetAlignment = ( std::max )( VkDeviceSize( 1 ), properties.limits.minStorageBufferOffsetAlignment );
	}

	CRenderInfo::~CRenderInfo() 
	{
		vkDeviceWaitIdle( m_device );

		for ( auto &renderable : vecRenderables )
		{
			if ( renderable )
				delete renderable;
		}

		m_vecRetiredBuffers.clear();
		pSceneLightingBuffer.reset();

		if ( pDescriptors )
			delete pDescriptors;

		for ( auto &pipeline : vecGraphicsPipelines )
		{
			if ( pipeline != VK_NULL_HANDLE )
				vkDestroyPipeline( m_device, pipeline, nullptr );
		}

		for ( auto &layout : vecPipelineLayouts )
		{
			if ( layout != VK_NULL_HANDLE )
				vkDestroyPipelineLayout( m_device, layout, nullptr );
		}
	}

	uint16_t CRenderInfo::AddNewLayout( VkPipelineLayout layout ) 
	{ 
		vecPipelineLayouts.push_back( layout );
		return static_cast<uint16_t> ( vecPipelineLayouts.size() - 1 ); 
	}

	uint16_t CRenderInfo::AddNewPipeline( VkPipeline pipeline ) 
	{ 
		vecGraphicsPipelines.push_back( pipeline );
		return static_cast<uint16_t> ( vecGraphicsPipelines.size() - 1 ); 
	}

	uint32_t CRenderInfo::AddNewRenderable( CRenderable *renderable ) 
	{ 
		vecRenderables.push_back( renderable );
		return vecRenderables.size() - 1;
	}

	VkResult CRenderInfo::CreateFrameBuffer( std::unique_ptr< CDeviceBuffer > &outBuffer, VkBufferUsageFlags usageFlags, VkDeviceSize unStride, const void *pData, VkDeviceSize unSize )
	{
		if ( unStride == 0 || unSize > unStride )
			return VK_ERROR_INITIALIZATION_FAILED;

		outBuffer = std::make_unique< CDeviceBuffer >( m_pSession );
		VK_CHECK_RETURN( outBuffer->Init( usageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, unStride * m_unFramesInFlight ) );
		VK_CHECK_RETURN( outBuffer->MapMemory() );

		auto *mapped = static_cast< uint8_t * >( outBuffer->GetMappedData() );
		std::memset( mapped, 0, static_cast< size_t >( unStride * m_unFramesInFlight ) );
		if ( pData && unSize )
		{
			for ( uint32_t i = 0; i < m_unFramesInFlight; ++i )
				std::memcpy( mapped + unStride * i, pData, static_cast< size_t >( unSize ) );
		}

		return VK_SUCCESS;
	}

	void CRenderInfo::RetireBuffer( std::unique_ptr< CDeviceBuffer > pBuffer )
	{
		if ( pBuffer )
			m_vecRetiredBuffers.push_back( { ( 1u << m_unFramesInFlight ) - 1, std::move( pBuffer ) } );
	}

	VkResult CRenderInfo::UpdateFrameBuffers( uint32_t unFrameIndex )
	{
		if ( unFrameIndex >= m_unFramesInFlight )
			return VK_ERROR_INITIALIZATION_FAILED;

		state.unFrameIndex = unFrameIndex;
		const uint32_t frameBit = 1u << unFrameIndex;

		// This frame's earlier work is done, so buffers retired before it are free once every frame has passed
		for ( auto &retired : m_vecRetiredBuffers )
			retired.flgFrames &= ~frameBit;
		std::erase_if( m_vecRetiredBuffers, []( const SRetiredBuffer &retired ) { return retired.flgFrames == 0; } );

		if ( !pSceneLighting )
			return VK_SUCCESS;

		if ( m_flgDirtyEnvironmentFrames & frameBit )
		{
			const auto &images = m_pEnvironment->GetDescriptors();
			std::vector< VkWriteDescriptorSet > writes( images.size(), VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } );
			for ( uint32_t i = 0; i < images.size(); ++i )
			{
				writes[ i ].dstSet = vecSceneLightingDescriptors[ unFrameIndex ];
				writes[ i ].dstBinding = i + 1;
				writes[ i ].descriptorCount = 1;
				writes[ i ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writes[ i ].pImageInfo = &images[ i ];
			}
			vkUpdateDescriptorSets( m_device, uint32_t( writes.size() ), writes.data(), 0, nullptr );

			m_flgDirtyEnvironmentFrames &= ~frameBit;
			if ( !m_flgDirtyEnvironmentFrames )
				m_vecRetiredEnvironments.clear();
		}

		std::memcpy( static_cast< uint8_t * >( pSceneLightingBuffer->GetMappedData() ) + m_unSceneLightingStride * unFrameIndex, pSceneLighting, sizeof( SSceneLighting ) );
		return VK_SUCCESS;
	}

	VkResult CRenderInfo::SetEnvironment( std::shared_ptr< CEnvironmentLighting > environment, float intensity, float rotation )
	{
		if ( !pSceneLighting || vecSceneLightingDescriptors.empty() || !std::isfinite( intensity ) || intensity < 0.f || !std::isfinite( rotation ) )
			return VK_ERROR_INITIALIZATION_FAILED;
		auto selected = environment ? environment : m_pDefaultEnvironment;
		if ( !selected || !selected->IsReady() || selected->GetDevice() != m_device )
			return VK_ERROR_INITIALIZATION_FAILED;

		pSceneLighting->environmentIntensity = environment ? intensity : 0.f;
		pSceneLighting->environmentRotation = rotation;
		pSceneLighting->environmentMaxLod = selected->GetMaxLod();
		if ( selected == m_pEnvironment )
			return VK_SUCCESS;

		// Frames in flight may still sample the current images, so each frame rebinds in UpdateFrameBuffers
		if ( m_pEnvironment )
			m_vecRetiredEnvironments.push_back( std::move( m_pEnvironment ) );
		m_pEnvironment = std::move( selected );
		m_flgDirtyEnvironmentFrames = ( 1u << m_unFramesInFlight ) - 1;
		return VK_SUCCESS;
	}

	void CRenderInfo::SetupSceneLighting() 
	{
		assert( pDescriptors );

		m_unSceneLightingStride = GetUniformStride( sizeof( SSceneLighting ) );
		VK_CHECK_RESULT( CreateFrameBuffer( pSceneLightingBuffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_unSceneLightingStride ) );

		// Initialize the complete GPU block, including padding and unused lights
		m_sceneLighting = {};
		pSceneLighting = &m_sceneLighting;

		// Set default lighting
		pSceneLighting->mainLight.direction = { 0.0f, -1.0f, 0.0f };
		pSceneLighting->mainLight.intensity = 0.5f;
		pSceneLighting->mainLight.color = { 1.0f, 0.98f, 0.95f };

		pSceneLighting->ambientColor = { 1.0f, 1.0f, 1.0f };
		pSceneLighting->ambientIntensity = 0.5f;

		pSceneLighting->activePointLights = 0;
		pSceneLighting->activeSpotLights = 0;

		pSceneLighting->tonemapping = { .exposure = 1.0f, .gamma = 2.2f, .tonemap = 0, .contrast = 1.0f, .saturation = 1.0f };
		pSceneLighting->tonemapping.setRenderMode( ERenderMode::PBR );
		pSceneLighting->tonemapping.setTonemapOperator( ETonemapOperator::Uncharted2 );

		// Create a scene lighting descriptor set for each frame in flight
		VkResult result = pDescriptors->CreateDescriptorSets(
			vecSceneLightingDescriptors,
			lightingLayoutId,
			lightingPoolId,
			m_unFramesInFlight );

		assert( result == VK_SUCCESS && vecSceneLightingDescriptors.size() == m_unFramesInFlight );

		// Valid fallback images are required even while the shader's IBL branch is disabled
		auto *pVulkan = m_pSession->GetVulkan();
		VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.queueFamilyIndex = pVulkan->GetVkQueueIndex_GraphicsFamily();
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkCommandPool pool = VK_NULL_HANDLE;
		VK_CHECK_RESULT( vkCreateCommandPool( m_device, &poolInfo, nullptr, &pool ) );
		m_pDefaultEnvironment = std::make_shared< CEnvironmentLighting >();
		try
		{
			result = m_pDefaultEnvironment->Init( m_device, pVulkan->GetVkPhysicalDevice(), pool, pVulkan->GetVkQueue_Graphics(), CEnvironmentLighting::DisabledData() );
		}
		catch ( ... )
		{
			vkDestroyCommandPool( m_device, pool, nullptr );
			throw;
		}
		vkDestroyCommandPool( m_device, pool, nullptr );
		VK_CHECK_RESULT( result );
		VK_CHECK_RESULT( SetEnvironment( nullptr ) );

		// Point each frame's descriptor to that frame's section of the buffer
		for ( uint32_t i = 0; i < m_unFramesInFlight; ++i )
		{
			std::vector< VkDescriptorSet > descriptors = { vecSceneLightingDescriptors[ i ] };
			pDescriptors->UpdateUniformBuffer(
				descriptors,
				0, // binding = 0 in fragment shader (set 1)
				pSceneLightingBuffer->GetVkBuffer(),
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				m_unSceneLightingStride * i,
				sizeof( SSceneLighting ) );
		}
	}

} // namespace xrlib
