#include "Helper.h"
#ifdef _CONSOLE
#pragma warning(push, 3)
#include <iostream>
#include <fcntl.h>
#pragma warning(pop)
#else
#include "AVIMasterDlg.h"
#endif

#include "IOBase.h"
#include "Main.h"
#include "IOService.h"
#include "AVIParser.h"
#include "ASFParser.h"
#include "MOVParser.h"
#include "RMFParser.h"
#include "MPGParser.h"
#include "RIFFParser.h"
#include "RIFFSpecParser.h"
#include "MKVParser.h"

#if defined(LINUX32) || defined(LINUX64)
#include <unistd.h>
#define _isatty isatty
#define _fileno fileno
#endif

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif


#ifdef _CONSOLE
#if defined(_WIN32) || defined(_WIN64)
__declspec(noreturn)
#endif
void Usage( _tostream& o, bool bAll )
{
#if defined(_WIN32) || defined(_WIN64)
	o << "AVIMaster: Usage: avimaster -opt1 -opt2 ... filespec*.avi [out.avi]" << std::endl;
#else
	o << "AVIMaster: Usage: avimaster -opt1 -opt2 ... file.avi [out.avi]" << std::endl;
#endif
	o << " -t####  Parsing Trace: #### Number of 'movi','indx','idx1'-Entries shown (5)" << std::endl;
	if ( bAll ) {
	o << " -tm#### Parsing Trace: #### Number of 'movi'-Entries shown" << std::endl;
	o << " -ti#### Parsing Trace: #### Number of 'indx'-Entries shown" << std::endl;
	o << " -tx#### Parsing Trace: #### Number of 'idx1'-Entries shown" << std::endl;
	}
	o << " -v[#]   Parsing Trace: Show 1:Parse/2:Dumps/3:Verbose/4,5,6:Debug (2)" << std::endl;
	o << " -p####  Fastmode (only partial Parsing), Parse #### 'movi'-entries (all)" << std::endl;
	o << " -c      Don't Check 'idx1'-and 'indx'-Entries against 'movi'-Entries (on)" << std::endl;
	o << " -mr     Force parsing with Intel byte ordering, -mx with Network Byte Ordering (Apple)" << std::endl; // RMF, QT, RIFF
#ifdef _DEBUG
	o << " -mt     Force parsing as MPEG2 file (Program or Transport Stream)" << std::endl;
#endif
	o << " -n      Handle Chunk Size Errors differently (Subchunk enforces parent size)" << std::endl;
	o << " -h      Show extended help" << std::endl;
	o << std::ends;
	exit( 1 );
}
#endif

static QWORD DoWork(class Settings Settings, const _tstring& sIFilename, QWORD& nChecksum);
static bool Recurse(CommandLine::WorkList& worklist, _tstring sReadFileSpec, _tstring sDir);

std::string sProgramTitle = "AVIMaster 1.6.0";

