//--------------------------------------------------------------------------------------
// File: BasicVS.hlsl
//
// A vertex shader with the input layout:
// [float4 position : POSITION]
// [float3 normal : NORMAL]
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"

vs_gbuffer_out main(vs_full_in vIn)
{

	vs_gbuffer_out vOut;
	vOut.position = mul(vIn.position, gViewCB.MVP);

	vOut.normal = vIn.normal;
	vOut.worldPos = vOut.position / vOut.position.w;
	return vOut;
}