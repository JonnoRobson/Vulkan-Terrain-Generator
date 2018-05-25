#version 450
#extension GL_ARB_separate_shader_objects : enable

#define TERRAIN_CHUNK_COUNT 9

// input layout
layout(triangles) in;
layout(location = 0) in vec2 inTexCoord[];
layout(location = 1) in vec3 inMiscFactors[];

// output layout
layout(triangle_strip, max_vertices = 3) out;
layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 miscFactors;
layout(location = 3) out vec4 viewPosition;

// uniforms
layout(binding = 3) uniform MatrixBuffer
{
	mat4 model[TERRAIN_CHUNK_COUNT];
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
	viewPosition = matrices.view * gl_in[0].gl_Position;
	gl_Position = matrices.proj * viewPosition;
	texCoord = inTexCoord[0];
	normal = CalculateNormal(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	miscFactors = inMiscFactors[0];
	EmitVertex();

	// vertex 1
	viewPosition = matrices.view * gl_in[1].gl_Position;
	gl_Position = matrices.proj * viewPosition;
	texCoord = inTexCoord[1];
	miscFactors = inMiscFactors[1];
	EmitVertex();

	// vertex 2
	viewPosition = matrices.view * gl_in[2].gl_Position;
	gl_Position = matrices.proj * viewPosition;
	texCoord = inTexCoord[2];
	miscFactors = inMiscFactors[2];
	EmitVertex();

	EndPrimitive();
}