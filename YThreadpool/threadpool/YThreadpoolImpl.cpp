
#include "..\pch.h"
#include "..\include\YThreadpoolImpl.h"
#include <vector>
using namespace std;

YTPCallbackEnviron::YTPCallbackEnviron()
{
	InitializeThreadpoolEnvironment(this);
	auto func = [](YThreadpoolCallback*)
	{
	};
	SetCallbackFinalization<func>();
}

const YTPCallbackEnviron* YTPConcurrenceEnviron::Default()
{
	static YTPConcurrenceEnviron cbe;
	cbe.SetPriority(TP_CALLBACK_PRIORITY_NORMAL);
	return &cbe;
}

////////

YTPSingleThreadedEnviron::YTPSingleThreadedEnviron() :
	m_pRefCountOfPool(nullptr)
{
}

YTPSingleThreadedEnviron::~YTPSingleThreadedEnviron()
{
	ReleaseThreadpool();
}

YTPSingleThreadedEnviron* YTPSingleThreadedEnviron::Create()
{
	YTPSingleThreadedEnviron* pSingleThreadedEnviron = new YTPSingleThreadedEnviron();
	if (pSingleThreadedEnviron)
	{
		if (!pSingleThreadedEnviron->SetThreadpool())
		{
			delete pSingleThreadedEnviron;
			pSingleThreadedEnviron = nullptr;
		}
	}
	return pSingleThreadedEnviron;
}

YTPSingleThreadedEnviron* YTPSingleThreadedEnviron::Copy()
{
	YTPSingleThreadedEnviron* pSingleThreadedEnviron = new YTPSingleThreadedEnviron(*this);
	if (pSingleThreadedEnviron)
	{
		InterlockedIncrement(m_pRefCountOfPool);
	}
	return pSingleThreadedEnviron;
}

void YTPSingleThreadedEnviron::Destroy()
{
	delete this;
}

BOOL YTPSingleThreadedEnviron::SetThreadpool()
{
	this->Pool = CreateThreadpool(nullptr);
	if (this->Pool)
	{
		SetThreadpoolThreadMinimum(this->Pool, 1);
		SetThreadpoolThreadMaximum(this->Pool, 1);
		m_pRefCountOfPool = new long{ 1 };
		if (!m_pRefCountOfPool)
		{
			CloseThreadpool(this->Pool);
			this->Pool = nullptr;
		}
	}
	return this->Pool ? TRUE : FALSE;
}

void YTPSingleThreadedEnviron::ReleaseThreadpool()
{
	long nRefCount = InterlockedDecrement(m_pRefCountOfPool);
	if (nRefCount == 0)
	{
		delete m_pRefCountOfPool;
		m_pRefCountOfPool = nullptr;
		CloseThreadpool(this->Pool);
		this->Pool = nullptr;
	}
}

////////

template <typename TPCallbackObjectPtr>
void YThreadpoolCallbackT<TPCallbackObjectPtr>::Destroy()
{
	delete this;
}

void YThreadpoolSimpleCallback::Close()
{
}

void YThreadpoolSimpleCallback::Destroy()
{
	delete this;
}

void YThreadpoolSimpleCallback::Wait(BOOL fCancelPendingCallbacks)
{
}

BOOL YThreadpoolSimpleCallback::TrySubmit(const YTPCallbackEnviron* cbe)
{
	const TP_CALLBACK_ENVIRON* cbe2 = GetCallbackEnviron(cbe ? cbe : YTPConcurrenceEnviron::Default());
	return ::TrySubmitThreadpoolCallback(SimpleCallback, this, const_cast<TP_CALLBACK_ENVIRON*>(cbe2));
}

void YThreadpoolSimpleCallback::SimpleCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context)
{
	YThreadpoolSimpleCallback* pSimpleCallback = static_cast<YThreadpoolSimpleCallback*>(Context);
	if (!pSimpleCallback->ShouldExit())
	{
		pSimpleCallback->Execute(Instance);
	}
}

BOOL YThreadpoolWork::_Create(const YTPCallbackEnviron* cbe)
{
	const TP_CALLBACK_ENVIRON* cbe2 = GetCallbackEnviron(cbe ? cbe : YTPConcurrenceEnviron::Default()); 
	m_ptpCallbackObject = CreateThreadpoolWork(WorkCallback, this, const_cast<TP_CALLBACK_ENVIRON*>(cbe2));
	return m_ptpCallbackObject != nullptr;
}

