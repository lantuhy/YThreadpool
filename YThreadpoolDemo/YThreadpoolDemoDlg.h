
// YThreadpoolDemoDlg.h: 头文件
//

#pragma once

class YWindowTimer;
class YTPConcurrenceEnviron;

// CYThreadpoolDemoDlg 对话框
class CYThreadpoolDemoDlg : public CDialogEx
{
public:
	CYThreadpoolDemoDlg(CWnd* pParent = nullptr);	// 标准构造函数
	~CYThreadpoolDemoDlg();
// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_YTHREADPOOLDEMO };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
private:

	LRESULT AppendLogText(LPCWSTR lpszText);
	template <typename... Args>
	LRESULT AppendLogFormat(LPCWSTR lpszFormat, Args&&... args);

	void EnableButtons(BOOL enable);
private:
	PTP_CLEANUP_GROUP m_ptpCleanupGroup;
	YTPConcurrenceEnviron* m_pConcurrenceEnviron;
// 实现
protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedWindowTimer();
	afx_msg void OnBnClickedThreadpoolWork();
	afx_msg void OnBnClickedReadFile();
	afx_msg void OnBnClickedThreadpoolWait();
	afx_msg void OnBnClickedThreadpoolTimer();
	afx_msg void OnBnClickedBinder();
	afx_msg void OnClose();
	afx_msg void OnBnClickedClearLog();
};
