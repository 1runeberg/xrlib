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

#include <xrlib/ext/FB/display_refresh_rate.hpp>
#include <xrlib/common.hpp>
#include <xrlib/log.hpp>


namespace xrlib::FB
{
	DisplayRefreshRate::DisplayRefreshRate( XrInstance xrInstance ) :  
		ExtBase( xrInstance, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME )
	{
		// Initialize all function pointers available for this extension
		INIT_PFN( xrInstance, xrEnumerateDisplayRefreshRatesFB );
		INIT_PFN( xrInstance, xrGetDisplayRefreshRateFB );
		INIT_PFN( xrInstance, xrRequestDisplayRefreshRateFB );
	}

	XrResult DisplayRefreshRate::GetSupportedRefreshRates( XrSession xrSession, std::vector< float > &outSupportedRefreshRates )
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

	float DisplayRefreshRate::GetCurrentRefreshRate( XrSession xrSession )
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

	XrResult DisplayRefreshRate::RequestRefreshRate( XrSession xrSession, float fRequestedRefreshRate )
	{

		XrResult xrResult = xrRequestDisplayRefreshRateFB( xrSession, fRequestedRefreshRate );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( GetName(), "Error requesting refresh rate (%f): %s", fRequestedRefreshRate, XrEnumToString( xrResult ) );

		return xrResult;
	}

} 
