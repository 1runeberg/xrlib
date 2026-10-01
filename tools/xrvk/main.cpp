/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tools.hpp"

#include <algorithm>
#include <exception>
#include <iostream>

int main( int argc, char **argv )
{
	const std::string_view command = argc > 1 ? argv[ 1 ] : "";
	const std::span< char *const > args( argv + std::min( argc, 2 ), argv + argc );

	try
	{
		if ( command == "gltf" )
			return xrlib::tools::PrepareGltf( args );

		if ( command == "environment" )
			return xrlib::tools::PrepareEnvironment( args );

		if ( command == "background" )
			return xrlib::tools::PrepareBackground( args );

		if ( command == "night-grid" )
			return xrlib::tools::PrepareNightGrid( args );
	}
	catch ( const std::exception &error )
	{
		std::cerr << "xrvk-tools " << command << " failed: " << error.what() << '\n';
		return 1;
	}

	std::cerr << "Usage:\n"
				 "  xrvk-tools gltf --source model.gltf|model.glb --output directory [--ktx path] [--encode none|astc] [--force]\n"
				 "      Prepares a model's textures as KTX2 mip chains using KTX-Software (uncompressed or ASTC for mobile GPUs)\n"
				 "  xrvk-tools environment --source environment.hdr --output environment.ibl\n"
				 "      --diffuse-size n --specular-size n --lut-size n --samples n\n"
				 "      Bakes image based lighting from an equirectangular Radiance HDR\n"
				 "  xrvk-tools background --source environment.hdr --output environment.sky\n"
				 "      Packs an equirectangular Radiance HDR as an E5B9G9R9 backdrop cubemap\n"
				 "  xrvk-tools night-grid --ibl night_grid.ibl --sky night_grid.sky --width n --floor-size metres\n"
				 "      --diffuse-size n --specular-size n --lut-size n --samples n [--star-reflections scale]\n"
				 "      Bakes the procedural night grid's lighting and visible sky\n";
	return 1;
}
