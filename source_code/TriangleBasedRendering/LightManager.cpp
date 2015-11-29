//--------------------------------------------------------------------------------------
// File: LightManager.cpp
//--------------------------------------------------------------------------------------
#include "LightManager.h"
#include "d3dx12.h"

void LightManager::Init(int maxLightNum, int lightTypeSize)
{
	m_iTypeSize = lightTypeSize;

	CD3DX12_HEAP_PROPERTIES heapUploadProperty(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = lightTypeSize*maxLightNum;
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapUploadProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_lightBuffer.GetAddressOf())));

	D3D12_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Buffer.FirstElement = 0;
	desc.Buffer.NumElements = maxLightNum;
	desc.Buffer.StructureByteStride = m_iTypeSize;
	desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	m_srvDesc = desc;
}

void LightManager::UpdateLightBuffer(const void* pData, int iTypeSize, int iNumber, bool  bDefault)
{
	m_iNum = iNumber;

	void* mapped = nullptr;
	m_lightBuffer->Map(0, nullptr, &mapped);
	memcpy(mapped, pData, iTypeSize*iNumber);
	m_lightBuffer->Unmap(0, nullptr);

	if (bDefault)
	{
		CopyToDefault();
	}

}

void LightManager::CopyToDefault()
{

	g_copyManager->Add({ m_lightBuffer });
}
