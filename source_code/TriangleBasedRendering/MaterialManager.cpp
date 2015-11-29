//--------------------------------------------------------------------------------------
// File: MaterialManager.cpp
//
// A class for managing material data, it stores the texture resources and a material buffer.
//--------------------------------------------------------------------------------------
#include"MaterialManager.h"

MaterialManager::MaterialManager()
{
}

MaterialManager::~MaterialManager()
{

	m_textureResources.clear();
	m_textureResources.shrink_to_fit();
}

void MaterialManager::UpdateMaterialBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& matBuffer, UINT num, UINT size)
{
	// If we delete the original material resource right away (because of smart pointer), the driver may crash.
	// Keep 2 material resources at the same time to delay destruction.
	m_materialResources.push_back(matBuffer);
	std::swap(m_materialResources[0], m_materialResources.back());
	if (m_materialResources.size() > 2)
	{
		std::swap(m_materialResources[1], m_materialResources.back());
		m_materialResources.pop_back();
	}

	m_uMaterialNum = num;
	m_uMaterialSize = size;
}

void MaterialManager::AddResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, const std::string& name)
{
	m_textureResources.push_back({ resource,name });
}

void MaterialManager::Clear()
{
	m_textureResources.clear();
}




