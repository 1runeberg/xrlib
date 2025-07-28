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



#include <ranges>
#include <xrlib/instance.hpp>
#include <xrlib/utility_functions.hpp>

namespace xrlib
{
	#ifdef XR_USE_PLATFORM_ANDROID 
	CInstance::CInstance( struct android_app *pAndroidApp, const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel ):
		m_pAndroidApp( pAndroidApp ),
		m_sAppName( sAppName ),
		m_unAppVersion( unAppVersion ),
		m_eMinLogLevel( eMinLogLevel )
	#else
	CInstance::CInstance( const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel ) : 
		m_sAppName( sAppName), 
		m_unAppVersion( unAppVersion ),
		m_eMinLogLevel( eMinLogLevel )
	#endif
	{
		if ( sAppName.empty() )
		{
			m_sAppName = "XrApp";
			LogWarning( "", "No application name provided. Was set to: %s", m_sAppName.c_str() );
		}
		else if ( sAppName.size() > XR_MAX_APPLICATION_NAME_SIZE )
		{
			m_sAppName = sAppName.substr( 0, XR_MAX_APPLICATION_NAME_SIZE );
			LogWarning( "", "Provided application name is too long. Truncated to: %s", m_sAppName.c_str() );
		}
	}

	CInstance::~CInstance() 
	{
		if ( m_xrInstance != XR_NULL_HANDLE )
			xrDestroyInstance( m_xrInstance );

		#ifdef XR_USE_PLATFORM_ANDROID
		JNIEnv *env = nullptr;
		if ( m_pAndroidApp && m_pAndroidApp->activity && m_pAndroidApp->activity->vm )
		{
			if ( m_pAndroidApp->activity->vm->GetEnv( reinterpret_cast< void ** >( &env ), JNI_VERSION_1_6 ) == JNI_OK )
			{
				m_pAndroidApp->activity->vm->DetachCurrentThread();
			}
		}
#endif
	}

