
// YThreadpoolDemoDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "YThreadpoolDemoApp.h"
#include "YThreadpoolDemoDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "..\YThreadpool\include\YThreadpool.h"


class CAboutDlg : public CDialogEx 
{
public:
	CAboutDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CYThreadpoolDemoDlg 对话框

CYThreadpoolDemoDlg* g_pDemoDlg;

CYThreadpoolDemoDlg::CYThreadpoolDemoDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_YTHREADPOOLDEMO, pParent),
	m_pConcurrenceEnviron(nullptr),
	m_ptpCleanupGroup(nullptr)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	g_pDemoDlg = this;

	m_ptpCleanupGroup = CreateThreadpoolCleanupGroup();
	m_pConcurrenceEnviron = new YTPConcurrenceEnviron();
	auto funcCleanupGroupCancel = [](YThreadpoolCallback* pCallbackObject, PVOID /*CleanupContext*/)
	{
		CString str(typeid(*pCallbackObject).name());
		str.AppendFormat(L", cancelled, thread id : %d", GetCurrentThreadId());
		if (g_pDemoDlg)
		{
			YCallbackWindow::SharedWindow().SendCallback([=]()
			{
				g_pDemoDlg->AppendLogText(str);
				return 0;
			});
		}
				else {
			str.Append(L"\r\n");
			OutputDebugString(str);
		}
	};
	m_pConcurrenceEnviron->SetCleanupGroup<funcCleanupGroupCancel>(m_ptpCleanupGroup);	

	auto funcCallbackFinalization = [](YThreadpoolCallback* pCallbackObject)
	{
		CString str(typeid(*pCallbackObject).name());
		str.AppendFormat(L", finalization, thread id : %d", GetCurrentThreadId());
		if (g_pDemoDlg)
		{
			YCallbackWindow::SharedWindow().SendCallback([=]()
			{
				g_pDemoDlg->AppendLogText(str);
				return 0;
			});
		}
		else {
			str.Append(L"\r\n");
			OutputDebugString(str);
		}
	};
	m_pConcurrenceEnviron->SetCallbackFinalization<funcCallbackFinalization>();
}

CYThreadpoolDemoDlg::~CYThreadpoolDemoDlg()
{
}

void CYThreadpoolDemoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CYThreadpoolDemoDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_WINDOW_TIMER, &CYThreadpoolDemoDlg::OnBnClickedWindowTimer)
	ON_BN_CLICKED(IDC_THREADPOOL_WORK, &CYThreadpoolDemoDlg::OnBnClickedThreadpoolWork)
	ON_BN_CLICKED(IDC_READ_FILE, &CYThreadpoolDemoDlg::OnBnClickedReadFile)
	ON_BN_CLICKED(IDC_THREADPOOL_WAIT, &CYThreadpoolDemoDlg::OnBnClickedThreadpoolWait)
	ON_BN_CLICKED(IDC_THREADPOOL_TIMER, &CYThreadpoolDemoDlg::OnBnClickedThreadpoolTimer)
	ON_BN_CLICKED(IDC_BINDER, &CYThreadpoolDemoDlg::OnBnClickedBinder)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_CLEAR_LOG, &CYThreadpoolDemoDlg::OnBnClickedClearLog)
END_MESSAGE_MAP()


// CYThreadpoolDemoDlg 消息处理程序

BOOL CYThreadpoolDemoDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);		
	SetIcon(m_hIcon, FALSE);		

	// TODO: 在此添加额外的初始化代码

	YCallbackWindow::SharedWindow();
	LOGFONT lf;
	GetFont()->GetLogFont(&lf);
	wcscpy_s(lf.lfFaceName, L"Consolas");
	lf.lfHeight = 16;
	lf.lfWeight = FW_MEDIUM;
	lf.lfCharSet = ANSI_CHARSET;
	CFont font;
	font.CreateFontIndirect(&lf);
	CEdit* pEdit = (CEdit*) GetDlgItem(IDC_LOG);
	pEdit->SetFont(&font);
	pEdit->SetLimitText(4 * 1024 * 1024);

	CMFCEditBrowseCtrl* pBrowserCtrl = (CMFCEditBrowseCtrl*)GetDlgItem(IDC_SOURCE_FILE);
	pBrowserCtrl->EnableFileBrowseButton(nullptr, L"All Files(*.*)|*.*\0\0",  OFN_HIDEREADONLY);

	return TRUE;  
}

void CYThreadpoolDemoDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

void CYThreadpoolDemoDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); 

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CYThreadpoolDemoDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CYThreadpoolDemoDlg::OnClose()
{
	if (m_ptpCleanupGroup)
	{
		EnableWindow(FALSE);
		g_pDemoDlg = nullptr;
		YThreadpoolSimpleCallback::TrySubmit(YTPConcurrenceEnviron::Default(), [=](PTP_CALLBACK_INSTANCE) {
			CloseThreadpoolCleanupGroupMembers(m_ptpCleanupGroup, TRUE, nullptr);
			delete m_pConcurrenceEnviron;
			m_pConcurrenceEnviron = nullptr;
			CloseThreadpoolCleanupGroup(m_ptpCleanupGroup);
			m_ptpCleanupGroup = nullptr;
			this->PostMessage(WM_CLOSE);
		});
	}
	else
		CDialogEx::OnClose();
}

LRESULT CYThreadpoolDemoDlg::AppendLogText(LPCWSTR lpszText)
{
	CString str;
	str.Format(L"[%I64d] %s\r\n", GetTickCount64(), lpszText);

	CEdit* pEdit = (CEdit *)GetDlgItem(IDC_LOG);
	int nTextLength = pEdit->GetWindowTextLength();
	int nLimit = (int) pEdit->GetLimitText();
	int nOutOfLimit = nTextLength + str.GetLength() - nLimit;
	if (nOutOfLimit > 0)
	{
		int nDeleteLength = 0;
		int nLine = 0;
		do {
			nDeleteLength = pEdit->LineIndex(++nLine);
		} while (nDeleteLength < nOutOfLimit && nDeleteLength != -1);
		if (nDeleteLength == -1)
			nDeleteLength = nTextLength;
		pEdit->SetSel(0, nDeleteLength);
		pEdit->ReplaceSel(nullptr);
		nTextLength -= nDeleteLength;
	}
	pEdit->SetSel(nTextLength, nTextLength);
	pEdit->ReplaceSel(str);
	return 0;
}

template <typename... Args>
LRESULT CYThreadpoolDemoDlg::AppendLogFormat(LPCWSTR lpszFormat, Args&&... args)
{
	CString str;
	str.Format(lpszFormat, args...);
	return AppendLogText(str);
}

void CYThreadpoolDemoDlg::EnableButtons(BOOL enable)
{
	EnumChildWindows(m_hWnd, [](HWND hWnd, LPARAM lParam) {
		wchar_t szClassName[256];
		::GetClassName(hWnd, szClassName, _countof(szClassName));
		if (wcscmp(szClassName, WC_BUTTON) == 0)
		{
			BOOL enable = (BOOL)lParam;
			::EnableWindow(hWnd, enable);
		}
		return TRUE;
	}, enable);
}

void CYThreadpoolDemoDlg::OnBnClickedClearLog()
{
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_LOG);
	pEdit->SetSel(0, -1);
	pEdit->ReplaceSel(nullptr);
}

