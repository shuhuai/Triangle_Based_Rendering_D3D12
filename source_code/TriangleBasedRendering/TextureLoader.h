//--------------------------------------------------------------------------------------
// File: TextureLoader.h
//
// Functions for loading textures. A modification of DirectXTK to support DirectX12.
//--------------------------------------------------------------------------------------

#include <d3d12.h>
#include <stdint.h>

// Load a texture from a file path.
// Return a D3D12 Resource.
HRESULT __cdecl CreateWICTextureFromFileEx(_In_ ID3D12Device* d3dDevice,
	_In_z_ const wchar_t* szFileName,
	_In_ size_t maxsize,
	_In_ unsigned int bindFlags,
	_In_ unsigned int cpuAccessFlags,
	_In_ unsigned int miscFlags,
	_In_ bool forceSRGB,
	_Out_opt_ ID3D12Resource** texture
	);