#ifdef _CONSOLE
int __cdecl _tmain(int argc, _TCHAR* argv[] )
{
#if defined(_WIN32) || defined(_WIN64)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_CRT_DF |*/ _CRTDBG_LEAK_CHECK_DF );
	win::COORD c;
	c.X = 120;
	c.Y = 9999;
	/*VERIFY(*/ win::SetConsoleScreenBufferSize( win::GetStdHandle( STD_ERROR_HANDLE ), c ) /*)*/; 
//	VERIFY( win::SetConsoleOutputCP( 1252 ) ); / don't forget to set font to "TT Lucida Console"
//	VERIFY( win::SetConsoleCP( 1252 ) ); // I could not find how to switch CP to Unicode...
//	VERIFY( win::SetFileApisToANSI() );
#endif

#ifdef _DEBUG
	WORD w = 0x0102;
	ASSERT( *reinterpret_cast<BYTE*>( &w ) == 0x02 ); // We are on a little-endian machine
	ASSERT( FlipBytes( &w ) == 0x0201 ); // Flip does work
#endif

#ifdef _UNICODE
	if ( !_isatty(_fileno(stdout ) ) )
		_setmode(_fileno(stdout), _O_WTEXT );
//	_setmode( _fileno( stderr ), _O_BINARY );
#endif

	_tcerr << ToString( sProgramTitle ) + _T(" (c)2004-2020 by thozie productions, Thomas Ziegler, München") << std::endl;
	_tcerr << _T("Contact and Bug Reports: www.thozie.de/avimaster, avimaster@thozie.de") << std::endl;
	if ( !_isatty(_fileno(stdout ) ) ) {
		_tcout << std::endl << ToString( sProgramTitle ) + ToString( _T(" (c)2004-2020 by thozie productions, Thomas Ziegler, München") ) << std::endl;
		_tcout << _T("Contact and Bug Reports: www.thozie.de/avimaster, avimaster@thozie.de") << std::endl;
	}

	if ( argc <= 1 ) {
		Usage( _tcout, false );
	}

	class Settings Settings;
	CommandLine cl( Settings, argc, argv );
	switch ( cl.GetError() ) {
	case CommandLine::CLError::CL_MESSAGE:
			_tcout << cl.GetMessage() << std::endl;
			exit( 4 );
		case CommandLine::CLError::CL_USAGE:
			Usage( _tcout, true );
	}

	QWORD nTotalSize = 0; int nCount = 0;
#if defined(_WIN32) || defined(_WIN64)
	double fTotalCounts = 0;
	win::LARGE_INTEGER nFrequency, nCounterStart, nCounterStop;
	win::QueryPerformanceFrequency( &nFrequency );
#endif

	std::unordered_map<QWORD, std::vector<_tstring>> dups;

	CommandLine::WorkList wl = cl.GetWorkList();
	for ( int i = 0; i < wl.size(); i++ ) {

#if defined(_WIN32) || defined(_WIN64)
		win::QueryPerformanceCounter( &nCounterStart );
#endif
		QWORD nChecksum = 0;
		nTotalSize += DoWork( Settings, wl[i], nChecksum);
		++nCount;
		if (nChecksum > 0)
		{
			auto x = dups.find(nChecksum);
			if(x != dups.end())
				x->second.push_back(wl[i]);
			else {
				std::vector<_tstring> v;
				v.push_back(wl[i]);
				dups[nChecksum] = v;
			}
		}

#if defined(_WIN32) || defined(_WIN64)
		win::QueryPerformanceCounter( &nCounterStop );
		fTotalCounts += (double)(nCounterStop.QuadPart - nCounterStart.QuadPart) / nFrequency.QuadPart;
#endif

#if defined(_WIN32) || defined(_WIN64)
		if ( _isatty(_fileno(stdout ) ) ) {
			_tcout << _T("Next File... [ENTER]") << std::endl;
			_TCHAR s;
			_tcin.get( s );
		}
#endif
	}

#if defined(_WIN32) || defined(_WIN64)
	fTotalCounts = ((double)((int)(fTotalCounts * 100))) / 100;
	_tcout << std::endl << "Total Files processed: " << nCount << ", Total Size: " << nTotalSize << " (" << nTotalSize / 1024 / 1024 << "MB)" << ", Elapsed Time [s]: " << fTotalCounts << std::endl;
#else
	_tcout << std::endl << "Total Files processed: " << nCount << ", Total Size: " << nTotalSize << " (" << nTotalSize / 1024 / 1024 << "MB)" << std::endl;
#endif


	for (const auto& v : dups) {
		auto f = v.second;
		if (f.size() > 1)
		{
			for (const auto& x : f)
				_tcout << std::endl << "Duplicates: " << x;
			_tcout << std::endl;
		}
	}


	if (_isatty(_fileno(stdout))) {
		_tcout << "End... [ENTER]" << std::endl;
		_TCHAR s;
		_tcin.get(s);
	}
	else
		_tcerr << std::endl;

	if (!_isatty(_fileno(stdout)))
		_tcerr << std::endl;
	return 0;
}
#endif

#if defined(_WIN32) || defined(_WIN64)
CommandLine::CommandLine( class Settings& Settings, win::LPTSTR lpCommandLine )
{
	m_eCLError = CLError::CL_OK;
	int argc = 1;
	_TCHAR *argv[100]; // HACK
	_TCHAR *p = lpCommandLine;
	_TCHAR *pb = p;
	bool bQuote = false;
	argv[0] = nullptr;
	while ( *p ) {
		if ( *p == _T('\"') )
			bQuote = !bQuote;
		if ( !bQuote && *p == _T(' ') ) {
			if ( pb + 1 < p )
				argv[argc++] = pb;
			pb = p + 1;
			*p = _T('\0');
		}
		p++;
	}
	argv[argc++] = pb;
	Parse( Settings, argc, argv );
}
#endif

CommandLine::CommandLine( class Settings& Settings, int argc, _TCHAR* argv[] )
{
	m_eCLError = CLError::CL_OK;
	Parse( Settings, argc, argv );
}

