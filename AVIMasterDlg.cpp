#include "stdafx.h"
#include "afxmt.h"
#include "AVIMasterApp.h"
#include "Parser.h"
#include "AVIMasterDlg.h"
#include "TabHexedit.h"
#include "TabEdit.h"
#include "DlgOptions.h"
#include "DlgOptionsWriterAVI.h"
#include "Main.h"
//#include "TabPlayer.h"
// #include "wmp.h"

#define _CRTDBG_MAP_ALLOC
#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

using namespace std;

/*static*/ const UINT CAVIMasterDlg::wm_UMInsert = RegisterWindowMessage( "WM_AVIMASTER_INSERT" );
/*static*/ const UINT CAVIMasterDlg::wm_UMInsertNew = RegisterWindowMessage( "WM_AVIMASTER_INSERT_NEW" );
/*static*/ const UINT CAVIMasterDlg::wm_UMInsertEnd = RegisterWindowMessage( "WM_AVIMASTER_INSERT_END" );
/*static*/ const UINT CAVIMasterDlg::wm_UMAppendText = RegisterWindowMessage( "WM_AVIMASTER_APPEND_TEXT" );
/*static*/ const UINT CAVIMasterDlg::wm_UMAppendSize = RegisterWindowMessage( "WM_AVIMASTER_APPEND_SIZE" );
/*static*/ const UINT CAVIMasterDlg::wm_UMAppendImportant = RegisterWindowMessage( "WM_AVIMASTER_APPEND_IMPORTANT" );
/*static*/ const UINT CAVIMasterDlg::wm_UMStatsSet = RegisterWindowMessage( "WM_AVIMASTER_STATS_SET" );


// CAboutDlg-Dialogfeld für Anwendungsbefehl 'Info'

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialogfelddaten
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV-Unterstützung

// Implementierung
protected:
	DECLARE_MESSAGE_MAP()
};

// TODO Version und Programmname programmatisch setzen...

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CAVIMasterDlg Dialogfeld



CAVIMasterDlg::CAVIMasterDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(CAVIMasterDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_fileinfo = nullptr;
	m_pTabPage1 = nullptr;
	m_pTabPage2 = nullptr;
//	m_pTabPage3 = nullptr;
	m_nQueueSize = 0;
}

void CAVIMasterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MAIN_TREE, m_Tree);
	DDX_Control(pDX, IDC_TAB1, m_TabCtrl);
}

BEGIN_MESSAGE_MAP(CAVIMasterDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_GETMINMAXINFO()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(ID_MAIN_OPEN_PARSE, OnBnClickedMainOpenParse)
	ON_BN_CLICKED(ID_MAIN_OPEN_WRITEAVI, OnBnClickedMainOpenWriteAVI)
	ON_BN_CLICKED(ID_MAIN_OPEN_DEMUX, OnBnClickedMainOpenDemux)
	ON_BN_CLICKED(ID_MAIN_OPTIONS, OnBnClickedMainOptions)
	ON_BN_CLICKED(ID_MAIN_CLOSE, OnBnClickedMainClose)
	ON_NOTIFY(TVN_SELCHANGED, IDC_MAIN_TREE, OnSelchangedTree)
	ON_COMMAND(ID_CONTEXTMENU_PLAYAVI, OnCMPlayavi)
	ON_COMMAND(ID_CONTEXTMENU_DELETE, OnCMDelete)
	ON_NOTIFY(NM_RCLICK, IDC_MAIN_TREE, OnNMRclickMainTree)
	ON_WM_DROPFILES()
	ON_REGISTERED_MESSAGE(wm_UMInsert, HandleUMInsert)
	ON_REGISTERED_MESSAGE(wm_UMInsertNew, HandleUMInsertNew)
	ON_REGISTERED_MESSAGE(wm_UMInsertEnd, HandleUMInsertEnd)
	ON_REGISTERED_MESSAGE(wm_UMAppendSize, HandleUMAppendSize)
	ON_REGISTERED_MESSAGE(wm_UMAppendImportant, HandleUMAppendImportant)
	ON_REGISTERED_MESSAGE(wm_UMAppendText, HandleUMAppendText)
	ON_REGISTERED_MESSAGE(wm_UMStatsSet, HandleUMStatsSet)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, &CAVIMasterDlg::OnTcnSelchangeTab1)
	ON_NOTIFY(TVN_DELETEITEM, IDC_MAIN_TREE, &CAVIMasterDlg::OnTvnDeleteitemMainTree)
	ON_WM_SIZE()
	ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
	ON_WM_CLOSE()
END_MESSAGE_MAP()


static UINT indicators[] =
{
	ID_INDICATOR_TEXT_STATE,
	ID_INDICATOR_TEXT_QUEUE,
	ID_INDICATOR_PROGRESS,
};

// CAVIMasterDlg Meldungshandler

BOOL CAVIMasterDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CIOService::SetDialog( this );

	// Hinzufügen des Menübefehls "Info..." zum Systemmenü.

	// IDM_ABOUTBOX muss sich im Bereich der Systembefehle befinden.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Symbol für dieses Dialogfeld festlegen. Wird automatisch erledigt
	//  wenn das Hauptfenster der Anwendung kein Dialogfeld ist
	SetIcon(m_hIcon, TRUE);			// Großes Symbol verwenden
	SetIcon(m_hIcon, FALSE);		// Kleines Symbol verwenden

	m_TabCtrl.InsertItem( 0, _T("Detail") );
	m_TabCtrl.InsertItem( 1, _T("Editor") );
//	m_TabCtrl.InsertItem( 2, _T("Player") );
	m_pTabPage1 = new CTabEdit();
	m_pTabPage2 = new CTabHexedit( this );
//	m_pTabPage3 = new CTabPlayer();
	m_TabCtrl.SetCurSel( 0 );
	VERIFY( m_pTabPage1->Create( IDD_TAB_EDIT, m_TabCtrl.GetParent() ) );
	VERIFY( m_pTabPage2->Create( IDD_TAB_HEXEDIT, m_TabCtrl.GetParent() ) );
