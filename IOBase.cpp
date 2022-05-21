#include "IOBase.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
bool io::Seek( __int64 n, bool bCount ) 
{ 
	if ( win::SetFilePointerEx( m_fh, *((win::LARGE_INTEGER*)&n), nullptr, FILE_CURRENT ) != 0 )	{
		if ( bCount )
			m_nTotalBytes += n;
		ASSERT( m_nTotalBytes < 20i64 * 1024 * 1024 * 1024 ); 
		return true;
	}
	return Error( false );
}
QWORD io::GetPos() const 
{ 
	win::LARGE_INTEGER li;
	li.QuadPart = 0;
	if ( win::SetFilePointerEx( m_fh, li, &li, FILE_CURRENT ) != 0 )
		return (QWORD)li.QuadPart;
	Error( false );
	return 0;
}
bool    io::SetPos( QWORD n ) { return Error( win::SetFilePointerEx( m_fh, *((win::LARGE_INTEGER*)&n), nullptr, FILE_BEGIN ) != 0 ); }
bool    io::SetPosStart() { win::LARGE_INTEGER n; n.QuadPart = 0; return Error( win::SetFilePointerEx( m_fh, n, nullptr, FILE_BEGIN ) != INVALID_SET_FILE_POINTER ); }
bool    io::SetPosEnd() { win::LARGE_INTEGER n; n.QuadPart = (__int64)m_nTotalBytes; return Error( win::SetFilePointerEx( m_fh, n, nullptr, FILE_BEGIN ) != INVALID_SET_FILE_POINTER ); }
// bool    io::SetPosEnd() { win::LARGE_INTEGER n; n.QuadPart = 0; return Error( win::SetFilePointerEx( m_fh, n, nullptr, FILE_END ) != INVALID_SET_FILE_POINTER ); }
 QWORD io::GetFileSize() const { win::LARGE_INTEGER l; if ( win::GetFileSizeEx( m_fh, &l ) != 0 ) return (QWORD)l.QuadPart; Error( false ); return 0; }; 
// TODO würde sich das cachen der FileSize für ior lohnen?
// win::SYSTEMTIME io::GetFileDate() const { 
//	win::FILETIME ftWrite; 
//	win::SYSTEMTIME stUTC, stLocal; 
//	if (!win::GetFileTime(m_fh, nullptr, nullptr, &ftWrite)) 
//		return stLocal; // TODO Fehlerbehandlung
//	win::FileTimeToSystemTime(&ftWrite, &stUTC); 
//	win::SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal); 
//	return stLocal; 
//}

bool io::Close() 
{ 	
	ASSERT( m_bOpen ); 
//	ASSERT( GetFileSize() == GetTotalBytes() );
	m_bOpen = false; 
	return Error( win::CloseHandle( m_fh ) != 0 ); 
}


bool io::Error( bool b ) const 
{ 
	if ( b ) return true; 
	_TCHAR* lpMsgBuf;
	if (!win::FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, win::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(_TCHAR*) &lpMsgBuf, 0, nullptr ))
	{
	// Handle the error.
	return false;
	}
#ifdef _CONSOLE
	_tcout << lpMsgBuf;
	ASSERT( _tcout.good() );
	if ( !_isatty( _fileno( stderr ) ) ) _tcerr << std::endl << lpMsgBuf; 
#else
	AfxMessageBox( lpMsgBuf, MB_ICONEXCLAMATION | MB_OK );
#endif
	win::LocalFree( lpMsgBuf );
	return false; 
}


bool ior::Read( /*[Pre(Access=Write, WritableBytesParam="n")]*/ void *p, DWORD& n, bool bCount ) 
{
DWORD r;
	if ( win::ReadFile( m_fh, p, n, reinterpret_cast<win::LPDWORD>( &r ), nullptr ) != 0 && n == r ) {
		if ( bCount ) 
			m_nTotalBytes += n;
		ASSERT( m_nTotalBytes < 20i64 * 1024 * 1024 * 1024 ); 
		return true;
	}
	n = r;
	if ( n == 0 ) return false; // Suppress error for EOF
	return Error( false ); 
}

