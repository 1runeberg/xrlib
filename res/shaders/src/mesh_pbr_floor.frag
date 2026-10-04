// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450

#define GRID_MAJOR_WIDTH 0.03                       // Width of major grid lines
#define GRID_MINOR_WIDTH 0.04                       // Width of minor grid lines
#define MINOR_GRID_SPACING 10.0                     // Spacing multiplier for minor grid lines
#define FADE_RADIUS 1.0                             // Radius for circular fade
#define FADE_SOFTNESS 2.0                           // Softness of fade edge
#define COMPASS_SIZE 0.1                            // Size of the compass indicator
#define COMPASS_ROTATION (3.14159 / 4.0)            // 45-degree clockwise rotation in radians
#define USER_INDICATOR_RADIUS 0.08                  // Default radius for the user indicator
#define USER_INDICATOR_SOFTNESS 0.8                 // Softness of the user indicator edge
#define USER_INDICATOR_COLOR vec3(1.0, 1.0, 0.5)    // Light yellow color for the user indicator


#define MAX_POINT_LIGHTS 3
#define MAX_SPOTLIGHTS 2

// Flag definitions for packed bits in tonemap uint
#define TONEMAP_MASK 0xFu         // First 4 bits reserved for tonemap enum (16 values)
#define TONEMAP_SHIFT 0u          // Shift amount for tonemap bits

#define RENDER_MODE_MASK 0xF0u    // Next 4 bits reserved for render mode enum (16 values)
#define RENDER_MODE_SHIFT 4u      // Shift amount for render mode bits

// Render modes (values after shift)
#define RENDER_MODE_UNLIT 0u
#define RENDER_MODE_BLINN_PHONG 1u
#define RENDER_MODE_PBR 2u

// Material texture flags
#define TEXTURE_BASE_COLOR_BIT     0x01u
#define TEXTURE_METALLIC_ROUGH_BIT 0x02u
#define TEXTURE_NORMAL_BIT         0x04u
#define TEXTURE_EMISSIVE_BIT       0x08u
#define TEXTURE_OCCLUSION_BIT      0x10u

// Remaining bits 8-31 available for additional flags

#include "tonemaps.glsl"    // Supported tonemapping functions (see xrlib::ETonemapOperator)

// Material UBO
layout(set = 0, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    vec4 emissiveFactor;    // input for x,y of user marker
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float normalScale;
    uint textureFlags;
    int alphaMode;
    vec2 padding;
} material;

// Scene Lighting UBO
    
// Main directional light
struct DirectionalLight {
    vec3 direction;
    float intensity;
    vec3 color;
};

// Point lights
struct PointLight {
    vec3 position;
    float range;
    vec3 color;
    float intensity;
};

// Spot lights
struct SpotLight {
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    float innerCone;
    float outerCone;
};

// Tonemapping parameters with packed flags
struct Tonemapping {
    float exposure;
    float gamma;
    uint tonemap;      // Bits 0-3: tonemap operator, Bits 4-5: render mode, Bits 6-31: available
    float contrast;
    float saturation;
};

layout(set = 1, binding = 0) uniform SceneLighting {
    // Scene lighting
    DirectionalLight mainLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOTLIGHTS];

    // Scene ambient and active lights
    vec3 ambientColor;
    float ambientIntensity;
    uint activePointLights;
    uint activeSpotLights;

    Tonemapping tonemapping;
} scene;

// Texture samplers
layout(binding = 1) uniform sampler2D baseColorMap;
layout(binding = 2) uniform sampler2D metallicRoughnessMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D emissiveMap;
layout(binding = 5) uniform sampler2D occlusionMap;

// Input from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

// Output
layout(location = 0) out vec4 outColor;

// Function to create grid lines
float gridLine(float position, float width) {
    float halfWidth = width * 0.5;
    return 1.0 - smoothstep(-halfWidth, halfWidth, abs(fract(position - 0.5) - 0.5));
}

