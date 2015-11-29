//--------------------------------------------------------------------------------------
// File: Profiler.h
//
// A class for creating a query heap to record timestamps and for computing the delta between timestamps.
//--------------------------------------------------------------------------------------
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>
#pragma once
class Profiler
{
public:
	Profiler();
	void Init(ID3D12Device* const device, UINT maxNumber);

	double GetProfileTime(UINT index);

	// Read time delta buffer.
	void CopyTimeDelta(UINT64 tick);

	void StartTime(ID3D12GraphicsCommandList*const commandList);
	void EndTime(ID3D12GraphicsCommandList*const commandList, const std::string name);
	void ResolveTimeDelta(ID3D12GraphicsCommandList * const commandList);

private:
	void createReadBuffer();

	struct ProfilerData {
		UINT64 frequency;
		UINT id;
		std::string name;
	};

	std::vector<ProfilerData> m_profileDataList;

	UINT m_uAccumulateNum;
	UINT m_uMaxNum;

	ID3D12Device* m_d3ddevice;

	Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_copyBuffer;

	std::vector<double> m_profileTimeList;
};