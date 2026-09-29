/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <xrvk/environment.hpp>
#include <xrvk/night_grid.hpp>
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

	int PrepareBackground( std::span< char *const > args )
	{
		const Options options = ParseArguments( args, { "--source", "--output" }, {} );
		const std::vector< uint8_t > source = ReadFile( Required( options, "--source" ) );
		const std::vector< uint8_t > bytes = EncodeBackground( PrepareBackgroundHDR( source ) );

		const std::filesystem::path output = Required( options, "--output" );
		WriteFile( output, bytes );

		std::cout << "Prepared background: " << output.string() << '\n';
		return 0;
	}

	int PrepareNightGrid( std::span< char *const > args )
	{
		const Options options = ParseArguments( args, { "--ibl", "--sky", "--width", "--floor-size", "--diffuse-size", "--specular-size", "--lut-size", "--samples" }, {} );
		const SEnvironmentBakeConfig config {
			ParsePositive( Required( options, "--diffuse-size" ) ),
			ParsePositive( Required( options, "--specular-size" ) ),
			ParsePositive( Required( options, "--lut-size" ) ),
			ParsePositive( Required( options, "--samples" ) ) };

		const uint32_t width = ParsePositive( Required( options, "--width" ) );
		const uint32_t height = width / 2;

		// Lighting sees the floor from a model's height, the visible sky leaves it to real geometry
		SNightGridConfig lighting;
		lighting.floorSize = ParsePositiveFloat( Required( options, "--floor-size" ) );
		const std::filesystem::path ibl = Required( options, "--ibl" );
		WriteFile( ibl, EncodeEnvironment( BakeEnvironment( RenderNightGrid( width, height, lighting ), width, height, config ) ) );

		SNightGridConfig backdrop;
		backdrop.floor = false;
		const std::filesystem::path sky = Required( options, "--sky" );
		WriteFile( sky, EncodeBackground( xrlib::PrepareBackground( RenderNightGrid( width, height, backdrop ), width, height ) ) );

		std::cout << "Prepared night grid: " << ibl.string() << ", " << sky.string() << '\n';
		return 0;
	}
} // namespace xrlib::tools
