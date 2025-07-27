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

namespace xrlib
{
	class CSystemProperties
	{
	  public:
		~CSystemProperties() = default;

		bool AddSystemProperty( XrStructureType type )
		{
			using NodePtr = std::unique_ptr< XrBaseOutStructure >;
			NodePtr pNewProps;

			switch ( type )
			{
				case XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT:
				{
					using T = XrSystemHandTrackingPropertiesEXT;
					if constexpr ( HasNextField< T > )
					{
						auto *pNewProperty = new T { type, nullptr, XR_FALSE };
						pNewProps.reset( reinterpret_cast< XrBaseOutStructure * >( pNewProperty ) );
					}
					else
					{
						LogWarning( "CSystemProperties", "Struct %d does not have required fields (type, next)", type );
						return false;
					}
					break;
				}
				case XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT:
				{
					using T = XrSystemEyeGazeInteractionPropertiesEXT;
					if constexpr ( HasNextField< T > )
					{
						auto *pNewProperty = new T { type, nullptr, XR_FALSE };
						pNewProps.reset( reinterpret_cast< XrBaseOutStructure * >( pNewProperty ) );
					}
					else
					{
						LogWarning( "CSystemProperties", "Struct %d does not have required fields (type, next)", type );
						return false;
					}
					break;
				}
				case XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES_FB:
				{
					using T = XrSystemPassthroughPropertiesFB;
					if constexpr ( HasNextField< T > )
					{
						auto *pNewProperty = new T { type, nullptr, XR_FALSE };
						pNewProps.reset( reinterpret_cast< XrBaseOutStructure * >( pNewProperty ) );
					}
					else
					{
						LogWarning( "CSystemProperties", "Struct %d does not have required fields (type, next)", type );
						return false;
					}
					break;
				}
				case XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_AND_CONTROLLERS_PROPERTIES_META:
				{
					using T = XrSystemSimultaneousHandsAndControllersPropertiesMETA;
					if constexpr ( HasNextField< T > )
					{
						auto *pNewProperty = new T { type, nullptr, XR_FALSE };
						pNewProps.reset( reinterpret_cast< XrBaseOutStructure * >( pNewProperty ) );
					}
					else
					{
						LogWarning( "CSystemProperties", "Struct %d does not have required fields (type, next)", type );
						return false;
					}
					break;
				}
				default:
				{
					LogWarning( "CSystemProperties", "Unsupported system property type %d", type );
					return false;
				}
			}

			if ( !pNewProps )
				return false;

			AddToChain( std::move( pNewProps ) );
			return true;
		}

		[[nodiscard]] XrBaseOutStructure *GetNextChain() const { return m_pRoot; }

	  protected:
		void AddToChain( std::unique_ptr< XrBaseOutStructure > pNewProps )
		{
			auto isDuplicateType = [ type = pNewProps->type ]( XrBaseOutStructure *pCurrent )
			{
				while ( pCurrent )
				{
					if ( pCurrent->type == type )
					{
						LogWarning( "CSystemProperties", "Duplicate system property type %d found in chain", type );
						return true;
					}
					pCurrent = pCurrent->next;
				}
				return false;
			};

			if ( !m_pRoot )
			{
				m_nodes.push_back( std::move( pNewProps ) );
				m_pRoot = m_nodes.back().get();
				return;
			}

			if ( isDuplicateType( m_pRoot ) )
			{
				LogWarning( "CSystemProperties", "Skipping duplicate system property type %d", pNewProps->type );
				return;
			}

			auto *pPrev = m_pRoot;
			auto *pCurrent = m_pRoot->next;
			while ( pCurrent )
			{
				pPrev = pCurrent;
				pCurrent = pCurrent->next;
			}

			m_nodes.push_back( std::move( pNewProps ) );
			pPrev->next = m_nodes.back().get();

			LogInfo( "CSystemProperties", "Added system property type %d to chain", pPrev->next->type );
		}

		XrBaseOutStructure *m_pRoot = nullptr;
		std::vector< std::unique_ptr< XrBaseOutStructure > > m_nodes;
	};
} // namespace xrlib