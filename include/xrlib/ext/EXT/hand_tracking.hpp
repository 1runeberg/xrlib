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

#include <vector>
#include <array>
#include <xrlib/ext/ext_base.hpp>

namespace xrlib::EXT
{
	class CHandTracking : public CExtBase
	{
	  public:
		explicit CHandTracking( XrInstance xrInstance );
		~CHandTracking();

		XrResult Init( XrSession xrSession, 
			XrHandJointSetEXT leftHandJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT, 
			void *pNextLeft = nullptr, 
			XrHandJointSetEXT rightHandJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT, 
			void *pNextRight = nullptr );

		struct SJointVelocities
		{
			XrHandJointVelocitiesEXT left { XR_TYPE_HAND_JOINT_VELOCITIES_EXT };
			XrHandJointVelocitiesEXT right { XR_TYPE_HAND_JOINT_VELOCITIES_EXT };

			std::array< XrHandJointVelocityEXT, XR_HAND_JOINT_COUNT_EXT > leftVelocities;
			std::array< XrHandJointVelocityEXT, XR_HAND_JOINT_COUNT_EXT > rightVelocities;

			SJointVelocities( void *pNextLeft = nullptr, void *pNextRight = nullptr );
			~SJointVelocities() {};
		};

		struct SJointLocations
		{
			XrHandJointLocationsEXT left { XR_TYPE_HAND_JOINT_LOCATIONS_EXT };
			XrHandJointLocationsEXT right { XR_TYPE_HAND_JOINT_LOCATIONS_EXT };

			std::array< XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT > leftJointLocations;
			std::array< XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT > rightJointLocations;

			SJointLocations( SJointVelocities &jointVelocities );
			SJointLocations( void *pNextLeft = nullptr, void *pNextRight = nullptr );
			~SJointLocations() {};
		};

		XrResult LocateHandJoints( SJointLocations *outHandJointLocations, XrSpace baseSpace, XrTime time, void *pNextLeft = nullptr, void *pNextRight = nullptr );
		XrResult LocateHandJoints( XrHandJointLocationsEXT *outHandJointLocations, XrHandEXT hand, XrSpace baseSpace, XrTime time, void *pNext = nullptr );
		
		XrSystemHandTrackingPropertiesEXT GenerateSystemProperties( void *pNext = nullptr );

		XrHandTrackerEXT *GetHandTracker( XrHandEXT hand );
		std::vector< XrHandTrackerEXT > &GetHandTrackers() { return m_vecHandTrackers; }

	  private:	
		XrSession m_xrSession = XR_NULL_HANDLE;
		PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT = nullptr;
		std::vector< XrHandTrackerEXT > m_vecHandTrackers = { XR_NULL_HANDLE, XR_NULL_HANDLE };

	};

} // namespace xrlib