//	VERIFY( m_pTabPage3->Create( IDD_TAB_PLAYER, m_TabCtrl.GetParent() ) );
	// Read Only setzen geht nicht so ((CTabHexedit*)m_pTabPage2)->m_Hexedit.m_currentMode = CHexEdit::EDIT_NONE;
	//m_Tree.ModifyStyle( TVS_CHECKBOXES, 0 );
	//m_Tree.ModifyStyle( 0, TVS_CHECKBOXES );

	VERIFY( m_ilTree.Create( IDB_IL_TREE, 16, 1, /*CRREF*/ 0 ) );
	m_Tree.SetImageList( &m_ilTree, TVSIL_NORMAL /*TVSIL_STATE*/ ); // kein Verify

	m_pTabPage1->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );
	m_pTabPage1->ShowWindow(SW_SHOW);
	((CTabHexedit*)m_pTabPage2)->m_Hexedit.SetReadOnly( !Settings.bHexEdit );

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}


	CRect rect;
	GetClientRect(&rect);
	//Size the two panes
	m_wndStatusBar.SetPaneInfo( 0, ID_INDICATOR_TEXT_STATE, SBPS_NORMAL, 200 );      
	m_wndStatusBar.SetPaneInfo( 1, ID_INDICATOR_TEXT_QUEUE, SBPS_NORMAL, 100 );      
	m_wndStatusBar.SetPaneInfo( 2, ID_INDICATOR_PROGRESS, SBPS_STRETCH, 0 );

	//This is where we actually draw it on the screen
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST,AFX_IDW_CONTROLBAR_LAST,
		ID_INDICATOR_PROGRESS);

	VERIFY( m_statusProgress.Create(&m_wndStatusBar, ID_INDICATOR_PROGRESS,
					WS_VISIBLE | PBS_SMOOTH) );


	int x = (int)AfxGetApp()->GetProfileInt( "Settings", "MainWndX", 200 );
	int y = (int)AfxGetApp()->GetProfileInt( "Settings", "MainWndY", 200 );
	int w = (int)AfxGetApp()->GetProfileInt( "Settings", "MainWndW", 600 );
	int h = (int)AfxGetApp()->GetProfileInt( "Settings", "MainWndH", 500 );

	Settings.nVerbosity = AfxGetApp()->GetProfileInt( "Settings", "Verbosity", 3 );
	Settings.nTraceIdx1Entries = AfxGetApp()->GetProfileInt( "Settings", "TraceIdx1Entrie", 1000 );
	Settings.nTraceIndxEntries = AfxGetApp()->GetProfileInt( "Settings", "TraceIndxEntrie", 1000 );
	Settings.nTraceMoviEntries = AfxGetApp()->GetProfileInt( "Settings", "TraceMoviEntries", 1000 );
	Settings.nDumpJunkCount = AfxGetApp()->GetProfileInt( "Settings", "DumpJunkCount", 256 );
	Settings.nFastmodeCount = AfxGetApp()->GetProfileInt( "Settings", "FastmodeCount", 0 );
	Settings.nWriteFileSize = AfxGetApp()->GetProfileInt( "Settings", "WriteFileSize", 0 );
	Settings.nWriteMoviSize = AfxGetApp()->GetProfileInt( "Settings", "WriteMoviSize", (UINT)(2i64 * 1024 * 1024 * 1024) ); 

	Settings.bCheckIndex = (AfxGetApp()->GetProfileInt( "Settings", "CheckIndex", 1 ) != 0);
	Settings.bForceChunkSize = (AfxGetApp()->GetProfileInt( "Settings", "ForceChunkSize", 0 ) != 0);
	Settings.bHexEdit = (AfxGetApp()->GetProfileInt( "Settings", "HexEditEditable", 1 ) != 0);
	Settings.bHexHex = (AfxGetApp()->GetProfileInt( "Settings", "HexeditHex", 0 ) != 0);
	Settings.bIgnoreIndexOrder = (AfxGetApp()->GetProfileInt( "Settings", "WriteIgnoreIndexOrder", 0 ) != 0);
	Settings.bVerify = (AfxGetApp()->GetProfileInt( "Settings", "ParseOutput", 1 ) != 0);
	Settings.bTight = (AfxGetApp()->GetProfileInt( "Settings", "WriteTight", 1 ) != 0);
	Settings.bWithIdx1 = (AfxGetApp()->GetProfileInt( "Settings", "WriteWithIdx1", 0 ) != 0);
	Settings.bWithODML = (AfxGetApp()->GetProfileInt( "Settings", "WriteWithODML", 1 ) != 0);

//	AfxGetApp()->GetProfileInt( "Settings", "WriteFixAudio", Settings.bFixAudio );
//	AfxGetApp()->GetProfileInt( "Settings", "ForceType", Settings::EM_FORCENONE );
//	AfxGetApp()->GetProfileInt( "Settings", "WriteVideoFPS", Settings.fSetVideoFPS * 10000 );

	Settings.sExtASF   = AfxGetApp()->GetProfileString( "Settings", "ExtensionsASF", Settings.sExtASF.c_str() );
	Settings.sExtAVI   = AfxGetApp()->GetProfileString( "Settings", "ExtensionsAVI", Settings.sExtAVI.c_str() );
	Settings.sExtMOV   = AfxGetApp()->GetProfileString( "Settings", "ExtensionsMOV", Settings.sExtMOV.c_str() );
	Settings.sExtMPG   = AfxGetApp()->GetProfileString( "Settings", "ExtensionsMPG", Settings.sExtMPG.c_str() );
	Settings.sExtOther = AfxGetApp()->GetProfileString( "Settings", "ExtensionsOther", Settings.sExtOther.c_str() );
	Settings.sExtRIFF  = AfxGetApp()->GetProfileString( "Settings", "ExtensionsRIFF", Settings.sExtRIFF.c_str() );
	Settings.sExtRMF   = AfxGetApp()->GetProfileString( "Settings", "ExtensionsRMF", Settings.sExtRMF.c_str() );

	StatsSet( CIOService::EPARSERSTATE::PS_READY );
	MoveWindow( x, y, w, h ); 
	EnableToolTips();

	if ( *AfxGetApp()->m_lpCmdLine ) {
		CommandLine cl( Settings, AfxGetApp()->m_lpCmdLine );

		switch ( cl.GetError() ) {
			case CommandLine::CL_MESSAGE:
				AfxMessageBox( cl.GetMessage().c_str(), MB_OK | MB_ICONEXCLAMATION );
			break;
			case CommandLine::CL_USAGE:
			break;
			case CommandLine::CL_OK:
				CommandLine::WorkList wl = cl.GetWorkList();
				for ( int i = 0; i < wl.size(); i++ ) {
					ThreadInfo* ti = new ThreadInfo( Settings, wl[i].first, wl[i].second );
					m_nQueueSize++; // InterlockedIncrement...
					CWinThread* wt = AfxBeginThread( MyThreadProc, ti );
				}
			break;
		}
	}

	return TRUE;  // Geben Sie TRUE zurück, außer ein Steuerelement soll den Fokus erhalten
}

BOOL CAVIMasterDlg::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    UINT_PTR nID =pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)nID);
        if(nID)
        {
            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);
        }
    }
    return(FALSE);
}

void CAVIMasterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// Wenn Sie dem Dialogfeld eine Schaltfläche "Minimieren" hinzufügen, benötigen Sie 
//  den nachstehenden Code, um das Symbol zu zeichnen. Für MFC-Anwendungen, die das 
//  Dokument/Ansicht-Modell verwenden, wird dies automatisch ausgeführt.

void CAVIMasterDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // Gerätekontext zum Zeichnen

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Symbol in Clientrechteck zentrieren
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Symbol zeichnen
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// Die System ruft diese Funktion auf, um den Cursor abzufragen, der angezeigt wird, während der Benutzer
//  das minimierte Fenster mit der Maus zieht.
HCURSOR CAVIMasterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void PlayFile (LPCSTR szFile, CStatic *c);

