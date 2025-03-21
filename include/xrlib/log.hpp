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
#pragma warning( disable : 4996 ) // windows: warning C4996: 'strncpy'

#include <cstring>
#include <string>
#include <mutex>
#include <stdarg.h>

#ifdef XR_USE_PLATFORM_ANDROID
	#include <android/log.h>
#endif

#include <project_config.h>			// Generated by cmake from project_config.h.in

namespace xrlib
{
	constexpr const char *LOG_CATEGORY_DEFAULT = "xrlib";

	enum class ELogLevel
	{
		LogNone = 0,
		LogError = 1,
		LogWarning = 2,
		LogInfo = 3,
		LogDebug = 4,
		LogVerbose = 5,
		LogEMax
	};

	static const char *GetLogLevelName( ELogLevel eLogLevel )
	{
		switch ( eLogLevel )
		{
			case ELogLevel::LogVerbose:
				return "Verbose";
				break;
			case ELogLevel::LogDebug:
				return "Debug";
				break;
			case ELogLevel::LogInfo:
				return "Info";
				break;
			case ELogLevel::LogWarning:
				return "Warning";
				break;
			case ELogLevel::LogError:
				return "Error";
				break;
			case ELogLevel::LogNone:
			case ELogLevel::LogEMax:
			default:
				break;
		}

		return "None";
	}

	static bool CheckLogLevel( ELogLevel eLogLevel, ELogLevel eMinLogLevel )
	{
		if ( eLogLevel < eMinLogLevel || eLogLevel == ELogLevel::LogNone )
			return false;

		return true;
	}

	static bool CheckLogLevelDebug( ELogLevel eLogLevel ) { return CheckLogLevel( eLogLevel, ELogLevel::LogDebug ); }

	static bool CheckLogLevelVerbose( ELogLevel eLogLevel ) { return CheckLogLevel( eLogLevel, ELogLevel::LogVerbose ); }

	static void Log( ELogLevel eLoglevel, std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		switch ( eLoglevel )
		{
			case xrlib::ELogLevel::LogDebug:
				__android_log_vprint( ANDROID_LOG_DEBUG, pccCategory, pccMessageFormat, pArgs );
				break;
			case xrlib::ELogLevel::LogInfo:
				__android_log_vprint( ANDROID_LOG_INFO, pccCategory, pccMessageFormat, pArgs );
				break;
			case xrlib::ELogLevel::LogWarning:
				__android_log_vprint( ANDROID_LOG_WARN, pccCategory, pccMessageFormat, pArgs );
				break;
			case xrlib::ELogLevel::LogError:
				__android_log_vprint( ANDROID_LOG_ERROR, pccCategory, pccMessageFormat, pArgs );
				break;
			case xrlib::ELogLevel::LogNone:
			case xrlib::ELogLevel::LogEMax:
			default:
				break;
		}
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( eLoglevel ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogInfo( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_INFO, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogInfo ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogVerbose( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_VERBOSE, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogVerbose ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogDebug( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_DEBUG, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogDebug ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogWarning( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_WARN, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogWarning ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogError( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_ERROR, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogError ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
	}
} 
