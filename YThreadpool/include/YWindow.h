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
	 * @function SendCallback : �򴰿ڷ���һ��YWMSimpleCallback��Ϣ,���ȴ����ڹ��̴�����Ϣ.YWMSimpleCallback��Ϣִ�в���fun��ʾ�Ŀɵ��ö���.
	 * @param func : ��������ΪLRESULT�Ŀɵ��ö���.
	 * @param args : ���ݸ�func�Ĳ���.
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
	 * @function PostCallback : �򴰿ڷ���һ��YWMSimpleCallback��Ϣ����������.YWMSimpleCallback��Ϣִ�в���fun��ʾ�Ŀɵ��ö���.
	 * @param func : ��������ΪLRESULT�Ŀɵ��ö���.
	 * @param args : ���ݸ�func�Ĳ���.
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
 * @class YWindowTimer : ���ڼ�ʱ���İ�װ��
 */

class YWindowTimer
{
public:
	/*!
	 * @function Create : �������ڼ�ʱ��.
	 * @param hWnd : ���ھ��.
	 * @param uElapse �� ��ʱ��ʱ�������Ժ���Ϊ��λ.
	 * @param func : �ɵ��ö��󣬼�ʱ������ʱִ��.
	 * @param args : ���ݸ�func�Ĳ���.
	 * @����ĵ� : https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer
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
