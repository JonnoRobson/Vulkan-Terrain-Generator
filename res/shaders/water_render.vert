#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inMatIndex;

#define TERRAIN_CHUNK_COUNT 25
#define TERRAIN_CHUNK_SIZE 5

layout(binding = 0) uniform MatrixBuffer
{
	mat4 model;
	mat4 view;
	mat4 proj;
} matrices;

layout(binding = 1) uniform WaterDataBuffer
{
	vec3 camera_pos;
	float water_size;
	float time;
	float padding[3];
} water_data;

layout(binding = 2, r16f) uniform image2D watermap;


out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec4 worldPosition;
layout(location = 3) out vec3 miscFactors;
layout(location = 4) out vec4 viewPosition;

vec3 Sobel(vec2 uv)
{
	vec2 texelSize = vec2 (1.0f / water_data.water_size, 1.0f / water_data.water_size);

	// compute texel offsets
	vec2 offset00 = uv + vec2(-texelSize.x, -texelSize.y);
	vec2 offset10 = uv + vec2(0.0f, -texelSize.y);
	vec2 offset20 = uv + vec2(texelSize.x, -texelSize.y);
	
	vec2 offset01 = uv + vec2(-texelSize.x, 0.0f);
	vec2 offset21 = uv + vec2(texelSize.x, 0.0f);
	
	vec2 offset02 = uv + vec2(-texelSize.x, texelSize.y);
	vec2 offset12 = uv + vec2(0.0f, texelSize.y);
	vec2 offset22 = uv + vec2(texelSize.x, texelSize.y);

	// get the eight samples surrounding the current pixel
	float height00 = imageLoad(watermap, ivec2(offset00 * water_data.water_size)).r;
	float height10 = imageLoad(watermap, ivec2(offset10 * water_data.water_size)).r;
	float height20 = imageLoad(watermap, ivec2(offset20 * water_data.water_size)).r;
	
	float height01 = imageLoad(watermap, ivec2(offset01 * water_data.water_size)).r;
	float height21 = imageLoad(watermap, ivec2(offset21 * water_data.water_size)).r;
	
	float height02 = imageLoad(watermap, ivec2(offset02 * water_data.water_size)).r;
	float height12 = imageLoad(watermap, ivec2(offset12 * water_data.water_size)).r;
	float height22 = imageLoad(watermap, ivec2(offset22 * water_data.water_size)).r;

	// evaluate the sobel filters
	float Gx = height00 - height20 + 2.0f * height01 - 2.0f * height21 + height02 - height22;
	float Gy = height00 + 2.0f * height10 + height20 - height02 - 2.0f * height12 - height22;

	// generate the missing z
	float Gz = 0.01f * sqrt(max(0.0f, 1.0f - Gx * Gx - Gy * Gy));

	// make sure the returned normal is of unit length
	return normalize(vec3(2.0f * Gx, 2.0f * Gy, Gz));
}

void main()
{
	// get heightmap data
	vec4 mappedPosition = vec4(inPosition, 1.0f);
	mappedPosition.z = clamp(imageLoad(watermap, ivec2(inTexCoord * water_data.water_size)).r, 0, 10.0);
	
	vec2 radialCoords = inTexCoord * 2.0 - 1.0;
	float offset = clamp(sin((water_data.time + (radialCoords.x + radialCoords.y) * 100.0) * 0.5) * 1.1, -1, 1) * 0.0025;
	mappedPosition.z += offset;

	miscFactors = water_data.camera_pos;

	worldPosition = matrices.model * mappedPosition;
	viewPosition = matrices.view * worldPosition;
	gl_Position = matrices.proj * viewPosition;
	fragTexCoord = inTexCoord;
	worldNormal = Sobel(inTexCoord);
}