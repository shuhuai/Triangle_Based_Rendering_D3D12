//--------------------------------------------------------------------------------------
// File: AdvancedShadingVS.hlsl
//
// A vertex shader with the input layout:
// [position : POSITION]
// [normal : NORMAL]
// [texcoord : TEXCOORD]
// [matIdx: MATIDX]
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"

vs_full_out  main(vs_full_in vIn)
{
	vs_full_out vOut;
	vOut.position = mul(vIn.position, gViewCB.MVP);

	vOut.normal = vIn.normal;
	vOut.worldPos = vOut.position / vOut.position.w;
	vOut.texcoord = vIn.texcoord;

	vOut.matIdx = vIn.matIdx;
	return vOut;
}