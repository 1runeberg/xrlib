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

#include <xrlib/ext/EXT/hand_interaction.hpp>

namespace xrlib
{
	XrResult SHandInteraction::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		LogError( "SHandInteraction::AddBinding", "For this controller, you must use the AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, EXT::CHandInteraction::EHandInteractionComponent component ) to add bindings." );
		return XR_ERROR_PATH_INVALID;
	}

	XrResult SHandInteraction::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, EXT::CHandInteraction::EHandInteractionComponent component )
	{
		// Get binding path based on component
		std::string sFullBindingPath;
		switch ( component )
		{
			case EXT::CHandInteraction::EHandInteractionComponent::PinchPose:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_PinchPose( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::PinchPoke:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_PinchPoke( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::PinchValue:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_PinchValue( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::PinchReady:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_PinchReady( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::AimActivateValue:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_AimActivateValue( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::AimActivateReady:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_AimActivateReady( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::GraspValue:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_GraspValue( hand );
				break;
			case EXT::CHandInteraction::EHandInteractionComponent::GraspReady:
				sFullBindingPath = EXT::CHandInteraction::GetBindingPath_GraspReady( hand );
				break;
			default:
				LogError( "SHandInteraction::AddBinding", "Unknown component type: %d", static_cast< int >( component ) );
				return XR_ERROR_PATH_INVALID;
		}

		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sFullBindingPath.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "SHandInteraction::AddBinding", "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sFullBindingPath.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( "SHandInteraction::AddBinding", "Added binding path: (%s) for: (%s)", sFullBindingPath.c_str(), Path() );
		return XR_SUCCESS;
	}

} // namespace xrlib
