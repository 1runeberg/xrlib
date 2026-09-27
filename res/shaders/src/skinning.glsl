// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0
#ifndef XRVK_SKINNING
#define XRVK_SKINNING

layout(set=2, binding=0, std430) readonly buffer Skinning {
    mat4 modelFromAsset;
    mat4 transforms[];
} skinning;

// Joint indices and normalised weights are validated before vertex upload
mat4 skinningTransform(ivec4 joints, vec4 weights) {
    mat4 pose = skinning.transforms[joints.x] * weights.x
              + skinning.transforms[joints.y] * weights.y
              + skinning.transforms[joints.z] * weights.z
              + skinning.transforms[joints.w] * weights.w;
    return skinning.modelFromAsset * pose;
}
#endif
