//--------------------------------------------------------------------------------------
// File: TextureLoader.cpp
//--------------------------------------------------------------------------------------
#include "TextureLoader.h"
#include <wincodec.h>
#include <stdlib.h>
#include <wrl/client.h>
#include <assert.h>  
#include <memory>
static bool g_WIC2 = false;
using namespace Microsoft::WRL;

//-------------------------------------------------------------------------------------
// WIC Pixel Format Translation Data.
//-------------------------------------------------------------------------------------
struct WICTranslate
{
	GUID                wic;
	DXGI_FORMAT         format;
};
static WICTranslate g_WICFormats[] =
{
	{ GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

	{ GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

	{ GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
	{ GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

	{ GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

	{ GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
	{ GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

	{ GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
	{ GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
	{ GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
	{ GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

	{ GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
};
struct WICConvert
{
	GUID        source;
	GUID        target;
};
static D3D_FEATURE_LEVEL D3DFeatureLevels[] = { D3D_FEATURE_LEVEL_12_1 ,D3D_FEATURE_LEVEL_12_0 ,D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0 };
static WICConvert g_WICConvert[] =
{
	// Note target GUID in this conversion table must be one of those directly supported formats (above).

	{ GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 

	{ GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM 
	{ GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM 

	{ GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT 
	{ GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT 

	{ GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

	{ GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

	{ GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 

	{ GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
	{ GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
	{ GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
	{ GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
	{ GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
	{ GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 

	{ GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT 
	{ GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT 
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT 
	{ GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT 
	{ GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT 

	{ GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM 
	{ GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
{ GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
{ GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
{ GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT 
#endif

																				// We don't support n-channel formats
};

bool _IsWIC2()
{
	return g_WIC2;
}

IWICImagingFactory* _GetWIC()
{
	static IWICImagingFactory* s_Factory = nullptr;

	if (s_Factory)
		return s_Factory;

#if(_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory2,
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(IWICImagingFactory2),
		(LPVOID*)&s_Factory
		);

	if (SUCCEEDED(hr))
	{
		// WIC2 is available on Windows 8 and Windows 7 SP1 with KB 2670838 installed.
		g_WIC2 = true;
	}
	else
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory1,
			nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IWICImagingFactory),
			(LPVOID*)&s_Factory
			);

		if (FAILED(hr))
		{
			s_Factory = nullptr;
			return nullptr;
		}
	}
#else
	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(IWICImagingFactory),
		(LPVOID*)&s_Factory
		);

	if (FAILED(hr))
	{
		s_Factory = nullptr;
		return nullptr;
	}
#endif

	return s_Factory;
}

//---------------------------------------------------------------------------------
static DXGI_FORMAT _WICToDXGI(const GUID& guid)
{
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (memcmp(&g_WICFormats[i].wic, &guid, sizeof(GUID)) == 0)
			return g_WICFormats[i].format;
	}

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
	if (g_WIC2)
	{
		if (memcmp(&GUID_WICPixelFormat96bppRGBFloat, &guid, sizeof(GUID)) == 0)
			return DXGI_FORMAT_R32G32B32_FLOAT;
	}
#endif

	return DXGI_FORMAT_UNKNOWN;
}

//---------------------------------------------------------------------------------
static size_t _WICBitsPerPixel(REFGUID targetGuid)
{
	IWICImagingFactory* pWIC = _GetWIC();
	if (!pWIC)
		return 0;

	ComPtr<IWICComponentInfo> cinfo;
	if (FAILED(pWIC->CreateComponentInfo(targetGuid, cinfo.GetAddressOf())))
		return 0;

	WICComponentType type;
	if (FAILED(cinfo->GetComponentType(&type)))
		return 0;

	if (type != WICPixelFormat)
		return 0;

	ComPtr<IWICPixelFormatInfo> pfinfo;
	if (FAILED(cinfo.As(&pfinfo)))
		return 0;

	UINT bpp;
	if (FAILED(pfinfo->GetBitsPerPixel(&bpp)))
		return 0;

	return bpp;
}


//--------------------------------------------------------------------------------------
static DXGI_FORMAT MakeSRGB(_In_ DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;

	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;

	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;

	default:
		return format;
	}
}


//---------------------------------------------------------------------------------
static HRESULT CreateTextureFromWIC(_In_ ID3D12Device* d3dDevice,
	_In_ IWICBitmapFrameDecode *frame,
	_In_ size_t maxsize,
	_In_ unsigned int bindFlags,
	_In_ unsigned int cpuAccessFlags,
	_In_ unsigned int miscFlags,
	_In_ bool forceSRGB,
	_Out_opt_ ID3D12Resource** texture)
{
	UINT width, height;
	HRESULT hr = frame->GetSize(&width, &height);
	if (FAILED(hr))
		return hr;

	assert(width > 0 && height > 0);

	if (!maxsize)
	{
		// This is a bit conservative because the hardware could support larger textures than
		// the Feature Level defined minimums, but doing it this way is much easier and more
		// performant for WIC than the 'fail and retry' model used by DDSTextureLoader.

		D3D12_FEATURE_DATA_FEATURE_LEVELS features;
		features.pFeatureLevelsRequested = D3DFeatureLevels;
		features.NumFeatureLevels = 4;
		hr = d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, (void*)&features, sizeof(features));
		if (FAILED(hr))
			return hr;
		switch (features.MaxSupportedFeatureLevel)
		{
		case D3D_FEATURE_LEVEL_9_1:
		case D3D_FEATURE_LEVEL_9_2:
			maxsize = 2048 /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
			break;

		case D3D_FEATURE_LEVEL_9_3:
			maxsize = 4096 /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
			break;

		case D3D_FEATURE_LEVEL_10_0:
		case D3D_FEATURE_LEVEL_10_1:
			maxsize = 8192 /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
			break;

		default:
			maxsize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
			break;
		}
	}

	assert(maxsize > 0);

	UINT twidth, theight;
	if (width > maxsize || height > maxsize)
	{
		float ar = static_cast<float>(height) / static_cast<float>(width);
		if (width > height)
		{
			twidth = static_cast<UINT>(maxsize);
			theight = static_cast<UINT>(static_cast<float>(maxsize) * ar);
		}
		else
		{
			theight = static_cast<UINT>(maxsize);
			twidth = static_cast<UINT>(static_cast<float>(maxsize) / ar);
		}
		assert(twidth <= maxsize && theight <= maxsize);
	}
	else
	{
		twidth = width;
		theight = height;
	}

	// Determine format.
	WICPixelFormatGUID pixelFormat;
	hr = frame->GetPixelFormat(&pixelFormat);
	if (FAILED(hr))
		return hr;

	WICPixelFormatGUID convertGUID;
	memcpy(&convertGUID, &pixelFormat, sizeof(WICPixelFormatGUID));

	size_t bpp = 0;

	DXGI_FORMAT format = _WICToDXGI(pixelFormat);
	if (format == DXGI_FORMAT_UNKNOWN)
	{
		if (memcmp(&GUID_WICPixelFormat96bppRGBFixedPoint, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
		{
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
			if (g_WIC2)
			{
				memcpy(&convertGUID, &GUID_WICPixelFormat96bppRGBFloat, sizeof(WICPixelFormatGUID));
				format = DXGI_FORMAT_R32G32B32_FLOAT;
			}
			else
#endif
			{
				memcpy(&convertGUID, &GUID_WICPixelFormat128bppRGBAFloat, sizeof(WICPixelFormatGUID));
				format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
		}
		else
		{
			for (size_t i = 0; i < _countof(g_WICConvert); ++i)
			{
				if (memcmp(&g_WICConvert[i].source, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
				{
					memcpy(&convertGUID, &g_WICConvert[i].target, sizeof(WICPixelFormatGUID));

					format = _WICToDXGI(g_WICConvert[i].target);
					assert(format != DXGI_FORMAT_UNKNOWN);
					bpp = _WICBitsPerPixel(convertGUID);
					break;
				}
			}
		}

		if (format == DXGI_FORMAT_UNKNOWN)
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	}
	else
	{
		bpp = _WICBitsPerPixel(pixelFormat);
	}


#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
	if ((format == DXGI_FORMAT_R32G32B32_FLOAT))
	{
		// Special case test for optional device support for autogen mipchains for R32G32B32_FLOAT.
		UINT fmtSupport = 0;



		D3D12_FEATURE_DATA_FORMAT_SUPPORT formats;
		formats.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		formats.Support1 = D3D12_FORMAT_SUPPORT1_TEXTURE2D;
		hr = d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, (void*)&formats, sizeof(formats));
		if (FAILED(hr))
		{
			// Use R32G32B32A32_FLOAT instead which is required for Feature Level 10.0 and up.
			memcpy(&convertGUID, &GUID_WICPixelFormat128bppRGBAFloat, sizeof(WICPixelFormatGUID));
			format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			bpp = 128;
		}
	}
#endif

	if (!bpp)
		return E_FAIL;

	// Handle sRGB formats.
	if (forceSRGB)
	{
		format = MakeSRGB(format);
	}
	else
	{
		ComPtr<IWICMetadataQueryReader> metareader;
		if (SUCCEEDED(frame->GetMetadataQueryReader(metareader.GetAddressOf())))
		{
			GUID containerFormat;
			if (SUCCEEDED(metareader->GetContainerFormat(&containerFormat)))
			{
				// Check for sRGB colorspace metadata.
				bool sRGB = false;

				PROPVARIANT value;
				PropVariantInit(&value);

				if (memcmp(&containerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
				{
					// Check for sRGB chunk.
					if (SUCCEEDED(metareader->GetMetadataByName(L"/sRGB/RenderingIntent", &value)) && value.vt == VT_UI1)
					{
						sRGB = true;
					}
				}
				else if (SUCCEEDED(metareader->GetMetadataByName(L"System.Image.ColorSpace", &value)) && value.vt == VT_UI2 && value.uiVal == 1)
				{
					sRGB = true;
				}


				PropVariantClear(&value);

				if (sRGB)
					format = MakeSRGB(format);
			}
		}
	}

	// Verify our target format is supported by the current device
	// (handles WDDM 1.0 or WDDM 1.1 device driver cases as well as DirectX 11.0 Runtime without 16bpp format support).
	D3D12_FEATURE_DATA_FORMAT_SUPPORT formats;
	formats.Format = format;
	formats.Support1 = D3D12_FORMAT_SUPPORT1_TEXTURE2D;
	hr = d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, (void*)&formats, sizeof(formats));
	if (FAILED(hr))
	{
		// Fallback to RGBA 32-bit format which is supported by all devices.
		memcpy(&convertGUID, &GUID_WICPixelFormat32bppRGBA, sizeof(WICPixelFormatGUID));
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
		bpp = 32;
	}

	// Allocate temporary memory for image.
	size_t rowPitch = (twidth * bpp + 7) / 8;
	size_t imageSize = rowPitch * theight;

	std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[imageSize]);
	if (!temp)
		return E_OUTOFMEMORY;

	// Load image data.
	if (memcmp(&convertGUID, &pixelFormat, sizeof(GUID)) == 0
		&& twidth == width
		&& theight == height)
	{
		// No format conversion or resize needed.
		hr = frame->CopyPixels(0, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
		if (FAILED(hr))
			return hr;
	}
	else if (twidth != width || theight != height)
	{
		// Resize.
		IWICImagingFactory* pWIC = _GetWIC();
		if (!pWIC)
			return E_NOINTERFACE;

		ComPtr<IWICBitmapScaler> scaler;
		hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
		if (FAILED(hr))
			return hr;

		hr = scaler->Initialize(frame, twidth, theight, WICBitmapInterpolationModeFant);
		if (FAILED(hr))
			return hr;

		WICPixelFormatGUID pfScaler;
		hr = scaler->GetPixelFormat(&pfScaler);
		if (FAILED(hr))
			return hr;

		if (memcmp(&convertGUID, &pfScaler, sizeof(GUID)) == 0)
		{
			// No format conversion needed.
			hr = scaler->CopyPixels(0, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
			if (FAILED(hr))
				return hr;
		}
		else
		{
			ComPtr<IWICFormatConverter> FC;
			hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
			if (FAILED(hr))
				return hr;

			BOOL canConvert = FALSE;
			hr = FC->CanConvert(pfScaler, convertGUID, &canConvert);
			if (FAILED(hr) || !canConvert)
			{
				return E_UNEXPECTED;
			}

			hr = FC->Initialize(scaler.Get(), convertGUID, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
			if (FAILED(hr))
				return hr;

			hr = FC->CopyPixels(0, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
			if (FAILED(hr))
				return hr;
		}
	}
	else
	{
		// Format conversion but no resize.
		IWICImagingFactory* pWIC = _GetWIC();
		if (!pWIC)
			return E_NOINTERFACE;

		ComPtr<IWICFormatConverter> FC;
		hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
		if (FAILED(hr))
			return hr;

		BOOL canConvert = FALSE;
		hr = FC->CanConvert(pixelFormat, convertGUID, &canConvert);
		if (FAILED(hr) || !canConvert)
		{
			return E_UNEXPECTED;
		}

		hr = FC->Initialize(frame, convertGUID, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
			return hr;

		hr = FC->CopyPixels(0, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
		if (FAILED(hr))
			return hr;
	}

	// Create a D3D12 resource in upload heap.
	D3D12_HEAP_PROPERTIES heapProperty;
	ZeroMemory(&heapProperty, sizeof(heapProperty));
	heapProperty.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;


	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = format;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = twidth;;
	resourceDesc.Height = theight;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	hr = d3dDevice->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(texture));
	hr = (*texture)->WriteToSubresource(0, nullptr, temp.get(), rowPitch, imageSize);

	if (FAILED(hr))
		return hr;


	return hr;
}


HRESULT CreateWICTextureFromFileEx(ID3D12Device * d3dDevice, const wchar_t * fileName, size_t maxsize, unsigned int bindFlags, unsigned int cpuAccessFlags, unsigned int miscFlags, bool forceSRGB, ID3D12Resource ** texture)
{


	if (texture)
	{
		*texture = nullptr;
	}


	if (!d3dDevice || !fileName || (!texture))
		return E_INVALIDARG;

	IWICImagingFactory* pWIC = _GetWIC();
	if (!pWIC)
		return E_NOINTERFACE;

	// Initialize WIC.
	ComPtr<IWICBitmapDecoder> decoder;
	HRESULT hr = pWIC->CreateDecoderFromFilename(fileName, 0, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
	if (FAILED(hr))
		return hr;

	ComPtr<IWICBitmapFrameDecode> frame;
	hr = decoder->GetFrame(0, frame.GetAddressOf());
	if (FAILED(hr)) {
		return hr;
	}


	hr = CreateTextureFromWIC(d3dDevice,
		frame.Get(), maxsize,
		bindFlags, cpuAccessFlags, miscFlags, forceSRGB,
		texture);

	return S_OK;
}
