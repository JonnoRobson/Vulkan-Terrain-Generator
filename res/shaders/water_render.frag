#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(early_fragment_tests) in;

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec4 worldPosition;
layout(location = 3) in vec3 miscFactors;
layout(location = 4) in vec4 viewPosition;

// outputs
layout(location = 0) out vec4 outColor;

// inputs
layout(binding = 3) uniform FogBuffer
{
	float extinction;
	float in_scattering;
	float camera_height;
	float padding;
} fog_data;

layout(binding = 4) uniform ColorBuffer
{
	vec4 rockColor;
	vec4 snowColor;
	vec4 fogColor;
	vec4 waterColor;
} color_data;

void main()
{
	//discard;

	// calculate diffuse color
	vec3 diffuseColor = color_data.waterColor.xyz;

	// calculate simple lighting
	vec3 lightDir = vec3(0.0f, -0.5f, -0.5f);
	vec3 lightColor = vec3(0.88f, 0.94f, 1.0f) * 2.0f;
	vec3 eyeVec = normalize(worldPosition.xyz - miscFactors.xyz);
	vec3 reflectDir = reflect(lightDir, worldNormal);
	float lightStrength = pow(clamp(dot(reflectDir, eyeVec), 0, 1), 1.0f);
	vec3 diffuseLightColor = lightColor * lightStrength * diffuseColor;
	float ambientStrength = 0.2f;
	vec3 ambientLightColor = lightColor * ambientStrength * diffuseColor;
	
	// calculate fresnel factor
	float r = -0.01f + pow(1.0 + dot(eyeVec, normalize(worldNormal)), 1.0);

	// calculate exponential height fog
	float dist = length(viewPosition);

	// use smoothstep function to determine extinction and in-scattering

	float be = fog_data.extinction * smoothstep(0.0, 6.0, fog_data.camera_height - viewPosition.z);
	float bi = fog_data.in_scattering * smoothstep(0.0, 100.0, fog_data.camera_height - viewPosition.z);

	float ext = exp(-dist * be);
	float insc = exp(-dist * bi);

	vec3 color = diffuseLightColor;// + ambientLightColor;
	color = color * ext + color_data.fogColor.xyz * (1.0 - insc);

	outColor.xyz = color;
	outColor.w = clamp((1.0 - insc) + r, 0, 1);
}