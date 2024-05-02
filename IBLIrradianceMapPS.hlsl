#include "Lighting.hlsli"

cbuffer externalData : register(b0)
{
	int faceIndex;
};

struct VertexToPixel
{
	float4 position			:	SV_POSITION;
	float2 uv				:	TEXCOORD;
};

TextureCube EnvironmentMap			:	register(t0);
SamplerState BasicSampler			:	register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	float2 o = input.uv * 2 - 1;

	float3 xDir, yDir, zDir;

	switch (faceIndex)
	{
		default:
		case 0: zDir = float3(1, -o.y, -o.x); break;
		case 1: zDir = float3(-1, -o.y, o.x); break;
		case 2: zDir = float3(o.x, 1, o.y); break;
		case 3: zDir = float3(o.x, -1, -o.y); break;
		case 4: zDir = float3(o.x, -o.y, 1); break;
		case 5: zDir = float3(-o.x, -o.y, -1); break;
	}
	zDir = normalize(zDir);

	xDir = normalize(cross(float3(0, 1, 0), zDir));
	yDir = normalize(cross(zDir, xDir));

	float3 totalColor = float3(0, 0, 0);
	int sampleCount = 0;

	//  Variables for various sin/cos values
	float sinT, cosT, sinP, cosP;

	// Loop around the hemisphere (360 degrees)
	for (float phi = 0.0f; phi < PI * 2; phi += IRRADIANCE_SAMPLE_STEP_PHI)
	{
		sincos(phi, sinP, cosP); // Grab the sin and cos of phi

		// Loop down the hemisphere (90 degrees)
		for (float theta = 0.0f; theta < PI / 2; theta += IRRADIANCE_SAMPLE_STEP_THETA)
		{
			sincos(theta, sinT, cosT);

			// Get an X/Y/Z direction from the polar coords, then change to world space
			float3 hemisphereDir = float3(sinT * cosP, sinT * sinP, cosT);
			hemisphereDir = hemisphereDir.x * xDir + hemisphereDir.y * yDir + hemisphereDir.z * zDir;

			// Sample in that direction
			totalColor += cosT * sinT * pow(abs(EnvironmentMap.Sample(BasicSampler, hemisphereDir).rgb), 2.2f);
			sampleCount++;
		}
	}

	float3 finalColor = PI * totalColor / sampleCount;
	return float4(pow(abs(finalColor), 1.0f / 2.2f), 1);
}