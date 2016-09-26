//--------------------------------------------------------------------------------------
// File: main.cpp
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "windowsApp.h"
#include "DirectxHelper.h"
#include <stdio.h>
#include "FbxRender.h"
#include "DeferredRender.h"
#include "CameraManager.h"
#include "LightClusteredManager.h"
#include "Profiler.h"
#include "MaterialManager.h"
#include "TextureLoader.h"
#include"PreviewLight.h"
#include "LightManager.h"
#include "D2DManager.h"
#include <thread>
#include <future>

// Multithreading enable/disable.
#define SINGLETHREADED false


class directxApp :public windowsApp
{

private:
	// The maximum number of lights.
	static const int MaxLight = 2048;

	// A command list for main rendering thread.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_depthPrePassList;

	// A command list for computing thread.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

	// Visualize the number of lights per culling triangle or tile.
	bool m_bDebugMode = false;
	// Show every lights' positions and colors.
	bool m_bLightDebugMode = false;

	// After initialization?
	bool m_bInit = false;
	// After resize?
	bool m_bResize = false;
	// Change Light data?
	bool m_bEditLight = false;

	// Finish loading model?
	bool m_bSwitchSceneFinished = false;
	// Loading model?
	bool m_bSwitchingScene = false;
	// Should we rebuild bundle?
	bool m_bBundleEdit = false;

	FbxRender m_fbxRender;
	MaterialManager m_materialManager;

	ViewData m_cameraData;
	GCamera m_camera;

	AllocatorEngineManager m_computeEngine;
	AllocatorEngineManager m_bundleAllocator;

	Profiler m_profiler;
	Profiler m_computeProfiler;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_GBufferBundle;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_whiteTexture;

	DeferredRender m_deferredTech;

	// Lighting objects.
	LightClusteredManager m_clusteredManager;
	LightManager m_lightManager;
	LightPreviewer m_lightPreviewer;
	std::vector<PointLight> m_lights;

	D2DManager m_uiManager;

	// Thread objects.
	std::condition_variable m_computeCV;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fenceComputeThread;
	std::thread m_copyThread;
	UINT m_uCurFenCounter = 0;

	// Debug information.
	LARGE_INTEGER  m_time;
	std::string m_sSceneInfo;
	int m_iLightNumberInfo;
	double m_dLightCullTimeInfo;
	double m_dLightingTimeInfo;

	// Create a string for debugging.
	std::wstring  CreateInfoText()
	{
		std::wstring output;
		std::wstring scene(m_sSceneInfo.begin(), m_sSceneInfo.end());
		output.append(L"Scene:");

		output.append(scene);
		output.append(L"\n");

		output.append(L"Light Number:");
		output.append(std::to_wstring(m_iLightNumberInfo));
		output.append(L"\n");

		output.append(L"Create Light Table:");
		output.append(std::to_wstring(m_dLightCullTimeInfo * 1000));
		output.append(L"ms\n");

		output.append(L"Light Pass:");
		output.append(std::to_wstring(m_dLightingTimeInfo * 1000));
		output.append(L"ms\n");
		if (UseTriLightCulling)
		{
			output.append(L"Triangle-based culling\n");
		}

		return output;
	}

	// Multithreading functions.
	void LoadThreadContexts()
	{
#if !SINGLETHREADED
		std::thread(&directxApp::GpuComputeWorkThread, this, this).detach();
#endif
	};


	void GpuComputeWorkThread(LPVOID pvoid)
	{
		std::mutex ComputeMutex;
		std::unique_lock<std::mutex> locker(ComputeMutex);
		while (1)
		{
			m_computeCV.wait(locker);
			UpdateClusteredLightCB();
		}
	};
	void GpuCopyWorkThread(LPVOID pvoid)
	{
		g_copyManager->GpuDataCopy();

	};
	void ShaderDataLoadWorkThread(std::string name)
	{
		g_ShaderManager.LoadMtFolder(name);
	}
	void ModelDataLoadWorkThread(std::string filename, float scale)
	{
		m_fbxRender.LoadModel(filename, scale);
		m_bSwitchSceneFinished = true;
		// Save debug data.
		m_sSceneInfo = filename;
	}

	void ModelDataLoadInitThread(std::string filename, float scale)
	{
		m_fbxRender.LoadModel(filename, scale);
		// Save debug data.
		m_sSceneInfo = filename;
	}


