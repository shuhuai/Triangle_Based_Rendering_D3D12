//--------------------------------------------------------------------------------------
// File: MaterialManager.h
//
// A class for managing material data, it stores the texture resources and a material buffer.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include <vector>
#include <queue>
#include "CDescriptorHeapWrapper.h"
#include "ShaderTypeDefine.h"

#pragma once
// Material properties.
struct MaterialResource
{
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	std::string name;
};

class MaterialManager
{
public:
	MaterialManager();
	~MaterialManager();

	// Send a material resource to this manager, and then this manager will take the responsibility to manage this resource.
	void UpdateMaterialBuffer(Microsoft::WRL::ComPtr<ID3D12Resource >& matBuffer, UINT num, UINT size);
	// Add a new texture to the manager.
	void AddResource(Microsoft::WRL::ComPtr<ID3D12Resource >& resource,const std::string& name);

	int GetMaterialNum() { return m_uMaterialNum; }
	int GetMaterialSize() { return m_uMaterialSize; }
	std::vector<MaterialResource>& GetTextureResource() {return m_textureResources; }
	Microsoft::WRL::ComPtr<ID3D12Resource>& GetMaterial() { return m_materialResources[0]; }

	void Clear();

private:
	
	UINT m_uMaterialNum;
	UINT m_uMaterialSize;
	std::vector<MaterialResource> m_textureResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_materialResources;
};

