// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Procedural night grid shared by the floor and sky shaders, include after pbr_types.glsl and tonemaps.glsl
// The nebula, horizon glow and overhead panel are baked by xrvk-tools night-grid from src/xrvk/night_grid.cpp
// Keep the shared floor values in sync with that file
#ifndef XRVK_NIGHT_GRID
#define XRVK_NIGHT_GRID

const float NIGHT_GRID_MAJOR_WIDTH = 0.03;
const float NIGHT_GRID_MINOR_WIDTH = 0.04;
const float NIGHT_GRID_MINOR_SPACING = 10.0;
const float NIGHT_GRID_FADE_START = 1.0 / 3.0; // Fractions of the floor half extent
const float NIGHT_GRID_FADE_SOFTNESS = 2.0 / 3.0;
const vec3 NIGHT_GRID_MAJOR_COLOR = vec3( 1.0, 1.0, 0.0 );
const vec3 NIGHT_GRID_MINOR_COLOR = vec3( 0.5 );
const float NIGHT_GRID_COMPASS_SIZE = 0.1;
const float NIGHT_GRID_USER_RADIUS = 0.08;
const float NIGHT_GRID_USER_SOFTNESS = 0.8;
const vec3 NIGHT_GRID_USER_COLOR = vec3( 1.0, 1.0, 0.5 );

// Matches displayColor in mesh_pbr.frag so procedural surfaces agree with lit models
vec3 nightGridDisplay( vec3 linearColor )
{
	uint op = scene.tonemapping.tonemap & 15u;
	vec3 color = max( linearColor, vec3( 0 ) ) * max( scene.tonemapping.exposure, 0.0 );
	TonemapParams params = TonemapParams( 1.0, scene.tonemapping.gamma );
	if ( op == 1u ) color = tonemapReinhard( color, params );
	else if ( op == 2u ) color = tonemapACES( color, params );
	else if ( op == 3u ) color = tonemapKHRNeutral( color, params );
	else if ( op == 4u ) color = tonemapUncharted2( color, params );
	float luminance = dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
	color = mix( vec3( luminance ), color, max( scene.tonemapping.saturation, 0.0 ) );
	color = max( ( color - 0.18 ) * max( scene.tonemapping.contrast, 0.0 ) + 0.18, vec3( 0 ) );
	return scene.outputSRGB != 0u ? color : gammaCorrect( color, scene.tonemapping.gamma );
}

// The original floor profile, widened to at least a pixel and dimmed to match so thin lines don't shimmer
float nightGridLine( float position, float width )
{
	float distance = abs( position - floor( position + 0.5 ) );
	float halfWidth = width * 0.5;
	float filtered = max( halfWidth, fwidth( position ) );
	return ( 1.0 - smoothstep( -filtered, filtered, distance ) ) * ( halfWidth / filtered );
}

// Quadrants of the demo 6 compass, rotated 45 degrees, from sign tests instead of atan
vec3 nightGridCompass( vec2 position )
{
	if ( position.x > abs( position.y ) ) return vec3( 0.0, 1.0, 0.0 );
	if ( position.y > abs( position.x ) ) return vec3( 0.25, 0.0, 0.25 );
	if ( -position.x > abs( position.y ) ) return vec3( 1.0, 0.0, 0.0 );
	return vec3( 0.0, 0.0, 1.0 );
}

// Floor colour and fade alpha, position is floor-local in metres and userDistance is from the viewer's feet
vec4 nightGridFloor( vec2 position, float floorSize, float userDistance )
{
	float major = max( nightGridLine( position.x, NIGHT_GRID_MAJOR_WIDTH ), nightGridLine( position.y, NIGHT_GRID_MAJOR_WIDTH ) );
	float minor = max( nightGridLine( position.x * NIGHT_GRID_MINOR_SPACING, NIGHT_GRID_MINOR_WIDTH ), nightGridLine( position.y * NIGHT_GRID_MINOR_SPACING, NIGHT_GRID_MINOR_WIDTH ) ) * ( 1.0 - major );
	vec3 color = mix( mix( vec3( 0.0 ), NIGHT_GRID_MINOR_COLOR * 0.5, minor ), NIGHT_GRID_MAJOR_COLOR, major );

	float glow = 1.0 - smoothstep( NIGHT_GRID_USER_RADIUS, NIGHT_GRID_USER_RADIUS + NIGHT_GRID_USER_SOFTNESS, userDistance );
	color = mix( color, NIGHT_GRID_USER_COLOR * glow, glow );

	float radius = length( position );
	if ( radius < NIGHT_GRID_COMPASS_SIZE )
		color = nightGridCompass( position );

	return vec4( color, 1.0 - smoothstep( NIGHT_GRID_FADE_START, NIGHT_GRID_FADE_START + NIGHT_GRID_FADE_SOFTNESS, radius / floorSize ) );
}

// How much of the floor is grid, glow or compass rather than black ground, for alpha over passthrough
// Same inputs as nightGridFloor, keep the two in step
float nightGridFloorCoverage( vec2 position, float floorSize, float userDistance )
{
	float major = max( nightGridLine( position.x, NIGHT_GRID_MAJOR_WIDTH ), nightGridLine( position.y, NIGHT_GRID_MAJOR_WIDTH ) );
	float minor = max( nightGridLine( position.x * NIGHT_GRID_MINOR_SPACING, NIGHT_GRID_MINOR_WIDTH ), nightGridLine( position.y * NIGHT_GRID_MINOR_SPACING, NIGHT_GRID_MINOR_WIDTH ) );
	float glow = 1.0 - smoothstep( NIGHT_GRID_USER_RADIUS, NIGHT_GRID_USER_RADIUS + NIGHT_GRID_USER_SOFTNESS, userDistance );

	float radius = length( position );
	float coverage = radius < NIGHT_GRID_COMPASS_SIZE ? 1.0 : max( max( major, minor ), glow );
	return coverage * ( 1.0 - smoothstep( NIGHT_GRID_FADE_START, NIGHT_GRID_FADE_START + NIGHT_GRID_FADE_SOFTNESS, radius / floorSize ) );
}

#endif
