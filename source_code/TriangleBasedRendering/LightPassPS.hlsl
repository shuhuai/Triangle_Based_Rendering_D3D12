//--------------------------------------------------------------------------------------
// File: LightPassPS.hlsl
//
// A pixel shader for triangle-based lighting method.
// It uses the information passed from geometry shader to loop lights.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"
#include "Lighting.hlsli"

// G-buffer.
Texture2D gAlbedoTexture : register(t0);
Texture2D gNormalTexture : register(t1);
Texture2D gSpecularGlossTexture : register(t2);
Texture2D gDepth: register(t3);

// Light culling data.
StructuredBuffer<ClusteredBuffer> gPerTileLightIndex : register(t5);	// Light indexed buffer.
ConstantBuffer<ClusteredData> gCB : register(b2);	// Light culling information.
StructuredBuffer<PointLight> gLightSRV : register(t6);	// Light buffer.

float4 main(gs_out pIn) : SV_TARGET
{
	// Reconstruct world position with depth buffer.
	float z = gDepth[pIn.position.xy].x;
	float4 vProjectedPos = float4(pIn.position.xy, z, 1.0f);

	// Transform by the inverse screen view projection matrix to world space.
	float4 vPositionWS = mul(vProjectedPos, gViewCB.InvPV);
	vPositionWS = vPositionWS / vPositionWS.w;

	// Load G-buffer.
	float3 albedo = gAlbedoTexture[pIn.position.xy].xyz;
	float3 normal = normalize(gNormalTexture[pIn.position.xy].xyz);
	float4 specGloss = gSpecularGlossTexture[pIn.position.xy].xyzw;
	float3 viewDir = normalize(gViewCB.CamPos - vPositionWS.xyz);

	float3 col = 0;

	[loop]
	for (uint i = 0; i < pIn.tileCounter; i++)
	{
	
		{
			// Load a light in light buffer.
			PointLight L;
			L = gLightSRV[gPerTileLightIndex[pIn.tileID].lightIdxs[i]];
			// Attenuation light (This computation make sure the light intensity decrease to 0, but it is not physically-based).
			float d = length(L.pos - vPositionWS.xyz);
			d = saturate(1 - d / L.radius) * 1;
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
	}

return float4(col,1);
}