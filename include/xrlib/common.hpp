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

#define XR_USE_GRAPHICS_API_VULKAN 1
#define XR_LEFT 0
#define XR_RIGHT 1

#include <cstring>
#include <cassert>
#include <cmath>
#include <future>
#include <vulkan/vulkan.h>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

// Add android headers - must include before the openxr headers
#ifdef XR_USE_PLATFORM_ANDROID
	#include <android/asset_manager.h>
	#include <android/asset_manager_jni.h>
	#include <android/native_window.h>
	#include <android_native_app_glue.h>
	#include <jni.h>
	#include <sys/system_properties.h>
#endif

// Openxr headers
#include <third_party/openxr/include/openxr/openxr.h>
#include <third_party/openxr/include/openxr/openxr_platform.h>
#include <third_party/openxr/include/openxr/openxr_reflection.h>
#include <third_party/openxr/src/common/xr_linear.h>

#include "xrlib/log.hpp"
namespace xrlib
{
	inline static float GetLength( const XrVector4f *v ) { return sqrtf( v->x * v->x + v->y * v->y + v->z * v->z + v->w * v->w ); }

	static float GetDistance( XrVector3f &a, XrVector3f &b )
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float dz = a.z - b.z;
		return std::sqrt( dx * dx + dy * dy + dz * dz );
	};

	template < typename T >
	concept HasStaticType = requires {
		{ T::type } -> std::convertible_to< XrStructureType >;
	};

	template < typename T >
	concept HasNextField = requires( const T &t ) {
		{ t.next } -> std::convertible_to< const void * >;
	};

	template < typename TTarget, typename TContainer >
		requires HasStaticType< TTarget > && HasNextField< TContainer >
	const TTarget *GetStructureFromChain( const TContainer &container, XrStructureType xrTargetType )
	{
		const void *current = &container;
		LogDebug("GetStructureFromChain", "Searching for structure type %d in chain", xrTargetType );

		// Traverse next chain
		while ( current )
		{
			// Debug log: print the type of the current structure
			const auto *pBaseStruct = static_cast< const TTarget * >( current );
			LogDebug("GetStructureFromChain", "Debug: Structure type = %d, Target type = %d", pBaseStruct->type, xrTargetType);

			// Get the target structure type at compile time
			if ( pBaseStruct->type == xrTargetType )
			{
				LogDebug("GetStructureFromChain", "Found structure type %d in chain", xrTargetType );
				return pBaseStruct;
			}

			current = pBaseStruct->next;
		}

		return nullptr;
	}

#ifdef XR_USE_PLATFORM_ANDROID
	struct AndroidAppState
	{
		ANativeWindow *NativeWindow = nullptr;
		bool Resumed = false;
	};

	static void app_handle_cmd( struct android_app *app, int32_t cmd )
	{
		AndroidAppState *appState = (AndroidAppState *) app->userData;

		switch ( cmd )
		{
			case APP_CMD_RESUME:
			{
				appState->Resumed = true;
				break;
			}
			case APP_CMD_PAUSE:
			{
				appState->Resumed = false;
				break;
			}
			case APP_CMD_DESTROY:
			{
				appState->NativeWindow = NULL;
				break;
			}
			case APP_CMD_INIT_WINDOW:
			{
				appState->NativeWindow = app->window;
				break;
			}
			case APP_CMD_TERM_WINDOW:
			{
				appState->NativeWindow = NULL;
				break;
			}
		}
	}
	#endif

	#define XR_MAKE_VERSION32( major, minor, patch ) ( ( ( major ) << 22 ) | ( ( minor ) << 12 ) | ( patch ) )
	#define XR_VERSION_MAJOR32( version ) ( (uint32_t) ( version ) >> 22 )
	#define XR_VERSION_MINOR32( version ) ( ( (uint32_t) ( version ) >> 12 ) & 0x3ff )
	#define XR_VERSION_PATCH32( version ) ( (uint32_t) (version) &0xfff )
	
	#define XR_ENUM_STRINGIFY( sEnum, val )                                                                                                                                                                \
		case sEnum:                                                                                                                                                                                        \
			return #sEnum;
	#define XR_ENUM_TYPE_STRINGIFY( xrEnumType )                                                                                                                                                           \
		constexpr const char *XrEnumToString( xrEnumType eNum )                                                                                                                                            \
		{                                                                                                                                                                                                  \
			switch ( eNum )                                                                                                                                                                                \
			{                                                                                                                                                                                              \
				XR_LIST_ENUM_##xrEnumType( XR_ENUM_STRINGIFY ) default : return "Unknown Enum. Define in openxr_reflection.h";                                                                             \
			}                                                                                                                                                                                              \
		}

		XR_ENUM_TYPE_STRINGIFY( XrResult );
		XR_ENUM_TYPE_STRINGIFY( XrViewConfigurationType );
		XR_ENUM_TYPE_STRINGIFY( XrSessionState );
		XR_ENUM_TYPE_STRINGIFY( XrReferenceSpaceType );

	#define INIT_PFN( instance, pfn ) xrGetInstanceProcAddr( instance, #pfn, (PFN_xrVoidFunction *) ( &pfn ) );

	#define XR_RETURN_ON_ERROR( xrResult )                                                                                                                                                                 \
		do                                                                                                                                                                                                 \
		{                                                                                                                                                                                                  \
			XrResult result = xrResult;                                                                                                                                                                    \
			if ( result != XR_SUCCESS )                                                                                                                                                                    \
				return result;                                                                                                                                                                             \
		} while ( false );
}