void CAVIMasterDlg::OnBnClickedMainOpenParse()
{
	const _tstring w =
		_T("Supported Media Files|") + Settings.sExtAVI + _T(";") + Settings.sExtASF + _T(";") + Settings.sExtMOV + _T(";") + Settings.sExtMPG + _T(";") + Settings.sExtRIFF + _T(";") + Settings.sExtRMF + _T(";") + Settings.sExtMKV + _T(";") + Settings.sExtOther +
		_T("|AVI Files (") + Settings.sExtAVI + _T(")|") + Settings.sExtAVI + 
		_T("|Other Files (") + Settings.sExtOther + _T(")|") + Settings.sExtOther + 
		_T("|Windows-Media Files (") + Settings.sExtASF + _T(")|") + Settings.sExtASF + 
		_T("|Apple Quicktime Files (") + Settings.sExtMOV + _T(")|") + Settings.sExtMOV + 
		_T("|MPEG-Files (") + Settings.sExtMPG + _T(")|") + Settings.sExtMPG + 
		_T("|Real Media Files (") + Settings.sExtRMF + _T(")|") + Settings.sExtRMF + 
		_T("|Matroska Media Files (") + Settings.sExtMKV + _T(")|") + Settings.sExtMKV +
		_T("|All Files (*.*)|*.*||");
	CFileDialog d( true, nullptr, nullptr, /*OFN_HIDEREADONLY |*/ OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT, w.c_str() );
	d.m_ofn.lpstrFile = new _TCHAR[16000];
	d.m_ofn.nMaxFile = 16000;
	d.m_ofn.lpstrFile[0] = _T('\0');
	if ( d.DoModal() == IDOK ) {
		POSITION pos = d.GetStartPosition();
//#pragma omp parallel
		// das funktioniert nicht, solange die TreeItems nicht einem Parser zugeordnet werden können --> m_FileInfo, hti
		int nCount = 0;
		while ( pos ) {
			_tstring s = d.GetNextPathName( pos ).GetString();
			if ( FilenameGetExtension( s ) == _T(".met") )
				s = s.substr( 0, s.length() - 4 );
			ThreadInfo* ti = new ThreadInfo ( Settings, s, _T("") );
			m_nQueueSize++; // InterlockedIncrement...
			CWinThread* wt = AfxBeginThread( MyThreadProc, ti ); 
#if 0
			s = "file://" + s;
			((CTabPlayer*)m_pTabPage3)->m_Player.put_URL( s.c_str() );
#endif
		}
	}
	delete[] d.m_ofn.lpstrFile;
}


void CAVIMasterDlg::OnBnClickedMainOpenWriteAVI()
{
	const _tstring w =
		_T("Supported Media Files|") + Settings.sExtAVI + _T(";") + Settings.sExtOther + 
		_T("|AVI Files (") + Settings.sExtAVI + _T(")|") + Settings.sExtAVI + 
		_T("|Other Files (") + Settings.sExtOther + _T(")|") + Settings.sExtOther + 
		_T("|All Files (*.*)|*.*||");
	CFileDialog d( true, nullptr, nullptr, OFN_FILEMUSTEXIST, w.c_str() );
	if ( d.DoModal() == IDOK ) {
		_tstring s = d.GetPathName().GetString(), t;
		if ( FilenameGetExtension( s ) == _T(".met") )
			s = s.substr( 0, s.length() - 4 );
		if ( FilenameGetExtension( s ) == _T(".part") ) 
			PartMetInfo( s, t );
		else
			t = FilenameAppend( s, _T("_new") );

r:		CFileDialog d( false, _T(".avi"), t.c_str(), OFN_CREATEPROMPT | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST, w.c_str() );
		if ( d.DoModal() != IDOK ) 
			return;
		t = d.GetPathName();
		if ( t == s ) {
			AfxMessageBox( "You can't use the same file for source and destination!", MB_ICONEXCLAMATION | MB_OK );
			goto r;
		}
		CDlgOptionsWriterAVI dlg( Settings, s.c_str(), t.c_str() );
		if ( dlg.DoModal() != IDOK )
			return;
		ThreadInfo* ti = new ThreadInfo ( Settings, s, t );
		m_nQueueSize++; // InterlockedIncrement...
		CWinThread* wt = AfxBeginThread( MyThreadProc, ti ); 
		if ( Settings.bVerify ) {
			ThreadInfo* ti = new ThreadInfo ( Settings, t, _T("") );
			m_nQueueSize++; // InterlockedIncrement...
			CWinThread* wt = AfxBeginThread( MyThreadProc, ti ); 
		}
	}
}


void CAVIMasterDlg::OnBnClickedMainOpenDemux()
{
	const _tstring w =
		_T("Supported Media Files|") + Settings.sExtMPG + _T(";") + _T(";") + Settings.sExtOther + 
		_T("|Other Files (") + Settings.sExtOther + _T(")|") + Settings.sExtOther + 
		_T("|MPEG-Files (") + Settings.sExtMPG + _T(")|") + Settings.sExtMPG + 
		_T("|All Files (*.*)|*.*||");
	CFileDialog d( true, nullptr, nullptr, OFN_FILEMUSTEXIST, w.c_str() );
	if ( d.DoModal() == IDOK ) {
		_tstring s = d.GetPathName().GetString(), t;
		if ( FilenameGetExtension( s ) == _T(".met") )
			s = s.substr( 0, s.length() - 4 );
		if ( FilenameGetExtension( s ) == _T(".part") ) 
			PartMetInfo( s, t );
		else
			t = FilenameAppend( s, _T("_new") );

r:		CFileDialog d( false, _T(".mpg"), t.c_str(), OFN_CREATEPROMPT | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST, w.c_str() );
		if ( d.DoModal() != IDOK ) 
			return;
		t = d.GetPathName();
		if ( t == s ) {
			AfxMessageBox( "You can't use the same file for source and destination!", MB_ICONEXCLAMATION | MB_OK );
			goto r;
		}
#if 0 // TODO
		CDlgOptionsWriterMPG dlg( Settings );
		if ( dlg.DoModal() != IDOK )
			return;
#endif
		ThreadInfo* ti = new ThreadInfo ( Settings, s, t );
		m_nQueueSize++; // InterlockedIncrement...
		CWinThread* wt = AfxBeginThread( MyThreadProc, ti ); 
		if ( Settings.bVerify ) {
			ThreadInfo* ti = new ThreadInfo ( Settings, t, _T("") );
			m_nQueueSize++; // InterlockedIncrement...
			CWinThread* wt = AfxBeginThread( MyThreadProc, ti ); 
		}
	}
}


#if 0 // _DEBUG

#undef DVINFO
#include "dshow.h"


#include <windows.h>
#include <mmsystem.h>
#include ".\avimasterdlg.h"
//#include <streams.h>
//#include "playfile.h"


  IMediaSeeking *m_pMediaSeeking;

 #define WM_GRAPHNOTIFY  WM_USER+13
 #define HELPER_RELEASE(x) { if (x) x->Release(); x = nullptr; }

 HWND      ghApp;
 HINSTANCE ghInst;
 HRESULT   hr;
 LONG      evCode;
 LONG      evParam1;
 LONG      evParam2;

  IGraphBuilder *pigb  = nullptr;
  IMediaControl *pimc  = nullptr;
  IMediaEventEx *pimex = nullptr;
  IVideoWindow  *pivw  = nullptr;
  IAMCollection *pifg = nullptr;

