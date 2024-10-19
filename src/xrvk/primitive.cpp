/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <xrvk/primitive.hpp>

namespace xrlib
{
	CPrimitive::CPrimitive( CSession *pSession, uint32_t drawPriority, XrVector3f scale, EAssetMotionType motionType, VkPipeline graphicsPipeline, XrSpace space ) :
		m_pSession( pSession ),
		unDrawPriority( drawPriority ), 
		pipeline( graphicsPipeline )
	{
		assert( pSession );

		// Create device memory buffers
		m_pIndexBuffer = new CDeviceBuffer( pSession );
		m_pVertexBuffer = new CDeviceBuffer( pSession );
		m_pInstanceBuffer = new CDeviceBuffer( pSession );

		// Pre-fill first instance
		instances.push_back( SInstanceState { motionType, space, scale } ); // pre-fill first instance
		instanceMatrices.push_back( XrMatrix4x4f() );
		XrMatrix4x4f_CreateTranslationRotationScale( &instanceMatrices[ 0 ], GetPosition( 0 ), GetOrientation( 0 ), GetScale( 0 ) );
	}

	CPrimitive::~CPrimitive() 
	{ 
		Reset();
		DeleteBuffers();
	}

	VkResult CPrimitive::InitBuffers() 
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
		return InitBuffer( m_pInstanceBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( XrMatrix4x4f ) * instanceMatrices.size(), instanceMatrices.data() );
	}

	VkResult CPrimitive::InitBuffer( CDeviceBuffer *pBuffer, VkBufferUsageFlags usageFlags, VkDeviceSize unSize, void *pData, VkMemoryPropertyFlags memPropFlags, VkAllocationCallbacks *pCallbacks )
	{
		// @todo debug assert. 
		// assert( pBuffer );

		return pBuffer->Init( usageFlags, memPropFlags, unSize, pData, pCallbacks );
	}

	void CPrimitive::ResetScale( float x, float y, float z, uint32_t unInstanceIndex ) 
	{
		// @todo - debug assert. ideally no checks here other than debug assert for perf
		instances[ unInstanceIndex ].scale = { x, y, z }; 
	}

	void CPrimitive::ResetScale( float fScale, uint32_t unInstanceIndex ) 
	{ 
		// @todo - debug assert. ideally no checks here other than debug assert for perf
		instances[ unInstanceIndex ].scale = { fScale, fScale, fScale }; 
	}

	void CPrimitive::Scale( float fPercent, uint32_t unInstanceIndex ) 
	{ 
		// @todo - debug assert. ideally no checks here other than debug assert for perf
		instances[ unInstanceIndex ].scale.x *= fPercent;
		instances[ unInstanceIndex ].scale.y *= fPercent;
		instances[ unInstanceIndex ].scale.z *= fPercent;
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
	}

	void CPrimitive::ResetVertices() 
	{ 
		m_vecVertices.clear(); }

	CDeviceBuffer *CPrimitive::UpdateBuffer( VkCommandBuffer transferCmdBuffer ) 
	{
		// Calculate buffer size
		VkDeviceSize bufferSize = instanceMatrices.size() * sizeof( XrMatrix4x4f );

		// Create staging buffer
		CDeviceBuffer *pStagingBuffer = new CDeviceBuffer( m_pSession );
		pStagingBuffer->Init(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			bufferSize, 
			instanceMatrices.data() );

		// Copy buffer
		VkBufferCopy bufferCopyRegion = {};
		bufferCopyRegion.size = bufferSize;
		vkCmdCopyBuffer( transferCmdBuffer, pStagingBuffer->GetVkBuffer(), m_pInstanceBuffer->GetVkBuffer(), 1, &bufferCopyRegion );

		return pStagingBuffer;
	}

	uint32_t CPrimitive::AddInstance( uint32_t unCount, XrVector3f scale ) 
	{ 
		for ( size_t i = 0; i < unCount; i++ )
		{
			instances.push_back( SInstanceState( scale ) );
			instanceMatrices.push_back( XrMatrix4x4f() );
		}

		if ( m_pInstanceBuffer )
			delete m_pInstanceBuffer;

		m_pInstanceBuffer = new CDeviceBuffer( m_pSession );
		assert( InitBuffer( m_pInstanceBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( XrMatrix4x4f ) * instanceMatrices.size(), nullptr ) == VK_SUCCESS);

		return GetInstanceCount();
	}

	void CPrimitive::UpdateModelMatrix( uint32_t unInstanceIndex, XrSpace baseSpace, XrTime time, bool bForceUpdate )
	{
		if ( instances[ unInstanceIndex ].motionType == EAssetMotionType::STATIC_INIT )
		{
			bForceUpdate = true;
			instances[ unInstanceIndex ].motionType = EAssetMotionType::STATIC_NO_INIT;
		}

		if ( instances[ unInstanceIndex ].motionType == EAssetMotionType::DYNAMIC || bForceUpdate )
		{
			if ( instances[ unInstanceIndex ].space != XR_NULL_HANDLE && baseSpace != XR_NULL_HANDLE )
			{
				XrSpaceLocation spaceLocation { XR_TYPE_SPACE_LOCATION };
				if ( XR_UNQUALIFIED_SUCCESS ( xrLocateSpace( instances[ unInstanceIndex ].space, baseSpace, time, &spaceLocation ) ) )
				{
					if ( spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT )
						instances[ unInstanceIndex ].pose.orientation = spaceLocation.pose.orientation;

					if ( spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT )
						instances[ unInstanceIndex ].pose.position = spaceLocation.pose.position;
				}
			}
			
			XrMatrix4x4f_CreateTranslationRotationScale( 
				&instanceMatrices[ unInstanceIndex ], 
				&instances[ unInstanceIndex ].pose.position,
				&instances[ unInstanceIndex ].pose.orientation, 
				&instances[ unInstanceIndex ].scale );
		}
	}

	XrMatrix4x4f *CPrimitive::GetModelMatrix( uint32_t unInstanceIndex, bool bRefresh ) 
	{ 
		if ( bRefresh )
			XrMatrix4x4f_CreateTranslationRotationScale( 
				&instanceMatrices[ unInstanceIndex ], 
				&instances[ unInstanceIndex ].pose.position, 
				&instances[ unInstanceIndex ].pose.orientation,
				&instances[ unInstanceIndex ].scale );

		return &instanceMatrices[ unInstanceIndex ];
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
		uint32_t drawPriority, 
		XrVector3f scale,
		EAssetMotionType motionType,
		float fAlpha, 
		VkPipeline graphicsPipeline, 
		XrSpace space ) 
		: CPrimitive( pSession, drawPriority, scale, motionType, graphicsPipeline, space )
	{
	
	}

	CColoredPrimitive::~CColoredPrimitive() {}

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

	VkResult CColoredPrimitive::InitBuffers() 
	{ 
		VkResult result = InitBuffer( m_pIndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof( unsigned short ) * m_vecIndices.size(), m_vecIndices.data() );
		if ( result != VK_SUCCESS )
			return result;

		result = InitBuffer( m_pVertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof( SColoredVertex ) * m_vecVertices.size(), m_vecVertices.data() );
		if ( result != VK_SUCCESS )
			return result;

		// todo assert for debug, matrices must be at least 1
		return InitBuffer( m_pInstanceBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof( XrMatrix4x4f ) * instanceMatrices.size(), instanceMatrices.data() );
	}

	CPyramid::CPyramid( CSession *pSession, uint32_t drawPriority, XrVector3f scale, EAssetMotionType EAssetMotionType, VkPipeline graphicsPipeline, XrSpace space )
		: CPrimitive( pSession, drawPriority, scale, EAssetMotionType, graphicsPipeline, space )

	{
		const XrVector3f VertPyramid_Tip { 0.f, 0.f, -0.5f };
		const XrVector3f VertPyramid_Top { 0.f, 0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Left { -0.5f, -0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Right { 0.5f, -0.5f, 0.5f };

		// Add Vertices
		AddTri( VertPyramid_Base_Left, VertPyramid_Top, VertPyramid_Tip );			// left (port)
		AddTri( VertPyramid_Base_Right, VertPyramid_Top, VertPyramid_Base_Left );	// back
		AddTri( VertPyramid_Tip, VertPyramid_Top, VertPyramid_Base_Right );			// right (starboard)
		AddTri( VertPyramid_Base_Left, VertPyramid_Tip, VertPyramid_Base_Right );	// base - this is wound clockwise so the face will end up facing downwards

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 12; i++ )
			m_vecIndices.push_back( i );
	}

	CPyramid::~CPyramid() {}

	CColoredPyramid::CColoredPyramid( CSession *pSession, uint32_t drawPriority, XrVector3f scale, EAssetMotionType motionType, float fAlpha, VkPipeline graphicsPipeline, XrSpace space )
		: CColoredPrimitive( pSession, drawPriority, scale, motionType, fAlpha, graphicsPipeline, space )
	{
		const XrVector3f VertPyramid_Tip { 0.f, 0.f, -0.5f };
		const XrVector3f VertPyramid_Top { 0.f, 0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Left { -0.5f, -0.5f, 0.5f };
		const XrVector3f VertPyramid_Base_Right { 0.5f, -0.5f, 0.5f };

		// Add Vertices
		AddColoredTri( VertPyramid_Top, VertPyramid_Base_Left, VertPyramid_Tip, k_ColorRed, fAlpha );				// left (port)
		AddColoredTri( VertPyramid_Top, VertPyramid_Base_Right, VertPyramid_Base_Left, k_ColorPurple, fAlpha );		// back
		AddColoredTri( VertPyramid_Top, VertPyramid_Tip, VertPyramid_Base_Right, k_ColorGreen, fAlpha );			// right (starboard)
		AddColoredTri( VertPyramid_Tip, VertPyramid_Base_Left, VertPyramid_Base_Right, k_ColorBlue, fAlpha );		// base - this is wound clockwise so the face will end up facing downwards

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 12; i++ )
			m_vecIndices.push_back( i );
	}

	CColoredPyramid::~CColoredPyramid() {}

	CColoredCube::CColoredCube( CSession *pSession, uint32_t drawPriority, XrVector3f scale, EAssetMotionType motionType, float fAlpha, VkPipeline graphicsPipeline, XrSpace space ) 
		: CColoredPrimitive( pSession, drawPriority, scale, motionType, fAlpha, graphicsPipeline, space )
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

		AddColoredTri( RBF, RTB, RBB, k_ColorRed, fAlpha );		// right (starboard)
		AddColoredTri( RTF, RTB, RBF, k_ColorRed, fAlpha );	

		AddColoredTri( RBF, LBB, LBF, k_ColorBlue, fAlpha );	// bottom
		AddColoredTri( RBB, LBB, RBF, k_ColorBlue, fAlpha ); 

		AddColoredTri( RTF, LTB, RTB, k_ColorTeal, fAlpha );	// top
		AddColoredTri( LTF, LTB, RTF, k_ColorTeal, fAlpha ); 

		AddColoredTri( RTB, LBB, RBB, k_ColorPurple, fAlpha );	// back 
		AddColoredTri( LTB, LBB, RTB, k_ColorPurple, fAlpha );

		AddColoredTri( RTF, LBF, LTF, k_ColorGold, fAlpha );	// front
		AddColoredTri( RBF, LBF, RTF, k_ColorGold, fAlpha );

		// Add indices
		// @todo
		for ( uint32_t i = 0; i < 36; i++ )
			m_vecIndices.push_back( i );
	}

	CColoredCube::~CColoredCube() {}

} // namespace xrlib