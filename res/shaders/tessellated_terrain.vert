#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inMatIndex;

layout(binding = 0) uniform MatrixBuffer
{
	mat4 model;
	mat4 view;
	mat4 proj;
} matrices;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
	gl_Position = matrices.model * vec4(inPosition, 1.0);
	worldPosition = gl_Position.xyz;
	fragTexCoord = inTexCoord;
}