void PlayFile (LPCSTR szFile, CStatic *c)
{   
	HRESULT hr;

        WCHAR wFile[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, szFile, -1, wFile, MAX_PATH );

		CoInitialize( nullptr );
        hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pigb );

   pigb->QueryInterface(IID_IMediaControl, (void **)&pimc);
   pigb->QueryInterface(IID_IMediaEventEx, (void **)&pimex);
   pigb->QueryInterface(IID_IVideoWindow, (void **)&pivw);

  hr = pigb->RenderFile(wFile, nullptr);
  hr = pimc->Run();
#if 0
  IEnumFilters *ief;
  // ief->Next( Enumerating Filters anschauen...
   IAMCollection* iam = (IAMCollection*) pigb->EnumFilters(f->FilterCollection;

   pigb->QueryInterface(IID_IAMCollection, (void **)&pifg);

   long lc;
	pifg->get_Count( &lc );
	for ( int i = 0; i < lc; i++ ) {
		IFilterInfo *fi;
		pifg->Item( i, (IUnknown**)&fi );
		BSTR s;
		fi->get_Name( &s );
	}

   pimex->SetNotifyWindow((OAHWND)ghApp, WM_GRAPHNOTIFY, 0);

  pigb->QueryInterface(IID_IMediaSeeking,
                           (void **)&m_pMediaSeeking);

  m_pMediaSeeking->SetTimeFormat( &TIME_FORMAT_FRAME );

  LONGLONG pos = 500;
  m_pMediaSeeking->SetPositions( &pos, AM_SEEKING_AbsolutePositioning, &pos, AM_SEEKING_AbsolutePositioning );
  pimc->Pause();
#endif



#if 0
  IBasicVideo *bv;
  pigb->QueryInterface(IID_IBasicVideo,
                           (void **)&bv);

  long bs;
  bv->GetCurrentImage( &bs, nullptr );
  long *b;
  b = new long[bs];
  bv->GetCurrentImage( &bs, b );
  c->SetBitmap( ::CreateBitmapIndirect( (const BITMAP *)b ) );
  c->Invalidate();
#endif

#if 0
  IMediaPosition *pimp;
  hr = pigb->QueryInterface(&IID_IMediaPosition, (void **)&pimp);
  hr = pimp->put_CurrentPosition(0);
#endif

//    hr = pimp->put_CurrentPosition(10000000);

#if 0
The term media time refers to positions within a seekable medium. Media time can be expressed in a variety of units, and indicates a position within the data in the file. The following table shows the possible media time formats. Value Description 
TIME_FORMAT_MEDIA_TIME  Seeks to the specified time in the media file, in 100-nanosecond units. This is the default.  
TIME_FORMAT_BYTE  Seeks to the specified byte in the stream.  
TIME_FORMAT_FIELD  Seeks to the specified interlaced video field.  
TIME_FORMAT_FRAME  Seeks to the specified video frame.  
TIME_FORMAT_SAMPLE  Seeks to the specified sample in the stream.  


For example, the following code sets the format so that the application seeks for sample numbers. 

   IMediaSeeking *pims; 
   hr = pigb->QueryInterface(IID_IMediaSeeking, (void **)&pims);
   hr = pims->SetTimeFormat(&TIME_FORMAT_SAMPLE);

An application can use the various seeking modes to seek in a stream to a particular video frame or audio sample without doing time/rate conversions itself. This is useful for editing, which requires sample-accurate playback. The frame or sample number that the application specifies is passed through to the AVI or MPEG parser without the risk of rounding errors. 

The following steps show how to set which frame in a media file to start playing at and which frame to stop playing at; for example, to start playing a movie at the fifth frame after its beginning. You can insert this code into the PlayFile function anywhere after the RenderFile function has built the filter graph. 

Access the IMediaSeeking interface. 
   IMediaSeeking *pims; 
   hr = pigb->QueryInterface(IID_IMediaSeeking, (void **)&pims);

Set the time format. In the following example, the time format is set to seek to frames. 
   hr = pims->SetTimeFormat(&TIME_FORMAT_FRAME);

Declare and initialize the media-seeking variables. In this case, they are frames within the media file to start and stop playback. The following values set the start frame to 5 and the stop frame to 15. 
   LONGLONG start = 5L;
   LONGLONG stop = 15L;

Set the start and stop media time with the IMediaSeeking::SetPositions method. The AM_SEEKING_AbsolutePositioning flag means that the start and stop frames are absolute positions within the media file (not relative to the present position in the media file). In this example, the media file will start playing at frame 5 into the file and stop at frame 15, for a duration of 10 frames. The length of playing time depends on the video's frame rate. 
   pims->SetPositions(&start, AM_SEEKING_AbsolutePositioning, &stop,
               AM_SEEKING_AbsolutePositioning);

Release the IMediaSeeking interface. 
   pims->Release();

By removing the SetTimeFormat call and setting the values of start and stop as follows, you can set the media file to start playing 5 seconds into the file and stop 7 seconds into the file, for a duration of 2 seconds. 

   LONGLONG start = 50000000L;
   LONGLONG stop = 70000000L;

By setting other formats in the SetTimeFormat call, you can seek to frames, sample numbers, byte, and so on. 
#endif
	}



#endif

