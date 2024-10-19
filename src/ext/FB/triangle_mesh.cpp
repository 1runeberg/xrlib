/* 
 * Copyright 2023-24 Beyond Reality Labs Ltd (https://beyondreality.io)
 * Copyright 2021-24 Rune Berg (GitHub: https://github.com/1runeberg, YT: https://www.youtube.com/@1RuneBerg, X: https://twitter.com/1runeberg, BSky: https://runeberg.social)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <xrlib/ext/FB/triangle_mesh.hpp>

#include <xrlib/common.hpp>

namespace xrlib::FB
{
	TriangleMesh::TriangleMesh( XrInstance xrInstance ): 
		ExtBase( xrInstance, XR_FB_TRIANGLE_MESH_EXTENSION_NAME )
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

	TriangleMesh::~TriangleMesh() 
	{ 
		ClearGeometryCache();
	}

	XrResult TriangleMesh::AddGeometry(
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

	XrResult TriangleMesh::RemoveGeometry( uint32_t unIndex ) 
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

	void TriangleMesh::ClearGeometryCache() 
	{
		for ( auto &mesh : m_vecMeshes )
			if ( mesh != XR_NULL_HANDLE )
				xrDestroyTriangleMeshFB( mesh );
	}

} 
