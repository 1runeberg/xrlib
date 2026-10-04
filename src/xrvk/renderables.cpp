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
			pipelineLayoutIndex( pipelineLayoutIdx ), 
			graphicsPipelineIndex( graphicsPipelineIdx ), 
			descriptorLayoutIndex( descriptorLayoutIdx ),
			isVisible( bIsVisible )
	{
		assert( pSession );

		// Fill descriptors (if any)
		if ( descriptorLayoutIdx < ( std::numeric_limits< uint32_t >::max )() )
		{
			assert( pRenderInfo );
			vertexDescriptors = pRenderInfo->pDescriptors->GetDescriptorSets( descriptorLayoutIdx );
		}

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

		auto pBuffer = std::make_unique< CDeviceBuffer >( m_pSession );
		const VkResult result = InitBuffer( pBuffer.get(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof( XrMatrix4x4f ) * newMatrices.size(), newMatrices.data() );
		if ( result != VK_SUCCESS )
			throw std::runtime_error( "Failed to grow instance buffer" );

		delete m_pInstanceBuffer;
		m_pInstanceBuffer = pBuffer.release();
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

	VkResult CRenderable::UpdateInstancesBuffer()
	{
		if ( instanceMatrices.empty() )
			return VK_SUCCESS;

		const VkDeviceSize size = instanceMatrices.size() * sizeof( XrMatrix4x4f );
		if ( !m_pInstanceBuffer || size > m_pInstanceBuffer->GetBufferSize() )
			return VK_ERROR_INITIALIZATION_FAILED;

		const VkResult result = m_pInstanceBuffer->MapMemory();
		if ( result != VK_SUCCESS )
			return result;

		memcpy( m_pInstanceBuffer->GetMappedData(), instanceMatrices.data(), size );
		if ( !( m_pInstanceBuffer->GetMemoryPropertyFlags() & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) )
			return m_pInstanceBuffer->FlushMemory();

		return VK_SUCCESS;
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

	CRenderInfo::CRenderInfo( CSession *pSession ) 
	{ 
		assert( pSession );

		m_pSession = pSession;
		m_device = pSession->GetVulkan()->GetVkLogicalDevice();
		pDescriptors = new CDescriptorManager( pSession );
	}

	CRenderInfo::~CRenderInfo() 
	{
		vkDeviceWaitIdle( m_device );

		if ( pSceneLightingBuffer )
			delete pSceneLightingBuffer;

		if ( pDescriptors )
			delete pDescriptors;

		for ( auto &renderable : vecRenderables )
		{
			if ( renderable )
				delete renderable;
		}

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

	VkResult CRenderInfo::SetEnvironment( std::shared_ptr< CEnvironmentLighting > environment, float intensity, float rotation )
	{
		if ( !pSceneLighting || !sceneLightingDescriptor || !std::isfinite( intensity ) || intensity < 0.f || !std::isfinite( rotation ) )
			return VK_ERROR_INITIALIZATION_FAILED;
		auto selected = environment ? environment : m_pDefaultEnvironment;
		if ( !selected || !selected->IsReady() || selected->GetDevice() != m_device )
			return VK_ERROR_INITIALIZATION_FAILED;

		const auto &images = selected->GetDescriptors();
		std::array< VkWriteDescriptorSet, 3 > writes {};
		for ( uint32_t i = 0; i < writes.size(); ++i )
		{
			writes[ i ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[ i ].dstSet = sceneLightingDescriptor;
			writes[ i ].dstBinding = i + 1;
			writes[ i ].descriptorCount = 1;
			writes[ i ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[ i ].pImageInfo = &images[ i ];
		}
		vkUpdateDescriptorSets( m_device, uint32_t( writes.size() ), writes.data(), 0, nullptr );
		pSceneLighting->environmentIntensity = environment ? intensity : 0.f;
		pSceneLighting->environmentRotation = rotation;
		pSceneLighting->environmentMaxLod = selected->GetMaxLod();
		m_pEnvironment = std::move( selected );
		return VK_SUCCESS;
	}

	void CRenderInfo::SetupSceneLighting() 
	{
		assert( pDescriptors );

		// Create buffer for scene lighting
		pSceneLightingBuffer = pDescriptors->CreateBuffer( 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			sizeof( SSceneLighting ) );

		assert( pSceneLightingBuffer );

		// Map the buffer immediately and keep it mapped
		if ( pSceneLightingBuffer->MapMemory() == VK_SUCCESS )
		{
			pSceneLighting = static_cast< SSceneLighting * >( pSceneLightingBuffer->GetMappedData() );
		}

		// Initialize the complete GPU block, including padding and unused lights.
		if ( !pSceneLighting )
			throw std::runtime_error( "Failed to map scene lighting buffer" );
		new ( pSceneLighting ) SSceneLighting {};

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

		// Create descriptor set for scene lighting
		std::vector< VkDescriptorSet > sceneLightingSets;
		VkResult result = pDescriptors->CreateDescriptorSets(
			sceneLightingSets,
			lightingLayoutId,
			lightingPoolId,
			1 // Single set
		);

		assert( result == VK_SUCCESS && sceneLightingSets.size() == 1 );
		sceneLightingDescriptor = sceneLightingSets[ 0 ];

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

		// Update descriptor to point to the buffer
		std::vector< VkDescriptorSet > descriptors = { sceneLightingDescriptor };
		pDescriptors->UpdateUniformBuffer(
			descriptors,
			0, // binding = 0 in fragment shader (set 1)
			pSceneLightingBuffer->GetVkBuffer(),
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			sizeof( SSceneLighting ) );
	}

} // namespace xrlib
