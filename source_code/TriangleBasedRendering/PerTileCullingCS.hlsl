//--------------------------------------------------------------------------------------
// File: PerTileCullingCS.hlsl
//
// A light culling compute shader to compute the intersections between lights and tiles.
//--------------------------------------------------------------------------------------
#include "ClusteredCommon.h"
#include "CameraCommon.h"

// Global variables.
StructuredBuffer<PointLight> gLightSRV : register(t0);
StructuredBuffer<float> gDepthPlaneSRV : register(t1);
ConstantBuffer<ClusteredData> gCB : register(b0);
ConstantBuffer<ViewData> gViewCB : register(b1);
RWStructuredBuffer<ClusteredBuffer> gDataUAV : register(u0);
RWStructuredBuffer<int> gLightCounterUAV : register(u1);	// Light counter buffer.
Texture2D gDepthBuffer : register(t2);

// Group shared variables.
groupshared uint ldsLightCounter;
groupshared float4 ldsVertexes[8];
groupshared float4 ldsPlanes[6];
groupshared uint ldsLightIdx[PerClusterMaxLight];
groupshared float ldsDepth[TileSize*TileSize];
groupshared uint ldsZMax;
groupshared uint ldsZMin;


// Convert a point from post-projection space into view space.
float4 ConvertProjToView(float4 p)
{
	p = mul(p, gViewCB.ProjInv);
	p /= p.w;
	return p;
}

// This creates the standard Hessian-normal-form plane equation from three points, 
// except it is simplified for the case where the first point is the origin.
float4 CreatePlaneEquation(float4 b, float4 c)
{
	float4 n;
	
	// Normalize(cross( b.xyz-a.xyz, c.xyz-a.xyz )), except we know "a" is the origin.
	n.xyz = normalize(cross(b.xyz, c.xyz));

	// -(n dot a), except we know "a" is the origin.
	n.w = 0;

	return n;
}
float4 CreatePlaneEquation(float4 b, float4 c, float4 a)
{
	float4 n;

	// Normalize(cross( b.xyz-a.xyz, c.xyz-a.xyz )), except we know "a" is the origin.
	n.xyz = normalize(cross(b.xyz - a.xyz, c.xyz - a.xyz));

	// -(n dot a), except we know "a" is the origin.
	n.w = -dot(a.xyz, n.xyz);

	return n;
}
// Point-plane distance, simplified for the case where 
// the plane passes through the origin.
float GetSignedDistanceFromPlane(float4 p, float4 eqn)
{
	// Dot( eqn.xyz, p.xyz ) + eqn.w, , except we know eqn.w is zero.
	return dot(eqn.xyz, p.xyz) + eqn.w;
}

