/* 
 * Copyright 2023-24 Beyond Reality Labs Ltd (https://beyondreality.io)
 * Copyright 2021-24 Rune Berg (GitHub: https://github.com/1runeberg, YT: https://www.youtube.com/@1RuneBerg, X: https://twitter.com/1runeberg, BSky: https://runeberg.social)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
