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

#pragma once

#include <xrlib/ext/ext_base.hpp>

#include <vector>

namespace xrlib::FB
{
	class CHandTrackingMesh : public CExtBase
	{
	  public:

		/// <summary>
		/// Loads the hand mesh extension entry point
		/// </summary>
		/// <param name="xrInstance">Active instance with XR_EXT_hand_tracking and XR_FB_hand_tracking_mesh enabled</param>
		explicit CHandTrackingMesh( XrInstance xrInstance );
		~CHandTrackingMesh() = default;

		struct SMesh
		{
			std::vector< XrPosef > jointBindPoses;
			std::vector< float > jointRadii;
			std::vector< XrHandJointEXT > jointParents;

			std::vector< XrVector3f > vertexPositions;
			std::vector< XrVector3f > vertexNormals;
			std::vector< XrVector2f > vertexUVs;
			std::vector< XrVector4sFB > vertexBlendIndices;
			std::vector< XrVector4f > vertexBlendWeights;

			std::vector< int16_t > indices;
		};

		/// <summary>
		/// Retrieves the hand mesh and joint bind poses (unchanged for the instance's lifetime)
		/// </summary>
		/// <param name="handTracker">Valid hand tracker created from the same instance</param>
		/// <param name="outMesh">Caller-owned mesh data (may be resized or partially written on failure)</param>
		/// <returns>Runtime result, or XR_ERROR_FUNCTION_UNSUPPORTED if the entry point isn't available</returns>
		XrResult GetMesh( XrHandTrackerEXT handTracker, SMesh &outMesh );

		// Runtime entry point
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_hand_tracking_mesh
		PFN_xrGetHandMeshFB xrGetHandMeshFB = nullptr;
	};
}
