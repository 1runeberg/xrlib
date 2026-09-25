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


#include <xrvk/mesh.hpp>

namespace xrlib
{
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
		CRenderable( pSession, pRenderInfo, 0, 0, std::numeric_limits< uint32_t >::max(), bIsVisible, xrScale, xrSpace )
	{
	}

	CRenderModel::~CRenderModel() 
	{ 
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
			VkResult result = InitBuffer( m_pVertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( SMeshVertex ) * vertices.size(), vertices.data() );
			if ( result != VK_SUCCESS )
				return result;
		}

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

		// Reset if requested
		if ( bReset )
			Reset();

		return VK_SUCCESS;
	}

	void CRenderModel::Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) 
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

		// Bind the graphics pipeline for this shape
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderInfo.vecGraphicsPipelines[ graphicsPipelineIndex ] );

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
			vkCmdDrawIndexed( commandBuffer, indices.size(), GetInstanceCount(), 0, 0, 0 );
		}
		else
		{
			// Draw indexed - draw per mesh's material sections
			for ( auto &section : materialSections )
			{
				if ( materials[ section.materialIndex ].descriptors.empty() )
				{
					vkCmdDrawIndexed( commandBuffer, section.indexCount, GetInstanceCount(), section.firstIndex, 0, 0 );
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

				vkCmdDrawIndexed( commandBuffer, section.indexCount, GetInstanceCount(), section.firstIndex, 0, 0 );
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
		const VkDeviceSize alignment = std::max( VkDeviceSize( 1 ), properties.limits.minUniformBufferOffsetAlignment );
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
			VK_CHECK_RESULT( pRenderInfo->pDescriptors->CreateDescriptorSets( material.descriptors, poolId, layoutId, 1 ) );
			const int maps[] = { material.baseColorTexture, material.metallicRoughnessTexture, material.normalTexture, material.emissiveTexture, material.occlusionTexture };
			material.setTextureFlag( TEXTURE_BASE_COLOR_SRGB_BIT, false );
			material.setTextureFlag( TEXTURE_EMISSIVE_SRGB_BIT, false );
			for ( unsigned slot = 0; slot < 5; ++slot )
			{
				const STexture *texture = nullptr;
				if ( maps[ slot ] >= 0 )
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
