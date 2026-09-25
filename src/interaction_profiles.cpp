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


#include <xrlib/interaction_profiles.hpp>
#include <xrlib/log.hpp>

namespace xrlib
{
	XrResult Controller::AddBinding( XrInstance xrInstance, XrAction action, std::string &sFullBindingPath )
	{
		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sFullBindingPath.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "Controller::AddBinding", "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sFullBindingPath.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( "Controller::AddBinding", "Added binding path: (%s) for: (%s)", sFullBindingPath.c_str(), Path() );
		return XR_SUCCESS;
	}

	XrResult Controller::SuggestControllerBindings( XrInstance xrInstance, void *pOtherInfo )
	{
		// Convert interaction profile path to an xrpath
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, Path(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "Controller::SuggestControllerBindings", "Error converting interaction profile to an xrpath (%s): %s", XrEnumToString( xrResult ), Path() );
			return xrResult;
		}

		XrInteractionProfileSuggestedBinding xrInteractionProfileSuggestedBinding { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		xrInteractionProfileSuggestedBinding.next = pOtherInfo;
		xrInteractionProfileSuggestedBinding.interactionProfile = xrPath;
		xrInteractionProfileSuggestedBinding.suggestedBindings = vecSuggestedBindings.data();
		xrInteractionProfileSuggestedBinding.countSuggestedBindings = static_cast< uint32_t >( vecSuggestedBindings.size() );

		xrResult = xrSuggestInteractionProfileBindings( xrInstance, &xrInteractionProfileSuggestedBinding );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "Controller::SuggestControllerBindings", "Error suggesting bindings (%s) for %s", XrEnumToString( xrResult ), Path() );
		}

		LogInfo( "Controller::SuggestControllerBindings", "All action bindings sent to runtime for: (%s)", Path() );
		return xrResult;
	}

	XrResult ValveFrame::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		if ( hand != XR_HAND_LEFT_EXT && hand != XR_HAND_RIGHT_EXT )
			return XR_ERROR_VALIDATION_FAILURE;

		const bool bLeft = hand == XR_HAND_LEFT_EXT;
		const char *pccComponent = nullptr;
		const char *pccQualifier = nullptr;

		switch ( component )
		{
			case Component::GripPose:
				pccComponent = k_pccGripPose;
				break;
			case Component::AimPose:
				pccComponent = k_pccAimPose;
				break;
			case Component::Haptic:
				pccComponent = k_pccHaptic;
				break;
			case Component::Trigger:
				pccComponent = k_pccTrigger;
				break;
			case Component::Squeeze:
				pccComponent = k_pccSqueeze;
				break;
			case Component::AxisControl:
				pccComponent = k_pccThumbstick;
				break;
			case Component::PrimaryButton:
				pccComponent = bLeft ? "/dpad_down" : k_pccA;
				break;
			case Component::SecondaryButton:
				pccComponent = bLeft ? "/dpad_right" : k_pccB;
				break;
			case Component::Menu:
				pccComponent = bLeft ? "/view" : k_pccMenu;
				break;
			case Component::System:
				pccComponent = k_pccSystem;
				break;
			case Component::Shoulder:
				pccComponent = "/shoulder";
				break;
			case Component::DpadUp:
				pccComponent = bLeft ? "/dpad_up" : nullptr;
				break;
			case Component::DpadDown:
				pccComponent = bLeft ? "/dpad_down" : nullptr;
				break;
			case Component::DpadLeft:
				pccComponent = bLeft ? "/dpad_left" : nullptr;
				break;
			case Component::DpadRight:
				pccComponent = bLeft ? "/dpad_right" : nullptr;
				break;
			case Component::ButtonA:
				pccComponent = bLeft ? nullptr : k_pccA;
				break;
			case Component::ButtonB:
				pccComponent = bLeft ? nullptr : k_pccB;
				break;
			case Component::ButtonX:
				pccComponent = bLeft ? nullptr : k_pccX;
				break;
			case Component::ButtonY:
				pccComponent = bLeft ? nullptr : k_pccY;
				break;
			default:
				break;
		}

		// Match BaseController's additive binding behaviour for unavailable components
		if ( !pccComponent )
			return XR_SUCCESS;

		if ( component == Component::GripPose || component == Component::AimPose || component == Component::Haptic )
		{
			if ( qualifier != Qualifier::None )
				return XR_ERROR_PATH_UNSUPPORTED;

			pccQualifier = "";
		}
		else if ( component == Component::AxisControl )
		{
			switch ( qualifier )
			{
				case Qualifier::None:
					pccQualifier = "";
					break;
				case Qualifier::Click:
					pccQualifier = k_pccClick;
					break;
				case Qualifier::Touch:
					pccQualifier = k_pccTouch;
					break;
				case Qualifier::X:
					pccQualifier = k_pccX;
					break;
				case Qualifier::Y:
					pccQualifier = k_pccY;
					break;
				default:
					return XR_ERROR_PATH_UNSUPPORTED;
			}
		}
		else
		{
			const bool bAnalog = component == Component::Trigger || component == Component::Squeeze;

			switch ( qualifier )
			{
				case Qualifier::None:
					pccQualifier = bAnalog ? k_pccValue : k_pccClick;
					break;
				case Qualifier::Click:
					pccQualifier = k_pccClick;
					break;
				case Qualifier::Touch:
					pccQualifier = k_pccTouch;
					break;
				case Qualifier::Value:
					if ( !bAnalog )
						return XR_ERROR_PATH_UNSUPPORTED;

					pccQualifier = k_pccValue;
					break;
				default:
					return XR_ERROR_PATH_UNSUPPORTED;
			}
		}

		std::string sBinding = bLeft ? k_pccLeftHand : k_pccRightHand;
		sBinding += component == Component::Haptic ? k_pccOutput : k_pccInput;
		sBinding += pccComponent;
		sBinding += pccQualifier;

		return Controller::AddBinding( xrInstance, action, sBinding );
	}

	XrResult ValveIndex::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
			{
				sBinding += k_pccTrigger;
				if ( qualifier == Controller::Qualifier::Value )
					sBinding += k_pccValue;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::PrimaryButton:
			{
				sBinding += k_pccA;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::SecondaryButton:
			{
				sBinding += k_pccB;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccThumbstick;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
			}
			break;
			case Controller::Component::Squeeze:
			{
				sBinding += k_pccSqueeze;
				if ( qualifier == Controller::Qualifier::Value )
					sBinding += k_pccValue;
				else
					sBinding += k_pccForce;
			}
			break;
			case Controller::Component::Menu:
			case Controller::Component::System:
			{
				sBinding += k_pccSystem;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
				break;
			default:
				sBinding.clear();
				break;
		}

		// If binding can't be mapped, don't add
		// We'll then let the dev do "additive" bindings instead using AddBinding( XrInstance xrInstance, XrAction action, FULL INPUT PAHT )
		if ( sBinding.empty() )
		{
			LogInfo( "ValveIndex::AddBinding", "Skipping (%s) as there's no equivalent controller component for this binding", Path() );
			return XR_SUCCESS;
		}

		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sBinding.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "ValveIndex::AddBinding", "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sBinding.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( "ValveIndex::AddBinding", "Added binding path: (%s) for: (%s)", sBinding.c_str(), Path() );
		return XR_SUCCESS;
	}

	XrResult OculusTouch::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
			{
				sBinding += k_pccTrigger;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccValue;
			}
			break;
			case Controller::Component::PrimaryButton:
			{
				sBinding += ( hand == XR_HAND_LEFT_EXT ) ? k_pccX : k_pccA;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::SecondaryButton:
			{
				sBinding += ( hand == XR_HAND_LEFT_EXT ) ? k_pccY : k_pccB;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccThumbstick;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
				else if ( qualifier == Controller::Qualifier::None )
					break;
			}
			break;
			case Controller::Component::Squeeze:
			{
				sBinding += k_pccSqueeze;
				if ( qualifier == Controller::Qualifier::Value )
					sBinding += k_pccValue;
			}
			break;
			case Controller::Component::Menu:
				if ( hand == XR_HAND_LEFT_EXT )
				{
					sBinding += k_pccMenu;
					sBinding += k_pccClick;
				}
				else
				{
					sBinding.clear();
				}
				break;
			case Controller::Component::System:
				if ( hand == XR_HAND_RIGHT_EXT )
				{
					sBinding += k_pccSystem;
					sBinding += k_pccClick;
				}
				else
				{
					sBinding.clear();
				}
				break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
				break;
			default:
				sBinding.clear();
				break;
		}

		// If binding can't be mapped, don't add
		// We'll then let the dev do "additive" bindings instead using AddBinding( XrInstance xrInstance, XrAction action, FULL INPUT PAHT )
		if ( sBinding.empty() )
		{
			LogInfo( "ValveIndex::AddBinding", "Skipping (%s) as there's no equivalent controller component for this binding", Path() );
			return XR_SUCCESS;
		}

		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sBinding.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "ValveIndex::AddBinding", "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sBinding.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( "ValveIndex::AddBinding", "Added binding path: (%s) for: (%s)", sBinding.c_str(), Path() );
		return XR_SUCCESS;
	}

	XrResult HTCVive::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
			{
				sBinding += k_pccTrigger;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else
					sBinding += k_pccValue;
			}
			break;
			case Controller::Component::PrimaryButton:
			case Controller::Component::SecondaryButton:
				sBinding.clear();
				break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccTrackpad;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
				else if ( qualifier == Controller::Qualifier::None )
					break;
			}
			break;
			case Controller::Component::Squeeze:
				sBinding += k_pccSqueeze;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Menu:
				sBinding += k_pccMenu;
				sBinding += k_pccClick;
				break;
			case Controller::Component::System:
				sBinding += k_pccSystem;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
				break;
			default:
				sBinding.clear();
				break;
		}

		// If binding can't be mapped, don't add
		// We'll then let the dev do "additive" bindings instead using AddBinding( XrInstance xrInstance, XrAction action, FULL INPUT PAHT )
		if ( sBinding.empty() )
		{
			LogInfo( "HTCVive::AddBinding", "Skipping (%s) as there's no equivalent controller component for this binding", Path() );
			return XR_SUCCESS;
		}

		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sBinding.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "HTCVive::AddBinding", "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sBinding.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( "HTCVive::AddBinding", "Added binding path: (%s) for: (%s)", sBinding.c_str(), Path() );
		return XR_SUCCESS;
	}

	XrResult MicrosoftMixedReality::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
				sBinding += k_pccTrigger;
				sBinding += k_pccValue;
				break;
			case Controller::Component::PrimaryButton:
			case Controller::Component::SecondaryButton:
				sBinding.clear();
				break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccThumbstick;
				if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
				else if ( qualifier == Controller::Qualifier::None )
					break;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::Squeeze:
				sBinding += k_pccSqueeze;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Menu:
				sBinding += k_pccMenu;
				sBinding += k_pccClick;
				break;
			case Controller::Component::System:
				sBinding += k_pccSystem;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
				break;
			default:
				sBinding.clear();
				break;
		}

		// If binding can't be mapped, don't add
		// We'll then let the dev do "additive" bindings instead using AddBinding( XrInstance xrInstance, XrAction action, FULL INPUT PAHT )
		if ( sBinding.empty() )
		{
			LogInfo( "HTCVive::AddBinding", "Skipping (%s) as there's no equivalent controller component for this binding", Path() );
			return XR_SUCCESS;
		}

		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sBinding.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "HTCVive::AddBinding", "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sBinding.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( "HTCVive::AddBinding", "Added binding path: (%s) for: (%s)", sBinding.c_str(), Path() );
		return XR_SUCCESS;
	}

} // namespace xrlib