VOID NTAPI YThreadpoolWork::WorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
	YThreadpoolWork* pWork = static_cast<YThreadpoolWork*>(Context);
	if (!pWork->ShouldExit())
	{
		pWork->Execute(Instance);
	}
}

void YThreadpoolWork::Close()
{
	if (m_ptpCallbackObject)
	{
		PTP_WORK ptpWork = m_ptpCallbackObject;
		m_ptpCallbackObject = nullptr;
		CloseThreadpoolWork(ptpWork);
	}
}

void YThreadpoolWork::Wait(BOOL fCancelPendingCallbacks)
{
	WaitForThreadpoolWorkCallbacks(m_ptpCallbackObject, fCancelPendingCallbacks);
}

////////

BOOL YThreadpoolWait::_Create(const YTPCallbackEnviron* cbe)
{
	const TP_CALLBACK_ENVIRON* cbe2 = GetCallbackEnviron(cbe ? cbe : YTPConcurrenceEnviron::Default()); 
	m_ptpCallbackObject = CreateThreadpoolWait(&WaitCallback, this, const_cast<TP_CALLBACK_ENVIRON*>(cbe2));
	return m_ptpCallbackObject != nullptr;
}

void YThreadpoolWait::Close()
{
	if (m_ptpCallbackObject)
	{
		PTP_WAIT ptpWait = m_ptpCallbackObject;
		m_ptpCallbackObject = nullptr;
		CloseThreadpoolWait(ptpWait);
	}
}

void YThreadpoolWait::Wait(BOOL fCancelPendingCallbacks)
{
	WaitForThreadpoolWaitCallbacks(m_ptpCallbackObject, fCancelPendingCallbacks);
}

VOID CALLBACK YThreadpoolWait::WaitCallback(
	PTP_CALLBACK_INSTANCE Instance,
	PVOID                 Context,
	PTP_WAIT              Wait,
	TP_WAIT_RESULT        WaitResult)
{
	YThreadpoolWait* pWait = static_cast<YThreadpoolWait*>(Context);
	if (!pWait->ShouldExit())
	{
		pWait->Execute(Instance, WaitResult);
	}
}

////////

BOOL YThreadpoolTimer::_Create(const YTPCallbackEnviron* cbe)
{
	const TP_CALLBACK_ENVIRON* cbe2 = GetCallbackEnviron(cbe ? cbe : YTPConcurrenceEnviron::Default()); 
	m_ptpCallbackObject = CreateThreadpoolTimer(&TimerCallback, this, const_cast<TP_CALLBACK_ENVIRON*>(cbe2));
	return m_ptpCallbackObject != nullptr;
}

void YThreadpoolTimer::Close()
{
	if (m_ptpCallbackObject)
	{
		PTP_TIMER ptpTimer = m_ptpCallbackObject;
		m_ptpCallbackObject = nullptr;
		CloseThreadpoolTimer(ptpTimer);
	}
}

void YThreadpoolTimer::Wait(BOOL fCancelPendingCallbacks)
{
	WaitForThreadpoolTimerCallbacks(m_ptpCallbackObject, fCancelPendingCallbacks);
}

VOID CALLBACK YThreadpoolTimer::TimerCallback(
	PTP_CALLBACK_INSTANCE Instance,
	PVOID                 Context,
	PTP_TIMER             Timer)
{
	YThreadpoolTimer* pTimer = static_cast<YThreadpoolTimer*>(Context);
	if (!pTimer->ShouldExit())
	{
		pTimer->Execute(Instance);
	}
}

////////

YOverlapped::YOverlapped(DWORD nBufferSize) :
	m_nBufferSize(nBufferSize)
{
	Internal = 0;
	InternalHigh = 0;
	Offset = 0;
	OffsetHigh = 0;
	hEvent = nullptr;

	m_lpBuffer[0] = 0;
}

YOverlapped::~YOverlapped()
{
}

YOverlapped* YOverlapped::Create(UINT64 nOffset, UINT32 nBufferSize) noexcept
{
	size_t nAllocSize = offsetof(YOverlapped, m_lpBuffer) + nBufferSize;
	if (nAllocSize < sizeof(YOverlapped))
		nAllocSize = sizeof(YOverlapped);

	YOverlapped* po = (YOverlapped*)new BYTE[nAllocSize];
	if (po) 
	{
		new(po)YOverlapped(nBufferSize);
		po->SetOffset(nOffset);
	}
	return po;
}

