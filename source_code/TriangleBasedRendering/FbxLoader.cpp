//--------------------------------------------------------------------------------------
// File: FbxLoader.cpp
//--------------------------------------------------------------------------------------
#include "FbxLoader.h"

FbxLoader::~FbxLoader()
{
	Clear();
}

void FbxLoader::Init(const std::string name, const std::vector<PropertyDesc>& descs)
{

	Clear();
	// Copy properties.
	m_propertyDescs = descs;

	// Create FBX Manager to load FBX data.
	if (g_pFbxSdkManager == nullptr)
	{
		g_pFbxSdkManager = FbxManager::Create();
	}

	// Load a FBX file by the filename.
	FbxIOSettings* pIOsettings = FbxIOSettings::Create(g_pFbxSdkManager, IOSROOT);
	g_pFbxSdkManager->SetIOSettings(pIOsettings);
	FbxImporter* pImporter = FbxImporter::Create(g_pFbxSdkManager, "");
	FbxScene* pFbxScene = FbxScene::Create(g_pFbxSdkManager, "");
	bool bSuccess = pImporter->Initialize(name.c_str(), -1, g_pFbxSdkManager->GetIOSettings());
	if (!bSuccess) ThrowIfFailed(E_FAIL);
	bSuccess = pImporter->Import(pFbxScene);
	if (!bSuccess) ThrowIfFailed(E_FAIL);
	pImporter->Destroy();

	FbxNode* pFbxRootNode = pFbxScene->GetRootNode();

	// Parse data.
	if (pFbxRootNode)
	{
		// Recursive function to load nodes.
		ParseNode(pFbxRootNode);
	}

	// Destroy the manager in order to release memory.
	g_pFbxSdkManager->Destroy();
	g_pFbxSdkManager = nullptr;

}

void FbxLoader::Clear()
{
	m_TriangleList.clear();
	m_TriangleList.shrink_to_fit();

	m_MaterialList.clear();
	m_MaterialList.shrink_to_fit();

	m_TextureList.clear();
	m_TextureList.shrink_to_fit();
}

void FbxLoader::ClearTriangles()
{
	m_TriangleList.clear();
	m_TriangleList.shrink_to_fit();
}

void FbxLoader::ParseNode(FbxNode * pNode)
{
	FbxMesh* pMesh = pNode->GetMesh();

	if (pMesh)
	{
		ParseMesh(pMesh);
	}

	if (pNode->GetMaterialCount())
	{
		ParseMaterials(pNode);
	}

	for (int i = 0; i < pNode->GetChildCount(); i++)
	{
		ParseNode(pNode->GetChild(i));
	}

}