void CAVIMasterDlg::OnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	static int i = 0;
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>( pNMHDR );
	if ( ((CTabHexedit*)m_pTabPage2)->m_Hexedit.GetModify() ) {
		INT_PTR ret = AfxMessageBox( _T("You have modified the chunks data in the HexEditor.\nDo you want to save this changes to the source file?\nWarning: This may corrupt your source file!"), MB_ICONQUESTION | MB_YESNOCANCEL );
		if ( ret == IDCANCEL ) { // TODO geht das eigentlich?
			*pResult = 1;
			return;
		}
		if ( ret == IDYES ) {
			NodeInfo* p = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( pNMTreeView->itemOld.hItem ) );
			SaveHexeditChunk( p );
		}

	}
	NodeInfo* p = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( pNMTreeView->itemNew.hItem ) );
	if ( p ) {
		CString s;
		s.Format( _T("FilePos: 0x%8.8I64x (%I64i)\r\nChunkSize: 0x%8.8I64x (%I64i)\r\n\r\n"), p->m_nFilePos, p->m_nFilePos, p->m_nChunkSize, p->m_nChunkSize );
		((CTabEdit*)m_pTabPage1)->m_Edit.SetWindowText( s + p->m_sText );
		if ( p->m_pData == nullptr && p->m_fileinfo && p->m_nFilePos != (QWORD)-1 && p->m_nChunkSize != (QWORD)-1 ) {
			if ( !p->m_fileinfo->ior.IsOpen() ) {
				p->m_fileinfo->ior.Open( p->m_fileinfo->m_sFilepath.GetBuffer() ); 
			}
			if ( p->m_fileinfo->ior.IsOpen() ) {
				p->m_nDataSize = (WORD)min( p->m_nChunkSize, 1024 );
				p->m_pData = new BYTE[p->m_nDataSize];
				p->m_fileinfo->ior.SetPos( p->m_nFilePos );
				p->m_fileinfo->ior.Read( p->m_pData, p->m_nDataSize, false );
			}
		}
		if ( p->m_pData )
			((CTabHexedit*)m_pTabPage2)->m_Hexedit.SetData( p->m_nFilePos, p->m_pData, p->m_nDataSize );
		else
			((CTabHexedit*)m_pTabPage2)->m_Hexedit.SetData( 0, (LPBYTE)"", 0 );


#if 0
IDispatch *pIDisControls = nullptr;
IWMPControls3 *pControls = nullptr;
HRESULT hr = S_OK;

// Get a pointer to the IDispatch interface from CWMPSettings.
pIDisControls = ((CTabPlayer*)m_pTabPage3)->m_Player.get_controls();

if( pIDisControls != nullptr )
{
    // Get a pointer to the IWMPSettings2 interface. 
	hr = pIDisControls->QueryInterface(__uuidof( IWMPControls3 ), reinterpret_cast<void**>( &pControls ) );

    if( SUCCEEDED( hr ) )
    {
        hr = pControls->put_currentPosition( i++ );
        hr = pControls->pause();
		CString s;
		BSTR t;
		pControls->get_currentPositionTimecode( &t );
    }

    if( SUCCEEDED( hr ) )
    {
        // Use the lLangID value somehow...
    }

    // Don't forget to call release through the pointers.
    if( pIDisControls )
    { 
        pIDisControls->Release();
    }

    if( pControls )
    {
        pControls->Release();
    }
}
#endif

//		CWMPControls3* c = new CWMPControls3();
//		c->AttachDispatch( ((CTabPlayer*)m_pTabPage3)->m_pPlayer->get_controls() );
#if 0
		LPDISPATCH lpd = ((CTabPlayer*)m_pTabPage3)->m_Player.get_controls();
		CWMPControls* c;
		lpDispatch->QueryInterface(IID_CWM, 
                                (void**)&c);
		lpDispatch->Release();
#endif
//		c->put_currentPosition( i++ );
		
//		LPDISPATCH lpd = ((CTabPlayer*)m_pTabPage3)->m_Player.get_controls();
//		CComPtr<IWMPControls> m_Controls( (IWMPControls*)lpd );
//		m_Controls->put_currentPosition( i++ );
	}
#if 0
	struct NodeInfo* p = (struct NodeInfo*)m_Tree.GetItemData( pNMTreeView->itemNew.hItem );
	if ( p && m_pMediaSeeking ) {
		CString s;
		s.Format( "Pos: %I64d, Size: %u.", p->nPos, p->nSize );
		m_dPreviewEdit.SetWindowText( s );
		s = m_Tree.GetItemText( pNMTreeView->itemNew.hItem );
		int p = s.Find( "Entry #" );
		if ( p > 0 ) {
			LONGLONG pos = atoi( s.Mid( p + 7 ) );
			m_pMediaSeeking->SetPositions( &pos, AM_SEEKING_AbsolutePositioning, &pos, AM_SEEKING_AbsolutePositioning );
			pimc->Pause();

		}
	}
#endif
	*pResult = 0;
}


void CAVIMasterDlg::OnNMRclickMainTree(NMHDR *pNMHDR, LRESULT *pResult)
{
//	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	CPoint pScreen, pClient;
	::GetCursorPos( &pScreen );
	pClient = pScreen;
    UINT nFlags;
	m_Tree.ScreenToClient( &pClient );
	HTREEITEM hti = m_Tree.HitTest( pClient, &nFlags );
	if ( hti != nullptr && (nFlags & TVHT_ONITEM) != 0 ) {
		CMenu menu;
		menu.LoadMenu( IDR_CM_TREE );
		CMenu *submenu = menu.GetSubMenu(0);
		struct NodeInfo* p = (struct NodeInfo*)m_Tree.GetItemData( m_Tree.GetSelectedItem() );
		if ( !p || !p->m_fileinfo ) 
			submenu->EnableMenuItem( ID_CONTEXTMENU_PLAYAVI, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED );
		submenu->TrackPopupMenu( TPM_RIGHTBUTTON|TPM_LEFTALIGN, pScreen.x, pScreen.y, this );
	    menu.DestroyMenu();
	}
	*pResult = 0;
}

void CAVIMasterDlg::OnDropFiles(HDROP hDropInfo)
{
	int nCount = DragQueryFile( hDropInfo, -1, nullptr, 0 );
//#pragma omp parallel for  
		// das funktioniert nicht, solange die TreeItems nicht einem Parser zugeordnet werden können --> m_FileInfo, hti
	for ( int i = 0; i < nCount; i++ ) {
		int nLength = DragQueryFile( hDropInfo, i, nullptr, 0 );
		_TCHAR* b = new _TCHAR[nLength + 1];
		VERIFY( DragQueryFile( hDropInfo, i, b, nLength + 1 ) == nLength );
		_tstring s = b;
		delete[] b;
		if ( FilenameGetExtension( s ) == _T(".met") )
			s = s.substr( 0, s.length() - 4 ); // TODO verhindern, dass Files doppelt verarbeitet werden...
		ThreadInfo* ti = new ThreadInfo ( Settings, s, _T("") );
		m_nQueueSize++; // InterlockedIncrement...
		CWinThread* wt = AfxBeginThread( MyThreadProc, ti ); 
	}

	CDialog::OnDropFiles(hDropInfo);
}


// POR = Parser Output Representation


int CAVIMasterDlg::OSNodeFileNew( _tstring s, int hParent ) 
{ 
	ASSERT( hParent == 0 && s.length() > 0 );
	UMData* p = new UMData( s, hParent, true, false );
	ASSERT( !m_fileinfo );
	m_fileinfo = new FileInfo( s.c_str() );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMInsertNew, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMInsertNew, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return (hParent + 1);
}

int CAVIMasterDlg::OSNodeFileEnd( _tstring s, int hParent ) 
{ 
	ASSERT( hParent == 1 /*&& s.length() > 0*/ );
	ASSERT( m_fileinfo );
	m_fileinfo = nullptr; // delete muss am Schluss wenn der Eintrag aus dem Baum entfernt wird gemacht werden...
	UMData* p = new UMData( s, hParent, false, false );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMInsertEnd, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMInsertEnd, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return (hParent + 1);
}

int CAVIMasterDlg::OSNodeCreate( _tstring s, int hParent, QWORD nFilePos, QWORD nChunkSize, DWORD nFrame ) 
{ 
	ASSERT( hParent > 0 && hParent < nMaxLevel && s.length() > 0 ); 
	ASSERT( m_fileinfo );
	UMData* p = new UMData( s, hParent, nFilePos, nChunkSize, nFrame );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMInsert, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMInsert, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return (hParent + 1);
}

