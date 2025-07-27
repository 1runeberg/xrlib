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


#include <xrvk/primitive.hpp>

namespace xrlib
{

	CPlane2D::CPlane2D(
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

	CPlane2D::CPlane2D( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace )
		: CRenderable( pSession, pRenderInfo, 0, 0, std::numeric_limits< uint32_t >::max(), bIsVisible, xrScale, xrSpace )
	{
	}

	CPlane2D::~CPlane2D()
	{
		Reset();
		DeleteBuffers();
	}

	VkResult CPlane2D::InitBuffers( bool bReset )
	{
		if ( m_pIndexBuffer )
			delete m_pIndexBuffer;

		m_pIndexBuffer = new CDeviceBuffer( m_pSession );
		VkResult result = InitBuffer( m_pIndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof( unsigned short ) * m_vecIndices.size(), m_vecIndices.data() );
		if ( result != VK_SUCCESS )
			return result;

		if ( m_pVertexBuffer )
			delete m_pVertexBuffer;

		m_pVertexBuffer = new CDeviceBuffer( m_pSession );
		result = InitBuffer( m_pVertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( XrVector2f ) * m_vecVertices.size(), m_vecVertices.data() );
		if ( result != VK_SUCCESS )
			return result;

		// todo assert for debug, matrices must be at least 1
		if ( m_pInstanceBuffer )
			delete m_pInstanceBuffer;

		m_pInstanceBuffer = new CDeviceBuffer( m_pSession );
		result = InitBuffer( m_pInstanceBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof( XrMatrix4x4f ) * instanceMatrices.size(), instanceMatrices.data() );

		if ( result != VK_SUCCESS )
			return result;

		// Reset
		if ( bReset )
			Reset();

		return VK_SUCCESS;
	}

	void CPlane2D::Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo )
	{
		// Push constants
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
		vkCmdBindIndexBuffer( commandBuffer, GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16 );
		vkCmdBindVertexBuffers( commandBuffer, 0, 1, GetVertexBuffer()->GetVkBufferPtr(), vertexOffsets );
		vkCmdBindVertexBuffers( commandBuffer, 1, 1, GetInstanceBuffer()->GetVkBufferPtr(), instanceOffsets );

		// Bind the descriptor sets
		if ( !vertexDescriptors.empty() )
		{
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ],
				0,   // should match set = x in shader
				vertexDescriptors.size(), // descriptorSetCount
				vertexDescriptors.data(), // descriptor sets
				0,					   // dynamicOffsetCount
				nullptr				   // pDynamicOffsets
			);
		}

		// Draw instanced
		vkCmdDrawIndexed( commandBuffer, GetIndices()->size(), GetInstanceCount(), 0, 0, 0 );
	}

	void CPlane2D::AddTri( XrVector2f v1, XrVector2f v2, XrVector2f v3 )
	{
		AddVertex( v1 );
		AddVertex( v2 );
		AddVertex( v3 );
	}

	void CPlane2D::AddIndex( unsigned short index ) { m_vecIndices.push_back( index ); }

	void CPlane2D::AddVertex( XrVector2f vertex ) { m_vecVertices.push_back( vertex ); }

	void CPlane2D::ResetIndices() { m_vecIndices.clear(); }

	void CPlane2D::ResetVertices() { m_vecVertices.clear(); }

	void CPlane2D::Reset()
	{
		ResetIndices();
		ResetVertices();
	}

	void CPlane2D::DeleteBuffers()
	{
		if ( m_pIndexBuffer )
			delete m_pIndexBuffer;

		if ( m_pVertexBuffer )
			delete m_pVertexBuffer;
	}

	CPrimitive::CPrimitive(
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

	CPrimitive::CPrimitive( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace ) : 
		CRenderable( pSession, pRenderInfo, 0, 0, std::numeric_limits< uint32_t >::max(), bIsVisible, xrScale, xrSpace )
	{
	}

	CPrimitive::~CPrimitive() 
	{
		Reset();
		DeleteBuffers();
	}

	VkResult CPrimitive::InitBuffers( bool bReset ) 
	{ 
		if ( m_pIndexBuffer )
			delete m_pIndexBuffer;

		m_pIndexBuffer = new CDeviceBuffer( m_pSession );
		VkResult result = InitBuffer( m_pIndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof( unsigned short ) * m_vecIndices.size(), m_vecIndices.data() );
		if ( result != VK_SUCCESS )
			return result;

		if ( m_pVertexBuffer )
			delete m_pVertexBuffer;

		m_pVertexBuffer = new CDeviceBuffer( m_pSession );
		result = InitBuffer( m_pVertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( XrVector3f ) * m_vecVertices.size(), m_vecVertices.data() );
		if ( result != VK_SUCCESS )
			return result;

		// todo assert for debug, matrices must be at least 1
		if ( m_pInstanceBuffer )
			delete m_pInstanceBuffer;

		m_pInstanceBuffer = new CDeviceBuffer( m_pSession );
		result = InitBuffer( 
			m_pInstanceBuffer, 
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
			sizeof( XrMatrix4x4f ) * instanceMatrices.size(), instanceMatrices.data() );
			
		if ( result != VK_SUCCESS )
				return result;

		// Reset
		if ( bReset )
			Reset();

		return VK_SUCCESS;
	}

	void CPrimitive::Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) 
	{
		// Push constants
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
		vkCmdBindIndexBuffer( commandBuffer, GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16 );
		vkCmdBindVertexBuffers( commandBuffer, 0, 1, GetVertexBuffer()->GetVkBufferPtr(), vertexOffsets ); 
		vkCmdBindVertexBuffers( commandBuffer, 1, 1, GetInstanceBuffer()->GetVkBufferPtr(), instanceOffsets ); 

		// Bind the descriptor sets
		if ( !vertexDescriptors.empty() )
		{
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ],
				0,   // should match set = x in shader
				vertexDescriptors.size(), // descriptorSetCount
				vertexDescriptors.data(), // descriptor sets
				0,					   // dynamicOffsetCount
				nullptr				   // pDynamicOffsets
			);
		}

		// Draw instanced
		vkCmdDrawIndexed( commandBuffer, GetIndices().size(), GetInstanceCount(), 0, 0, 0 );
	}

	void CPrimitive::AddTri( XrVector3f v1, XrVector3f v2, XrVector3f v3 ) 
	{ 
		AddVertex( v1 );
		AddVertex( v2 );
		AddVertex( v3 );

	}

	void CPrimitive::AddQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 ) 
	{
		AddVertex( v1 );
		AddVertex( v2 );
		AddVertex( v4 );

		AddVertex( v2 );
		AddVertex( v3 );
		AddVertex( v4 );
	}

	void CPrimitive::AddQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 ) 
	{
		AddVertex( v4 );
		AddVertex( v3 );
		AddVertex( v2 );

		AddVertex( v2 );
		AddVertex( v1 );
		AddVertex( v4 );
	}

	void CPrimitive::AddIndex( unsigned short index ) 
	{ 
		m_vecIndices.push_back( index ); 
	}

	void CPrimitive::AddVertex( XrVector3f vertex ) 
	{ 
		m_vecVertices.push_back( vertex ); 
	}

	void CPrimitive::Reset() 
	{ 
		ResetIndices();
		ResetVertices();
	}

	void CPrimitive::ResetIndices() 
	{ 
		m_vecIndices.clear(); 
		m_vecIndices.shrink_to_fit();
	}

	void CPrimitive::ResetVertices() 
	{ 
		m_vecVertices.clear(); 
		m_vecVertices.shrink_to_fit();
	}

	void CPrimitive::DeleteBuffers() 
	{
		if ( m_pIndexBuffer )
			delete m_pIndexBuffer;

		if ( m_pVertexBuffer )
			delete m_pVertexBuffer;

		if ( m_pInstanceBuffer )
			delete m_pInstanceBuffer;
	}

	CColoredPrimitive::CColoredPrimitive( 
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		float fAlpha,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: CRenderable( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, xrScale, xrSpace )
	{
	}

	CColoredPrimitive::CColoredPrimitive( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace, float fAlpha )
		: CRenderable( pSession, pRenderInfo, 0, 0, std::numeric_limits< uint32_t >::max(), bIsVisible, xrScale, xrSpace ) 
	{
	}

	void CColoredPrimitive::Reset() 
	{
		ResetIndices();
		ResetVertices();
	}

	VkResult CColoredPrimitive::InitBuffers( bool bReset ) 
	{ 
		if ( m_pIndexBuffer )
			delete m_pIndexBuffer;

		m_pIndexBuffer = new CDeviceBuffer( m_pSession );
		VkResult result = InitBuffer( m_pIndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof( unsigned short ) * m_vecIndices.size(), m_vecIndices.data() );
		if ( result != VK_SUCCESS )
			return result;

		if ( m_pVertexBuffer )
			delete m_pVertexBuffer;

		m_pVertexBuffer = new CDeviceBuffer( m_pSession );
		result = InitBuffer( m_pVertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( SColoredVertex ) * m_vecVertices.size(), m_vecVertices.data() );
		if ( result != VK_SUCCESS )
			return result;

		// todo assert for debug, matrices must be at least 1
		if ( m_pInstanceBuffer )
			delete m_pInstanceBuffer;

		m_pInstanceBuffer = new CDeviceBuffer( m_pSession );
		result = InitBuffer( 
			m_pInstanceBuffer, 
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
			sizeof( XrMatrix4x4f ) * instanceMatrices.size(), instanceMatrices.data() );
			
		if ( result != VK_SUCCESS )
				return result;
		
		// Reset
		if ( bReset )
			Reset();

		return VK_SUCCESS;
	}

	void CColoredPrimitive::Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) 
	{
		// Push constants
		vkCmdPushConstants( commandBuffer, renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ], VK_SHADER_STAGE_VERTEX_BIT, 0, k_pcrSize, renderInfo.state.eyeVPs.data() );

		// Set stencil reference
		vkCmdSetStencilReference( commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1 );

		// Bind the graphics pipeline for this shape
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderInfo.vecGraphicsPipelines[ graphicsPipelineIndex ] );

		// Bind shape's index and vertex buffers
		vkCmdBindIndexBuffer( commandBuffer, GetIndexBuffer()->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16 );
		vkCmdBindVertexBuffers( commandBuffer, 0, 1, GetVertexBuffer()->GetVkBufferPtr(), vertexOffsets );
		vkCmdBindVertexBuffers( commandBuffer, 1, 1, GetInstanceBuffer()->GetVkBufferPtr(), instanceOffsets );

		// Bind the descriptor sets
		if ( !vertexDescriptors.empty() )
		{
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				renderInfo.vecPipelineLayouts[ pipelineLayoutIndex ],
				0,   // should match set = x in shader
				vertexDescriptors.size(), // descriptorSetCount
				vertexDescriptors.data(), // descriptor sets
				0,					   // dynamicOffsetCount
				nullptr				   // pDynamicOffsets
			);
		}

		// Draw instanced
		vkCmdDrawIndexed( commandBuffer, GetIndices().size(), GetInstanceCount(), 0, 0, 0 );
	}

	void CColoredPrimitive::AddIndex( unsigned short index ) 
	{ 
		m_vecIndices.push_back( index ); 
	}

	void CColoredPrimitive::AddVertex( XrVector3f vertex ) 
	{ 
		AddColoredVertex( vertex, k_ColorMagenta );
	}

	void CColoredPrimitive::AddColoredVertex( XrVector3f vertex, XrVector3f color, float fAlpha ) 
	{ 
		m_vecVertices.push_back( { vertex, { color.x, color.y, color.z, fAlpha } } );
	}

	void CColoredPrimitive::AddTri( XrVector3f v1, XrVector3f v2, XrVector3f v3 ) 
	{ 
		AddColoredTri( v1, v2, v3, k_ColorMagenta );
	}

	void CColoredPrimitive::AddColoredTri( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f color, float fAlpha ) 
	{ 
		m_vecVertices.push_back( { v1, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v2, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v3, { color.x, color.y, color.z, fAlpha } } );
	}

	void CColoredPrimitive::AddQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 ) 
	{ 
		AddColoredQuadCW( v1, v2, v3, v4, k_ColorMagenta );
	}

	void CColoredPrimitive::AddColoredQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4, XrVector3f color, float fAlpha ) 
	{ 
		m_vecVertices.push_back( { v1, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v2, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v4, { color.x, color.y, color.z, fAlpha } } );

		m_vecVertices.push_back( { v2, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v3, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v4, { color.x, color.y, color.z, fAlpha } } );
	}

	void CColoredPrimitive::AddQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 ) 
	{ 
		AddColoredQuadCCW( v1, v2, v3, v4, k_ColorMagenta );
	}

	void CColoredPrimitive::AddColoredQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4, XrVector3f color, float fAlpha ) 
	{
		m_vecVertices.push_back( { v4, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v3, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v2, { color.x, color.y, color.z, fAlpha } } );

		m_vecVertices.push_back( { v2, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v1, { color.x, color.y, color.z, fAlpha } } );
		m_vecVertices.push_back( { v4, { color.x, color.y, color.z, fAlpha } } );
	}

	void CColoredPrimitive::Recolor( XrVector3f color, float fAlpha ) 
	{
		for ( auto &vertex : m_vecVertices )
			vertex.color = { color.x, color.y, color.z, fAlpha };
	}

	void CColoredPrimitive::ResetIndices() 
	{
		m_vecIndices.clear();
		m_vecIndices.shrink_to_fit();
	}

	void CColoredPrimitive::ResetVertices() 
	{
		m_vecVertices.clear();
		m_vecVertices.shrink_to_fit();
	}

	void CColoredPrimitive::DeleteBuffers() 
	{
		if ( m_pIndexBuffer )
			delete m_pIndexBuffer;

		if ( m_pVertexBuffer )
			delete m_pVertexBuffer;
	}

	CPlane::CPlane( CSession *pSession, CRenderInfo *pRenderInfo, uint16_t pipelineLayoutIdx, uint16_t graphicsPipelineIdx, bool bFacePlayer, uint32_t descriptorLayoutIdx, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace )
		: CPrimitive( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, xrScale, xrSpace )
	{
		InitShape( bFacePlayer );
	}

	CPlane::CPlane( CSession *pSession, CRenderInfo *pRenderInfo, bool bFacePlayer, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace )
		: CPrimitive( pSession, pRenderInfo, 0, 0, std::numeric_limits< uint32_t >::max(), bIsVisible, xrScale, xrSpace )
	{
		InitShape( bFacePlayer );
	}

	void CPlane::InitShape( bool bFacePlayer )
	{
		m_vecVertices.clear();
		m_vecIndices.clear();

		if ( bFacePlayer )
		{
			// XY plane (Z=0), facing player (+Z)
			m_vecVertices.push_back( { -0.5f, 0.5f, 0.0f } );  // top left
			m_vecVertices.push_back( { 0.5f, 0.5f, 0.0f } );   // top right
			m_vecVertices.push_back( { 0.5f, -0.5f, 0.0f } );  // bottom right
			m_vecVertices.push_back( { -0.5f, -0.5f, 0.0f } ); // bottom left
		}
		else
		{
			// XZ plane (Y=0)
			m_vecVertices.push_back( { -0.5f, 0.0f, -0.5f } ); // left, back
			m_vecVertices.push_back( { 0.5f, 0.0f, -0.5f } );  // right, back
			m_vecVertices.push_back( { 0.5f, 0.0f, 0.5f } );   // right, front
			m_vecVertices.push_back( { -0.5f, 0.0f, 0.5f } );  // left, front
		}

		m_vecIndices = { 0, 1, 2, 2, 3, 0 };
	}

	CPlane::~CPlane()
	{
		Reset();
		DeleteBuffers();
	}

	constexpr XrVector3f k_ColorWhite { 1.f, 1.f, 1.f };

	CColoredPlane::
		CColoredPlane( CSession *pSession, CRenderInfo *pRenderInfo, uint16_t pipelineLayoutIdx, uint16_t graphicsPipelineIdx, bool bFacePlayer, uint32_t descriptorLayoutIdx, bool bIsVisible, float fAlpha, XrVector3f xrScale, XrSpace xrSpace )
		: CColoredPrimitive( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, fAlpha, xrScale, xrSpace )
	{
		InitShape( bFacePlayer, fAlpha );
	}

	CColoredPlane::CColoredPlane( CSession *pSession, CRenderInfo *pRenderInfo, bool bFacePlayer, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace, float fAlpha )
		: CColoredPrimitive( pSession, pRenderInfo, bIsVisible, xrScale, xrSpace, fAlpha )
	{
		InitShape( bFacePlayer, fAlpha );
	}

	void CColoredPlane::InitShape( bool bFacePlayer, float fAlpha )
	{
		m_vecVertices.clear();
		m_vecIndices.clear();

		if ( bFacePlayer )
		{
			// XY plane (Z=0), facing player (+Z)
			AddColoredVertex( { -0.5f, 0.5f, 0.0f }, k_ColorWhite, fAlpha );  // top left
			AddColoredVertex( { 0.5f, 0.5f, 0.0f }, k_ColorWhite, fAlpha );   // top right
			AddColoredVertex( { 0.5f, -0.5f, 0.0f }, k_ColorWhite, fAlpha );  // bottom right
			AddColoredVertex( { -0.5f, -0.5f, 0.0f }, k_ColorWhite, fAlpha ); // bottom left
		}
		else
		{
			// XZ plane (Y=0)
			AddColoredVertex( { -0.5f, 0.0f, -0.5f }, k_ColorWhite, fAlpha ); // left, back
			AddColoredVertex( { 0.5f, 0.0f, -0.5f }, k_ColorWhite, fAlpha );  // right, back
			AddColoredVertex( { 0.5f, 0.0f, 0.5f }, k_ColorWhite, fAlpha );	  // right, front
			AddColoredVertex( { -0.5f, 0.0f, 0.5f }, k_ColorWhite, fAlpha );  // left, front
		}

		m_vecIndices = { 0, 1, 2, 2, 3, 0 };
	}

	CPyramid::CPyramid(
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: CPrimitive( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, xrScale, xrSpace )
	{
		InitShape();
	}

	CPyramid::CPyramid( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace ) : 
		CPrimitive( pSession, pRenderInfo, 0, 0, std::numeric_limits< uint32_t >::max(), bIsVisible, xrScale, xrSpace )
	{
		InitShape();
	}

	void CPyramid::InitShape() 
	{
		const XrVector3f VertPyramid_Tip { 0.f, 0.f, -0.5f };
		const XrVector3f VertPyramid_Top { 0.f, 0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Left { -0.5f, -0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Right { 0.5f, -0.5f, 0.5f };

		// Add Vertices
		AddTri( VertPyramid_Base_Left, VertPyramid_Top, VertPyramid_Tip );		  // left (port)
		AddTri( VertPyramid_Base_Right, VertPyramid_Top, VertPyramid_Base_Left ); // back
		AddTri( VertPyramid_Tip, VertPyramid_Top, VertPyramid_Base_Right );		  // right (starboard)
		AddTri( VertPyramid_Base_Left, VertPyramid_Tip, VertPyramid_Base_Right ); // base - this is wound clockwise so the face will end up facing downwards

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 12; i++ )
			m_vecIndices.push_back( i );
	}

	CColoredPyramid::CColoredPyramid(
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		float fAlpha,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: CColoredPrimitive( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, fAlpha, xrScale, xrSpace )
	{
		InitShape( fAlpha );
	}

	CColoredPyramid::CColoredPyramid( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace, float fAlpha ) : CColoredPrimitive( pSession, pRenderInfo, bIsVisible, xrScale, xrSpace, fAlpha )
	{
		InitShape( fAlpha );
	}

	void CColoredPyramid::InitShape( float fAlpha ) 
	{
		const XrVector3f VertPyramid_Tip { 0.f, 0.f, -0.5f };
		const XrVector3f VertPyramid_Top { 0.f, 0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Left { -0.5f, -0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Right { 0.5f, -0.5f, 0.5f };

		// Add Vertices
		AddColoredTri( VertPyramid_Top, VertPyramid_Base_Left, VertPyramid_Tip, k_ColorRed, fAlpha );			// left (port)
		AddColoredTri( VertPyramid_Top, VertPyramid_Base_Right, VertPyramid_Base_Left, k_ColorPurple, fAlpha ); // back
		AddColoredTri( VertPyramid_Top, VertPyramid_Tip, VertPyramid_Base_Right, k_ColorGreen, fAlpha );		// right (starboard)
		AddColoredTri( VertPyramid_Tip, VertPyramid_Base_Left, VertPyramid_Base_Right, k_ColorGold, fAlpha );	// base - this is wound clockwise so the face will end up facing downwards

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 12; i++ )
			m_vecIndices.push_back( i );
	}

	CColoredCube::CColoredCube(
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		float fAlpha,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: CColoredPrimitive( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, fAlpha, xrScale, xrSpace )
	{
		InitShape( fAlpha );
	}

	CColoredCube::CColoredCube( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace, float fAlpha )
		: CColoredPrimitive( pSession, pRenderInfo, bIsVisible, xrScale, xrSpace, fAlpha )
	{
		InitShape( fAlpha );
	}

	void CColoredCube::InitShape( float fAlpha ) 
	{
		// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
		const XrVector3f LBB { -0.5f, -0.5f, -0.5f };
		const XrVector3f LBF { -0.5f, -0.5f, 0.5f };
		const XrVector3f LTB { -0.5f, 0.5f, -0.5f };
		const XrVector3f LTF { -0.5f, 0.5f, 0.5f };
		const XrVector3f RBB { 0.5f, -0.5f, -0.5f };
		const XrVector3f RBF { 0.5f, -0.5f, 0.5f };
		const XrVector3f RTB { 0.5f, 0.5f, -0.5f };
		const XrVector3f RTF { 0.5f, 0.5f, 0.5f };

		AddColoredTri( LBB, LTB, LBF, k_ColorRed, fAlpha ); // left (port)
		AddColoredTri( LBF, LTB, LTF, k_ColorRed, fAlpha );

		AddColoredTri( RBF, RTB, RBB, k_ColorGreen, fAlpha ); // right (starboard)
		AddColoredTri( RTF, RTB, RBF, k_ColorGreen, fAlpha );

		AddColoredTri( RBF, LBB, LBF, k_ColorGold, fAlpha ); // bottom
		AddColoredTri( RBB, LBB, RBF, k_ColorGold, fAlpha );

		AddColoredTri( RTF, LTB, RTB, k_ColorTeal, fAlpha ); // top
		AddColoredTri( LTF, LTB, RTF, k_ColorTeal, fAlpha );

		AddColoredTri( RTB, LBB, RBB, k_ColorBlue, fAlpha ); // front
		AddColoredTri( LTB, LBB, RTB, k_ColorBlue, fAlpha );

		AddColoredTri( RTF, LBF, LTF, k_ColorPurple, fAlpha ); // back
		AddColoredTri( RBF, LBF, RTF, k_ColorPurple, fAlpha );

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 36; i++ )
			m_vecIndices.push_back( i );
	}

	CInvertedCube::CInvertedCube(
		CSession *pSession,
		CRenderInfo *pRenderInfo,
		uint16_t pipelineLayoutIdx,
		uint16_t graphicsPipelineIdx,
		uint32_t descriptorLayoutIdx,
		bool bIsVisible,
		float fAlpha,
		XrVector3f xrScale,
		XrSpace xrSpace )
		: CColoredPrimitive( pSession, pRenderInfo, pipelineLayoutIdx, graphicsPipelineIdx, descriptorLayoutIdx, bIsVisible, fAlpha, xrScale, xrSpace )
	{
		InitShape( fAlpha );
	}

	CInvertedCube::CInvertedCube( CSession *pSession, CRenderInfo *pRenderInfo, bool bIsVisible, XrVector3f xrScale, XrSpace xrSpace, float fAlpha ) : CColoredPrimitive( pSession, pRenderInfo, bIsVisible, xrScale, xrSpace, fAlpha )
	{
		InitShape( fAlpha );
	}

	void CInvertedCube::InitShape( float fAlpha ) 
	{
		// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
		const XrVector3f LBB { -0.5f, -0.5f, -0.5f };
		const XrVector3f LBF { -0.5f, -0.5f, 0.5f };
		const XrVector3f LTB { -0.5f, 0.5f, -0.5f };
		const XrVector3f LTF { -0.5f, 0.5f, 0.5f };
		const XrVector3f RBB { 0.5f, -0.5f, -0.5f };
		const XrVector3f RBF { 0.5f, -0.5f, 0.5f };
		const XrVector3f RTB { 0.5f, 0.5f, -0.5f };
		const XrVector3f RTF { 0.5f, 0.5f, 0.5f };

		AddColoredTri( LTB, LBB, LBF, k_ColorRed, fAlpha ); // left (port)
		AddColoredTri( LBF, LTF, LTB, k_ColorRed, fAlpha );

		AddColoredTri( RTB, RBF, RBB, k_ColorGreen, fAlpha ); // right (starboard)
		AddColoredTri( RTB, RTF, RBF, k_ColorGreen, fAlpha );

		AddColoredTri( LBB, RBF, LBF, k_ColorGold, fAlpha ); // bottom
		AddColoredTri( LBB, RBB, RBF, k_ColorGold, fAlpha );

		AddColoredTri( LTB, RTF, RTB, k_ColorTeal, fAlpha ); // top
		AddColoredTri( LTB, LTF, RTF, k_ColorTeal, fAlpha );

		AddColoredTri( LBB, RTB, RBB, k_ColorPurple, fAlpha ); // back
		AddColoredTri( LBB, LTB, RTB, k_ColorPurple, fAlpha );

		AddColoredTri( LBF, RTF, LTF, k_ColorBlue, fAlpha ); // front
		AddColoredTri( LBF, RBF, RTF, k_ColorBlue, fAlpha );

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 36; i++ )
			m_vecIndices.push_back( i );
	}

} // namespace xrlib