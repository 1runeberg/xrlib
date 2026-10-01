// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Head-centred star quads at the far plane. Position is an environment-space direction, uv0 the quad
// corner, uv1 the angular radius and quad half extent, normal.x scales the corner expansion (0 when
// corners are already placed, as for shooting star streaks) and color0 the linear radiance
#version 450
#extension GL_EXT_multiview : require
#include "pbr_types.glsl"
layout( push_constant ) uniform PushConsts { mat4 eyeVPs[ 2 ]; }
pushConsts;
layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec3 inNormal;
layout( location = 3 ) in vec2 inUV0;
layout( location = 4 ) in vec2 inUV1;
layout( location = 5 ) in vec3 inColor;
layout( location = 8 ) in vec4 model0;
layout( location = 9 ) in vec4 model1;
layout( location = 10 ) in vec4 model2;
layout( location = 11 ) in vec4 model3;
layout( location = 0 ) out vec2 outOffset;
layout( location = 1 ) out float outSize;
layout( location = 2 ) out vec3 outRadiance;
void main()
{
	// Undo the environment rotation the sky and lighting apply
	float c = cos( scene.environmentRotation ), s = sin( scene.environmentRotation );
	vec3 d = vec3( c * inPosition.x + s * inPosition.z, inPosition.y, -s * inPosition.x + c * inPosition.z );

	vec3 tangent = normalize( cross( abs( d.y ) < 0.999 ? vec3( 0.0, 1.0, 0.0 ) : vec3( 1.0, 0.0, 0.0 ), d ) );
	vec3 bitangent = cross( d, tangent );
	outOffset = inUV0 * inUV1.y;
	vec3 local = d + ( tangent * outOffset.x + bitangent * outOffset.y ) * inNormal.x;

	vec4 world = mat4( model0, model1, model2, model3 ) * vec4( local, 1.0 );
	gl_Position = pushConsts.eyeVPs[ gl_ViewIndex ] * world;
	gl_Position.z = gl_Position.w;
	outSize = inUV1.x;
	outRadiance = inColor;
}
