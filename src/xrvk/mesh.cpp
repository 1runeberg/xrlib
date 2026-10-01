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


#include <xrvk/mesh.hpp>
#include <xrvk/animation.hpp>

namespace xrlib
{
	static bool IsMirrored( const XrMatrix4x4f &matrix )
	{
		const auto &m = matrix.m;
		return m[ 0 ] * ( m[ 5 ] * m[ 10 ] - m[ 6 ] * m[ 9 ] ) - m[ 4 ] * ( m[ 1 ] * m[ 10 ] - m[ 2 ] * m[ 9 ] ) + m[ 8 ] * ( m[ 1 ] * m[ 6 ] - m[ 2 ] * m[ 5 ] ) < 0.f;
	}

	void SSkin::UpdateMatrices( const std::vector< XrQuaternionf > &orientation, const std::vector< XrVector3f > &position, XrVector3f scale )
	{
		if ( orientation.size() != joints.size() || position.size() != joints.size() )
			throw std::invalid_argument( "Joint pose count doesn't match skin" );

		std::vector< XrMatrix4x4f > localMatrices( joints.size() );
		for ( size_t i = 0; i < joints.size(); ++i )
			XrMatrix4x4f_CreateTranslationRotationScale( &localMatrices[ i ], &position[ i ], &orientation[ i ], &scale );

		UpdateMatrices( localMatrices.data() );
	}

	void SSkin::UpdateMatrices( const std::vector< XrPosef > &newJointPoses, XrVector3f scale )
	{
		std::vector< XrQuaternionf > orientation;
		std::vector< XrVector3f > position;
		for ( const auto &pose : newJointPoses )
		{
			orientation.push_back( pose.orientation );
			position.push_back( pose.position );
		}

		UpdateMatrices( orientation, position, scale );
	}

	void SSkin::UpdateMatrices( XrMatrix4x4f *localMatrices )
	{
		if ( !localMatrices && !joints.empty() )
			throw std::invalid_argument( "Missing joint matrices" );

		std::vector< bool > isChild( joints.size(), false );
		for ( const auto &[parent, children] : hierarchy )
		{
			if ( parent >= joints.size() )
				throw std::out_of_range( "Invalid parent joint" );

			for ( uint32_t child : children )
			{
				if ( child >= joints.size() || isChild[ child ] )
					throw std::invalid_argument( "Invalid joint hierarchy" );

				isChild[ child ] = true;
			}
		}

		matrices.resize( joints.size() );
		std::vector< bool > visited( joints.size(), false );
		std::function< void( uint32_t, const XrMatrix4x4f & ) > UpdateJoint;
		UpdateJoint = [&]( uint32_t joint, const XrMatrix4x4f &parent ) {
			if ( visited[ joint ] )
				throw std::invalid_argument( "Cyclic joint hierarchy" );

			visited[ joint ] = true;
			XrMatrix4x4f world;
			XrMatrix4x4f_Multiply( &world, &parent, &localMatrices[ joint ] );
			if ( joint < inverseBindMatrices.size() )
				XrMatrix4x4f_Multiply( &matrices[ joint ], &world, &inverseBindMatrices[ joint ] );
			else
				matrices[ joint ] = world;

			auto children = hierarchy.find( joint );
			if ( children != hierarchy.end() )
				for ( uint32_t child : children->second )
					UpdateJoint( child, world );
		};

		XrMatrix4x4f identity;
		XrMatrix4x4f_CreateIdentity( &identity );
		for ( size_t i = 0; i < joints.size(); ++i )
			if ( !isChild[ i ] )
				UpdateJoint( static_cast< uint32_t >( i ), identity );

		if ( std::find( visited.begin(), visited.end(), false ) != visited.end() )
			throw std::invalid_argument( "Cyclic joint hierarchy" );
	}

