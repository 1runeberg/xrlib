// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0
#ifndef XRVK_SKINNING
#define XRVK_SKINNING

layout(set=2, binding=0, std430) readonly buffer Skinning {
    mat4 modelFromAsset;
    mat4 transforms[];
} skinning;

struct MorphDelta {
    vec4 position;
    vec4 normal;
    vec4 tangent;
};
layout(set=2, binding=1, std430) readonly buffer MorphVertices {
    uvec4 header;
    uvec4 vertices[];
} morphVertices;
layout(set=2, binding=2, std430) readonly buffer MorphDeltas {
    MorphDelta deltas[];
} morphDeltas;
layout(set=2, binding=3, std430) readonly buffer MorphWeights {
    float weights[];
} morphWeights;

void morphVertex(uint vertex, inout vec3 position, inout vec3 normal, inout vec3 tangent) {
    if (vertex >= morphVertices.header.x) return;
    uvec4 entry = morphVertices.vertices[vertex];
    for (uint target = 0; target < entry.y; ++target) {
        float weight = morphWeights.weights[entry.z + target];
        if (weight == 0.0) continue;
        MorphDelta delta = morphDeltas.deltas[entry.x + target * entry.w];
        position += delta.position.xyz * weight;
        normal += delta.normal.xyz * weight;
        tangent += delta.tangent.xyz * weight;
    }
}

// Joint indices and normalised weights are validated before vertex upload
mat4 skinningTransform(ivec4 joints, vec4 weights) {
    mat4 pose = skinning.transforms[joints.x] * weights.x
              + skinning.transforms[joints.y] * weights.y
              + skinning.transforms[joints.z] * weights.z
              + skinning.transforms[joints.w] * weights.w;
    return skinning.modelFromAsset * pose;
}
#endif
