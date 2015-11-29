//--------------------------------------------------------------------------------------
// File: DebugLightPS.hlsl
//
// A pixel shader to show the number of lights for each triangle ( or tile).
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"
#include "Lighting.hlsli"

Texture2D gAlbedoTexture : register(t0);
Texture2D gNormalTexture : register(t1);
Texture2D gSpecularGlossTexture : register(t2);
Texture2D gDepth: register(t3);
StructuredBuffer<ClusteredBuffer> gPerTileLightIndex : register(t5);
ConstantBuffer<ClusteredData> gCB : register(b2);
StructuredBuffer<PointLight> gLightSRV : register(t6);

sampler gLinearSample;

float4 main(gs_out pIn) : SV_TARGET
{
	float3 color = 0;
	uint channelNum = (PerClusterMaxLight / 3);
	uint colIndex = pIn.tileCounter / channelNum;
	float remaider = (float)(pIn.tileCounter % channelNum) / (float)(channelNum);

	if (colIndex > 1)
	{
		color.r = remaider*0.5f + 0.5f;
	}
	else if (colIndex == 1)
	{
		color.g = remaider*0.5f + 0.5f;
	}
	else if (colIndex == 0)
	{
		color.b = remaider;
	}

	return float4(color,1.0f);
}