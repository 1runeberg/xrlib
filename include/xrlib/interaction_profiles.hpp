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

#include <xrlib/common.hpp>
#include <vector>

namespace xrlib
{
	struct Controller
	{
	  public:
		const char *k_pccLeftHand = "/user/hand/left";
		const char *k_pccRightHand = "/user/hand/right";
		const char *k_pccInput = "/input";
		const char *k_pccOutput = "/output";

		const char *k_pccTrigger = "/trigger";
		const char *k_pccThumbstick = "/thumbstick";
		const char *k_pccTrackpad = "/trackpad";
		const char *k_pccSqueeze = "/squeeze";
		const char *k_pccMenu = "/menu";
		const char *k_pccSystem = "/system";

		const char *k_pccGripPose = "/grip/pose";
		const char *k_pccAimPose = "/aim/pose";
		const char *k_pccHaptic = "/haptic";

		const char *k_pccClick = "/click";
		const char *k_pccTouch = "/touch";
		const char *k_pccValue = "/value";
		const char *k_pccForce = "/force";

		const char *k_pccA = "/a";
		const char *k_pccB = "/b";
		const char *k_pccX = "/x";
		const char *k_pccY = "/y";

		enum class Component
		{
			GripPose = 1,
			AimPose = 2,
			Trigger = 3,
			PrimaryButton = 4,
			SecondaryButton = 5,
			AxisControl = 6,
			Squeeze = 7,
			Menu = 8,
			System = 9,
			Haptic = 10,
			ComponentEMax
		};

		enum class Qualifier
		{
			None = 0,
			Click = 1,
			Touch = 2,
			Value = 3,
			Force = 4,
			X = 5,
			Y = 6,
			Grip = 7,
			Haptic = 8,
			ComponentEMax
		};

		std::vector< XrActionSuggestedBinding > vecSuggestedBindings;

		virtual const char *Path() = 0;
		virtual XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) = 0;
		virtual XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) = 0;

		XrResult AddBinding( XrInstance xrInstance, XrAction action, std::string &sFullBindingPath );
		XrResult SuggestControllerBindings( XrInstance xrInstance, void *pOtherInfo );
	};
	using InputComponent = Controller::Component;
	using InputQualifier = Controller::Qualifier;

	struct HTCVive : public Controller
	{
		const char *Path() override { return "/interaction_profiles/htc/vive_controller"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings(XrInstance xrInstance, void* pOtherInfo) override { return SuggestControllerBindings(xrInstance, pOtherInfo); };
	};

	struct MicrosoftMixedReality : public Controller
	{
		const char *Path() override { return "/interaction_profiles/microsoft/motion_controller"; }
		XrResult AddBinding(XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct OculusTouch : public Controller
	{
		const char *Path() override { return "/interaction_profiles/oculus/touch_controller"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct ValveIndex : public Controller
	{
		const char *Path() override { return "/interaction_profiles/valve/index_controller"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct BaseController
	{
	  public:
		XrResult xrResult = XR_SUCCESS;
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
		{
			for ( auto &interactionProfile : vecSupportedControllers )
			{
				xrResult = interactionProfile->AddBinding( xrInstance, action, hand, component, qualifier );

				if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
					return xrResult;
			}

			return XR_SUCCESS;
		}

		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo )
		{
			for ( auto &interactionProfile : vecSupportedControllers )
			{
				xrResult = interactionProfile->SuggestBindings( xrInstance, pOtherInfo );

				if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
					LogWarning( "BaseController::SuggestBindings", "Error suggesting bindings for interaction profile %s: %s", interactionProfile->Path(), XrEnumToString( xrResult ) );
			}

			return XR_SUCCESS;
		}

		XrResult AddBinding( XrInstance xrInstance, XrAction action, std::string &sFullBindingPath )
		{
			for ( auto &interactionProfile : vecSupportedControllers )
			{
				xrResult = interactionProfile->AddBinding( xrInstance, action, sFullBindingPath );

				if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
					return xrResult;
			}

			return XR_SUCCESS;
		}

		std::vector< Controller * > vecSupportedControllers;
	};

} // namespace xrlib
