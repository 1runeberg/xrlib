// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450

#define SCALE_CONSTANT 5.0

#define MAX_POINT_LIGHTS 3
#define MAX_SPOTLIGHTS 2

// Material texture flags
#define TEXTURE_BASE_COLOR_BIT     0x01u
#define TEXTURE_METALLIC_ROUGH_BIT 0x02u
#define TEXTURE_NORMAL_BIT         0x04u
#define TEXTURE_EMISSIVE_BIT       0x08u
#define TEXTURE_OCCLUSION_BIT      0x10u

// Material UBO
layout(set = 0, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    vec4 emissiveFactor;        // x: time, y: alpha
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float normalScale;
    uint textureFlags;
    int alphaMode;
    vec2 padding;
} material;

// Scene Lighting UBO
struct DirectionalLight {
    vec3 direction;
    float intensity;
    vec3 color;
};

struct PointLight {
    vec3 position;
    float range;
    vec3 color;
    float intensity;
};

struct SpotLight {
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    float innerCone;
    float outerCone;
};

struct Tonemapping {
    float exposure;
    float gamma;
    uint tonemap;
    float contrast;
    float saturation;
};

layout(set = 1, binding = 0) uniform SceneLighting {
    DirectionalLight mainLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOTLIGHTS];
    vec3 ambientColor;
    float ambientIntensity;
    uint activePointLights;
    uint activeSpotLights;
    Tonemapping tonemapping;
} scene;

layout(binding = 1) uniform sampler2D baseColorMap;
layout(binding = 2) uniform sampler2D metallicRoughnessMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D emissiveMap;
layout(binding = 5) uniform sampler2D occlusionMap;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec4 outColor;

const float ITER_COUNT = 18;

// Helper function for generating random noise based on UV coordinates
vec3 nrand3(vec2 co) {
    vec3 a = fract(cos(co.x * 8.3e-3 + co.y) * vec3(1.3e5, 4.7e5, 2.9e5));
    vec3 b = fract(sin(co.x * 0.3e-3 + co.y) * vec3(8.1e5, 1.0e5, 0.1e5));
    vec3 c = mix(a, b, 0.5);
    return c;
}

// Function for creating fractal noise pattern
float field(vec3 p, float s, int iter) {
    float accum = s / 4.0;
    float prev = 0.0;
    float tw = 0.0;
    for (int i = 0; i < ITER_COUNT; ++i) {
        if (i >= iter) {
            break;
        }
        float mag = dot(p, p);
        p = abs(p) / mag + vec3(-0.5, -0.4, -1.487);
        float w = exp(-float(i) / 5.0);
        accum += w * exp(-9.025 * pow(abs(mag - prev), 2.2));
        tw += w;
        prev = mag;
    }
    return max(0.0, 5.2 * accum / tw - 0.65);
}

// Function for generating star layer with time-based animation
vec4 starLayer(vec2 p, float time) {
    vec2 seed = 1.9 * p.xy;
    seed = floor(seed * 600.0 / 0.08);
    vec3 rnd = nrand3(seed);
    vec4 col = vec4(pow(rnd.y, 17.0));
    float mul = 10.0 * rnd.x;
    col.xyz *= sin(time * mul + mul) * 0.25 + 1.0;
    return col;
}

// Main shader entry point
void main() {
    // Calculate time from the emissive factor for animation
    float time = material.emissiveFactor.x * 0.1;  // time input on emissiveFactor.x
    
    // Convert inUV to a 2D plane space [-1, 1]
    vec2 uv = 2.0 * inUV - 1.0;

    // Apply the constant scale to the UV coordinates
    vec2 uvScaled = uv * SCALE_CONSTANT;

    // Apply the fractal pattern in UV space with scaling
    vec3 p = vec3(uvScaled, 0.0) + vec3(0.8, -1.3, 0.0);
    p += 0.45 * vec3(sin(time / 32.0), sin(time / 24.0), sin(time / 64.0));

    // Apply noise-based fractal field
    float t = field(p, 0.15, 13);
    float v = (1.0 - exp((abs(uvScaled.x) - 1.0) * 6.0)) * (1.0 - exp((abs(uvScaled.y) - 1.0) * 6.0));

    // Generate nebulae and star layers with scaled UV
    vec3 p2 = vec3(uvScaled / (4.0 + sin(time * 0.11) * 0.2 + 0.2 + sin(time * 0.15) * 0.3 + 0.4), 4.0);
    p2 += 0.16 * vec3(sin(time / 32.0), sin(time / 24.0), sin(time / 64.0));

    float t2 = field(p2, 0.9, 18);
    vec4 c2 = mix(0.5, 0.2, v) * vec4(5.5 * t2 * t2 * t2, 2.1 * t2 * t2, 2.2 * t2 * 0.45, t2);

    // Add stars with multiple layers
    vec4 starColour = vec4(0.0);
    //starColour += starLayer(uvScaled, time);  // first layer of stars
    starColour += starLayer(p2.xy, time);  // second layer of stars

    // Combine nebulae and star colours
    const float brightness = 1.0;
    vec4 colour = mix(0.9, 1.0, v) * vec4(1.5 * 0.15 * t * t * t, 1.2 * 0.4 * t * t, 0.9 * t, 1.0) + c2 + starColour;

    // Output the final color, applying the emissive factor as a time effect
    outColor = vec4(brightness * colour.xyz, 1.0 * material.emissiveFactor.y);
}