	CRenderModel::CRenderModel( 
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: CRenderable( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, xrScale, xrSpace )
	{
	}

	CRenderModel::CRenderModel( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace ) : 
		CRenderable( pSession, pRenderInfo, 0, 0, ( std::numeric_limits< uint32_t >::max )(), bIsVisible, xrScale, xrSpace )
	{
	}

	CRenderModel::~CRenderModel() 
	{ 
		if ( m_vkSkinningPool )
			vkDestroyDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), m_vkSkinningPool, nullptr );
		Reset();
		DeleteBuffers();
	}

	VkResult CRenderModel::InitBuffers( bool bReset )
	{

		// Initialize vertex buffer
		if ( vertices.size() > 0 )
		{
			if ( m_pVertexBuffer )
				delete m_pVertexBuffer;

			m_pVertexBuffer = new CDeviceBuffer( m_pSession );
			m_unBufferedVertexCount = 0;
			VkResult result = InitBuffer( m_pVertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( SMeshVertex ) * vertices.size(), vertices.data() );
			if ( result != VK_SUCCESS )
				return result;
		}

		m_unBufferedVertexCount = vertices.size();

		// Initialize index buffer
		if ( indices.size() > 0 )
		{
			if ( m_pIndexBuffer )
				delete m_pIndexBuffer;

			m_pIndexBuffer = new CDeviceBuffer( m_pSession );
			VkResult result = InitBuffer( m_pIndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof( uint32_t ) * indices.size(), indices.data() );
			if ( result != VK_SUCCESS )
				return result;
		}

		// Initialize instance buffer
		if ( instanceMatrices.size() > 0 )
		{
			if ( m_pInstanceBuffer )
				delete m_pInstanceBuffer;

			m_pInstanceBuffer = new CDeviceBuffer( m_pSession );
			VkResult result = InitBuffer( m_pInstanceBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof( XrMatrix4x4f ) * instanceMatrices.size(), instanceMatrices.data() );
			if ( result != VK_SUCCESS )
				return result;
		}

		UpdateSectionBounds();

		// Reset if requested
		if ( bReset )
			Reset();

		return VK_SUCCESS;
	}

	VkResult CRenderModel::InitSkinning( VkDescriptorSetLayout layout, const XrMatrix4x4f &modelFromAsset )
	{
		if ( !pAnimation || !layout || m_pSkinningBuffer || m_vkSkinningPool )
			return VK_ERROR_INITIALIZATION_FAILED;

		m_modelFromAsset = modelFromAsset;
		pAnimation->GetSkinningVertices( vertices );
		pAnimation->GetSkinningMatrices( m_vecSkinningMatrices );
		const auto &morphVertices = pAnimation->GetMorphVertices();
		const auto &morphDeltas = pAnimation->GetMorphDeltas();
		const auto &morphWeights = pAnimation->GetMorphWeights();
		const VkDeviceSize size = sizeof( XrMatrix4x4f ) * ( 1 + m_vecSkinningMatrices.size() );
		const VkDeviceSize vertexSize = sizeof( SMorphVertex ) * ( 1 + morphVertices.size() );
		const VkDeviceSize deltaSize = sizeof( SMorphDelta ) * std::max( size_t { 1 }, morphDeltas.size() );
		const VkDeviceSize weightSize = sizeof( float ) * std::max( size_t { 1 }, morphWeights.size() );
		auto *pVulkan = m_pSession->GetVulkan();
		const VkDevice device = pVulkan->GetVkLogicalDevice();
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties( pVulkan->GetVkPhysicalDevice(), &properties );
		if ( std::max( { size, vertexSize, deltaSize, weightSize } ) > properties.limits.maxStorageBufferRange || properties.limits.maxPerStageDescriptorStorageBuffers < 4 || properties.limits.maxDescriptorSetStorageBuffers < 4 )
			return VK_ERROR_FEATURE_NOT_PRESENT;

		m_pSkinningBuffer = std::make_unique< CDeviceBuffer >( m_pSession );
		VK_CHECK_RETURN( m_pSkinningBuffer->Init( VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size ) );
		VK_CHECK_RETURN( m_pSkinningBuffer->MapMemory() );
		std::memcpy( m_pSkinningBuffer->GetMappedData(), &modelFromAsset, sizeof( modelFromAsset ) );

		auto InitMorphBuffer = [ & ]( std::unique_ptr< CDeviceBuffer > &buffer, VkDeviceSize bytes )
		{
			buffer = std::make_unique< CDeviceBuffer >( m_pSession );
			VK_CHECK_RETURN( buffer->Init( VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bytes ) );
			VK_CHECK_RETURN( buffer->MapMemory() );
			std::memset( buffer->GetMappedData(), 0, static_cast< size_t >( bytes ) );
			return VK_SUCCESS;
		};
		VK_CHECK_RETURN( InitMorphBuffer( m_pMorphVertexBuffer, vertexSize ) );
		VK_CHECK_RETURN( InitMorphBuffer( m_pMorphDeltaBuffer, deltaSize ) );
		VK_CHECK_RETURN( InitMorphBuffer( m_pMorphWeightBuffer, weightSize ) );

		// The header lets models without morphs bind small, valid dummy buffers
		const SMorphVertex header { static_cast< uint32_t >( morphVertices.size() ), 0, 0, 0 };
		auto *pVertices = static_cast< uint8_t * >( m_pMorphVertexBuffer->GetMappedData() );
		std::memcpy( pVertices, &header, sizeof( header ) );
		if ( !morphVertices.empty() )
			std::memcpy( pVertices + sizeof( header ), morphVertices.data(), morphVertices.size() * sizeof( SMorphVertex ) );
		if ( !morphDeltas.empty() )
			std::memcpy( m_pMorphDeltaBuffer->GetMappedData(), morphDeltas.data(), morphDeltas.size() * sizeof( SMorphDelta ) );

		m_unMorphWeightCount = morphWeights.size();
		VK_CHECK_RETURN( UpdateSkinning() );

		VkDescriptorPoolSize poolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 };
		VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.maxSets = 1;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		VK_CHECK_RETURN( vkCreateDescriptorPool( device, &poolInfo, nullptr, &m_vkSkinningPool ) );

		VkDescriptorSetAllocateInfo allocation { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocation.descriptorPool = m_vkSkinningPool;
		allocation.descriptorSetCount = 1;
		allocation.pSetLayouts = &layout;
		VK_CHECK_RETURN( vkAllocateDescriptorSets( device, &allocation, &m_vkSkinningSet ) );

		const std::vector< VkDescriptorBufferInfo > buffers {
			{ m_pSkinningBuffer->GetVkBuffer(), 0, size }, { m_pMorphVertexBuffer->GetVkBuffer(), 0, vertexSize }, { m_pMorphDeltaBuffer->GetVkBuffer(), 0, deltaSize }, { m_pMorphWeightBuffer->GetVkBuffer(), 0, weightSize } };
		std::vector< VkWriteDescriptorSet > descriptors( buffers.size(), { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } );
		for ( uint32_t i = 0; i < descriptors.size(); ++i )
		{
			auto &descriptor = descriptors[ i ];
			descriptor.dstSet = m_vkSkinningSet;
			descriptor.dstBinding = i;
			descriptor.descriptorCount = 1;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptor.pBufferInfo = &buffers[ i ];
		}

		vkUpdateDescriptorSets( device, static_cast< uint32_t >( descriptors.size() ), descriptors.data(), 0, nullptr );
		return VK_SUCCESS;
	}

	VkResult CRenderModel::UpdateSkinning()
	{
		if ( !pAnimation || !m_pSkinningBuffer || !m_pMorphWeightBuffer )
			return VK_ERROR_INITIALIZATION_FAILED;

		const auto count = m_vecSkinningMatrices.size();
		pAnimation->GetSkinningMatrices( m_vecSkinningMatrices );
		if ( m_vecSkinningMatrices.size() != count || pAnimation->GetMorphWeights().size() != m_unMorphWeightCount )
			return VK_ERROR_INITIALIZATION_FAILED;

		auto *pData = static_cast< uint8_t * >( m_pSkinningBuffer->GetMappedData() );
		if ( !pData || !m_pMorphWeightBuffer->GetMappedData() )
			return VK_ERROR_MEMORY_MAP_FAILED;

		if ( m_unMorphWeightCount )
			std::memcpy( m_pMorphWeightBuffer->GetMappedData(), pAnimation->GetMorphWeights().data(), m_unMorphWeightCount * sizeof( float ) );

		std::memcpy( pData + sizeof( XrMatrix4x4f ), m_vecSkinningMatrices.data(), count * sizeof( XrMatrix4x4f ) );
		return VK_SUCCESS;
	}

	VkResult CRenderModel::UpdateVertexBuffer()
	{
		if ( !m_pVertexBuffer || vertices.size() != m_unBufferedVertexCount )
			return VK_ERROR_INITIALIZATION_FAILED;

		const VkResult result = m_pVertexBuffer->MapMemory();
		if ( result != VK_SUCCESS )
			return result;

		memcpy( m_pVertexBuffer->GetMappedData(), vertices.data(), sizeof( SMeshVertex ) * vertices.size() );
		UpdateSectionBounds();
		return VK_SUCCESS;
	}

	void CRenderModel::UpdateSectionBounds()
	{
		m_vecSectionBounds.assign( materialSections.size(), {} );
		for ( size_t i = 0; i < materialSections.size(); ++i )
		{
			const auto &section = materialSections[ i ];
			auto &bounds = m_vecSectionBounds[ i ];
			if ( !section.indexCount )
				continue;
			bounds.lower = { FLT_MAX, FLT_MAX, FLT_MAX };
			bounds.upper = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for ( uint32_t index = 0; index < section.indexCount; ++index )
			{
				const auto vertexIndex = indices.at( section.firstIndex + index );
				const auto &vertex = vertices.at( vertexIndex );
				const auto &position = vertex.position;
				bounds.lower = { std::min( bounds.lower.x, position.x ), std::min( bounds.lower.y, position.y ), std::min( bounds.lower.z, position.z ) };
				bounds.upper = { std::max( bounds.upper.x, position.x ), std::max( bounds.upper.y, position.y ), std::max( bounds.upper.z, position.z ) };
				if ( !m_pSkinningBuffer )
					continue;

				for ( uint32_t joint = 0; joint < JOINT_INFLUENCE_COUNT; ++joint )
					if ( vertex.weights[ joint ] > 0.f && std::find( bounds.joints.begin(), bounds.joints.end(), vertex.joints[ joint ] ) == bounds.joints.end() )
						bounds.joints.push_back( vertex.joints[ joint ] );

				const auto &morphVertices = pAnimation->GetMorphVertices();
				if ( vertexIndex >= morphVertices.size() )
					continue;
				const auto &morph = morphVertices[ vertexIndex ];
				for ( uint32_t target = 0; target < morph.targetCount; ++target )
				{
					const auto &delta = pAnimation->GetMorphDeltas().at( morph.firstDelta + target * morph.targetStride ).position;
					const uint32_t weight = morph.firstWeight + target;
					auto entry = std::find_if( bounds.morphs.begin(), bounds.morphs.end(), [ & ]( const auto &value ) { return value.weight == weight; } );
					if ( entry == bounds.morphs.end() )
					{
						bounds.morphs.push_back( { weight } );
						entry = std::prev( bounds.morphs.end() );
					}
					entry->lower = { std::min( entry->lower.x, delta.x ), std::min( entry->lower.y, delta.y ), std::min( entry->lower.z, delta.z ) };
					entry->upper = { std::max( entry->upper.x, delta.x ), std::max( entry->upper.y, delta.y ), std::max( entry->upper.z, delta.z ) };
				}
			}
		}
	}

	XrVector3f CRenderModel::GetSectionCenter( uint32_t unSection ) const
	{
		const auto &bounds = m_vecSectionBounds.at( unSection );
		auto lower = bounds.lower, upper = bounds.upper;

		// Animate conservative boxes, vertex deformation stays in the GPU shader
		for ( const auto &morph : bounds.morphs )
		{
			const float weight = pAnimation->GetMorphWeights().at( morph.weight );
			lower.x += std::min( morph.lower.x * weight, morph.upper.x * weight );
			lower.y += std::min( morph.lower.y * weight, morph.upper.y * weight );
			lower.z += std::min( morph.lower.z * weight, morph.upper.z * weight );
			upper.x += std::max( morph.lower.x * weight, morph.upper.x * weight );
			upper.y += std::max( morph.lower.y * weight, morph.upper.y * weight );
			upper.z += std::max( morph.lower.z * weight, morph.upper.z * weight );
		}
		if ( !bounds.joints.empty() )
		{
			XrVector3f animatedLower { FLT_MAX, FLT_MAX, FLT_MAX }, animatedUpper { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for ( const auto joint : bounds.joints )
			{
				XrMatrix4x4f transform;
				XrMatrix4x4f_Multiply( &transform, &m_modelFromAsset, &m_vecSkinningMatrices.at( joint ) );
				for ( uint32_t corner = 0; corner < 8; ++corner )
				{
					const XrVector3f local { corner & 1 ? upper.x : lower.x, corner & 2 ? upper.y : lower.y, corner & 4 ? upper.z : lower.z };
					XrVector3f world;
					XrMatrix4x4f_TransformVector3f( &world, &transform, &local );
					animatedLower = { std::min( animatedLower.x, world.x ), std::min( animatedLower.y, world.y ), std::min( animatedLower.z, world.z ) };
					animatedUpper = { std::max( animatedUpper.x, world.x ), std::max( animatedUpper.y, world.y ), std::max( animatedUpper.z, world.z ) };
				}
			}
			lower = animatedLower;
			upper = animatedUpper;
		}
		return { ( lower.x + upper.x ) * .5f, ( lower.y + upper.y ) * .5f, ( lower.z + upper.z ) * .5f };
	}

	void CRenderModel::CollectDraws( std::vector< SMaterialDraw > &outDraws, const CRenderInfo &renderInfo )
	{
		if ( instances.empty() )
			return;

		const bool sharedWinding = std::all_of( instanceMatrices.begin(), instanceMatrices.end(), [ & ]( const auto &matrix ) { return IsMirrored( matrix ) == IsMirrored( instanceMatrices.front() ); } );

		for ( uint32_t sectionIndex = 0; sectionIndex < materialSections.size(); ++sectionIndex )
		{
			const auto &section = materialSections[ sectionIndex ];
			const bool blend = materials.at( section.materialIndex ).getAlphaMode() == EAlphaMode::Blend;
			// Keep opaque hardware instances together when they share the same winding
			if ( !blend && sharedWinding )
			{
				outDraws.push_back( { this, sectionIndex, UINT32_MAX, 0.f, false } );
				continue;
			}

			const XrVector3f center = GetSectionCenter( sectionIndex );

			for ( uint32_t instance = 0; instance < GetInstanceCount(); ++instance )
			{
				XrVector3f world, left, right;
				XrMatrix4x4f_TransformVector3f( &world, &instanceMatrices[ instance ], &center );
				XrMatrix4x4f_TransformVector3f( &left, &renderInfo.state.eyeViewMatrices[ 0 ], &world );
				XrMatrix4x4f_TransformVector3f( &right, &renderInfo.state.eyeViewMatrices[ 1 ], &world );
				outDraws.push_back( { this, sectionIndex, instance, -( left.z + right.z ) * .5f, blend } );
			}
		}
	}

	void CRenderModel::Draw( VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo )
	{
		if ( !renderInfo.materialPipelines.contains( graphicsPipelineIndex ) || materialSections.empty() )
		{
			DrawSection( commandBuffer, renderInfo, UINT32_MAX, UINT32_MAX );
			return;
		}

		std::vector< SMaterialDraw > draws;
		CollectDraws( draws, renderInfo );
		std::stable_sort( draws.begin(), draws.end(), []( const auto &a, const auto &b ) { return a.blend != b.blend ? !a.blend : ( a.blend && a.depth > b.depth ); } );
		for ( const auto &draw : draws )
			DrawSection( commandBuffer, renderInfo, draw.section, draw.instance );
	}

	void CRenderModel::DrawSection( VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo, uint32_t unSection, uint32_t unInstance )
	{
		// Set push constants
		vkCmdPushConstants( 
			commandBuffer, 
			renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ], 
			VK_SHADER_STAGE_VERTEX_BIT, 
			0, 
			k_pcrSize, 
			renderInfo.state.eyeVPs.data() );

		// Set stencil reference
		vkCmdSetStencilReference( commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1 );

		// Custom pipelines retain their caller-supplied state
		uint32_t pipeline = graphicsPipelineIndex;
		const auto variants = renderInfo.materialPipelines.find( pipeline );
		if ( variants != renderInfo.materialPipelines.end() && unSection < materialSections.size() )
		{
			const auto &material = materials.at( materialSections[ unSection ].materialIndex );
			const bool mirrored = IsMirrored( instanceMatrices.at( unInstance == UINT32_MAX ? 0 : unInstance ) );
			const uint32_t variant = ( material.getAlphaMode() == EAlphaMode::Blend ? 1 : 0 ) | ( material.doubleSided ? 2 : 0 ) | ( mirrored ? 4 : 0 );
			pipeline = variants->second[ variant ];
		}
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderInfo.vecGraphicsPipelines[ pipeline ] );
		const uint32_t instanceCount = unInstance == UINT32_MAX ? GetInstanceCount() : 1;
		const uint32_t firstInstance = unInstance == UINT32_MAX ? 0 : unInstance;

		// Bind shape's index and vertex buffers
		vkCmdBindIndexBuffer( commandBuffer, GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT32 );
		vkCmdBindVertexBuffers( commandBuffer, 0, 1, GetVertexBuffer()->GetVkBufferPtr(), vertexOffsets );
		vkCmdBindVertexBuffers( commandBuffer, 1, 1, GetInstanceBuffer()->GetVkBufferPtr(), instanceOffsets );

		// Bind vertex descriptors
		if ( !vertexDescriptors.empty() )
		{
			vkCmdBindDescriptorSets( 
				commandBuffer, 
				VK_PIPELINE_BIND_POINT_GRAPHICS, 
				renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ], 
				0, // should match set = x in shader
				vertexDescriptors.size(), 
				vertexDescriptors.data(), 
				0, nullptr ); // dynamic offsets not supported
		}

		if ( m_vkSkinningSet )
			vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ], 2, 1, &m_vkSkinningSet, 0, nullptr );

		// Bind environment lighting
		if ( renderInfo.pSceneLighting )
		{
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ],
				1, // set 1 in pbr fragment shader
				1, // one set
				&renderInfo.sceneLightingDescriptor,
				0, // no dynamic offset needed
				nullptr );
		}

		// Draw, bind material descriptors if present
		if ( materialSections.empty() )
		{
			// Draw indexed - no material
			vkCmdDrawIndexed( commandBuffer, indices.size(), instanceCount, 0, 0, firstInstance );
		}
		else
		{
			// Draw indexed - draw per mesh's material sections
			for ( uint32_t i = 0; i < materialSections.size(); ++i )
			{
				if ( unSection != UINT32_MAX && unSection != i )
					continue;
				const auto &section = materialSections[ i ];
				if ( materials[ section.materialIndex ].descriptors.empty() )
				{
					vkCmdDrawIndexed( commandBuffer, section.indexCount, instanceCount, section.firstIndex, 0, firstInstance );
					continue;
				}

				vkCmdBindDescriptorSets(
					commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ],
					0, // Set 0 in fragment shader
					materials[ section.materialIndex ].descriptors.size(),
					materials[ section.materialIndex ].descriptors.data(),
					0,
					nullptr ); // dynamic offsets not supported

				vkCmdDrawIndexed( commandBuffer, section.indexCount, instanceCount, section.firstIndex, 0, firstInstance );
			}
		}

	}

	uint32_t CRenderModel::LoadMaterial( CRenderInfo *pRenderInfo, uint32_t layoutId, uint32_t poolId, CTextureManager *pTextureManager )
	{
		std::vector< SMaterialUBO * > materialData;
		const auto count = LoadMaterial( materialData, pRenderInfo, layoutId, poolId, pTextureManager );
		if ( pFragmentDescriptorsBuffer )
			pFragmentDescriptorsBuffer->UnmapMemory();
		return count;
	}

	uint32_t CRenderModel::LoadMaterial( std::vector< SMaterialUBO * > &outMaterialData, CRenderInfo *pRenderInfo, uint32_t layoutId, uint32_t poolId, CTextureManager *pTextureManager )
	{
		if ( materials.empty() )
			return 0;

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties( m_pSession->GetVulkan()->GetVkPhysicalDevice(), &properties );
		const VkDeviceSize alignment = ( std::max )( VkDeviceSize( 1 ), properties.limits.minUniformBufferOffsetAlignment );
		const VkDeviceSize stride = ( sizeof( SMaterialUBO ) + alignment - 1 ) / alignment * alignment;
		delete pFragmentDescriptorsBuffer;
		pFragmentDescriptorsBuffer = pRenderInfo->pDescriptors->CreateBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stride * materials.size() );
		if ( !pFragmentDescriptorsBuffer || pFragmentDescriptorsBuffer->MapMemory() != VK_SUCCESS )
			return 0;

		auto *mapped = static_cast< uint8_t * >( pFragmentDescriptorsBuffer->GetMappedData() );
		const STexture *fallback = nullptr;
		for ( size_t i = 0; i < materials.size(); ++i )
		{
			auto &material = materials[ i ];
			if ( pRenderInfo->pDescriptors->CreateDescriptorSets( material.descriptors, layoutId, poolId, 1 ) != VK_SUCCESS )
				return 0;
			const int maps[] = { material.baseColorTexture, material.metallicRoughnessTexture, material.normalTexture, material.emissiveTexture, material.occlusionTexture };
			material.setTextureFlag( TEXTURE_BASE_COLOR_SRGB_BIT, false );
			material.setTextureFlag( TEXTURE_EMISSIVE_SRGB_BIT, false );
			for ( unsigned slot = 0; slot < 5; ++slot )
			{
				const STexture *texture = nullptr;

				// Textures the GPU couldn't create fall back like missing maps
				if ( maps[ slot ] >= 0 && textures.at( maps[ slot ] ).view )
					texture = &textures.at( maps[ slot ] );
				else
				{
					if ( !fallback )
						fallback = &pTextureManager->GetDefaultTexture();
					texture = fallback;
				}
				const bool color = slot == 0 || slot == 3;
				const auto view = color && texture->srgbView ? texture->srgbView : texture->view;
				if ( slot == 0 ) material.setTextureFlag( TEXTURE_BASE_COLOR_SRGB_BIT, texture->srgbView != VK_NULL_HANDLE );
				if ( slot == 3 ) material.setTextureFlag( TEXTURE_EMISSIVE_SRGB_BIT, texture->srgbView != VK_NULL_HANDLE );
				pRenderInfo->pDescriptors->UpdateImageDescriptor( material.descriptors, slot + 1, view, texture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
			}
			material.resetPadding();
			material.updateTextureFlags();
			auto *data = reinterpret_cast< SMaterialUBO * >( mapped + stride * i );
			std::memcpy( data, &material, sizeof( SMaterialUBO ) );
			outMaterialData.push_back( data );
			material.descriptorsBufferIndex = static_cast< uint32_t >( i );
			pRenderInfo->pDescriptors->UpdateUniformBuffer( material.descriptors, 0, pFragmentDescriptorsBuffer->GetVkBuffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stride * i, sizeof( SMaterialUBO ) );
		}
		return static_cast< uint32_t >( materials.size() );
	}


	void CRenderModel::Reset()
	{
		// Clear mesh data
		vertices.clear();
		vertices.shrink_to_fit();

		indices.clear();
		indices.shrink_to_fit();
	}

	void CRenderModel::DeleteBuffers()
	{
		if ( m_pIndexBuffer )
		{
			delete m_pIndexBuffer;
			m_pIndexBuffer = nullptr;
		}

		if ( m_pVertexBuffer )
		{
			delete m_pVertexBuffer;
			m_pVertexBuffer = nullptr;
		}

		if ( m_pInstanceBuffer )
		{
			delete m_pInstanceBuffer;
			m_pInstanceBuffer = nullptr;
		}
	}

} // namespace xrlib
