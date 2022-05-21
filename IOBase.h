#pragma once

#include "Helper.h"

#pragma warning(push, 3)
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#ifdef _CONSOLE
namespace win {
#include <windows.h>
}
#else
#include "stdafx.h"
#define win
#endif
#include <io.h>
#define IO_ASYNC_WRITE
#else // Linux
#include <stdio.h>
#endif
#pragma warning(pop)

bool MakeSplitFilename( const _tstring& sFilename, WORD nPart, _tstring& sSplitFilename );
#if defined(_WIN32) || defined(_WIN64)
__declspec(noreturn) 
#endif
void Usage( _tostream& o, bool bAll );
#if 0 // defined(_WIN32) || defined(_WIN64)
bool EnablePrivilege( win::HANDLE htok, const _TCHAR* pszPriv, win::TOKEN_PRIVILEGES* tpOld );
#endif

class io {
protected:
#if defined(_WIN32) || defined(_WIN64)
	typedef win::HANDLE HANDLE;
	typedef win::HANDLE FHANDLE;
#else // Linux 
	typedef FILE* FHANDLE;
#endif

	FHANDLE m_fh;
	QWORD m_nTotalBytes;
	_tstring  m_sFileName;
	bool    m_bOpen;
public:
	io() { m_nTotalBytes = 0; m_fh = 0; m_bOpen = false; };
	~io() { if ( m_bOpen ) Close(); };
	bool  Close();
	bool  Seek( __int64 nOff, bool bCount = true );
	bool  SetPos( QWORD nPos );
	bool  SetPosStart();
	bool  SetPosEnd();
	QWORD GetPos() const;
	QWORD GetTotalBytes() const { return m_nTotalBytes; };
	QWORD GetFileSize() const;
//	win::SYSTEMTIME GetFileDate() const; // Beim findfirst wird eine FileTime zurückgeliefert...
	//time_t GetFileDate() const;
	const _tstring& GetFileName() const { return m_sFileName; };
	void  AddTotalBytes( QWORD nOffset ) { m_nTotalBytes += nOffset; };
	bool IsOpen() const { return m_bOpen; }
protected:
#if defined(_WIN32) || defined(_WIN64)
	bool Error( bool b ) const;
#else // Linux 
	bool Error( bool b ) const { if ( b ) return true; perror( "AVIMaster" ); return false; };
#endif
};

class ior : public io {
protected:
	mutable QWORD nFileSize;

public:
	ior() : io() { nFileSize = 0; }
	bool Open( const _tstring& sFilename );
	bool Read( void *p, DWORD& n, bool bCount = true );
	bool ReadN( void *p, DWORD n, bool bCount = true ) { DWORD m = n; return Read( p, m, bCount ) && m == n; }
	QWORD GetFileSize() const { if ( nFileSize == 0 ) nFileSize = io::GetFileSize(); return nFileSize; }
};

class iow : public io {
#ifdef IO_ASYNC_WRITE
protected:
	win::HANDLE m_io;
	win::OVERLAPPED* Overlapped;
	BYTE* last_buffer;
	DWORD last_len;
	QWORD pos;
	QWORD nPreAllocateSize;
	bool m_bCheckSize;
#endif
public:
#ifdef IO_ASYNC_WRITE
	iow() : io() { pos = 0; last_buffer = nullptr; Overlapped = new win::OVERLAPPED; m_bCheckSize = true; }
	~iow() { delete Overlapped; }
	bool  SetPos( QWORD nPos ) { pos = nPos; return true; }
	bool  SetPosStart() { pos = 0; return true; }
	bool  SetPosEnd() { pos = m_nTotalBytes; return true; }
	bool  Seek( __int64 nOff, bool bCount = true ) { pos += nOff; if ( win::SetFilePointerEx( m_fh, *((win::LARGE_INTEGER*)&pos), nullptr, FILE_BEGIN ) != 0 ) { if ( bCount ) m_nTotalBytes += nOff; return true; } return Error( false ); }
	QWORD GetPos() const { return pos; }
	bool  Close() { if ( last_buffer ) WaitForCompletion(); return io::Close(); } // MUST be called to eat the completion message
	bool WaitForCompletion() { win::DWORD t; win::ULONG_PTR k; VERIFY( win::GetQueuedCompletionStatus( m_io, &t, &k, &Overlapped, INFINITE ) ); ASSERT( t == last_len ); delete last_buffer; last_buffer = nullptr; return true; }
	QWORD GetFileSize() const { ASSERT( nPreAllocateSize > 0 || io::GetFileSize() == m_nTotalBytes ); return m_nTotalBytes; }
#else
	iow() : io() { };
	QWORD GetFileSize() const { ASSERT(io::GetFileSize() == m_nTotalBytes); return m_nTotalBytes; }
#endif
	bool OpenExisting( const _tstring& sFilename );
	bool OpenCreate( const _tstring& sFilename );
	bool Write(const void *p, DWORD n, bool bAsync, bool bCount = true);
	bool Read( void *p, DWORD& n );
	bool ExtendNull( QWORD nSize );
	bool Preallocate( QWORD nSize );
#if defined(_WIN32) || defined(_WIN64)
	bool SeekBegin() { pos = 0; if ( win::SetFilePointerEx( m_fh, *((win::LARGE_INTEGER*)&pos), nullptr, FILE_BEGIN ) != 0 ) return true; return Error( false ); }
#else
	bool SeekBegin() { return Seek( 0, false ); }
#endif
};

