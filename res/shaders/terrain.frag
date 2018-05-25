#version 450
#extension GL_ARB_separate_shader_objects : enable

// early z test
layout(early_fragment_tests) in;

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 miscFactors;
layout(location = 3) in vec4 viewPosition;

// outputs
layout(location = 0) out vec4 outColor;

// uniforms
layout(binding = 5) uniform FogBuffer
{
	float extinction;
	float in_scattering;
	float camera_height;
	float padding;
} fog_data;

layout(binding = 6) uniform ColorBuffer
{
	vec4 rockColor;
	vec4 snowColor;
	vec4 fogColor;
	vec4 waterColor;
} color_data;

// textures
layout(binding = 7) uniform sampler2D mountainTex;
layout(binding = 8) uniform sampler2D sandTex;
layout(binding = 9) uniform sampler2D grassTex;

void main()
{
	// calculate ratios
	vec3 snowDir = vec3(0.0f, 0.0f, 1.0f);
	float snowValue = clamp(dot(normalize(worldNormal), snowDir), 0, 1);
	snowValue = clamp(snowValue + (miscFactors.x - 1.0), 0, 1);			// snow is more prevalent higher up

	float grassValue = clamp(dot(normalize(worldNormal), vec3(0.0, 0.0, 1.0f)), 0, 1);
	grassValue = clamp(grassValue - (miscFactors.x - 0.1), 0, 1);		// grass is less prevalent higher up

	// sample textures
	vec3 mountainColor = texture(mountainTex, fragTexCoord.xy * 32.0f).xyz * color_data.rockColor.xyz;
	vec3 sandColor = texture(sandTex, fragTexCoord.xy * 32.0f).xyz;
	vec3 grassColor = texture(grassTex, fragTexCoord.xy * 32.0f).xyz;

	// mix colours
	vec3 diffuseColor = mix(mountainColor, grassColor, grassValue);			// calculate grass to rock ratio
	diffuseColor = mix(sandColor, diffuseColor, miscFactors.y);			// calculate sand to diffuse ratio
	diffuseColor = mix(diffuseColor, color_data.snowColor.xyz, snowValue);	// calculate diffuse to snow ratio

	// calculate simple lighting
	vec3 lightDir = vec3(0.0f, -0.5f, -0.5f);
	vec3 lightColor = vec3(0.88f, 0.94f, 1.0f) * 2.0f;
	float lightStrength = clamp(dot(normalize(worldNormal), lightDir), 0, 1);
	vec3 diffuseLightColor = lightColor * lightStrength * diffuseColor;
	float ambientStrength = 0.2f;
	vec3 ambientLightColor = lightColor * ambientStrength * diffuseColor;

	// calculate exponential height fog
	float dist = length(viewPosition);

	// use smoothstep function to determine extinction and in-scattering
	float be = fog_data.extinction * smoothstep(0.0, 6.0, fog_data.camera_height - viewPosition.z);
	float bi = fog_data.in_scattering * smoothstep(0.0, 100.0, fog_data.camera_height - viewPosition.z);

	float ext = exp(-dist * be);
	float insc = exp(-dist * bi);

	vec3 color = diffuseLightColor + ambientLightColor;
	color = color * ext + color_data.fogColor.xyz * (1 - insc);

	outColor.xyz = color;
	outColor.w = 1.0f;
}