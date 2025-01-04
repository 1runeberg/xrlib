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


#include <xrlib/ext/FB/triangle_mesh.hpp>

#include <xrlib/common.hpp>

namespace xrlib::FB
{
	CTriangleMesh::CTriangleMesh( XrInstance xrInstance ): 
		CExtBase( xrInstance, XR_FB_TRIANGLE_MESH_EXTENSION_NAME )
	{
		INIT_PFN( xrInstance, xrCreateTriangleMeshFB );
		INIT_PFN( xrInstance, xrDestroyTriangleMeshFB );
		INIT_PFN( xrInstance, xrTriangleMeshGetVertexBufferFB );
		INIT_PFN( xrInstance, xrTriangleMeshGetIndexBufferFB );
		INIT_PFN( xrInstance, xrTriangleMeshBeginUpdateFB );
		INIT_PFN( xrInstance, xrTriangleMeshEndUpdateFB );
		INIT_PFN( xrInstance, xrTriangleMeshBeginVertexBufferUpdateFB );
		INIT_PFN( xrInstance, xrTriangleMeshEndVertexBufferUpdateFB );
	}

	CTriangleMesh::~CTriangleMesh() 
	{ 
		ClearGeometryCache();
	}

	XrResult CTriangleMesh::AddGeometry(
		XrSession xrSession,
		XrPassthroughLayerFB &layer,
		std::vector< XrVector3f > &vecVertices,
		std::vector< uint32_t > &vecIndices,
		XrSpace xrSpace,
		XrPosef xrPose,
		XrVector3f xrScale )
	{
		XrTriangleMeshCreateInfoFB xrMeshCI = { XR_TYPE_TRIANGLE_MESH_CREATE_INFO_FB };
		xrMeshCI.vertexBuffer = vecVertices.data();
		xrMeshCI.triangleCount = vecVertices.size();
		xrMeshCI.vertexCount = xrMeshCI.triangleCount * 3;
		xrMeshCI.indexBuffer = vecIndices.data();

		XrTriangleMeshFB xrTriangleMeshFB = XR_NULL_HANDLE;
		XR_RETURN_ON_ERROR( xrCreateTriangleMeshFB( xrSession, &xrMeshCI, &xrTriangleMeshFB ) );

		GetMeshes()->push_back( xrTriangleMeshFB );
		return XR_SUCCESS;
	}

	XrResult CTriangleMesh::RemoveGeometry( uint32_t unIndex ) 
	{ 
		assert( unIndex < GetMeshes()->size() );

		// delete mesh
		if ( m_vecMeshes[ unIndex ] != XR_NULL_HANDLE )
			XR_RETURN_ON_ERROR( xrDestroyTriangleMeshFB( m_vecMeshes[ unIndex ] ) );

		// delete entry from vector
		m_vecMeshes.erase( m_vecMeshes.begin() + unIndex );
		m_vecMeshes.shrink_to_fit();

		return XR_SUCCESS; 
	}

	void CTriangleMesh::ClearGeometryCache() 
	{
		for ( auto &mesh : m_vecMeshes )
			if ( mesh != XR_NULL_HANDLE )
				xrDestroyTriangleMeshFB( mesh );
	}

} 
