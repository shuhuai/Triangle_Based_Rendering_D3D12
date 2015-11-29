//--------------------------------------------------------------------------------------
// File: D2DManager.h
//
// A class for UI rendering.
// To use Direct2D to draw UI, we need to create a DX11 device wrapped around the DX12 device.
// Before we recreate the render targets of back buffer, we should clear all components in this class, or a crash will happen.
//--------------------------------------------------------------------------------------
#pragma once
#include "D3D12.h"
#include <vector>
#include "Exception.h" 
#include <d2d1_3.h>
#include <wrl/client.h>
#include <dwrite.h>
#include <d3d11on12.h>

class D2DManager
{
public:
	// Release all components manually.
	~D2DManager();
	// Initialize factory components.
	void Init();
	// Recreate all data after Reset().
	void CreateD2dData(ID3D12Device*  dxgi, ID3D12CommandQueue* commandQueue, const std::vector<ID3D12Resource*>& resources);

	void RenderUI(int frameIndex, std::wstring info, std::wstring centerInfo = L"");
	// Clear all data related to back buffer.
	void Reset();

private:
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
	Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12Device;
	Microsoft::WRL::ComPtr<IDWriteFactory> m_dWriteFactory;
	Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
	Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dDeviceContext;

	std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> m_wrappedBackBuffers;
	std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>> m_d2dRenderTargets;

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_centerBrush;

	Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
	Microsoft::WRL::ComPtr<IDWriteTextFormat> m_centerTextFormat;

	UINT m_uFrameCount;

};