//--------------------------------------------------------------------------------------
// File: FbxLoader.h
//
// A class for loading FBX files by FBX SDK from Autodesk.
//--------------------------------------------------------------------------------------
#pragma once
#include <fbxsdk.h>
#include "DirectxHelper.h"
#include <vector>

// Supported data types.
enum PropertyElementType
{
	FbxLoaderElement_Texture2D,
	FbxLoaderElement_COLOR
};

// A structure to describe the property of materials we want to load.
struct PropertyDesc
{
	std::string name;
	PropertyElementType type;
	size_t offset;
};

// A structure to store temporary vertex data before using a D3D12 input structure (D3D12_INPUT_LAYOUT).
struct ExportMeshVertex
{
public:
	ExportMeshVertex()
	{
		Initialize();
	}
	void Initialize()
	{
		ZeroMemory(this, sizeof(ExportMeshVertex));
		BoneWeights.x = 1.0f;
	}
	UINT                            DCCVertexIndex;
	DirectX::XMFLOAT3               Position;
	DirectX::XMFLOAT3               Normal;
	DirectX::XMFLOAT3               SmoothNormal;
	DirectX::XMFLOAT3               Tangent;
	DirectX::XMFLOAT3               Binormal;
	DirectX::XMFLOAT4               BoneWeights;
	DirectX::XMFLOAT2               TexCoords[8];
	DirectX::XMFLOAT4               Color;
};
// A structure to store temporary triangle information.
struct ExportMeshTriangle
{
public:
	ExportMeshTriangle()
		: SubsetIndex(0),
		PolygonIndex(-1)
	{
	}
	void Initialize()
	{
		SubsetIndex = 0;
		Vertex[0].Initialize();
		Vertex[1].Initialize();
		Vertex[2].Initialize();
	}
	ExportMeshVertex    Vertex[3];
	INT                 SubsetIndex;
	INT                 PolygonIndex;
};

// A structure to store temporary material data.
// Don't know what properties of materials we should load, so
// I use a typeless buffer to store information.
struct ExportMaterial
{
	byte rawData[256];
};

// A structure to store temporary texture information.
struct ExportTexture
{
	std::string name;
	std::string uvName;
};


class FbxLoader
{
public:
	~FbxLoader();
	// Temporary data.
	// We will convert the information to the resources we can use in GPU.
	std::vector<ExportMeshTriangle> m_TriangleList;
	std::vector<ExportMaterial> m_MaterialList;
	std::vector<ExportTexture> m_TextureList;

	// Input the model filename, and the material properties we want to load.
	void Init(const std::string name, const std::vector<PropertyDesc>& descs);
	// Clear all temporary data.
	void Clear();
	// Clear temporary triangle data.
	void ClearTriangles();

private:
	const int MaxTextureNum = 8;
	FbxManager* g_pFbxSdkManager = nullptr;
	std::vector<PropertyDesc> m_propertyDescs;
	void ParseNode(FbxNode* pNode);
	void ParseMesh(FbxMesh* pFbxMesh);
	void ParseMaterials(FbxNode* pFbxMesh);
};