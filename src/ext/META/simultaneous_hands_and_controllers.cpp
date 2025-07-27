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

#include <xrlib/ext/META/simultaneous_hands_and_controllers.hpp>

namespace xrlib::META
{
	CHandsAndControllers::CHandsAndControllers( XrInstance xrInstance )
		: CExtBase( xrInstance, XR_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );

		// Initialize all function pointers available for this extension
		INIT_PFN( xrInstance, xrPauseSimultaneousHandsAndControllersTrackingMETA );
		INIT_PFN( xrInstance, xrResumeSimultaneousHandsAndControllersTrackingMETA );
	}

	XrResult CHandsAndControllers::Enable( XrSession xrSession, void* pNext ) const
	{
		XrSimultaneousHandsAndControllersTrackingResumeInfoMETA resumeInfo { XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_RESUME_INFO_META };
		resumeInfo.next = pNext;

		return xrResumeSimultaneousHandsAndControllersTrackingMETA( xrSession, &resumeInfo );
	}

	XrResult CHandsAndControllers::Disable( XrSession xrSession, void* pNext ) const
	{
		XrSimultaneousHandsAndControllersTrackingPauseInfoMETA pauseInfo { XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_PAUSE_INFO_META };
		pauseInfo.next = pNext;

		return xrPauseSimultaneousHandsAndControllersTrackingMETA( xrSession, &pauseInfo );
	}

} // namespace xrlib::META
