//--------------------------------------------------------------------------------------
// File: DeferredRender.cpp
//--------------------------------------------------------------------------------------
#include "DeferredRender.h"
#include "VertexStructures.h"


void DeferredRender::SetMaterials(MaterialManager & materialMgr, bool bUploadToDefault)
{
	// Create a SRV for a material buffer.
	// The materials are put in "m_cbvsrvHeap" with the offset "MaxMaterialHeapOffset".
	D3D12_SHADER_RESOURCE_VIEW_DESC descMatSRV;
	ZeroMemory(&descMatSRV, sizeof(descMatSRV));
	descMatSRV.Buffer.FirstElement = 0;
	descMatSRV.Buffer.NumElements = materialMgr.GetMaterialNum();
	descMatSRV.Buffer.StructureByteStride = materialMgr.GetMaterialSize();
	descMatSRV.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	descMatSRV.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	descMatSRV.Format = DXGI_FORMAT_UNKNOWN;
	descMatSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(materialMgr.GetMaterial().Get(), &descMatSRV, m_cbvsrvHeap.hCPU(MaterialHeapOffset));

	// Add the material buffer to copy manager to prepare to be copied to default heap.
	if (bUploadToDefault)
	{
		UploadToDefaultObject copyObj(materialMgr.GetMaterial(), m_cbvsrvHeap.hCPU(MaterialHeapOffset), descMatSRV);
		g_copyManager->Add(copyObj);
	}

	// Insert all textures in the material manager to the heap.
	// Create a SRV for each texture, and increase offset.
	for (unsigned int uMatIdx = 0; uMatIdx < materialMgr.GetTextureResource().size(); uMatIdx++)
	{
		if (materialMgr.GetTextureResource()[uMatIdx].resource)
		{
			auto resourceDesc = materialMgr.GetTextureResource()[uMatIdx].resource->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(descSRV));
			descSRV.Texture2D.MipLevels = resourceDesc.MipLevels;
			descSRV.Texture2D.MostDetailedMip = 0;
			descSRV.Format = resourceDesc.Format;
			descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(materialMgr.GetTextureResource()[uMatIdx].resource.Get(), &descSRV, m_cbvsrvHeap.hCPU(uMatIdx + MaterialHeapOffset + 1));

			if (bUploadToDefault)
			{
				UploadToDefaultObject copyObj(materialMgr.GetTextureResource()[uMatIdx].resource, m_cbvsrvHeap.hCPU(uMatIdx + MaterialHeapOffset + 1), descSRV);
				g_copyManager->Add(copyObj);
			}
		}
	}
}

void DeferredRender::SetLightCullingMgr(LightClusteredManager & mgr)
{
	m_lightIdxCbGpuAdr = mgr.GetClusteredCB()->GetGPUVirtualAddress();
	m_lightIdxBufferGpuAdr = mgr.GetClusteredBuffer()->GetGPUVirtualAddress();
}

void DeferredRender::Init()
{
	m_cbvsrvHeap.Create(g_d3dObjects->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxDescriptorHeapSize, true);
	m_samplerHeap.Create(g_d3dObjects->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MaxSamplerDescriptorHeapSize, true);
	// Create deferred buffers.
	// 1. Albedo.
	// 2. Normal.
	// 3. Specular + Gloss.
	m_rtvHeap.Create(g_d3dObjects->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3);
	// 4. Depth.
	m_dsvHeap.Create(g_d3dObjects->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);


	// Create constant buffer for camera data.
	CreateCameraCb();

	// In shaders, static sampling is not working while using a dynamic index to load textures,
	// but using a sampler in the heap is Okay, so I create one sampler.
	CreateSampler();

	CreateRootSignature();

	CreateDepthPassPso();
	CreateLightPassPsO();
	CreateAdvancedPso();
	// Create resources depending on windows size.
	InitWindowSizeDependentResources();
}

void DeferredRender::InitWindowSizeDependentResources()
{
	// Create render targets for G-buffers.
	CreateRTV();
	// Create depth buffer.
	CreateDSV();
}

