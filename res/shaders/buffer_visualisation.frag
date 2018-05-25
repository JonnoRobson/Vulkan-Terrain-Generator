#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler bufferSampler;
layout(binding = 1) uniform texture2D bufferTexture;

// outputs
layout(location = 0) out vec4 outColor;


void main()
{
	outColor = texture(sampler2D(bufferTexture, bufferSampler), fragTexCoord);
}