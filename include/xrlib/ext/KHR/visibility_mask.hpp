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
#include <xrlib/common.hpp>

#include <vector>

namespace xrlib::KHR
{
	class CVisibilityMask : public CExtBase
	{
	  public:

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="xrInstance">Currently active openxr instance</param>
		explicit CVisibilityMask( XrInstance xrInstance );


		/// <summary>
		/// Destructor
		/// </summary>
		~CVisibilityMask() = default;


		/// <summary>
		/// Retrieves the visibility mask for a given view (e.g. one per eye)
		/// </summary>
		/// <param name="xrEventData">Base header data after openxr poll</param>
		/// <param name="xrSession">Currently running openxr session</param>
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


		/// <summary>
		/// Retrieves the visibility mask for a given view (e.g. one per eye)
		/// This is the same as GetVisMask() but outputs unsigned short for indices instead of the spec's uint32_t
		/// Max ~65K verts should be more than enough for a vismask, this typically should be way lower.
		/// </summary>
		/// <param name="xrEventData">Base header data after openxr poll</param>
		/// <param name="xrSession">Currently running openxr session</param>
		/// <param name="outVertices">Output parameter that will hold the vismask vertices</param>
		/// <param name="outIndices">Output parameter that will hold the vismask indices</param>
		/// <param name="xrViewConfigurationType">The view configuration type (e.g. STEREO for vr)</param>
		/// <param name="unViewIndex">The view index (e.g. 0 for the left eye, 1 for the right eye</param>
		/// <param name="xrVisibilityMaskType">The mask polygon type (e.g. line loop, visible tris, etc)</param>
		/// <returns>Result of retrieving the visibility mask</returns>
		XrResult GetVisMaskShortIndices( XrSession xrSession, std::vector< XrVector2f > &outVertices, std::vector< unsigned short > &outIndices, XrViewConfigurationType xrViewConfigurationType, uint32_t unViewIndex, XrVisibilityMaskTypeKHR xrVisibilityMaskType );


		/// <summary>
		/// Updates the visibility mask for a given view (e.g. one per eye) if the runtiome triggers the appropriate event
		/// </summary>
		/// <param name="xrEventData">Base header data after openxr poll</param>
		/// <param name="xrSession">Currently running openxr session</param>
		/// <param name="outVertices">Output parameter that will hold the vismask vertices</param>
		/// <param name="outIndices">Output parameter that will hold the vismask indices</param>
		/// <param name="xrViewConfigurationType">The view configuration type (e.g. STEREO for vr)</param>
		/// <param name="unViewIndex">The view index (e.g. 0 for the left eye, 1 for the right eye</param>
		/// <param name="xrVisibilityMaskType">The mask polygon type (e.g. line loop, visible tris, etc)</param>
		/// <returns>Result of retrieving the visibility mask</returns>			
		XrResult UpdateVisMask(
			XrEventDataBaseHeader xrEventData,
			XrSession xrSession,
			std::vector< XrVector2f > &outVertices,
			std::vector< uint32_t > &outIndices,
			XrViewConfigurationType xrViewConfigurationType,
			uint32_t unViewIndex,
			XrVisibilityMaskTypeKHR xrVisibilityMaskType );


		/// <summary>
		/// Updates the visibility mask for a given view (e.g. one per eye) if the runtiome triggers the appropriate event
		/// This is the same as UpdateVisMask() but outputs unsigned short for indices instead of the spec's uint32_t
		/// Max ~65K verts should be more than enough for a vismask, this typically should be way lower.
		/// </summary>
		/// <param name="xrEventData">Base header data after openxr poll</param>
		/// <param name="xrSession">Currently running openxr session</param>
		/// <param name="outVertices">Output parameter that will hold the vismask vertices</param>
		/// <param name="outIndices">Output parameter that will hold the vismask indices</param>
		/// <param name="xrViewConfigurationType">The view configuration type (e.g. STEREO for vr)</param>
		/// <param name="unViewIndex">The view index (e.g. 0 for the left eye, 1 for the right eye</param>
		/// <param name="xrVisibilityMaskType">The mask polygon type (e.g. line loop, visible tris, etc)</param>
		/// <returns>Result of retrieving the visibility mask</returns>	
		XrResult UpdateVisMaskShortIndices(
			XrEventDataBaseHeader xrEventData, 
			XrSession xrSession,
			std::vector< XrVector2f > &outVertices,
			std::vector< unsigned short > &outIndices,
			XrViewConfigurationType xrViewConfigurationType,
			uint32_t unViewIndex,
			XrVisibilityMaskTypeKHR xrVisibilityMaskType );

		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_visibility_mask
		PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR = nullptr;

	  private:

	};
}
