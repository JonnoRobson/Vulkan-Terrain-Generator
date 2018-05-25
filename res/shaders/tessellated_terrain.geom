#version 450
#extension GL_ARB_separate_shader_objects : enable

// input layout
layout(triangles) in;
layout(location = 0) in vec4 inPosition[];
layout(location = 1) in vec2 inTexCoord[];

// output layout
layout(triangle_strip, max_vertices = 3) out;
layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec3 normal;

// uniforms
layout(binding = 3) uniform MatrixBuffer
{
	mat4 model;
	mat4 view;
	mat4 proj;
} matrices;

vec3 CalculateNormal(vec3 p1, vec3 p2, vec3 p3)
{
	vec3 c1 = p2 - p1;
	vec3 c2 = p3 - p1;

	return cross(c1, c2);
}

void main()
{
	// vertex 0
	gl_Position = matrices.proj * matrices.view * inPosition[0];
	texCoord = inTexCoord[0];
	normal = CalculateNormal(inPosition[0].xyz, inPosition[1].xyz, inPosition[2].xyz);
	EmitVertex();

	// vertex 1
	gl_Position = matrices.proj * matrices.view * inPosition[1];
	texCoord = inTexCoord[1];
	normal = CalculateNormal(inPosition[0].xyz, inPosition[1].xyz, inPosition[2].xyz);
	EmitVertex();

	// vertex 2
	gl_Position = matrices.proj * matrices.view * inPosition[2];
	texCoord = inTexCoord[2];
	normal = CalculateNormal(inPosition[0].xyz, inPosition[1].xyz, inPosition[2].xyz);
	EmitVertex();
}