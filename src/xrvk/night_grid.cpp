/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <xrvk/night_grid.hpp>

namespace xrlib
{
	namespace
	{
		constexpr float k_Pi = 3.14159265358979323846f;

		struct SColor
		{
			float r, g, b;
		};

		SColor operator+( SColor a, SColor b ) { return { a.r + b.r, a.g + b.g, a.b + b.b }; }
		SColor operator*( SColor a, float b ) { return { a.r * b, a.g * b, a.b * b }; }
		SColor Mix( SColor a, SColor b, float t ) { return a * ( 1.f - t ) + b * t; }

		float Smoothstep( float edge0, float edge1, float x )
		{
			const float t = std::clamp( ( x - edge0 ) / ( edge1 - edge0 ), 0.f, 1.f );
			return t * t * ( 3.f - 2.f * t );
		}

		// Shared with night_grid.glsl
		constexpr float k_MajorWidth = .03f;
		constexpr float k_MinorWidth = .04f;
		constexpr float k_MinorSpacing = 10.f;
		constexpr float k_FadeStart = 1.f / 3.f; // Fractions of the floor half extent
		constexpr float k_FadeSoftness = 2.f / 3.f;
		constexpr SColor k_MajorColor { 1.f, 1.f, 0.f };
		constexpr SColor k_MinorColor { .5f, .5f, .5f };
		constexpr SColor k_GlowColor { .12f, .16f, .45f };
		constexpr float k_GlowHeight = .12f;
		constexpr float k_GroundGlowHeight = .01f; // Below the horizon, so the black floor fades into black ground
		constexpr SColor k_PanelColor { .16f, .17f, .22f };
		constexpr float k_PanelInner = .98f, k_PanelOuter = .9f; // Cosines from the zenith
		constexpr float k_NebulaStrength = .3f;

		// Star field, placed on a grid of cells over each cube face
		constexpr uint32_t k_StarCells = 160;
		constexpr float k_StarDensity = .07f;
		constexpr SColor k_StarCool { .8f, .85f, 1.f };
		constexpr SColor k_StarWarm { 1.f, .9f, .75f };

		std::array< uint32_t, 3 > StarHash( uint32_t x, uint32_t y, uint32_t z )
		{
			std::array< uint32_t, 3 > v { x * 1664525u + 1013904223u, y * 1664525u + 1013904223u, z * 1664525u + 1013904223u };
			auto Mix = [ & ]()
			{
				v[ 0 ] += v[ 1 ] * v[ 2 ];
				v[ 1 ] += v[ 2 ] * v[ 0 ];
				v[ 2 ] += v[ 0 ] * v[ 1 ];
			};

			Mix();
			for ( auto &value : v )
				value ^= value >> 16;
			Mix();

			return v;
		}

		// The original floor profile, half strength at the centre and zero at the half width
		float GridLine( float position, float width )
		{
			const float distance = std::abs( position - std::floor( position + .5f ) );
			const float halfWidth = width * .5f;
			return 1.f - Smoothstep( -halfWidth, halfWidth, distance );
		}

		SColor Floor( float x, float z )
		{
			const float major = std::max( GridLine( x, k_MajorWidth ), GridLine( z, k_MajorWidth ) );
			const float minor = std::max( GridLine( x * k_MinorSpacing, k_MinorWidth ), GridLine( z * k_MinorSpacing, k_MinorWidth ) ) * ( 1.f - major );
			return Mix( Mix( SColor {}, k_MinorColor * .5f, minor ), k_MajorColor, major );
		}

		// Nebula field from the demo 6 sky shader, sampled at time zero
		float Field( float x, float y, float z, float s, int iterations )
		{
			float accum = s / 4.f, prev = 0.f, total = 0.f;
			for ( int i = 0; i < iterations; ++i )
			{
				const float magnitude = x * x + y * y + z * z;
				x = std::abs( x ) / magnitude - .5f;
				y = std::abs( y ) / magnitude - .4f;
				z = std::abs( z ) / magnitude - 1.487f;

				const float weight = std::exp( -float( i ) / 5.f );
				accum += weight * std::exp( -9.025f * std::pow( std::abs( magnitude - prev ), 2.2f ) );
				total += weight;
				prev = magnitude;
			}

			return std::max( 0.f, 5.2f * accum / total - .65f );
		}

		SColor Nebula( float dx, float dy, float dz )
		{
			if ( dy <= 0.f )
				return {};

			// Project onto the original overhead plane, 100 m up and 5 km across
			const float u = dx / dy * .1f, v = dz / dy * .1f;
			const float t = Field( u + .8f, v - 1.3f, 0.f, .15f, 13 );
			const float vignette = std::clamp( ( 1.f - std::exp( ( std::abs( u ) - 1.f ) * 6.f ) ) * ( 1.f - std::exp( ( std::abs( v ) - 1.f ) * 6.f ) ), 0.f, 1.f );
			const float t2 = Field( u / 4.6f, v / 4.6f, 4.f, .9f, 18 );

			const SColor clouds = SColor { 5.5f * t2 * t2 * t2, 2.1f * t2 * t2, 2.2f * t2 * .45f } * ( .5f + ( .2f - .5f ) * vignette );
			const SColor core = SColor { .225f * t * t * t, .48f * t * t, .9f * t } * ( .9f + .1f * vignette );
			return ( core + clouds ) * ( k_NebulaStrength * Smoothstep( .05f, .35f, dy ) );
		}

