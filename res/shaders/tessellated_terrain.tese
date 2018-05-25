#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2, r32f) uniform image2D heightmap;

layout(quads, fractional_odd_spacing, ccw) in;
layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec2 inTexCoord[];
layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec2 texCoord;

vec2 coord[] = 
{
	{0.0, 0.5},
	{0.5, -0.5},
	{-0.5, -0.5}
};

void main()
{
	vec3 uv = gl_TessCoord;

	// interpolate values
	vec3 finalVertexCoord = vec3(0, 0, 0);
	finalVertexCoord.xy = inPosition[0].xy * (1.0f - uv.x) * (1.0f - uv.y)
						+ inPosition[1].xy * uv.x * (1.0f - uv.y)
						+ inPosition[2].xy * (1.0f - uv.x) * uv.y
						+ inPosition[3].xy * uv.x * uv.y;
	
	finalVertexCoord.xy = mix(mix(inPosition[0].xy, inPosition[1].xy, uv.x), mix(inPosition[2].xy, inPosition[3].xy, uv.x), uv.y);
		
	texCoord = vec2(0, 0);
	texCoord = inTexCoord[0] * (1.0f - uv.x) * (1.0f - uv.y)
			 + inTexCoord[1] * uv.x * (1.0f - uv.y)
			 + inTexCoord[2] * (1.0f - uv.x) * uv.y
			 + inTexCoord[3] * uv.x * uv.y;
			 
	// read height from the heightmap
	ivec2 heightmapCoord = ivec2(texCoord) * 256;
	float height = imageLoad(heightmap, ivec2(heightmapCoord)).r;
	finalVertexCoord.z = height;

	outPosition = vec4(finalVertexCoord, 1.0f);

	// debug lod shading
	float lod = (gl_TessLevelInner[0] - 5.0f) / (10.0f - 5.0f);
}