//--------------------------------------------------------------------------------------
// File: FbxRender.h
//
// A class for loading a FBX model and rendering it.
//--------------------------------------------------------------------------------------

#pragma once
#include "FbxLoader.h"
#include "ShaderTypeDefine.h"
#include "VertexStructures.h"
#include "MaterialStructures.h"
#include "MaterialManager.h"
#include "d3dx12.h"
#include "DirectXMathConverter.h"

class FbxRender
{
public:
	// Loading the FBX model to memory.
	void LoadModel(const std::string name, float fScale = 1);

	void Render(ID3D12GraphicsCommandList* const commandList);

	// After loading the model, it copies materials data to GPU.
	void CreateMaterials(MaterialManager& materialMgr);
	// After loading the model, it copies vertex data to GPU.
	void CreateGpuResources(MaterialManager& materialMgr);

	const DirectX::XMFLOAT3 GetMaxAxis() const { return m_maxAxis; }
	const DirectX::XMFLOAT3 GetMinAxis() const { return m_minAxis; }
	DirectX::XMFLOAT3 GetCenter();
private:
	// Create a vertex buffer and a vertex buffer view, and T is the vertex structure we use.
	template<class T> void CreateResource(void* VB, int num);

	FbxLoader m_fbxLoader;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vbView;

	std::vector<FullVertex> m_vertexData;

	DirectX::XMFLOAT3 m_minAxis;
	DirectX::XMFLOAT3 m_maxAxis;
	float m_fScale;
	UINT m_uVertexNumber;
};