//--------------------------------------------------------------------------------------
// File: DirectXHelper.cpp
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "DirectxHelper.h"

using namespace Microsoft::WRL;

std::shared_ptr<D3dDeviceManager> g_d3dObjects;
std::shared_ptr<ResourceCopyManager> g_copyManager;

D3dDeviceManager::D3dDeviceManager() :
	m_uCurrentFrame(0),
	m_screenViewport(),
	m_rtvDescriptorSize(0),
	m_fenceEvent(0),
	m_bDeviceRemoved(false)
{
	ZeroMemory(m_uFenceValues, sizeof(m_uFenceValues));
	CreateDeviceResources();
}

D3dDeviceManager::~D3dDeviceManager()
{

}
void D3dDeviceManager::CreateDeviceResources()
{

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf())));

#if defined(_DEBUG)

	// Get current graphics hardware for debugging.
	IDXGIAdapter* adapter;
	DXGI_ADAPTER_DESC desc;
	m_dxgiFactory->EnumAdapters(0, &adapter);
	adapter->GetDesc(&desc);
	// If the project is in a debug build, enable debugging via SDK Layers.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			debugController->EnableDebugLayer();
		else
			OutputDebugString("WARNING:  Unable to enable D3D12 debug validation layer\n");
	
	}
#endif




	// Create the Direct3D 12 API device object
	HRESULT hr = D3D12CreateDevice(
		0,						// Specify nullptr to use the default adapter.
		D3D_FEATURE_LEVEL_12_0,			// Feature levels this app can support.
		IID_PPV_ARGS(&m_d3dDevice)		// Returns the Direct3D device created.
		);

	if (FAILED(hr))
	{
		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see: 
		// http://go.microsoft.com/fwlink/?LinkId=286690

		ComPtr<IDXGIAdapter> warpAdapter;
		m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));

		ThrowIfFailed(
			D3D12CreateDevice(
				warpAdapter.Get(),
				D3D_FEATURE_LEVEL_12_0,
				IID_PPV_ARGS(&m_d3dDevice)
				)
			);
	}

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	for (UINT n = 0; n < FrameCount; n++)
	{
		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
	}

	// Create synchronization objects.
	ThrowIfFailed(m_d3dDevice->CreateFence(m_uFenceValues[m_uCurrentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_uFenceValues[m_uCurrentFrame]++;

	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);

#if defined(_DEBUG)
	m_d3dDevice->SetStablePowerState(true);
#endif

}



void D3dDeviceManager::CreateWindowSizeDependentResources()
{
	// Wait until all previous GPU work is complete.
	WaitForGPU();

	// Clear the previous window size specific context.
	for (UINT n = 0; n < FrameCount; n++)
	{
		m_renderTargets[n].Reset();

	}

	m_rtvHeap.Reset();

	m_uRenderTargetWidth = m_uOutputWidth;
	m_uRenderTargetHeight = m_uOutputHeight;

	if (m_swapChain != nullptr)
	{
		// If the swap chain already exists, resize it.
		HRESULT hr = m_swapChain->ResizeBuffers(
			FrameCount,
			m_uRenderTargetWidth,
			m_uRenderTargetHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			m_bDeviceRemoved = true;

			// Do not continue execution of this method. DeviceResources will be destroyed and re-created.
			return;
		}
		else
		{
			ThrowIfFailed(hr);
		}
	}
	else
	{
		// Otherwise, create a new one using the same adapter as the existing Direct3D device.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

		swapChainDesc.Width = m_uRenderTargetWidth; // Match the size of the window.
		swapChainDesc.Height = m_uRenderTargetHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // This is the most common swap chain format.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = FrameCount; // Use double-buffering to minimize latency.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // All Windows Store apps must use this SwapEffect.
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		ThrowIfFailed(
			m_dxgiFactory->CreateSwapChainForHwnd(
				m_commandQueue.Get(),
				m_hwnd,
				&swapChainDesc,
				nullptr,
				nullptr,
				&m_swapChain
				)
			);
	}




	// Create a render target view of the swap chain back buffer.
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = FrameCount;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap)));
		m_rtvHeap->SetName(L"Render Target View Descriptor Heap");

		// All pending GPU work was already finished. Update the tracked fence values
		// to the last value signaled.
		for (UINT n = 0; n < FrameCount; n++)
		{
			m_uFenceValues[n] = m_uFenceValues[m_uCurrentFrame];
		}

		m_uCurrentFrame = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE  hCPUHeapHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
		m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		for (UINT n = 0; n < FrameCount; n++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle;
			handle.ptr = hCPUHeapHandle.ptr + m_rtvDescriptorSize*n;
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, handle);


			WCHAR name[25];
			swprintf_s(name, L"Render Target %d", n);
			m_renderTargets[n]->SetName(name);
		}
	}

	// Set the 3D rendering viewport to target the entire window.
	m_screenViewport = { 0.0f, 0.0f,static_cast<float>(m_uRenderTargetWidth),static_cast<float>(m_uRenderTargetHeight), 0.0f, 1.0f };
}

