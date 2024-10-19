// Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450

#pragma shader_stage(fragment)
#pragma fragment

// Inputs
layout (location = 0) in vec4 inColor;

// Outputs
layout (location = 0) out vec4 FragColor;


void main()
{
    FragColor = inColor;
}