void DeferredRender::CreateDepthPassPso()
{
	// A PSO for rendering depth only.
	// Graphics pipeline name : Basic depth creation.
	// Shader pipeline : VS.
	// Shader name : BasicVS.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	const ShaderObject* vs = g_ShaderManager.GetShaderObj("BasicVS");
	descPipelineState.VS = { vs->binaryPtr,vs->size };
	descPipelineState.InputLayout.pInputElementDescs = DescFullVertex;
	descPipelineState.InputLayout.NumElements = _countof(DescFullVertex);
	descPipelineState.pRootSignature = m_rootSignature.Get();
	descPipelineState.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	descPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPipelineState.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPipelineState.SampleMask = UINT_MAX;
	descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPipelineState.NumRenderTargets = 0;
	descPipelineState.DSVFormat = m_dsvFormat;
	descPipelineState.SampleDesc.Count = 1;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(&m_depthPassPso)));
}

void DeferredRender::CreateAdvancedPso()
{
	// G-buffers creation, the input layout includes position, normal, texcoord and material index.
	// Graphics pipeline name : Advanced G-buffers creation.
	// Shader pipeline : VS->PS.
	// Shader name : AdvancedShadingVS, AdvancedShadingPS.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	const ShaderObject* vs = g_ShaderManager.GetShaderObj("AdvancedShadingVS");
	const ShaderObject* ps = g_ShaderManager.GetShaderObj("AdvancedShadingPS");
	descPipelineState.VS = { vs->binaryPtr,vs->size };
	descPipelineState.PS = { ps->binaryPtr,ps->size };
	descPipelineState.InputLayout.pInputElementDescs = DescFullVertex;
	descPipelineState.InputLayout.NumElements = _countof(DescFullVertex);
	descPipelineState.pRootSignature = m_rootSignature.Get();
	descPipelineState.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	descPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPipelineState.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPipelineState.SampleMask = UINT_MAX;
	descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPipelineState.NumRenderTargets = NumRTV;
	descPipelineState.RTVFormats[0] = m_rtvFormat[0];
	descPipelineState.RTVFormats[1] = m_rtvFormat[1];
	descPipelineState.RTVFormats[2] = m_rtvFormat[2];
	descPipelineState.DSVFormat = m_dsvFormat;
	descPipelineState.SampleDesc.Count = 1;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(&m_lightAccumulationPso)));
}

void DeferredRender::CreateCameraCb()
{
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
	resourceDesc.Width = sizeof(ViewData);
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_viewCb.GetAddressOf())));
}

void DeferredRender::CreateCameraCbView()
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC	descBuffer;
	descBuffer.BufferLocation = m_viewCb->GetGPUVirtualAddress();
	descBuffer.SizeInBytes = (sizeof(ViewData) + 255) & ~255;

}

void DeferredRender::CreateDSV()
{

	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = m_dsvFormat;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = (UINT)g_d3dObjects->GetScreenViewport().Width;
	resourceDesc.Height = (UINT)g_d3dObjects->GetScreenViewport().Height;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Set DSV initial values.
	D3D12_CLEAR_VALUE clearVal;
	clearVal = { m_dsvFormat , m_fClearDepth };

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(m_dsvTexture.GetAddressOf())));

	D3D12_DEPTH_STENCIL_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Texture2D.MipSlice = 0;
	desc.Format = resourceDesc.Format;
	desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_DSV_FLAG_NONE;
	g_d3dObjects->GetD3DDevice()->CreateDepthStencilView(m_dsvTexture.Get(), &desc, m_dsvHeap.hCPU(0));

	// Create a SRV for the depth texture, because this depth texture is also one texture of G-buffers.
	D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descSRV, sizeof(descSRV));
	descSRV.Texture2D.MipLevels = resourceDesc.MipLevels;
	descSRV.Texture2D.MostDetailedMip = 0;
	descSRV.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(m_dsvTexture.Get(), &descSRV, m_cbvsrvHeap.hCPU(GBufferHeapOffset + NumRTV));

	m_depthSrvDesc= descSRV;
}

void DeferredRender::CreateRTV()
{

	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = (UINT)g_d3dObjects->GetScreenViewport().Width;
	resourceDesc.Height = (UINT)g_d3dObjects->GetScreenViewport().Height;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearVal;
	clearVal.Color[0] = m_fClearColor[0];
	clearVal.Color[1] = m_fClearColor[1];
	clearVal.Color[2] = m_fClearColor[2];
	clearVal.Color[3] = m_fClearColor[3];

	for (int i = 0; i < NumRTV; i++) {
		resourceDesc.Format = m_rtvFormat[i];
		clearVal.Format = m_rtvFormat[i];
		ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(m_rtvTextures[i].GetAddressOf())));
	}

	D3D12_RENDER_TARGET_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Texture2D.MipSlice = 0;
	desc.Texture2D.PlaneSlice = 0;
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < NumRTV; i++) {
		desc.Format = m_rtvFormat[i];
		g_d3dObjects->GetD3DDevice()->CreateRenderTargetView(m_rtvTextures[i].Get(), &desc, m_rtvHeap.hCPU(i));
	}


	// Create SRVs for RTs.
	D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descSRV, sizeof(descSRV));
	descSRV.Texture2D.MipLevels = resourceDesc.MipLevels;
	descSRV.Texture2D.MostDetailedMip = 0;
	descSRV.Format = resourceDesc.Format;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	for (int i = 0; i < NumRTV; i++) {
		descSRV.Format = m_rtvFormat[i];
		g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(m_rtvTextures[i].Get(), &descSRV, m_cbvsrvHeap.hCPU(i + GBufferHeapOffset));
	}


}

