//--------------------------------------------------------------------------------------
// File: VertexStructures.h
// 
// Define vertex structures and input layouts.
//--------------------------------------------------------------------------------------
#include "DirectXMath.h"

struct NormalVertex
{
	DirectX::XMFLOAT4 position;
	DirectX::XMFLOAT3 normal;
};

static D3D12_INPUT_ELEMENT_DESC DescNormalVertex[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

struct ScreenQuadVertex
{
	DirectX::XMFLOAT4 position;
	DirectX::XMFLOAT2 texcoord;
};

static D3D12_INPUT_ELEMENT_DESC DescScreenQuadVertex[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

struct FullVertex
{
	DirectX::XMFLOAT4 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 texcoord;
	UINT matIdx;
};

static D3D12_INPUT_ELEMENT_DESC DescFullVertex[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "MATIDX", 0, DXGI_FORMAT_R32_UINT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};