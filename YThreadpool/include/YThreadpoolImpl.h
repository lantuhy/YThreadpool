#pragma once

/*!
 * YThreadpool是封装了Windows线程池API的C++模板库
 * 有关Windows线程池的详细内容，请参考:https://docs.microsoft.com/zh-cn/windows/win32/procthread/thread-pools
*/

#include <threadpoolapiset.h>
#include "YThreadpoolInvoker.h"
#include <array>

class YThreadpoolCallback;

typedef VOID(*YTPCleanupGroupCancelCallback)(YThreadpoolCallback*, PVOID);
typedef VOID(*YTPCallbackFinalization)(YThreadpoolCallback*);

/*!
 * @struct YTPCallbackEnviron : 线程池回调环境（callback environment）
 * @相关文档 ：https://docs.microsoft.com/zh-cn/windows/win32/api/winbase/nf-winbase-initializethreadpoolenvironment
*/
class YTPCallbackEnviron : protected TP_CALLBACK_ENVIRON
/*!
 * 保护继承,阻止非YThreadpoolCallback对象以如下方式添加到清理组:
 * YTPCallbackEnviron cbe;
 * CreateThreadpoolWork(WorkCallback, &WorkData, &cbe); // error : 从“YTPCallbackEnviron *”到“PTPSimpleCallback_ENVIRON*”的转换存在，但无法访问
*/
{
protected:
	__declspec(dllexport) YTPCallbackEnviron();
	virtual ~YTPCallbackEnviron()
	{
		DestroyThreadpoolEnvironment(this);
	}
public:
	template <YTPCleanupGroupCancelCallback func>
	void SetCleanupGroup(PTP_CLEANUP_GROUP ptpcg)
	{
		TpSetCallbackCleanupGroup(
			this,
			ptpcg, 
			CleanupGroupCancelCallbackT<func>);
	}

	template <YTPCallbackFinalization func>
	void SetCallbackFinalization()
	{
		TpSetCallbackFinalizationCallback(this, CallbackFinalizationT<func>);
	}

	void SetCleanupGroupWith(const YTPCallbackEnviron& cbe)
	{
		SetThreadpoolCallbackCleanupGroup(this, cbe.CleanupGroup, cbe.CleanupGroupCancelCallback);
	}

	void SetCallbackFinalizationWith(const YTPCallbackEnviron& cbe)
	{
		TpSetCallbackFinalizationCallback(this, cbe.FinalizationCallback);
	}
//
	friend YThreadpoolCallback;
private:
	template <YTPCleanupGroupCancelCallback func>
	static VOID WINAPI CleanupGroupCancelCallbackT(PVOID ObjectContext, PVOID CleanupContext);
	
	template <YTPCallbackFinalization func>
	static VOID WINAPI CallbackFinalizationT(PTP_CALLBACK_INSTANCE Instance, PVOID ObjectContext);
};

class YTPConcurrenceEnviron : public YTPCallbackEnviron
{
public:
	void SetPriority(TP_CALLBACK_PRIORITY priority)
	{
		SetThreadpoolCallbackPriority(this, priority);
	}
	void SetCallbackRunsLong()
	{
		SetThreadpoolCallbackRunsLong(this);
	}
	void SetCallbackPersistent()
	{
		SetThreadpoolCallbackPersistent(this);
	}
	__declspec(dllexport) static const YTPCallbackEnviron* Default();
};

struct YTPSingleThreadedEnviron : public YTPCallbackEnviron
{
public:
	__declspec(dllexport) static YTPSingleThreadedEnviron* Create();
	__declspec(dllexport) YTPSingleThreadedEnviron* Copy();
	__declspec(dllexport) void Destroy();

private:
	YTPSingleThreadedEnviron();
	YTPSingleThreadedEnviron(const YTPCallbackEnviron& cbe);
	~YTPSingleThreadedEnviron();
	BOOL SetThreadpool();
	void ReleaseThreadpool();

//
	volatile long* m_pRefCountOfPool;
};

/*!
 * @class YThreadpoolCallback : 线程池回调对象包装器的基类
*/
class YThreadpoolCallback
{
	template <YTPCallbackFinalization func>
	friend VOID WINAPI YTPCallbackEnviron::CallbackFinalizationT(PTP_CALLBACK_INSTANCE Instance, PVOID ObjectContext);

