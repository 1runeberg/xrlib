/* 
 * Copyright 2024-26 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#include <xrlib/ext/FB/display_refresh_rate.hpp>
#include <xrlib/common.hpp>
#include <xrlib/log.hpp>


namespace xrlib::FB
{
	CDisplayRefreshRate::CDisplayRefreshRate( XrInstance xrInstance ) :  
		CExtBase( xrInstance, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME )
	{
		// Initialize all function pointers available for this extension
		INIT_PFN( xrInstance, xrEnumerateDisplayRefreshRatesFB );
		INIT_PFN( xrInstance, xrGetDisplayRefreshRateFB );
		INIT_PFN( xrInstance, xrRequestDisplayRefreshRateFB );
	}

	XrResult CDisplayRefreshRate::GetSupportedRefreshRates( XrSession xrSession, std::vector< float > &outSupportedRefreshRates )
	{
		// Get the refresh rate count capacity
		uint32_t unRefreshRateCount = 0;
		XrResult xrResult = xrEnumerateDisplayRefreshRatesFB( xrSession, 0, &unRefreshRateCount, nullptr );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( GetName(), "Error retrieving all supported refresh rates: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Get the all available refresh rates
		uint32_t unRefreshRateCapacity = unRefreshRateCount;
		outSupportedRefreshRates.resize( unRefreshRateCapacity );

		xrResult = xrEnumerateDisplayRefreshRatesFB( xrSession, unRefreshRateCapacity, &unRefreshRateCount, outSupportedRefreshRates.data() );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error retrieving all supported refresh rates: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	float CDisplayRefreshRate::GetCurrentRefreshRate( XrSession xrSession )
	{
		float outRefreshRate = 0.0f;
		XrResult xrResult = xrGetDisplayRefreshRateFB( xrSession, &outRefreshRate );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( GetName(), "Error retrieving current refresh rate: %s", XrEnumToString( xrResult ) );
			return 0.0f;
		}

		return outRefreshRate;
	}

	XrResult CDisplayRefreshRate::RequestRefreshRate( XrSession xrSession, float fRequestedRefreshRate )
	{

		XrResult xrResult = xrRequestDisplayRefreshRateFB( xrSession, fRequestedRefreshRate );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error requesting refresh rate (%f): %s", fRequestedRefreshRate, XrEnumToString( xrResult ) );

		return xrResult;
	}

} 
