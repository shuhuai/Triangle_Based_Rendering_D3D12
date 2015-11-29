//--------------------------------------------------------------------------------------
// File: FbxRender.cpp
//--------------------------------------------------------------------------------------
#include "FbxRender.h"
#include "TextureLoader.h"

using namespace Microsoft::WRL;

// Define the structure of materials for FBX models.
const PropertyDesc standardMaterialProperties[] = {
	{ FbxSurfaceMaterial::sDiffuse,FbxLoaderElement_COLOR,offsetof(StandardMaterial,albedoColor) },
	{ FbxSurfaceMaterial::sSpecular,FbxLoaderElement_COLOR,offsetof(StandardMaterial,specularColor) },
	{ FbxSurfaceMaterial::sDiffuse,FbxLoaderElement_Texture2D,offsetof(StandardMaterial,albedoMapIdx) }
};

void FbxRender::LoadModel(const std::string name, float fScale)
{

	m_fScale = fScale;
	std::vector<PropertyDesc> p;
	for (int i = 0; i < _countof(standardMaterialProperties); i++)
	{
		p.push_back(standardMaterialProperties[i]);
	}


	m_fbxLoader.Init(name, p);

	// Calculate an AABB.
	m_vertexData.resize(m_fbxLoader.m_TriangleList.size() * 3);
	float maxX = FBXSDK_FLOAT_MIN, maxY = FBXSDK_FLOAT_MIN, maxZ = FBXSDK_FLOAT_MIN;
	float minX = FBXSDK_FLOAT_MAX, minY = FBXSDK_FLOAT_MAX, minZ = FBXSDK_FLOAT_MAX;
	for (unsigned int i = 0; i < m_fbxLoader.m_TriangleList.size(); i++)
	{
		for (unsigned int j = 0; j < 3; j++)
		{
			m_vertexData[3 * i + j].matIdx = m_fbxLoader.m_TriangleList[i].SubsetIndex;
			m_vertexData[3 * i + j].texcoord = m_fbxLoader.m_TriangleList[i].Vertex[j].TexCoords[0];
			m_vertexData[3 * i + j].texcoord.x = m_vertexData[3 * i + j].texcoord.x*-1;
			m_vertexData[3 * i + j].texcoord.y = m_vertexData[3 * i + j].texcoord.y*-1;
			m_vertexData[3 * i + j].normal = m_fbxLoader.m_TriangleList[i].Vertex[j].Normal;
			m_vertexData[3 * i + j].position = DirectX::XMFLOAT4(m_fbxLoader.m_TriangleList[i].Vertex[j].Position.x*m_fScale, m_fbxLoader.m_TriangleList[i].Vertex[j].Position.y*m_fScale, m_fbxLoader.m_TriangleList[i].Vertex[j].Position.z*m_fScale, 1.0f);
			maxX = max(maxX, m_vertexData[3 * i + j].position.x);
			maxY = max(maxY, m_vertexData[3 * i + j].position.y);
			maxZ = max(maxZ, m_vertexData[3 * i + j].position.z);
			minX = min(minX, m_vertexData[3 * i + j].position.x);
			minY = min(minY, m_vertexData[3 * i + j].position.y);
			minZ = min(minZ, m_vertexData[3 * i + j].position.z);

		}
	}
	m_minAxis = DirectX::XMFLOAT3(minX, minY, minZ);
	m_maxAxis = DirectX::XMFLOAT3(maxX, maxY, maxZ);

	m_uVertexNumber = m_fbxLoader.m_TriangleList.size() * 3;
	// Release m_fbxLoader memory, or we have two copies in memory.
	m_fbxLoader.ClearTriangles();
}

void FbxRender::CreateGpuResources(MaterialManager & materialMgr)
{
	CreateMaterials(materialMgr);
	CreateResource<FullVertex>(&m_vertexData[0], m_uVertexNumber);
	// After copying data to GPU memory, we can release CPU memory.
	m_vertexData.clear();
	m_vertexData.shrink_to_fit();
}