void DeferredRender::CreateSampler()
{
	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	g_d3dObjects->GetD3DDevice()->CreateSampler(&samplerDesc, m_samplerHeap.hCPU(0));
}



void DeferredRender::ClearGbuffer(ID3D12GraphicsCommandList * const  command)
{
	// Clear all render targets.
	for (int i = 0; i < NumRTV; i++)
		command->ClearRenderTargetView(m_rtvHeap.hCPU(i), m_fClearColor, 0, nullptr);
	// Clear depth texture.
	command->ClearDepthStencilView(m_dsvHeap.hCPUHeapStart, D3D12_CLEAR_FLAG_DEPTH, m_fClearDepth, 0xff, 0, nullptr);
	
}

void DeferredRender::SetGbuffer(ID3D12GraphicsCommandList * const command)
{
	// Set render targets and depth texture.
	command->OMSetRenderTargets(NumRTV, &m_rtvHeap.hCPUHeapStart, true, &m_dsvHeap.hCPUHeapStart);
}

void DeferredRender::SetDescriptorHeaps(ID3D12GraphicsCommandList * const  command)
{
	// Set heaps.
	ID3D12DescriptorHeap* ppHeaps[2] = { m_cbvsrvHeap.pDH.Get(),m_samplerHeap.pDH.Get() };
	command->SetDescriptorHeaps(2, ppHeaps);
}

