#include "..\pch.h"
#include "..\include\YWindow.h"

extern HMODULE g_hModule;

static ATOM WINAPI RegisterWindowClass(LPCWSTR lpszClassName, WNDPROC lpfnWndProc, HINSTANCE hInstance)
{
	WNDCLASS WndClass;
	memset(&WndClass, 0, sizeof(WndClass));
	WndClass.lpfnWndProc = lpfnWndProc;
	WndClass.hInstance = hInstance ? hInstance : ::GetModuleHandle(NULL);
	WndClass.lpszClassName = lpszClassName;
	return ::RegisterClass(&WndClass);
}

YCallbackWindow::YCallbackWindow() : m_hWnd(NULL)
{
}

YCallbackWindow::~YCallbackWindow()
{
}

YCallbackWindow& WINAPI YCallbackWindow::SharedWindow()
{
	static YCallbackWindow instance;
	if (instance.m_hWnd == NULL)
	{
		HINSTANCE hInstance = g_hModule;
		ATOM wndclass = RegisterWindowClass(L"YCallbackWindow", WindowProc, hInstance);
		instance.m_hWnd = CreateWindow((LPCWSTR)wndclass, L"", WS_OVERLAPPED, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, 0);
	}
	return instance;
}

LRESULT YCallbackWindow::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case YWMSimpleCallback:
		PFN_INVOKE_BINDER pfn = reinterpret_cast<PFN_INVOKE_BINDER>(wParam);
		return pfn(lParam);
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL YWindowTimer::_Create(HWND hWnd, UINT uElapse)
{
	m_hWnd = hWnd;
	UINT_PTR nIDTimer = ::SetTimer(hWnd, (UINT_PTR)this, uElapse, TimerCallback);
	if (nIDTimer != (UINT_PTR)this)
	{
		delete this;
		return FALSE;
	}
	return TRUE;
}

VOID CALLBACK YWindowTimer::TimerCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	YWindowTimer* pWindowTimer = reinterpret_cast<YWindowTimer*>(idEvent);
	pWindowTimer->Execute(dwTime);
}

void YWindowTimer::Kill()
{
	KillTimer(m_hWnd, (UINT_PTR)this);
	delete this;
}
