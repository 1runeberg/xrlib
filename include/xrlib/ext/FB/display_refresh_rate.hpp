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

#include <xrlib/ext/ext_base.hpp>

#include <vector>

namespace xrlib::FB
{
	class CDisplayRefreshRate : public CExtBase
	{
	  public:

		explicit CDisplayRefreshRate( XrInstance xrInstance );

		/// <summary>
		/// Retrieve all supported display refresh rates from the openxr runtime that is valid for the running session and hardware
		/// </summary>
		/// <param name="outSupportedRefreshRates">vector fo floats to put the supported refresh rates in</param>
		/// <returns>Result from the openxr runtime of retrieving all supported refresh rates</returns>
		XrResult GetSupportedRefreshRates( XrSession xrSession, std::vector< float > &outSupportedRefreshRates );

		/// <summary>
		/// Retrieves the currently active display refresh rate
		/// </summary>
		/// <returns>The active display refresh rate, 0.0f if an error is encountered (check logs) - you can also check for event XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB</returns>
		float GetCurrentRefreshRate( XrSession xrSession );

		/// <summary>
		/// Request a refresh rate from the openxr runtime. Provided refresh rate must be a supported refresh rate via GetSupportedRefreshRates()
		/// </summary>
		/// <param name="fRequestedRefreshRate">The requested refresh rate. This must be a valid refresh rate from GetSupportedRefreshRates(). Use 0.0f to indicate no prefrence and let the runtime
		/// choose the most appropriate one for the session.</param>
		/// <returns>Result from the openxr runtime of requesting a refresh rate</returns>
		XrResult RequestRefreshRate( XrSession xrSession, float fRequestedRefreshRate );

		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_display_refresh_rate
		PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = nullptr;
		PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB = nullptr;
		PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;

	  private:

	};

} 