bool iow::Write( /*[Pre(Access=Read, ValidBytesParam="n")]*/ const void *p, DWORD n, bool bAsync, bool bCount ) 
{ 
DWORD w;
#ifdef IO_ASYNC_WRITE
	if ( last_buffer ) {
#if 0 // def _DEBUG
		if ( !/*win::*/HasOverlappedIoCompleted( Overlapped ) )
			_tcerr << _T("P2\n");
		else
			_tcerr << _T("F2\n");
#endif
		WaitForCompletion();
	}
//	ASSERT( m_nTotalBytes == GetPos() ); stimmt nicht
	ASSERT( !m_bCheckSize || GetFileSize() == GetTotalBytes() );
	ASSERT( pos <= GetTotalBytes() );
	memset( Overlapped, '\0', sizeof( win::OVERLAPPED ) );
 	Overlapped->Offset = (DWORD)(pos & 0x00000000ffffffffi64);
	Overlapped->OffsetHigh = (DWORD)(pos >> 32);
	if ( bAsync ) 
		last_buffer = (BYTE*)p;
	else {
		last_buffer = new BYTE[n];
		memcpy( last_buffer, p, n );
		bAsync = true;
	}
	last_len = n;

	if ( win::WriteFile( m_fh, last_buffer, n, reinterpret_cast<win::LPDWORD>( &w ), Overlapped ) == 0 ) {
		if ( win::GetLastError() != ERROR_IO_PENDING )
			return Error( false );
//		_tcerr << _T("P1\n");
	} else {
//		_tcerr << _T("F1\n");
	}
	if ( !bAsync ) {
//		_tcerr << _T("W1\n");
		WaitForCompletion();
	}

	pos += n;
	if ( bCount ) 
		m_nTotalBytes += n;
	return true;
#else
	if ( win::WriteFile( m_fh, p, n, reinterpret_cast<win::LPDWORD>( &w ), nullptr ) != 0 ) {
		if ( bCount ) {
			m_nTotalBytes += w;
//			ASSERT( GetFileSize() == GetTotalBytes() );
		}
		return (n == w);
	}
	return Error( false ); 
#endif
}

bool iow::Read( /*[Pre(Access=Write, WritableBytesParam="n")]*/ void *p, DWORD& n ) 
{
DWORD r;
	if ( win::ReadFile( m_fh, p, n, reinterpret_cast<win::LPDWORD>( &r ), nullptr ) != 0 && n == r ) {
		return true;
	}
	n = r;
	return Error( false ); 
}

bool ior::Open( const _tstring& sFilename ) 
{ 
	ASSERT( !m_bOpen );	
	nFileSize = 0;
	m_nTotalBytes = 0;

#ifdef _DEBUG
//	win::HANDLE	fh = win::CreateFile( sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
//	win::LockFile( fh, 0, 0, -1, -1 );
//	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS, 0 );
#endif

#if 0
// Enable the backup privilege 
	win::HANDLE htok = 0; 
	if ( win::OpenProcessToken( win::GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &htok ) ) {
		win::TOKEN_PRIVILEGES tpOld; 
		VERIFY( EnablePrivilege( htok, SE_BACKUP_NAME, &tpOld ) ); 
		VERIFY( EnablePrivilege( htok, SE_RESTORE_NAME, &tpOld ) ); 
		VERIFY( EnablePrivilege( htok, SE_TCB_NAME, &tpOld ) ); 
		Error( false );
	} else ASSERT( false );

#if 0 // schliessen nicht vergessen

	RestorePrivilege( htok, tpOld ); 
	CloseHandle( htok ); 

#endif....
#endif

	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
//	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS, 0 );
//	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
	if ( m_fh != INVALID_HANDLE_VALUE ) {
		m_nTotalBytes = 0; 
		m_sFileName = sFilename;
		m_bOpen = true;
		return true; 
	}
	return Error( false ); 
}

bool iow::OpenExisting( const _tstring& sFilename ) 
{ 
	ASSERT( !m_bOpen );
	m_bCheckSize = false;
#ifdef IO_ASYNC_WRITE
	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
	win::ULONG_PTR k = 0x1234;
	m_io = win::CreateIoCompletionPort( m_fh, nullptr, k, 1 );
#else
	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
#endif
	if ( m_fh != INVALID_HANDLE_VALUE ) {
		m_nTotalBytes = 0; 
		m_sFileName = sFilename;
		m_bOpen = true;
		return true; 
	}
	return Error( false ); 
}
bool iow::OpenCreate( const _tstring& sFilename ) 
{ 
	ASSERT( !m_bOpen );
	m_bCheckSize = true;
#ifdef IO_ASYNC_WRITE
	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
	win::ULONG_PTR k = 0x1234;
	m_io = win::CreateIoCompletionPort( m_fh, nullptr, k, 1 );
#else
	m_fh = win::CreateFile( sFilename.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0 );
#endif
	if ( m_fh != INVALID_HANDLE_VALUE ) {
		m_nTotalBytes = 0; 
		nPreAllocateSize = 0;
		m_sFileName = sFilename;
		m_bOpen = true;
		return true; 
	}
	return Error( false ); 
}

