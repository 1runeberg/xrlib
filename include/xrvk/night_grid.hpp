/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
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

	// Linear RGB in the equirectangular layout BakeEnvironment and PrepareBackground expect
	std::vector< float > RenderNightGrid( uint32_t width, uint32_t height, const SNightGridConfig &config );

	// A fixed star in the same environment space as the baked sky and lighting
	struct SNightGridStar
	{
		std::array< float, 3 > direction; // Unit vector
		std::array< float, 3 > radiance;  // Linear RGB at the centre
		float size;						  // Angular radius in radians
	};

	// The same star field each call, drawn as geometry so stars stay sharp at any sky resolution
	std::vector< SNightGridStar > NightGridStars();
} // namespace xrlib
