#pragma once

#include <WinUser.h>
#include "YThreadpoolInvoker.h"

class YCallbackWindow
{
	YCallbackWindow();
	~YCallbackWindow();
public:
	static __declspec(dllexport) YCallbackWindow& WINAPI SharedWindow();

	/*!
	 * @function SendCallback : 向窗口发送一条YWMSimpleCallback消息,并等待窗口过程处理消息.YWMSimpleCallback消息执行参数fun表示的可调用对象.
	 * @param func : 返回类型为LRESULT的可调用对象.
	 * @param args : 传递给func的参数.
	*/
	template <typename Func, typename... Args,
		typename Ret = enable_if_t<is_convertible_v<typename YBinder<Func, Args...>::template ResultOf<>, LRESULT>, LRESULT>>
		LRESULT SendCallback(Func&& func, Args&& ... args)
	{
		auto binder = YBinderMake(forward<Func>(func), forward<Args>(args)...);
		PFN_INVOKE_BINDER pfInvoke = &InvokeBinder_NoDelete<decltype(binder)>;
		return SendMessage(m_hWnd, YWMSimpleCallback, (WPARAM)pfInvoke, (LPARAM)&binder);
	}

	/*!
	 * @function PostCallback : 向窗口发送一条YWMSimpleCallback消息并立即返回.YWMSimpleCallback消息执行参数fun表示的可调用对象.
	 * @param func : 返回类型为LRESULT的可调用对象.
	 * @param args : 传递给func的参数.
	*/
	template <typename Func, typename... Args,
		typename Ret = enable_if_t<is_convertible_v<typename YBinder<Func, Args...>::template ResultOf<>, LRESULT>, LRESULT>>
	BOOL PostCallback(Func&& func, Args&& ... args)
	{
		auto pBinder = YBinderNew(forward<Func>(func), forward<Args>(args)...);
		PFN_INVOKE_BINDER pfInvoke = &InvokeBinder_Delete<remove_pointer_t<decltype(pBinder)>>;
		return PostMessage(m_hWnd, YWMSimpleCallback, (WPARAM)pfInvoke, (LPARAM)pBinder);
	}
private:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	typedef LRESULT(CALLBACK* PFN_INVOKE_BINDER)(LPARAM);

	template <typename Binder>
	static LRESULT CALLBACK InvokeBinder_Delete(LPARAM lParam)
	{
		Binder* pBinder = reinterpret_cast<Binder*>(lParam);
		LRESULT lRet = (*pBinder)();
		delete pBinder;
		return lRet;
	}

	template <typename Binder>
	static LRESULT CALLBACK InvokeBinder_NoDelete(LPARAM lParam)
	{
		Binder* pBinder = reinterpret_cast<Binder*>(lParam);
		return (*pBinder)();
	}

	static const UINT YWMSimpleCallback = WM_USER + 1;

	HWND m_hWnd;
};


template <typename Func, typename... Args>
class YWindowTimerT;

/*!
 * @class YWindowTimer : 窗口计时器的包装器
 */

class YWindowTimer
{
public:
	/*!
	 * @function Create : 创建窗口计时器.
	 * @param hWnd : 窗口句柄.
	 * @param uElapse ： 计时器时间间隔，以毫秒为单位.
	 * @param func : 可调用对象，计时器触发时执行.
	 * @param args : 传递给func的参数.
	 * @相关文档 : https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer
	 */
	template <typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<YWindowTimer*, DWORD>>
	static YWindowTimer* Create(HWND hWnd, UINT uElapse, Func&& func, Args && ... args)
	{
		auto pWindowTimer = new YWindowTimerT<Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		return pWindowTimer->_Create(hWnd, uElapse) ? pWindowTimer : nullptr;
	}
	YWindowTimer() : m_hWnd(nullptr)
	{
	}
	virtual ~YWindowTimer()
	{
	}
	__declspec(dllexport) void Kill();
protected:
	virtual void Execute(DWORD dwTime) = 0;
private:
	__declspec(dllexport) BOOL _Create(HWND hWnd, UINT uElapse);
	static VOID CALLBACK TimerCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

	HWND m_hWnd;
};

template <typename Func, typename... Args>
class YWindowTimerT : public YWindowTimer, private YThreadpoolInvoker<Func, Args...>
{
public:
	YWindowTimerT(Func&& func, Args&& ... args) :
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	virtual void Execute(DWORD dwTime) override
	{
		YThreadpoolInvoker<Func, Args...>::Invoke(this, dwTime);
	}
};
