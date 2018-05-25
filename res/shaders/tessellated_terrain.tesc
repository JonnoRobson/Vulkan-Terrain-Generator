#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec2 inTexCoord[];

layout(binding = 1) uniform LODFactorsBuffer
{
	vec4 minMaxDistance;
	vec4 minMaxLOD;
	vec4 cameraPos;
} lod_factors;

layout(vertices = 4) out;
layout(location = 0) out vec3 outPosition[];
layout(location = 1) out vec2 texCoord[];

vec3 ComputePatchMidPoint(vec3 cp0, vec3 cp1, vec3 cp2, vec3 cp3)
{
	// determine the mid point of the patch
	return (cp0 + cp1 + cp2 + cp3) / 4.0f;
}

float ComputeScaledDistance(vec3 from, vec3 to)
{
	// compute raw distance from point to the midpoint of this patch
	float d = distance(from, to);

	// scale the distance to 0-1 range
	return (d - lod_factors.minMaxDistance.x) / (lod_factors.minMaxDistance.y - lod_factors.minMaxDistance.x);
}

float ComputePatchLOD(vec3 midPoint)
{
	// compute the distance to the camera from the patch midpoint
	float d = ComputeScaledDistance(lod_factors.cameraPos.xyz, midPoint);

	// transform the distance scale into the LOD level
	return mix(lod_factors.minMaxLOD.x, lod_factors.minMaxLOD.y, 1.0f - d);
}

void main()
{
	uint ID = gl_InvocationID;
	outPosition[gl_InvocationID] = inPosition[gl_InvocationID];
	texCoord[gl_InvocationID] = inTexCoord[gl_InvocationID];

	// determine the midpoint of the patch
	vec3 midPoints[] = 
	{
		// main quad
		ComputePatchMidPoint(inPosition[0], inPosition[1], inPosition[2], inPosition[3]),
		// +x neighbour
		ComputePatchMidPoint(inPosition[2], inPosition[3], inPosition[4], inPosition[5]),
		// +y neighbour
		ComputePatchMidPoint(inPosition[1], inPosition[3], inPosition[6], inPosition[7]),
		// -x neighbour
		ComputePatchMidPoint(inPosition[0], inPosition[1], inPosition[8], inPosition[9]),
		// -y neighbour
		ComputePatchMidPoint(inPosition[0], inPosition[2], inPosition[10], inPosition[11])
	};

	float detail[] =
	{
		// main quad
		ComputePatchLOD(midPoints[0]),
		// +x neighbour
		ComputePatchLOD(midPoints[1]),
		// +y
		ComputePatchLOD(midPoints[2]),
		// -x
		ComputePatchLOD(midPoints[3]),
		// -y
		ComputePatchLOD(midPoints[4])
	};

	// patch interior tessellation always matches patch LOD
	gl_TessLevelInner[0] = gl_TessLevelInner[1] = detail[0];

	// if neighbour patch is lower LOD then pick that LOD
	// if neighbour patch is higher LOD then keep LOD and neighbour will blend towards it
	gl_TessLevelOuter[0] = min(detail[0], detail[4]);
	gl_TessLevelOuter[1] = min(detail[0], detail[3]);
	gl_TessLevelOuter[2] = min(detail[0], detail[2]);
	gl_TessLevelOuter[3] = min(detail[0], detail[1]);	
}