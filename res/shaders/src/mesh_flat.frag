// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#version 450

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

void main() 
{
    // Ambient lighting multiplier
    float ambient = 0.2; 
    
    vec3 finalColor = inColor * ambient;
    fragColor = vec4(finalColor, 1.0);
}
