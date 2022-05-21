#include "Helper.h"
#include "IOService.h"
#ifndef _CONSOLE
#include "AVIMasterDlg.h"
#endif
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

#ifndef _CONSOLE
CAVIMasterDlg* CIOService::m_pDialog = nullptr;

void CIOService::SetDialog(CAVIMasterDlg* pDialog) { m_pDialog = pDialog; }
void CIOServiceHelper::SetDialog(CAVIMasterDlg* pDialog) { CIOService::SetDialog(pDialog); }
#endif

int CIOService::OSNodeCreateValue( int htiparent, const _tstring& s ) const
{
#ifdef _CONSOLE
	_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << s << std::endl;
	HandleUnicodeError( _tcout );
	return htiparent + 1;
#else
	return m_pDialog->OSNodeCreateValue( s.c_str(), htiparent, nullptr );
#endif
}

int CIOService::OSNodeCreateValue( int htiparent, const _tstring& s, bool bBold ) const
{
#ifdef _CONSOLE
	if ( Settings.nVerbosity >= 3 || bBold ) {
		_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << (bBold ? _T("+ ") : _T("- ")) << s << std::endl;
		HandleUnicodeError( _tcout );
	}
	return htiparent + 1;
#else
	return m_pDialog->OSNodeCreateValue( s.c_str(), htiparent, bBold );
#endif
}

int CIOService::OSNodeAppendSize( int htiparent, QWORD nSize ) const
{
	if ( Settings.nVerbosity == 0 ) 
		return htiparent;
#ifdef _CONSOLE
	if ( Settings.nVerbosity >= 2 ) {
		_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << _T("-> Chunk Size: ") + ToString( nSize ) << std::endl;
	}
	return htiparent + 1;
#else
	return m_pDialog->OSNodeAppendSize( nSize, htiparent );
#endif
}

int CIOService::OSNodeAppendImportant( int htiparent ) const
{
#ifdef _CONSOLE
	return htiparent; // TODO ???
#else
	return m_pDialog->OSNodeAppendImportant( htiparent );
#endif
}

int CIOService::OSNodeCreateAlert( int htiparent, CIOService::EPRIORITY ePriority, const _tstring& sText ) const 
{
	_tstring s;
	switch ( ePriority ) {
		case CIOService::EPRIORITY::EP_NONE:
			s = sText;
			break;
		case CIOService::EPRIORITY::EP_INFO:
			s = _T("Info: ") + sText;
			break;
		case CIOService::EPRIORITY::EP_WARNING:
			s = _T("Warning: ") + sText;
			break;
		case CIOService::EPRIORITY::EP_ERROR:
			s = _T("Error: ") + sText;
			break;
		case CIOService::EPRIORITY::EP_FATAL:
			s = _T("Fatal: ") + sText;
			break;
		default:
			ASSERT( false );
			break;
	}
#ifdef _CONSOLE
	_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << _T("*** ") << s << std::endl;
	HandleUnicodeError( _tcout );
	return htiparent + 1;
#else
	return m_pDialog->OSNodeCreateAlert( s.c_str(), htiparent, (ePriority != CIOService::EPRIORITY::EP_WARNING) );
#endif
}