		// Horizon glow and a soft overhead panel keep models readable, below the horizon is black ground
		SColor Sky( float dx, float dy, float dz )
		{
			const SColor glow = k_GlowColor * std::exp( -std::abs( dy ) / ( dy >= 0.f ? k_GlowHeight : k_GroundGlowHeight ) );
			const SColor panel = k_PanelColor * Smoothstep( k_PanelOuter, k_PanelInner, dy );
			return Nebula( dx, dy, dz ) + glow + panel;
		}

		SColor Environment( float dx, float dy, float dz, const SNightGridConfig &config )
		{
			const SColor sky = Sky( dx, dy, dz );
			if ( !config.floor || dy >= 0.f )
				return sky;

			const float distance = config.eyeHeight / -dy;
			const float x = dx * distance, z = dz * distance;
			const float alpha = 1.f - Smoothstep( k_FadeStart, k_FadeStart + k_FadeSoftness, std::sqrt( x * x + z * z ) / config.floorSize );
			return alpha > 0.f ? Mix( sky, Floor( x, z ), alpha ) : sky;
		}
	} // namespace

	std::vector< float > RenderNightGrid( uint32_t width, uint32_t height, const SNightGridConfig &config )
	{
		if ( !width || !height || width > 16384 || height > 16384 || !std::isfinite( config.eyeHeight ) || config.eyeHeight <= 0.f ||
			 !std::isfinite( config.floorSize ) || config.floorSize <= 0.f )
			throw std::invalid_argument( "Invalid night grid size, eye height or floor size" );

		std::vector< float > rgb( size_t( width ) * height * 3 );
		for ( uint32_t y = 0; y < height; ++y )
			for ( uint32_t x = 0; x < width; ++x )
			{

				// Thin floor lines need supersampling, the sky is smooth
				const bool floor = config.floor && ( y + 1.f ) / height > .5f;
				const int samples = floor ? 4 : 1;
				SColor sum {};
				for ( int sy = 0; sy < samples; ++sy )
					for ( int sx = 0; sx < samples; ++sx )
					{

						// Same mapping as the IBL bake, north at the first row and +X at u = .5
						const float u = ( x + ( sx + .5f ) / samples ) / width;
						const float v = ( y + ( sy + .5f ) / samples ) / height;
						const float theta = v * k_Pi, phi = ( u - .5f ) * 2.f * k_Pi;
						sum = sum + Environment( std::sin( theta ) * std::cos( phi ), std::cos( theta ), std::sin( theta ) * std::sin( phi ), config );
					}

				const SColor color = sum * ( 1.f / float( samples * samples ) );
				float *pixel = rgb.data() + ( size_t( y ) * width + x ) * 3;
				pixel[ 0 ] = color.r;
				pixel[ 1 ] = color.g;
				pixel[ 2 ] = color.b;
			}

		return rgb;
	}

	std::vector< SNightGridStar > NightGridStars()
	{
		std::vector< SNightGridStar > stars;
		constexpr float cellAngle = 2.f / k_StarCells; // Near a face centre

		for ( uint32_t face = 0; face < 6; ++face )
			for ( uint32_t cy = 0; cy < k_StarCells; ++cy )
				for ( uint32_t cx = 0; cx < k_StarCells; ++cx )
				{
					const auto hash = StarHash( cx, cy, face );
					float random[ 3 ];
					for ( int i = 0; i < 3; ++i )
						random[ i ] = float( double( hash[ i ] ) / 4294967295.0 );

					if ( random[ 0 ] > k_StarDensity )
						continue;

					// Face axes match the cube face lookup the stars were designed on
					const float u = ( cx + .25f + .5f * random[ 1 ] ) / k_StarCells * 2.f - 1.f;
					const float v = ( cy + .25f + .5f * random[ 2 ] ) / k_StarCells * 2.f - 1.f;
					const float sign = ( face & 1 ) ? -1.f : 1.f;
					float x = u, y = v, z = sign;
					if ( face < 2 )
						x = sign, y = u, z = v;
					else if ( face < 4 )
						x = u, y = sign, z = v;

					const float length = std::sqrt( x * x + y * y + z * z );
					x /= length, y /= length, z /= length;
					if ( y < .02f )
						continue;

					const float brightness = ( .4f + 2.2f * random[ 2 ] * random[ 2 ] ) * Smoothstep( .02f, .2f, y );
					const SColor radiance = Mix( k_StarCool, k_StarWarm, random[ 1 ] ) * brightness;
					stars.push_back( { { x, y, z }, { radiance.r, radiance.g, radiance.b }, ( .03f + .02f * random[ 1 ] ) * cellAngle } );
				}

		return stars;
	}
} // namespace xrlib