int CAVIMasterDlg::OSNodeAppendImportant( int hParent ) 
{ 
	ASSERT( hParent > 0 && hParent < nMaxLevel ); 
	ASSERT( m_fileinfo );
	UMData* p = new UMData( hParent, true );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMAppendImportant, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMAppendImportant, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return hParent;
}

int CAVIMasterDlg::OSNodeCreateValue( _tstring s, int hParent, bool bBold ) 
{ 
	ASSERT( hParent > 0 && hParent < nMaxLevel && s.length() > 0 );
	ASSERT( m_fileinfo );
	UMData* p = new UMData( s, hParent, bBold, false );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMAppendText, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMAppendText, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return hParent;
}
int CAVIMasterDlg::OSNodeAppendSize( QWORD nSize, int hParent ) 
{ 
	ASSERT( hParent > 0 && hParent < nMaxLevel && nSize > 0 && nSize != (QWORD)-1 ); // size kann auch 0 sein
	ASSERT( m_fileinfo );
	UMData* p = new UMData( nSize, hParent );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMAppendSize, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMAppendSize, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return hParent;
}

int CAVIMasterDlg::OSNodeCreateAlert( _tstring s, int hParent, bool bShow ) 
{ 
	ASSERT( s.length() > 0 && hParent > 0 && hParent < nMaxLevel );
	ASSERT( m_fileinfo );
	UMData* p = new UMData( s, hParent, true, bShow );
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMInsert, 0, reinterpret_cast<LPARAM>( p ) ) );
#else
	while ( !PostMessage( wm_UMInsert, 0, reinterpret_cast<LPARAM>( p ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
	return (hParent + 1);
}

void CAVIMasterDlg::OSStatsSet( CIOService::EPARSERSTATE eParserState )
{ 
#ifdef _DEBUG
	VERIFY( !SendMessage( wm_UMStatsSet, 0, static_cast<LPARAM>( eParserState ) ) );
#else
	while ( !PostMessage( wm_UMStatsSet, 0, static_cast<LPARAM>( eParserState ) ) ) {
		ASSERT( GetLastError() == ERROR_NOT_ENOUGH_QUOTA );
		Sleep( 100 );
	}
#endif
}

LRESULT CAVIMasterDlg::HandleUMInsertNew( WPARAM wParam, LPARAM lParam )
{
	return HandleUMInsert( wParam, lParam );
}

LRESULT CAVIMasterDlg::HandleUMInsert( WPARAM wParam, LPARAM lParam )
{
	UMData* p = reinterpret_cast<UMData*>( lParam );

	ASSERT( p->m_htLevel < nMaxLevel );
	HTREEITEM htItem = TVI_ROOT;
	if ( p->m_htLevel > 0 )
		htItem = ahtiBuffer[p->m_htLevel - 1];
	htItem = m_Tree.InsertItem( p->m_sText.c_str(), htItem ); 
	ahtiBuffer[p->m_htLevel] = htItem;
	if ( p->m_bBold ) 
		VERIFY( m_Tree.SetItemState( htItem, TVIS_BOLD, TVIS_BOLD ) ); 
	if ( p->m_bVisible ) {
		HTREEITEM hti = htItem;
		do {
			VERIFY( m_Tree.SetItemImage( hti, 1, 1 ) );  
		} while ( (hti = m_Tree.GetParentItem( hti )) != nullptr );
	} else if ( p->m_htLevel == 0 )
		VERIFY( m_Tree.SetItemImage( htItem, 3, 3 ) );  
	else
		VERIFY( m_Tree.SetItemImage( htItem, 0, 0 ) );  
	NodeInfo* pni = new NodeInfo( "", p->m_nFilePos, p->m_nChunkSize, p->m_nFrame );
	pni->m_fileinfo = m_fileinfo;
//	pni->sText = ToString( (DWORD)htItem ).c_str();
	VERIFY( m_Tree.SetItemData( htItem, reinterpret_cast<DWORD_PTR>( pni ) ) );
	delete p;
	return 0;
}

LRESULT CAVIMasterDlg::HandleUMInsertEnd( WPARAM wParam, LPARAM lParam )
{
	UMData* p = reinterpret_cast<UMData*>( lParam );

	m_nQueueSize--;
	OSStatsSet( 0 );
	StatsSet( CIOService::EPARSERSTATE::PS_READY );
	ASSERT( p->m_htLevel == 1 && m_Tree.GetCount() > 0 );
	HTREEITEM htItem = ahtiBuffer[0];
	int n1, n2;
	VERIFY( m_Tree.GetItemImage( htItem, n1, n2 ) );
	if ( n1 == 3 )
		VERIFY( m_Tree.SetItemImage( htItem, 0, 0 ) ); 
	if ( p->m_sText.length() > 0 ) {
		htItem = m_Tree.InsertItem( _T("Summary"), htItem ); 
		NodeInfo* pni = new NodeInfo( "", -1, -1, -1 );
		pni->m_fileinfo = m_fileinfo;
		CString s = p->m_sText.c_str();
		s.Replace( "\n"), "\r\n" );
		if ( p->m_bBold )
			s += "(+)";
		pni->m_sText = s + "\r\n";
		VERIFY( m_Tree.SetItemData( htItem, reinterpret_cast<DWORD_PTR>( pni ) ) );
	}
	delete p;
	return 0;
}

LRESULT CAVIMasterDlg::HandleUMAppendText( WPARAM wParam, LPARAM lParam )
{
	UMData* p = reinterpret_cast<UMData*>( lParam );

	ASSERT( p->m_htLevel < nMaxLevel && m_Tree.GetCount() > 0 );
	HTREEITEM htItem = TVI_ROOT;
	if ( p->m_htLevel > 0 )
		htItem = ahtiBuffer[p->m_htLevel - 1];
	NodeInfo* pni = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( htItem ) );	
	ASSERT( pni );
	if ( pni ) {
		CString s = p->m_sText.c_str();
		s.Replace( "\n"), "\r\n" );
		if ( p->m_bBold )
			s += "(+)";
		pni->m_sText += s + "\r\n";
	}
	delete p;
	return 0;
}

LRESULT CAVIMasterDlg::HandleUMAppendSize( WPARAM wParam, LPARAM lParam )
{
	UMData* p = reinterpret_cast<UMData*>( lParam );

	int ret = 0;
	ASSERT( p->m_htLevel < nMaxLevel && m_Tree.GetCount() > 0 );
	HTREEITEM htItem = TVI_ROOT;
	if ( p->m_htLevel > 0 )
		htItem = ahtiBuffer[p->m_htLevel];
	NodeInfo* pni = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( htItem ) );	
	ASSERT( pni );
	if ( pni ) {
		ASSERT( pni->m_nChunkSize == (QWORD)-1 || pni->m_nChunkSize == p->m_nChunkSize ); // TODO manchmal werden sie doppelt gesetzt...
#ifdef _DEBUG
		if ( pni->m_nChunkSize != (QWORD)-1 && pni->m_nChunkSize != p->m_nChunkSize ) {
			pni->m_sText = "ACHTUNG SIZE!!!" + pni->m_sText;
			ASSERT( pni->m_nChunkSize != (QWORD)-1 );
			ret = 1;
		}
#endif
		pni->m_nChunkSize = p->m_nChunkSize;
	}
	delete p;
	return ret;
}