[numthreads(NumThreadX, NumThreadY, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint Gindex : SV_GroupIndex)
{
	float x[2];
	float y[2];
	float z[2];

	x[0] = gCB.tileSizeX*Gid.x;
	y[0] = gCB.tileSizeY*Gid.y;
	uint tileIdxFlattened = Gid.x + Gid.y*gCB.widthDim + Gid.z*gCB.widthDim*gCB.heightDim;

	if (Gindex ==0)
	{
		ldsZMin = 0x7f7fffff;
		ldsZMax = 0;
	}

	for (uint i = Gindex; i < TileSize*TileSize; i += NUM_THREADS_PER_TILE)
	{
		float tileThreadIdxY = i / TileSize;
		float tileThreadIdxX = i %  TileSize;
		ldsDepth[i] = gDepthBuffer[float2(x[0]+ tileThreadIdxX,y[0]+ tileThreadIdxY)].x;
	}
	GroupMemoryBarrierWithGroupSync();

	for (uint i = Gindex; i < TileSize*TileSize; i += NUM_THREADS_PER_TILE)
	{
		InterlockedMax(ldsZMax, asuint(ldsDepth[i]));
		InterlockedMin(ldsZMin, asuint(ldsDepth[i]));
	}
	GroupMemoryBarrierWithGroupSync();
	
	// Use 8 threads of a group thread to compute 8 different vertexes.
	[branch]
	if (Gindex < 8)
	{
		// Create a projected position according to group index.
		z[0] = asfloat(ldsZMin);
		x[1] = gCB.tileSizeX*(Gid.x + 1);
		y[1] = gCB.tileSizeY*(Gid.y + 1);
		z[1] = asfloat(ldsZMax);
		// Select a vertex of 8 vertexes.
		uint xId = Gindex & 0x1;
		uint yId = (Gindex & 0x2) >> 1;
		uint zId = (Gindex & 0x4) >> 2;
		uint uWindowWidthEvenlyDivisibleByTileRes = gCB.tileSizeX*gCB.widthDim;
		uint uWindowHeightEvenlyDivisibleByTileRes = gCB.tileSizeY*gCB.heightDim;
		float4 projPos = float4(x[xId] / (float)uWindowWidthEvenlyDivisibleByTileRes*2.f - 1.f,
			(uWindowHeightEvenlyDivisibleByTileRes - y[yId]) / (float)uWindowHeightEvenlyDivisibleByTileRes*2.f - 1.f,
			z[zId], 1.0f);
		// Transform to the view-space.
		ldsVertexes[Gindex] = ConvertProjToView(projPos);
	}

	GroupMemoryBarrierWithGroupSync();

	[branch]
	if (Gindex == 0)
	{
		// Initialize groupshared variables.
		ldsLightCounter = 0;

		// Vertexes of a frustum.
		//   4---5
		//  /   /|
		// 0---1 7
		// |   |/
		// 2---3
		// Plane1: Up.
		ldsPlanes[0] = CreatePlaneEquation(ldsVertexes[1], ldsVertexes[4], ldsVertexes[5]);
		// Plane2: Front.
		ldsPlanes[1] = CreatePlaneEquation(ldsVertexes[2], ldsVertexes[0], ldsVertexes[1]);
		// Plane3: Right.
		ldsPlanes[2] = CreatePlaneEquation(ldsVertexes[7], ldsVertexes[1], ldsVertexes[5]);
		// Plane4: Down.
		ldsPlanes[3] = CreatePlaneEquation(ldsVertexes[7], ldsVertexes[2], ldsVertexes[3]);
		// Plane5: Left.
		ldsPlanes[4] = CreatePlaneEquation(ldsVertexes[2], ldsVertexes[4], ldsVertexes[0]);
		// Plane6: Back.
		ldsPlanes[5] = CreatePlaneEquation(ldsVertexes[4], ldsVertexes[7], ldsVertexes[5]);
	}

	GroupMemoryBarrierWithGroupSync();

	// Use threads of a group to compute the intersections between lights and frustums.
	// Every thread compute a different intersection, and
	// loop offset is equal to the number of threads of a group.
	float r[6];
	for (uint i = Gindex; i < gCB.lightNum; i += NUM_THREADS_PER_TILE)
	{
		// Transform lights to view-space.
		uint dstIdx = 0;
		[branch]
		if (i < gCB.lightNum)
		{
			PointLight L = gLightSRV[i];
			float4 center = mul(float4(L.pos, 1), gViewCB.View);
			center /= center.w;
			// Determine the intersections between lights and six planes.
			[unroll]
			for (int j = 0; j < 6; j++)
			{
				r[j] = GetSignedDistanceFromPlane(center, ldsPlanes[j]);
			}
			// In the frustum?
			[branch]
			if (r[0] < L.radius  && r[1] < L.radius  && r[2] < L.radius && r[3] < L.radius && r[4] < L.radius && r[5] < L.radius)
			{
				InterlockedAdd(ldsLightCounter, 1, dstIdx);
				ldsLightIdx[dstIdx] = i;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();
	// Store data into light indexed buffer.
	[branch]
	if (Gindex == 0)
	{
		//gDataUAV[tileIdxFlattened].counter = ldsLightCounter;
		gLightCounterUAV[tileIdxFlattened] = ldsLightCounter;
		gDataUAV[tileIdxFlattened].lightIdxs = ldsLightIdx;
	}
}