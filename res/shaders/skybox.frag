#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in float height;


// textures
layout(binding = 1) uniform sampler mapSampler;
layout(binding = 2) uniform texture2D diffuseMap;

// outputs
layout(location = 0) out vec4 outColor;


layout(binding = 3) uniform ColorBuffer
{
	vec4 rockColor;
	vec4 snowColor;
	vec4 fogColor;
	vec4 waterColor;
} color_data;

void main()
{
	vec3 fogColor = color_data.fogColor.xyz;
	vec4 color = texture(sampler2D(diffuseMap, mapSampler), fragTexCoord);
	outColor = mix(vec4(fogColor, 1.0f), color, clamp(height, 0, 1));
}