void DeferredRender::RtvToSrv(ID3D12GraphicsCommandList * const  command)
{
	std::vector<ID3D12Resource*> rtvVector;
	for (int i = 0; i < NumRTV; i++) rtvVector.push_back(m_rtvTextures[i].Get());
	// Convert all resources of G-buffers to read-only resources.
	
	AddResourceBarrier(command, rtvVector, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	AddResourceBarrier(command, m_dsvTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ);

}

void DeferredRender::SrvToRtv(ID3D12GraphicsCommandList * const  command)
{
	std::vector<ID3D12Resource*> rtvVector;
	for (int i = 0; i < NumRTV; i++) rtvVector.push_back(m_rtvTextures[i].Get());
	// Convert all resources of G-buffers to write-only resources.

	AddResourceBarrier(command, rtvVector, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	AddResourceBarrier(command, m_dsvTexture.Get(),  D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

}

void DeferredRender::ApplyDepthPassPso(ID3D12GraphicsCommandList * const command, bool bSetPSO)
{
	if (bSetPSO)
	{
		command->SetPipelineState(m_depthPassPso.Get());
	}

	ID3D12DescriptorHeap* ppHeaps[1] = { m_cbvsrvHeap.pDH.Get()};
	command->SetDescriptorHeaps(1, ppHeaps);

	command->SetGraphicsRootSignature(m_rootSignature.Get());
	command->SetGraphicsRootConstantBufferView(0, m_viewCb->GetGPUVirtualAddress());

}

void DeferredRender::ApplyCreateGbufferPso(ID3D12GraphicsCommandList * const command, bool bSetPSO)
{
	if (bSetPSO)
	{
		command->SetPipelineState(m_lightAccumulationPso.Get());
	}

	ID3D12DescriptorHeap* ppHeaps[2] = { m_cbvsrvHeap.pDH.Get(),m_samplerHeap.pDH.Get() };
	command->SetDescriptorHeaps(2, ppHeaps);

	command->SetGraphicsRootSignature(m_rootSignature.Get());
	command->SetGraphicsRootConstantBufferView(0,m_viewCb->GetGPUVirtualAddress());
	command->SetGraphicsRootDescriptorTable(1, m_cbvsrvHeap.hGPU(GBufferHeapOffset));
	command->SetGraphicsRootDescriptorTable(5, m_cbvsrvHeap.hGPU(MaterialHeapOffset));

	// Set samplers.
	command->SetGraphicsRootDescriptorTable(6, m_samplerHeap.hGPU(0));
}

void DeferredRender::ApplyLightAccumulationPso(ID3D12GraphicsCommandList * const command, bool bSetPSO)
{
	if (bSetPSO)
	{
		command->SetPipelineState(m_lightPso.Get());
	}
	// The techniques should already set DescriptorHeaps(m_cbvsrvHeap and m_samplerHeap), and
	// set a root signature in the G-buffer creation stage.
	//command->SetGraphicsRootSignature(m_rootSignature.Get());
	command->SetGraphicsRootShaderResourceView(2, m_lightIdxBufferGpuAdr);
	command->SetGraphicsRootShaderResourceView(3, m_lightBufferGpuAdr);
	command->SetGraphicsRootConstantBufferView(4, m_lightIdxCbGpuAdr);
}

void DeferredRender::ApplyDebugPso(ID3D12GraphicsCommandList * const command, bool bSetPSO)
{

	if (bSetPSO)
	{
		command->SetPipelineState(m_lightListDebugPso.Get());
	}
	// The techniques should already set DescriptorHeaps(m_cbvsrvHeap and m_samplerHeap), and
	// set a root signature in the G-buffer creation stage.
	//command->SetGraphicsRootSignature(m_rootSignature.Get());
	command->SetGraphicsRootShaderResourceView(2, m_lightIdxBufferGpuAdr);
	command->SetGraphicsRootShaderResourceView(3, m_lightBufferGpuAdr);
	command->SetGraphicsRootConstantBufferView(4, m_lightIdxCbGpuAdr);
}

void DeferredRender::SetLightBuffer(ID3D12Resource* const lightBuffer)
{
	m_lightBufferGpuAdr = lightBuffer->GetGPUVirtualAddress();
}

void DeferredRender::UpdateConstantBuffer(const ViewData& camData)
{
	void* mapped = nullptr;
	m_viewCb->Map(0, nullptr, &mapped);
	memcpy(mapped, &camData, sizeof(ViewData));
	m_viewCb->Unmap(0, nullptr);
}

void DeferredRender::ApplyTradLightAccumulation(ID3D12GraphicsCommandList * const command, bool bSetPSO)
{
	if (bSetPSO)
	{
		command->SetPipelineState(m_traditionalAccumulationPso.Get());
	}

	command->SetGraphicsRootShaderResourceView(2, m_lightIdxBufferGpuAdr);
	command->SetGraphicsRootShaderResourceView(3, m_lightBufferGpuAdr);
	command->SetGraphicsRootConstantBufferView(4, m_lightIdxCbGpuAdr);

}

void DeferredRender::RenderQuad(ID3D12GraphicsCommandList * const command)
{
	m_quadRenderer.Render(command);
}

void DeferredRender::InitTraditionalTileBased()
{
	CreateTileBasedPSO();

	// Initialize a full-screen quad renderer.
	m_quadRenderer.Init();
}

void DeferredRender::CreateTileBasedPSO()
{
	// A light accumulation pass. Use a pixel shader to load light culling information.
	// Graphics pipeline name : Tile-based light accumulation.
	// Shader pipeline : VS->PS.
	// Shader name : ScreenQuadVS, TiledLightPassPS.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	const ShaderObject* vs = g_ShaderManager.GetShaderObj("ScreenQuadVS");
	const ShaderObject* ps = g_ShaderManager.GetShaderObj("TiledLightPassPS");
	descPipelineState.VS = { vs->binaryPtr,vs->size };
	descPipelineState.PS = { ps->binaryPtr,ps->size };
	descPipelineState.InputLayout.pInputElementDescs = DescScreenQuadVertex;
	descPipelineState.InputLayout.NumElements = _countof(DescScreenQuadVertex);
	descPipelineState.pRootSignature = m_rootSignature.Get();
	descPipelineState.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	descPipelineState.DepthStencilState.DepthEnable = false;
	descPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPipelineState.BlendState.RenderTarget[0].BlendEnable = false;
	descPipelineState.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPipelineState.RasterizerState.DepthClipEnable = false;
	descPipelineState.SampleMask = UINT_MAX;
	descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPipelineState.NumRenderTargets = 1;
	descPipelineState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPipelineState.SampleDesc.Count = 1;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(&m_traditionalAccumulationPso)));
}

void DeferredRender::CreateLightPassPsO()
{
	// A light accumulation pass. In order to avoid using a texture-dependent loop in a pixel shader,
	// use a geometry shader to load light culling information instead.
	// Graphics pipeline name : Triangle-based light accumulation.
	// Shader pipeline : VS->GS->PS.
	// Shader name : ScreenQuadVS, LightPassTriangleGS, LightPassPS.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	const ShaderObject* vs = g_ShaderManager.GetShaderObj("ScreenQuadVS");
	const ShaderObject* gs = g_ShaderManager.GetShaderObj("LightPassQuadGS");
	if (UseTriLightCulling)
	{
		 gs = g_ShaderManager.GetShaderObj("LightPassTriangleGS");
	}
	const ShaderObject* ps = g_ShaderManager.GetShaderObj("LightPassPS");
	descPipelineState.VS = { vs->binaryPtr,vs->size };
	descPipelineState.GS = { gs->binaryPtr,gs->size };
	descPipelineState.PS = { ps->binaryPtr,ps->size };
	descPipelineState.InputLayout.pInputElementDescs = DescScreenQuadVertex;
	descPipelineState.InputLayout.NumElements = _countof(DescScreenQuadVertex);
	descPipelineState.pRootSignature = m_rootSignature.Get();
	descPipelineState.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	descPipelineState.DepthStencilState.DepthEnable = false;
	descPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPipelineState.BlendState.RenderTarget[0].BlendEnable = true;
	descPipelineState.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	descPipelineState.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	descPipelineState.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPipelineState.RasterizerState.DepthClipEnable = false;
	descPipelineState.SampleMask = UINT_MAX;
	descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPipelineState.NumRenderTargets = 1;
	descPipelineState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPipelineState.SampleDesc.Count = 1;


	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(&m_lightPso)));


	// A debug pipeline to show per triangle or per tile information on screen.
	// It loads the total light number in the pixel shader, and this shader outputs different colors depending on the light number.
	// Graphics pipeline name : Debug light number.
	// Shader pipeline : VS->GS->PS.
	// Shader name : ScreenQuadVS, LightPassTriangleGS, DebugLightPS.
	ps = g_ShaderManager.GetShaderObj("DebugLightPS");
	descPipelineState.PS = { ps->binaryPtr,ps->size };
	descPipelineState.BlendState.RenderTarget[0].BlendEnable = false;
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(&m_lightListDebugPso)));

}

