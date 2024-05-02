#include "Lighting.hlsli"

struct VertexToPixel
{
	float4 position			:	SV_POSITION;
	float2 uv				:	TEXCOORD;
};

float G1_Schlick(float roughness, float NdotV)

{
	float roughSquare = roughness * roughness;
	roughSquare /= 2.0f;

	return NdotV / (NdotV * (1.0f - roughSquare) + roughSquare);
}

float G_Smith(float roughness, float NdotV, float NdotL)
{
	return G1_Schlick(roughness, NdotV) * G1_Schlick(roughness, NdotL);
}

float2 IntegrateBRDF(float roughness, float nDotV)
{
	float3 V;
	V.x = sqrt(1.0f - nDotV * nDotV);
	V.y = 0;
	V.z = nDotV;

	float N = float3(0, 0, 1);
	float a = 0;
	float b = 0;

	for (uint i = 0; i < MAX_IBL_SAMPLES; i++)
	{
		float2 Xi = Hammersley2d(i, MAX_IBL_SAMPLES);	  // Evenly spaced 2D point from index
		float3 H = ImportanceSampleGGX(Xi, roughness, N); // Turn 2D point into 3D vector
		float3 L = 2 * dot(V, H) * H - V;

		float nDotL = saturate(L.z);
		float nDotH = saturate(H.z);
		float vDotH = saturate(dot(V, H));

		// Check N dot L result
		if (nDotL > 0)
		{
			float G = G_Smith(roughness, nDotV, nDotL);
			float G_Vis = G * vDotH / (nDotH * nDotV);
			float Fc = pow(1 - vDotH, 5);
			a += (1 - Fc) * G_Vis; // Fresnel scale (part of the output)
			b += Fc * G_Vis;	   // Fresnel bias (other part of the output)
		}
	}

	return float2(a, b) / MAX_IBL_SAMPLES;
}

float4 main(VertexToPixel input) : SV_TARGET
{
	float roughness = input.uv.y;
	float NdotV = input.uv.x;

	float2 brdf = IntegrateBRDF(roughness, NdotV);

	return float4(brdf, 0, 1);
}