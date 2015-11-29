//--------------------------------------------------------------------------------------
// File: ScreenQuadVS.hlsl
//
// A vertex shader to render a screen-based quad.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"

// Pass data to geometry shader.
gs_in  main(vs_in vIn)
{
	gs_in vOut;
	vOut.position = vIn.position;
	vOut.texcoord = vIn.texcoord;

	return vOut;
}