void D3dDeviceManager::SetWindows(HWND hwnd, UINT width, UINT height)
{
	m_hwnd = hwnd;
	m_uOutputWidth = width;
	m_uOutputHeight = height;


	CreateWindowSizeDependentResources();
}

void D3dDeviceManager::
ValidateDevice()
{
	// The D3D Device is no longer valid if the default adapter changed since the device
	// was created or if the device has been removed.

	// First, get the information for the default adapter from when the device was created.

	ComPtr<IDXGIDevice3> dxgiDevice;
	ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory2> deviceFactory;
	ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC previousDesc;
	ThrowIfFailed(previousDefaultAdapter->GetDesc(&previousDesc));

	// Next, get the information for the current default adapter.

	ComPtr<IDXGIFactory2> currentFactory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC currentDesc;
	ThrowIfFailed(currentDefaultAdapter->GetDesc(&currentDesc));

	// If the adapter LUIDs don't match, or if the device reports that it has been removed,
	// a new D3D device must be created.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Release references to resources related to the old device.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		m_bDeviceRemoved = true;
	}
}

void D3dDeviceManager::Present()
{
	// The first argument instructs DXGI to block until VSync, putting the application
	// to sleep until the next VSync. This ensures we don't waste any cycles rendering
	// frames that will never be displayed to the screen.
	HRESULT hr = m_swapChain->Present(1, 0);

	// If the device was removed either by a disconnection or a driver upgrade, we 
	// must recreate all device resources.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		m_bDeviceRemoved = true;
	}
	else
	{
		ThrowIfFailed(hr);

		MoveToNextFrame();
	}
}


// Wait for pending GPU work to complete.
void D3dDeviceManager::WaitForGPU()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_uFenceValues[m_uCurrentFrame]));

	// Wait until the fence has been crossed.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_uFenceValues[m_uCurrentFrame], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_uFenceValues[m_uCurrentFrame]++;
}

// Prepare to render the next frame.
void D3dDeviceManager::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_uFenceValues[m_uCurrentFrame];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Advance the frame index.
	m_uCurrentFrame = (m_uCurrentFrame + 1) % FrameCount;

	// Check to see if the next frame is ready to start.
	if (m_fence->GetCompletedValue() < m_uFenceValues[m_uCurrentFrame])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_uFenceValues[m_uCurrentFrame], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_uFenceValues[m_uCurrentFrame] = currentFenceValue + 1;
}

ResourceCopyManager::ResourceCopyManager()
{
	// The copy manager initializes a new queue with Copy type (D3D12 multi-engine).
	m_uCopyIdx = 0;
	m_copyEngine.Init(D3D12_COMMAND_LIST_TYPE_COPY, 2);

	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_copyEngine.GetCommandAllocator(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf())));
	ThrowIfFailed(m_commandList->Close());
}

void ResourceCopyManager::Add(UploadToDefaultObject obj)
{
	m_copyObjects.push_back(obj);
}


