//--------------------------------------------------------------------------------------
// File: PreviewLight.h
//
// A class for rendering lights' positions.
//--------------------------------------------------------------------------------------
#include"DirectxHelper.h"
#include "ShaderManager.h"
#include "d3dx12.h"
#include "SphereRenderer.h"


class LightPreviewer
{
public:

	void Init();
	void const Render(ID3D12GraphicsCommandList* const  command, bool bSetPSO = true);

	// Set a light buffer, the number of light.
	void SetLightBuffer(ID3D12Resource* const  lightBuffer,const UINT uNumber);
	// Update camera's CB by the pointer or handle of a D3D12 resource.
	void UpdateCameraBuffer(ID3D12Resource* const  camCB);
	void UpdateCameraBuffer(const D3D12_GPU_VIRTUAL_ADDRESS camGpuHandle);
	
private:
	// A renderer to render spheres.
	SphereRenderer m_sphereRenderer;
	UINT m_uLightNum;

	// A geometry rendering pass.
	// Use SV_InstanceID to access light data from light buffer to draw all light sources.
	// Graphics pipeline name : Preview Light.
	// Shader pipeline : VS->PS.
	// Shader name : PreviewLightVS, PreviewLightPS.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

	// Total root parameter number : 2.
	// [0] : CBV for the camera data
	// [1] : SRV(Buffer) for the light data
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	D3D12_GPU_VIRTUAL_ADDRESS m_lightBufferGpuAdr;
	D3D12_GPU_VIRTUAL_ADDRESS m_camCbGpuAdr;

	DXGI_FORMAT m_DsvDxgiFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT m_RtvDxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	void CreateRootSignature();
	void CreatePso();
	
};