int CIOService::OSNodeCreate( int htiparent, QWORD nPos, const _tstring& sText, const _tstring& sToken1, QWORD nSize, const _tstring& sToken2 ) const
{
_tstringstream s;

	if ( Settings.nVerbosity == 0 ) 
		return htiparent;

#ifdef _CONSOLE
	if ( Settings.nVerbosity > 2 ) {
#else
	if ( false ) {
#endif
		if ( !sToken2.empty() ) 
			s << sToken1 << _T(" (S:") << nSize << _T(", P:") << nPos << _T(") ") << sToken2; // s.Format( "'%s' (S:%u, P:%I64d) '%s'", FCCToString( fcc1 ), nSize, nPos, FCCToString( fcc2 ) );
		else if ( nSize != (QWORD)-1 )
			s << sToken1 << _T(" (S:") << nSize << _T(", P:") << nPos << _T(")"); // s.Format( "'%s' (S:%u, P:%I64d)", FCCToString( fcc1 ), nSize, nPos );
		else if ( !sToken1.empty() || sText.empty() )
			s << sToken1; // s.Format( "'%s'", FCCToString( fcc1 ) );
	} else {
		if ( !sToken2.empty() ) 
			s << sToken1 << _T(" (") << nSize << _T(") ") << sToken2; // s.Format( "'%s' (%u) '%s'", FCCToString( fcc1 ), nSize, FCCToString( fcc2 ) );
		else if ( nSize != (QWORD)-1 )
			s << sToken1 << _T(" (") << nSize << _T(")"); // s.Format( "'%s' (%u)", FCCToString( fcc1 ), nSize );
		else if ( !sToken1.empty() || sText.empty() )
			s << sToken1; // s.Format( "'%s'", FCCToString( fcc1 ) );
	}

	if ( !sText.empty() ) {
		if ( s.tellp() > 0 )
			s << _T(": ");
		s << sText;
	}
//	s << std::ends;

#ifdef _CONSOLE
	_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << s.str() << std::endl;
	HandleUnicodeError( _tcout );
	return htiparent + 1;
#else
	return m_pDialog->OSNodeCreate( s.str().c_str(), htiparent, nPos, nSize, (DWORD)-1 ); // TODO nFrame fehlt
#endif
}

int CIOService::OSNodeCreate( int htiparent, const _tstring& sText ) const
{
#ifdef _CONSOLE
	_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << sText << std::endl;
	HandleUnicodeError( _tcout );
	return htiparent + 1;
#else
	return m_pDialog->OSNodeCreate( sText.c_str(), htiparent );
#endif
}

int CIOService::OSNodeFileNew( int htiparent, const _tstring& sText ) const 
{
#ifdef _CONSOLE
	_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << sText << std::endl;
	HandleUnicodeError( _tcout );
	return htiparent + 1;
#else
	return m_pDialog->OSNodeFileNew( sText.c_str(), htiparent );
#endif
}

int CIOService::OSNodeFileEnd( int htiparent, const _tstring& sText ) const // Summary Text
{
#ifdef _CONSOLE
	_tcout << _tstring( (size_t)(htiparent * 2), _T(' ') ) << sText << std::endl;
	HandleUnicodeError( _tcout );
	return htiparent + 1;
#else
	return m_pDialog->OSNodeFileEnd( sText.c_str(), htiparent );
#endif
}

#ifdef _UNICODE
/*static*/ void CIOService::HandleUnicodeError( _tostream& o )
{
	if ( !_tcout.good() ) {
		ASSERT( _tcout.good() );
		o.clear(); // clear Error
		o << _T("...<can't display UNICODE>...") << std::endl;
	}
}
#endif


#ifdef _CONSOLE
void CIOService::OSStatsInit( QWORD /*nSize*/ ) const 
#else
void CIOService::OSStatsInit( QWORD nSize ) const 
#endif
{
#ifdef _CONSOLE
	m_nStatCount = 0;
#else
	m_pDialog->OSStatsInit( nSize ); // nicht mit GetReadFileSize() ersetzen, sonst kann man nicht Tricksen wenn man nur einen kleinen Teil parst
#endif
}
#ifdef _CONSOLE
void CIOService::OSStatsSet( QWORD /*nPos*/ ) const 
#else
void CIOService::OSStatsSet( QWORD nPos ) const 
#endif
{
#ifdef _CONSOLE
	if ( ((m_nStatCount++ == 0 && !_isatty( _fileno( stdout ) )) || m_nStatCount % 1000 == 0) && _isatty(_fileno( stderr ) ) )
		_tcerr << _T("."); 
#else
	if ( m_nStatCount++ % 13 == 0 )
		m_pDialog->OSStatsSet( nPos );
#endif
}
#ifdef _CONSOLE
void CIOService::OSStatsSet( EPARSERSTATE /*eParserState*/ ) const 
#else
void CIOService::OSStatsSet( CIOService::EPARSERSTATE eParserState ) const 
#endif
{
#ifdef _CONSOLE
//	if ( ((m_nStatCount++ == 0 && !_isatty( _fileno( stdout ) )) || m_nStatCount % 1000 == 0) && _isatty( _fileno( stderr ) ) )
//		_tcerr << _T("."); 
#else
		m_pDialog->OSStatsSet( eParserState );
#endif
}

// -------------------------------------------------------------------------------------------------------------------------

/*[Post(MustCheck=Yes)]*/
bool CIOService::ReadBytes( /*[Pre(Access=SA_Write, WritableBytesParam="n")]*/ void* p, DWORD n, int hti, bool bCount )  
{ 
	if ( !ior.Read( p, n, bCount ) ) {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!") );
		if ( n > 0 && hti )
			DumpBytes( p, n, hti );
		return false;
	}
	return true;
}

/*[Post(MustCheck=Yes)]*/
bool CIOService::ReadMax( /*[Pre(Access=SA_Write, WritableBytesParam="nActual")]*/ void* p, DWORD nActual, DWORD nMax, int hti ) 
{ 
	DWORD n = std::min<DWORD>( nActual, nMax );
	if ( ReadBytes( p, n, hti ) ) { 
		if ( nActual > nMax )
			return ReadSeekFwdDump( nActual - nMax, hti );
		return true; 
	} 
	return false; 
}

bool CIOService::WriteSeekRead( QWORD nPos, DWORD nSize ) 
{
	BYTE *p = new BYTE[nSize];
	if ( !ior.SetPos( nPos ) )
		return false;
	if ( !ior.Read( p, nSize, false ) )
		return false;
	if ( !iow.Write( p, nSize, true ) )
		return false;
#ifndef IO_ASYNC_WRITE
	delete[] p;
#endif
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

bool CIOService::ReadSeekFwdDump( QWORD n, int hti ) // only forward
{	
#ifdef _CONSOLE
	QWORD nPos = GetReadPos();

	if ( nPos + n > GetReadFileSize() ) {
		n = GetReadFileSize() - GetReadPos();
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("File ends prematurely!") );
	}

	if ( !hti || Settings.nVerbosity == 0 ) {
		if ( !ReadSeek( (__int64)n ) ) { // now reposition and set counter
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!") );
			return false;
		}
		return true;
	}

	WORD nCount = (WORD)std::min<DWORD>( Settings.nDumpJunkCount, (n == 0 ? Settings.nDumpJunkCount : (DWORD)(n&0xFFFF)) );

	if ( Settings.nVerbosity > 3 ) {
		WORD o = static_cast<WORD>( std::min<QWORD>( GetReadTotalBytes(), 10 ) );
		ReadSeek( -o, false ); // Seek back
		nCount += o;
//		nCount += static_cast<WORD>( std::min<QWORD>( GetReadTotalBytes() - nPos, 10 ) ); // TODO weiss nicht, was das sollte?
	}

	nCount = (WORD)std::min<QWORD>( nCount, GetReadFileSize() - GetReadPos() );

	BYTE* p = new BYTE[nCount];
	if ( !ReadDumpBytes( p, nCount, hti, false ) )
		;
	delete[] p;

	ior.SetPos( nPos ); // return to old pos
	if ( !ReadSeek( (__int64)n ) ) { // now reposition and set counter
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!") );
		return false;
	}
	return true;
#else
	if ( hti == 0 || Settings.nVerbosity == 0 ) {
		if ( !ReadSeek( (__int64)n ) ) { // now reposition and set counter
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!") );
			return false;
		}
		return true;
	} else {
		WORD nCount = (WORD)std::min<DWORD>( Settings.nDumpJunkCount, (DWORD)n );
		BYTE* p = new BYTE[nCount];
		bool b = ReadDumpBytes( p, nCount, hti, true );
		delete[] p;

		if ( n > nCount )
			b &= ReadSeek( (__int64)n - nCount );
		return b;
	}
#endif
}
bool CIOService::ReadSeekFwdDumpChecksum(QWORD n, QWORD& nChecksum, int hti) // only forward
{
#ifdef _CONSOLE
	QWORD nPos = GetReadPos();

	if (nPos + n > GetReadFileSize()) {
		n = GetReadFileSize() - GetReadPos();
		OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("File ends prematurely!"));
	}

	WORD nCount = (WORD)std::min<QWORD>((n & 0xFFFF), GetReadFileSize() - GetReadPos());

	BYTE* p = new BYTE[nCount];
	if (Settings.nVerbosity > 3) {
		if (!ReadDumpBytes(p, nCount, hti, false))
			;
	}
	else
	{
		if (!ReadBytes(p, nCount, hti, false))
			;
	}
	for (int i = 0; i < nCount; i++)
		nChecksum += p[i];
	delete[] p;

	ior.SetPos(nPos); // return to old pos
	if (!ReadSeek((__int64)n)) { // now reposition and set counter
		OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!"));
		return false;
	}
	return true;
#else
	if (hti == 0 || Settings.nVerbosity == 0) {
		if (!ReadSeek((__int64)n)) { // now reposition and set counter
			OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!"));
			return false;
		}
		return true;
	}
	else {
		WORD nCount = (WORD)std::min<DWORD>(Settings.nDumpJunkCount, (DWORD)n);
		BYTE* p = new BYTE[nCount];
		bool b = ReadDumpBytes(p, nCount, hti, true);
		delete[] p;

		if (n > nCount)
			b &= ReadSeek((__int64)n - nCount);
		return b;
	}
#endif
}

