//--------------------------------------------------------------------------------------
// File: BasicPS.hlsl
//
// A pixel shader for storing data to G-buffers.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"

ps_output main(vs_gbuffer_out pIn) 
{
	ps_output output;
	output.albedo = 0;
	output.normal = 0;
	output.specgloss = 0;
	return output;
}