//--------------------------------------------------------------------------------------
// File: LightPassTriangleGS.hlsl
//
// A geometry shader to send light indexed data to the pixel shader.
//--------------------------------------------------------------------------------------
#include "DeferredRender.hlsli"

StructuredBuffer<ClusteredBuffer> gPerTileLightIndex : register(t5);	// Light indexed buffer.

[maxvertexcount(3)]
void main(
	triangle gs_in input[3], uint index :  SV_PrimitiveID,
	inout TriangleStream< gs_out > output
	)
{

	uint tileIndex = index;

	// Use triangles' indexes to load an element in light indexed buffer.
	uint totalNum = gPerTileLightIndex[tileIndex].counter;
	{
		gs_out element;
		// Triangle's index.
		element.tileID = tileIndex;
		// The total number of lights in this triangle.
		element.tileCounter = totalNum;
		if (totalNum > 0)
		{
			for (uint j = 0; j < 3; j++)
			{
				element.position = input[j].position;
				element.texcoord = input[j].texcoord;
				output.Append(element);
			}
		}
		// Generate a new triangle.
		output.RestartStrip();;
	}
}