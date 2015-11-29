//--------------------------------------------------------------------------------------
// File: LightClusteredManager.h
//
// The class runs a compute shader for light culling to create light indexed buffer.
// It finds the intersections between lights and flat triangles (or tiles).
// According to width, height and depth, the class allocates a buffer to store light indexes.
// To do:
// 1. Support for clustered rendering (Now only support depth=1 for the modified light accumulation stage).
// 2. Good distribution of depth planes (exponential distribution).
//--------------------------------------------------------------------------------------

#pragma once
#include "DirectxHelper.h"
#include <vector>
#include "ScreenQuadRenderer.h"
#include "ShaderTypeDefine.h"
#include "ClusteredCommon.h"
#include "CameraCommon.h"

class LightClusteredManager
{
public:

	void Init(int width, int height, int depth);
	void InitWindowSizeDependentResources();

	void RunLightCullingCS(ID3D12GraphicsCommandList* const  command);
	// Get light indexed buffer.
	ID3D12Resource* const   GetClusteredBuffer() const { return m_clusteredBuffer.Get(); }
	// Get light culling CB.
	ID3D12Resource* const   GetClusteredCB() const { return m_clusteredCB.Get(); }
	UINT GetAxisXNumber() { return m_uWidth; }
	UINT GetAxisYNumber() { return m_uHeight; }


	void UpdateCullingCB();
	void UpdateDepthPlanes(const float* const  planes);
	void SetCameraCB(const D3D12_GPU_VIRTUAL_ADDRESS address) { m_camCbGpuAdr = address; }
	// Set the light buffer which we are going to cull.
	void SetLightBuffer(ID3D12Resource* const  lightBuffer, const D3D12_SHADER_RESOURCE_VIEW_DESC& SrvDesc, int lightNum);

	void SetDepthBuffer(ID3D12Resource* const depthBuffer, const D3D12_SHADER_RESOURCE_VIEW_DESC& SrvDesc);

	// Get a renderer for the screen-size geometry (m_uWidth * m_uHeight*2).
	ScreenQuadRenderer& GetQuadRenderer() { return m_quadRenderer; }

private:
	// Create constant buffer for culling.
	void CreateCB();
	// Create constant buffer view for culling.
	void CreateCBV();
	void CreateTiledResources();
	void CreateSRV();
	void CreateUAV();
	void CreatePSO();
	void CreateRootSignature();
	void CreateDepthPlaneResource();
	void CreateTiledMesh();

	// Enable to use triangle-based culling.
	bool m_bUseTriangle;
	// Light indexed buffer.
	Microsoft::WRL::ComPtr<ID3D12Resource> m_clusteredBuffer;
	// Light culling CB.
	Microsoft::WRL::ComPtr<ID3D12Resource> m_clusteredCB;
	// Depth value for every depth plane (the total number : depth+1).
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthPlanesBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS m_camCbGpuAdr;

	D3D12_GPU_VIRTUAL_ADDRESS m_depthSrvGpuAdr;

	// A compute shader for light culling, culling lights per triangle or per tile.
	// Compute pipeline name :  light culling.
	// Shader name : PerTileCullingCS, PerTriangleCullingCS.
	Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_lightCullPso;

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
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	ScreenQuadRenderer m_quadRenderer;

	// Descriptor Table Range Count : 3.
	// --------------------------------------
	// [0][0] : CBV Range Count : 1
	// [0][0][0] : CBV for culling data (b0)
	// [0][1] : UAV Range Count : 1
	// [0][1][0]: UAV for saving light indexed for every triangle(or tile) (u0)
	// [0][2] : SRV Range Count : 3
	// [0][2][0] : SRV for light buffer (t0)
	// [0][2][1] : SRV for depth planes (t1)
	// [0][2][2] : SRV for depth texture (t2)
	CDescriptorHeapWrapper m_viewsHeap;

	UINT m_uWidth;
	UINT m_uHeight;
	UINT m_uDepth;
	int m_iLightNum;
	float m_uNumPixelPerTileX;
	float m_uNumPixelPerTileY;
};