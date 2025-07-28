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

#include <algorithm>

#include <xrlib/log.hpp>
#include <xrlib/data_types.hpp>

namespace xrlib
{
	static constexpr uint8_t k_Left = 0;
	static constexpr uint8_t k_Right = 1;
 
	class CInstance
	{
	  public:
		#ifdef XR_USE_PLATFORM_ANDROID
			CInstance( struct android_app *pAndroidApp, const std::string &sAppName, XrVersion32 unAppVersion, ELogLevel eMinLogLevel = ELogLevel::LogVerbose );
		#else
			CInstance( const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel = ELogLevel::LogVerbose );
		#endif

		~CInstance();

		XrResult Init( 
			std::vector< const char * > &vecInstanceExtensions, 
			std::vector< const char * > &vecAPILayers,
			XrInstanceCreateFlags createFlags = 0,
			const void *pNext = nullptr	);

		bool IsApiLayerEnabled( std::string &sApiLayerName );

        bool IsExtensionEnabled( const char *extensionName );

        bool IsExtensionEnabled( std::string &sExtensionName );

		XrResult GetSupportedApiLayers( std::vector< XrApiLayerProperties > &vecApiLayers );

		XrResult GetSupportedApiLayers( std::vector< std::string > &vecApiLayers );

		XrResult GetSupportedExtensions( std::vector< XrExtensionProperties > &outExtensions, const char *pcharApiLayerName = nullptr );

		XrResult GetSupportedExtensions( std::vector< std::string > &outExtensions, const char *pcharApiLayerName = nullptr );

		XrResult RemoveUnsupportedExtensions( std::vector< std::string > &vecRequestedExtensionNames );

		XrResult RemoveUnsupportedExtensions( std::vector< const char * > &vecExtensionNames );

		XrResult RemoveUnsupportedApiLayers( std::vector< const char * > &vecApiLayerNames );

		void RemoveUnsupportedGraphicsApis( std::vector< const char * > &vecExtensionNames );

		std::vector< XrViewConfigurationType > GetSupportedViewConfigurations();

		void GetApilayerNames( std::vector< std::string > &outApiLayerNames, const std::vector< XrApiLayerProperties > &vecApilayerProperties );

		void GetExtensionNames( std::vector< std::string > &outExtensionNames, const std::vector< XrExtensionProperties > &vecExtensionProperties );

		const char *GetAppName() { return m_sAppName.c_str(); }

		[[nodiscard]] XrVersion32 GetAppVersion() const { return m_unAppVersion; }

        [[nodiscard]] XrSystemId GetXrSystemId() const { return m_xrSystemId; }

		const XrInstanceProperties *GetXrInstanceProperties() { return &m_xrInstanceProperties; }

		const XrSystemProperties *GetXrSystemProperties( bool bUpdate = false, void *pNext = nullptr, bool bResetNextChain = true );

		void ResetXrSystemPropertyNextChain( bool bClearMemory = false);

		const std::vector< std::string > &GetEnabledApiLayers() { return m_vecEnabledApiLayers; }

		const std::vector< std::string > &GetEnabledExtensions() { return m_vecEnabledExtensions; }

        XrInstance GetXrInstance() { return m_xrInstance; }

        ELogLevel GetMinLogLevel() { return m_eMinLogLevel; }


		#ifdef XR_USE_PLATFORM_ANDROID
		AndroidAppState androidAppState {};

		XrResult InitAndroidLoader( void *pNext = nullptr )
		{
			m_pAndroidApp->activity->vm->AttachCurrentThread( &m_jniEnv, nullptr );
			m_pAndroidApp->userData = &androidAppState;

			PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
			XrResult xrResult = xrGetInstanceProcAddr( XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *) ( &initializeLoader ) );
			if ( XR_SUCCEEDED( xrResult ) )
			{
				XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
				memset( &loaderInitInfoAndroid, 0, sizeof( loaderInitInfoAndroid ) );
				loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
				loaderInitInfoAndroid.next = pNext;
				loaderInitInfoAndroid.applicationVM = m_pAndroidApp->activity->vm;
				loaderInitInfoAndroid.applicationContext = m_pAndroidApp->activity->clazz;
				initializeLoader( (const XrLoaderInitInfoBaseHeaderKHR *) &loaderInitInfoAndroid );
			}

			return xrResult;
		}

		struct android_app *GetAndroidApp() { return m_pAndroidApp; }
		JNIEnv *GetJNIEnv() { return m_jniEnv; }
		AAssetManager *GetAssetManager() { return m_pAndroidApp->activity->assetManager;  }

		private:
		JNIEnv *m_jniEnv = nullptr;
		struct android_app *m_pAndroidApp = nullptr;

		#else
		private:
		#endif

		std::string m_sAppName;
		XrVersion32 m_unAppVersion = 0;
		ELogLevel m_eMinLogLevel = ELogLevel::LogVerbose;

		XrInstance m_xrInstance = XR_NULL_HANDLE;
		XrInstanceProperties m_xrInstanceProperties { XR_TYPE_INSTANCE_PROPERTIES };
		XrSystemId m_xrSystemId = XR_NULL_SYSTEM_ID;
		XrSystemProperties m_xrSystemProperties { XR_TYPE_SYSTEM_PROPERTIES };

		std::vector< std::string > m_vecEnabledApiLayers;
		std::vector< std::string > m_vecEnabledExtensions;
	};

}