bool CIOService::ReadDumpBytes( /*[Pre(ValidBytesParam="n")]*/ BYTE* p, DWORD n, int hti, bool bCount ) 
{
	if ( GetReadPos() + n > GetReadFileSize() ) {
		n = (DWORD)(GetReadFileSize() - GetReadPos());
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_FATAL, _T("File ends prematurely!") );
		if ( n == 0 )
			return false;
	}

	if ( !ReadBytes( p, n, hti, bCount ) )
		return false;

	return DumpBytes( p, n, hti );
}

bool CIOService::ReadDumpErrorContext( DWORD n, int hti )
{
	ASSERT( false );
	DWORD m = n / 2;
	QWORD pos = GetReadPos();
	if ( m > pos )
		m = (DWORD)pos;
	if ( pos + n > GetReadFileSize() )
		n = (DWORD)(GetReadFileSize() - pos) + m;
	hti = OSNodeCreate( hti, pos, _T("Error Context Dump"), _T(""), n );
	ReadSeek( -(int)m, false );
	BYTE* p = new BYTE[n];
	ReadBytes( p, n, hti, false );
	DumpBytes( p, m, hti );
	DumpBytes( p + m, n - m, hti );
	delete[] p;
	ReadSeek( -(int)(n - m), false );
	ASSERT( pos == GetReadPos() );
	ASSERT( pos == GetReadTotalBytes() );
	return true;
}

