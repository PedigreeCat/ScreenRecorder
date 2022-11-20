
// screen_recoderDlg.h: 头文件
//

#pragma once

#include <vector>
#include <string>
#include "CAudioRecorder.h"
#include "CDeskRecorder.h"
#include "CMP4Muxer.h"

// CscreenrecoderDlg 对话框
class CscreenrecoderDlg : public CDialogEx
{
// 构造
public:
	CscreenrecoderDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SCREEN_RECODER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
private:
	void LogAreaUpdate();	/* 更新日志显示 */
	void AudioDeviceListUpdate();	/* 更新音频输入设备列表 */
public:
	afx_msg void OnEnKillfocusEditLogarea();
	afx_msg void OnEnSetfocusEditLogarea();
	afx_msg void OnBnClickedButtonCleanLog();
	afx_msg void OnBnClickedButtonRecord();
	afx_msg void OnBnClickedButtonRefresh();
private:
	CEdit m_logAreaCtrl;
	CString m_logAreaStr;
	CComboBox m_audioDeviceCtrl;

	std::vector<std::string> m_audioDeviceVec;	/* 音频输入设备列表 */
	CAudioRecorder* m_audioRecorderObj;
	CDeskRecorder* m_deskRecorderObj;
	CMP4Muxer* m_mp4MuxerObj;

	BOOL m_logAreaGetFocus;
};
