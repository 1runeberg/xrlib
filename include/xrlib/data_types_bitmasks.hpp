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


#pragma once
#include <stdint.h>

namespace xrlib
{
	struct Flag8
	{
		uint8_t flag = 0;

		// True
		void Set( int nflag ) { flag |= nflag; }

		// False
		void Reset( int nflag ) { flag &= ~nflag; }

		// Flip flop flag value
		void Flip( int nflag ) { flag ^= nflag; }

		// Check flag
		bool IsSet( int nflag ) { return ( flag & nflag ) ==  nflag; }

		// Checks for any flag set
		bool IsAnySet( int nflags ) { return ( flag & nflags ) != 0; }

	};

	struct Flag16
	{
		uint16_t flag = 0;

		// True
		void Set( int nflag ) { flag |= nflag; }

		// False
		void Reset( int nflag ) { flag &= ~nflag; }

		// Flip flop flag value
		void Flip( int nflag ) { flag ^= nflag; }

		// Check flag
		bool IsSet( int nflag ) { return ( flag & nflag ) == nflag; }

		// Checks for any flag set
		bool IsAnySet( int nflags ) { return ( flag & nflags ) != 0; }
	};
} // namespace xrlib
