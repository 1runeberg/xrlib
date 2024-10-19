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
#include <xrlib/common.hpp>

#include <vector>

namespace xrlib::KHR
{
	class VisibilityMask : public ExtBase
	{
	  public:

		VisibilityMask( XrInstance xrInstance );

		~VisibilityMask() {}

		/// <summary>
		/// Retrieves the visibility mask for a given view (e.g. one per eye)
		/// </summary>
		/// <param name="outVertices">Output parameter that will hold the vismask vertices</param>
		/// <param name="outIndices">Output parameter that will hold the vismask indices</param>
		/// <param name="xrViewConfigurationType">The view configuration type (e.g. STEREO for vr)</param>
		/// <param name="unViewIndex">The view index (e.g. 0 for the left eye, 1 for the right eye</param>
		/// <param name="xrVisibilityMaskType">The mask polygon type (e.g. line loop, visible tris, etc)</param>
		/// <returns>Result of retrieving the visibility mask</returns>
		XrResult GetVisMask(
			XrSession xrSession,
			std::vector< XrVector2f > &outVertices,
			std::vector< uint32_t > &outIndices,
			XrViewConfigurationType xrViewConfigurationType,
			uint32_t unViewIndex,
			XrVisibilityMaskTypeKHR xrVisibilityMaskType );

		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_visibility_mask
		PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR = nullptr;

	  private:

	};
}