void CommandLine::Parse( class Settings& Settings, int argc, _TCHAR* argv[] )
{
	bool bOverwrite = false;
	int nFilecount = 0;
	_tstring sReadFileSpec;
	for ( int i = 1; i < argc && m_eCLError == CLError::CL_OK; ++i ) {
		if ( argv[i][0] != _T('-') ) {
			if ( nFilecount == 0 ) {
				sReadFileSpec = argv[i];
				nFilecount = 1;
			} else
				m_eCLError = CLError::CL_USAGE;
		} else {
			int j = -1;
			if ( _tcslen( argv[i] ) > 2 )
				j = _tstoi( argv[i] + 2 );
			switch( tolower( argv[i][1] ) ) {
			case _T('v'): // -v Verbosity
				Settings.nVerbosity = (_tcslen( argv[i] ) > 2 ? j : 1);
				break;
			case _T('p'): // -pnnnn Fastmode
				Settings.nFastmodeCount = (DWORD)j;
				break;
			case _T('n'): // -n Subchunk Lengh
				Settings.bForceChunkSize = true;
				break;
			case _T('m'): // -m Force RIFF
				if ( _tcslen( argv[i] ) != 3 )
					m_eCLError = CLError::CL_USAGE;
				switch ( argv[i][2] ) {
				case _T('x'):
					Settings.eForce = Settings::EFORCE::EM_FORCERIFFNETWORK;
					break;
				case _T('i'):
					Settings.eForce = Settings::EFORCE::EM_FORCERIFFINTEL;
					break;
				case _T('t'):
					Settings.eForce = Settings::EFORCE::EM_FORCEMPEG;
					break;
				default:
					m_eCLError = CLError::CL_USAGE;
					break;
				}
				break;
			case _T('k'): // -k Tight
				Settings.bTight = true;
				break;
			case _T('o'): // -o Overwrite
				bOverwrite = true;
				break;
			case _T('h'): // -h Extended Help
				m_eCLError = CLError::CL_USAGE;
				break;
			case _T('f'): // -fvnn.nn Set fps and adjust Audio Length als, -fa fix audio
				if ( _tcslen( argv[i] ) < 3 )
					m_eCLError = CLError::CL_USAGE;
				switch ( argv[i][2] ) {
				case _T('v'):
					if ( _tcslen( argv[i] ) < 3 || !isdigit( argv[i][3] ) )
						m_eCLError = CLError::CL_USAGE;
					Settings.fSetVideoFPS = _tstof( argv[i] + 3 );
					if ( Settings.fSetVideoFPS < 1.0 || Settings.fSetVideoFPS >= 100.0 )
						m_eCLError = CLError::CL_USAGE;
					break;
				case _T('a'):
					Settings.bFixAudio = true;
					break;
				default:
					m_eCLError = CLError::CL_USAGE;
					break;
				}
				break;
			case _T('t'): // -txnnnn
				if ( _tcslen( argv[i] ) < 2 )
					m_eCLError = CLError::CL_USAGE;
				if ( Settings.nVerbosity == 0 )
					Settings.nVerbosity = 1;
				if ( isdigit( argv[i][2] ) ) { // -tnnnn
					Settings.nTraceMoviEntries = (DWORD)j;
					Settings.nTraceIdx1Entries = (DWORD)j;
					Settings.nTraceIndxEntries = (DWORD)j;
					break;
				}
				if ( _tcslen( argv[i] ) < 4 || !isdigit( argv[i][3] ) )
					m_eCLError = CLError::CL_USAGE;
				j = _tstoi( argv[i] + 3 );
				switch ( tolower( argv[i][2] ) ) {
				case _T('m'):
					Settings.nTraceMoviEntries = (DWORD)j;
					break;
				case _T('x'):
					Settings.nTraceIdx1Entries = (DWORD)j;
					break;
				case _T('i'):
					Settings.nTraceIndxEntries = (DWORD)j;
					break;
				default:
					m_eCLError = CLError::CL_USAGE;
					break;
				}
				break;
			case _T('c'): // -c Check Index
				Settings.bCheckIndex = false;
				break;
			default:
				m_eCLError = CLError::CL_USAGE;
				break;
			}
		}
	}
	if ( m_eCLError != CLError::CL_OK )
		return;

	_TCHAR sDrive[_MAX_DRIVE], sPath[_MAX_PATH], sFName[_MAX_FNAME], sExt[_MAX_EXT], fullpath[_MAX_PATH];
	_tsplitpath( _tfullpath( fullpath, sReadFileSpec.c_str(), _MAX_PATH ), sDrive, sPath, sFName, sExt );

	_tstring sDir = sDrive;
	sDir += sPath;
	_tstring sFile = sFName;
	sFile += sExt;

	Recurse(m_vWorkList, sFile, sDir);

}

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static bool Recurse(CommandLine::WorkList& worklist, _tstring sReadFileSpec, _tstring sDir)
{
	if (sReadFileSpec.find_first_of(_T("*?")) == _tstring::npos) 
	{
		worklist.push_back(sDir + sReadFileSpec);
		return true;
	}
	for (const auto& entry : fs::directory_iterator(sDir)) {
		if (entry.is_directory()) 
			Recurse(worklist, sReadFileSpec, entry.path());
		else if (entry.is_regular_file()) 
			worklist.push_back(entry.path());
	}
	return true;
}