	template <YTPCleanupGroupCancelCallback func>
	friend VOID WINAPI  YTPCallbackEnviron::CleanupGroupCancelCallbackT(PVOID ObjectContext, PVOID CleanupContext);
public:
	YThreadpoolCallback() :
		m_bShouldExit(FALSE)
	{
	}
	virtual ~YThreadpoolCallback()
	{
	}
	/*!
	 * @function Close : 关闭回调对象.
	*/
	virtual void Close() = 0;

	/*!
	 * @function : 等待回调对象完成
	 * @param fCancelPendingCallbacks : 是否取消已经进入队列的但尚未执行的回调对象
	*/
	virtual void Wait(BOOL fCancelPendingCallbacks) = 0;

	/*!
	 * @function Exit : 设置"Exit"标志。当在回调函数中检测到"Exit"标志时,应尽快地从函数中退出.
	*/
	void Exit()
	{
		m_bShouldExit = TRUE;
	}

	BOOL ShouldExit() const 
	{
		return m_bShouldExit;
	}
protected:
	virtual void Destroy() = 0;

	static const TP_CALLBACK_ENVIRON* GetCallbackEnviron(const YTPCallbackEnviron* cbe)
	{
		return static_cast<const TP_CALLBACK_ENVIRON*>(cbe);
	}

	BOOL m_bShouldExit;
};

template <YTPCleanupGroupCancelCallback func>
VOID WINAPI YTPCallbackEnviron::CleanupGroupCancelCallbackT(PVOID ObjectContext, PVOID CleanupContext)
{
	auto CallbackObject = static_cast<YThreadpoolCallback*>(ObjectContext);
	func(CallbackObject, CleanupContext);
}

template <YTPCallbackFinalization func>
VOID WINAPI YTPCallbackEnviron::CallbackFinalizationT(PTP_CALLBACK_INSTANCE/* Instance*/, PVOID ObjectContext)
{
	auto CallbackObject = static_cast<YThreadpoolCallback*>(ObjectContext);
	func(CallbackObject);
	CallbackObject->Destroy();
}

template <typename TPCallbackObjectPtr>
class YThreadpoolCallbackT : public YThreadpoolCallback
{
protected:
	YThreadpoolCallbackT() :
		YThreadpoolCallback(),
		m_ptpCallbackObject(nullptr)
	{
	}
	__declspec(dllexport) void Destroy() override;
	TPCallbackObjectPtr m_ptpCallbackObject;
};

////////
template <typename Func, typename... Args>
class YThreadpoolSimpleCallbackT;

class YThreadpoolSimpleCallback : public YThreadpoolCallback
{
public:
	/*!
	 * @function TrySubmit : 请求线程池的工作线程调用指定的可调用对象 
	 * @param cbe	: 线程池回调环境
	 * @param func	: 可调用对象 
	 * @param args	: 传递给func的参数
	 * @remarks     : func的形参列表：(Args..., PTP_CALLBACK_INSTANCE)
	 * @相关文档	    : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-trysubmitthreadpoolcallback
	*/
	template <typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<PTP_CALLBACK_INSTANCE>>
	static BOOL TrySubmit(const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		auto pSimpleCallback = new YThreadpoolSimpleCallbackT<Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		BOOL bRet = pSimpleCallback->TrySubmit(cbe);
		if (!bRet)
			pSimpleCallback->Destroy();
		return bRet;
	}
	__declspec(dllexport) void Wait(BOOL fCancelPendingCallbacks) override;
protected:
	__declspec(dllexport) void Close() override;
	__declspec(dllexport) void Destroy() override;
	virtual void Execute(PTP_CALLBACK_INSTANCE pci) = 0;
private:
	__declspec(dllexport) BOOL TrySubmit(const YTPCallbackEnviron* cbe);
	static void WINAPI SimpleCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context);
};

template <typename Func, typename... Args>
class YThreadpoolSimpleCallbackT :
	public YThreadpoolSimpleCallback,
	private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolSimpleCallbackT(Func&& func, Args&& ... args) :
		YThreadpoolSimpleCallback(),
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
	void Execute(PTP_CALLBACK_INSTANCE pci) override
	{
		this->Invoke(pci);
	}
};

////////
template <class BaseClass, typename Func, typename... Args>
class YThreadpoolWorkT;

