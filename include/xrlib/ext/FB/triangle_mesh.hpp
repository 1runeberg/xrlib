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

#pragma once

#include <xrlib/ext/ext_base.hpp>

#include <vector>

namespace xrlib::FB
{
	class TriangleMesh : public ExtBase
	{
	  public:
		TriangleMesh( XrInstance xrInstance );
		~TriangleMesh();

		std::vector< XrTriangleMeshFB > *GetMeshes() { return &m_vecMeshes; }

		XrResult AddGeometry(
			XrSession xrSession,
			XrPassthroughLayerFB &layer,
			std::vector< XrVector3f > &vecVertices,
			std::vector< uint32_t > &vecIndices,
			XrSpace xrSpace,
			XrPosef xrPose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } },
			XrVector3f xrScale = { 1.f, 1.f, 1.f } );

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
