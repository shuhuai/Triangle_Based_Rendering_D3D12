//--------------------------------------------------------------------------------------
// File: MaterialStructures.h
// 
// Define material data structures.
//--------------------------------------------------------------------------------------
#ifndef MaterialTypes_H
#define MaterialTypes_H

struct StandardMaterial
{
	float3 albedoColor;
	float3 specularColor;
	float gloss;
	uint albedoMapIdx;
};
#endif