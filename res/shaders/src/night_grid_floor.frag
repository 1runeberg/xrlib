// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Night grid floor in metres on the mesh XZ plane, fading to the black ground so it can draw opaque and write depth
// Emissive x is the floor half extent in metres, matching the mesh and the baked lighting
// Alpha is the grid's coverage, so only the lines, glow and compass show over passthrough
#version 450
#include "pbr_types.glsl"
#include "tonemaps.glsl"
#include "night_grid.glsl"
layout( location = 0 ) in vec3 localPosition;
layout( location = 1 ) in vec3 worldPosition;
layout( location = 0 ) out vec4 outColor;
void main()
{
	vec3 eyes = ( scene.eyePositions[ 0 ].xyz + scene.eyePositions[ 1 ].xyz ) * 0.5;
	float userDistance = length( worldPosition.xz - eyes.xz );
	vec4 floorColor = nightGridFloor( localPosition.xz, material.emissiveFactor.x, userDistance );
	float coverage = nightGridFloorCoverage( localPosition.xz, material.emissiveFactor.x, userDistance );
	outColor = vec4( nightGridDisplay( floorColor.rgb * scene.environmentIntensity ) * floorColor.a, coverage );
}
