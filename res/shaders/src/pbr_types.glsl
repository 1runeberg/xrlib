// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#ifndef XRVK_PBR_TYPES
#define XRVK_PBR_TYPES
#define MAX_POINT_LIGHTS 3
#define MAX_SPOTLIGHTS 2
#define TEXTURE_BASE_COLOR_BIT 1u
#define TEXTURE_METALLIC_ROUGH_BIT 2u
#define TEXTURE_NORMAL_BIT 4u
#define TEXTURE_EMISSIVE_BIT 8u
#define TEXTURE_OCCLUSION_BIT 16u
#define TEXTURE_BASE_COLOR_SRGB_BIT (1u << 16)
#define TEXTURE_EMISSIVE_SRGB_BIT (1u << 17)

layout(set=0, binding=0, std140) uniform MaterialUBO {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float normalScale;
    uint textureFlags;
    float occlusionStrength;
    vec2 padding;
} material;

struct DirectionalLight { vec3 direction; float intensity; vec3 color; };
struct PointLight { vec3 position; float range; vec3 color; float intensity; };
struct SpotLight {
    vec3 position; float range;
    vec3 direction; float intensity;
    vec3 color; float innerCone;
    float outerCone;

    // Keep the array stride explicit when translating std140 to Metal
    float padding0;
    float padding1;
    float padding2;
};
struct Tonemapping { float exposure; float gamma; uint tonemap; float contrast; float saturation; };
layout(set=1, binding=0, std140) uniform SceneLighting {
    DirectionalLight mainLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOTLIGHTS];
    vec3 ambientColor;
    float ambientIntensity;
    uint activePointLights;
    uint activeSpotLights;
    Tonemapping tonemapping;
    vec4 eyePositions[2];
    uint outputSRGB;
} scene;
#endif
