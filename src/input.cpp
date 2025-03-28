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


#include <xrlib/input.hpp>

namespace xrlib
{
	SAction::~SAction()
	{
		for ( auto &space : vecActionSpaces )
		{
			if ( space != XR_NULL_HANDLE )
			{
				xrDestroySpace( space );
			}
		}

		if ( xrActionHandle != XR_NULL_HANDLE )
		{
			xrDestroyAction( xrActionHandle );
		}
	}

	bool SAction::IsActive( uint32_t unActionStateIndex )
	{
		switch ( xrActionType )
		{
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
				return vecActionStates[ unActionStateIndex ].stateBoolean.isActive;
				break;
			case XR_ACTION_TYPE_FLOAT_INPUT:
				return vecActionStates[ unActionStateIndex ].stateFloat.isActive;
				break;
			case XR_ACTION_TYPE_VECTOR2F_INPUT:
				return vecActionStates[ unActionStateIndex ].stateVector2f.isActive;
				break;
			case XR_ACTION_TYPE_POSE_INPUT:
				return vecActionStates[ unActionStateIndex ].statePose.isActive;
				break;
			case XR_ACTION_TYPE_MAX_ENUM:
			default:
				break;
		}

		return false;
	}

	void SAction::SetActionStateType( uint32_t unActionStateIndex )
	{
		switch ( xrActionType )
		{
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
				vecActionStates[ unActionStateIndex ].stateBoolean.type = XR_TYPE_ACTION_STATE_BOOLEAN;
				break;
			case XR_ACTION_TYPE_FLOAT_INPUT:
				vecActionStates[ unActionStateIndex ].stateFloat.type = XR_TYPE_ACTION_STATE_FLOAT;
				break;
			case XR_ACTION_TYPE_VECTOR2F_INPUT:
				vecActionStates[ unActionStateIndex ].stateVector2f.type = XR_TYPE_ACTION_STATE_VECTOR2F;
				break;
			case XR_ACTION_TYPE_POSE_INPUT:
				vecActionStates[ unActionStateIndex ].statePose.type = XR_TYPE_ACTION_STATE_POSE;
				break;
			case XR_ACTION_TYPE_MAX_ENUM:
			default:
				break;
		}
	}