void CYThreadpoolDemoDlg::OnBnClickedBinder()
{
	const char* s = "0123456789";

	auto binder1 = YBinderMake(strlen);			
	// YBinder<unsigned int(__cdecl&)(char const*)>, Func是函数引用
	int result1 = (int)binder1(s);

	auto binder2 = YBinderMake(&strlen);	
	// YBinder<unsigned int(__cdecl*)(char const*)>, Func是函数指针
	int result2 = (int)binder2(s);

	auto binder3 = YBinderMake([](int x, int y)
	{
		return x + y;
	}, 2);		
	// YBinder<int <lambda>(int, int), int>, Func是Lambda
	int result3 = binder3(3);

	auto binder4 = YBinderMake(&CYThreadpoolDemoDlg::AppendLogText);	
	// YBinder<long(__thiscall CYThreadpoolDemoDlg::*)(wchar_t const*)>, Func是成员函数指针
	CString str;
	str.Format(L"OnBnClickedBinder, result1 : %d, result2 : %d, result3 : %d", result1, result2, result3);
	binder4(this, (LPCWSTR)str);
}

void CYThreadpoolDemoDlg::OnBnClickedWindowTimer()
{
	YWindowTimer::Create(m_hWnd, 1000, [this](int& nCount, YWindowTimer *pTimer, DWORD/* dwTime*/) 
	{
		++nCount;
		this->AppendLogFormat(L"YWindowTimer, %d.", nCount);
		if (nCount == 6)
		{
			this->EnableButtons(TRUE);
			this->AppendLogFormat(L"Kill YWindowTimer : 0x%p.", pTimer);
			pTimer->Kill();
		}
	}, 0);
}

void CYThreadpoolDemoDlg::OnBnClickedThreadpoolTimer()
{
	EnableButtons(FALSE);
	auto func = [this](long& nCounter, PTP_CALLBACK_INSTANCE/* pci*/, YThreadpoolTimer* pTimer)
	{
		long nCount = InterlockedIncrement(&nCounter);
		YCallbackWindow::SharedWindow().SendCallback(
			&CYThreadpoolDemoDlg::AppendLogFormat<long&, DWORD&>,
			this,
			L"YThreadpoolTimer, count : %d, thread id : %d",
			nCount,
			GetCurrentThreadId());
		if (nCount == 6)
		{
			pTimer->Close();
			YCallbackWindow::SharedWindow().SendCallback([=]() 
			{
				this->EnableButtons(TRUE);
				return 0;
			});
		}
	};
	YThreadpoolTimer* pTimer = YThreadpoolTimer::Create(m_pConcurrenceEnviron, move(func), (long)0);
	if (pTimer)
		pTimer->SetTimer(1000, 0);
}

void CYThreadpoolDemoDlg::OnBnClickedThreadpoolWork()
{
	EnableButtons(FALSE);
	const long CountOfInstances = 8;
	YThreadpoolWork* pWork = YThreadpoolWork::Create(m_pConcurrenceEnviron, [=](long& counter, PTP_CALLBACK_INSTANCE /*pci*/, YThreadpoolWork* pWork){
		long nIndexOfInstance = InterlockedIncrement(&counter);
		DWORD nThreadId = GetCurrentThreadId();
		srand((unsigned int)(GetTickCount64() + nIndexOfInstance + nThreadId));
		int nMillseconds = rand() % 3000;
		Sleep(nMillseconds);
		YCallbackWindow::SharedWindow().SendCallback(
			&CYThreadpoolDemoDlg::AppendLogFormat<long&, int&, DWORD&>,
			this,
			L"YThreadpoolWork, index : % 2d, sleep : % 6dms, thread id : % 6u",
			nIndexOfInstance,
			nMillseconds,
			nThreadId);
		if (nIndexOfInstance == CountOfInstances)
		{
			pWork->Close();
			this->EnableButtons(TRUE);
		}
	}, (long)0);
	for (int i = 0; i < CountOfInstances; ++i)
		pWork->Submit();
}