static QWORD DoWork( Settings Settings, const _tstring& sIFilename, QWORD& nChecksum)
{
#ifdef _CONSOLE
	_tcout << std::endl;
#endif
	class ior ior;
	BYTE buffer[12];
	QWORD nFileSize = 0;
	bool bOk = true;

	if ( ior.Open( sIFilename ) ) {
		nFileSize = ior.GetFileSize();
		if ( ior.GetFileSize() >= sizeof( buffer ) ) 
			bOk &= ior.ReadN( buffer, sizeof( buffer ) );
		bOk &= ior.Close();
	}
	if ( !bOk )
		return 0;

	if ( CAVIParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CAVIParser p( io );
		p.Parse( sIFilename );
	} else if ( CMOVParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CMOVParser p( io );
		p.Parse( sIFilename );
		nChecksum = p.GetChecksum();
	} else if ( CRMFParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CRMFParser p( io );
		p.Parse( sIFilename );
	} else if ( CASFParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CASFParser p( io );
		p.Parse( sIFilename );
		nChecksum = p.GetChecksum();
	} else if ( CRIFFSpecParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CRIFFSpecParser p( io );
		p.Parse( sIFilename );
	} else if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFINTEL || Settings.eForce == Settings::EFORCE::EM_FORCERIFFNETWORK || CRIFFParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CRIFFParser p( io );
		p.Parse( sIFilename );
	} else if ( Settings.eForce == Settings::EFORCE::EM_FORCEMPEG || CMPGParser::Probe( buffer ) ) {
		CIOService io(Settings);
		CMPGParser p( io );
		p.Parse( sIFilename );
	} else if ( CMKVParser::Probe(buffer) ) {
		CIOService io(Settings);
		CMKVParser p(io);
		p.Parse(sIFilename);
	}
	else {
		Settings::EFORCE e = Settings.eForce;
		Settings.eForce = Settings::EFORCE::EM_FORCEMPEG; // Second Try Forced MPEG...
		if ( CMPGParser::Probe( buffer ) ) { // kann nicht false sein
			CIOService io(Settings);
			CMPGParser p( io );
			p.Parse( sIFilename );		
			Settings.eForce = e;
		} else {
			CIOService ios( Settings );
			_tstring sSummary;
			int hti = ios.OSNodeFileNew( 0, sIFilename );
			int htitmp =
				ios.OSNodeCreate( hti, 0, _T("Probe") ); // TODO ToString(fcc)
			ios.DumpBytes( buffer, 12, htitmp );
			ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			const DWORD* fcc = reinterpret_cast<const DWORD*>( buffer ); 

			if ( buffer[0] == 'P' && buffer[1] == 'K' ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably ZIP (.zip)") );
				sSummary = _T("This might be a .ZIP file.");
			} else if ( buffer[0] == 'M' && buffer[1] == 'Z' ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably a Windows Executable (.exe)") );
				sSummary = _T("This might be a .EXE file.");
			} else if ( buffer[0] == 0xbc || buffer[0] == 0xe9 ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably a Windows Executable (.com)") );
				sSummary = _T("This might be a .COM file.");
			} else if ( fcc[0] == 0x46445025 ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably Adobe PDF (.pdf)") );
				sSummary = _T("This might be a .PDF file.");
			} else if ( fcc[0] == 0xe0ffd8ff ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably JPG (.jpg)") );
				sSummary = _T("This might be a .JPG file.");
	#if 0
			} else if ( fcc[1] == 0x766f6f6d /*'moov'*/ || fcc[1] == 0x746f6e70 /*pnot - preview node, could by anything MACish*/ ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably Quicktime (.mov)") );
				sSummary = _T("This might be a .MOV (Quicktime) file.");
			} else if ( fcc[0] == 0x464d522e ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably Real Media (.rm)") );
				sSummary = _T("This might be a .RM (Real Media) file.");
	#endif
			} else if ( fcc[0] == 0x5367674f ) {
				ios.OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably Ogg Vorbis (.ogg)") );
				sSummary = _T("This might be a Ogg Vorbis file.");
			}
			else if (fcc[0] == 0x2000d8b7) { // Header 0xB7 0xD8 0x00 0x20 0x37 0x49 0xDA 0x11 0xA6 0x4E 0x00 0x07 0xE9 0x5E 0xAD 0x8D
				ios.OSNodeCreateAlert(htitmp, CIOService::EPRIORITY::EP_INFO, _T("File is presumably Windows Recorded TV Show (.wtv)"));
				sSummary = _T("This might be a Windows Recorded TV Show file.");
			} else
				sSummary = _T("This file is not recognized.");
			ios.OSNodeFileEnd( hti, sSummary );
		}
	}
	return nFileSize;
}
