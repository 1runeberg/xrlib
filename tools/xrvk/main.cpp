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
	}
	catch ( const std::exception &error )
	{
		std::cerr << "xrvk-tools " << command << " failed: " << error.what() << '\n';
		return 1;
	}

	std::cerr << "Usage:\n"
				 "  xrvk-tools gltf --source model.gltf|model.glb --output directory [--ktx path] [--force]\n"
				 "      Prepares a model's textures as uncompressed KTX2 mip chains using KTX-Software\n"
				 "  xrvk-tools environment --source environment.hdr --output environment.ibl\n"
				 "      --diffuse-size n --specular-size n --lut-size n --samples n\n"
				 "      Bakes image based lighting from an equirectangular Radiance HDR\n";
	return 1;
}
