//--------------------------------------------------------------------------------------
// File: DeferredRender.h
//
// This class is utilized to run triangle-based rendering, and this method uses a two-pass deferred rendering pipeline.
// The first pass is to create G-buffer.
// The G-buffer consists of 4 textures: albedo, normal, specular + gloss and depth separately.
//
// The second pass is to accumulate lights with G-buffer and the light indexed buffer:
// The modified light accumulation is applied in this stage to optimize the lighting performance when light distribution is irregular.
//
// To do:
// 1. Support bump/gloss/specular maps.
// 2. Support MSAA.
//--------------------------------------------------------------------------------------
#include"DirectxHelper.h"
#include "ShaderManager.h"
#include "ScreenQuadRenderer.h"
#include "d3dx12.h"
#include "ShaderTypeDefine.h"
#include "MaterialManager.h"
#include "CameraCommon.h"
#include "LightClusteredManager.h"

class DeferredRender
{
public:

	void Init();
	void InitWindowSizeDependentResources();
	void InitTraditionalTileBased();

	void SetMaterials(MaterialManager& materialMgr, bool bUploadToDefault = true);
	void SetLightCullingMgr(LightClusteredManager& materialMgr);
	void SetLightBuffer(ID3D12Resource* const lightBuffer);
	

	const D3D12_GPU_VIRTUAL_ADDRESS GetViewCbGpuHandle() { return m_viewCb->GetGPUVirtualAddress(); } // I should use a camera manager to manage this constant buffer.
	ID3D12PipelineState* const  GetPso() { return m_depthPassPso.Get(); }
	const ID3D12RootSignature* const  GetSignaturet() { return m_rootSignature.Get(); }
	const D3D12_CPU_DESCRIPTOR_HANDLE* const  GetDsvHandle() { return &m_dsvHeap.hCPUHeapStart; }
	ID3D12Resource* const GetDepthResource() { return m_dsvTexture.Get(); }
	const D3D12_SHADER_RESOURCE_VIEW_DESC GetDepthSrvDesc() { return m_depthSrvDesc; }
	const CDescriptorHeapWrapper& GetHeap() { return m_cbvsrvHeap; }

	// Add commands to a list to clear G-buffer.
	void ClearGbuffer(ID3D12GraphicsCommandList* const  command);
	void SetGbuffer(ID3D12GraphicsCommandList* const  command);
	void SetDescriptorHeaps(ID3D12GraphicsCommandList * const  command);
	// Use resource barrier to convert G-buffers to readable resources.
	void RtvToSrv(ID3D12GraphicsCommandList* const  command);
	void SrvToRtv(ID3D12GraphicsCommandList * const  command);

	void ApplyDepthPassPso(ID3D12GraphicsCommandList* const  command, bool bSetPso = true);
	void ApplyCreateGbufferPso(ID3D12GraphicsCommandList* const  command, bool bSetPso = true);
	void ApplyLightAccumulationPso(ID3D12GraphicsCommandList* const  command, bool bSetPSO = true);
	void ApplyDebugPso(ID3D12GraphicsCommandList* const  command, bool bSetPSO = true);
	// Update a constant buffer for the camera.
	void UpdateConstantBuffer(const ViewData& camData); // To do: I should use a camera manager to manage this camera constant buffer.
	
	void ApplyTradLightAccumulation(ID3D12GraphicsCommandList* const  command, bool bSetPSO = true);
	void RenderQuad(ID3D12GraphicsCommandList* const  command);



private:
	void CreateTileBasedPSO();
	void CreateLightPassPsO();
	void CreateRootSignature();
	void CreateDepthPassPso();
	void CreateAdvancedPso();
	void CreateCameraCb();
	void CreateCameraCbView();
	void CreateDSV();
	void CreateRTV();
	void CreateSampler();
	void SetParametersLightPso(ID3D12GraphicsCommandList * const command);

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
	// [5][0] : SRV Range Count : unbounded
	// [5][0][0] : material buffer
	// [5][0][1~oo] : textures of materials
	// --------------------------------------
	// [6] : Descriptor Table for samplers
	// --------------------------------------
	// [6][0] : Sampler Range Count : 1
	// [6][0][0] : Sampler for sampling textures of materials (s0)
	// --------------------------------------
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_depthPassPso;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_lightAccumulationPso;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_lightListDebugPso;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_lightPso;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_traditionalAccumulationPso;

	ScreenQuadRenderer m_quadRenderer;

	D3D12_GPU_VIRTUAL_ADDRESS m_lightBufferGpuAdr;
	D3D12_GPU_VIRTUAL_ADDRESS m_lightIdxBufferGpuAdr;
	D3D12_GPU_VIRTUAL_ADDRESS m_lightCounterBufferGpuAdr;
	D3D12_GPU_VIRTUAL_ADDRESS m_lightIdxCbGpuAdr;


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
	// [5] : Descriptor Table for material data. Total Range Count: 1
	// --------------------------------------
	// [5][0] : CBV Range Count : unbounded
	// [5][0][0] : material buffer
	// [5][0][1~oo] : textures of materials
	// --------------------------------------
	CDescriptorHeapWrapper m_cbvsrvHeap;

	// A heap to store the depth (dsv), and this depth is used as a SRV in:
	// [1][0][3] : SRV for depth texture (t3)
	CDescriptorHeapWrapper m_dsvHeap;

	// A heap to store the G-buffer (rtvs) and the buffer is used as SRVs in:
	// [1][0][0] : SRV for albedo texture (t0)
	// [1][0][1] : SRV for normal texture (t1)
	// [1][0][2] : SRV for specular+gloss texture data (t2)
	CDescriptorHeapWrapper m_rtvHeap;

	CDescriptorHeapWrapper m_samplerHeap;

	float m_fClearColor[4] = { 0.2f,0.5f,0.7f,1.0f };
	float m_fClearDepth = 1.0f;

	const static int NumRTV = 3;
	const static int MaterialHeapOffset = 20;
	const static int ViewDataHeapOffset = 0;
	const static int GBufferHeapOffset = 1;
	const static int MaxDescriptorHeapSize = 64;
	const static int MaxSamplerDescriptorHeapSize = 1;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_rtvTextures[NumRTV];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_viewCb;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_dsvTexture;

	DXGI_FORMAT m_dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT m_dsvResourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
	DXGI_FORMAT m_rtvFormat[3] = { DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R8G8B8A8_SNORM,DXGI_FORMAT_R8G8B8A8_UNORM };

	D3D12_SHADER_RESOURCE_VIEW_DESC m_depthSrvDesc;
};