void YOverlapped::Destroy()
{
	delete this;
}

////////

BOOL YThreadpoolIoBase::_Create(HANDLE hFile, const YTPCallbackEnviron* cbe)
{
	const TP_CALLBACK_ENVIRON* cbe2 = GetCallbackEnviron(cbe ? cbe : YTPConcurrenceEnviron::Default());
	m_ptpCallbackObject = CreateThreadpoolIo(hFile, &IoCallback, this,  const_cast<TP_CALLBACK_ENVIRON*>(cbe2));
	return m_ptpCallbackObject != nullptr;
}

void YThreadpoolIoBase::Close()
{
	if (m_ptpCallbackObject)
	{
		PTP_IO ptpIo = m_ptpCallbackObject;
		m_ptpCallbackObject = nullptr;
		CloseThreadpoolIo(ptpIo);
	}
}

void YThreadpoolIoBase::Wait(BOOL fCancelPendingCallbacks)
{
	WaitForThreadpoolIoCallbacks(m_ptpCallbackObject, fCancelPendingCallbacks);
}

void YThreadpoolIoBase::Execute(PTP_CALLBACK_INSTANCE Instance, PVOID Overlapped, ULONG IoResult, UINT64 NumberOfBytesTransferred)
{
}

VOID WINAPI YThreadpoolIoBase::IoCallback(
	PTP_CALLBACK_INSTANCE Instance,
	PVOID                 Context,
	PVOID                 Overlapped,
	ULONG                 IoResult,
	ULONG_PTR             NumberOfBytesTransferred,
	PTP_IO                Io)
{
	YThreadpoolIo* pIo = static_cast<YThreadpoolIo*>(Context);
	if (!pIo->ShouldExit())
	{
		pIo->Execute(Instance, Overlapped, IoResult, NumberOfBytesTransferred);
	}
}

////////

YThreadpoolIo::YThreadpoolIo() :
	m_hFile(nullptr)
{
}

YThreadpoolIo::~YThreadpoolIo()
{
	if (m_hFile)
	{
		CloseHandle(m_hFile);
		m_hFile = nullptr;
	}
}

BOOL YThreadpoolIo::_Create(HANDLE hFile, const YTPCallbackEnviron* cbe)
{
	BOOL bRet = YThreadpoolIoBase::_Create(hFile, cbe);
	if (bRet)
		m_hFile = hFile;
	return bRet;
}

BOOL YThreadpoolIo::Read(LPVOID lpBuffer, UINT32 nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped)
{
	StartThreadpoolIo();
	BOOL bRet = ::ReadFile(m_hFile, lpBuffer, nNumberOfBytesToRead, NULL, lpOverlapped);
	if (!bRet)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
			bRet = TRUE;
		else
			CancelThreadpoolIo();
	}
	return bRet;
}

BOOL YThreadpoolIo::Write(LPCVOID lpBuffer, UINT32 nNumberOfBytesToWrite, LPOVERLAPPED lpOverlapped)
{
	StartThreadpoolIo();
	BOOL bRet = ::WriteFile(m_hFile, lpBuffer, nNumberOfBytesToWrite, nullptr, lpOverlapped);
	if (!bRet)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
			bRet = TRUE;
		else
			CancelThreadpoolIo();
	}
	return bRet;
}

////////

YThreadpoolIoReader::YThreadpoolIoReader() :
	m_pOverlapped(nullptr)
{
}

YThreadpoolIoReader::~YThreadpoolIoReader()
{
	if (m_pOverlapped)
	{
		m_pOverlapped->Destroy();
		m_pOverlapped = nullptr;
	}
}

void YThreadpoolIoReader::Execute(PTP_CALLBACK_INSTANCE Instance, PVOID Overlapped, ULONG IoResult, UINT64 NumberOfBytesTransferred)
{
	Execute2(IoResult, m_pOverlapped->Buffer(), (UINT32)NumberOfBytesTransferred);
	Close();
}