/*!
 * @class YThreadpoolWork : 工作对象(work object)的包装器
*/
class YThreadpoolWork : public YThreadpoolCallbackT<PTP_WORK>
{
public:
	/*!
	 * @function Create : 创建工作对象(work object)
	 * @param cbe	: 线程池回调环境
	 * @param func  : 工作线程执行的可调用对象.
	 * @param args	: 传递给func的参数
	 * @remarks     : func的形参列表：(Args..., PTP_CALLBACK_INSTANCE, BaseClass*)
	 * @相关文档		: https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwork
	*/
	template <class BaseClass = YThreadpoolWork, typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<PTP_CALLBACK_INSTANCE, BaseClass*> >
	static BaseClass* Create(const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		auto pWork = new YThreadpoolWorkT<BaseClass, Func, Args...>(forward<Func>(func), forward<Args>(args)...);
		if (!pWork->_Create(cbe))
		{
			pWork->Destroy();
			pWork = nullptr;
		}
		return pWork;
	}

	void Submit()
	{
		SubmitThreadpoolWork(m_ptpCallbackObject);
	}
	/*!
	 * @function Close : 关闭工作对象.
	 * @相关文档	 : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwork
    */
	__declspec(dllexport) void Close() override;
	__declspec(dllexport) void Wait(BOOL fCancelPendingCallbacks) override;
protected:
	__declspec(dllexport) BOOL _Create(const YTPCallbackEnviron* cbe);
	virtual void Execute(PTP_CALLBACK_INSTANCE pci) = 0;
private:
	static VOID NTAPI WorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);
};

template <class BaseClass, typename Func, typename... Args>
class YThreadpoolWorkT : 
	public BaseClass,
	private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolWorkT(Func&& func, Args&& ... args) :
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	void Execute(PTP_CALLBACK_INSTANCE Instance) override
	{
		this->Invoke(Instance, this);
	}
};

////////
template <class BaseClass, typename Func, typename... Args>
class YThreadpoolWaitT;

/*!
 * @class YThreadpoolWait : 等待对象(wait object)包装器
*/
class YThreadpoolWait : public YThreadpoolCallbackT<PTP_WAIT>
{
public:
	/*!
	 * @function Create : 创建等待对象.
	 * @param cbe : 线程池回调环境
	 * @param func : 当等待完成或超时由线程池执行的可调用对象.
	 * @param args : 传递给func的参数
	 * @remarks : func的形参列表：(Args...,PTP_CALLBACK_INSTANCE, BaseClass*, TP_WAIT_RESULT WaitResult)
	 * @相关文档 : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolwait
	*/

	template <class BaseClass = YThreadpoolWait, typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<PTP_CALLBACK_INSTANCE, BaseClass*, TP_WAIT_RESULT> >
	static BaseClass* Create(const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		auto pWait = new YThreadpoolWaitT<BaseClass, Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		if (!pWait->_Create(cbe))
		{
			pWait->Destroy();
			pWait = nullptr;
		}
		return pWait;
	}
	void SetWait(HANDLE h, PFILETIME pftTimeout)
	{
		SetThreadpoolWait(m_ptpCallbackObject, h, pftTimeout);
	}
	void SetWait(HANDLE h, DWORD dwMillseconds)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG) ((LONGLONG)dwMillseconds * -10000);
		FILETIME ftDueTime;
		ftDueTime.dwHighDateTime = ulDueTime.HighPart;
		ftDueTime.dwLowDateTime = ulDueTime.LowPart;
		SetThreadpoolWait(m_ptpCallbackObject, h, &ftDueTime);
	}
	/*!
	 * @function Close : 关闭等待对象.
	 * @相关文档	 : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwait
	*/
	__declspec(dllexport) void Close() override;
	__declspec(dllexport) void Wait(BOOL fCancelPendingCallbacks) override;
protected:
	__declspec(dllexport) BOOL _Create(const YTPCallbackEnviron* cbe);
	virtual void Execute(PTP_CALLBACK_INSTANCE Instance, TP_WAIT_RESULT WaitResult) = 0;
private:

	static VOID CALLBACK WaitCallback(
		PTP_CALLBACK_INSTANCE Instance,
		PVOID                 Context,
		PTP_WAIT              Wait,
		TP_WAIT_RESULT        WaitResult
	);
};

template <class BaseClass, typename Func, typename... Args>
class YThreadpoolWaitT : public BaseClass, private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolWaitT(Func&& func, Args&& ... args) :
		BaseClass(),
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	virtual void Execute(PTP_CALLBACK_INSTANCE Instance, TP_WAIT_RESULT WaitResult) override
	{
		this->Invoke(Instance, this, WaitResult);
	}
};

////////
template <class BaseClass, typename Func, typename... Args>
class YThreadpoolTimerT;

/*!
 * @class YThreadpoolTimer : 计时器对象(timer object)包装器
 */
