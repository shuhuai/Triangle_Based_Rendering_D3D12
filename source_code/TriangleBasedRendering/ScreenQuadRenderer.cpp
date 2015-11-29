//--------------------------------------------------------------------------------------
// File: ScreenQuadRenderer.cpp
//--------------------------------------------------------------------------------------
#include "ScreenQuadRenderer.h"
#include  "VertexStructures.h"
#include "d3dx12.h"
#include <vector>

void ScreenQuadRenderer::Init()
{
	ScreenQuadVertex QuadVerts[] =
	{
		{ { -1.0f,1.0f, 0.0f,1.0f },{ 0.0f,0.0f } },
		{ { 1.0f, 1.0f, 0.0f,1.0f }, {1.0f,0.0f } },
		{ { -1.0f, -1.0f, 0.0f,1.0f },{ 0.0f,1.0f } },
	{ { 1.0f, -1.0f, 0.0f,1.0f },{ 1.0f,1.0f } }
	};

	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = sizeof(QuadVerts);
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())));

	UINT8* dataBegin;
	ThrowIfFailed(m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dataBegin)));
	memcpy(dataBegin, QuadVerts, sizeof(QuadVerts));
	m_vertexBuffer->Unmap(0, nullptr);

	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.StrideInBytes = sizeof(ScreenQuadVertex);
	m_vbView.SizeInBytes = sizeof(QuadVerts);

	g_copyManager->Add({ m_vertexBuffer,&m_vbView });

}

void ScreenQuadRenderer::InitTiles(int iTileNumX, int iTileNumY)
{

	std::vector<ScreenQuadVertex> VertexList;
	float invTileXNum = 1.0f / iTileNumX;
	float invTileYNum = 1.0f / iTileNumY;
	// Create vertex buffer.
	for (int j = 0; j < iTileNumY + 1; j++)
	{
		float v = invTileYNum*j;
		float y = -v * 2 + 1.0f;
		for (int i = 0; i < iTileNumX + 1; i++)
		{
			float u = invTileXNum*i;
			float x = u * 2 - 1.0f;
			ScreenQuadVertex vertex = { { x,y, 0.0f,1.0f },{ u,v } };
			VertexList.push_back(vertex);
		}

	}
	CreateCommittedBufferResource<ScreenQuadVertex>(&VertexList[0], VertexList.size(), m_vertexBuffer);

	std::vector<UINT> IndexList;
	// Create index buffer.
	for (int j = 0; j < iTileNumY; j++)
	{
		for (int i = 0; i < iTileNumX; i++)
		{
			IndexList.push_back(i + (iTileNumX + 1)*j);
			IndexList.push_back(i + 1 + (iTileNumX + 1)*j);
			IndexList.push_back((j + 1)*(iTileNumX + 1) + i);

			IndexList.push_back(i + 1 + (iTileNumX + 1)*j);
			IndexList.push_back((j + 1)*(iTileNumX + 1) + i + 1);
			IndexList.push_back((j + 1)*(iTileNumX + 1) + i);
		}
	}
	CreateCommittedBufferResource<UINT>(&IndexList[0], IndexList.size(), m_indexBuffer);

	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.StrideInBytes = sizeof(ScreenQuadVertex);
	m_vbView.SizeInBytes = sizeof(ScreenQuadVertex)*VertexList.size();

	m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_ibView.Format = DXGI_FORMAT_R32_UINT;
	m_ibView.SizeInBytes = sizeof(UINT)*IndexList.size();
	m_iNumTriangle = IndexList.size();

	g_copyManager->Add({ m_vertexBuffer,&m_vbView });
	g_copyManager->Add({ m_indexBuffer,&m_ibView });
}

void ScreenQuadRenderer::Render(ID3D12GraphicsCommandList* const  commandList)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &m_vbView);
	commandList->DrawInstanced(4, 1, 0, 0);
}

void ScreenQuadRenderer::RenderTiles(ID3D12GraphicsCommandList* const commandList, const  UINT instanceNum)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetIndexBuffer(&m_ibView);
	commandList->IASetVertexBuffers(0, 1, &m_vbView);
	commandList->DrawIndexedInstanced(m_iNumTriangle, instanceNum, 0, 0, 0);
}