bool iow::ExtendNull( QWORD nSize ) {
	if ( !Seek( (__int64)nSize ) )
		return false;
	if ( GetPos() > nPreAllocateSize )
		return Error( win::SetEndOfFile( m_fh ) != 0 );
	else
		return true;
}
bool iow::Preallocate( QWORD nSize) {
	if ( !Seek( (__int64)nSize, false ) )
		return false;
	nPreAllocateSize = nSize;
	if ( !Error( win::SetEndOfFile( m_fh ) != 0 ) )
		return false;
	return SeekBegin();
}
#else // Linux
bool io::Seek( __int64 n, bool bCount ) 
{ 
	if ( fseeko64( m_fh, n, SEEK_CUR ) >= 0 ) {
		if ( bCount )
			m_nTotalBytes += n;
		return true;
	}
	return Error( false ); 
}
QWORD io::GetPos() const { return ftello64( m_fh ); }
bool    io::SetPos( QWORD n ) { return Error( fseeko64( m_fh, n, SEEK_SET ) == 0 ); }
bool    io::SetPosStart() { return Error( fseeko64( m_fh, 0, SEEK_SET ) == 0 ); }
bool    io::SetPosEnd() { return Error( fseeko64( m_fh, 0, SEEK_END ) == 0 ); }
bool    io::Close() { m_bOpen = false; return Error( fclose( m_fh ) == 0 ); }
QWORD io::GetFileSize() const { fpos64_t p; fgetpos64( m_fh, &p ); fseeko64( m_fh, 0, SEEK_END ); __int64 s = ftello64( m_fh ); fsetpos64( m_fh, &p ); return s; } // TODO error handling
bool    ior::Read( void *p, DWORD& n, bool bCount ) 
{ 
	DWORD r;
	if ( (r = (DWORD)fread( p, 1, n, m_fh )) == n ) {
		if ( bCount )
			m_nTotalBytes += n;
		return true;
	}
	n = r;
	return Error( false ); 
}
bool iow::Write( const void *p, DWORD n, bool bAsync, bool bCount )
{ 
	if ( (DWORD)fwrite( p, 1, n, m_fh ) == n ) {
		if ( bCount )
			m_nTotalBytes += n;
		return true;
	}
	return Error( false ); 
}
bool ior::Open( const _tstring& sFilename ) 
{ 
	m_fh = fopen64( sFilename.c_str(), _T("rb") ); 
	if ( m_fh ) {
		m_nTotalBytes = 0; 
		m_sFileName = sFilename;
		m_bOpen = true;
		return true; 
	}
	return Error( false ); 
}
bool iow::OpenCreate( const _tstring& sFilename ) 
{ 
	m_fh = fopen64( sFilename.c_str(), _T("wb") ); 
	if ( m_fh ) {
		m_nTotalBytes = 0; 
		m_sFileName = sFilename;
		m_bOpen = true;
		return true; 
	}
	return Error( false ); 
}
bool iow::ExtendNull( QWORD nSize ) {
	BYTE *p = new BYTE[nSize];
	memset( p, 0, nSize );
	if ( !Write( p, nSize, false ) ) {
		delete[] p;
		return false;
	}
	delete[] p;
	return true;
}
bool iow::Preallocate(QWORD nSize) {
/*	if (!Seek((__int64)nSize, false))
		return false;
	nPreAllocateSize = nSize;
	if (!Error(win::SetEndOfFile(m_fh) != 0))
		return false;
	return SeekBegin();*/
	return true;
}
#endif


// Useful helper function for enabling a single privilege 
#if 0 // defined(_WIN32) || defined(_WIN64)
bool EnablePrivilege( win::HANDLE htok, const _TCHAR* pszPriv, win::TOKEN_PRIVILEGES* tpOld ) 
{ 
	win::TOKEN_PRIVILEGES tp; 
	tp.PrivilegeCount = 1; 
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	if ( !win::LookupPrivilegeValue( 0, pszPriv, &tp.Privileges[0].Luid ) ) 
		return false;

	// htok must have been opened with the following permissions: 
	// TOKEN_QUERY (to get the old priv setting) 
	// TOKEN_ADJUST_PRIVILEGES (to adjust the priv) 
	win::DWORD cbOld = (tpOld ? sizeof( win::TOKEN_PRIVILEGES ) : 0); 
	if ( !win::AdjustTokenPrivileges( htok, FALSE, &tp, cbOld, tpOld, &cbOld ) ) 
		return false;

	// Note that AdjustTokenPrivileges may succeed, and yet 
	// some privileges weren't actually adjusted. 
	// You've got to check GetLastError() to be sure! 
	return ( ERROR_NOT_ALL_ASSIGNED != win::GetLastError() ); 
} 

// Corresponding restoration helper function 
bool RestorePrivilege( win::HANDLE htok, const win::TOKEN_PRIVILEGES& tpOld ) 
{ 
	return (win::AdjustTokenPrivileges( htok, FALSE, const_cast<win::TOKEN_PRIVILEGES*>( &tpOld ), 0, 0, 0 ) != 0);
}
#endif