class YThreadpoolTimer : public YThreadpoolCallbackT<PTP_TIMER>
{
public:
	/*!
	 * @function Create : 创建计时器对象
	 * @param cbe : 线程池回调环境
	 * @param func : 每次定时器到期时执行的可调用对象.
	 * @param args : 传递给func的参数
	 * @remarks : func的形参列表：(Args..., PTP_CALLBACK_INSTANCE, BaseClass*)
	 * @相关文档 : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpooltimer
	*/
	template <class BaseClass = YThreadpoolTimer, typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<PTP_CALLBACK_INSTANCE, BaseClass*> >
	static BaseClass* Create(const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		auto pTimer = new YThreadpoolTimerT<BaseClass, Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		if (!pTimer->_Create(cbe))
		{
			pTimer->Destroy();
			pTimer = nullptr;
		}
		return pTimer;
	}
	void SetTimer(
		_In_opt_ PFILETIME pftDueTime,
		_In_	 DWORD msPeriod,
		_In_opt_ DWORD msWindowLength)
	{
		SetThreadpoolTimer(m_ptpCallbackObject, pftDueTime, msPeriod, msWindowLength);
	}
	void SetTimer(_In_ DWORD msPeriod, _In_opt_ DWORD msWindowLength)
	{
		ULARGE_INTEGER ulDueTime;
		ulDueTime.QuadPart = (ULONGLONG)((LONGLONG)msPeriod * -10000);
		FILETIME ftDueTime;
		ftDueTime.dwHighDateTime = ulDueTime.HighPart;
		ftDueTime.dwLowDateTime = ulDueTime.LowPart;
		SetThreadpoolTimer(m_ptpCallbackObject, &ftDueTime, msPeriod, msWindowLength);
	}
	BOOL IsTimerSet()
	{
		return IsThreadpoolTimerSet(m_ptpCallbackObject);
	}
	/*!
	 * @function Close : 关闭计时器对象.
	 * @相关文档	 : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpooltimer
	*/
	__declspec(dllexport) void Close() override;
	__declspec(dllexport) void Wait(BOOL fCancelPendingCallbacks) override;
protected:
	__declspec(dllexport) BOOL _Create(const YTPCallbackEnviron* cbe);
	virtual void Execute(PTP_CALLBACK_INSTANCE pci) = 0;
private:
	static VOID CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer);
};

template <class BaseClass, typename Func, typename... Args>
class YThreadpoolTimerT : public BaseClass, private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolTimerT(Func&& func, Args&& ... args) :
		BaseClass(),
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	void Execute(PTP_CALLBACK_INSTANCE pci) override
	{
		this->Invoke(pci, this);
	}
};


////////
template <class BaseClass, typename Func, typename... Args>
class YThreadpoolIoT;

/*!
 * @class YThreadpoolIoBase	: I/O完成对象(I/O completion object)包装器基类
*/
class YThreadpoolIoBase : public YThreadpoolCallbackT<PTP_IO>
{
public:
	void StartThreadpoolIo()
	{
		::StartThreadpoolIo(m_ptpCallbackObject);
	}
	void CancelThreadpoolIo()
	{
		::CancelThreadpoolIo(m_ptpCallbackObject);
	}
	/*!
	 * @function Close : 关闭I/O完成对象.
	 * @相关文档	 : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolio
	*/
	__declspec(dllexport) void Close() override;
	__declspec(dllexport) void Wait(BOOL fCancelPendingCallbacks) override;
protected:
	__declspec(dllexport) BOOL _Create(HANDLE hFile, const YTPCallbackEnviron* cbe);
	virtual void Execute(PTP_CALLBACK_INSTANCE Instance, PVOID Overlapped, ULONG nIoResult, UINT64 nNumberOfBytesTransferred) = 0;
private:
	static VOID WINAPI IoCallback(
		PTP_CALLBACK_INSTANCE Instance,
		PVOID                 Context,
		PVOID                 Overlapped,
		ULONG                 IoResult,
		ULONG_PTR             NumberOfBytesTransferred,
		PTP_IO                Io);
};

