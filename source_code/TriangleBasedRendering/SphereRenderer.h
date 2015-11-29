//--------------------------------------------------------------------------------------
// File: SphereRenderer.h
//
// The class is to create and render sphere models.
//--------------------------------------------------------------------------------------
#pragma once
#include "DirectxHelper.h"

class SphereRenderer
{
public:
	void Init(float fRadius, int iSlices, int iSegment);
	const void Render(ID3D12GraphicsCommandList* const commandList, const UINT instanceNum = 1);

private:
	void resourceSetup();

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW m_vbView;
	D3D12_INDEX_BUFFER_VIEW m_ibView;

	int m_iSlices;
	int m_iSegments;
	int m_iTriangleSize;
	int m_iIndexSize;
	float m_fRadius;
};