// Function to create compass indicator at the origin
vec3 compassIndicator(vec2 position, vec2 origin, float size) {
    float distance = length(position - origin);
    
    if (distance < size) {
        // Calculate angle from the origin (0, 0)
        float angle = atan(position.y, position.x); // angle to the origin
        
        // Normalize angle to be between 0 and 2PI
        angle = mod(angle + 2.0 * 3.14159, 2.0 * 3.14159);
        
        // Apply rotation
        angle += COMPASS_ROTATION;
        
        // Normalize angle again after rotation
        angle = mod(angle, 2.0 * 3.14159);
        
        // Define the new angle ranges for the quadrants after the rotation
        float quadrantSize = 3.14159 / 2.0; // 90 degrees per quadrant
        float q0 = 0.0;   
        float q1 = quadrantSize; 
        float q2 = 2.0 * quadrantSize; 
        float q3 = 3.0 * quadrantSize;  
        
        // Determine the color based on the quadrant
        if (angle >= q0 && angle < q1) {
            return vec3(0.0, 1.0, 0.0);  // Green
        } else if (angle >= q1 && angle < q2) {
            return vec3(0.25, 0.0, 0.25);  // Purple
        } else if (angle >= q2 && angle < q3) {
            return vec3(1.0, 0.0, 0.0);  // Red
        } else {
            return vec3(0.0, 0.0, 1.0);  // Blue
        }
    }
    return vec3(0.0, 0.0, 0.0);  // Return black if not in compass area
}

void main() 
{
    // Base color from material
    vec4 baseColor = material.baseColorFactor;
    vec3 finalColor = vec3(0.0, 0.0, 0.0); // Background color
    
    // Major grid lines
    float majorX = gridLine(inWorldPos.x, GRID_MAJOR_WIDTH);
    float majorZ = gridLine(inWorldPos.z, GRID_MAJOR_WIDTH);
    float majorGrid = max(majorX, majorZ);
    
    // Minor grid lines
    float minorX = gridLine(inWorldPos.x * MINOR_GRID_SPACING, GRID_MINOR_WIDTH);
    float minorZ = gridLine(inWorldPos.z * MINOR_GRID_SPACING, GRID_MINOR_WIDTH);
    float minorGrid = max(minorX, minorZ);
    
    // Ensure major grid lines override minor grid lines
    float combinedMinorGrid = minorGrid * (1.0 - majorGrid);
    
    // Add minor grid lines in base color
    finalColor = mix(finalColor, baseColor.rgb * 0.5, combinedMinorGrid);
    
    // Overlay major grid lines in yellow
    finalColor = mix(finalColor, vec3(1.0, 1.0, 0.0), majorGrid);
    
    // User indicator (radial glow around userOrigin)
    vec2 userOrigin = material.emissiveFactor.xy; // User's position in world coordinates
    float userDistance = length(inWorldPos.xz - userOrigin);
    float userGlow = 1.0 - smoothstep(USER_INDICATOR_RADIUS, USER_INDICATOR_RADIUS + USER_INDICATOR_SOFTNESS, userDistance);
    vec3 userGlowColor = USER_INDICATOR_COLOR * userGlow;
    
    // Blend user indicator into the grid
    finalColor = mix(finalColor, userGlowColor, userGlow);
    
    // Fade edges of the mesh in a circular gradient from the origin
    float fade = smoothstep(FADE_RADIUS, FADE_RADIUS + FADE_SOFTNESS, length(inWorldPos.xz));
    
    // Apply fade to alpha channel
    outColor = vec4(finalColor, baseColor.a * (1.0 - fade));
    
    // Only add compass at the origin (near the origin)
    vec2 origin = vec2(0.0, 0.0);  // Center at the origin
    
    // Only apply compass if within the specified radius
    if (length(inWorldPos.xz) < COMPASS_SIZE) {
        vec3 compassColor = compassIndicator(inWorldPos.xz, origin, COMPASS_SIZE);
        finalColor = mix(finalColor, compassColor, 1.0); // Blend compass with grid
    }
    
    // Apply the final color to output
    outColor.rgb = finalColor;
}
