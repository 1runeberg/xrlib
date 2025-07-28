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

namespace xrlib::EXT
{
	class CHandJointsMotionRange : public CExtBase
	{
	  public:
		explicit CHandJointsMotionRange( XrInstance xrInstance ) : CExtBase( xrInstance, XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME )
		{
			assert( xrInstance != XR_NULL_HANDLE );
		}

		~CHandJointsMotionRange() = default;

	  static XrHandJointsMotionRangeInfoEXT GetHandJointsMotionRangeInfo( XrHandJointsMotionRangeEXT motionRange )
	  {
		  XrHandJointsMotionRangeInfoEXT motionRangeInfo { XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT };
		  motionRangeInfo.handJointsMotionRange = motionRange;
		  return motionRangeInfo;
	  }

	  private:	

	};

} // namespace xrlib
