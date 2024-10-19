// Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450
#extension GL_EXT_multiview : enable

#pragma shader_stage(vertex)
#pragma vertex

// Define push constants
layout (push_constant) uniform PushConstants 
{
    mat4 leftEyeVP;
    mat4 rightEyeVP;
} pcr;

// Inputs - per vertex
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec4 inColor;

// Inputs - per instance
layout (location = 2) in vec4 inModelMatrixC0;
layout (location = 3) in vec4 inModelMatrixC1;
layout (location = 4) in vec4 inModelMatrixC2;
layout (location = 5) in vec4 inModelMatrixC3;

// Outputs
layout (location = 0) out vec4 outColor;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    outColor  = inColor;

    mat4 modelMatrix = mat4( inModelMatrixC0, inModelMatrixC1, inModelMatrixC2, inModelMatrixC3 );

    if( gl_ViewIndex == 0 )
    {
        gl_Position = pcr.leftEyeVP * modelMatrix * vec4(inPosition, 1.0);
    }
    else
    {
        gl_Position = pcr.rightEyeVP * modelMatrix * vec4(inPosition, 1.0);
    }
}