BOOL YThreadpoolIoReader::Read(HANDLE hFile, UINT64 nOffset, UINT32 nNumberOfBytesToRead, const YTPCallbackEnviron* cbe)
{
	ULARGE_INTEGER fileSize;
	GetFileSizeEx(hFile, (LARGE_INTEGER*)&fileSize);
	if (nOffset >= fileSize.QuadPart || nNumberOfBytesToRead == 0)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		goto Failure;
	}

	if (_Create(hFile, cbe))
	{
		UINT32 nNumberOfBytesToRead2 = nNumberOfBytesToRead;
		if (nNumberOfBytesToRead2 == UINT32_MAX)
		{
			UINT64 nDistanceToEnd = fileSize.QuadPart - nOffset;
			if (nDistanceToEnd < UINT32_MAX)
				nNumberOfBytesToRead2 = (UINT32)nDistanceToEnd;
		}
		m_pOverlapped = YOverlapped::Create(nOffset, nNumberOfBytesToRead2);
		if (m_pOverlapped &&
			YThreadpoolIo::Read(m_pOverlapped->Buffer(), nNumberOfBytesToRead2, m_pOverlapped))
			return TRUE;
	}
Failure:
	Close();
	return FALSE;
}

////////

YThreadpoolIoReader2::YThreadpoolIoReader2() :
	m_nOffsetBegin(0),
	m_nOffsetEnd(0),
	m_nOffsetCur(0),
	m_pEnviron(nullptr)
{
	m_oq.fill(nullptr);
}

YThreadpoolIoReader2::~YThreadpoolIoReader2()
{
	for (YOverlapped*& poref : m_oq)
	{
		if (poref != nullptr)
		{
			poref->Destroy();
			poref = nullptr;
		}
	}
	if (m_pEnviron)
	{
		m_pEnviron->Destroy();
		m_pEnviron = nullptr;
	}
}

BOOL YThreadpoolIoReader2::Read(HANDLE hFile, UINT64 nOffset, UINT64 nNumberOfBytesToRead, UINT32 nBufferSize, YTPSingleThreadedEnviron* pEnviron)
{
	ULARGE_INTEGER fileSize;
	GetFileSizeEx(hFile, (LARGE_INTEGER*)&fileSize);

	if (nOffset >= fileSize.QuadPart || nNumberOfBytesToRead == 0)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		goto Failure;
	}

	m_pEnviron = pEnviron ? pEnviron->Copy() : YTPSingleThreadedEnviron::Create();
	if (m_pEnviron == nullptr)
		goto Failure;

	if (!_Create(hFile, m_pEnviron ? m_pEnviron : pEnviron))
		goto Failure;

	m_oq[0] = YOverlapped::Create(0, nBufferSize);
	m_oq[1] = YOverlapped::Create(0, nBufferSize);
	if (m_oq[0] == nullptr || m_oq[1] == nullptr)
		goto Failure;

	m_nOffsetBegin = nOffset;
	m_nOffsetEnd = m_nOffsetBegin + nNumberOfBytesToRead;
	if (m_nOffsetEnd > fileSize.QuadPart)
		m_nOffsetEnd = fileSize.QuadPart;
	m_nOffsetCur = m_nOffsetBegin;

	m_oq[0]->SetOffset(m_nOffsetCur);
	if (!YThreadpoolIo::Read(m_oq[0]->Buffer(), m_oq[0]->BufferSize(), m_oq[0]))
		goto Failure;
	m_nOffsetCur += nBufferSize;
	return TRUE;
Failure:
	Close();
	return FALSE;
}

void YThreadpoolIoReader2::Execute(PTP_CALLBACK_INSTANCE Instance, PVOID Overlapped, ULONG IoResult, UINT64 NumberOfBytesTransferred)
{
	YOverlapped* poThis = static_cast<YOverlapped*>(Overlapped);
	if (IoResult != NOERROR)
	{
		Close();
		return;
	}
	if (m_nOffsetCur < m_nOffsetEnd)
	{
		YOverlapped* poOther = poThis == m_oq[0] ? m_oq[1] : m_oq[0];
		poOther->SetOffset(m_nOffsetCur);
		if (YThreadpoolIo::Read(poOther->Buffer(), poOther->BufferSize(), poOther))
			m_nOffsetCur += poOther->BufferSize();
	}
	LARGE_INTEGER offset;
	offset.LowPart = poThis->Offset;
	offset.HighPart = poThis->OffsetHigh;
	BOOL bDone = offset.QuadPart + NumberOfBytesTransferred == m_nOffsetEnd;
	Execute2(bDone, IoResult, poThis->Buffer(), (UINT32)NumberOfBytesTransferred);
	if (bDone)
	{
		Close();
	}
}
