//--------------------------------------------------------------------------------------
// File: DirectXHelper.h
//
// This file contains many helper classes and functions for my D3D12 implementation.
//
// The helper classes are shown as follows:
// 1. D3dDeviceManager: Manage D3D12 devices.
// 2. AllocatorEngineManager: Manage D3D12 queues and allocators.
// 3. ResourceCopyManager: Copy upload heap to default heap.
// 4. PIX Event functions: Set different events for graphics debuggers.
// 5. CreateCommittedBufferResource: A helper function to create committed buffers.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "Exception.h" 
#include <vector>
#include "CDescriptorHeapWrapper.h"
#pragma once

// The types of resource views.
enum UploadObjectViewType
{
	IB_VIEW,		// D3D12_INDEX_BUFFER_VIEW
	VB_VIEW,		// D3D12_VERTEX_BUFFER_VIEW
	DESCRIPTOR,		// D3D12_SHADER_RESOURCE_VIEW_DESC
	NONE			// Without view (GPUVirtualAddress)
};

// To use the copy manager, input a D3D12 resource, and the view of this resource.
struct UploadToDefaultObject
{
	Microsoft::WRL::ComPtr<ID3D12Resource>& resource;

	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	UploadObjectViewType type;
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVdesc;
	D3D12_INDEX_BUFFER_VIEW* IBdesc;
	D3D12_VERTEX_BUFFER_VIEW*  VBdesc;

	UploadToDefaultObject(Microsoft::WRL::ComPtr<ID3D12Resource>& res, D3D12_VERTEX_BUFFER_VIEW* desc) : resource(res), VBdesc(desc) { type = VB_VIEW; }
	UploadToDefaultObject(Microsoft::WRL::ComPtr<ID3D12Resource>& res, D3D12_INDEX_BUFFER_VIEW* desc) : resource(res), IBdesc(desc) { type = IB_VIEW; }
	UploadToDefaultObject(Microsoft::WRL::ComPtr<ID3D12Resource>& res, D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_SHADER_RESOURCE_VIEW_DESC desc) : resource(res), handle(handle), SRVdesc(desc) { type = DESCRIPTOR; }
	UploadToDefaultObject(Microsoft::WRL::ComPtr<ID3D12Resource>& res, D3D12_SHADER_RESOURCE_VIEW_DESC desc) : resource(res), SRVdesc(desc) { type = NONE; }
	UploadToDefaultObject(Microsoft::WRL::ComPtr<ID3D12Resource>& res) : resource(res) { type = NONE; }
};




class D3dDeviceManager
{

public:
	D3dDeviceManager();
	~D3dDeviceManager();
	void CreateDeviceResources();
	void WaitForGPU();
	void CreateWindowSizeDependentResources();
	void SetWindows(HWND hwnd, UINT width, UINT height);
	void ValidateDevice();
	void Present();
	void MoveToNextFrame();
	bool						IsDeviceRemoved() const { return m_bDeviceRemoved; }
	// D3D Accessors.
	ID3D12Device*				GetD3DDevice() const { return m_d3dDevice.Get(); }
	IDXGISwapChain*				GetSwapChain() const { return m_swapChain.Get(); }
	ID3D12Resource*				GetRenderTarget() const { return m_renderTargets[m_uCurrentFrame].Get(); }
	ID3D12Resource*				GetRenderTarget(int index) const { return m_renderTargets[index].Get(); }
	ID3D12CommandQueue*			GetCommandQueue() const { return m_commandQueue.Get(); }
	ID3D12CommandAllocator*		GetCommandAllocator() const { return m_commandAllocators[m_uCurrentFrame].Get(); }
	D3D12_VIEWPORT				GetScreenViewport() const { return m_screenViewport; }
	UINT GetFramCounter() { return FrameCount; }
	UINT GetCurrentCounter() { return m_uCurrentFrame; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = m_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_rtvDescriptorSize*m_uCurrentFrame;
		return handle;
	}
	void ResetCommandQueue()
	{
		m_commandQueue.Reset();
	};
private:
	// Cached reference to the Window.
	HWND m_hwnd;
	// Direct3D objects.
	Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
	WCHAR* m_sDeviceName;
	static const UINT								FrameCount = 3;	// Use triple buffering.
	UINT											m_uCurrentFrame;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
	Microsoft::WRL::ComPtr<ID3D12Resource>			m_renderTargets[FrameCount];
	bool											m_bDeviceRemoved;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT m_rtvDescriptorSize;
	D3D12_VIEWPORT									m_screenViewport;

