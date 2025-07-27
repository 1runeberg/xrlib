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

#include <xrlib/ext/ext_base_passthrough.hpp>
#include <xrlib/ext/FB/triangle_mesh.hpp>
#include <xrlib/instance.hpp>
#include <xrlib/session.hpp>

#include <assert.h>
#include <string>
#include <vector>

namespace xrlib::FB
{
	class CPassthrough : public xrlib::ExtBase_Passthrough
	{
	  public:
		struct SPassthroughLayer
		{
			XrPassthroughLayerFB layer = XR_NULL_HANDLE;
			XrCompositionLayerPassthroughFB composition { XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB };
			XrPassthroughStyleFB style { XR_TYPE_PASSTHROUGH_STYLE_FB };
		};

		explicit CPassthrough( XrInstance xrInstance );
		~CPassthrough();

		// CPassthrough base functions
		XrResult Init( XrSession session, CInstance *pInstance, void *pOtherInfo = nullptr ) override;
		XrResult Init( XrSession session, CInstance *pInstance, XrPassthroughFlagsFB flags, void *pOtherInfo = nullptr );

		bool BSystemSupportsPassthrough();
		bool BSystemSupportsColorPassthrough();
		
		XrResult AddLayer(
			XrSession session,
			ELayerType eLayerType,
			XrCompositionLayerFlags flags, 
			XrFlags64 layerFlags = 0,
			float fOpacity = 1.0f, XrSpace xrSpace = XR_NULL_HANDLE, void *pOtherInfo = nullptr ) override;
		XrResult RemoveLayer( uint32_t unIndex ) override;
		
		XrResult Start() override;
		XrResult Stop() override;

		XrResult PauseLayer( int index = -1 ) override;
		XrResult ResumeLayer( int index = -1 ) override;

		void GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers, bool bReset = true ) override;
		void GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers, std::vector< uint32_t > &vecIndices, bool bReset = true ) override;

		// FB CPassthrough additional capabilities
		std::vector< SPassthroughLayer > *GetPassthroughLayers() { return &m_vecPassthroughLayers; }
		
		XrResult SetPassThroughOpacity( SPassthroughLayer &refLayer, float fOpacity );
		XrResult SetPassThroughEdgeColor( SPassthroughLayer &refLayer, XrColor4f xrEdgeColor );
		XrResult SetPassThroughParams( SPassthroughLayer &refLayer, float fOpacity, XrColor4f xrEdgeColor );

		XrResult SetStyleToMono( int index, float fOpacity = 1.0f );
		XrResult SetStyleToColorMap( int index, bool bRed, bool bGreen, bool bBlue, float fAlpha = 1.0f, float fOpacity = 1.0f );
		XrResult SetBCS( int index, float fOpacity = 1.0f, float fBrightness = 0.0f, float fContrast = 1.0f, float fSaturation = 1.0f );

		// FB Triangle mesh
		void SetTriangleMesh( CTriangleMesh *pTriangleMesh );
		CTriangleMesh *GetTriangleMesh() { return m_pTriangleMesh;  }
		bool IsTriangleMeshSupported() { return m_pTriangleMesh && flagSupportedLayerTypes.IsSet( (int) ELayerType::MESH_PROJECTION ); }
		bool CreateTriangleMesh( CInstance *pInstance );

		// Geometry handling
		std::vector< XrGeometryInstanceFB > *GetGeometryInstances() { return &m_vecGeometryInstances;  }

		XrResult AddGeometry(
			XrSession session,
			XrPassthroughLayerFB &layer,
			std::vector< XrVector3f > &vecVertices,
			std::vector< uint32_t > &vecIndices,
			XrSpace baseSpace,
			XrTriangleMeshFlagsFB triFlags = 0,
			XrPosef xrPose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } },
			XrVector3f xrScale = { 1.f, 1.f, 1.f } );

		XrResult UpdateGeometry(
			XrGeometryInstanceFB xrGeomInstance,
			XrSpace baseSpace,
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
		CInstance *m_pInstance = nullptr;

		// CPassthrough layers	
		std::vector< SPassthroughLayer > m_vecPassthroughLayers;

		// The main passthrough object which represents the passthrough feature
		XrPassthroughFB m_fbPassthrough = XR_NULL_HANDLE;

		// Triangle mesh extension for mesh projections of the passthrough layer
		CTriangleMesh *m_pTriangleMesh = nullptr;

		// Geometry instances for mesh projections
		std::vector< XrGeometryInstanceFB > m_vecGeometryInstances;
	};

} 
