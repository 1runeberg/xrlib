// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// NOTE: This shader is meant for multiview, however since as of writing,
// the openxr spec for khr vismask doesnt have a convenient way to hand gl_ViewIndex,
// the eyeprojection matrix index here is hardcoded for the intended layer.
// The z value puts the intended layer within view, and out of view otherwise.

#version 450
#extension GL_EXT_multiview : enable

#pragma shader_stage(vertex)
#pragma vertex

// Define push constants
layout (push_constant) uniform PushConstants 
{
    mat4 eyeProjections[2]; // Use an array for both eye matrices
} pcr;

layout(location = 0) in vec2 inPosition; // 2D vertex positions for stencil mask

void main() {
    // Create a 4D position vector with z = -1 and w = 1 for homogeneous coordinates
    float z = (gl_ViewIndex == 1) ? -1.0 : 1.0;
    vec4 position = vec4(inPosition, z, 1.0);

    // Transform the position using the projection matrix
    gl_Position = pcr.eyeProjections[1] * position;
}
