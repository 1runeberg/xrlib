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


#include <xrlib/ext/FB/passthrough.hpp>
#include <xrlib/log.hpp>

namespace xrlib::FB
{

	CPassthrough::CPassthrough( XrInstance xrInstance )
		: ExtBase_Passthrough( xrInstance, XR_FB_PASSTHROUGH_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );

		// Initialize all function pointers
		INIT_PFN( xrInstance, xrCreatePassthroughFB );
		INIT_PFN( xrInstance, xrDestroyPassthroughFB );
		INIT_PFN( xrInstance, xrPassthroughStartFB );
		INIT_PFN( xrInstance, xrPassthroughPauseFB );
		INIT_PFN( xrInstance, xrCreatePassthroughLayerFB );
		INIT_PFN( xrInstance, xrDestroyPassthroughLayerFB );
		INIT_PFN( xrInstance, xrPassthroughLayerSetStyleFB );
		INIT_PFN( xrInstance, xrPassthroughLayerPauseFB );
		INIT_PFN( xrInstance, xrPassthroughLayerResumeFB );
		INIT_PFN( xrInstance, xrCreateGeometryInstanceFB );
		INIT_PFN( xrInstance, xrDestroyGeometryInstanceFB );
		INIT_PFN( xrInstance, xrGeometryInstanceSetTransformFB );
	}

	CPassthrough::~CPassthrough()
	{
		// Delete all layers
		for ( auto& layer : m_vecPassthroughLayers )
		{
			if ( layer.layer != XR_NULL_HANDLE )
				xrDestroyPassthroughLayerFB( layer.layer );
		}

		m_vecPassthroughLayers.clear();

		// Delete triangle mesh - this will also cleanup all triangle mesh extension objects/cache
		if ( m_pTriangleMesh )
			delete m_pTriangleMesh;
		
		// Delete geometry instances
		for ( auto &geom : m_vecGeometryInstances )
		{
			if ( geom != XR_NULL_HANDLE )
				xrDestroyGeometryInstanceFB( geom );
		}

		// Delete main passthrough object
		if ( m_fbPassthrough != XR_NULL_HANDLE )
			xrDestroyPassthroughFB( m_fbPassthrough );
	}

	XrResult CPassthrough::Init( XrSession session, CInstance *pInstance, void *pOtherInfo )
	{
		m_pInstance = pInstance;

		// Create passthrough objects
		XrPassthroughCreateInfoFB xrPassthroughCI = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };
		xrPassthroughCI.flags = 0;
		XrResult xrResult = xrCreatePassthroughFB( session, &xrPassthroughCI, &m_fbPassthrough );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error - Unable to create fb passthrough: %s", XrEnumToString( xrResult ) );

		flagSupportedLayerTypes.Set( static_cast< int >( ELayerType::FULLSCREEN ) );
		return xrResult;
	}

	XrResult CPassthrough::Init( XrSession session, CInstance *pInstance, XrPassthroughFlagsFB flags, void *pOtherInfo )
	{
		m_pInstance = pInstance;

		// Create passthrough objects
		XrPassthroughCreateInfoFB xrPassthroughCI = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };
		xrPassthroughCI.flags = flags;
		xrPassthroughCI.next = pOtherInfo;
		XrResult xrResult = xrCreatePassthroughFB( session, &xrPassthroughCI, &m_fbPassthrough );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error - Unable to create fb passthrough: %s", XrEnumToString( xrResult ) );

		flagSupportedLayerTypes.Set( static_cast< int >( flags ) );
		return xrResult;
	}

	bool CPassthrough::BSystemSupportsPassthrough() 
	{ 
		XrSystemPassthroughProperties2FB passthroughSystemProperties { XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES2_FB };
		XrSystemProperties systemProperties { XR_TYPE_SYSTEM_PROPERTIES, &passthroughSystemProperties };

		m_pInstance->GetXrSystemProperties( true, &systemProperties );
		return ( passthroughSystemProperties.capabilities & XR_PASSTHROUGH_CAPABILITY_BIT_FB ) == XR_PASSTHROUGH_CAPABILITY_BIT_FB;	
	}

	bool CPassthrough::BSystemSupportsColorPassthrough() 
	{ 
		XrSystemPassthroughProperties2FB passthroughSystemProperties { XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES2_FB };
		XrSystemProperties systemProperties { XR_TYPE_SYSTEM_PROPERTIES, &passthroughSystemProperties };

		m_pInstance->GetXrSystemProperties( true, &systemProperties );
		return ( passthroughSystemProperties.capabilities & XR_PASSTHROUGH_CAPABILITY_COLOR_BIT_FB ) == XR_PASSTHROUGH_CAPABILITY_COLOR_BIT_FB;			
	}

	XrResult CPassthrough::AddLayer( 
		XrSession session, 
		ELayerType eLayerType, 
		XrCompositionLayerFlags flags,
		XrFlags64 layerFlags,
		float fOpacity, 
		XrSpace xrSpace, 
		void *pOtherInfo ) 
	{ 
		assert( session != XR_NULL_HANDLE );
		assert( m_fbPassthrough );

		// Define layer
		XrPassthroughLayerCreateInfoFB xrPassthroughLayerCI = { XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB };
		xrPassthroughLayerCI.passthrough = m_fbPassthrough;
        xrPassthroughLayerCI.flags = layerFlags;

		switch ( eLayerType )
		{
			case xrlib::ExtBase_Passthrough::ELayerType::FULLSCREEN:
				xrPassthroughLayerCI.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
				break;
			case xrlib::ExtBase_Passthrough::ELayerType::MESH_PROJECTION:
				xrPassthroughLayerCI.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_PROJECTED_FB;
				break;
			default:
				xrPassthroughLayerCI.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_MAX_ENUM_FB;
				break;
		}

		// Create  layer
		m_vecPassthroughLayers.push_back( { XR_NULL_HANDLE, { XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB }, { XR_TYPE_PASSTHROUGH_STYLE_FB } } );
		XrResult xrResult = xrCreatePassthroughLayerFB( session, &xrPassthroughLayerCI, &m_vecPassthroughLayers.back().layer );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			m_vecPassthroughLayers.pop_back();
			LogError( GetName(), "Error - unable to create requested passthrough layer of type (%i): %s", uint32_t( xrPassthroughLayerCI.purpose ), XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Set composition layer parameters
		m_vecPassthroughLayers.back().composition.layerHandle = m_vecPassthroughLayers.back().layer;
		m_vecPassthroughLayers.back().composition.flags = flags;
		m_vecPassthroughLayers.back().composition.space = xrSpace;

		// Set style parameters
		m_vecPassthroughLayers.back().style.textureOpacityFactor = fOpacity;

		xrResult = xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers.back().layer, &m_vecPassthroughLayers.back().style );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error trying to set opacity to layer: %s", XrEnumToString( xrResult ) );

		return xrResult; 
	}

	XrResult CPassthrough::RemoveLayer( uint32_t unIndex ) 
	{ 
		assert( m_fbPassthrough );
		assert( unIndex < m_vecPassthroughLayers.size() );

		// delete layer
		if ( m_vecPassthroughLayers[ unIndex ].layer != XR_NULL_HANDLE )
			XR_RETURN_ON_ERROR ( xrDestroyPassthroughLayerFB( m_vecPassthroughLayers[ unIndex ].layer ) );

		// delete entry from vector
		m_vecPassthroughLayers.erase( m_vecPassthroughLayers.begin() + unIndex );
		m_vecPassthroughLayers.shrink_to_fit();

		return XR_SUCCESS; 
	}

	XrResult CPassthrough::Start()
	{
		assert( m_fbPassthrough );

		if ( IsActive() || m_vecPassthroughLayers.empty() )
			return XR_SUCCESS;

		// start passthrough
		XrResult xrResult = xrPassthroughStartFB( m_fbPassthrough );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( GetName(), "Error - Unable to start passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		m_bIsActive = true;
		return XR_SUCCESS;
	}

	XrResult CPassthrough::Stop()
	{
		if ( !IsActive() || m_vecPassthroughLayers.empty() )
			return XR_SUCCESS;
	
		// stop passthrough
		XrResult xrResult = xrPassthroughPauseFB( m_fbPassthrough );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( GetName(), "Error - Unable to stop passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		m_bIsActive = false;
		return XR_SUCCESS;
	}

	XrResult CPassthrough::PauseLayer( int index ) 
	{ 
		assert( m_fbPassthrough );
        assert( index < (int) m_vecPassthroughLayers.size() );

		if ( !IsActive() )
			return XR_SUCCESS;

		if ( index < 0 )
		{
			for ( auto &layer: m_vecPassthroughLayers )
				XR_RETURN_ON_ERROR( xrPassthroughLayerPauseFB( layer.layer ) );
		}
		else
		{
			XR_RETURN_ON_ERROR( xrPassthroughLayerPauseFB( m_vecPassthroughLayers[ index ].layer ) );
		}

		return XR_SUCCESS; 
	}

	XrResult CPassthrough::ResumeLayer( int index ) 
	{ 
		assert( m_fbPassthrough );
		assert( index < (int) m_vecPassthroughLayers.size() );

		if ( !IsActive() )
			XR_RETURN_ON_ERROR( Start() );

		if ( index < 0 )
		{
			for ( auto &layer : m_vecPassthroughLayers )
                XR_RETURN_ON_ERROR( xrPassthroughLayerResumeFB( layer.layer ) );
		}
		else
		{
			XR_RETURN_ON_ERROR( xrPassthroughLayerResumeFB( m_vecPassthroughLayers[ index ].layer ) );
		}

		return XR_SUCCESS; 
	}

	void CPassthrough::GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers, bool bReset ) 
	{
		if ( bReset )
			outCompositionLayers.clear();

		for ( auto& layer : m_vecPassthroughLayers )
		{
			outCompositionLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( &layer.composition ) );
		}
	}


	void CPassthrough::GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers, std::vector< uint32_t > &vecIndices, bool bReset )
	{
		if ( bReset )
			outCompositionLayers.clear();
		for ( auto index : vecIndices )
		{
			outCompositionLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( &( m_vecPassthroughLayers.at( index ).composition ) ) );
		}
	}

	XrResult CPassthrough::SetPassThroughOpacity( SPassthroughLayer &refLayer, float fOpacity )
	{
		refLayer.style.textureOpacityFactor = fOpacity;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( refLayer.layer, &refLayer.style );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error changing passthrough parameter - opacity: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult CPassthrough::SetPassThroughEdgeColor( SPassthroughLayer &refLayer, XrColor4f xrEdgeColor )
	{
		refLayer.style.edgeColor = xrEdgeColor;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( refLayer.layer, &refLayer.style );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error changing passthrough parameter - edge color: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult CPassthrough::SetPassThroughParams( SPassthroughLayer &refLayer, float fOpacity, XrColor4f xrEdgeColor )
	{
		refLayer.style.textureOpacityFactor = fOpacity;
		refLayer.style.edgeColor = xrEdgeColor;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( refLayer.layer, &refLayer.style );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error changing passthrough parameters: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult CPassthrough::SetStyleToMono( int index, float fOpacity )
	{
		assert( m_fbPassthrough );
		assert( m_vecPassthroughLayers.size() > index );

		// Start passthrough if inactive
		if ( !IsActive() )
			XR_RETURN_ON_ERROR( Start() )

		// Create mono color map
		XrPassthroughColorMapMonoToMonoFB colorMap_Mono = { XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_MONO_FB };
		for ( int i = 0; i < XR_PASSTHROUGH_COLOR_MAP_MONO_SIZE_FB; ++i )
		{
			uint8_t colorMono = i;
			colorMap_Mono.textureColorMap[ i ] = colorMono;
		}

		// Set style to mono and resume layer 
		if ( index < 0 )
		{
			for ( uint32_t i = 0; i < m_vecPassthroughLayers.size(); i++ )
			{
				XR_RETURN_ON_ERROR( ResumeLayer( i ) );
				m_vecPassthroughLayers[ i ].style.textureOpacityFactor = fOpacity;
				m_vecPassthroughLayers[ i ].style.next = &colorMap_Mono;
				XR_RETURN_ON_ERROR( xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers[ i ].layer, &m_vecPassthroughLayers[ i ].style ) );
			}
		}
		else
		{
			XR_RETURN_ON_ERROR( ResumeLayer( index ) );
			m_vecPassthroughLayers[ index ].style.textureOpacityFactor = fOpacity;
			m_vecPassthroughLayers[ index ].style.next = &colorMap_Mono;
			XR_RETURN_ON_ERROR( xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers[ index ].layer, &m_vecPassthroughLayers[ index ].style ) );
		}

		return XR_SUCCESS;
	}

	XrResult CPassthrough::SetStyleToColorMap( int index, bool bRed, bool bGreen, bool bBlue, float fAlpha, float fOpacity )
	{
		assert( m_fbPassthrough );
		assert( m_vecPassthroughLayers.size() > index );

		// Start passthrough if inactive
		if ( !IsActive() )
			XR_RETURN_ON_ERROR( Start() )

		// Create color map
		XrPassthroughColorMapMonoToRgbaFB colorMap_RGBA = { XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_RGBA_FB };
		for ( int i = 0; i < XR_PASSTHROUGH_COLOR_MAP_MONO_SIZE_FB; ++i )
		{
			float color_rgb = i / 255.0f;
			colorMap_RGBA.textureColorMap[ i ] = { bRed ? color_rgb : 0.0f, bGreen ? color_rgb : 0.0f, bBlue ? color_rgb : 0.0f, fAlpha };
		}

		// Set style to color map and resume layer
		if ( index < 0 )
		{
			for ( uint32_t i = 0; i < m_vecPassthroughLayers.size(); i++ )
			{
				XR_RETURN_ON_ERROR( ResumeLayer( i ) );
				m_vecPassthroughLayers[ i ].style.textureOpacityFactor = fOpacity;
				m_vecPassthroughLayers[ i ].style.next = &colorMap_RGBA;
				XR_RETURN_ON_ERROR( xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers[ i ].layer, &m_vecPassthroughLayers[ i ].style ) );
			}
		}
		else
		{
			XR_RETURN_ON_ERROR( ResumeLayer( index ) );
			m_vecPassthroughLayers[ index ].style.textureOpacityFactor = fOpacity;
			m_vecPassthroughLayers[ index ].style.next = &colorMap_RGBA;
			XR_RETURN_ON_ERROR( xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers[ index ].layer, &m_vecPassthroughLayers[ index ].style ) );
		}

		return XR_SUCCESS;
	}

	XrResult CPassthrough::SetBCS( int index, float fOpacity, float fBrightness, float fContrast, float fSaturation )
	{
		assert( m_fbPassthrough );
		assert( m_vecPassthroughLayers.size() > index );

		// Start passthrough if inactive
		if ( !IsActive() )
			XR_RETURN_ON_ERROR( Start() )

		// Create mono color map
		// BCS - Brightness, Contrast and Saturation channels
		XrPassthroughBrightnessContrastSaturationFB bcs = { XR_TYPE_PASSTHROUGH_BRIGHTNESS_CONTRAST_SATURATION_FB };
		bcs.brightness = fBrightness;
		bcs.contrast = fContrast;
		bcs.saturation = fSaturation;

		// Set style to mono and resume layer
		if ( index < 0 )
		{
			for ( uint32_t i = 0; i < m_vecPassthroughLayers.size(); i++ )
			{
				XR_RETURN_ON_ERROR( ResumeLayer( i ) );
				m_vecPassthroughLayers[ i ].style.textureOpacityFactor = fOpacity;
				m_vecPassthroughLayers[ i ].style.next = &bcs;
				XR_RETURN_ON_ERROR( xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers[ i ].layer, &m_vecPassthroughLayers[ i ].style ) );
			}
		}
		else
		{
			XR_RETURN_ON_ERROR( ResumeLayer( index ) );
			m_vecPassthroughLayers[ index ].style.textureOpacityFactor = fOpacity;
			m_vecPassthroughLayers[ index ].style.next = &bcs;
			XR_RETURN_ON_ERROR( xrPassthroughLayerSetStyleFB( m_vecPassthroughLayers[ index ].layer, &m_vecPassthroughLayers[ index ].style ) );
		}

		return XR_SUCCESS;
	}

	void CPassthrough::SetTriangleMesh( CTriangleMesh *pTriangleMesh ) 
	{ 
		if ( pTriangleMesh )
		{
			m_pTriangleMesh = pTriangleMesh;
			flagSupportedLayerTypes.Set( (int) ELayerType::MESH_PROJECTION );
			return;
		}

		flagSupportedLayerTypes.Reset( (int) ELayerType::MESH_PROJECTION );
	}

	bool CPassthrough::CreateTriangleMesh( CInstance *pInstance ) 
	{ 
		for ( auto &ext : pInstance->GetEnabledExtensions() )
		{
			if ( ext == XR_FB_TRIANGLE_MESH_EXTENSION_NAME )
			{
				SetTriangleMesh( new CTriangleMesh( pInstance->GetXrInstance() ) );
				flagSupportedLayerTypes.Set( (int) ELayerType::MESH_PROJECTION );
				return true;
			}
		}

		return false;
	}

	XrResult CPassthrough::AddGeometry( 
		XrSession session,
		XrPassthroughLayerFB &layer, 
		std::vector< XrVector3f > &vecVertices, 
		std::vector< uint32_t > &vecIndices, 
		XrSpace baseSpace, 
		XrTriangleMeshFlagsFB triFlags,
		XrPosef xrPose, 
		XrVector3f xrScale )
	{
		assert( IsTriangleMeshSupported() );
		XR_RETURN_ON_ERROR( GetTriangleMesh()->AddGeometry( session, layer, vecVertices, vecIndices, triFlags ) );

		XrGeometryInstanceFB xrGeometryInstance = XR_NULL_HANDLE;
		XrGeometryInstanceCreateInfoFB xrGeomInstanceCI = { XR_TYPE_GEOMETRY_INSTANCE_CREATE_INFO_FB };
		xrGeomInstanceCI.layer = layer;
		xrGeomInstanceCI.mesh = GetTriangleMesh()->GetMeshes()->back();
		xrGeomInstanceCI.baseSpace = baseSpace;
		xrGeomInstanceCI.pose = xrPose;
		xrGeomInstanceCI.scale = xrScale;
		XrResult xrResult = xrCreateGeometryInstanceFB( session, &xrGeomInstanceCI, &xrGeometryInstance );

		if ( !XR_UNQUALIFIED_SUCCESS ( xrResult ) )
		{
			XR_RETURN_ON_ERROR( GetTriangleMesh()->RemoveGeometry( GetTriangleMesh()->GetMeshes()->size() - 1 ) );
			return xrResult;
		}
		
		// add geometry instance to cache
		GetGeometryInstances()->push_back( xrGeometryInstance );

		return XR_SUCCESS;
	}

	XrResult CPassthrough::UpdateGeometry( XrGeometryInstanceFB xrGeomInstance, XrSpace baseSpace, XrTime xrTime, XrPosef xrPose, XrVector3f xrScale ) 
	{ 
		XrGeometryInstanceTransformFB xrGeomInstTransform = { XR_TYPE_GEOMETRY_INSTANCE_TRANSFORM_FB };
		xrGeomInstTransform.baseSpace = baseSpace;
		xrGeomInstTransform.time = xrTime;
		xrGeomInstTransform.pose = xrPose;
		xrGeomInstTransform.scale = xrScale;

		XR_RETURN_ON_ERROR( xrGeometryInstanceSetTransformFB( xrGeomInstance, &xrGeomInstTransform ) );

		return XR_SUCCESS; 
	}

} 
