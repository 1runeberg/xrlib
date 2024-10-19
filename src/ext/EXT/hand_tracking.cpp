/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <xrlib/ext/EXT/hand_tracking.hpp>
#include <xrlib/common.hpp>

namespace xrlib::EXT
{
	CHandTracking::SJointVelocities::SJointVelocities( void *pNextLeft, void *pNextRight )
	{
		left.next = pNextLeft;
		left.jointCount = XR_HAND_JOINT_COUNT_EXT;
		left.jointVelocities = leftVelocities.data();

		right.next = pNextRight;
		right.jointCount = XR_HAND_JOINT_COUNT_EXT;
		right.jointVelocities = rightVelocities.data();
	}

	CHandTracking::SJointLocations::SJointLocations( SJointVelocities &jointVelocities ) 
	{
		left.next = &jointVelocities.left;
		left.isActive = XR_FALSE;
		left.jointCount = XR_HAND_JOINT_COUNT_EXT;
		left.jointLocations = leftJointLocations.data();

		right.next = &jointVelocities.right;
		right.isActive = XR_FALSE;
		right.jointCount = XR_HAND_JOINT_COUNT_EXT;
		right.jointLocations = rightJointLocations.data();
	}

	CHandTracking::SJointLocations::SJointLocations( void *pNextLeft, void *pNextRight ) 
	{
		left.next = pNextLeft;
		left.isActive = XR_FALSE;
		left.jointCount = XR_HAND_JOINT_COUNT_EXT;
		left.jointLocations = leftJointLocations.data();

		right.next = pNextRight;
		right.isActive = XR_FALSE;
		right.jointCount = XR_HAND_JOINT_COUNT_EXT;
		right.jointLocations = rightJointLocations.data();
	}

	CHandTracking::CHandTracking( XrInstance xrInstance ) : 
		ExtBase( xrInstance, XR_EXT_HAND_TRACKING_EXTENSION_NAME) 
	{
		// Initialize function pointers
        XrResult result = INIT_PFN( xrInstance, xrLocateHandJointsEXT );
		assert( XR_UNQUALIFIED_SUCCESS( result ) );
	}

	CHandTracking::~CHandTracking() 
	{ 
		PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT = nullptr;
		XrResult result = INIT_PFN( m_xrInstance, xrDestroyHandTrackerEXT );

		if ( XR_UNQUALIFIED_SUCCESS( result ) )
		{
			for ( auto &handTracker : m_vecHandTrackers )
				if ( handTracker != XR_NULL_HANDLE ) xrDestroyHandTrackerEXT( handTracker );
		}
	}

	XrResult CHandTracking::Init( XrSession xrSession, XrHandJointSetEXT leftHandJointSet, void *pNextLeft, XrHandJointSetEXT rightHandJointSet, void *pNextRight ) 
	{ 
		assert( xrSession != XR_NULL_HANDLE );
		m_xrSession = xrSession;

		// Create hand trackers
		PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT = nullptr;
		XR_RETURN_ON_ERROR( INIT_PFN( m_xrInstance, xrCreateHandTrackerEXT ) );

		XrHandTrackerCreateInfoEXT handTrackerCI { XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
		handTrackerCI.next = pNextLeft;
		handTrackerCI.hand = XR_HAND_LEFT_EXT;
		handTrackerCI.handJointSet = leftHandJointSet;
		XR_RETURN_ON_ERROR( xrCreateHandTrackerEXT( m_xrSession, &handTrackerCI, &m_vecHandTrackers[ XR_LEFT ] ) );

		handTrackerCI.next = pNextRight;
		handTrackerCI.hand = XR_HAND_RIGHT_EXT;
		handTrackerCI.handJointSet = rightHandJointSet;
		XR_RETURN_ON_ERROR( xrCreateHandTrackerEXT( m_xrSession, &handTrackerCI, &m_vecHandTrackers[ XR_RIGHT ] ) );

		return XR_SUCCESS;
	}

	XrResult CHandTracking::LocateHandJoints( SJointLocations *outHandJointLocations, XrSpace baseSpace, XrTime time, void *pNextLeft, void *pNextRight ) 
	{ 
		XR_RETURN_ON_ERROR( LocateHandJoints( &outHandJointLocations->left, XrHandEXT::XR_HAND_LEFT_EXT, baseSpace, time, pNextLeft ) );
		XR_RETURN_ON_ERROR( LocateHandJoints( &outHandJointLocations->right, XrHandEXT::XR_HAND_RIGHT_EXT, baseSpace, time, pNextRight ) );

		return XR_SUCCESS; 
	}

	XrResult CHandTracking::LocateHandJoints( XrHandJointLocationsEXT *outHandJointLocations, XrHandEXT hand, XrSpace baseSpace, XrTime time, void *pNext ) 
	{ 
		XrHandJointsLocateInfoEXT xrHandJointsLocateInfo { XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
		xrHandJointsLocateInfo.next = pNext,
		xrHandJointsLocateInfo.baseSpace = baseSpace;
		xrHandJointsLocateInfo.time = time;

		return xrLocateHandJointsEXT( 
			( hand == XrHandEXT::XR_HAND_LEFT_EXT ) ? m_vecHandTrackers[ XR_LEFT ] : m_vecHandTrackers[ XR_RIGHT ],
			&xrHandJointsLocateInfo,
			outHandJointLocations );
	}

	XrSystemHandTrackingPropertiesEXT CHandTracking::GenerateSystemProperties( void *pNext ) 
	{ 
		XrSystemHandTrackingPropertiesEXT systemProps { XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT }; 
		systemProps.next = pNext;
		systemProps.supportsHandTracking = XR_FALSE;

		return systemProps;
	}

	XrHandTrackerEXT *CHandTracking::GetHandTracker( XrHandEXT hand ) 
	{ 
		switch ( hand )
		{
			case XR_HAND_LEFT_EXT:
				return &m_vecHandTrackers[ XR_LEFT ];
				break;
			case XR_HAND_RIGHT_EXT:
				return &m_vecHandTrackers[ XR_RIGHT ];
				break;
			case XR_HAND_MAX_ENUM_EXT:
			default:
				break;
		}

		return nullptr;
	}



} // namespace xrlib::EXT
