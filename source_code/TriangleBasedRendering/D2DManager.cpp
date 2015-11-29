//--------------------------------------------------------------------------------------
// File: D2DManager.cpp
//--------------------------------------------------------------------------------------
#include "D2DManager.h"
#include "d3dx12.h"

using namespace Microsoft::WRL;
D2DManager::~D2DManager()
{
	for (UINT n = 0; n < m_uFrameCount; n++)
	{

		m_wrappedBackBuffers[n].Reset();
		m_d2dRenderTargets[n].Reset();
	}

	m_d3d11On12Device.Reset();
	m_d3d11DeviceContext.Reset();
	m_textBrush.Reset();

	m_centerBrush.Reset();
	m_d2dDeviceContext.Reset();
	m_d2dDevice.Reset();
	m_d2dFactory.Reset();
	m_dWriteFactory.Reset();
}
void D2DManager::Init()
{

	D2D1_FACTORY_OPTIONS d2dFactoryOptions = {};

#ifdef _DEBUG
	// Enable the D2D debug layer.
	d2dFactoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif


	ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &d2dFactoryOptions, &m_d2dFactory));
	ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_dWriteFactory));
}

void D2DManager::CreateD2dData(ID3D12Device*  dxgi, ID3D12CommandQueue* commandQueue, const std::vector<ID3D12Resource*>& resources)
{
	D2D1_FACTORY_OPTIONS d2dFactoryOptions = {};
	UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	//d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	ComPtr<ID3D11Device> d3d11Device;
	ThrowIfFailed(D3D11On12CreateDevice(
		dxgi,
		d3d11DeviceFlags,
		nullptr,
		0,
		reinterpret_cast<IUnknown**>(&commandQueue),
		1,
		0,
		&d3d11Device,
		&m_d3d11DeviceContext,
		nullptr
		));

	// Query the 11On12 device from the 11 device.
	ThrowIfFailed(d3d11Device.As(&m_d3d11On12Device));
	ComPtr<IDXGIDevice> dxgiDevice;
	D2D1_DEVICE_CONTEXT_OPTIONS deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;
	ThrowIfFailed(m_d3d11On12Device.As(&dxgiDevice));
	ThrowIfFailed(m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice));
	ThrowIfFailed(m_d2dDevice->CreateDeviceContext(deviceOptions, &m_d2dDeviceContext));


	// Query the desktop's dpi settings, which will be used to create
	// D2D's render targets.
	float dpiX;
	float dpiY;
	m_d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
		dpiX,
		dpiY
		);

	m_uFrameCount = resources.size();
	// Initialize the same number of wrapped resources as back buffers.
	m_wrappedBackBuffers.resize(m_uFrameCount);
	m_d2dRenderTargets.resize(m_uFrameCount);

	// Create a RTV, D2D render target, and a command allocator for each frame.
	for (UINT n = 0; n < m_uFrameCount; n++)
	{


		// Create a wrapped 11On12 resource of this back buffer. Since we are 
		// rendering all D3D12 content first and then all D2D content, we specify 
		// the In resource state as RENDER_TARGET - because D3D12 will have last 
		// used it in this state - and the Out resource state as PRESENT. When 
		// ReleaseWrappedResources() is called on the 11On12 device, the resource 
		// will be transitioned to the PRESENT state.
		D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
		ThrowIfFailed(m_d3d11On12Device->CreateWrappedResource(
			resources[n],
			&d3d11Flags,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT,
			IID_PPV_ARGS(&m_wrappedBackBuffers[n])
			));

		// Create a render target for D2D to draw directly to this back buffer.
		ComPtr<IDXGISurface> surface;
		ThrowIfFailed(m_wrappedBackBuffers[n].As(&surface));
		ThrowIfFailed(m_d2dDeviceContext->CreateBitmapFromDxgiSurface(
			surface.Get(),
			&bitmapProperties,
			&m_d2dRenderTargets[n]
			));

	}

	// Create D2D/DWrite objects for rendering text.
	{
		ThrowIfFailed(m_d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::WhiteSmoke), &m_textBrush));
		ThrowIfFailed(m_dWriteFactory->CreateTextFormat(
			L"Verdana",
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			20,
			L"en-us",
			&m_textFormat
			));
		ThrowIfFailed(m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
		ThrowIfFailed(m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
	}

	// Create D2D/DWrite objects for rendering text.
	{
		ThrowIfFailed(m_d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &m_centerBrush));
		ThrowIfFailed(m_dWriteFactory->CreateTextFormat(
			L"Verdana",
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			50,
			L"en-us",
			&m_centerTextFormat
			));
		ThrowIfFailed(m_centerTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
		ThrowIfFailed(m_centerTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
	}
}

void D2DManager::RenderUI(int frameIndex, std::wstring info, std::wstring centerInfo)
{

	D2D1_SIZE_F rtSize = m_d2dRenderTargets[frameIndex]->GetSize();
	D2D1_RECT_F textRect = D2D1::RectF(0, 0, rtSize.width, rtSize.height);



	// Acquire our wrapped render target resource for the current back buffer.
	m_d3d11On12Device->AcquireWrappedResources(m_wrappedBackBuffers[frameIndex].GetAddressOf(), 1);

	// Render text directly to the back buffer.
	m_d2dDeviceContext->SetTarget(m_d2dRenderTargets[frameIndex].Get());
	m_d2dDeviceContext->BeginDraw();
	m_d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
	m_d2dDeviceContext->DrawText(
		info.c_str(),
		info.size(),
		m_textFormat.Get(),
		&textRect,
		m_textBrush.Get()
		);
	if (centerInfo.size() > 0)
	{
		m_d2dDeviceContext->DrawText(
			centerInfo.c_str(),
			centerInfo.size(),
			m_centerTextFormat.Get(),
			&textRect,
			m_centerBrush.Get()
			);
	}

	// My computer (Nvidia GTX 965m) always throw exception while calling EndDraw.
	ThrowIfFailed(m_d2dDeviceContext->EndDraw());




	// Release our wrapped render target resource. Releasing 
	// transitions the back buffer resource to the state specified
	// as the OutState when the wrapped resource was created.
	m_d3d11On12Device->ReleaseWrappedResources(m_wrappedBackBuffers[frameIndex].GetAddressOf(), 1);


	// Flush to submit the 11 command list to the shared command queue.
	m_d3d11DeviceContext->Flush();
}

void D2DManager::Reset()
{
	for (UINT n = 0; n < m_uFrameCount; n++)
	{

		m_wrappedBackBuffers[n].Reset();
		m_d2dRenderTargets[n].Reset();
	}

	m_d3d11On12Device.Reset();
	m_d3d11DeviceContext.Reset();
	m_textBrush.Reset();
	m_centerBrush.Reset();
	m_d2dDeviceContext.Reset();
	m_d2dDevice.Reset();
}