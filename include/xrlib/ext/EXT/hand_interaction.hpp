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
#include <xrlib/interaction_profiles.hpp>

namespace xrlib::EXT
{
	class CHandInteraction : public CExtBase
	{
	  public:
		explicit CHandInteraction( XrInstance xrInstance ) : CExtBase( xrInstance, XR_EXT_HAND_INTERACTION_EXTENSION_NAME )
		{
			assert( xrInstance != XR_NULL_HANDLE );
		}

		~CHandInteraction() = default;

		enum class EHandInteractionComponent
		{
			PinchPose,
			PinchPoke,
			PinchValue,
			PinchReady,
			AimActivateValue,
			AimActivateReady,
			GraspValue,
			GraspReady
		};

		static std::string GetInteractionProfilePath() 	{ return "/interaction_profiles/ext/hand_interaction_ext"; }
		static std::string GetBindingPath_PinchPose( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/pinch_ext/pose"; }
		static std::string GetBindingPath_PinchPoke( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/pinch_poke/pose"; }
		static std::string GetBindingPath_PinchValue( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/pinch_ext/value"; }
		static std::string GetBindingPath_PinchReady( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/pinch_ext/ready_ext"; }
		static std::string GetBindingPath_AimActivateValue( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/aim_activate_ext/value"; }
		static std::string GetBindingPath_AimActivateReady( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/aim_activate_ext/ready_ext"; }
		static std::string GetBindingPath_GraspValue( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/grasp_ext/value"; }
		static std::string GetBindingPath_GraspReady( XrHandEXT hand ) { return "/user/hand/" + std::string( hand == XR_HAND_LEFT_EXT ? "left" : "right" ) + "/input/grasp_ext/ready_ext"; }

	  private:	

	};

} // namespace xrlib::EXT

namespace xrlib
{
	struct SHandInteraction : public Controller
	{
		// Implement the Controller interface
		const char *Path() override { return "/interaction_profiles/ext/hand_interaction_ext"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };

		// Custom binding interface
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, EXT::CHandInteraction::EHandInteractionComponent component );
	};
}