void DeferredRender::CreateRootSignature()
{
	// Total Root Parameter Count: 7.
	// [0] : CBV for the camera data (b0)
	// --------------------------------------
	// [1] : Descriptor Table for G-buffer. Total Range Count: 1
	// --------------------------------------
	// [1][0] : SRV Range Count : 4
	// [1][0][0] : SRV for albedo texture (t0)
	// [1][0][1] : SRV for normal texture (t1)
	// [1][0][2] : SRV for specular+gloss texture data (t2)
	// [1][0][3] : SRV for depth texture (t3)
	// --------------------------------------
	// [2] : SRV for light indexed buffer (t5)
	// [3] : SRV for light buffer (t6)
	// [4] : CBV for culling data (b2)
	// [5] : Descriptor Table for material data. Total Range Count: 1
	// --------------------------------------
	// [5][0] : CBV Range Count : unbounded
	// [5][0][0] : material buffer
	// [5][0][1~oo] : textures of materials
	// --------------------------------------
	// [6] : Descriptor Table for samplers
	// --------------------------------------
	// [6][0] : Sampler Range Count : 1
	// [6][0][0] : Sampler for sampling textures of materials (s0)
	// --------------------------------------
	CD3DX12_ROOT_PARAMETER rootParameters[7];
	CD3DX12_DESCRIPTOR_RANGE range[4];
	// Camera data CBV.
	rootParameters[0].InitAsConstantBufferView(0);

	// Pack 4 textures as the G-buffer.
	range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
	rootParameters[1].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);

	// Light indexed buffer.
	rootParameters[2].InitAsShaderResourceView(5);
	// Light data.
	rootParameters[3].InitAsShaderResourceView(6);
	// Light culling data.
	rootParameters[4].InitAsConstantBufferView(2,0, D3D12_SHADER_VISIBILITY_PIXEL);

	// All materials' SRVs for G-buffer creation.
	range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0, 1);
	rootParameters[5].InitAsDescriptorTable(1, &range[2], D3D12_SHADER_VISIBILITY_PIXEL);

	// Sampler.
	range[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
	rootParameters[6].InitAsDescriptorTable(1, &range[3], D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.Init(7, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> rootSigBlob, errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}