DirectX::XMFLOAT3 FbxRender::GetCenter()
{
	using namespace DirectX;
	return   GMathVF((GMathFV(m_maxAxis) + GMathFV(m_minAxis)));
}


void FbxRender::Render(ID3D12GraphicsCommandList* const  commandList)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &m_vbView);

	commandList->DrawInstanced(m_uVertexNumber, 1, 0, 0);
}

void FbxRender::CreateMaterials(MaterialManager& materialMgr)
{

	std::vector<StandardMaterial> materialsVector;
	int defaultIdx = 0;

	// Try to use a default texture,
	// when we are unable to load a texture.
	for (unsigned int i = 0; i < materialMgr.GetTextureResource().size(); i++)
	{
		if (materialMgr.GetTextureResource()[i].name == "Default")
		{
			defaultIdx = i;
			break;
		}
	}


	for (unsigned int matIdx = 0; matIdx < m_fbxLoader.m_MaterialList.size(); matIdx++)
	{
		StandardMaterial material;

		// Use reinterpret_cast to load data for a material type.
		StandardMaterial* pConveter = reinterpret_cast<StandardMaterial*>(m_fbxLoader.m_MaterialList[matIdx].rawData);
		unsigned int texIdx = pConveter->albedoMapIdx;

		if (texIdx < m_fbxLoader.m_TextureList.size())
		{

			ComPtr<ID3D12Resource> Texture;
			std::string path = m_fbxLoader.m_TextureList[texIdx].name;

			if (path.size() > 10)
			{
				bool repeatTexture = false;
				// Search the textures in the material manager.
				for (unsigned int i = 0; i < materialMgr.GetTextureResource().size(); i++)
				{
					if (materialMgr.GetTextureResource()[i].name == path)
					{
						repeatTexture = true;
						texIdx = i;
						break;
					}

				}
				// Create a new texture resource, if we don't find the same one in the material manager.
				if (!repeatTexture)
				{
					HRESULT hr = CreateWICTextureFromFileEx(g_d3dObjects->GetD3DDevice(), std::wstring(path.begin(), path.end()).c_str(), 0, 0, 0, 0, 0, &Texture);
					materialMgr.AddResource(Texture, path);
					texIdx = materialMgr.GetTextureResource().size() - 1;
				}
			}
			else {
				// This filename is invalid.
				texIdx = defaultIdx;
			}

		}
		else {
			// No any texture in the FBX model.
			texIdx = defaultIdx;
		}
		// Store the material data into this manager.
		material.albedoColor = pConveter->albedoColor;
		material.specularColor = pConveter->specularColor;
		material.albedoMapIdx = texIdx;
		materialsVector.push_back(material);
	}

	if (materialsVector.size())
	{
		// Create a material buffer.
		ComPtr<ID3D12Resource> materialBuffer;
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
		resourceDesc.Height = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Width = sizeof(materialsVector[0])*materialsVector.size();
		ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(materialBuffer.GetAddressOf())));

		void* mapped = nullptr;
		materialBuffer->Map(0, nullptr, &mapped);
		memcpy(mapped, &materialsVector[0], sizeof(materialsVector[0])*materialsVector.size());
		materialBuffer->Unmap(0, nullptr);

		materialMgr.UpdateMaterialBuffer(materialBuffer, materialsVector.size(), sizeof(materialsVector[0]));
	}
}


template<class T>
void FbxRender::CreateResource(void* VB, int num)
{
	UINT size = num*sizeof(T);
	D3D12_HEAP_PROPERTIES heapProperty;
	ZeroMemory(&heapProperty, sizeof(heapProperty));
	heapProperty.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = size;
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())));

	UINT8* dataBegin;
	ThrowIfFailed(m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dataBegin)));
	memcpy(dataBegin, VB, size);
	m_vertexBuffer->Unmap(0, nullptr);

	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.StrideInBytes = sizeof(T);
	m_vbView.SizeInBytes = size;

	g_copyManager->Add({ m_vertexBuffer,&m_vbView });
}
