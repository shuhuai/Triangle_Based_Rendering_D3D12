//--------------------------------------------------------------------------------------
// File: ScreenQuadRenderer.h
//
// A class for creating a screen quad or tiles for screen-based techniques.
//--------------------------------------------------------------------------------------
#pragma once
#include "DirectxHelper.h"

class ScreenQuadRenderer
{
public:

	void Init();
	void InitTiles(int iTileNumX, int iTileNumY);

	void Render(ID3D12GraphicsCommandList* const commandList);
	void RenderTiles(ID3D12GraphicsCommandList* const commandList, const UINT instanceNum = 1);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

	D3D12_INDEX_BUFFER_VIEW m_ibView;
	D3D12_VERTEX_BUFFER_VIEW m_vbView;

	int m_iNumTriangle;
};