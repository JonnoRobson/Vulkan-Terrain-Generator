#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(fragTexCoord, 0.0f, 1.0f);
}