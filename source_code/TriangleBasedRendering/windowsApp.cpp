//--------------------------------------------------------------------------------------
// File: windowsApp.cpp
//--------------------------------------------------------------------------------------
#include "windowsApp.h"
#include <Windowsx.h>
#include "resource.h"
windowsApp* appPointer;

LRESULT CALLBACK windowsApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		appPointer->OnDestroy();
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		appPointer->Resize(LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_KEYDOWN:
		appPointer->KeyDown(wParam);
	case WM_LBUTTONDOWN:
		appPointer->MousePress(LOWORD(lParam), HIWORD(lParam));
	case WM_LBUTTONUP:
		appPointer->MouseRelease(wParam);
	case WM_MOUSEMOVE:
		if (wParam == MK_LBUTTON)
		{
			appPointer->MouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


windowsApp::windowsApp()
{
	m_iWidth = 1600;
	m_iHeight = 900;
}

int windowsApp::Run(HINSTANCE hInstance, int     nCmdShow)
{
	appPointer = this;
	//UNREFERENCED_PARAMETER(hPrevInstance);
	//UNREFERENCED_PARAMETER(lpCmdLine);
	MSG msg;

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));
	ZeroMemory(&msg, sizeof(msg));

	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = &windowsApp::WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = "Tile-based Rendering";
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm= LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	RegisterClassEx(&wc);

	RECT rect = { 0, 0, m_iWidth, m_iHeight };
	DWORD dwStyle =  WS_OVERLAPPEDWINDOW &  ~WS_MAXIMIZEBOX;

	AdjustWindowRect(&rect, dwStyle, FALSE);
	m_hwnd = CreateWindow(wc.lpszClassName,
		wc.lpszClassName,
		dwStyle,
		CW_USEDEFAULT, 0, rect.right-rect.left, rect.bottom- rect.top,
		NULL, NULL, hInstance, NULL);
	ShowWindow(m_hwnd, nCmdShow);
	UpdateWindow(m_hwnd);

	Setup();

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

		}
		else {
			Update();
			Render();

		}
	}



	return (int)msg.wParam;
}

