//--------------------------------------------------------------------------------------
// File: PreviewLightPS.hlsl
//
// A pixel shader to output lights' colors.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"

float4 main(vs_show_light_out pIn) : SV_TARGET
{
	return float4(pIn.color, 1.0f);
}