	// Initialize and manage a D3D12 device.
	std::shared_ptr<D3dDeviceManager> GetDeviceResources()
	{
		if (g_d3dObjects != nullptr && g_d3dObjects->IsDeviceRemoved())
		{

			g_d3dObjects = nullptr;
			g_copyManager = nullptr;
			OnDeviceRemoved();
		}

		if (g_d3dObjects == nullptr)
		{
			g_d3dObjects = std::make_shared<D3dDeviceManager>();
			g_d3dObjects->SetWindows(m_hwnd, m_iWidth, m_iHeight);
		}
		if (g_copyManager == nullptr)
		{
			g_copyManager = std::make_shared<ResourceCopyManager>();

		}

		return g_d3dObjects;

	};
	void OnDestroy()
	{
		g_d3dObjects = nullptr;
		g_copyManager = nullptr;
	};
	void OnDeviceRemoved()
	{

	};

	// Load new FBX data.
	void SwitchScene(std::string filename, float scale)
	{
		m_bSwitchingScene = true;


#if !SINGLETHREADED
		std::thread(&directxApp::ModelDataLoadWorkThread, this, filename, scale).detach();
#else
		ModelDataLoadWorkThread(filename, scale);
#endif

	}
	// Create new bundles.
	void RecordBundle()
	{

		m_bundleAllocator.InitCommandList(D3D12_COMMAND_LIST_TYPE_BUNDLE, m_GBufferBundle);
		// Build a bundle for creating G-buffer.
		m_deferredTech.ApplyCreateGbufferPso(m_GBufferBundle.Get(), true);
		m_fbxRender.Render(m_GBufferBundle.Get());

		m_GBufferBundle->Close();
	};

	void UpdateClusteredLightCB()
	{

		ThrowIfFailed(m_computeEngine.GetCommandAllocator()->Reset());
		ThrowIfFailed(m_computeCommandList->Reset(m_computeEngine.GetCommandAllocator(), nullptr));

		// Profile light culling time.
		m_computeProfiler.StartTime(m_computeCommandList.Get());

	
		m_clusteredManager.RunLightCullingCS(m_computeCommandList.Get());

		m_computeProfiler.EndTime(m_computeCommandList.Get(), "Build Light");
		m_computeProfiler.ResolveTimeDelta(m_computeCommandList.Get());

		m_computeCommandList->Close();
		

		ID3D12CommandList* ppCommandLists[] = { m_computeCommandList.Get() };
		// Wait until the pre-depth stage is finished.
		m_computeEngine.Wait(m_fenceComputeThread.Get(), m_uCurFenCounter + 101);
		m_computeEngine.GetCommandQueue()->ExecuteCommandLists(1, ppCommandLists);


		m_computeEngine.MoveToNextFrame();

		// Copy profiling data.
		UINT64 freq;
		m_computeEngine.GetCommandQueue()->GetTimestampFrequency(&freq);
		m_computeProfiler.CopyTimeDelta(freq);

		// Save debug data.
		m_dLightCullTimeInfo = m_computeProfiler.GetProfileTime(0);
	}


	float GetRandomFloat()
	{
		return (float)rand() / (float)RAND_MAX;
	}

	// Regenerate all lights randomly between up-bound and low-bound.
	void RandomLights(DirectX::XMFLOAT3 upbound, DirectX::XMFLOAT3 lowbound)
	{
		// srand(static_cast<unsigned int>(time(NULL)));
		float rand = GetRandomFloat();
		float extentX = upbound.x - lowbound.x;
		float extentY = upbound.y - lowbound.y;
		float extentZ = upbound.z - lowbound.z;
		for (int i = 0; i < MaxLight*0.5 ; i++)
		{

			DirectX::XMFLOAT3 pos((GetRandomFloat() * extentX + lowbound.x), (GetRandomFloat() *extentY + lowbound.y), (GetRandomFloat() * extentZ + lowbound.z));
			DirectX::XMFLOAT3 color(GetRandomFloat(), GetRandomFloat(), GetRandomFloat());

			PointLight light;
			light.pos = pos;
			light.color = color;
			// To do :
			// Use the light attenuation function to calculate radius.
			light.radius = GetRandomFloat() * 10;
			m_lights.push_back(light);
		}
		m_lightManager.UpdateLightBuffer(&m_lights[0], sizeof(m_lights[0]), m_lights.size(), false);
	}

