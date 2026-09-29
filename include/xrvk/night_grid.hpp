/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <vector>

namespace xrlib
{
	// Procedural night sky with a glowing grid floor, the CPU side of night_grid.glsl
	// Keep shared floor and light values in sync with the shader
	struct SNightGridConfig
	{
		float eyeHeight = 1.f; // Viewpoint above the floor centre in metres
		float floorSize = 3.f; // Floor half extent in metres, the grid fade scales with it
		bool floor = true;	   // Off for the visible backdrop, where the floor is real geometry
	};

	// Linear RGB in the equirectangular layout BakeEnvironment expects, stars are left to the shader
	std::vector< float > RenderNightGrid( uint32_t width, uint32_t height, const SNightGridConfig &config );
} // namespace xrlib
