//--------------------------------------------------------------------------------------
// File: SphereRenderer.cpp
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "SphereRenderer.h"
#include "VertexStructures.h"
#include "DirectXMathConverter.h"
#include <vector>
#include <math.h>

using namespace DirectX;
void SphereRenderer::Init(float fRadius, int iSlices, int iSegment)
{
	m_fRadius = fRadius;
	m_iSlices = iSlices;
	m_iSegments = iSegment;

	resourceSetup();
}

void SphereRenderer::resourceSetup()
{

	std::vector< NormalVertex > verts;
	verts.resize((m_iSegments + 1)*m_iSlices + 2);

	const float _pi = XM_PI;
	const float _2pi = XM_2PI;

	verts[0].position = XMFLOAT4(0, m_fRadius, 0, 1);
	for (int lat = 0; lat < m_iSlices; lat++)
	{
		float a1 = _pi * (float)(lat + 1) / (m_iSlices + 1);
		float sin1 = sinf(a1);
		float cos1 = cosf(a1);

		for (int lon = 0; lon <= m_iSegments; lon++)
		{
			float a2 = _2pi * (float)(lon == m_iSegments ? 0 : lon) / m_iSegments;
			float sin2 = sinf(a2);
			float cos2 = cosf(a2);

			verts[lon + lat * (m_iSegments + 1) + 1].position = XMFLOAT4(sin1 * cos2* m_fRadius, cos1* m_fRadius, sin1 * sin2* m_fRadius, 1);
		}
	}
	verts[verts.size() - 1].position = XMFLOAT4(0, -m_fRadius, 0, 1);

	for (unsigned int n = 0; n < verts.size(); n++)
		verts[n].normal = GMathVF(XMVector3Normalize(GMathFV(XMFLOAT3(verts[n].position.x, verts[n].position.y, verts[n].position.z))));

	int nbFaces = verts.size();
	int nbTriangles = nbFaces * 2;
	int nbIndexes = nbTriangles * 3;
	std::vector< int >  triangles(nbIndexes);

	int i = 0;
	for (int lon = 0; lon < m_iSegments; lon++)
	{
		triangles[i++] = lon + 2;
		triangles[i++] = lon + 1;
		triangles[i++] = 0;
	}

	// Middle.
	for (int lat = 0; lat < m_iSlices - 1; lat++)
	{
		for (int lon = 0; lon < m_iSegments; lon++)
		{
			int current = lon + lat * (m_iSegments + 1) + 1;
			int next = current + m_iSegments + 1;

			triangles[i++] = current;
			triangles[i++] = current + 1;
			triangles[i++] = next + 1;

			triangles[i++] = current;
			triangles[i++] = next + 1;
			triangles[i++] = next;
		}
	}

	// Bottom Cap.
	for (int lon = 0; lon < m_iSegments; lon++)
	{
		triangles[i++] = verts.size() - 1;
		triangles[i++] = verts.size() - (lon + 2) - 1;
		triangles[i++] = verts.size() - (lon + 1) - 1;
	}
	m_iTriangleSize = verts.size();
	m_iIndexSize = triangles.size();

	D3D12_HEAP_PROPERTIES heapProperty;
	ZeroMemory(&heapProperty, sizeof(heapProperty));
	heapProperty.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = sizeof(NormalVertex)*verts.size();
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// Create a D3D12 resources for a vertex buffer.
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())));

	// Copy data to this vertex buffer.
	UINT8* dataBegin;
	ThrowIfFailed(m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dataBegin)));
	memcpy(dataBegin, &verts[0], sizeof(NormalVertex)*verts.size());
	m_vertexBuffer->Unmap(0, nullptr);

	// Create a vertex buffer view.
	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.StrideInBytes = sizeof(NormalVertex);
	m_vbView.SizeInBytes = sizeof(NormalVertex)*verts.size();

	// Create a D3D12 resource for the index buffer.
	resourceDesc.Width = sizeof(int)*triangles.size();
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_indexBuffer.GetAddressOf())));
	ThrowIfFailed(m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dataBegin)));
	memcpy(dataBegin, &triangles[0], sizeof(int)*triangles.size());
	m_indexBuffer->Unmap(0, nullptr);

	// Create an index buffer view.
	m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_ibView.Format = DXGI_FORMAT_R32_UINT;
	m_ibView.SizeInBytes = sizeof(int)*triangles.size();

	// Copy vertex and index buffers to default heap.
	g_copyManager->Add(UploadToDefaultObject(m_indexBuffer, &m_ibView));
	g_copyManager->Add(UploadToDefaultObject(m_vertexBuffer, &m_vbView));
}

const void SphereRenderer::Render(ID3D12GraphicsCommandList* const commandList, const UINT instanceNum)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetIndexBuffer(&m_ibView);
	commandList->IASetVertexBuffers(0, 1, &m_vbView);

	commandList->DrawIndexedInstanced(m_iIndexSize, instanceNum, 0, 0, 0);
}
