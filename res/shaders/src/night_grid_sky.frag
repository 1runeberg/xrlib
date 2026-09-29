// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Night grid backdrop on a head-centred cube, base colour is the baked sky and emissive x is time in seconds
#version 450
#include "pbr_types.glsl"
#include "tonemaps.glsl"
#include "night_grid.glsl"
layout( set = 0, binding = 1 ) uniform sampler2D baseColorMap;
layout( location = 0 ) in vec3 localPosition;
layout( location = 0 ) out vec4 outColor;
const float PI = 3.14159265358979;
void main()
{
	vec3 d = normalize( localPosition );

	// The baked ground is black past the thin glow under the horizon, so skip the lookups there
	if ( d.y < -0.06 )
	{
		outColor = vec4( nightGridDisplay( vec3( 0.0 ) ), 1.0 );
		return;
	}

	float c = cos( scene.environmentRotation ), s = sin( scene.environmentRotation );
	d = vec3( c * d.x - s * d.z, d.y, s * d.x + c * d.z );

	// Same equirectangular mapping as the IBL bake. Explicit LOD avoids a mip seam where atan wraps
	float halfTexel = 0.5 / float( textureSize( baseColorMap, 0 ).y );
	vec2 uv = vec2( atan( d.z, d.x ) / ( 2.0 * PI ) + 0.5, clamp( acos( clamp( d.y, -1.0, 1.0 ) ) / PI, halfTexel, 1.0 - halfTexel ) );
	vec3 radiance = textureLod( baseColorMap, uv, 0.0 ).rgb + nightGridStars( d, material.emissiveFactor.x );
	outColor = vec4( nightGridDisplay( radiance * scene.environmentIntensity ), 1.0 );
}
