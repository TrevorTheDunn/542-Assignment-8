#include "Lighting.hlsli"

cbuffer data : register(b0)
{
	float roughness;
	int faceIndex;
	int mipLevel;
}

struct VertexToPixel
{
	float4 position			:	SV_POSITION;
	float2 uv				:	TEXCOORD;
};

TextureCube EnvironmentMap		:	register(t0);
SamplerState BasicSampler		:	register(s0);

float3 ConvolveTextureCube(float roughness, float3 r)
{
	float3 N = r;
	float3 V = r;

	float3 finalColor = float3(0, 0, 0);
	float totalWeight = 0;

	// Run the calculation MANY times
	for (uint i = 0; i < MAX_IBL_SAMPLES; i++)
	{
		// Grab this sample
		float2 Xi = Hammersley2d(i, MAX_IBL_SAMPLES); // Evenly spaced 2D point from index
		float3 H = ImportanceSampleGGX(Xi, roughness, N); // Turn 2D point into 3D vector
		float L = 2 * dot(V, H) * H - V;

		// Check N dot L result
		float nDotL = saturate(dot(N, L));
		if (nDotL > 0)
		{
			float3 color = EnvironmentMap.SampleLevel(BasicSampler, L, 0).rgb;
			color = pow(abs(color), 2.2f);
			finalColor += color * nDotL;
			totalWeight += nDotL;
		}
	}

	return pow(abs(finalColor / totalWeight), 1.0f / 2.2f);
}

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

	float3 c = ConvolveTextureCube(roughness, zDir);

	return float4(c, 1);
}