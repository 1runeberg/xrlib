// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0
#version 450
#extension GL_EXT_multiview : require
#include "pbr_types.glsl"
layout(push_constant) uniform PushConsts { mat4 eyeVPs[2]; } pushConsts;
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec4 inTangent;
layout(location=3) in vec2 inTexCoord0;
layout(location=4) in vec2 inTexCoord1;
layout(location=5) in vec3 inColor0;
layout(location=8) in vec4 model0;
layout(location=9) in vec4 model1;
layout(location=10) in vec4 model2;
layout(location=11) in vec4 model3;
layout(location=0) out vec3 outWorldPos;
layout(location=1) out vec2 outUV;
layout(location=2) out vec3 outNormal;
layout(location=3) out vec3 outTangent;
layout(location=4) out vec3 outBitangent;
layout(location=5) out vec3 outViewDirection;
layout(location=6) out vec3 outColor;
layout(location=7) out vec2 outUV1;
void main() {
    mat4 modelMatrix = mat4(model0, model1, model2, model3);
    mat3 basis = mat3(modelMatrix);

    // xrvk instances are TRS: reciprocal squared column lengths give the
    // inverse-transpose without a matrix inverse for every vertex
    vec3 scaleSquared = vec3(dot(basis[0],basis[0]), dot(basis[1],basis[1]), dot(basis[2],basis[2]));
    vec3 normal = basis * (inNormal / max(scaleSquared, vec3(1e-12)));
    outNormal = normal * inversesqrt(max(dot(normal,normal),1e-20));
    vec3 tangent = basis * inTangent.xyz;
    tangent -= outNormal * dot(outNormal,tangent);
    tangent *= inversesqrt(max(dot(tangent,tangent),1e-20));
    outTangent = tangent;
    float mirrored = dot(cross(basis[0],basis[1]),basis[2]) < 0.0 ? -1.0 : 1.0;
    outBitangent = cross(outNormal,tangent) * inTangent.w * mirrored;
    vec4 world = modelMatrix * vec4(inPosition,1);
    outWorldPos = world.xyz;
    outViewDirection = scene.eyePositions[gl_ViewIndex].xyz - world.xyz;
    outUV = inTexCoord0;
    outUV1 = inTexCoord1;
    outColor = inColor0;
    gl_Position = pushConsts.eyeVPs[gl_ViewIndex] * world;
}
