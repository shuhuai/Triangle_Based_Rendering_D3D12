//--------------------------------------------------------------------------------------
// File: MaterialDefine.hlsli
//
// Define materials' structures and resources for shaders.
//--------------------------------------------------------------------------------------

struct StandardMaterial
{
	float3 albedoColor;
	float3 specularColor;
	float gloss;
	uint albedoMapIdx;
};

Texture2D<float4> gMapSet[128] : register(t1, space1);

StructuredBuffer<StandardMaterial> gMaterialBuffer : register(t0, space1);