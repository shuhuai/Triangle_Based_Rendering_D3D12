//--------------------------------------------------------------------------------------
// File: LightClusteredManager.cpp
//--------------------------------------------------------------------------------------
#include "LightClusteredManager.h"
#include "d3dx12.h"
#include "ShaderManager.h"
#include <time.h>

using namespace Microsoft::WRL;

void LightClusteredManager::Init(int width, int height, int depth)
{

	m_bUseTriangle = UseTriLightCulling;
	m_uNumPixelPerTileX = width;
	m_uNumPixelPerTileY = height;
	m_uDepth = depth;

	// Create a heap class to store all resource views.
	m_viewsHeap.Create(g_d3dObjects->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 10, true);

	// Create rendering pipeline data.
	CreateRootSignature();
	CreatePSO();

	// Create GPU resources.
	CreateDepthPlaneResource();
	InitWindowSizeDependentResources();
}

void LightClusteredManager::RunLightCullingCS(ID3D12GraphicsCommandList * const command)
{
	ID3D12DescriptorHeap* ppHeaps[1] = { m_viewsHeap.pDH.Get() };
	command->SetDescriptorHeaps(1, ppHeaps);
	command->SetComputeRootSignature(m_rootSignature.Get());

	command->SetComputeRootDescriptorTable(0, m_viewsHeap.hGPU(1));
	command->SetComputeRootConstantBufferView(1, m_camCbGpuAdr);
	command->SetComputeRootConstantBufferView(2, m_clusteredCB->GetGPUVirtualAddress());

	command->SetPipelineState(m_lightCullPso.Get());

	command->Dispatch(m_uWidth, m_uHeight, m_uDepth);
}

void LightClusteredManager::InitWindowSizeDependentResources()
{
	// Calculate the number of tiles (or triangles) for light culling.
	m_uWidth = (UINT)((float)g_d3dObjects->GetScreenViewport().Width / (float)m_uNumPixelPerTileX + 0.5f);
	m_uHeight = (UINT)((float)g_d3dObjects->GetScreenViewport().Height / (float)m_uNumPixelPerTileY + 0.5f);
	m_uNumPixelPerTileX = g_d3dObjects->GetScreenViewport().Width *(1/ (float)m_uWidth);
	m_uNumPixelPerTileY = g_d3dObjects->GetScreenViewport().Height *(1 / (float)m_uHeight);

	// Create a plane mesh for the light accumulation stage.
	CreateTiledMesh();

	// Initialize light indexed buffer (UAV and SRV).
	CreateTiledResources();
	CreateUAV();

	// Create depth planes SRV.
	CreateSRV();

	// Create light culling data.
	CreateCB();
	UpdateCullingCB();
}

void LightClusteredManager::CreateCB()
{
	// Allocate a D3D12 resource to store light culling data.
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
	resourceDesc.Width = sizeof(ClusteredData);
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_clusteredCB.GetAddressOf())));
}

void LightClusteredManager::CreateCBV()
{
    // Create a CBV for light culling data.
	D3D12_CONSTANT_BUFFER_VIEW_DESC	descBuffer;
	descBuffer.BufferLocation = m_clusteredCB->GetGPUVirtualAddress();
	descBuffer.SizeInBytes = (sizeof(ClusteredData) + 255) & ~255;
	g_d3dObjects->GetD3DDevice()->CreateConstantBufferView(&descBuffer, m_viewsHeap.hCPU(0));
}

void LightClusteredManager::CreateTiledResources()
{
	CD3DX12_HEAP_PROPERTIES heapDefaultProperty(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Width = sizeof(ClusteredBuffer)*m_uWidth*m_uHeight*m_uDepth;
	// Allocate double size memory to store per triangle data.
	if (m_bUseTriangle)
	{
		resourceDesc.Width = resourceDesc.Width * 2;
	}
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapDefaultProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(m_clusteredBuffer.GetAddressOf())));

}


void LightClusteredManager::CreateSRV()
{
	// Create a SRV for depth planes.
	D3D12_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Buffer.FirstElement = 0;
	desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Buffer.NumElements = m_uDepth + 1;
	desc.Buffer.StructureByteStride = sizeof(float);

	g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(m_depthPlanesBuffer.Get(), &desc, m_viewsHeap.hCPU(3));
}