LRESULT CAVIMasterDlg::HandleUMAppendImportant( WPARAM wParam, LPARAM lParam )
{
	UMData* p = reinterpret_cast<UMData*>( lParam );
	int ret = 0;
	ASSERT( p->m_htLevel < nMaxLevel && m_Tree.GetCount() > 0 && p->m_htLevel > 0 );
	HTREEITEM htItem = TVI_ROOT;
	htItem = ahtiBuffer[p->m_htLevel];
	VERIFY( m_Tree.SetItemState( htItem, TVIS_BOLD, TVIS_BOLD ) ); 
	return ret;
}

LRESULT CAVIMasterDlg::HandleUMStatsSet( WPARAM wParam, LPARAM lParam )
{
	StatsSet( static_cast<CIOService::EPARSERSTATE>( lParam ) );
	return 0;
}


CCriticalSection m_CritSection;
/*static*/ UINT CAVIMasterDlg::MyThreadProc( LPVOID pParam ) 
{
	CSingleLock singleLock(&m_CritSection);
	singleLock.Lock();
	ThreadInfo* ti = reinterpret_cast<ThreadInfo*>( pParam );
	DoWork( ti->Settings, ti->m_sReadFilePath, ti->m_sWriteFilePath );
	delete ti;
	singleLock.Unlock();
	return 0;
}

void CAVIMasterDlg::OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult)
{
	int nSel = m_TabCtrl.GetCurSel();

//	if(m_Dialog[nSel]->m_hWnd)
//
	m_pTabPage1->ShowWindow(SW_HIDE);
	m_pTabPage2->ShowWindow(SW_HIDE);
//	m_pTabPage3->ShowWindow(SW_HIDE);

	m_pTabPage1->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE );
	m_pTabPage2->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE );
//	m_pTabPage3->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE );

	if ( nSel == 0 ) {
		m_pTabPage1->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );
		m_pTabPage1->ShowWindow(SW_SHOW);
	} else if ( nSel == 1 ) {
		m_pTabPage2->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );
		m_pTabPage2->ShowWindow(SW_SHOW);
	} else if ( nSel == 2 ) {
		m_pTabPage3->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );
		m_pTabPage3->ShowWindow(SW_SHOW);
	} else
		ASSERT( false );

	*pResult = 0;
}

BOOL CAVIMasterDlg::DestroyWindow()
{
	RECT r;
	GetWindowRect( &r );
	AfxGetApp()->WriteProfileInt( "Settings", "MainWndX", r.left );
	AfxGetApp()->WriteProfileInt( "Settings", "MainWndY", r.top );
	AfxGetApp()->WriteProfileInt( "Settings", "MainWndW", r.right - r.left );
	AfxGetApp()->WriteProfileInt( "Settings", "MainWndH", r.bottom - r.top );

	delete m_pTabPage1;
	delete m_pTabPage2;
//	delete m_pTabPage3;
	return CDialog::DestroyWindow();
}

void CAVIMasterDlg::OnTvnDeleteitemMainTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	NodeInfo* p = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( pNMTreeView->itemOld.hItem ) );
//	delete[] p->m_pData; braucht nicht gelöscht werden...
	delete p;
	// TODO ggf. m_FileInfo einmalig löschen
	*pResult = 0;
}

void CAVIMasterDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	SetSize( cx, cy );
}

void CAVIMasterDlg::OnGetMinMaxInfo( MINMAXINFO* lpMMI )
{
	lpMMI->ptMinTrackSize = CPoint( 600, 300 );
}

void CAVIMasterDlg::SetSize(int cx, int cy)
{
	static constexpr int nTabWidth = 420; // Ausrechnen Breite einer HexEdit-Zeile
	if ( m_pTabPage1 && m_pTabPage1->m_hWnd ) {
		CRect r;
		m_Tree.MoveWindow( 10, 10, cx - 40 - nTabWidth, cy - 35 );
		m_TabCtrl.MoveWindow( cx - nTabWidth - 10, 10, nTabWidth, cy - 70 );
		m_TabCtrl.GetWindowRect( &r );
		ScreenToClient( &r );
		m_pTabPage1->MoveWindow( cx - nTabWidth - 10 + 2, r.top + 25, nTabWidth - 6, cy - 99 );
		m_pTabPage2->MoveWindow( cx - nTabWidth - 10 + 2, r.top + 25, nTabWidth - 6, cy - 99 );
//		m_pTabPage3->MoveWindow( cx - nTabWidth - 10 + 2, r.top + 25, nTabWidth - 6, cy - 99 );
		GetDlgItem( ID_MAIN_CLOSE )->MoveWindow( cx - 80, cy - 50, 70, 25, TRUE );
		GetDlgItem( ID_MAIN_OPTIONS )->MoveWindow( cx - 160, cy - 50, 70, 25, TRUE );
		GetDlgItem( ID_MAIN_OPEN_DEMUX )->MoveWindow( cx - 240, cy - 50, 70, 25, TRUE );
		GetDlgItem( ID_MAIN_OPEN_WRITEAVI )->MoveWindow( cx - 320, cy - 50, 70, 25, TRUE );
		GetDlgItem( ID_MAIN_OPEN_PARSE )->MoveWindow( cx - 400, cy - 50, 70, 25, TRUE );

		// otherwise redraw errors on buttons occur
		GetDlgItem( ID_MAIN_CLOSE )->Invalidate();
		GetDlgItem( ID_MAIN_OPTIONS )->Invalidate();
		GetDlgItem( ID_MAIN_OPEN_DEMUX )->Invalidate();
		GetDlgItem( ID_MAIN_OPEN_WRITEAVI )->Invalidate();
		GetDlgItem( ID_MAIN_OPEN_PARSE )->Invalidate();

		CRect client, remaining;
		GetClientRect(&client);
		RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, AFX_IDW_PANE_FIRST, CWnd::reposQuery, &remaining, &client);
		RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, AFX_IDW_PANE_FIRST, CWnd::reposDefault, nullptr, &client);
	    m_statusProgress.Reposition();
	}
}

void CAVIMasterDlg::OnOK() 
{
}

void CAVIMasterDlg::OnCancel() 
{
}

void CAVIMasterDlg::OnBnClickedMainClose()
{
	CDialog::OnOK();
}

void CAVIMasterDlg::OnBnClickedMainOptions()
{
	CDlgOptions dlg( Settings );
	if ( dlg.DoModal() == IDOK ) {
		((CTabHexedit*)m_pTabPage2)->m_Hexedit.SetReadOnly( !Settings.bHexEdit );
	}
}

 // ------------------------------------

void CAVIMasterDlg::OnCMDelete()
{
	m_Tree.DeleteItem( m_Tree.GetSelectedItem() );
}

