//--------------------------------------------------------------------------------------
// File: LightManager.h
//
// A class for managing a light buffer.
//--------------------------------------------------------------------------------------
#include "DirectxHelper.h"
#include <vector>

class LightManager
{
public:
	void Init(int maxLightNum, int lightTypeSize);

	void UpdateLightBuffer(const void* pData, int iTypeSize, int iNumber, bool  bDefault);

	void CopyToDefault();

	const Microsoft::WRL::ComPtr<ID3D12Resource> GetLightBuffer() { return m_lightBuffer; }
	const D3D12_SHADER_RESOURCE_VIEW_DESC&  GetSrvDesc() { return m_srvDesc; }
	int GetLightNum() { return m_iNum; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_lightBuffer;
	D3D12_SHADER_RESOURCE_VIEW_DESC m_srvDesc;
	int m_iNum;
	int m_iTypeSize;

};