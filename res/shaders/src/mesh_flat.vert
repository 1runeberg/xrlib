// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450
#extension GL_EXT_multiview : require

layout(push_constant) uniform PushConsts {
    mat4 viewProjection[2];  // One for each view in multiview
} pushConsts;


// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inColor;

layout(location = 4) in vec4 inModelMatrix0;
layout(location = 5) in vec4 inModelMatrix1;
layout(location = 6) in vec4 inModelMatrix2;
layout(location = 7) in vec4 inModelMatrix3;

// Outputs to fragment shader
layout(location = 0) out vec3 outColor;

void main() 
{
    // Reconstruct model matrix
    mat4 modelMatrix = mat4(
        inModelMatrix0,
        inModelMatrix1,
        inModelMatrix2,
        inModelMatrix3
    );

    vec4 worldPos = modelMatrix *  vec4(inPosition, 1.0);
    
    // Pass color to fragment shader as-is
    outColor = inColor;
  
    // Final position
    gl_Position = pushConsts.viewProjection[gl_ViewIndex] * worldPos;
}
