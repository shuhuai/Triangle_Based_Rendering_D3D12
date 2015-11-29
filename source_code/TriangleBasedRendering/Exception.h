//--------------------------------------------------------------------------------------
// File: Exception.h
//--------------------------------------------------------------------------------------
#include <comdef.h>
#pragma once
inline void ThrowIfFailed(HRESULT hr)

{
	if (FAILED(hr))
	{
		// Set a breakpoint on this line to catch Win32 API errors..
		throw _com_error(hr);
	}
}

