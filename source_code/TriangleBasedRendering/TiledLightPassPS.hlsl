//--------------------------------------------------------------------------------------
// File: TiledLightPassPS.hlsl
//
// A pixel shader for original tile-based lighting method, so it loops all lights of a tile.
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

float4 main(gs_in  pIn) : SV_TARGET
{
	uint2 tileAddress = floor(pIn.position.xy / float2(gCB.tileSizeX, gCB.tileSizeY));
	
	float z = gDepth[pIn.position.xy].x;
	float4 vProjectedPos = float4(pIn.position.xy, z, 1.0f);

	// Transform by the inverse screen view projection matrix to world space.
	float4 vPositionWS = mul(vProjectedPos, gViewCB.InvPV);

	vPositionWS = vPositionWS / vPositionWS.w;
	float3 albedo = gAlbedoTexture[pIn.position.xy].xyz;
	float3 normal = normalize(gNormalTexture[pIn.position.xy].xyz);
	float4 specGloss = gSpecularGlossTexture[pIn.position.xy].xyzw;


	float3 viewDir = normalize(gViewCB.CamPos - vPositionWS.xyz);

	int index = tileAddress.x + max((int)tileAddress.y,0) * gCB.widthDim;

	float3 col = 0;
		[loop]
		for (int i = 0; i < gPerTileLightIndex[index].counter; i++)
		{
			PointLight L;
			L = gLightSRV[gPerTileLightIndex[index].lightIdxs[i]];
			float d = length(L.pos - vPositionWS.xyz);
			d = saturate(1 - d / L.radius)*1;
			[branch]
			if (d > 0)
			{
				// Lighting calculation.
				float3 lightVector = normalize(L.pos - vPositionWS.xyz);
				[branch]
				if (dot(lightVector, normal) > 0)
				{
					float3 res = GGXBRDF(lightVector, L.pos, albedo, normal,
						viewDir, specGloss.xyz, specGloss.w);

					col += res*d*L.color;
				}
			}
		}
	

	return float4(col,1.0f);


}