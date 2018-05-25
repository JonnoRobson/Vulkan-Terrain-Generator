#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;


// textures
layout(binding = 0) uniform sampler bufferSampler;
layout(binding = 1) uniform texture2D sourceTexture;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	vec4 color = texture(sampler2D(sourceTexture, bufferSampler), fragTexCoord);
	
	if(color.r > 1.0f || color.g >= 1.0f || color.b >= 1.0f)
		outColor = color;
	else
		outColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}