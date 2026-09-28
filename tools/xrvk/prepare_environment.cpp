/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <xrvk/environment.hpp>
#include "tools.hpp"

#include <iostream>

namespace xrlib::tools
{
	int PrepareEnvironment( std::span< char *const > args )
	{
		const Options options = ParseArguments( args, { "--source", "--output", "--diffuse-size", "--specular-size", "--lut-size", "--samples" }, {} );
		const SEnvironmentBakeConfig config {
			ParsePositive( Required( options, "--diffuse-size" ) ),
			ParsePositive( Required( options, "--specular-size" ) ),
			ParsePositive( Required( options, "--lut-size" ) ),
			ParsePositive( Required( options, "--samples" ) ) };

		const std::vector< uint8_t > source = ReadFile( Required( options, "--source" ) );
		const std::vector< uint8_t > bytes = EncodeEnvironment( BakeEnvironmentHDR( source, config ) );

		const std::filesystem::path output = Required( options, "--output" );
		WriteFile( output, bytes );

		std::cout << "Prepared environment: " << output.string() << '\n';
		return 0;
	}
} // namespace xrlib::tools
