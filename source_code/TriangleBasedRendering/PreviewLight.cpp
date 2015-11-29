//--------------------------------------------------------------------------------------
// File: PreviewLight.cpp
//--------------------------------------------------------------------------------------
#include"PreviewLight.h"
#include "VertexStructures.h"

using namespace Microsoft::WRL;

void LightPreviewer::Init()
{
	m_sphereRenderer.Init(1, 20, 20);
	// Create GPU resources for rendering spheres.
	CreateRootSignature();
	CreatePso();
}

void LightPreviewer::SetLightBuffer(ID3D12Resource* const lightBuffer,const UINT number)
{
	m_lightBufferGpuAdr = lightBuffer->GetGPUVirtualAddress();
	m_uLightNum = number;
}

void const LightPreviewer::Render(ID3D12GraphicsCommandList * const command, bool bSetPSO)
{
	if (bSetPSO)
	{
		command->SetPipelineState(m_pipelineState.Get());
	}
	command->SetGraphicsRootSignature(m_rootSignature.Get());
	command->SetGraphicsRootShaderResourceView(0, m_lightBufferGpuAdr);
	command->SetGraphicsRootConstantBufferView(1, m_camCbGpuAdr);

	m_sphereRenderer.Render(command, m_uLightNum);
}

void LightPreviewer::UpdateCameraBuffer(ID3D12Resource* const camCB)
{
	m_camCbGpuAdr = camCB->GetGPUVirtualAddress();
}


void LightPreviewer::UpdateCameraBuffer(const D3D12_GPU_VIRTUAL_ADDRESS camGpuHandle)
{
	m_camCbGpuAdr = camGpuHandle;
}


void LightPreviewer::CreateRootSignature()
{
	// Total root parameter number : 2.
	// [0] : CBV for the camera data (b0)
	// [1] : SRV(Buffer) for the light data (t0)
	CD3DX12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].InitAsShaderResourceView(0);
	rootParameters[1].InitAsConstantBufferView(0);

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.Init(2, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSigBlob, errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.GetAddressOf())));

}

void LightPreviewer::CreatePso()
{
	// A geometry rendering pass.
	// Use SV_InstanceID to access light data from light buffer to draw all light sources.
	// Graphics pipeline name : Preview Light.
	// Shader pipeline : VS->PS.
	// Shader name : PreviewLightVS, PreviewLightPS.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
	ZeroMemory(&descPipelineState, sizeof(descPipelineState));
	const ShaderObject* vs = g_ShaderManager.GetShaderObj("PreviewLightVS");
	const ShaderObject* ps = g_ShaderManager.GetShaderObj("PreviewLightPS");
	descPipelineState.VS = { vs->binaryPtr,vs->size };
	descPipelineState.PS = { ps->binaryPtr,ps->size };
	descPipelineState.InputLayout.pInputElementDescs = DescNormalVertex;
	descPipelineState.InputLayout.NumElements = _countof(DescNormalVertex);
	descPipelineState.pRootSignature = m_rootSignature.Get();
	descPipelineState.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	descPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPipelineState.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPipelineState.SampleMask = UINT_MAX;
	descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPipelineState.NumRenderTargets = 1;
	descPipelineState.RTVFormats[0] = m_RtvDxgiFormat;
	descPipelineState.DSVFormat = m_DsvDxgiFormat;
	descPipelineState.SampleDesc.Count = 1;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(&m_pipelineState)));
}