/*!
 * @class YThreadpoolIo	: I/O完成对象包装器
*/
class YThreadpoolIo : public YThreadpoolIoBase
{
protected:
	__declspec(dllexport) YThreadpoolIo();
	__declspec(dllexport) ~YThreadpoolIo();
public:
	/*
	 * @function Create : 创建I/O完成对象
	 * @param hFile	: 绑定到I/O完成对象的文件句柄
	 * @param cbe	: 线程池回调环境
	 * @param func  : 每次重叠I/O操作完成时执行的可调用对象
	 * @param args	: 传递给func的参数
	 * @remarks		: func的形参列表：(Args..., PTP_CALLBACK_INSTANCE, BaseClass*, PVOID Overlapped, ULONG IoResult, UINT64 NumberOfBytesTransferred)
	 * @相关文档		: https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolio
	*/
	template <class BaseClass = YThreadpoolIo, typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<PTP_CALLBACK_INSTANCE, BaseClass*, PVOID, ULONG, UINT64>>
		static BaseClass* Create(HANDLE hFile, const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		auto pIo = new YThreadpoolIoT<BaseClass, Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		if (!pIo->_Create(hFile, cbe))
		{
			pIo->Destroy();
			pIo = nullptr;
		}
		return pIo;
	}
	__declspec(dllexport) BOOL Read(LPVOID lpBuffer, UINT32 nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped);
	__declspec(dllexport) BOOL Write(LPCVOID lpBuffer, UINT32 nNumberOfBytesToWrite, LPOVERLAPPED lpOverlapped);
protected:
	__declspec(dllexport) BOOL _Create(HANDLE hFile, const YTPCallbackEnviron* cbe);

	HANDLE m_hFile;
};

template <typename BaseClass, typename Func, typename... Args>
class YThreadpoolIoT : public BaseClass, private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolIoT(Func&& func, Args&& ... args) noexcept :
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	virtual void Execute(PTP_CALLBACK_INSTANCE Instance, PVOID Overlapped, ULONG IoResult, UINT64 NumberOfBytesTransferred) override
	{
		this->Invoke(Instance, this, Overlapped, IoResult, NumberOfBytesTransferred);
	}
};

class YOverlapped : public OVERLAPPED
{
private:
	UINT32 m_nBufferSize;
	BYTE m_lpBuffer[8];

	YOverlapped(DWORD nBufferSize = 0);
	~YOverlapped();
public:
	__declspec(dllexport) static YOverlapped* Create(UINT64 nOffset, UINT32 nBufferSize) noexcept;
	__declspec(dllexport) void Destroy();
	BYTE* Buffer()
	{
		return m_lpBuffer;
	}
	UINT32 BufferSize()const
	{
		return m_nBufferSize;
	}
	void SetOffset(UINT64 nOffset)
	{
		Offset = (DWORD)nOffset;
		OffsetHigh = (DWORD)(nOffset >> sizeof(DWORD) * 8);
	}
};

////////
template <typename Func, typename... Args>
class YThreadpoolIoReaderT;

/*!
 * @class YThreadpoolIoReader
*/
class YThreadpoolIoReader : protected YThreadpoolIo
{
public:
	/* !
	 * @function Read : 从指定偏移处读文件
	 * @param hFile	  : 绑定到I/O完成对象的文件句柄
	 * @param nOffset : 文件偏移,从此偏移出读文件.
	 * @param nNumberOfBytesToRead : 读多少个字节.指定此参数为ULONG_MAX读取到文件末尾.
	 * @param cbe	  : 线程池回调环境
	 * @param func    : 每次重叠I/O操作完成时执行的可调用对象
	 * @param args	  : 传递给func的参数
	 * @remarks		  : func的形参列表：(Args..., ULONG IoResult, BYTE* Buffer, UINT32 NumberOfBytesRead)
	 * @相关文档		  : https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolio
	*/
	template <typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<ULONG, BYTE*, UINT32>>
		static BOOL Read(HANDLE hFile, UINT64 nOffset, UINT32 nNumberOfBytesToRead, const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		auto pIo = new YThreadpoolIoReaderT<Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		return pIo->Read(hFile, nOffset, nNumberOfBytesToRead, cbe);
	}
	template <typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<ULONG, BYTE*, UINT32>>
		static BOOL Read(LPCWSTR lpszPath, UINT64 nOffset, UINT32 nNumberOfBytesToRead, const YTPCallbackEnviron* cbe, Func&& func, Args&& ... args)
	{
		HANDLE hFile = CreateFile(lpszPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
			return FALSE;
		return Read(hFile, nOffset, nNumberOfBytesToRead, cbe, forward<Func>(func), forward<Args>(args)...);
	}
protected:
	__declspec(dllexport) YThreadpoolIoReader();
	__declspec(dllexport) ~YThreadpoolIoReader();
	__declspec(dllexport) BOOL Read(HANDLE hFile, UINT64 nOffset, UINT32 nNumberOfBytesToRead, const YTPCallbackEnviron* cbe);
	__declspec(dllexport) void Execute(PTP_CALLBACK_INSTANCE Instance, PVOID Overlapped, ULONG IoResult, UINT64 NumberOfBytesTransferred) override;
	virtual void Execute2(ULONG nIoResult, PBYTE pBuffer, UINT32 nNumberOfBytesTransferred) = 0;
private:
	YOverlapped* m_pOverlapped;
};

template <typename Func, typename... Args>
class YThreadpoolIoReaderT : public YThreadpoolIoReader, private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolIoReaderT(Func&& func, Args&& ... args) noexcept :
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	virtual void Execute2(ULONG nIoResult, PBYTE pBuffer, UINT32 nNumberOfBytesTransferred) override
	{
		this->Invoke(nIoResult, pBuffer, nNumberOfBytesTransferred);
	}
};