void LightClusteredManager::CreateUAV()
{
	// Create an UAV for light indexes.
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Buffer.FirstElement = 0;
	desc.Buffer.NumElements = m_uWidth*m_uHeight*m_uDepth;;
	// If we use per triangle culling, we need double size buffer to store indexes.
	if (m_bUseTriangle)
	{
		desc.Buffer.NumElements = desc.Buffer.NumElements * 2;
	}
	desc.Buffer.StructureByteStride = sizeof(ClusteredBuffer);
	desc.Buffer.CounterOffsetInBytes = 0;
	desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Format = DXGI_FORMAT_UNKNOWN;

	g_d3dObjects->GetD3DDevice()->CreateUnorderedAccessView(m_clusteredBuffer.Get(), nullptr, &desc, m_viewsHeap.hCPU(1));
}

void LightClusteredManager::UpdateCullingCB()
{
	ClusteredData clusteredData;
	clusteredData.widthDim = m_uWidth;
	clusteredData.heightDim = m_uHeight;
	clusteredData.depthDim = m_uDepth;
	clusteredData.lightNum = m_iLightNum;
	clusteredData.tileSizeX = m_uNumPixelPerTileX;
	clusteredData.tileSizeY = m_uNumPixelPerTileY;

	void* mapped = nullptr;
	m_clusteredCB->Map(0, nullptr, &mapped);
	memcpy(mapped, &clusteredData, sizeof(ClusteredData));
	m_clusteredCB->Unmap(0, nullptr);
}

void LightClusteredManager::UpdateDepthPlanes(const float * planes)
{
	void* mapped = nullptr;
	m_depthPlanesBuffer->Map(0, nullptr, &mapped);
	memcpy(mapped, planes, sizeof(float)*(m_uDepth + 1));
	m_depthPlanesBuffer->Unmap(0, nullptr);
}

void LightClusteredManager::SetLightBuffer(ID3D12Resource* const lightBuffer, const D3D12_SHADER_RESOURCE_VIEW_DESC& SrvDesc, int lightNum)
{
	m_iLightNum = lightNum;
	g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(lightBuffer, &SrvDesc, m_viewsHeap.hCPU(2));
	// Update light culling CB, after the number of light is different.
	UpdateCullingCB();
}

void LightClusteredManager::SetDepthBuffer(ID3D12Resource * const depthBuffer, const D3D12_SHADER_RESOURCE_VIEW_DESC& SrvDesc)
{

	g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(depthBuffer, &SrvDesc, m_viewsHeap.hCPU(4));
	
}

void LightClusteredManager::CreateTiledMesh()
{                      
	m_quadRenderer.InitTiles((m_uWidth), (m_uHeight));
}

void LightClusteredManager::CreatePSO()
{
	// Select a light culling method, per tile or per triangle.

	// Per tile culling.
	const ShaderObject* cs = g_ShaderManager.GetShaderObj("PerTileCullingCS");
	if (m_bUseTriangle)
	{
		// Per triangle culling.
		cs = g_ShaderManager.GetShaderObj("PerTriangleCullingCS");
	}

	// A compute shader for light culling, culling lights per triangle or per tile.
	// Compute pipeline name :  light culling.
	// Shader name : PerTileCullingCS, PerTriangleCullingCS.
	D3D12_COMPUTE_PIPELINE_STATE_DESC  descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	descPipelineState.CS = { cs->binaryPtr,cs->size };;
	descPipelineState.pRootSignature = m_rootSignature.Get();

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateComputePipelineState(&descPipelineState, IID_PPV_ARGS(&m_lightCullPso)));
}

void LightClusteredManager::CreateRootSignature()
{

	// Total Root Parameter Count: 2.
	// [0] : Descriptor Table Range Count: 3
	// --------------------------------------
	// [0][0] : CBV Range Count : 1
	// [0][0][0] : CBV for culling data (b0)
	// [0][1] : UAV Range Count : 1
	// [0][1][0]: UAV for saving light indexed for every triangle(or tile) (u0)
	// [0][2] : SRV Range Count : 3
	// [0][2][0] : SRV for light buffer (t0)
	// [0][2][1] : SRV for depth planes (t1)
	// [0][2][2] : SRV for depth texture (t2)
	// --------------------------------------
	// [1] : CBV for the camera data (b1)
	CD3DX12_DESCRIPTOR_RANGE range[2];
	CD3DX12_ROOT_PARAMETER parameter[3];
	range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
	parameter[0].InitAsDescriptorTable(_countof(range), range, D3D12_SHADER_VISIBILITY_ALL);
	parameter[1].InitAsConstantBufferView(1);
	parameter[2].InitAsConstantBufferView(0);

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.Init(3, parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ComPtr<ID3DBlob> rootSigBlob, errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}

void LightClusteredManager::CreateDepthPlaneResource()
{
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
	resourceDesc.Width = sizeof(float)*(m_uDepth + 1);
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapUploadProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_depthPlanesBuffer.GetAddressOf())));
}