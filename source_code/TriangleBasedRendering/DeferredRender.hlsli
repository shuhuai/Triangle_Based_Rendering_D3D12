//--------------------------------------------------------------------------------------
// File: DeferredRender.hlsli
//
// Common structures and resources for deferred rendering.
//--------------------------------------------------------------------------------------
#include "ClusteredCommon.h"
#include "CameraCommon.h"

ConstantBuffer<ViewData> gViewCB : register(b0);

struct vs_in {
	float4 position : POSITION;
	float2 texcoord : TEXCOORD;
};

struct gs_in {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

struct ps_output
{
	float3 albedo : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 specgloss : SV_TARGET2;
};

struct gs_out {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
	nointerpolation uint tileID : TEXCOORD1;
	nointerpolation uint tileCounter : TEXCOORD2;
	//nointerpolation uint tileBaseID : TEXCOORD3;
};

struct vs_out {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
	uint tileCounter : TEXCOORD1;
};

struct vs_gbuffer_in {
	float4 position : POSITION;
	float3 normal : NORMAL;
};


struct vs_gbuffer_out {
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float4 worldPos :TEXCOORD;
};

struct vs_full_in {
	float4 position : POSITION;
	float3 normal : NORMAL;
	float2 texcoord : TEXCOORD;
	uint matIdx: MATIDX;
};

struct vs_full_out {
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float4 worldPos :TEXCOORD0;
	float2 texcoord : TEXCOORD1;
	uint matIdx: TEXCOORD2;

};
struct vs_show_light_in {
	float4 position : POSITION;
	float3 normal : NORMAL;
};

struct vs_show_light_out {
	float4 position : SV_POSITION;
	float3 color : COLOR;
};


struct SurfaceData
{
	float3 positionView;         // View space position.
	float3 normal;               // View space normal.
	float4 albedo;
	float3 specular;
	float gloss;
};