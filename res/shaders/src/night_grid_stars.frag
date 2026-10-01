// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Additive star and shooting star glow over the night grid sky
#version 450
#include "pbr_types.glsl"
#include "tonemaps.glsl"
#include "night_grid.glsl"
layout( location = 0 ) in vec2 inOffset;
layout( location = 1 ) in float inSize;
layout( location = 2 ) in vec3 inRadiance;
layout( location = 0 ) out vec4 outColor;
void main()
{
	// Keep each star close to a pixel wide, dimming it to match so it doesn't flicker as the head moves
	// Much narrower and stars shimmer
	float pixel = 0.75 * max( length( dFdx( inOffset ) ), length( dFdy( inOffset ) ) );
	float filtered = max( inSize, pixel );
	float disc = exp( -dot( inOffset, inOffset ) / ( filtered * filtered ) ) * ( inSize * inSize ) / ( filtered * filtered );

	// Blending adds after tonemapping, so remove what black maps to
	vec3 radiance = inRadiance * disc * scene.environmentIntensity;
	outColor = vec4( nightGridDisplay( radiance ) - nightGridDisplay( vec3( 0.0 ) ), 0.0 );
}