void FbxLoader::ParseMesh(FbxMesh * pFbxMesh)
{
	FbxStringList	uvsetNames;
	pFbxMesh->GetUVSetNames(uvsetNames);

	int iNumUVSet = uvsetNames.GetCount();
	// Max UV sets.
	iNumUVSet = static_cast<int>(fminf(static_cast<float>(iNumUVSet), static_cast<float>(MaxTextureNum)));

	DWORD dwLayerCount = pFbxMesh->GetLayerCount();

	int iPolyCount = pFbxMesh->GetPolygonCount();
	if (!dwLayerCount || !pFbxMesh->GetLayer(0)->GetNormals())
	{
		pFbxMesh->InitNormals();
		pFbxMesh->GenerateNormals();

	}

	// The global material index of this model, so this index starts with the number of the materials which we've already loaded.
	int  basicMatIndex = m_MaterialList.size();

	FbxGeometryElement::EMappingMode   materialMappingMode = FbxGeometryElement::eNone;
	int LayerCount = pFbxMesh->GetLayerCount();

	// Get this mesh's material index array.
	FbxLayerElementArrayTemplate<int>* pMaterialIndices = &pFbxMesh->GetElementMaterial()->GetIndexArray();

	if (pFbxMesh->GetElementMaterial())
	{
		materialMappingMode = pFbxMesh->GetElementMaterial()->GetMappingMode();
	}

	int iNonConformingSubDPolys = 0;
	// Loop over polygons.
	for (int iPolyIndex = 0; iPolyIndex < iPolyCount; ++iPolyIndex)
	{
		// Triangulate each polygon into one or more triangles.
		int iPolySize = pFbxMesh->GetPolygonSize(iPolyIndex);
		assert(iPolySize >= 3);
		int iTriangleCount = iPolySize - 2;
		assert(iTriangleCount > 0);
		if (iPolySize > 4)
		{
			++iNonConformingSubDPolys;
		}

		// Material Index.
		int iSubsetIndex = 0;

		// Select material index depending on materialMappingMode
		if (pFbxMesh->GetElementMaterial()) {
			switch (materialMappingMode)
			{
			case FbxGeometryElement::eByPolygon:
			{
				if (pMaterialIndices->GetCount() == iPolyCount)
				{

					iSubsetIndex = pMaterialIndices->GetAt(iPolyIndex) + basicMatIndex;

				}
			}
			break;

			case FbxGeometryElement::eAllSame:
			{
				int lMaterialIndex = pMaterialIndices->GetAt(0);
				iSubsetIndex = lMaterialIndex + basicMatIndex;
			}
			break;

			}

		}

		int iCornerIndices[3];
		// Loop over triangles in the polygon.
		for (int iTriangleIndex = 0; iTriangleIndex < iTriangleCount; ++iTriangleIndex)
		{
			// Build the triangle.
			ExportMeshTriangle Triangle;

			iCornerIndices[0] = pFbxMesh->GetPolygonVertex(iPolyIndex, 0);
			iCornerIndices[1] = pFbxMesh->GetPolygonVertex(iPolyIndex, iTriangleIndex + 1);
			iCornerIndices[2] = pFbxMesh->GetPolygonVertex(iPolyIndex, iTriangleIndex + 2);

			// Store polygon index.
			Triangle.PolygonIndex = (iPolyIndex);

			Triangle.SubsetIndex = iSubsetIndex;

			int iVertIndex[3] = { 0,(iTriangleIndex + 1), (iTriangleIndex + 2) };

			// Store normal data.
			FbxVector4 vNormals[3];
			ZeroMemory(vNormals, 3 * sizeof(FbxVector4));
			pFbxMesh->GetPolygonVertexNormal(iPolyIndex, iVertIndex[0], vNormals[0]);
			pFbxMesh->GetPolygonVertexNormal(iPolyIndex, iVertIndex[1], vNormals[1]);
			pFbxMesh->GetPolygonVertexNormal(iPolyIndex, iVertIndex[2], vNormals[2]);




			auto pVertexPositions = pFbxMesh->GetControlPoints();

			std::vector<FbxVector2> uvCoordinates;
			uvCoordinates.resize(iNumUVSet * 3);

			// Store UV coordinates.
			for (int uvSet = 0; uvSet < iNumUVSet; uvSet++)
			{
				bool bUV[3];
				pFbxMesh->GetPolygonVertexUV(iPolyIndex, iVertIndex[0], uvsetNames[uvSet], uvCoordinates[uvSet * 3], bUV[0]);
				pFbxMesh->GetPolygonVertexUV(iPolyIndex, iVertIndex[1], uvsetNames[uvSet], uvCoordinates[uvSet * 3 + 1], bUV[1]);
				pFbxMesh->GetPolygonVertexUV(iPolyIndex, iVertIndex[2], uvsetNames[uvSet], uvCoordinates[uvSet * 3 + 2], bUV[2]);
			}

			// Loop vertexes in a triangle.
			for (int iCornerIndex = 0; iCornerIndex < 3; ++iCornerIndex)
			{

				const int& iDCCIndex = iCornerIndices[iCornerIndex];
				// Store DCC vertex index (this helps the mesh reduction/VB generation code).
				Triangle.Vertex[iCornerIndex].DCCVertexIndex = iDCCIndex;
				
				// UV coordinates.
				for (int uvSet = 0; uvSet < iNumUVSet; uvSet++)
				{
					Triangle.Vertex[iCornerIndex].TexCoords[uvSet].x = (float)uvCoordinates[uvSet * 3 + iCornerIndex].mData[0];
					Triangle.Vertex[iCornerIndex].TexCoords[uvSet].y = (float)uvCoordinates[uvSet * 3 + iCornerIndex].mData[1];
				}

				// Position.
				Triangle.Vertex[iCornerIndex].Position.x = (float)pVertexPositions[iDCCIndex].mData[0];
				Triangle.Vertex[iCornerIndex].Position.y = (float)pVertexPositions[iDCCIndex].mData[1];
				Triangle.Vertex[iCornerIndex].Position.z = (float)pVertexPositions[iDCCIndex].mData[2];

				// Normal.
				Triangle.Vertex[iCornerIndex].Normal.x = (float)vNormals[iCornerIndex].mData[0];
				Triangle.Vertex[iCornerIndex].Normal.y = (float)vNormals[iCornerIndex].mData[1];
				Triangle.Vertex[iCornerIndex].Normal.z = (float)vNormals[iCornerIndex].mData[2];
			}
			m_TriangleList.push_back(Triangle);
		}
	}

}

