//--------------------------------------------------------------------------------------
// File: DirectXMathConverter.h
//
// Functions for converting XMVECTOR to XMFLOAT or XMFLOAT to XMVECTOR.
//--------------------------------------------------------------------------------------

#ifndef DirectXMathConverter
#define DirectXMathConverter

#include <DirectXMath.h>

inline DirectX::XMVECTOR GMathFV(DirectX::XMFLOAT4& val)
{
	return XMLoadFloat4(&val);
}


inline DirectX::XMVECTOR GMathFV(DirectX::XMFLOAT3& val)
{
	return XMLoadFloat3(&val);
}

inline DirectX::XMVECTOR GMathFV(DirectX::XMFLOAT2& val)
{
	return XMLoadFloat2(&val);
}

inline DirectX::XMFLOAT3 GMathVF(DirectX::XMVECTOR& vec)
{
	DirectX::XMFLOAT3 val;
	XMStoreFloat3(&val, vec);
	return val;
}

inline DirectX::XMFLOAT2 GMathVF2(DirectX::XMVECTOR& vec)
{
	DirectX::XMFLOAT2 val;
	XMStoreFloat2(&val, vec);
	return val;
}
inline auto GMathVF4(DirectX::XMVECTOR& vec)
{
	DirectX::XMFLOAT4 val;
	XMStoreFloat4(&val, vec);
	return val;
}

inline DirectX::XMMATRIX GMathFM(DirectX::XMFLOAT4X4& mat)
{
	return XMLoadFloat4x4(&mat);
}

inline DirectX::XMFLOAT4X4 GMathMF(DirectX::XMMATRIX& mat)
{
	DirectX::XMFLOAT4X4 val;
	XMStoreFloat4x4(&val, mat);
	return val;
}
#endif