	// Create a new light source.
	void AddLight()
	{

		if (m_lights.size() < MaxLight)
		{
			DirectX::XMFLOAT3 upbound = m_fbxRender.GetMaxAxis(); DirectX::XMFLOAT3 lowbound = m_fbxRender.GetMinAxis();
			float extentX = upbound.x - lowbound.x;
			float extentY = upbound.y - lowbound.y;
			float extentZ = upbound.z - lowbound.z;

			DirectX::XMFLOAT3 pos((GetRandomFloat() * extentX + lowbound.x), (GetRandomFloat() *extentY + lowbound.y), (GetRandomFloat() * extentZ + lowbound.z));
			DirectX::XMFLOAT3 color(GetRandomFloat(), GetRandomFloat(), GetRandomFloat());

			//DirectX::XMFLOAT3 pos(30.5f,2,0);
			PointLight light;
			light.pos = pos;
			light.color = color;
			light.radius = GetRandomFloat() * 10;
			m_lights.push_back(light);
			m_bEditLight = true;

		}
	}

	// Remove a light source.
	void RemoveLight()
	{
		if (m_lights.size() > 1)
		{
			m_lights.pop_back();
			m_bEditLight = true;
		}
	}

	// Regenerate all lights randomly between a fbxRender's up-bound and low-bound.
	void RandomLight(const FbxRender& render)
	{
		//(static_cast<unsigned int>(time(NULL)));
		for (unsigned int i = 0; i < m_lights.size(); i++)
		{
			DirectX::XMFLOAT3 upbound = render.GetMaxAxis(); DirectX::XMFLOAT3 lowbound = render.GetMinAxis();
			float extentX = upbound.x - lowbound.x;
			float extentY = upbound.y - lowbound.y;
			float extentZ = upbound.z - lowbound.z;

			DirectX::XMFLOAT3 pos((GetRandomFloat() * extentX + lowbound.x), (GetRandomFloat() *extentY + lowbound.y), (GetRandomFloat() * extentZ + lowbound.z));
			DirectX::XMFLOAT3 color(GetRandomFloat(), GetRandomFloat(), GetRandomFloat());

			PointLight light;
			light.pos = pos;
			light.color = color;
			light.radius = GetRandomFloat() * 10;
			m_lights[i] = light;
		}

		m_bEditLight = true;
	}

	// Create a white texture.
	void CreateWhiteTexture()
	{
		std::string path = "Arts\\white.png";
		HRESULT hr = CreateWICTextureFromFileEx(g_d3dObjects->GetD3DDevice(), std::wstring(path.begin(), path.end()).c_str(), 0, 0, 0, 0, 0, &m_whiteTexture);
	}

	// Initialization stage.
	void Setup()
	{

		// Data loading tasks.
#if !SINGLETHREADED
		std::thread shaderLoader = std::thread(&directxApp::ShaderDataLoadWorkThread, this, "Shaders");
		std::thread modelLoader = std::thread(&directxApp::ModelDataLoadInitThread, this, "Arts\\sibenik.FBX", 1.25f);
#else
		//Initialize CPU Resources
		g_ShaderManager.LoadFolder("Shaders");   
	
		m_fbxRender.LoadModel("Arts\\sibenik.FBX", 1.25f);
#endif
			
		
		// Initialize camera.
		m_camera.InitProjMatrix(3.14f*0.45f, static_cast<float>(m_iWidth), static_cast<float>(m_iHeight), 0.1f, 1000.0f);


		// Initialize pure D3D12 components (no data dependency).
		m_computeEngine.Init(D3D12_COMMAND_LIST_TYPE_DIRECT, 3);	// Unable to profile LIST_TYPE_COMPUTE, previous driver versions could.
		m_bundleAllocator.Init(D3D12_COMMAND_LIST_TYPE_BUNDLE, 1);
		m_computeEngine.InitCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, m_computeCommandList);
		m_computeCommandList->Close();
		m_computeProfiler.Init(GetDeviceResources()->GetD3DDevice(), 16);
		m_profiler.Init(GetDeviceResources()->GetD3DDevice(), 16);
		m_lightManager.Init(MaxLight, sizeof(PointLight));

		// Create a white texture for my material manager.
		// If it doesn't find a texture for a material, it can use the white texture.
		CreateWhiteTexture();
		m_materialManager.AddResource(m_whiteTexture, "Default");

