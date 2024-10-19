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

#include <xrlib/ext/FB/passthrough.hpp>
#include <xrlib/log.hpp>

namespace xrlib::FB
{

	Passthrough::Passthrough( XrInstance xrInstance )
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

	Passthrough::~Passthrough()
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

	XrResult Passthrough::Init( XrSession session, CInstance *pInstance, void *pOtherInfo )
	{
		// Get flags if any
		XrPassthroughFlagsFB flags = 0;
		if ( pOtherInfo )
		{
			flags = *(static_cast< XrPassthroughFlagsFB * >( pOtherInfo ));
		}

		// Create passthrough objects
		XrPassthroughCreateInfoFB xrPassthroughCI = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };
		xrPassthroughCI.flags = flags;
		XrResult xrResult = xrCreatePassthroughFB( session, &xrPassthroughCI, &m_fbPassthrough );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error - Unable to create fb passthrough: %s", XrEnumToString( xrResult ) );

		flagSupportedLayerTypes.Set( (int) ELayerType::FULLSCREEN );
		return xrResult;
	}

	XrResult Passthrough::AddLayer( XrSession session, ELayerType eLayerType, XrCompositionLayerFlags flags, 
		float fOpacity, XrSpace xrSpace, void *pOtherInfo ) 
	{ 
		assert( session != XR_NULL_HANDLE );
		assert( m_fbPassthrough );

		// Define layer
		XrPassthroughLayerCreateInfoFB xrPassthroughLayerCI = { XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB };
		xrPassthroughLayerCI.passthrough = m_fbPassthrough;

		switch ( eLayerType )
		{
			case xrlib::ExtBase_Passthrough::ELayerType::FULLSCREEN:
				xrPassthroughLayerCI.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
				break;
			case xrlib::ExtBase_Passthrough::ELayerType::MESH_PROJECTION:
				xrPassthroughLayerCI.purpose = IsTriangleMeshSupported() ? XR_PASSTHROUGH_LAYER_PURPOSE_PROJECTED_FB : XR_PASSTHROUGH_LAYER_PURPOSE_MAX_ENUM_FB;
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
			LogError( GetName(), "Error - unable to create requested passthrough layer of type (%i): %s", uint32_t ( eLayerType ), XrEnumToString( xrResult ) );
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

	XrResult Passthrough::RemoveLayer( uint32_t unIndex ) 
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

	XrResult Passthrough::Start()
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

	XrResult Passthrough::Stop()
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

	XrResult Passthrough::PauseLayer( int index ) 
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

	XrResult Passthrough::ResumeLayer( int index ) 
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

	void Passthrough::GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers ) 
	{
		for ( auto& layer : m_vecPassthroughLayers )
		{
			outCompositionLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( &layer.composition ) );
		}
	}

	XrResult Passthrough::SetPassThroughOpacity( PassthroughLayer &refLayer, float fOpacity )
	{
		refLayer.style.textureOpacityFactor = fOpacity;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( refLayer.layer, &refLayer.style );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error changing passthrough parameter - opacity: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult Passthrough::SetPassThroughEdgeColor( PassthroughLayer &refLayer, XrColor4f xrEdgeColor )
	{
		refLayer.style.edgeColor = xrEdgeColor;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( refLayer.layer, &refLayer.style );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error changing passthrough parameter - edge color: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult Passthrough::SetPassThroughParams( PassthroughLayer &refLayer, float fOpacity, XrColor4f xrEdgeColor )
	{
		refLayer.style.textureOpacityFactor = fOpacity;
		refLayer.style.edgeColor = xrEdgeColor;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( refLayer.layer, &refLayer.style );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error changing passthrough parameters: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult Passthrough::SetStyleToMono( int index, float fOpacity )
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

	XrResult Passthrough::SetStyleToColorMap( int index, bool bRed, bool bGreen, bool bBlue, float fAlpha, float fOpacity )
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

	XrResult Passthrough::SetBCS( int index, float fOpacity, float fBrightness, float fContrast, float fSaturation )
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

	void Passthrough::SetTriangleMesh( TriangleMesh *pTriangleMesh ) 
	{ 
		if ( pTriangleMesh )
		{
			m_pTriangleMesh = pTriangleMesh;
			flagSupportedLayerTypes.Set( (int) ELayerType::MESH_PROJECTION );
			return;
		}

		flagSupportedLayerTypes.Reset( (int) ELayerType::MESH_PROJECTION );
	}

	bool Passthrough::CreateTriangleMesh( CInstance *pInstance ) 
	{ 
		for ( auto &ext : pInstance->GetEnabledExtensions() )
		{
			if ( ext == XR_FB_TRIANGLE_MESH_EXTENSION_NAME )
			{
				SetTriangleMesh( new TriangleMesh( pInstance->GetXrInstance() ) );
				flagSupportedLayerTypes.Set( (int) ELayerType::MESH_PROJECTION );
				return true;
			}
		}

		return false;
	}

	XrResult Passthrough::AddGeometry( 
		XrSession session,
		XrPassthroughLayerFB &layer, 
		std::vector< XrVector3f > &vecVertices, 
		std::vector< uint32_t > &vecIndices, 
		XrSpace xrSpace, 
		XrPosef xrPose, 
		XrVector3f xrScale )
	{
		assert( IsTriangleMeshSupported() );
		XR_RETURN_ON_ERROR( GetTriangleMesh()->AddGeometry( session, layer, vecVertices, vecIndices, xrSpace, xrPose, xrScale ) );

		XrGeometryInstanceFB xrGeometryInstance = XR_NULL_HANDLE;
		XrGeometryInstanceCreateInfoFB xrGeomInstanceCI = { XR_TYPE_GEOMETRY_INSTANCE_CREATE_INFO_FB };
		xrGeomInstanceCI.layer = layer;
		xrGeomInstanceCI.mesh = GetTriangleMesh()->GetMeshes()->back();
		xrGeomInstanceCI.baseSpace = xrSpace;
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

	XrResult Passthrough::UpdateGeometry( XrGeometryInstanceFB xrGeomInstance, XrSpace xrSpace, XrTime xrTime, XrPosef xrPose, XrVector3f xrScale ) 
	{ 
		XrGeometryInstanceTransformFB xrGeomInstTransform = { XR_TYPE_GEOMETRY_INSTANCE_TRANSFORM_FB };
		xrGeomInstTransform.baseSpace = xrSpace;
		xrGeomInstTransform.time = xrTime;
		xrGeomInstTransform.pose = xrPose;
		xrGeomInstTransform.scale = xrScale;

		XR_RETURN_ON_ERROR( xrGeometryInstanceSetTransformFB( xrGeomInstance, &xrGeomInstTransform ) );

		return XR_SUCCESS; 
	}

} 
