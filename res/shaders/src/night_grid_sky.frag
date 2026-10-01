// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Night grid backdrop on a head-centred cube, base colour is the baked sky cubemap
#version 450
#include "pbr_types.glsl"
#include "tonemaps.glsl"
#include "night_grid.glsl"
layout( set = 0, binding = 1 ) uniform samplerCube baseColorMap;
layout( location = 0 ) in vec3 localPosition;
layout( location = 0 ) out vec4 outColor;
void main()
{
	vec3 d = normalize( localPosition );

	// The baked ground is black past the thin glow under the horizon, so skip the lookup there
	if ( d.y < -0.06 )
	{
		outColor = vec4( nightGridDisplay( vec3( 0.0 ) ), 1.0 );
		return;
	}

	float c = cos( scene.environmentRotation ), s = sin( scene.environmentRotation );
	d = vec3( c * d.x - s * d.z, d.y, s * d.x + c * d.z );
	outColor = vec4( nightGridDisplay( texture( baseColorMap, d ).rgb * scene.environmentIntensity ), 1.0 );
}
