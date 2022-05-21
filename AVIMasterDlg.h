#pragma once

#include "afxcmn.h"
#include "afxwin.h"
#include "StatusControl.h"
#include "StatusProgress.h"
#include "resource.h"
#include "IOService.h"

// CAVIMasterDlg Dialogfeld
class CAVIMasterDlg : public CDialog
{
// Konstruktion
public:
	CAVIMasterDlg(CWnd* pParent = nullptr);	// Standardkonstruktor

// Dialogfelddaten
	enum { IDD = IDD_AVIMASTER_DIALOG };

	struct UMData {
		UMData( const _tstring& sText, int htLevel, bool bBold, bool bVisible ) { m_sText = sText; m_htLevel = htLevel; m_bBold = bBold; m_bVisible = bVisible; m_nFilePos = (QWORD)-1; m_nChunkSize = (QWORD)-1; m_nFrame = (DWORD)-1; }
		UMData( const _tstring& sText, int htLevel, QWORD nFilePos, QWORD nChunkSize, DWORD nFrame ) { m_sText = sText; m_htLevel = htLevel; m_nFilePos = nFilePos; m_nChunkSize = nChunkSize; m_nFrame = nFrame; m_bBold = false; m_bVisible = false; }
		UMData( QWORD nChunkSize, int htLevel ) { m_nChunkSize = nChunkSize; m_htLevel = htLevel; }
		UMData( int htLevel, bool bBold ) { m_bBold = bBold; m_htLevel = htLevel; }
		_tstring m_sText;
		int m_htLevel;
		bool m_bBold;
		bool m_bVisible;
		QWORD m_nFilePos;
		QWORD m_nChunkSize;
		DWORD m_nFrame;
	};

	struct ThreadInfo {
		ThreadInfo( class Settings settings, _tstring sReadFilePath, _tstring sWriteFilePath ) { Settings = settings; m_sReadFilePath = sReadFilePath; m_sWriteFilePath = sWriteFilePath; }
		class Settings Settings;
		_tstring m_sReadFilePath, m_sWriteFilePath;
	};
	static UINT MyThreadProc( LPVOID pParam );

	int OSNodeFileNew( _tstring s, int hParent ); 
	int OSNodeFileEnd( _tstring s, int hParent ); 
	int OSNodeCreate( _tstring s, int hParent, QWORD nFilePos = (QWORD)-1, QWORD nChunkSize = (QWORD)-1, DWORD nFrame = (DWORD)-1 );
	int OSNodeCreateValue( _tstring s, int hParent, bool bBold = false );
	int OSNodeCreateAlert( _tstring s, int hParent, bool bShow );
	int OSNodeAppendSize( QWORD nChunkSize, int htParent ); // Nachträglich die Knotengröße angeben
	int OSNodeAppendImportant( int htParent ); // Nachträglich Bold setzen

	void OSStatsInit( __int64 upper ) { m_statusProgress.SetRange32( 0, (int)(upper / 100) ); OSStatsSet( 0 ); }
	void OSStatsSet( __int64 pos ) { m_statusProgress.SetPos( (int)(pos / 100) ); }
	void OSStatsSet( CIOService::EPARSERSTATE eParserState ); // Wird per Message behandelt...

	bool HexeditSave();
	bool HexeditSaveAs();
	bool HexeditLoad();

	static const UINT wm_UMInsert, wm_UMInsertNew, wm_UMInsertEnd, wm_UMAppendText, wm_UMAppendSize, wm_UMAppendImportant, wm_UMStatsSet;

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV-Unterstützung

// Implementierung
protected:
	CTabCtrl m_TabCtrl;
	CTreeCtrl m_Tree;
	CDialog* m_pTabPage1;
	CDialog* m_pTabPage2;
	CDialog* m_pTabPage3;
	CImageList m_ilTree;
	CStatusBar  m_wndStatusBar;
	CStatusProgress m_statusProgress;
	WORD m_nQueueSize;
	class Settings Settings;

	static constexpr int nMaxLevel = 40;
	HTREEITEM ahtiBuffer[nMaxLevel];
	HICON m_hIcon;
	struct FileInfo {
		FileInfo( const CString& sFilepath ) { m_sFilepath = sFilepath; }
		CString m_sFilepath;
		ior ior;
	};
	FileInfo* m_fileinfo;
	struct NodeInfo {
		NodeInfo( const CString& sText, QWORD nFilePos = (QWORD)-1, QWORD nChunkSize = (QWORD)-1, DWORD nFrame = (DWORD)-1 ) {  m_sText = sText, m_nFilePos = nFilePos, m_nChunkSize = nChunkSize, m_nFrame = nFrame; m_pData = nullptr; m_nDataSize = (DWORD)-1; }
//		NodeInfo( const BYTE* p, WORD n ) { pData = new BYTE[nSize]; nSize = n; memcpy( pData, p, nSize ); }
		~NodeInfo() { delete[] m_pData; }
		CString m_sText;
		QWORD m_nFilePos;
		QWORD m_nChunkSize;
		DWORD m_nFrame;
		BYTE* m_pData;
		DWORD m_nDataSize; // WORD would be sufficient, but then ior.Read( ..., m_nDataSize, ...) would not work
		FileInfo* m_fileinfo;
	};

protected:
	virtual void OnOK() override;
	virtual void OnCancel() override;
	void SetSize(int cx, int cy);
	bool SaveHexeditChunk( const NodeInfo* p );
	bool SaveAsHexeditChunk( const NodeInfo* p, const _tstring& sFilepath );
	bool LoadHexeditChunk( const NodeInfo* p, const _tstring& sFilepath );
	void StatsSet( CIOService::EPARSERSTATE eParserState ); 

	// Generierte Funktionen für die Meldungstabellen
	virtual BOOL OnInitDialog() override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnGetMinMaxInfo( MINMAXINFO* lpMMI );
	afx_msg LRESULT HandleUMInsert( WPARAM, LPARAM );
	afx_msg LRESULT HandleUMInsertNew( WPARAM, LPARAM );
	afx_msg LRESULT HandleUMInsertEnd( WPARAM, LPARAM );
	afx_msg LRESULT HandleUMAppendText( WPARAM, LPARAM );
	afx_msg LRESULT HandleUMAppendSize( WPARAM, LPARAM );
	afx_msg LRESULT HandleUMAppendImportant( WPARAM, LPARAM );
	afx_msg LRESULT HandleUMStatsSet( WPARAM, LPARAM );
	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedMainOpenWriteAVI();
	afx_msg void OnBnClickedMainOpenDemux();
	afx_msg void OnBnClickedMainOpenParse();
	afx_msg void OnBnClickedMainClose();
	afx_msg void OnBnClickedMainOptions();
	afx_msg void OnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnCMPlayavi();
	afx_msg void OnCMDelete();
	afx_msg void OnNMRclickMainTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL DestroyWindow() override;
	afx_msg void OnTvnDeleteitemMainTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
public:
	afx_msg void OnClose();
};