void CAVIMasterDlg::OnCMPlayavi()
{
	struct NodeInfo* p = (struct NodeInfo*)m_Tree.GetItemData( m_Tree.GetSelectedItem() );
	if ( p && p->m_fileinfo ) {
		ShellExecute( nullptr, "open", p->m_fileinfo->m_sFilepath, "", "", SW_SHOW ); 

#if 0
		if ( p->mAVIFile->m_sOFilename.length() > 0 )
			ShellExecute( nullptr, "open", p->AVIFile->m_sOFilename.c_str(), "", "", SW_SHOW ); 
		else
			ShellExecute( nullptr, "open", p->AVIFile->m_sIFilename.c_str(), "", "", SW_SHOW );
#endif
	}
#if 0
	if ( p ) {
		if ( p->AVIFile->m_sOFilename.length() > 0 )
			PlayFile( p->AVIFile->m_sOFilename.c_str(), &m_dPict ); // .....
		else
			PlayFile( p->AVIFile->m_sIFilename.c_str(), &m_dPict );
	}
#endif
}

void CAVIMasterDlg::StatsSet( CIOService::EPARSERSTATE eParserState )
{
	CString s;
	switch ( eParserState ) {
	case CIOService::EPARSERSTATE::PS_PARSING:
		s = _T("Parsing");
		break;
	case CIOService::EPARSERSTATE::PS_INDEXCHECK:
		s = _T("Checking Index");
		break;
	case CIOService::EPARSERSTATE::PS_CLEANUP:
		s = _T("Cleanup");
		break;
	case CIOService::EPARSERSTATE::PS_WRITING:
		s = _T("Writing");
		break;
	case CIOService::EPARSERSTATE::PS_READY:
		s = _T("Ready");
		break;
	default:
		ASSERT( false );
		s = _T("Unknown");
		break;
	}
	/*VERIFY( */m_wndStatusBar.SetPaneText( 0, s )/* )*/;
	CString t;
	t.Format( "Queue: %d", m_nQueueSize );
	/*VERIFY( */m_wndStatusBar.SetPaneText( 1, t )/* )*/;
	
	m_wndStatusBar.Invalidate();
}

bool CAVIMasterDlg::SaveHexeditChunk( const NodeInfo* p )
{
	ASSERT( p && p->m_fileinfo );
	iow w;
	if ( !w.OpenExisting( p->m_fileinfo->m_sFilepath.GetBuffer() ) )
		return false;
	DWORD len = p->m_nDataSize;
	BYTE* buffer = new BYTE[len];
	((CTabHexedit*)m_pTabPage2)->m_Hexedit.GetData( buffer, len );
	VERIFY( w.Seek( p->m_nFilePos ) );
	VERIFY( w.Read( p->m_pData, len ) );
	if ( !memcmp( p->m_pData, buffer, len ) ) { // TODO
		AfxMessageBox( "Internal Error. Save not possible!", MB_ICONERROR | MB_OK );
		goto e;
	}
	VERIFY( w.Seek( p->m_nFilePos ) );
	VERIFY( w.Write( buffer, len, false ) );
e:	VERIFY( w.Close() );
	memcpy( p->m_pData, buffer, len );
	delete[] buffer;
	return true;
}

bool CAVIMasterDlg::SaveAsHexeditChunk( const NodeInfo* p, const _tstring& sFilepath )
{
	ASSERT( p && p->m_fileinfo );
	iow w;
	if ( !w.OpenCreate( sFilepath ) )
		return false;
	DWORD len = p->m_nDataSize;
	BYTE* buffer = new BYTE[len];
	((CTabHexedit*)m_pTabPage2)->m_Hexedit.GetData( buffer, len );
	VERIFY( w.Write( buffer, len, false ) );
	VERIFY( w.Close() );
	delete[] buffer;
	return true;
}

bool CAVIMasterDlg::LoadHexeditChunk( const NodeInfo* p, const _tstring& sFilepath )
{
	ASSERT( p && p->m_fileinfo );
	ior r;
	if ( !r.Open( sFilepath ) )
		return false;
	DWORD len = p->m_nDataSize;
	BYTE* buffer = new BYTE[len];
	if ( r.GetFileSize() != len ) {
		AfxMessageBox( "File size does not match chunk size!", MB_ICONERROR | MB_OK );
		goto e;
	}
	if ( !r.Read( buffer, len ) ) {
		goto e;
	}
	((CTabHexedit*)m_pTabPage2)->m_Hexedit.SetData( p->m_nFilePos, buffer, len );
e:	VERIFY( r.Close() );
	memcpy( p->m_pData, buffer, len );
	delete[] buffer;
	return true;
}

bool CAVIMasterDlg::HexeditSave()
{
	if ( m_Tree.GetSelectedItem() ) { // TODO MessageBox
		NodeInfo* p = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( m_Tree.GetSelectedItem() ) );
		if ( p && p->m_fileinfo )
			return SaveHexeditChunk( p );
		else
			ASSERT( false );
	} else
		ASSERT( false );
	return false;
}

bool CAVIMasterDlg::HexeditSaveAs()
{
	if ( m_Tree.GetSelectedItem() ) { // TODO MessageBox
		NodeInfo* p = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( m_Tree.GetSelectedItem() ) );
		if ( p && p->m_fileinfo ) {
			const CString w =
				_T("Binary Files|*.bin|All Files (*.*)|*.*||");
			CFileDialog d( false, _T(".bin"), FilenameExtensionReplace( p->m_fileinfo->m_sFilepath.GetBuffer(), _T("_") + ToString( p->m_nFilePos ) + _T(".bin") ).c_str(), OFN_CREATEPROMPT | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST, w );
			if ( d.DoModal() == IDOK ) {
				_tstring s = d.GetPathName().GetString();
				return SaveAsHexeditChunk( p, s );
			}
		} else
			ASSERT( false );
	} else
		ASSERT( false );
	return false;
}

bool CAVIMasterDlg::HexeditLoad()
{
	if ( m_Tree.GetSelectedItem() ) { // TODO MessageBox
		NodeInfo* p = reinterpret_cast<NodeInfo*>( m_Tree.GetItemData( m_Tree.GetSelectedItem() ) );
		if ( p && p->m_fileinfo ) {
			const CString w =
				_T("Binary Files|*.bin|All Files (*.*)|*.*||");
			CFileDialog d( true, _T(".bin"), FilenameExtensionReplace( p->m_fileinfo->m_sFilepath.GetBuffer(), _T("_") + ToString( p->m_nFilePos ) + _T(".bin") ).c_str(), OFN_FILEMUSTEXIST, w );
			if ( d.DoModal() == IDOK ) {
				_tstring s = d.GetPathName().GetString();
				return LoadHexeditChunk( p, s );
			}
		} else
			ASSERT( false );
	} else
		ASSERT( false );
	return false;
}


void CAVIMasterDlg::OnClose()
{
	CDialog::OnOK(); // make Close Button working
//	CDialog::OnClose();
}
