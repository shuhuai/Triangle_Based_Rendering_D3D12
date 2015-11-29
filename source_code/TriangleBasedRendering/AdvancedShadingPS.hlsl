//--------------------------------------------------------------------------------------
// File: AdvancedShadingPS.hlsl
//
// A pixel shader for storing data to G-buffers.
// It uses dynamic indexing to load textures in the descriptor heap.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"
#include "MaterialDefine.hlsli"

// To sample a texture in the descriptor heap correctly, I need to use a sampler in a sampler heap (Incorrectly sampling when I use a static sampler)
SamplerState gLinearSample : register(s0);

ps_output main(vs_full_out pIn)
{
	ps_output output;

	// Load material data from the material buffer.
	StandardMaterial mat = gMaterialBuffer[pIn.matIdx];
	// Use dynamic indexing to load an albedo texture.
	float3 albedo = gMapSet[mat.albedoMapIdx].Sample(gLinearSample, ((pIn.texcoord))).xyz*mat.albedoColor;
	output.albedo = albedo;
	output.normal = float4(pIn.normal, 1.0f);
	output.specgloss = float4(1 - mat.specularColor, 0.6);

	return output;
}