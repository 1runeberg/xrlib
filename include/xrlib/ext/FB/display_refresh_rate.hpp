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

#pragma once

#include <xrlib/ext/ext_base.hpp>

#include <vector>

namespace xrlib::FB
{
	class DisplayRefreshRate : public ExtBase
	{
	  public:

		DisplayRefreshRate( XrInstance xrInstance );

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