		ThrowIfFailed(GetDeviceResources()->GetD3DDevice()->CreateFence(m_uCurFenCounter, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fenceComputeThread)));

		// Create a main command list for this program.
		ThrowIfFailed(GetDeviceResources()->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_d3dObjects->GetCommandAllocator(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf())));
		ThrowIfFailed(m_commandList->Close());

		ThrowIfFailed(GetDeviceResources()->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_d3dObjects->GetCommandAllocator(), nullptr, IID_PPV_ARGS(m_depthPrePassList.GetAddressOf())));
		ThrowIfFailed(m_depthPrePassList->Close());
		// Initialize the UI manager.
		// Send the resources of back buffer to the UI manager.
		m_uiManager.Init();
		std::vector<ID3D12Resource *> RTVs;
		for (unsigned int i = 0; i < GetDeviceResources()->GetFramCounter(); i++)
		{
			RTVs.push_back(GetDeviceResources()->GetRenderTarget(i));
		}
		m_uiManager.CreateD2dData(GetDeviceResources()->GetD3DDevice(), GetDeviceResources()->GetCommandQueue(), RTVs);

		// Wait until shaders loading is finished.
#if !SINGLETHREADED
		shaderLoader.join();
#endif
		// Initialize D3D12 techniques.
		// Originally, a clustered lighting manager supports multi-depth. However, after I modified the light accumulation method,
		// it doesn't support Depth>1.
		m_clusteredManager.Init(TileSize, TileSize, 1); 
		m_deferredTech.Init();
		m_deferredTech.InitTraditionalTileBased();
		m_lightPreviewer.Init();
		
		// Wait until models loading is finished.
#if !SINGLETHREADED
		modelLoader.join();
