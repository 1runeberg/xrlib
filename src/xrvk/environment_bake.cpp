/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <stb/stb_image.h>
#include <stdexcept>
#include <xrvk/environment.hpp>

namespace xrlib
{
	namespace
	{
		constexpr float k_Pi = 3.14159265358979323846f;
		using V = XrVector3f;

		V Add( V a, V b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
		V Scale( V a, float b ) { return { a.x * b, a.y * b, a.z * b }; }
		float Dot( V a, V b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
		V Cross( V a, V b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
		V Normalise( V a ) { return Scale( a, 1.f / std::sqrt( std::max( Dot( a, a ), 1e-20f ) ) ); }

		uint16_t Half( float value )
		{
			const uint32_t bits = std::bit_cast< uint32_t >( std::clamp( value, 0.f, 65504.f ) );
			const int exponent = int( ( bits >> 23 ) & 255 ) - 127 + 15;
			const uint32_t mantissa = bits & 0x7fffff;
			if ( exponent <= 0 )
			{
				if ( exponent < -10 )
					return 0;
				const uint32_t shift = 14 - exponent;
				const uint32_t significand = mantissa | 0x800000;
				return uint16_t( ( significand + ( ( 1u << ( shift - 1 ) ) - 1 ) + ( ( significand >> shift ) & 1 ) ) >> shift );
			}

			const uint32_t rounded = mantissa + 0xfff + ( ( mantissa >> 13 ) & 1 );
			return uint16_t( ( uint32_t( exponent ) << 10 ) + ( rounded >> 13 ) );
		}

		void Append( std::vector< uint16_t > &pixels, V value ) { pixels.insert( pixels.end(), { Half( value.x ), Half( value.y ), Half( value.z ), Half( 1.f ) } ); }

		float RadicalInverse( uint32_t bits )
		{
			bits = ( bits << 16 ) | ( bits >> 16 );
			bits = ( ( bits & 0x55555555u ) << 1 ) | ( ( bits & 0xaaaaaaaau ) >> 1 );
			bits = ( ( bits & 0x33333333u ) << 2 ) | ( ( bits & 0xccccccccu ) >> 2 );
			bits = ( ( bits & 0x0f0f0f0fu ) << 4 ) | ( ( bits & 0xf0f0f0f0u ) >> 4 );
			bits = ( ( bits & 0x00ff00ffu ) << 8 ) | ( ( bits & 0xff00ff00u ) >> 8 );
			return float( double( bits ) / 4294967296.0 );
		}

		V ToWorld( V sample, V normal )
		{
			const V up = std::abs( normal.z ) < .999f ? V { 0, 0, 1 } : V { 1, 0, 0 };
			const V tangent = Normalise( Cross( up, normal ) );
			return Add( Add( Scale( tangent, sample.x ), Scale( Cross( normal, tangent ), sample.y ) ), Scale( normal, sample.z ) );
		}

		V GGX( float u, float v, float roughness )
		{
			const float alpha = roughness * roughness;
			const float cosine = std::sqrt( ( 1.f - v ) / std::max( 1.f + ( alpha * alpha - 1.f ) * v, 1e-7f ) );
			const float sine = std::sqrt( std::max( 0.f, 1.f - cosine * cosine ) );
			return { std::cos( 2.f * k_Pi * u ) * sine, std::sin( 2.f * k_Pi * u ) * sine, cosine };
		}

		V CubeDirection( uint32_t face, float u, float v )
		{
			switch ( face )
			{
				case 0:
					return Normalise( { 1, -v, -u } );
				case 1:
					return Normalise( { -1, -v, u } );
				case 2:
					return Normalise( { u, 1, v } );
				case 3:
					return Normalise( { u, -1, -v } );
				case 4:
					return Normalise( { u, -v, 1 } );
				default:
					return Normalise( { -u, -v, -1 } );
			}
		}

		V SampleHDR( std::span< const float > rgb, uint32_t width, uint32_t height, V direction )
		{
			const float x = ( std::atan2( direction.z, direction.x ) / ( 2.f * k_Pi ) + .5f ) * width - .5f;
			const float y = std::acos( std::clamp( direction.y, -1.f, 1.f ) ) / k_Pi * height - .5f;
			const int ix = int( std::floor( x ) ), iy = int( std::floor( y ) );
			auto Fetch = [ & ]( int px, int py )
			{
				px = ( px % int( width ) + int( width ) ) % int( width );
				py = std::clamp( py, 0, int( height ) - 1 );
				const size_t i = ( size_t( py ) * width + px ) * 3;
				return V { rgb[ i ], rgb[ i + 1 ], rgb[ i + 2 ] };
			};
			const float tx = x - ix, ty = y - iy;
			return Add( Scale( Add( Scale( Fetch( ix, iy ), 1.f - tx ), Scale( Fetch( ix + 1, iy ), tx ) ), 1.f - ty ), Scale( Add( Scale( Fetch( ix, iy + 1 ), 1.f - tx ), Scale( Fetch( ix + 1, iy + 1 ), tx ) ), ty ) );
		}

		bool ValidImage( const SEnvironmentImage &image, uint32_t layers )
		{
			if ( !image.size || !image.levels || image.levels > std::bit_width( image.size ) )
				return false;
			uint64_t texels = 0;
			for ( uint32_t level = 0, size = image.size; level < image.levels; ++level, size = std::max( 1u, size / 2 ) )
			{
				const uint64_t count = uint64_t( size ) * size;
				if ( count > UINT64_MAX / layers / 4 || texels > UINT64_MAX - count * layers * 4 )
					return false;
				texels += count * layers * 4;
			}
			return texels == image.pixels.size();
		}
	} // namespace

	bool IsValidEnvironment( const SEnvironmentData &data )
	{
		return ValidImage( data.diffuse, 6 ) && ValidImage( data.specular, 6 ) && ValidImage( data.brdf, 1 ) && data.diffuse.levels == 1 && data.brdf.levels == 1;
	}

	SEnvironmentData BakeEnvironment( std::span< const float > rgb, uint32_t width, uint32_t height, const SEnvironmentBakeConfig &config )
	{
		if ( !width || !height || width > INT32_MAX || height > INT32_MAX || uint64_t( width ) * height > SIZE_MAX / 3 || rgb.size() != size_t( width ) * height * 3 || !config.diffuseSize || !std::has_single_bit( config.specularSize ) ||
			 !config.lutSize || !config.sampleCount )
			throw std::invalid_argument( "Invalid environment dimensions or bake config" );
		if ( std::any_of( rgb.begin(), rgb.end(), []( float value ) { return !std::isfinite( value ) || value < 0.f || value > 65504.f; } ) )
			throw std::invalid_argument( "Environment radiance must be finite, nonnegative and representable as half floats" );

		SEnvironmentData data { { config.diffuseSize, 1, {} }, { config.specularSize, uint32_t( std::bit_width( config.specularSize ) ), {} }, { config.lutSize, 1, {} } };
		for ( uint32_t map = 0; map < 2; ++map )
		{
			auto &image = map == 0 ? data.diffuse : data.specular;
			for ( uint32_t level = 0, size = image.size; level < image.levels; ++level, size = std::max( 1u, size / 2 ) )
			{
				const float roughness = image.levels > 1 ? float( level ) / ( image.levels - 1 ) : 0.f;
				for ( uint32_t face = 0; face < 6; ++face )
					for ( uint32_t y = 0; y < size; ++y )
						for ( uint32_t x = 0; x < size; ++x )
						{
							const V normal = CubeDirection( face, 2.f * ( x + .5f ) / size - 1.f, 2.f * ( y + .5f ) / size - 1.f );
							V sum {};
							float weight = 0.f;
							if ( map == 1 && level == 0 )
							{
								Append( image.pixels, SampleHDR( rgb, width, height, normal ) );
								continue;
							}

							for ( uint32_t sample = 0; sample < config.sampleCount; ++sample )
							{
								const float u = ( sample + .5f ) / config.sampleCount, v = RadicalInverse( sample );
								V direction;
								float sampleWeight = 1.f;
								if ( map == 0 )
								{
									const float radius = std::sqrt( v );
									direction = ToWorld( { radius * std::cos( 2.f * k_Pi * u ), radius * std::sin( 2.f * k_Pi * u ), std::sqrt( 1.f - v ) }, normal );
								}
								else
								{
									const V half = ToWorld( GGX( u, v, roughness ), normal );
									direction = Add( Scale( half, 2.f * Dot( normal, half ) ), Scale( normal, -1.f ) );
									sampleWeight = std::max( Dot( normal, direction ), 0.f );
								}
								sum = Add( sum, Scale( SampleHDR( rgb, width, height, Normalise( direction ) ), sampleWeight ) );
								weight += sampleWeight;
							}
							Append( image.pixels, Scale( sum, 1.f / std::max( weight, 1e-7f ) ) );
						}
			}
		}

		// Split-sum GGX integration with the same correlated Smith visibility as direct light
		for ( uint32_t y = 0; y < config.lutSize; ++y )
			for ( uint32_t x = 0; x < config.lutSize; ++x )
			{
				const float noV = ( x + .5f ) / config.lutSize, roughness = ( y + .5f ) / config.lutSize;
				const float a2 = std::pow( roughness, 4.f );
				const V view { std::sqrt( 1.f - noV * noV ), 0, noV };
				float a = 0.f, b = 0.f;
				for ( uint32_t sample = 0; sample < config.sampleCount; ++sample )
				{
					const V half = GGX( ( sample + .5f ) / config.sampleCount, RadicalInverse( sample ), roughness );
					const float voH = std::max( Dot( view, half ), 0.f );
					const V light = Add( Scale( half, 2.f * voH ), Scale( view, -1.f ) );
					const float noL = std::max( light.z, 0.f );
					if ( noL <= 0.f )
						continue;
					const float visibility = .5f / std::max( noL * std::sqrt( noV * noV * ( 1.f - a2 ) + a2 ) + noV * std::sqrt( noL * noL * ( 1.f - a2 ) + a2 ), 1e-7f );
					const float weight = 4.f * visibility * noL * voH / std::max( half.z, 1e-7f );
					const float fresnel = std::pow( 1.f - voH, 5.f );
					a += ( 1.f - fresnel ) * weight;
					b += fresnel * weight;
				}
				Append( data.brdf.pixels, { a / config.sampleCount, b / config.sampleCount, 0 } );
			}
		return data;
	}

	SEnvironmentData BakeEnvironmentHDR( std::span< const uint8_t > encodedHDR, const SEnvironmentBakeConfig &config )
	{
		if ( encodedHDR.empty() || encodedHDR.size() > INT32_MAX || !stbi_is_hdr_from_memory( encodedHDR.data(), int( encodedHDR.size() ) ) )
			throw std::invalid_argument( "Expected a Radiance HDR environment" );

		int width = 0, height = 0, channels = 0;
		std::unique_ptr< float, decltype( &stbi_image_free ) > pixels( stbi_loadf_from_memory( encodedHDR.data(), int( encodedHDR.size() ), &width, &height, &channels, 3 ), stbi_image_free );
		if ( !pixels || width <= 0 || height <= 0 )
			throw std::invalid_argument( "Couldn't decode HDR environment" );
		return BakeEnvironment( { pixels.get(), size_t( width ) * height * 3 }, width, height, config );
	}

	std::vector< uint8_t > EncodeEnvironment( const SEnvironmentData &data )
	{
		if ( !IsValidEnvironment( data ) )
			throw std::invalid_argument( "Invalid environment images" );

		std::vector< uint8_t > bytes { 'X', 'R', 'V', 'K', 'I', 'B', 'L', '1' };
		auto AppendInteger = [ & ]( uint64_t value, uint32_t count )
		{
			for ( uint32_t i = 0; i < count; ++i )
				bytes.push_back( uint8_t( value >> ( 8 * i ) ) );
		};
		for ( const auto *image : { &data.diffuse, &data.specular, &data.brdf } )
		{
			AppendInteger( image->size, 4 );
			AppendInteger( image->levels, 4 );
			AppendInteger( image->pixels.size(), 8 );
			for ( uint16_t pixel : image->pixels )
				AppendInteger( pixel, 2 );
		}
		return bytes;
	}

	SEnvironmentData DecodeEnvironment( std::span< const uint8_t > bytes )
	{
		constexpr std::array< uint8_t, 8 > magic { 'X', 'R', 'V', 'K', 'I', 'B', 'L', '1' };
		if ( bytes.size() < magic.size() || !std::equal( magic.begin(), magic.end(), bytes.begin() ) )
			throw std::invalid_argument( "Invalid environment payload" );

		size_t offset = magic.size();
		auto ReadInteger = [ & ]( uint32_t count )
		{
			if ( count > bytes.size() - offset )
				throw std::invalid_argument( "Truncated environment payload" );
			uint64_t value = 0;
			for ( uint32_t i = 0; i < count; ++i )
				value |= uint64_t( bytes[ offset++ ] ) << ( 8 * i );
			return value;
		};
		SEnvironmentData data;
		for ( auto *image : { &data.diffuse, &data.specular, &data.brdf } )
		{
			image->size = uint32_t( ReadInteger( 4 ) );
			image->levels = uint32_t( ReadInteger( 4 ) );
			const uint64_t count = ReadInteger( 8 );
			if ( count > ( bytes.size() - offset ) / 2 )
				throw std::invalid_argument( "Truncated environment pixels" );
			image->pixels.resize( size_t( count ) );
			for ( auto &pixel : image->pixels )
			{
				pixel = uint16_t( ReadInteger( 2 ) );
				if ( ( pixel & 0x8000 ) || ( pixel & 0x7c00 ) == 0x7c00 )
					throw std::invalid_argument( "Invalid environment radiance" );
			}
		}
		if ( offset != bytes.size() || !IsValidEnvironment( data ) )
			throw std::invalid_argument( "Invalid environment image layout" );
		return data;
	}

	SEnvironmentData CEnvironmentLighting::DisabledData()
	{
		SEnvironmentData data { { 1, 1, {} }, { 1, 1, {} }, { 1, 1, {} } };
		for ( uint32_t face = 0; face < 6; ++face )
		{
			Append( data.diffuse.pixels, {} );
			Append( data.specular.pixels, {} );
		}
		Append( data.brdf.pixels, { 1, 0, 0 } );
		return data;
	}
} // namespace xrlib