void FbxLoader::ParseMaterials(FbxNode * pNode)
{

	int iMaterialCount = pNode->GetMaterialCount();

	for (int iIndex = 0; iIndex < iMaterialCount; iIndex++)
	{
		ExportMaterial mat;
		FbxSurfaceMaterial* material = pNode->GetMaterial(iIndex);

		std::string matName(material->GetName());

		// Loop the properties we want to load.
		for (unsigned int iPropertyIdx = 0; iPropertyIdx < m_propertyDescs.size(); iPropertyIdx++)
		{
			// Load color data type.
			if (m_propertyDescs[iPropertyIdx].type == FbxLoaderElement_COLOR)
			{
				std::string colorName = m_propertyDescs[iPropertyIdx].name;
				FbxPropertyT<FbxDouble3> p = material->FindProperty(colorName.c_str());
				// Use reinterpret_cast to copy data to typeless buffer.
				FbxDouble3 info = p.Get();
				memcpy(&(mat.rawData[0]) + m_propertyDescs[iPropertyIdx].offset, reinterpret_cast<void*>(&DirectX::XMFLOAT3(static_cast<float>(info[0]), 
				static_cast<float>(info[1]), static_cast<float>(info[2]))), sizeof(DirectX::XMFLOAT3));
			}

			// Load Texture2D data type.
			if (m_propertyDescs[iPropertyIdx].type == FbxLoaderElement_Texture2D)
			{
				std::string textureName = m_propertyDescs[iPropertyIdx].name;
				FbxProperty prop = material->FindProperty(textureName.c_str());
				const int iTextureCount = prop.GetSrcObjectCount<FbxFileTexture>();
				const int iLayeredTextureCount = prop.GetSrcObjectCount<FbxLayeredTexture>();
				// The number of textures is less than the MaxTextureNum.
				if (iTextureCount + iLayeredTextureCount < MaxTextureNum)
				{

					for (int i = 0; i < iTextureCount; i++)
					{
						FbxFileTexture* pFileTexture = prop.GetSrcObject<FbxFileTexture>(i);
						if (!pFileTexture)
							continue;

						ExportTexture exportTex;
						exportTex.name = pFileTexture->GetFileName();
						m_TextureList.push_back(exportTex);
						// Use reinterpret_cast to copy data to typeless buffer.
						int iTexIdx = m_TextureList.size() - 1;
						memcpy(&(mat.rawData[0]) + m_propertyDescs[iPropertyIdx].offset, reinterpret_cast<void*>(&iTexIdx), sizeof(iTexIdx));
						pFileTexture->Destroy();
					}


					for (int i = 0; i < iLayeredTextureCount; i++)
					{
						FbxLayeredTexture* LayeredTexture = prop.GetSrcObject<FbxLayeredTexture>(i);
						const int iTextureFileCount = LayeredTexture->GetSrcObjectCount<FbxFileTexture>();

						for (int j = 0; j < iTextureFileCount; j++)
						{
							FbxFileTexture* lFileTexture = LayeredTexture->GetSrcObject<FbxFileTexture>(j);
							if (!lFileTexture)
								continue;

							ExportTexture exportTex;
							exportTex.name = lFileTexture->GetFileName();
							m_TextureList.push_back(exportTex);
							// Use reinterpret_cast to copy data to typeless buffer.
							int iTexIndex = m_TextureList.size() - 1;
							memcpy(&(mat.rawData[0]) + m_propertyDescs[iPropertyIdx].offset, reinterpret_cast<void*>(&iTexIndex), sizeof(iTexIndex));
							lFileTexture->Destroy();
						}
					}
				}
			}
		}

		m_MaterialList.push_back(mat);
	}
}