// Copyright 2026 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

// Procedural backdrops and floors, passing mesh-local and app-space positions
#version 450
#extension GL_EXT_multiview : require
layout( push_constant ) uniform PushConsts { mat4 eyeVPs[ 2 ]; }
pushConsts;
layout( location = 0 ) in vec3 inPosition;
layout( location = 8 ) in vec4 model0;
layout( location = 9 ) in vec4 model1;
layout( location = 10 ) in vec4 model2;
layout( location = 11 ) in vec4 model3;
layout( location = 0 ) out vec3 localPosition;
layout( location = 1 ) out vec3 worldPosition;
void main()
{
	vec4 world = mat4( model0, model1, model2, model3 ) * vec4( inPosition, 1.0 );
	gl_Position = pushConsts.eyeVPs[ gl_ViewIndex ] * world;
	localPosition = inPosition;
	worldPosition = world.xyz;
}
