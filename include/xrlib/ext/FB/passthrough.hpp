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

#include <xrlib/ext/ext_base_passthrough.hpp>
#include <xrlib/ext/FB/triangle_mesh.hpp>
#include <xrlib/instance.hpp>
#include <xrlib/session.hpp>

#include <assert.h>
#include <string>
#include <vector>

namespace xrlib::FB
{
	class Passthrough : public xrlib::ExtBase_Passthrough
	{
	  public:
		struct PassthroughLayer
		{
			XrPassthroughLayerFB layer = XR_NULL_HANDLE;
			XrCompositionLayerPassthroughFB composition { XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB };
			XrPassthroughStyleFB style { XR_TYPE_PASSTHROUGH_STYLE_FB };
		};

		Passthrough( XrInstance xrInstance );
		~Passthrough();

		// Passthrough base functions
		XrResult Init( XrSession session, CInstance *pInstance, void *pOtherInfo ) override;
		
		XrResult AddLayer( XrSession session, ELayerType eLayerType, XrCompositionLayerFlags flags, 
			float fOpacity = 1.0f, XrSpace xrSpace = XR_NULL_HANDLE, void *pOtherInfo = nullptr ) override;
		XrResult RemoveLayer( uint32_t unIndex ) override;
		
		XrResult Start() override;
		XrResult Stop() override;

		XrResult PauseLayer( int index = -1 ) override;
		XrResult ResumeLayer( int index = -1 ) override;

		void GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers ) override;

		// FB Passthrough additional capabilities
		std::vector< PassthroughLayer > *GetPassthroughLayers() { return &m_vecPassthroughLayers; }
		
		XrResult SetPassThroughOpacity( PassthroughLayer &refLayer, float fOpacity );
		XrResult SetPassThroughEdgeColor( PassthroughLayer &refLayer, XrColor4f xrEdgeColor );
		XrResult SetPassThroughParams( PassthroughLayer &refLayer, float fOpacity, XrColor4f xrEdgeColor );

		XrResult SetStyleToMono( int index, float fOpacity = 1.0f );
		XrResult SetStyleToColorMap( int index, bool bRed, bool bGreen, bool bBlue, float fAlpha = 1.0f, float fOpacity = 1.0f );
		XrResult SetBCS( int index, float fOpacity = 1.0f, float fBrightness = 0.0f, float fContrast = 1.0f, float fSaturation = 1.0f );

		// FB Triangle mesh
		void SetTriangleMesh( TriangleMesh *pTriangleMesh );
		TriangleMesh *GetTriangleMesh() { return m_pTriangleMesh;  }
		bool IsTriangleMeshSupported() { return m_pTriangleMesh && flagSupportedLayerTypes.IsSet( (int) ELayerType::MESH_PROJECTION ); }
		bool CreateTriangleMesh( CInstance *pInstance );

		// Geometry handling
		std::vector< XrGeometryInstanceFB > *GetGeometryInstances() { return &m_vecGeometryInstances;  }

		XrResult AddGeometry(
			XrSession session,
			XrPassthroughLayerFB &layer,
			std::vector< XrVector3f > &vecVertices,
			std::vector< uint32_t > &vecIndices,
			XrSpace xrSpace,
			XrPosef xrPose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } },
			XrVector3f xrScale = { 1.f, 1.f, 1.f } );

		XrResult UpdateGeometry(
			XrGeometryInstanceFB xrGeomInstance,
			XrSpace xrSpace,
			XrTime xrTime,
			XrPosef xrPose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } },
			XrVector3f xrScale = { 1.f, 1.f, 1.f } );


		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_passthrough
		PFN_xrCreatePassthroughFB xrCreatePassthroughFB = nullptr;
		PFN_xrDestroyPassthroughFB xrDestroyPassthroughFB = nullptr;
		PFN_xrPassthroughStartFB xrPassthroughStartFB = nullptr;
		PFN_xrPassthroughPauseFB xrPassthroughPauseFB = nullptr;
		PFN_xrCreatePassthroughLayerFB xrCreatePassthroughLayerFB = nullptr;
		PFN_xrDestroyPassthroughLayerFB xrDestroyPassthroughLayerFB = nullptr;
		PFN_xrPassthroughLayerSetStyleFB xrPassthroughLayerSetStyleFB = nullptr;
		PFN_xrPassthroughLayerPauseFB xrPassthroughLayerPauseFB = nullptr;
		PFN_xrPassthroughLayerResumeFB xrPassthroughLayerResumeFB = nullptr;
		PFN_xrCreateGeometryInstanceFB xrCreateGeometryInstanceFB = nullptr;
		PFN_xrDestroyGeometryInstanceFB xrDestroyGeometryInstanceFB = nullptr;
		PFN_xrGeometryInstanceSetTransformFB xrGeometryInstanceSetTransformFB = nullptr;


	  private:
		// Passthrough layers	
		std::vector< PassthroughLayer > m_vecPassthroughLayers;

		// The main passthrough object which represents the passthrough feature
		XrPassthroughFB m_fbPassthrough = XR_NULL_HANDLE;

		// Triangle mesh extension for mesh projections of the passthrough layer
		TriangleMesh *m_pTriangleMesh = nullptr;

		// Geometry instances for mesh projections
		std::vector< XrGeometryInstanceFB > m_vecGeometryInstances;
	};

} 
