//--------------------------------------------------------------------------------------
// File: PreviewLightVS.hlsl
//
// A vertex shader to load data in light buffer, and it uses this data to transform positions.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"
StructuredBuffer<PointLight> gLightSRV : register(t0);	// Light buffer.

vs_show_light_out main(vs_show_light_in vIn,uint instanceIdx : SV_InstanceID)
{
	vs_show_light_out vOut;
	// Load the data.
	PointLight light = gLightSRV[instanceIdx];
	// Change the scale according to the light's radius.
	vIn.position.xyz *= 0.05f*(1+ light.radius);
	// Transform the position according to the light's position.
	vIn.position.xyz+= light.pos;

	vOut.position = mul(vIn.position, gViewCB.MVP);
	// Send color data to pixel shader.
	vOut.color = light.color;

	return vOut;
}