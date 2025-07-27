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

#include <cassert>
#include <string>

#include <xrlib/common.hpp>

namespace xrlib
{
	class CExtBase
	{
	  public:
		CExtBase( XrInstance xrInstance, const std::string& sName ) :
			m_xrInstance( xrInstance ),
			m_sName( sName )
		{
			assert( xrInstance != XR_NULL_HANDLE );
			assert( !sName.empty() );
		}

		~CExtBase() = default;

		std::string GetName() { return m_sName; }

	  protected:
		XrInstance m_xrInstance = XR_NULL_HANDLE;
		std::string m_sName;

	};

} 
