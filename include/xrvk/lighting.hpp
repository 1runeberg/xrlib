/* 
 * Copyright 2024,2025 Copyright Rune Berg 
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
#include <xrlib/common.hpp>
namespace xrlib
{
	// Optimized for:
	//  - Single directional light (sun/main light)
	//  - Limited point lights for important local illumination
	//  - Limited spot lights for specific effects

#define MAX_POINT_LIGHTS 3
#define MAX_SPOT_LIGHTS 2

	// Tonemap uint will be used to pack lighting flags and render mode
	constexpr uint32_t TONEMAP_MASK = 0xFu; // First 4 bits reserved for tonemap enum (16 values)
	constexpr uint32_t TONEMAP_SHIFT = 0u;	// Shift amount for tonemap bits

	constexpr uint32_t RENDER_MODE_MASK = 0xF0u; // Next 4 bits reserved for render mode enum (16 values)
	constexpr uint32_t RENDER_MODE_SHIFT = 4u;	 // Shift amount for render mode bits

	// Render modes (values before shift)
	enum class ERenderMode : uint8_t
	{
		Unlit = 0,
		BlinnPhong = 1,
		PBR = 2
	};

	struct alignas( 16 ) SDirectionalLight
	{
		alignas( 16 ) XrVector3f direction = { 0.0f, -1.0f, 0.0f }; // Direction of the light
		float intensity = 3.0f;										// Intensity of the light (do not move for optimal packing)
		alignas( 16 ) XrVector3f color = { 1.0f, 0.98f, 0.95f };	// Color of the light
		float padding = 0.0f;										// Explicit padding
	};

	struct alignas( 16 ) SPointLight
	{
		alignas( 16 ) XrVector3f position = { 0.0f, 0.0f, 0.0f }; // Position of the light
		float range = 15.0f;									  // Maximum distance the light can reach (do not move for optimal packing)
		alignas( 16 ) XrVector3f color = { 1.0f, 0.98f, 0.95f };  // Color of the light
		float intensity = 1.5f;									  // Intensity of the light
	};

	struct alignas( 16 ) SSpotLight
	{
		alignas( 16 ) XrVector3f position = { 0.0f, 0.0f, 0.0f };	// Position of the light
		float range = 15.0f;										// Maximum distance the light can reach (do not move for optimal packing)
		alignas( 16 ) XrVector3f direction = { 0.0f, -1.0f, 0.0f }; // Direction of the spotlight
		float intensity = 1.5f;										// Intensity of the light (do not move for optimal packing)
		alignas( 16 ) XrVector3f color = { 1.0f, 0.98f, 0.95f };	// Color of the spotlight
		float innerCone = 0.9f;										// Inner cone angle (cosine of the angle, must be > outerCone)
		float outerCone = 0.7f;										// Outer cone angle (cosine of the angle)
		float padding[ 2 ] = { 0.0f, 0.0f };						// Explicit padding
	};

	// Tonemapping operators
	enum class ETonemapOperator : uint8_t
	{
		None = 0,		// No tonemapping, direct output
		Reinhard = 1,	// Reinhard et al., Siggraph 2002 (Public domain)
		ACES = 2,		// Approximate ACES filmic response curve (OpenEXR)
		KHRNeutral = 3, // Khronos Group (Apache 2.0)
		Uncharted2 = 4	// John Hable (Public domain)
	};

	struct alignas( 16 ) STonemapping
	{
		float exposure = 1.0f;	 // Exposure adjustment
		float gamma = 2.2f;		 // Gamma correction value
		uint32_t tonemap = 0;	 // Packed bits: [0-3]: ETonemapOperator, [4-7]: ERenderMode, [8-31]: available
		float contrast = 1.0f;	 // Global contrast adjustment
		float saturation = 1.0f; // Global saturation control, particularly useful for ACES

		// Helper methods for setting and getting the render mode
		void setRenderMode( ERenderMode mode )
		{
			// Clear existing render mode while preserving other flags
			tonemap &= ~RENDER_MODE_MASK;
			// Set new render mode while preserving other flags
			tonemap |= ( static_cast< uint32_t >( mode ) << RENDER_MODE_SHIFT ) & RENDER_MODE_MASK;
		}

		ERenderMode getRenderMode() const { return static_cast< ERenderMode >( ( tonemap & RENDER_MODE_MASK ) >> RENDER_MODE_SHIFT ); }

		// Helper methods for getting and setting the tonemapping operator
		void setTonemapOperator( ETonemapOperator op )
		{
			// Clear existing operator while preserving other flags
			tonemap &= ~TONEMAP_MASK;
			// Set new operator while preserving other flags
			tonemap |= ( static_cast< uint32_t >( op ) & TONEMAP_MASK );
		}

		ETonemapOperator getTonemapOperator() const { return static_cast< ETonemapOperator >( tonemap & TONEMAP_MASK ); }
	};

	struct alignas( 16 ) SSceneLighting
	{
		alignas( 16 ) SDirectionalLight mainLight; // Single main directional light (typically sun/moon)
		alignas( 16 ) SPointLight pointLights[ MAX_POINT_LIGHTS ];
		alignas( 16 ) SSpotLight spotLights[ MAX_SPOT_LIGHTS ];
		alignas( 16 ) XrVector3f ambientColor = { 0.1f, 0.1f, 0.12f }; // Ambient light color
		float ambientIntensity = 1.0f;								   // Ambient light intensity
		uint32_t activePointLights = 0;								   // Number of active point lights
		uint32_t activeSpotLights = 0;								   // Number of active spot lights
		alignas( 16 ) STonemapping tonemapping;
		alignas( 16 ) XrVector4f eyePositions[ 2 ] {};				   // App-space eye positions, updated by the renderer
		uint32_t outputSRGB = 0;									   // The render target performs the linear-to-sRGB conversion
		uint32_t padding[ 3 ] {};

		// Helper functions to manage active lights
		bool addPointLight( const SPointLight &light )
		{
			if ( activePointLights >= MAX_POINT_LIGHTS )
				return false;
			pointLights[ activePointLights++ ] = light;
			return true;
		}

		bool addSpotLight( const SSpotLight &light )
		{
			if ( activeSpotLights >= MAX_SPOT_LIGHTS )
				return false;
			spotLights[ activeSpotLights++ ] = light;
			return true;
		}

		void clearLights()
		{
			activePointLights = 0;
			activeSpotLights = 0;
		}
	};

	// Verify structure sizes are multiples of 16 bytes
	static_assert( sizeof( SDirectionalLight ) % 16 == 0, "SDirectionalLight size must be multiple of 16 bytes" );
	static_assert( sizeof( SPointLight ) % 16 == 0, "SPointLight size must be multiple of 16 bytes" );
	static_assert( sizeof( SSpotLight ) % 16 == 0, "SSpotLight size must be multiple of 16 bytes" );
	static_assert( sizeof( STonemapping ) % 16 == 0, "STonemapping size must be multiple of 16 bytes" );
	static_assert( sizeof( SSceneLighting ) % 16 == 0, "SSceneLighting size must be multiple of 16 bytes" );

	// Verify structure alignments
	static_assert( alignof( SDirectionalLight ) == 16, "SDirectionalLight must be aligned to 16 bytes" );
	static_assert( alignof( SPointLight ) == 16, "SPointLight must be aligned to 16 bytes" );
	static_assert( alignof( SSpotLight ) == 16, "SSpotLight must be aligned to 16 bytes" );
	static_assert( alignof( STonemapping ) == 16, "STonemapping must be aligned to 16 bytes" );
	static_assert( alignof( SSceneLighting ) == 16, "SSceneLighting must be aligned to 16 bytes" );

	// Verify array sizes
	static_assert( MAX_POINT_LIGHTS > 0, "Must have at least one point light slot" );
	static_assert( MAX_SPOT_LIGHTS > 0, "Must have at least one spot light slot" );

} // namespace xrlib
static_assert( offsetof( xrlib::SSceneLighting, activePointLights ) == 272 );
static_assert( offsetof( xrlib::SSceneLighting, activeSpotLights ) == 276 );
static_assert( offsetof( xrlib::SSceneLighting, tonemapping ) == 288 );
static_assert( offsetof( xrlib::SSceneLighting, eyePositions ) == 320 );
static_assert( offsetof( xrlib::SSceneLighting, outputSRGB ) == 352 );
