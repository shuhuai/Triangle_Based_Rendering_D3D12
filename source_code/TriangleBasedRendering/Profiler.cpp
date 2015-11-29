//--------------------------------------------------------------------------------------
// File: Profiler.cpp
//--------------------------------------------------------------------------------------
#include "Profiler.h"
#include <string>
Profiler::Profiler()
{
	m_uAccumulateNum = 0;
}

void Profiler::Init(ID3D12Device * device, UINT maxNumber)
{
#if defined(_PROFILING)
	m_uMaxNum = maxNumber;
	m_d3ddevice = device;

	D3D12_QUERY_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	heapDesc.Count = maxNumber*2;	
	HRESULT hr = m_d3ddevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(m_queryHeap.GetAddressOf()));
	
	createReadBuffer();
#endif
}



void Profiler::ResolveTimeDelta(ID3D12GraphicsCommandList * commandList)
{
#if defined(_PROFILING)
	const UINT uAlignment = 8;
	UINT uBufferSize = m_uAccumulateNum * 2;
	if (uBufferSize > 0 && uBufferSize < m_uMaxNum)
	{
		commandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, uBufferSize, m_copyBuffer.Get(), uAlignment);
	}
#endif
}

void Profiler::CopyTimeDelta(UINT64 tick)
{
#if defined(_PROFILING)
	if (m_uAccumulateNum > 0)
	{
		m_profileTimeList.clear();
		UINT bufferSize = m_uAccumulateNum * 2;
		D3D12_RESOURCE_DESC resourceDesc = m_copyBuffer->GetDesc();

		void* mapped = nullptr;
		UINT64* data = new UINT64[bufferSize + 1];

		// Allocate an additional space to store a null value in the first element (bufferSize + 1), why?
		D3D12_RANGE range = D3D12_RANGE{ 0,bufferSize + 1 };
		m_copyBuffer->Map(0, &range, &mapped);
		memcpy(data, mapped, (bufferSize + 1) * sizeof(UINT64));
		m_copyBuffer->Unmap(0, nullptr);

		for (unsigned int i = 0; i < bufferSize; i += 2)
		{
			UINT64 begin = data[1 + i];
			UINT64 end = data[1 + i + 1];
			UINT64 delta = end - begin;

			double time = (double)delta / (double)tick;

			std::string debugStr = m_profileDataList[i / 2].name + ":" + std::to_string(time) + "\n";
			m_profileTimeList.push_back(time);
		}
		delete[] data;

		m_profileDataList.clear();
		m_uAccumulateNum = 0;
	}
#endif
}

void Profiler::StartTime(ID3D12GraphicsCommandList * commandList)
{
#if defined(_PROFILING)
	commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_uAccumulateNum);
#endif
}

void Profiler::EndTime(ID3D12GraphicsCommandList * commandList, const std::string name)
{
#if defined(_PROFILING)
	m_profileDataList.clear();
	m_uAccumulateNum++;
	commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_uAccumulateNum);
	// To do:
	// Make better code to support nested record timestamps.
	ProfilerData data;
	data.id = m_uAccumulateNum;
	data.name = name;
	m_profileDataList.push_back(data);
#endif
}

double Profiler::GetProfileTime(UINT index)
{
	if (m_profileTimeList.size() > index)
	{
		return m_profileTimeList[index];
	}
	else {
		return 0.0;
	}
}

void Profiler::createReadBuffer()
{
	D3D12_HEAP_PROPERTIES heapProperty;
	ZeroMemory(&heapProperty, sizeof(heapProperty));
	heapProperty.Type = D3D12_HEAP_TYPE_READBACK;
	heapProperty.CreationNodeMask = 1;
	heapProperty.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.DepthOrArraySize = 1;
	// Allocate an additional space to store a null value in the first element (bufferSize + 1), why?
	resourceDesc.Width = sizeof(UINT64)*(m_uMaxNum*2);
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	m_d3ddevice->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(m_copyBuffer.GetAddressOf()));
}
