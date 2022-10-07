
// screen_recoderDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "screen_recoder.h"
#include "screen_recoderDlg.h"
#include "afxdialogex.h"

#include "Clogger.h"
#include "Ctool.h"
#include "CAudioRecorder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

constexpr int TIMER_LOG_UPDATE = 1;	/* 编辑框日志更新定时器 */

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
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


// CscreenrecoderDlg 对话框



CscreenrecoderDlg::CscreenrecoderDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SCREEN_RECODER_DIALOG, pParent)
	, m_logAreaStr(_T(""))
	, m_logAreaGetFocus(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CscreenrecoderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_LOGAREA, m_logAreaCtrl);
	DDX_Text(pDX, IDC_EDIT_LOGAREA, m_logAreaStr);
	DDX_Control(pDX, IDC_COMBO_AUDIO_DEVICE, m_audioDeviceCtrl);
}

BEGIN_MESSAGE_MAP(CscreenrecoderDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_EN_KILLFOCUS(IDC_EDIT_LOGAREA, &CscreenrecoderDlg::OnEnKillfocusEditLogarea)
	ON_EN_SETFOCUS(IDC_EDIT_LOGAREA, &CscreenrecoderDlg::OnEnSetfocusEditLogarea)
	ON_BN_CLICKED(IDC_BUTTON_CLEAN_LOG, &CscreenrecoderDlg::OnBnClickedButtonCleanLog)
	ON_BN_CLICKED(IDC_BUTTON_RECORD, &CscreenrecoderDlg::OnBnClickedButtonRecord)
	ON_BN_CLICKED(IDC_BUTTON_REFRESH, &CscreenrecoderDlg::OnBnClickedButtonRefresh)
END_MESSAGE_MAP()


// CscreenrecoderDlg 消息处理程序

BOOL CscreenrecoderDlg::OnInitDialog()
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

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	SetTimer(TIMER_LOG_UPDATE, 1000, NULL);	/* 设置日志更新定时器 */
	AudioDeviceListUpdate();
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CscreenrecoderDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CscreenrecoderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CscreenrecoderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CscreenrecoderDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	switch (nIDEvent)
	{
		case TIMER_LOG_UPDATE:
		{
			LogAreaUpdate();
		}
		break;
	}
	CDialogEx::OnTimer(nIDEvent);
}

void CscreenrecoderDlg::LogAreaUpdate()
{
	/* 焦点在日志区域时不更新 */
	if (TRUE == m_logAreaGetFocus)
		return;
	std::string tmp;
	Clogger::getInstance()->getLogCacheAndClean(tmp);
	UpdateData(TRUE);
	m_logAreaStr += tmp.c_str();
	UpdateData(FALSE);
	m_logAreaCtrl.LineScroll(m_logAreaCtrl.GetLineCount());
}

void CscreenrecoderDlg::AudioDeviceListUpdate()
{
	UpdateData(TRUE);
	m_audioDeviceCtrl.ResetContent();
	CAudioRecorder audioRecorder;

	m_audioDeviceVec = audioRecorder.getDeviceList();
	if (m_audioDeviceVec.empty()) {
		ELOG_W("===== Audio Input Device Not Found =====");
		m_audioDeviceCtrl.AddString(_T("No Audio Input Device"));
	} else {
		wchar_t unicodestr[1024];
		for (auto& it : m_audioDeviceVec) {
			Ctool::getInstance()->convertANSIToUnicode(it.c_str(), unicodestr);
			m_audioDeviceCtrl.AddString(unicodestr);
		}
	}
	m_audioDeviceCtrl.SetCurSel(0);
	UpdateData(FALSE);
}


void CscreenrecoderDlg::OnEnKillfocusEditLogarea()
{
	// TODO: 在此添加控件通知处理程序代码
	m_logAreaGetFocus = FALSE;
}


void CscreenrecoderDlg::OnEnSetfocusEditLogarea()
{
	// TODO: 在此添加控件通知处理程序代码
	m_logAreaGetFocus = TRUE;
}

void CscreenrecoderDlg::OnBnClickedButtonCleanLog()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	m_logAreaStr.Empty();
	UpdateData(FALSE);
}


void CscreenrecoderDlg::OnBnClickedButtonRecord()
{
	// TODO: 在此添加控件通知处理程序代码

}


void CscreenrecoderDlg::OnBnClickedButtonRefresh()
{
	// TODO: 在此添加控件通知处理程序代码
	AudioDeviceListUpdate();
}