	XrResult CInstance::Init(
		std::vector< const char * > &vecInstanceExtensions,
		std::vector< const char * > &vecAPILayers,
		const XrInstanceCreateFlags createFlags,
		const void *pNext ) 
	{ 
		#ifdef XR_USE_PLATFORM_ANDROID
		// For Android, we need to initialize the android openxr loader
		XR_RETURN_ON_ERROR( InitAndroidLoader() );
		#endif

		XrResult xrResult = XR_ERROR_INITIALIZATION_FAILED;

		// Set openxr instance info
		XrInstanceCreateInfo instanceCI { XR_TYPE_INSTANCE_CREATE_INFO };
		instanceCI.next = pNext;
		instanceCI.createFlags = createFlags;
		instanceCI.applicationInfo = {};

		if ( !StringCopy(instanceCI.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, m_sAppName ) )
			return xrResult;

		if ( !StringCopy( instanceCI.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, XRLIB_NAME ) )
			return xrResult;

		instanceCI.applicationInfo.applicationVersion = m_unAppVersion;
		instanceCI.applicationInfo.engineVersion = XR_MAKE_VERSION32( XRLIB_VERSION_MAJOR, XRLIB_VERSION_MINOR, XRLIB_VERSION_PATCH );
		instanceCI.applicationInfo.apiVersion = XR_MAKE_VERSION( 1, 0, 0 ); // XR_CURRENT_API_VERSION;

		// Retrieve the supported extensions by the runtime
		std::vector< XrExtensionProperties > vecExtensionProperties;
		if ( !XR_SUCCEEDED( GetSupportedExtensions( vecExtensionProperties ) ) )
		{
			return XR_ERROR_RUNTIME_UNAVAILABLE;
		}

		if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			LogVerbose( "", "This runtime supports %i available extensions:", vecExtensionProperties.size() );

		// Remove any unsupported graphics api extensions from requested extensions (only Vulkan is supported)
		RemoveUnsupportedGraphicsApis( vecInstanceExtensions );
		RemoveUnsupportedExtensions( vecInstanceExtensions );

		// Get the supported extension names by the runtime so we can check for vulkan support
		std::vector< std::string > vecSupportedExtensionNames;
		GetExtensionNames( vecSupportedExtensionNames, vecExtensionProperties );

		// Add extensions to create instance info
		instanceCI.enabledExtensionCount = (uint32_t) vecInstanceExtensions.size();
		instanceCI.enabledExtensionNames = vecInstanceExtensions.data();

		// Cache enabled extensions
		for ( auto &xrExtensionProperty : vecExtensionProperties )
		{
			std::string sExtensionName = std::string( xrExtensionProperty.extensionName );
			bool bFound = std::find( vecInstanceExtensions.begin(), vecInstanceExtensions.end(), sExtensionName ) != vecInstanceExtensions.end();

			if ( bFound )
			{
				m_vecEnabledExtensions.push_back( sExtensionName );
			}

			if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			{
				std::string sEnableTag = bFound ? "[WILL ENABLE]" : "";
				LogVerbose( "", "\t%s (ver. %i) %s", &xrExtensionProperty.extensionName, xrExtensionProperty.extensionVersion, sEnableTag.c_str() );
			}
		}

		// Retrieve the supported api layers by the runtime
		std::vector< XrApiLayerProperties > vecApiLayerProperties;
		if ( !XR_SUCCEEDED( GetSupportedApiLayers( vecApiLayerProperties ) ) )
		{
			return XR_ERROR_RUNTIME_UNAVAILABLE;
		}

		if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			LogVerbose( "", "There are %i openxr api layers available:", vecApiLayerProperties.size() );

		// Prepare app requested api layers
		RemoveUnsupportedApiLayers( vecAPILayers );

		// Cache enabled api layers
		for ( auto &xrApiLayerProperty : vecApiLayerProperties )
		{
			std::string sApiLayerName = std::string( xrApiLayerProperty.layerName );
			bool bFound = std::find( vecAPILayers.begin(), vecAPILayers.end(), sApiLayerName ) != vecAPILayers.end();

			if ( bFound )
			{
				m_vecEnabledApiLayers.push_back( sApiLayerName );
			}

			if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			{
				std::string sEnableTag = bFound ? "[WILL ENABLE]" : "";

				LogVerbose( "", "\t%s (ver. %i) %s", xrApiLayerProperty.layerName, xrApiLayerProperty.specVersion, sEnableTag.c_str() );
				LogVerbose( "", "\t\t%s\n", xrApiLayerProperty.description );
			}
		}

		// Add api layers to create instance info
		instanceCI.enabledApiLayerCount = (uint32_t) vecAPILayers.size();
		instanceCI.enabledApiLayerNames = vecAPILayers.data();

		// Create openxr instance
		xrResult = xrCreateInstance( &instanceCI, &m_xrInstance );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "", "Error creating openxr instance: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}
		
		if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			LogVerbose( "", "OpenXR instance created. Handle (%" PRIu64 ")", ( uint64_t ) m_xrInstance );


		// Get instance properties
		xrResult = xrGetInstanceProperties( m_xrInstance, &m_xrInstanceProperties );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "", "Error getting active openxr instance properties (%i)", xrResult );
			return xrResult;
		}

		if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			LogVerbose(
				"",
				"OpenXR runtime %s version %i.%i.%i is now active for this instance.",
				m_xrInstanceProperties.runtimeName,
				XR_VERSION_MAJOR( m_xrInstanceProperties.runtimeVersion ),
				XR_VERSION_MINOR( m_xrInstanceProperties.runtimeVersion ),
				XR_VERSION_PATCH( m_xrInstanceProperties.runtimeVersion ) );

		// Get user's system info
		XrSystemGetInfo xrSystemGetInfo { XR_TYPE_SYSTEM_GET_INFO };
		xrSystemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

		xrResult = xrGetSystem( m_xrInstance, &xrSystemGetInfo, &m_xrSystemId );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "", "Error getting user's system id (%s)", XrEnumToString( xrResult ) );
			return xrResult;
		}

		xrResult = xrGetSystemProperties( m_xrInstance, m_xrSystemId, &m_xrSystemProperties );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "", "Error getting user's system info (%s)", XrEnumToString( xrResult ) );
			return xrResult;
		}

		xrGetSystemProperties( m_xrInstance, m_xrSystemId, &m_xrSystemProperties );
		if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
			LogVerbose( "", "Active tracking system is %s (Vendor Id %" PRIu32")", &m_xrSystemProperties.systemName[0], m_xrSystemProperties.vendorId );

		// Show supported view configurations
		if ( CheckLogLevelVerbose( m_eMinLogLevel ) )
		{
			auto vecSupportedViewConfigurations = GetSupportedViewConfigurations();

			LogVerbose( "", "This runtime supports %i view configuration(s):", vecSupportedViewConfigurations.size() );
			for ( auto &xrViewConfigurationType : vecSupportedViewConfigurations )
			{
				LogVerbose( "", "\t%s", XrEnumToString( xrViewConfigurationType ) );
			}
		}

		#ifdef XR_USE_PLATFORM_ANDROID
		// For android, we need to pass the android state management function to the xrProvider
		m_pAndroidApp->onAppCmd = app_handle_cmd;
		#endif

		return xrResult; 
	}

	bool CInstance::IsApiLayerEnabled( std::string &sApiLayerName ) 
	{ 
		return FindStringInVector( m_vecEnabledApiLayers, sApiLayerName ); 
	}

	XrResult CInstance::GetSupportedApiLayers( std::vector< XrApiLayerProperties > &vecApiLayers ) 
	{ 
		uint32_t unCount = 0;
		XrResult xrResult = xrEnumerateApiLayerProperties( 0, &unCount, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			if ( unCount == 0 )
				return XR_SUCCESS;

			vecApiLayers.clear();
			for ( uint32_t i = 0; i < unCount; ++i )
			{
				vecApiLayers.push_back( XrApiLayerProperties { XR_TYPE_API_LAYER_PROPERTIES, nullptr } );
			}

			return xrEnumerateApiLayerProperties( unCount, &unCount, vecApiLayers.data() );
		}

		return xrResult;
	}

	XrResult CInstance::GetSupportedApiLayers( std::vector< std::string > &vecApiLayers )
	{
		std::vector< XrApiLayerProperties > vecApiLayerProperties;
		XrResult xrResult = GetSupportedApiLayers( vecApiLayerProperties );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			GetApilayerNames( vecApiLayers, vecApiLayerProperties );
		}

		return xrResult;
	}

	bool CInstance::IsExtensionEnabled( const char *extensionName ) 
	{ 
		std::string sName = extensionName;
		return IsExtensionEnabled( sName ); 
	}

	bool CInstance::IsExtensionEnabled( std::string &sExtensionName ) 
	{ 
		return FindStringInVector( m_vecEnabledExtensions, sExtensionName ); 
	}

	XrResult CInstance::GetSupportedExtensions( std::vector< XrExtensionProperties > &outExtensions, const char *pcharApiLayerName ) 
	{ 
		uint32_t unCount = 0;
		XrResult xrResult = xrEnumerateInstanceExtensionProperties( pcharApiLayerName, 0, &unCount, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			if ( unCount == 0 )
				return XR_SUCCESS;

			outExtensions.clear();
			for ( uint32_t i = 0; i < unCount; ++i )
			{
				outExtensions.push_back( XrExtensionProperties { XR_TYPE_EXTENSION_PROPERTIES, nullptr } );
			}

			return xrEnumerateInstanceExtensionProperties( pcharApiLayerName, unCount, &unCount, outExtensions.data() );
		}

		return xrResult;
	}

	XrResult CInstance::GetSupportedExtensions( std::vector< std::string > &outExtensions, const char *pcharApiLayerName )
	{
		std::vector< XrExtensionProperties > vecExtensionProperties;
		XrResult xrResult = GetSupportedExtensions( vecExtensionProperties );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			GetExtensionNames( outExtensions, vecExtensionProperties );
		}

		return xrResult;
	}

	XrResult CInstance::RemoveUnsupportedExtensions( std::vector< std::string > &vecRequestedExtensionNames ) 
	{ 
		// Get supported extensions from active runtime
		std::vector< std::string > vecSupportedExtensions;
		XrResult xrResult = GetSupportedExtensions( vecSupportedExtensions );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			for ( std::vector< std::string >::iterator it = vecRequestedExtensionNames.begin(); it != vecRequestedExtensionNames.end(); )
			{
				// Find requested extension is the runtime's list of supported extensions
				bool bFound = false;
				for ( auto &sExtensionName : vecSupportedExtensions )
				{
					if ( sExtensionName == *it )
					{
						bFound = true;
						break;
					}
				}

				// If the requested extension isn't found, delete it
				if ( !bFound )
				{
					vecRequestedExtensionNames.erase( it );
				}
			}

			// shrink the requested extension vector in case we needed to delete values from it
			vecRequestedExtensionNames.shrink_to_fit();
		}

		return xrResult;
	}

	XrResult CInstance::RemoveUnsupportedExtensions( std::vector< const char * > &vecExtensionNames ) 
	{ 
		std::vector< XrExtensionProperties > vecSupportedExtensions;
		XR_RETURN_ON_ERROR( GetSupportedExtensions( vecSupportedExtensions ) )

		for ( auto it = vecExtensionNames.begin(); it != vecExtensionNames.end(); it++ )
		{
			// Check if our requested extension exists
			bool bFound = false;
			for ( auto &supportedExt : vecSupportedExtensions )
			{
				if ( std::strcmp( supportedExt.extensionName, *it ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			// If not supported by the current openxr runtime, then remove it from our list
			if ( !bFound )
			{
				vecExtensionNames.erase( it-- );
			}
		}

		return XR_SUCCESS;
	}

	XrResult CInstance::RemoveUnsupportedApiLayers( std::vector< const char * > &vecApiLayerNames ) 
	{ 
		std::vector< XrApiLayerProperties > vecSupportedApiLayers;
		XR_RETURN_ON_ERROR( GetSupportedApiLayers( vecSupportedApiLayers ) )

		for ( auto it = vecApiLayerNames.begin(); it != vecApiLayerNames.end(); it++ )
		{
			// Check if our requested extension exists
			bool bFound = false;
			for ( auto &supportedApiLayer : vecSupportedApiLayers )
			{
				if ( std::strcmp( supportedApiLayer.layerName, *it ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			// If not supported by the current openxr runtime, then remove it from our list
			if ( !bFound )
			{
				vecApiLayerNames.erase( it-- );
			}
		}

		return XR_SUCCESS;
	}

	void CInstance::RemoveUnsupportedGraphicsApis( std::vector< const char * > &vecExtensionNames ) 
	{
		// @todo - capture from openxr_platform.h
		std::vector< const char * > vecUnsupportedGraphicsApis { "XR_KHR_opengl_enable", "XR_KHR_opengl_es_enable", "XR_KHR_D3D11_enable", "XR_KHR_D3D12_enable", "XR_MNDX_egl_enable" };

		for ( auto it = vecExtensionNames.begin(); it != vecExtensionNames.end(); it++ )
		{
			// Check if there's an unsupported graphics api in the input vector
			bool bFound = false;
			for ( auto &unsupportedExt : vecUnsupportedGraphicsApis )
			{
				if ( std::strcmp( unsupportedExt, *it ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			// If an unsupported graphics api is found in the list, then remove it
			if ( bFound )
			{
				vecExtensionNames.erase( it-- );
			}
		}
	}

	std::vector< XrViewConfigurationType > CInstance::GetSupportedViewConfigurations() 
	{ 
		// Check if there's a valid openxr instance
		if ( m_xrInstance == XR_NULL_HANDLE )
			return std::vector< XrViewConfigurationType >();

		// Get number of view configurations supported by the runtime
		uint32_t unViewConfigurationsNum = 0;
		XrResult xrResult = xrEnumerateViewConfigurations( m_xrInstance, m_xrSystemId, unViewConfigurationsNum, &unViewConfigurationsNum, nullptr );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			xrlib::LogError( "", "Error getting supported view configuration types from the runtime (%s)", XrEnumToString( xrResult ) );
			return std::vector< XrViewConfigurationType >(); 
		}

		std::vector< XrViewConfigurationType > vecViewConfigurations( unViewConfigurationsNum );
		xrResult = xrEnumerateViewConfigurations( m_xrInstance, m_xrSystemId, unViewConfigurationsNum, &unViewConfigurationsNum, vecViewConfigurations.data() );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return std::vector< XrViewConfigurationType >();

		return vecViewConfigurations;
	}

	void CInstance::GetApilayerNames( std::vector< std::string > &outApiLayerNames, const std::vector< XrApiLayerProperties > &vecApilayerProperties ) 
	{
		for ( auto &apiLayerProperty : vecApilayerProperties )
		{
			outApiLayerNames.push_back( apiLayerProperty.layerName );
		}
	}

	void CInstance::GetExtensionNames( std::vector< std::string > &outExtensionNames, const std::vector< XrExtensionProperties > &vecExtensionProperties ) 
	{
		for ( auto &extensionProperty : vecExtensionProperties )
		{
			outExtensionNames.push_back( extensionProperty.extensionName );
		}
	}

	const XrSystemProperties *CInstance::GetXrSystemProperties( bool bUpdate, void *pNext, bool bResetNextChain )
	{ 
		assert( m_xrInstance != XR_NULL_HANDLE );
		assert( m_xrSystemId != XR_NULL_SYSTEM_ID );

		if ( bUpdate )
		{
			m_xrSystemProperties.next = pNext;
			XrResult xrResult = xrGetSystemProperties( m_xrInstance, m_xrSystemId, &m_xrSystemProperties );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				LogWarning( "", "Error updating user's system info (%s)", XrEnumToString( xrResult ) );
		}

		// Some runtimes appear to reset this to nullptr, so defensively setting it back here.
		m_xrSystemProperties.next =  bResetNextChain ? nullptr : pNext;

		return &m_xrSystemProperties;
	}

	void CInstance::ResetXrSystemPropertyNextChain( bool bClearMemory )
	{
		if ( !bClearMemory )
			m_xrSystemProperties.next = nullptr;

		if( !m_xrSystemProperties.next )
			return;

		// Cache next chain for deletion
		std::vector< XrBaseOutStructure * > nodes;
		auto *pCurrent = reinterpret_cast< XrBaseOutStructure * >( m_xrSystemProperties.next );
		while ( pCurrent )
		{
			nodes.push_back( pCurrent );
			pCurrent = pCurrent->next;
		}

		// Delete nodes from end of the chain
		for ( auto &node : std::ranges::reverse_view( nodes ) )
			delete node;

	}

}