bool CIOService::DumpBytes( /*[Pre(ValidBytesParam="n")]*/ const void* p, DWORD n, int hti ) const 
{
_tstringstream s1;
_tstringstream s2;
int j = 0;

	for ( DWORD i = 0; i < n; ++i ) {
		BYTE c = reinterpret_cast<const BYTE*>( p )[i];
		if ( c && __iscsym( c ) )
			++j;
		s1 << (c && (__iscsym( c ) || ispunct( c )) ? (char)c : '.');
		s2 << std::hex << (int)c << _T(" ");
	}
//	s1 << std::ends;
//	s2 << std::ends;
	if ( j > 2 || n <= 4 )
		OSNodeCreateValue( hti, _T("\"") + s1.str() + _T("\"") );
	if ( Settings.nVerbosity > 3 || n <= 4 )
		OSNodeCreateValue( hti, _T("\"") + s2.str() + _T("\"") );
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

Settings::Settings()
{
	nVerbosity = 2;
	eForce = Settings::EFORCE::EM_FORCENONE;
	nFastmodeCount = 0;
	nTraceMoviEntries = 15;
	nTraceIdx1Entries = 15;
	nTraceIndxEntries = 15;
	bCheckIndex = true;
	nDumpJunkCount = 160 / 3; // private, 2 Lines
	fSetVideoFPS = 0.0;
	bFixAudio = false;
	bForceChunkSize = false;
	bIgnoreIndexOrder = false;
	bTight = true;
#ifndef _CONSOLE
	bHexHex = false;
	bHexEdit = true;
#endif
}

#ifndef _CONSOLE
/*static*/ _tstring Settings::sExtAVI = _T("*.avi;*.wav;*.divx");
/*static*/ _tstring Settings::sExtMPG = _T("*.mp1;*.mp2;*.mpg;*.ts;*.mpeg;*.tp;*.m1?;*.m2?;*.vob;*.dat;*.tp");
/*static*/ _tstring Settings::sExtMOV = _T("*.mov;*.qt;*.amr;*.mp4"); 
/*static*/ _tstring Settings::sExtRMF = _T("*.rm;*.ra");
/*static*/ _tstring Settings::sExtRIFF = _T("*.mid;*.midi;*.rmi;*.wav;*.avi;*.aif?;*.riff;*.rifx");
/*static*/ _tstring Settings::sExtASF = _T("*.wm?;*.asf;*.dvr-ms;*.wtv");
/*static*/ _tstring Settings::sExtMKV = _T("*.mkv;*.webm;vp?");
/*static*/ _tstring Settings::sExtOther = _T("*.part.met;*.part");
#endif
