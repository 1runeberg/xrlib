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

#pragma once

#include <xrlib/ext/ext_base.hpp>
#include <vector>

namespace xrlib::FB
{
	class CTriangleMesh : public CExtBase
	{
	  public:
		explicit CTriangleMesh( XrInstance xrInstance );
		~CTriangleMesh();

		std::vector< XrTriangleMeshFB > *GetMeshes() { return &m_vecMeshes; }

		XrResult AddGeometry(
			XrSession xrSession,
			XrPassthroughLayerFB &layer,
			std::vector< XrVector3f > &vecVertices,
			std::vector< uint32_t > &vecIndices,
			XrTriangleMeshFlagsFB flags = 0,
			void * pOtherInfo = nullptr );

		XrResult RemoveGeometry( uint32_t unIndex );

		void ClearGeometryCache();

		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_triangle_mesh
		PFN_xrCreateTriangleMeshFB xrCreateTriangleMeshFB = nullptr;
		PFN_xrDestroyTriangleMeshFB xrDestroyTriangleMeshFB = nullptr;
		PFN_xrTriangleMeshGetVertexBufferFB xrTriangleMeshGetVertexBufferFB = nullptr;
		PFN_xrTriangleMeshGetIndexBufferFB xrTriangleMeshGetIndexBufferFB = nullptr;
		PFN_xrTriangleMeshBeginUpdateFB xrTriangleMeshBeginUpdateFB = nullptr;
		PFN_xrTriangleMeshEndUpdateFB xrTriangleMeshEndUpdateFB = nullptr;
		PFN_xrTriangleMeshBeginVertexBufferUpdateFB xrTriangleMeshBeginVertexBufferUpdateFB = nullptr;
		PFN_xrTriangleMeshEndVertexBufferUpdateFB xrTriangleMeshEndVertexBufferUpdateFB = nullptr;

	  private:
		std::vector< XrTriangleMeshFB > m_vecMeshes;

	};
}