	XrResult SAction::Init( XrInstance xrInstance, SActionSet *pActionSet, std::string sName, std::string sLocalizedName, std::vector< std::string > vecSubpaths, void *pOtherInfo )
	{
		assert( xrInstance != XR_NULL_HANDLE );

		if ( xrActionHandle != XR_NULL_HANDLE )
			return XR_SUCCESS;

		// Get subaction paths if available
		XrResult xrResult = XR_SUCCESS;
		for ( auto &path : vecSubpaths )
		{
			xrResult = AddSubActionPath( vecSubactionpaths, xrInstance, path );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		// Create action
		XrActionCreateInfo xrActionCreateInfo { XR_TYPE_ACTION_CREATE_INFO };
		xrActionCreateInfo.next = pOtherInfo;
		std::strncpy( xrActionCreateInfo.actionName, sName.c_str(), XR_MAX_ACTION_SET_NAME_SIZE );
		std::strncpy( xrActionCreateInfo.localizedActionName, sLocalizedName.c_str(), XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE );
		xrActionCreateInfo.actionType = xrActionType;

		uint32_t unSubpathsCount = static_cast< uint32_t >( vecSubactionpaths.size() );
		if ( unSubpathsCount > 0 )
		{
			xrActionCreateInfo.countSubactionPaths = unSubpathsCount;
			xrActionCreateInfo.subactionPaths = vecSubactionpaths.data();

			// Create action states
			vecActionStates.resize( unSubpathsCount );

			// Set action state type
			for ( uint32_t i = 0; i < unSubpathsCount; i++ )
				SetActionStateType( i );

			// Create action spaces if this action is a pose
			if ( xrActionType == XR_ACTION_TYPE_POSE_INPUT )
				vecActionSpaces.resize( unSubpathsCount, XR_NULL_HANDLE );
		}
		else
		{
			xrActionCreateInfo.countSubactionPaths = 0;
			xrActionCreateInfo.subactionPaths = nullptr;

			// Create action state
			vecActionStates.resize( 1 );

			// Set action state type
			SetActionStateType( 0 );

			// Create action space
			if ( xrActionType == XR_ACTION_TYPE_POSE_INPUT )
				vecActionSpaces.resize( 1, XR_NULL_HANDLE );
		}

		xrResult = xrCreateAction( pActionSet->xrActionSetHandle, &xrActionCreateInfo, &xrActionHandle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error creating action %s : %s", sName.c_str(), XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Add this action to the action set
		pActionSet->vecActions.push_back( this );

		LogInfo( XRLIB_NAME, "Action created (%s) : %s", sName.c_str(), sLocalizedName.c_str() );
		return XR_SUCCESS;
	}

	XrResult SAction::AddSubActionPath( std::vector< XrPath > &outSubactionPaths, XrInstance xrInstance, std::string sPath )
	{
		assert( xrInstance != XR_NULL_HANDLE );

		XrPath xrPath;
		XrResult xrResult = xrStringToPath( xrInstance, sPath.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error creating an openxr subpath - make sure only allowed characters are used in the path: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		outSubactionPaths.push_back( xrPath );
		return XR_SUCCESS;
	}

	CInput::CInput( CInstance *pInstance, ELogLevel eLoglevel ) : m_eMinLogLevel( eLoglevel )
	{
		assert( pInstance );
		m_pInstance = pInstance;
	}

	CInput::~CInput() {}

	void CInput::Init( CSession *pSession )
	{
		assert( pSession );
		m_pSession = pSession;
	}

	XrResult CInput::CreateActionSet( SActionSet *outActionSet, std::string sName, std::string sLocalizedName, uint32_t unPriority , void *pOtherInfo )
	{
		return outActionSet->Init( m_pInstance->GetXrInstance(), sName, sLocalizedName, unPriority, pOtherInfo );
	}

	XrResult CInput::CreateAction( SAction *outAction, SActionSet *pActionSet, std::string sName, std::string sLocalizedName, std::vector< std::string > vecSubpaths, void *pOtherInfo )
	{
		return outAction->Init( m_pInstance->GetXrInstance(), pActionSet, sName, sLocalizedName, vecSubpaths, pOtherInfo );
	}

	XrResult CInput::CreateActionSpace( SAction *outAction, XrPosef *poseInSpace, std::string subpath, void *pOtherInfo )
	{
		assert( outAction->xrActionType == XR_ACTION_TYPE_POSE_INPUT );

		XrResult xrResult = XR_SUCCESS;
		XrPath xrPath = XR_NULL_PATH;
		uint32_t unActionStateIndex = 0;

		if ( !subpath.empty() )
		{
			if ( !subpath.empty() )
			{
				xrResult = StringToXrPath( subpath.c_str(), &xrPath );

				if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
					return xrResult;
			}

			auto iter = std::find( outAction->vecSubactionpaths.begin(), outAction->vecSubactionpaths.end(), xrPath );
			if ( iter != outAction->vecSubactionpaths.end() )
			{
				unActionStateIndex = static_cast< uint32_t >( iter - outAction->vecSubactionpaths.begin() );
			}
		}

		XrActionSpaceCreateInfo xrActionSpaceCreateInfo { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		xrActionSpaceCreateInfo.action = outAction->xrActionHandle;
		xrActionSpaceCreateInfo.poseInActionSpace = *poseInSpace;
		xrActionSpaceCreateInfo.subactionPath = xrPath;

		xrResult = xrCreateActionSpace( m_pSession->GetXrSession(), &xrActionSpaceCreateInfo, &outAction->vecActionSpaces[ unActionStateIndex ] );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Unable to create an action space : ", XrEnumToString( xrResult ) );
			return xrResult;
		}

		LogInfo( XRLIB_NAME, "Action (%" PRIu64 ") created with reference space handle (%" PRIu64 ")", (uint64_t) ( outAction->xrActionHandle ), (uint64_t) ( outAction->vecActionSpaces[ unActionStateIndex ] ) );
		return XR_SUCCESS;
	}

	XrResult CInput::CreateActionSpaces( SAction *outAction, XrPosef *poseInSpace, void *pOtherInfo /*= nullptr*/ )
	{
		assert( outAction->xrActionType == XR_ACTION_TYPE_POSE_INPUT );
		assert( outAction->vecSubactionpaths.size() == outAction->vecActionSpaces.size() );

		XrResult xrResult = XR_SUCCESS;

		XrActionSpaceCreateInfo xrActionSpaceCreateInfo { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		xrActionSpaceCreateInfo.action = outAction->xrActionHandle;
		xrActionSpaceCreateInfo.poseInActionSpace = *poseInSpace;

		uint32_t unSize = static_cast< uint32_t >( outAction->vecSubactionpaths.size() );
		for ( size_t i = 0; i < unSize; i++ )
		{
			xrActionSpaceCreateInfo.subactionPath = outAction->vecSubactionpaths[ i ];

			xrResult = xrCreateActionSpace( m_pSession->GetXrSession(), &xrActionSpaceCreateInfo, &outAction->vecActionSpaces[ i ] );

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				LogError( XRLIB_NAME, "Unable to create an action space : ", XrEnumToString( xrResult ) );
				return xrResult;
			}

			LogInfo( XRLIB_NAME, "Action (%" PRIu64 ") created with reference space handle (%" PRIu64 ")", (uint64_t) ( outAction->xrActionHandle ), (uint64_t) ( outAction->vecActionSpaces[ i ] ) );
		}

		return XR_SUCCESS;
	}

	XrResult CInput::CreateActionSpaces( SAction *outAction, XrPosef *poseInSpaceLeft, XrPosef *poseInSpaceRight, void *pOtherInfo ) 
	{ 
		assert( outAction->xrActionType == XR_ACTION_TYPE_POSE_INPUT );
		assert( outAction->vecSubactionpaths.size() == outAction->vecActionSpaces.size() );
		assert( outAction->vecSubactionpaths.size() == 2 );

		XrResult xrResult = XR_SUCCESS;

		XrActionSpaceCreateInfo xrActionSpaceCILeft { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		xrActionSpaceCILeft.action = outAction->xrActionHandle;
		xrActionSpaceCILeft.poseInActionSpace = *poseInSpaceLeft;

		XrActionSpaceCreateInfo xrActionSpaceCIRight { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		xrActionSpaceCIRight.action = outAction->xrActionHandle;
		xrActionSpaceCIRight.poseInActionSpace = *poseInSpaceRight;

		for ( size_t i = 0; i < 2; i++ )
		{
			XrPath &path = i == 0 ? xrActionSpaceCILeft.subactionPath : xrActionSpaceCIRight.subactionPath;
			XrActionSpaceCreateInfo *pCreateInfo = i == 0 ? &xrActionSpaceCILeft : &xrActionSpaceCIRight;
			path = outAction->vecSubactionpaths[ i ];

			xrResult = xrCreateActionSpace( m_pSession->GetXrSession(), pCreateInfo, &outAction->vecActionSpaces[ i ] );

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				LogError( XRLIB_NAME, "Unable to create an action space : ", XrEnumToString( xrResult ) );
				return xrResult;
			}

			LogInfo( XRLIB_NAME, "Action (%" PRIu64 ") created with reference space handle (%" PRIu64 ")", (uint64_t) ( outAction->xrActionHandle ), (uint64_t) ( outAction->vecActionSpaces[ i ] ) );
		}

		return XR_SUCCESS;
	}

	XrResult CInput::AddBinding( Controller *controller, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		assert( controller );

		return controller->AddBinding( m_pInstance->GetXrInstance(), action, hand, component, qualifier );
	}

	XrResult CInput::AddBinding( Controller *controller, XrAction action, std::string sFullBindingPath )
	{
		assert( controller );

		return controller->AddBinding( m_pInstance->GetXrInstance(), action, sFullBindingPath );
	}

	XrResult CInput::SuggestBindings( Controller *controller, void *pOtherInfo ) { return controller->SuggestBindings( m_pInstance->GetXrInstance(), pOtherInfo ); }

	XrResult CInput::StringToXrPath( const char *string, XrPath *xrPath )
	{
		assert( xrPath );

		XrResult xrResult = xrStringToPath( m_pInstance->GetXrInstance(), string, xrPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( XRLIB_NAME, "Unable to convert %s to an XrPath: %s", string, XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult CInput::XrPathToString( std::string &outString, XrPath *xrPath )
	{
		outString.clear();

		uint32_t unCount = 0;
		char sPath[ XR_MAX_PATH_LENGTH ];
		XrResult xrResult = xrPathToString( m_pInstance->GetXrInstance(), *xrPath, sizeof( sPath ), &unCount, sPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Unable to convert XrPath: %" PRIu64 " to a readable string", *xrPath, XrEnumToString( xrResult ) );
			return xrResult;
		}

		outString = sPath;
		return XR_SUCCESS;
	}

	XrResult CInput::AddActionsetForSync( SActionSet *pActionSet, std::string subpath )
	{
		XrPath xrPath = XR_NULL_PATH;
		if ( !subpath.empty() )
		{
			XrResult xrResult = StringToXrPath( subpath.c_str(), &xrPath );

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		m_vecActiveActionSets.push_back( pActionSet );
		m_vecXrActiveActionSets.push_back( { pActionSet->xrActionSetHandle, xrPath } );

		return XR_SUCCESS;
	}

	XrResult CInput::RemoveActionsetForSync( SActionSet *pActionSet, std::string subpath )
	{
		XrPath xrPath;
		XrResult xrResult = StringToXrPath( subpath.c_str(), &xrPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Remove from Active ActiveActionsets
		auto end_actionset = std::remove_if( m_vecActiveActionSets.begin(), m_vecActiveActionSets.end(), [ pActionSet ]( const SActionSet *actionset ) { return actionset == pActionSet; } );

		m_vecActiveActionSets.erase( end_actionset, m_vecActiveActionSets.end() );

		// Remove from Active XrActiveActionsets
		auto end_xractionset = std::remove_if(
			m_vecXrActiveActionSets.begin(),
			m_vecXrActiveActionSets.end(),
			[ pActionSet, xrPath ]( const XrActiveActionSet &activeActionSet ) { return activeActionSet.actionSet == pActionSet->xrActionSetHandle && activeActionSet.subactionPath == xrPath; } );

		m_vecXrActiveActionSets.erase( end_xractionset, m_vecXrActiveActionSets.end() );

		return XR_SUCCESS;
	}

	XrResult CInput::AttachActionSetsToSession( XrActionSet *arrActionSets, uint32_t unActionSetCount )
	{
		XrSessionActionSetsAttachInfo xrSessionActionSetsAttachInfo { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
		xrSessionActionSetsAttachInfo.countActionSets = unActionSetCount;
		xrSessionActionSetsAttachInfo.actionSets = arrActionSets;

		XrResult xrResult = xrAttachSessionActionSets( m_pSession->GetXrSession(), &xrSessionActionSetsAttachInfo );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogInfo( XRLIB_NAME, "%i action sets attached to this session", unActionSetCount );
		else
			LogError( XRLIB_NAME, "Error attaching action sets to this session: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult CInput::ProcessInput()
	{
		if ( m_vecXrActiveActionSets.size() < 1 )
			return XR_SUCCESS;

		// Sync active action sets with openxr runtime
		XrActionsSyncInfo xrActionSyncInfo { XR_TYPE_ACTIONS_SYNC_INFO };
		xrActionSyncInfo.countActiveActionSets = static_cast< uint32_t >( m_vecXrActiveActionSets.size() );
		xrActionSyncInfo.activeActionSets = m_vecXrActiveActionSets.data();

		XrResult xrResult = xrSyncActions( m_pSession->GetXrSession(), &xrActionSyncInfo );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Update action states
		for ( auto &actionset : m_vecActiveActionSets )
		{
			for ( auto &action : actionset->vecActions )
			{
				GetActionState( action );
			}
		}

		return xrResult;
	}

	XrResult CInput::GetActionPose( XrSpaceLocation *outSpaceLocation, SAction *pAction, uint32_t unSpaceIndex, XrTime xrTime )
	{
		if ( pAction->vecActionSpaces[ unSpaceIndex ] == XR_NULL_HANDLE )
			return XR_ERROR_VALIDATION_FAILURE;

		return xrLocateSpace( pAction->vecActionSpaces[ unSpaceIndex ], m_pSession->GetAppSpace(), xrTime, outSpaceLocation );
	}

	XrResult CInput::GetActionState( SAction *pAction )
	{
		XrActionStateGetInfo xrActionStateGetInfo { XR_TYPE_ACTION_STATE_GET_INFO };
		xrActionStateGetInfo.action = pAction->xrActionHandle;

		XrResult xrResult = XR_SUCCESS;

		// get the action state from the runtime, block other threads while doing so
		const std::lock_guard< std::mutex > lock( pAction->mutexActionState );

		uint32_t unIterations = static_cast< uint32_t >( pAction->vecSubactionpaths.empty() ? 1 : pAction->vecSubactionpaths.size() );
		for ( uint32_t i = 0; i < unIterations; i++ )
		{
			xrActionStateGetInfo.subactionPath = pAction->vecSubactionpaths.empty() ? XR_NULL_PATH : pAction->vecSubactionpaths[ i ];

			switch ( pAction->xrActionType )
			{
				case XR_ACTION_TYPE_BOOLEAN_INPUT:
					xrResult = xrGetActionStateBoolean( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->vecActionStates[ i ].stateBoolean );
					if ( pAction->vecActionStates[ i ].stateBoolean.isActive && pAction->vecActionStates[ i ].stateBoolean.changedSinceLastSync )
						pAction->pfnCallback( pAction, i );
					break;
				case XR_ACTION_TYPE_FLOAT_INPUT:
					xrResult = xrGetActionStateFloat( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->vecActionStates[ i ].stateFloat );
					if ( pAction->vecActionStates[ i ].stateFloat.isActive && pAction->vecActionStates[ i ].stateFloat.changedSinceLastSync )
						pAction->pfnCallback( pAction, i );
					break;
				case XR_ACTION_TYPE_VECTOR2F_INPUT:
					xrResult = xrGetActionStateVector2f( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->vecActionStates[ i ].stateVector2f );
					if ( pAction->vecActionStates[ i ].stateVector2f.isActive && pAction->vecActionStates[ i ].stateVector2f.changedSinceLastSync )
						pAction->pfnCallback( pAction, i );
					break;
				case XR_ACTION_TYPE_POSE_INPUT:
					xrResult = xrGetActionStatePose( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->vecActionStates[ i ].statePose );
					pAction->pfnCallback( pAction, i );
					break;
				case XR_ACTION_TYPE_MAX_ENUM:
				default:
					xrResult = XR_ERROR_ACTION_TYPE_MISMATCH;
					break;
			}
		}

		return xrResult;
	}


    const std::string &CInput::GetCurrentInteractionProfile( const char *sUserPath )
    {
        static std::string sInteractionProfile;
        sInteractionProfile.clear();

        XrPath xrPath;
        XrResult xrResult = StringToXrPath( sUserPath, &xrPath );

        if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
            return sInteractionProfile;

        XrInteractionProfileState xrInteractionProfileState{ XR_TYPE_INTERACTION_PROFILE_STATE };
        xrResult = xrGetCurrentInteractionProfile( m_pSession->GetXrSession(), xrPath,
                                                  &xrInteractionProfileState );

        if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
            return sInteractionProfile;

        xrResult = XrPathToString( sInteractionProfile,
                                  &xrInteractionProfileState.interactionProfile );

        if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
            return sInteractionProfile;

        LogInfo( XRLIB_NAME, "Current interaction profile (%s) : %s", sUserPath,
                sInteractionProfile.c_str() );
        return sInteractionProfile;
    }

	XrResult CInput::GenerateHaptic( XrAction xrAction, XrPath subPath /*= XR_NULL_HANDLE */, uint64_t nDuration /*= XR_MIN_HAPTIC_DURATION*/, float fAmplitude /*= 0.5f*/, float fFrequency /*= XR_FREQUENCY_UNSPECIFIED */ )
	{
		XrHapticVibration xrHapticVibration { XR_TYPE_HAPTIC_VIBRATION };
		xrHapticVibration.duration = nDuration;
		xrHapticVibration.amplitude = fAmplitude;
		xrHapticVibration.frequency = fFrequency;

		XrHapticActionInfo xrHapticActionInfo { XR_TYPE_HAPTIC_ACTION_INFO };
		xrHapticActionInfo.action = xrAction;
		xrHapticActionInfo.subactionPath = subPath;

		return xrApplyHapticFeedback( m_pSession->GetXrSession(), &xrHapticActionInfo, (const XrHapticBaseHeader *) &xrHapticVibration );
	}

} // namespace xrlib
