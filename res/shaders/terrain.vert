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
	mat4 model[TERRAIN_CHUNK_COUNT];
	mat4 view;
	mat4 proj;
} matrices;

layout(binding = 1) uniform TerrainDataBuffer
{
	float terrain_size;
	vec3 camera_pos;
	vec4 water_size;
} terrain_data;

layout(binding = 2) uniform sampler heightmapSampler;
layout(binding = 3) uniform texture2D heightmaps[TERRAIN_CHUNK_COUNT];
layout(binding = 4) uniform texture2D watermap;


out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec3 miscFactors;
layout(location = 3) out vec4 viewPosition;

vec3 Sobel(vec2 uv)
{
	vec2 texelSize = vec2 (1.0f / terrain_data.terrain_size, 1.0f / terrain_data.terrain_size);

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
	float height00 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset00).r; 
	float height10 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset10).r; 
	float height20 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset20).r; 
	
	float height01 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset01).r; 
	float height21 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset21).r; 
	
	float height02 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset02).r; 
	float height12 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset12).r; 
	float height22 = texture(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), offset22).r; 

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
	vec4 mappedPosition = vec4(inPosition, 1.0f);

	// blend heightmap data at chunk edges
	if(inTexCoord.x == 1.0 && gl_InstanceIndex % TERRAIN_CHUNK_SIZE != TERRAIN_CHUNK_SIZE - 1)
		mappedPosition.z = texelFetch(sampler2D(heightmaps[gl_InstanceIndex + 1], heightmapSampler), ivec2(vec2(0.0, inTexCoord.y) * terrain_data.terrain_size), 0).r;
	else if (inTexCoord.y == 1.0 && gl_InstanceIndex > TERRAIN_CHUNK_SIZE - 1)
		mappedPosition.z = texelFetch(sampler2D(heightmaps[gl_InstanceIndex - TERRAIN_CHUNK_SIZE], heightmapSampler), ivec2(vec2(inTexCoord.x, 0.0) * terrain_data.terrain_size), 0).r;
	else
		mappedPosition.z = texelFetch(sampler2D(heightmaps[gl_InstanceIndex], heightmapSampler), ivec2(inTexCoord * terrain_data.terrain_size), 0).r;

	// get watermap data
	vec2 watermapIndices = vec2(0, 0);
	watermapIndices.y = (TERRAIN_CHUNK_SIZE - 1) - (gl_InstanceIndex  / TERRAIN_CHUNK_SIZE);
	watermapIndices.x = gl_InstanceIndex - ((gl_InstanceIndex / TERRAIN_CHUNK_SIZE) * TERRAIN_CHUNK_SIZE);
	vec2 watermapChunkDimensions = vec2(1280.0, 1280.0) / float(TERRAIN_CHUNK_SIZE);
	vec2 watermapCoords = vec2(inTexCoord.x, inTexCoord.y) * watermapChunkDimensions;
	watermapCoords = watermapCoords + (watermapChunkDimensions * watermapIndices);

	float waterValue = texelFetch(sampler2D(watermap, heightmapSampler), ivec2(watermapCoords), 0).x;

	//mappedPosition.z = waterValue;

	miscFactors = vec3(mappedPosition.z, clamp((mappedPosition.z - waterValue) * 20.0, 0, 1), float(gl_InstanceIndex) / float(TERRAIN_CHUNK_COUNT));

	viewPosition = matrices.view * matrices.model[gl_InstanceIndex] * mappedPosition;
	gl_Position = matrices.proj * viewPosition;
	fragTexCoord = inTexCoord;
	worldNormal = Sobel(inTexCoord);
}