//--------------------------------------------------------------------------------------
// File: ClusteredCommon.h
// 
// The constant buffer structure for camera data.
//--------------------------------------------------------------------------------------

#ifndef VIEWDATA_DEFINE
#define VIEWDATA_DEFINE
struct ViewData
{
	float4x4 MVP;
	float4x4 InvPV;
	float4x4 ProjInv;
	float4x4 View;
	float4x4 Proj;
	float3 CamPos;
};
#endif