	// CPU/GPU Synchronization.
	Microsoft::WRL::ComPtr<ID3D12Fence>				m_fence;
	UINT64											m_uFenceValues[FrameCount];
	HANDLE											m_fenceEvent;

	UINT					m_uRenderTargetWidth;
	UINT					m_uRenderTargetHeight;
	UINT					m_uOutputWidth;
	UINT					m_uOutputHeight;
};

class AllocatorEngineManager
{

public:
	AllocatorEngineManager();
	void Init(D3D12_COMMAND_LIST_TYPE type, int numAllocator);
	void WaitForGPU();
	void MoveToNextFrame();
	void Signal(ID3D12Fence* fence, UINT value);
	void Wait(ID3D12Fence* fence, UINT value);
	ID3D12CommandAllocator*	GetCommandAllocator() const { return m_commandAllocator[m_uCurrentFrame].Get(); }
	// Initialize a new command list by this allocator.
	void InitCommandList(D3D12_COMMAND_LIST_TYPE commandType, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
	ID3D12CommandQueue*	GetCommandQueue() const { return m_queue.Get(); }

private:
	UINT m_uNumFrame;
	UINT m_uCurrentFrame;
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_allocator;

	// CPU/GPU Synchronization.
	Microsoft::WRL::ComPtr<ID3D12Fence>				m_fence;
	std::vector<UINT64>			m_fenceValues;
	HANDLE											m_fenceEvent;
};


class ResourceCopyManager
{
public:
	ResourceCopyManager();
	void Add(UploadToDefaultObject obj);
	void GpuDataCopy();
	void RepleceResourceViews();
	int GetElementNum() { return m_copyObjects.size(); }
private:
	std::vector<UploadToDefaultObject> m_copyObjects;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_defaultResourc;
	AllocatorEngineManager m_copyEngine;

	UINT m_uCopyIdx;
	bool m_bCopying;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

};
// Global D3D device.
extern std::shared_ptr<D3dDeviceManager> g_d3dObjects;
// Global copying manager to copy upload heap to default heap.
extern std::shared_ptr<ResourceCopyManager> g_copyManager;


static void AddResourceBarrier(
	ID3D12GraphicsCommandList* command,
	ID3D12Resource* pResource,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after
	)
{
	D3D12_RESOURCE_BARRIER desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	desc.Transition.pResource = pResource;
	desc.Transition.StateBefore = before;
	desc.Transition.StateAfter = after;
	command->ResourceBarrier(1, &desc);
};

static void AddResourceBarrier(
	ID3D12GraphicsCommandList* command,
	std::vector<ID3D12Resource*> pResource,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after
	)
{
	int num = pResource.size();
	if (num > 0)
	{
		std::vector<D3D12_RESOURCE_BARRIER> resBarrierRes;
		resBarrierRes.resize(num);
		for (int i = 0; i < num; i++)
		{
			D3D12_RESOURCE_BARRIER& desc = resBarrierRes[i];
			ZeroMemory(&desc, sizeof(desc));
			desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			desc.Transition.pResource = pResource[i];
			desc.Transition.StateBefore = before;
			desc.Transition.StateAfter = after;
		}

		command->ResourceBarrier(num, &resBarrierRes[0]);
	}
};


const UINT PIX_EVENT_UNICODE_VERSION = 0;

inline void PIXBeginEvent(ID3D12CommandQueue* pCommandQueue, UINT64 /*metadata*/, PCWSTR pFormat)
{
	pCommandQueue->BeginEvent(PIX_EVENT_UNICODE_VERSION, pFormat, (wcslen(pFormat) + 1) * sizeof(pFormat[0]));
}

inline void PIXBeginEvent(ID3D12GraphicsCommandList* pCommandList, UINT64 /*metadata*/, PCWSTR pFormat)
{

	pCommandList->BeginEvent(PIX_EVENT_UNICODE_VERSION, pFormat, (wcslen(pFormat) + 1) * sizeof(pFormat[0]));
}
inline void PIXEndEvent(ID3D12CommandQueue* pCommandQueue)
{
	pCommandQueue->EndEvent();
}
inline void PIXEndEvent(ID3D12GraphicsCommandList* pCommandList)
{
	pCommandList->EndEvent();
}

template<class T> void CreateCommittedBufferResource(void* buffer, int num, Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
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

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource.GetAddressOf())));

	UINT8* dataBegin;
	ThrowIfFailed(resource->Map(0, nullptr, reinterpret_cast<void**>(&dataBegin)));
	memcpy(dataBegin, buffer, size);
	resource->Unmap(0, nullptr);
};