////////
template <typename Func, typename... Args>
class YThreadpoolIoReader2T;

/*!
 * @class YThreadpoolIoReader2
*/
class YThreadpoolIoReader2 : protected YThreadpoolIo
{
public:
	/*!
	 * @function Create : 创建I/O完成对象
	 * @param hFile : 绑定到I/O完成对象的文件句柄
	 * @param nOffset : 从第nOffset字节处开始读
	 * @param nNumberOfBytesToRead	: 一共读多少字节.指定此参数为UINT64_MAX读取到文件末尾.
	 * @param nBufferSize : 缓冲区大小
	 * @param pEnviron : 线程池回调环境指针.此参数可以为nulltr.
	 * @param func	: 每次重叠I/O操作完成时执行的可调用对象
	 * @param args	: 传递给func的参数
	 * @remarks		: func的形参列表：(Args..., YThreadpoolIoReader2*, BOOL Done, ULONG IoResult, BYTE* Buffer, UINT32 NumberOfBytesRead)
	 * @相关文档		: https://docs.microsoft.com/zh-cn/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolio
	*/
	template <typename Func, typename... Args,
		typename Ret = typename YThreadpoolInvoker<Func, Args...>::template ResultOf<YThreadpoolIoReader2*, BOOL, ULONG, BYTE*, UINT32>>
		static BOOL Read(
			HANDLE hFile,
			UINT64 nOffset,
			UINT64 nNumberOfBytesToRead,
			UINT32 nBufferSize,
			YTPSingleThreadedEnviron* pEnviron,
			Func&& func,
			Args&& ... args)
	{
		auto pIoReader = new YThreadpoolIoReader2T<Func, Args...>(
			forward<Func>(func),
			forward<Args>(args)...);
		return pIoReader->Read(hFile, nOffset, nNumberOfBytesToRead, nBufferSize, pEnviron);
	}
protected:
	__declspec(dllexport) YThreadpoolIoReader2();
	__declspec(dllexport) ~YThreadpoolIoReader2();
	__declspec(dllexport) void Execute(PTP_CALLBACK_INSTANCE pInstance, PVOID pOverlapped, ULONG nIoResult, UINT64 nNumberOfBytesTransferred) override;
	virtual void Execute2(BOOL bDone, ULONG nIoResult, PBYTE pBuffer, UINT32 nNumberOfBytesTransferred) = 0;
private:
	__declspec(dllexport) BOOL Read(HANDLE hFile, UINT64 nOffset, UINT64 nNumberOfBytesToRead, UINT32 nBufferSize, YTPSingleThreadedEnviron* pCbe);

	std::array<YOverlapped*, 2> m_oq;
	UINT64 m_nOffsetBegin;
	UINT64 m_nOffsetEnd;
	UINT64 m_nOffsetCur;
	YTPSingleThreadedEnviron* m_pEnviron;
};

template <typename Func, typename... Args>
class YThreadpoolIoReader2T : public YThreadpoolIoReader2, private YThreadpoolInvoker<Func, Args...>
{
public:
	YThreadpoolIoReader2T(Func&& func, Args&& ... args) noexcept :
		YThreadpoolInvoker<Func, Args...>(forward<Func>(func), forward<Args>(args)...)
	{
	}
private:
	void Execute2(BOOL bDone, ULONG nIoResult, PBYTE pBuffer, UINT32 nNumberOfBytesTransferred) override
	{
		this->Invoke(this, bDone, nIoResult, pBuffer, nNumberOfBytesTransferred);
	}
};