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
const float NIGHT_GRID_STAR_CELLS = 160.0;
const float NIGHT_GRID_STAR_DENSITY = 0.12;

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

uvec3 nightGridHash( uvec3 v )
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
	v ^= v >> 16u;
	v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
	return v;
}

// Sparse stars on cube face cells, each with its own slow twinkle
vec3 nightGridStars( vec3 direction, float seconds )
{
	if ( direction.y < 0.02 )
		return vec3( 0.0 );

	vec3 a = abs( direction );
	vec2 uv;
	uint face;
	if ( a.x >= a.y && a.x >= a.z ) { uv = direction.yz / a.x; face = direction.x > 0.0 ? 0u : 1u; }
	else if ( a.y >= a.z ) { uv = direction.xz / a.y; face = direction.y > 0.0 ? 2u : 3u; }
	else { uv = direction.xy / a.z; face = direction.z > 0.0 ? 4u : 5u; }

	vec2 grid = ( uv * 0.5 + 0.5 ) * NIGHT_GRID_STAR_CELLS;
	vec2 cell = floor( grid );
	vec3 random = vec3( nightGridHash( uvec3( uvec2( cell ), face ) ) ) / 4294967295.0;
	if ( random.x > NIGHT_GRID_STAR_DENSITY )
		return vec3( 0.0 );

	// Keep each star at least a pixel wide, dimming it to match so it doesn't flicker as the head moves
	vec2 offset = grid - ( cell + 0.25 + 0.5 * random.yz );
	float size = 0.05 + 0.05 * random.y;
	float filtered = max( size, length( fwidth( grid ) ) );
	float disc = exp( -dot( offset, offset ) / ( filtered * filtered ) ) * ( size * size ) / ( filtered * filtered );

	float twinkle = 0.7 + 0.3 * sin( seconds * ( 0.25 + 0.35 * random.z ) + random.y * 6.2831853 );
	float brightness = mix( 0.6, 5.0, random.z * random.z ) * twinkle * smoothstep( 0.02, 0.2, direction.y );
	return mix( vec3( 0.8, 0.85, 1.0 ), vec3( 1.0, 0.9, 0.75 ), random.y ) * ( disc * brightness );
}

#endif