void ResourceCopyManager::GpuDataCopy()
{
	if (m_copyObjects.size() > m_uCopyIdx)
	{

		ThrowIfFailed(m_commandList->Reset(m_copyEngine.GetCommandAllocator(), nullptr));

		// Create new default resources for every copy object,
		// and then add copy commands to the command list.
		for (unsigned int i = m_uCopyIdx; i < m_copyObjects.size(); i++)
		{
			if (m_copyObjects[i].resource != nullptr)
			{
				D3D12_HEAP_PROPERTIES heapDefaultProperty;
				ZeroMemory(&heapDefaultProperty, sizeof(heapDefaultProperty));
				heapDefaultProperty.Type = D3D12_HEAP_TYPE_DEFAULT;

				D3D12_RESOURCE_DESC resourceDesc;
				resourceDesc = m_copyObjects[i].resource->GetDesc();

				ComPtr<ID3D12Resource> DefaultResource;
				ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommittedResource(&heapDefaultProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(DefaultResource.GetAddressOf())));

				m_commandList->CopyResource(DefaultResource.Get(), m_copyObjects[i].resource.Get());

				m_defaultResourc.push_back(DefaultResource);
			}

		}

		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_copyEngine.GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		m_copyEngine.WaitForGPU();
		m_copyEngine.MoveToNextFrame();
		// Store the current copy index so that we can avoid copying the same resources.
		m_uCopyIdx = m_copyObjects.size();

	}
}

void ResourceCopyManager::RepleceResourceViews()
{
	// Replace upload resource with default resources.
	for (unsigned int i = 0; i < m_defaultResourc.size(); i++)
	{
		m_copyObjects[i].resource = m_defaultResourc[i];
		if (m_copyObjects[i].type == VB_VIEW) {
			m_copyObjects[i].VBdesc->BufferLocation = m_copyObjects[i].resource->GetGPUVirtualAddress();
		}
		else if (m_copyObjects[i].type == IB_VIEW) {
			m_copyObjects[i].IBdesc->BufferLocation = m_copyObjects[i].resource->GetGPUVirtualAddress();
		}
		else if (m_copyObjects[i].type == DESCRIPTOR) {
			g_d3dObjects->GetD3DDevice()->CreateShaderResourceView(m_copyObjects[i].resource.Get(), &m_copyObjects[i].SRVdesc, m_copyObjects[i].handle);
		}
	}
	// Clear data.
	m_uCopyIdx = 0;
	m_copyObjects.clear();
	m_defaultResourc.clear();

}

AllocatorEngineManager::AllocatorEngineManager()
{
	m_uCurrentFrame = 0;
}

void AllocatorEngineManager::Init(D3D12_COMMAND_LIST_TYPE type, int numAllocator)
{
	m_uNumFrame = numAllocator;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = type;

	// Create command queue if it is not a bundle.
	if (type != D3D12_COMMAND_LIST_TYPE_BUNDLE)
	{
		ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_queue.GetAddressOf())));
	}

	for (int i = 0; i < numAllocator; i++)
	{
		ComPtr<ID3D12CommandAllocator> allocator;
		ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(allocator.GetAddressOf())));
		m_commandAllocator.push_back(allocator);
	}

	// All pending GPU work was already finished. Update the tracked fence values
	// to the last value signaled.
	m_fenceValues.resize(m_uNumFrame);
	for (int n = 0; n < numAllocator; n++)
	{
		m_fenceValues[n] = m_fenceValues[m_uCurrentFrame];
	}
	// Create synchronization objects.
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateFence(m_fenceValues[m_uCurrentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValues[m_uCurrentFrame]++;

	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
}



// Wait for pending GPU work to complete.
void AllocatorEngineManager::WaitForGPU()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_queue->Signal(m_fence.Get(), m_fenceValues[m_uCurrentFrame]));

	// Wait until the fence has been crossed.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_uCurrentFrame], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_fenceValues[m_uCurrentFrame]++;
}

void AllocatorEngineManager::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValues[m_uCurrentFrame];
	ThrowIfFailed(m_queue->Signal(m_fence.Get(), currentFenceValue));

	// Advance the frame index.
	m_uCurrentFrame = (m_uCurrentFrame + 1) % m_uNumFrame;

	// Check to see if the next frame is ready to start.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_uCurrentFrame])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_uCurrentFrame], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_uCurrentFrame] = currentFenceValue + 1;
}

void AllocatorEngineManager::Signal(ID3D12Fence * fence, UINT value)
{
	ThrowIfFailed(m_queue->Signal(fence, value));
}

void AllocatorEngineManager::Wait(ID3D12Fence * fence, UINT value)
{
	ThrowIfFailed(m_queue->Wait(fence, value));
}

void AllocatorEngineManager::InitCommandList(D3D12_COMMAND_LIST_TYPE commandType, ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	ThrowIfFailed(g_d3dObjects->GetD3DDevice()->CreateCommandList(0, commandType, m_commandAllocator[m_uCurrentFrame].Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())));
}