#endif
		// Create GPU resource for this FBX model.
		m_fbxRender.CreateGpuResources(m_materialManager);

		// Randomly generate lights with model information.
		RandomLights(m_fbxRender.GetMaxAxis(), m_fbxRender.GetMinAxis());// Use FBX bounding boxes to generate lights.

		 // Update the camera data.
		m_camera.Position(DirectX::XMFLOAT3(-15,-14,0));
		m_camera.Target(DirectX::XMFLOAT3(0, -15,0));
		m_camera.Update();
		
	
		m_cameraData.MVP = m_camera.ProjView();
		m_cameraData.InvPV = m_camera.InvScreenProjView();
		m_cameraData.CamPos = m_camera.Position();
		m_cameraData.ProjInv = m_camera.InvProj();
		m_cameraData.Proj = m_camera.Proj();
		m_cameraData.View = m_camera.View();

		m_deferredTech.UpdateConstantBuffer(m_cameraData);

		// Create the depth planes for the clustered manager, when the culling stage supports multi-depth.
		// For N depth, we need N+1 planes for light culling.
		float planes[2];
		DirectX::XMFLOAT4 vec(0.0f, 0.0f, m_camera.GetNearestPlane(), 1.0f);
		DirectX::XMFLOAT4 plane = GMathVF4(XMVector4Transform(GMathFV(vec), XMMatrixTranspose(GMathFM(m_cameraData.Proj))));
		planes[0] = plane.z / plane.w;
		vec = DirectX::XMFLOAT4(0.0f, 0.0f, m_camera.GetFarthestPlane(), 1.0f);
		plane = GMathVF4(XMVector4Transform(GMathFV(vec), XMMatrixTranspose(GMathFM(m_cameraData.Proj))));
		planes[1] = plane.z / plane.w;
		m_clusteredManager.UpdateDepthPlanes(planes);

		// Copy all upload D3D12 resources to default D3D12 resources .
		g_copyManager->GpuDataCopy();
		// Use default resources to replace upload resources.
		g_copyManager->RepleceResourceViews();

		// Set GPU data for different classes.

		// Input camera and light data to the light culling manager.
		m_clusteredManager.SetCameraCB(m_deferredTech.GetViewCbGpuHandle());
		m_clusteredManager.SetLightBuffer(m_lightManager.GetLightBuffer().Get(), m_lightManager.GetSrvDesc(), m_lightManager.GetLightNum());
		m_clusteredManager.SetDepthBuffer(m_deferredTech.GetDepthResource(), m_deferredTech.GetDepthSrvDesc());

		// Input material, light culling data and light buffer to the deferred renderer.
		m_deferredTech.SetMaterials(m_materialManager);
		m_deferredTech.SetLightCullingMgr(m_clusteredManager);
		m_deferredTech.SetLightBuffer(m_lightManager.GetLightBuffer().Get());

		// Input camera and light data to the light previewer.
		m_lightPreviewer.UpdateCameraBuffer(m_deferredTech.GetViewCbGpuHandle());
		m_lightPreviewer.SetLightBuffer(m_lightManager.GetLightBuffer().Get(), m_lightManager.GetLightNum());

		// Clear shaders data, because this program doesn't create new PSOs after initialization.
		g_ShaderManager.Clear();
		// Create bundles for this program.
		RecordBundle();
		// Create multithreading data.
		LoadThreadContexts();

		// Set a flag after initialization.
		m_bInit = true;
	}
	void Render()
	{
		// Delta time for profiling.
		LARGE_INTEGER m_liPerfFreq = { 0 };
		QueryPerformanceFrequency(&m_liPerfFreq);
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		double delta = (time.QuadPart - m_time.QuadPart)*1000.0f / (float)m_liPerfFreq.QuadPart;
		m_time = time;
		std::string debugStr = "CPU Time:" + std::to_string(delta) + "ms\n";
		OutputDebugStringA(debugStr.c_str());

		// Rendering code.
		auto commandQueue = GetDeviceResources()->GetCommandQueue();
		PIXBeginEvent(commandQueue, 0, L"Render");
		{
			// Reset the command allocator.
			ThrowIfFailed(g_d3dObjects->GetCommandAllocator()->Reset());
			// Rest the pre-depth command list.
			ThrowIfFailed(m_depthPrePassList->Reset(g_d3dObjects->GetCommandAllocator(), m_deferredTech.GetPso()));
			
			D3D12_RECT rect = { 0, 0, m_iWidth, m_iHeight };
			// Reset viewport.
			m_depthPrePassList->RSSetScissorRects(1, &rect);
			m_depthPrePassList->RSSetViewports(1, &g_d3dObjects->GetScreenViewport());
			// Clear render targets for deferred shading.
			m_deferredTech.ClearGbuffer(m_depthPrePassList.Get());
			// Set depth buffer.
			m_depthPrePassList->OMSetRenderTargets(0, nullptr, true, m_deferredTech.GetDsvHandle());
			m_deferredTech.ApplyDepthPassPso(m_depthPrePassList.Get(), false);
			
			m_fbxRender.Render(m_depthPrePassList.Get());

			m_depthPrePassList->Close();

			ID3D12CommandList* ppPreCommandLists[1] = { m_depthPrePassList.Get() };
			commandQueue->ExecuteCommandLists(_countof(ppPreCommandLists), ppPreCommandLists);
			g_d3dObjects->WaitForGPU();
			
			// A signal to execute the light culling stage.
			commandQueue->Signal(m_fenceComputeThread.Get(), m_uCurFenCounter + 101);
			m_uCurFenCounter++;

			// Rest the rendering command list.
			ThrowIfFailed(m_commandList->Reset(g_d3dObjects->GetCommandAllocator(), m_deferredTech.GetPso()));
	
			// Reset back buffer render targets.
			AddResourceBarrier(m_commandList.Get(), g_d3dObjects->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			float clearColor[4] = { 0.0f,0.0f,0.0f,0.0f };
			m_commandList->ClearRenderTargetView(g_d3dObjects->GetRenderTargetView(), clearColor, 0, nullptr);
			// Clear deferred shading render targets.
			m_deferredTech.ClearGbuffer(m_commandList.Get());
			
			// Reset viewport.
			m_commandList->RSSetScissorRects(1, &rect);
			m_commandList->RSSetViewports(1, &g_d3dObjects->GetScreenViewport());
			m_deferredTech.SetGbuffer(m_commandList.Get());

			m_deferredTech.SetDescriptorHeaps(m_commandList.Get());
			// Run the bundle for G-buffer creation.
			m_commandList->ExecuteBundle(m_GBufferBundle.Get());

			// Set back buffer as render targets.
			m_commandList->OMSetRenderTargets(1, &g_d3dObjects->GetRenderTargetView(), true, nullptr);

			// Convert GPU resources for the light accumulation stage.
			m_deferredTech.RtvToSrv(m_commandList.Get());

			AddResourceBarrier(m_commandList.Get(),m_clusteredManager.GetClusteredBuffer(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_GENERIC_READ);
			if (m_bDebugMode)
			{
				// Visualization of the number of lights.
				m_deferredTech.ApplyDebugPso(m_commandList.Get());
			}
			else {
				if (UseTriLightCulling)
				{
					// Apply triangle-based light accumulation.
					m_deferredTech.ApplyLightAccumulationPso(m_commandList.Get());
				}
				else
				{
					// Apply tile-based light accumulation.
					m_deferredTech.ApplyTradLightAccumulation(m_commandList.Get());
				}

			}

			// Start profiling light accumulation.
			m_profiler.StartTime(m_commandList.Get());
			if (m_bDebugMode)
			{
				// Render a plane mesh.
				m_clusteredManager.GetQuadRenderer().RenderTiles(m_commandList.Get());
			}
			else
			{
				if (UseTriLightCulling)
				{
					// Render a plane mesh.
					m_clusteredManager.GetQuadRenderer().RenderTiles(m_commandList.Get());
				}
				else {
					// Render a quad.
					m_deferredTech.RenderQuad(m_commandList.Get());
				}
			}

			// End profiling light accumulation.
			m_profiler.EndTime(m_commandList.Get(), "LightPass");
			m_profiler.ResolveTimeDelta(m_commandList.Get());
			AddResourceBarrier(m_commandList.Get(), m_clusteredManager.GetClusteredBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// Draw the positions of lights.
			if (m_bLightDebugMode && !m_bDebugMode)
			{
				// Enable depth test.
				AddResourceBarrier(m_commandList.Get(), m_deferredTech.GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				m_commandList->OMSetRenderTargets(1, &g_d3dObjects->GetRenderTargetView(), true, m_deferredTech.GetDsvHandle());
				// Render spheres to represent lights.
				m_lightPreviewer.Render(m_commandList.Get());
			}
		
			// It doesn't need to add commands to convert render targets to PRESENT state,
			// because I use D2D1 to render UI, which converts render targts to PRESENT state automatically.
			// AddResourceBarrier(mCommandList.Get(), g_d3dObjects->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			m_deferredTech.SrvToRtv(m_commandList.Get());
			ThrowIfFailed(m_commandList->Close());
			ID3D12CommandList* ppCommandLists[1] = { m_commandList.Get() };
			
			commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	
			std::wstring centerInfomation;
			if (m_bSwitchingScene) {
				centerInfomation = L"Loading Model...";
			};

			m_uiManager.RenderUI(g_d3dObjects->GetCurrentCounter(), CreateInfoText(), centerInfomation);
			
			g_d3dObjects->Present();
			
			// Get profiling data.
			UINT64 freq;
			commandQueue->GetTimestampFrequency(&freq);
			m_profiler.CopyTimeDelta(freq);
			// Save debug data.
			m_dLightingTimeInfo = m_profiler.GetProfileTime(0);

		}
		PIXEndEvent(commandQueue);

	}
	void CreateWindowSizeDependentResources()
	{
		// Recreate window size dependent data.
		m_camera.OnResize(static_cast<float>(m_iWidth), static_cast<float>(m_iHeight));

		// Recreate GPU data.
		std::vector<ID3D12Resource*> RTVs;
		for (unsigned int i = 0; i < GetDeviceResources()->GetFramCounter(); i++)
		{
			RTVs.push_back(GetDeviceResources()->GetRenderTarget(i));
		}
		m_uiManager.CreateD2dData(GetDeviceResources()->GetD3DDevice(), GetDeviceResources()->GetCommandQueue(), RTVs);

		m_deferredTech.InitWindowSizeDependentResources();
		m_clusteredManager.InitWindowSizeDependentResources();
		
		m_deferredTech.SetLightCullingMgr(m_clusteredManager);
		m_clusteredManager.SetDepthBuffer(m_deferredTech.GetDepthResource(), m_deferredTech.GetDepthSrvDesc());
	}
	void Update()
	{

		auto commandQueue = GetDeviceResources()->GetCommandQueue();
		PIXBeginEvent(commandQueue, 0, L"Update");
		{
			// Window size change?
			if (m_bResize)
			{
				CreateWindowSizeDependentResources();
				m_bResize = false;
			}
			// Light data change?
			if (m_bEditLight)
			{
				m_lightManager.UpdateLightBuffer(&m_lights[0], sizeof(m_lights[0]), m_lights.size(), false);
				m_clusteredManager.SetLightBuffer(m_lightManager.GetLightBuffer().Get(), m_lightManager.GetSrvDesc(), m_lightManager.GetLightNum());
				m_lightPreviewer.SetLightBuffer(m_lightManager.GetLightBuffer().Get(), m_lightManager.GetLightNum());
				m_bEditLight = false;
			}
			// Finish loading data?
			if (m_bSwitchSceneFinished)
			{
				m_fbxRender.CreateGpuResources(m_materialManager);
				m_deferredTech.SetMaterials(m_materialManager);
				RandomLight(m_fbxRender);
				m_camera.Position(m_fbxRender.GetCenter());
				m_bBundleEdit = true;
				m_bSwitchSceneFinished = false;
				m_bSwitchingScene = false;
			}

			// Update camera.
			m_camera.Update();
			m_cameraData.MVP = m_camera.ProjView();
			m_cameraData.InvPV = m_camera.InvScreenProjView();
			m_cameraData.CamPos = m_camera.Position();
			m_cameraData.ProjInv = m_camera.InvProj();
			m_cameraData.Proj = m_camera.Proj();
			m_cameraData.View = m_camera.View();
			m_deferredTech.UpdateConstantBuffer(m_cameraData);

			// Save Debug data.
			m_iLightNumberInfo = m_lights.size();
#if !SINGLETHREADED

			// Notify to run computing thread.
			m_computeCV.notify_one();

			// After copying, we need to replace GPU data in the main thread to avoid exception.
			if (m_copyThread.joinable())
			{
				// Finish copying.
				m_copyThread.join();
				g_copyManager->RepleceResourceViews();
				// Recreate bundles, because the GPU resources change.
				if (m_bBundleEdit)
				{
					RecordBundle();
					m_bBundleEdit = false;
				}
			}

			// Run a thread to copy GPU resources.
			if (g_copyManager->GetElementNum() > 0)
			{
				m_copyThread = std::thread(&directxApp::GpuCopyWorkThread, this, this);
			}

#else
			UpdateClusteredLightCB();
			g_copyManager->GpuDataCopy();
			g_copyManager->RepleceResourceViews();
			if (m_bBundleEdit)
			{
				RecordBundle();
				m_bBundleEdit = false;
			}
#endif
		}

		PIXEndEvent(commandQueue);

	}
	void Resize(UINT width, UINT height)
	{
	

		if (width != 0 && height != 0)
		{
			m_iWidth = width;
			m_iHeight = height;

			// D2D1 resources have to be released before recreating back-buffer, or an exception happens.
			m_uiManager.Reset();
			GetDeviceResources()->SetWindows(m_hwnd, width, height);
			m_bResize = true;
		}

	}

	// Mouse and Keyboard Input.
	void KeyDown(UINT key)
	{
		m_camera.KeyDown(key);
		// P key.
		if (key == 0x50)
		{
			m_bDebugMode = !m_bDebugMode;
		}
		// L key.
		if (key == 0x4C)
		{
			m_bLightDebugMode = !m_bLightDebugMode;

		}
		// R key.
		if (key == 0x52)
		{
			RandomLight(m_fbxRender);

		}
		if (key == VK_UP)
		{
			AddLight();

		}
		if (key == VK_DOWN)
		{
			RemoveLight();

		}
		// Key1.
		if (key == 0x31)
		{

			if (!m_bSwitchingScene)
			{

				SwitchScene("Arts\\sponz.FBX", 2.2f);

			}

		}
		// Key2.
		if (key == 0x32)
		{
			if (!m_bSwitchingScene)
			{

				SwitchScene("Arts\\conference.FBX", 0.025f);


			}

		}
		// Key3.
		if (key == 0x33)
		{
			if (!m_bSwitchingScene)
			{
				SwitchScene("Arts\\sibenik2.FBX", 1.25f);
			}
		}


		return;
	}
	void KeyUp(UINT key)
	{

		return;
	}
	void MouseMove(UINT xpos, UINT ypos)
	{

		m_camera.InputMove(static_cast<float>(xpos), static_cast<float>(ypos));
		return;
	}
	void MousePress(UINT xpos, UINT ypos)
	{
		m_camera.InputPress(static_cast<float>(xpos), static_cast<float>(ypos));
		return;
	}
	void MouseRelease(UINT key)
	{

		return;
	}


};


_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow){
	directxApp App;
	return App.Run(hInstance, nCmdShow);
}