void CYThreadpoolDemoDlg::OnBnClickedThreadpoolWait()
{
	this->EnableButtons(FALSE);
	YThreadpoolWait* pWait = YThreadpoolWait::Create(m_pConcurrenceEnviron, [=](PTP_CALLBACK_INSTANCE /*pci*/, YThreadpoolWait* pWait, TP_WAIT_RESULT WaitResult) {
		YCallbackWindow::SharedWindow().SendCallback([=]() {
			this->AppendLogFormat(
				L"YThreadpoolWait, WaitResult : %u, thread id : %u",
				WaitResult,
				GetCurrentThreadId());
			this->EnableButtons(TRUE);
			return 0;
		});
		pWait->Close();
	});
	if (pWait)
	{
		HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
		pWait->SetWait(hEvent, INFINITE);

		YThreadpoolSimpleCallback::TrySubmit(m_pConcurrenceEnviron, [=](PTP_CALLBACK_INSTANCE)
		{
			Sleep(1000);
			SetEvent(hEvent);
			CloseHandle(hEvent);
		});
	}
}

void CYThreadpoolDemoDlg::OnBnClickedReadFile()
{
	CString strFilePath;
	GetDlgItemText(IDC_SOURCE_FILE, strFilePath);
	if (strFilePath.IsEmpty())
		return;
	HANDLE hFile = CreateFile(strFilePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return;
	LARGE_INTEGER nFileSize;
	GetFileSizeEx(hFile, &nFileSize);
	AppendLogFormat(L"Read file : %s, %I64d bytes", (LPCWSTR)strFilePath, nFileSize.QuadPart);

	ULONGLONG nStartTime = GetTickCount64();
	if (nFileSize.QuadPart < 512 * 1024 * 1024)
	{
		auto func = [=](ULONG/* nIoResult*/, BYTE* /*lpBuffer*/, UINT32 nNumberOfBytesRead) 
		{
			YCallbackWindow::SharedWindow().SendCallback([=]() 
			{
				this->AppendLogFormat(L"Read %d bytes, %dms, thread id : %d",
					nNumberOfBytesRead,
					(long)(GetTickCount64() - nStartTime),
					GetCurrentThreadId());
				return 0;
			});
		};
		YThreadpoolIoReader::Read(hFile, 0, UINT32_MAX, m_pConcurrenceEnviron, move(func));
	}
	else {
		auto func = [=](UINT64& nAllBytesRead, YThreadpoolIoReader2*, BOOL bDone, ULONG nIoResult, BYTE*, UINT32 nNumberOfBytesRead)
		{
			nAllBytesRead += nNumberOfBytesRead;
			DWORD nThreadId = GetCurrentThreadId();
			YCallbackWindow::SharedWindow().SendCallback([=]() 
			{
				ULONG nTime = (ULONG)(GetTickCount64() - nStartTime);
				this->AppendLogFormat(L"Read: %s, (%d, %I64d) bytes, %0.2f%%, %0.3fs, IoResult : %d, thread id : %u",
					PathFindFileName((LPCWSTR)strFilePath),
					nNumberOfBytesRead,
					nAllBytesRead,
					nAllBytesRead * 100.0 / nFileSize.QuadPart,
					nTime / 1000.0,
					nIoResult,
					nThreadId);
				if (bDone || nIoResult != NOERROR)
				{
					this->AppendLogFormat(L"Read %I64d bytes, done : %s, %0.3fs, %uM/s",
						nAllBytesRead,
						bDone ? L"TRUE" : L"FALSE",
						nTime / 1000.0,
						nAllBytesRead * 1000 / (1024 * 1024) / nTime);
				}
				return 0;
			});
		};

		YTPSingleThreadedEnviron* pEnviron = YTPSingleThreadedEnviron::Create();
		if (pEnviron)
		{
			pEnviron->SetCleanupGroupWith(*m_pConcurrenceEnviron);
			pEnviron->SetCallbackFinalizationWith(*m_pConcurrenceEnviron);
			YThreadpoolIoReader2::Read(hFile, 0, UINT64_MAX, 32 * 1024 * 1024, pEnviron, move(func), UINT64(0));
			pEnviron->Destroy();
		}
	}
}


