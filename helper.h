#pragma once

#pragma warning(disable:4061 4062 4200 4201 4355 4365 4512 4514 4996 4626 4571 4625 4710 4711 4820 5026 5027 5045)
//#pragma warning(4:4191 4242 4254 4263 4265 4266 4287 4302 4365 4555 4640 4905 4906 4946)
//#include <CodeAnalysis/SourceAnnotations.h>

#define _ALLOW_RTCc_IN_STL

#ifdef __clang__
#pragma clang diagnostic ignored "-Wc++98-compat"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wfour-char-constants"
#endif

#pragma warning(push, 3)

#include <string>
#include <sstream> // stringstream
#include <iostream> // cout, cerr
#include <iomanip> // setw
#include <algorithm> // min, max
#include <vector>
#include <map>
#include <math.h>
#include <cstring>
#include <limits.h>
#include <typeinfo>
#include <deque>
#include <stdlib.h>
#include <set>
#include <functional>


#if defined(LINUX32) || defined(LINUX64)
#ifndef _CONSOLE
#define _CONSOLE
#endif
#define POSIX_SOURCE
//#define _UNICODE
#include <uchar.h>
#endif

#ifdef _CONSOLE
#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX 
#include <crtdbg.h>
namespace win {
#define WINVER _WIN32_WINNT_WIN7 
#include <windows.h>
}
#endif // WIN
#ifdef _DEBUG   
#ifndef DBG_NEW      
#define DBG_NEW new( _NORMAL_BLOCK , __FILE__ , __LINE__ )  
#define new DBG_NEW 
#endif
#endif  // _DEBUG

#else // !_CONSOLE
#include "stdafx.h"
#define win
#endif // _CONSOLE
#include <assert.h>
#if defined(_WIN32) || defined(_WIN64)
#include <tchar.h>
#else // !WIN
#include <dirent.h>
#endif // WIN

#pragma warning(pop)


#ifdef _UNICODE
typedef std::wstring _tstring;
typedef std::wstringstream _tstringstream;
typedef std::wostream _tostream;
#define _tcout std::wcout
#define _tcerr std::wcerr
#define _tcin std::wcin
//#define _T(a) L##a
#else
typedef std::string _tstring;
typedef std::stringstream _tstringstream;
typedef std::ostream _tostream;
#define _tcout std::cout
#define _tcerr std::cerr
#define _tcin std::cin
//#define _T(a) a
#endif  // _UNICODE

#ifdef _DEBUG
#define VERIFY(a) ASSERT(a)
#ifdef _CONSOLE
#define ASSERT(a) _ASSERTE(a)

#ifdef UNICODE
#define TRACE2(t,a,b) { wchar_t _s[1024]; _stprintf( _s, t, a, b ); _tcerr << _s; }
#define TRACE3(t,a,b,c) { wchar_t _s[1024]; _stprintf( _s, t, a, b, c ); _tcerr << _s; }
#else
#define TRACE2(t,a,b) { char _s[1024]; _stprintf( _s, t, a, b ); _tcerr << _s; }
#define TRACE3(t,a,b,c) { char _s[1024]; _stprintf( _s, t, a, b, c ); _tcerr << _s; }
#endif
#endif
#else
#define VERIFY(a) ((void)(a))
#ifdef _CONSOLE
#define ASSERT(a) _ASSERTE(a)
#define TRACE2(t,a,b)
#define TRACE3(t,a,b,c)
#endif
#endif

typedef const char char08; // UTF-8, MBCS Windows
typedef const char16_t char16; // UTF-16, Windows
//typedef const char32_t char32;

#if defined(_WIN32) || defined(_WIN64)
typedef unsigned __int64 QWORD;
#ifdef _CONSOLE
typedef unsigned __int32 DWORD;
#endif
typedef unsigned __int16 WORD;
typedef unsigned __int8  BYTE;
#else // Linux
typedef unsigned long long QWORD;
typedef unsigned int   DWORD;
typedef unsigned short WORD;
typedef unsigned char  BYTE;
typedef char _TCHAR;
typedef long long __int64;
typedef unsigned int UINT;
#define MAXDWORD 0xffffffff
#define MAXWORD 0xffff
#define _T(a) a
#define _W64
#define _ASSERTE(a) assert(a)
#define __cdecl
#define _isatty isatty
#define __iscsym(a) (isalnum(a) || a == '_') 
#define _file _fileno // cout->_fileno
#define _tcslen strlen
#define _tstoi atoi
#define _tstof atof
#define _tcsncmp strncmp
#define _tcscmp strcmp
#define _tcsncpy strncpy
#define _tgetcwd getcwd
#define _tcscat strcat
#define _tcsstr strstr
#define _tcstol strtol
#define _taccess access
#define _tmain main
#define gmtime_s(a,b) gmtime_r(b,a)
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL
inline time_t WindowsTickToUnixSeconds(long long windowsTicks)
{
	return (time_t)(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}
#endif
typedef BYTE TRIPLE[3];
typedef QWORD DWORDLONG;
typedef wchar_t WCHAR;
#ifdef _CONSOLE
typedef QWORD LONGLONG;
typedef DWORD BOOL;
#endif
typedef long LONG;
typedef short SHORT;
typedef unsigned short USHORT;
typedef DWORD FOURCC;
#if defined(_WIN64) || defined(LINUX64) 
typedef __int64 INT_PTR;
typedef QWORD UINT_PTR;
typedef QWORD DWORD_PTR;
typedef __int64 LONG_PTR;
typedef QWORD ULONG_PTR;
#else
typedef int INT_PTR;
typedef unsigned int UINT_PTR;
typedef DWORD DWORD_PTR;
typedef LONG LONG_PTR;
typedef DWORD ULONG_PTR;
#endif

#if !defined(_WIN32) && !defined(_WIN64) // LINUX
#define _MAX_PATH PATH_MAX
#define _MAX_DRIVE  3   /* max. length of drive component */
#define _MAX_DIR    _MAX_PATH /* max. length of path component */
#define _MAX_FNAME  _MAX_PATH /* max. length of file name component */
#define _MAX_EXT    _MAX_PATH /* max. length of extension component */
#endif

#ifdef _CONSOLE
struct GUID {
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[8];
};
#endif

#undef LOWORD
#undef HIWORD
	inline WORD LOWORD( DWORD n ) { return static_cast<WORD>( n & 0x0000ffff ); }
	inline WORD HIWORD( DWORD n ) { return static_cast<WORD>( n >> 16 ); }

	inline std::wstring ToWideString( const char* pStr, size_t len = -1 ); 
	inline std::string ToNarrowString( const wchar_t* pws, size_t len );
	inline std::wstring ToWideString( const std::string& str ) { return ToWideString( str.c_str(), str.length() ); }
	inline std::string ToNarrowString( const std::wstring& str ) { return ToNarrowString( str.c_str(), str.length() ); }

	// ToString( t, iomanip& m );
	template<typename T> inline _tstring ToString( T t ) { _tstringstream s; s << t; return s.str(); };
//	template<> inline _tstring ToString<BYTE>( BYTE t ) { _tstringstream s; if ( isprint( t ) ) s << '\'' << (char)t << '\'' << " (" << t << ')'; else s << t; return s.str(); };
#ifdef _UNICODE
	template<> inline _tstring ToString<BYTE>( BYTE t ) { _tstringstream s; s << t; return s.str(); };
#else
	template<> inline _tstring ToString<BYTE>( BYTE t ) { _tstringstream s; s << (WORD)t; return s.str(); };
#endif
	template<> inline _tstring ToString<_TCHAR>( _TCHAR t ) { _tstringstream s; s << _T('\'') << (_TCHAR)t << _T('\'') << _T(" (") << (WORD)t << _T(')'); return s.str(); };
#if !defined(LINUX64) // ???
	template<> inline _tstring ToString<size_t>( size_t t ) { _tstringstream s; s << (unsigned __int64)t; return s.str(); };
#endif
	template<> inline _tstring ToString<GUID>( GUID t ) { _tstringstream s; s.fill( _T('0') ); s.flags( std::ios::right ); s << _T("{") << std::hex << std::setw(8) << t.Data1 << _T("-") << std::setw(4) << (DWORD)t.Data2 << _T("-") << std::setw(4) << (DWORD)t.Data3 << _T("-") << std::setw(2) << (DWORD)t.Data4[0] << std::setw(2) << (DWORD)t.Data4[1] << _T("-") << std::setw(2) << (DWORD)t.Data4[2] << std::setw(2) << (DWORD)t.Data4[3] << std::setw(2) << (DWORD)t.Data4[4] << std::setw(2) << (DWORD)t.Data4[5] << std::setw(2) << (DWORD)t.Data4[6] << std::setw(2) << (DWORD)t.Data4[7] << _T("}"); return s.str(); }; // cast to DWORD as WORD is wchar_t
	template<> inline _tstring ToString<const _tstring&>( const _tstring& t ) { return t; }
	template<> inline _tstring ToString<const _TCHAR*>( const _TCHAR* t ) { return _tstring( t ); }
	inline _tstring ToStringFromUTF8(const char08* t, size_t len);
	inline _tstring ToStringFromUTF16(const char16* t, size_t len);
	//inline _tstring ToStringFromUTF32(const char32* t, size_t len);
	inline _tstring ToStringFromMBCS(const char08* t, size_t len);
//	inline _tstring ToString( const _TCHAR* t, size_t len ) { return _tstring( t, len ); }
#ifdef _UNICODE
	inline _tstring ToString( const std::string& t ) { return ToWideString( t ); } // wird als Template nicht gegriffen
//	template<> inline _tstring ToString<const std::string&>( const std::string& t ) { return ToWideString( t ); }
	template<> inline std::wstring ToString<const char*>( const char* t ) { return ToWideString( t ); }
	inline std::wstring ToString( const char* t, size_t len ) { return ToWideString( t, len ); }
#else // MBCS
	//inline std::string ToString( const std::wstring& t ) { return ToNarrowString( t ); } // wird als Template nicht gegriffen
//	template<> inline std::string ToString<const std::wstring&>( const std::wstring& t ) { return ToNarrowString( t ); }
	template<> inline std::string ToString<const wchar_t*>( const wchar_t* t ) { return ToNarrowString( t ); }
	inline std::string ToString( const wchar_t* t, size_t len ) { return ToNarrowString( t, len ); }
#endif // _UNICODE

	template<typename T> static inline T FlipBytes( const T* t );
	template<> inline BYTE FlipBytes<BYTE>( const BYTE* t ) { return *t; }
	template<> inline WORD FlipBytes<WORD>( const WORD* t ) { const BYTE* p = reinterpret_cast<const BYTE*>( t ); return (WORD)((WORD)p[1] | (WORD)(p[0] << 8)); }
	template<> inline SHORT FlipBytes<SHORT>( const SHORT* t ) { const BYTE* p = reinterpret_cast<const BYTE*>( t ); return (SHORT)((WORD)p[1] | (WORD)(p[0] << 8)); }
	template<> inline DWORD FlipBytes<DWORD>( const DWORD* t ) { 
		const BYTE* p = reinterpret_cast<const BYTE*>( t ); return (DWORD)p[3] | (DWORD)p[2] << 8 | (DWORD)p[1] << 16 | (DWORD)p[0] << 24; }
	template<> inline LONG FlipBytes<LONG>( /*[Pre(ValidElements=1)]*/const LONG* t ) { 
		const BYTE* p = reinterpret_cast<const BYTE*>( t ); return (LONG)((DWORD)p[3] | (DWORD)p[2] << 8 | (DWORD)p[1] << 16 | (DWORD)p[0] << 24); }
	template<> inline QWORD FlipBytes<QWORD>( /*[Pre(ValidElements=1)]*/const QWORD* t ) { 
		const BYTE* p = reinterpret_cast<const BYTE*>( t ); return (QWORD)p[7] | (QWORD)p[6] << 8 | (QWORD)p[5] << 16 | (QWORD)p[4] << 24 | (QWORD)p[3] << 32 | (QWORD)p[2] << 40 | (QWORD)p[1] << 48 | (QWORD)p[0] << 56; }

#if !defined(_WIN32) && !defined(_WIN64)
inline void _tsplitpath( const _TCHAR* path, _TCHAR* drive, _TCHAR* dir, _TCHAR* fname, _TCHAR* ext );
#endif
	bool PartMetInfo( const _tstring& sMetFile, _tstring& sFilename ); // TODO in IOService oder IOBase verlagern???
	inline _tstring FilenameAppend( const _tstring& sFilepath, const _tstring sAppend ) // Appends to name (leaving extension at end)
	{
		_TCHAR sDrive[_MAX_DRIVE], sPath[_MAX_PATH], sFName[_MAX_FNAME], sExt[_MAX_EXT];
		_tsplitpath( sFilepath.c_str(), sDrive, sPath, sFName, sExt );
		return (_tstring)sDrive + (_tstring)sPath + (_tstring)sFName + sAppend + (_tstring)sExt;
	}
	inline _tstring FilenameExtensionReplace( const _tstring& sFilepath, const _tstring sNewExtension )
	{
		_TCHAR sDrive[_MAX_DRIVE], sPath[_MAX_PATH], sFName[_MAX_FNAME];
		_tsplitpath( sFilepath.c_str(), sDrive, sPath, sFName, nullptr );
		return (_tstring)sDrive + (_tstring)sPath + (_tstring)sFName + sNewExtension;
	}
	inline _tstring FilenameGetExtension( const _tstring& sFilepath ) 
	{
		_TCHAR sExt[_MAX_FNAME];
		_tsplitpath( sFilepath.c_str(), nullptr, nullptr, nullptr, sExt );
		return (_tstring)sExt;
	}
	inline _tstring FilenameGetNameExt( const _tstring& sFilepath ) 
	{
		_TCHAR sFName[_MAX_FNAME], sExt[_MAX_FNAME];
		_tsplitpath( sFilepath.c_str(), nullptr, nullptr, sFName, sExt );
		return (_tstring)sFName + (_tstring)sExt;
	}

	template<typename T> static inline _tstring ToHexString( T t ) 
	{ 
		_tstringstream s; s.fill( _T('0') ); 
		s.flags( std::ios::right ); 
		s << std::hex << std::setw( sizeof( t ) * 2 ) << t; 
		return s.str(); };
	template<> inline _tstring ToHexString( BYTE t ) 
	{ 
		_tstringstream s; s.fill( _T('0') ); 
		s.flags( std::ios::right ); 
		s << std::hex << std::setw( 2 ) << (WORD)t; 
		return s.str(); };
	static inline _tstring FCCToString( FOURCC fcc ) { const BYTE* p = reinterpret_cast<const BYTE*>( &fcc ); _tstring s = _T("'"); s += !isprint(p[0]) ? _T(' ') : (_TCHAR)p[0]; s += !isprint(p[1]) ? _T(' ') : (_TCHAR)p[1]; s += !isprint(p[2]) ? _T(' ') : (_TCHAR)p[2]; s += !isprint(p[3]) ? _T(' ') : (_TCHAR)p[3]; s+= _T("'"); return s; };
	static inline _tstring DurationToLengthString( DWORD lengthms, double fps ) { if ( fps < 0.01 ) return _T("\?\?\? fps < 0.01"); _tstringstream s; s.fill( _T('0') ); s.flags( std::ios::right ); s << (int)((double)lengthms / 1000 / 60 / 60) << _T(':') << std::setw(2) << (int)((double)lengthms / 1000 / 60)%60 << _T(':') << std::setw(2) << (int)((double)lengthms / 1000)%60 << _T(':') << std::setw(2) << (int)(fmod( (double)lengthms / 1000, fps ) + .5); s << std::setiosflags(std::ios::fixed) << std::setw(0) << std::setprecision(2) << _T(" (") << fps << _T(" fps") << (fps == 25.0 ? _T(", PAL)") : ((int)(fps * 100) == 2997 || fps == 30.0 ? _T(", NTSC)") : _T(")"))); return s.str(); }
	static inline _tstring FramesToLengthString( DWORD frames, double fps ) { if ( fps < 0.01 ) return _T("\?\?\? fps < 0.01"); _tstringstream s; s.fill( _T('0') ); s.flags( std::ios::right ); s << (int)(frames / fps / 60 / 60) << _T(':') << std::setw(2) << (int)(frames / fps / 60)%60 << _T(':') << std::setw(2) << (int)(frames / fps)%60 << _T(':') << std::setw(2) << (int)(fmod( frames, fps ) + .5); s << std::setiosflags(std::ios::fixed) << std::setw(0) << std::setprecision(2) << _T(" (") << fps << _T(" fps") << (fps == 25.0 ? _T(", PAL)") : ((int)(fps * 100) == 2997 || fps == 30.0 ? _T(", NTSC)") : _T(")"))); return s.str(); }

	static inline DWORD GCD( DWORD a, DWORD b );
	static inline int BCD2Int( BYTE bcd ) { return 10 * ((bcd & 0xf0) >> 4) + (bcd & 0x0f); }
	static inline const void* AddBytes( const void* t, DWORD n ) { return static_cast<const BYTE*>( t ) + n; }

	// Count set 1 bits
	static inline int BitsUsed( DWORD x ) { int numBits = 0; while (x != 0) { x >>= 1; ++numBits; } return numBits; }

inline std::wstring ToWideString( const char* pns, size_t len ) 
{
	size_t l = (len != (size_t)-1 ? len : strlen( pns ));
	ASSERT( l < UINT_MAX / sizeof( wchar_t ) );
//	if ( l >= UINT_MAX / sizeof( wchar_t ) )
//		l = (UINT_MAX / sizeof( wchar_t )) - sizeof( wchar_t );
	wchar_t* pws = new wchar_t[l + 1];
	l = mbstowcs( pws, pns, l );
	ASSERT( l != (size_t)-1 );
	if ( l == (size_t)-1 ) {
		delete[] pws;
		return  L"";
	}
	pws[l] = L'\0';
	std::wstring ws( pws );
	delete[] pws;
	return ws;
}

inline _tstring ToStringFromMBCS(const char08* ps, size_t len)
{
	ASSERT(len != (size_t)-1);
#if defined(_WIN32) || defined(_WIN64)
#if defined( _UNICODE )
	wchar_t* buff1 = new wchar_t[len + 1];
	if (win::MultiByteToWideChar(CP_UTF8, 0, ps, (int)len, buff1, (int)len) == 0)
		exit(1);
	buff1[len] = L'\0';
	_tstring s = _tstring(buff1);
	delete[] buff1;
	return s;
#else
	return _tstring(ps, len);
#endif
#else // Linux
	wchar_t* pws = new wchar_t[len + 1];
	char* loc = std::setlocale(LC_CTYPE, "en_US.cp1252");
	len = mbstowcs(pws, ps, len);
	ASSERT(len >= 0);
	pws[len] = L'\0';
	_tstring s = ToStringFromUTF16((const char16*)pws, len);
	delete[] pws;
	std::setlocale(LC_CTYPE, loc);
	return s;
#endif
}
inline _tstring ToStringFromUTF8(const char08* ps, size_t len)
{
#if defined(_WIN32) || defined(_WIN64)
	if (len == (size_t)-1)
		len = strlen((const char*)ps);
	wchar_t* buff1 = new wchar_t[len + 1];
	int rc = win::MultiByteToWideChar(CP_UTF8, 0, ps, (int)len, buff1, (int)len + 1);
	if (rc == 0 || rc > (int)len)
		exit(1);
	buff1[len] = _T('\0');
#if defined( _UNICODE )
	_tstring s = _tstring(buff1);
	delete[] buff1;
	return s;
#else
	char* buff2 = new char[len + 1];
	if (win::WideCharToMultiByte(CP_UTF8, 0, buff1, -1, buff2, (int)len + 1, nullptr, nullptr) == 0)
		exit(1);
	_tstring s = _tstring(buff2);
	delete[] buff1;
	delete[] buff2;
	return s;
#endif
#else // Linux
	return _tstring(ps, len);
#endif
}
inline _tstring ToStringFromUTF16(const char16* pws, [[maybe_unused]]size_t len)
{
#if defined(LINUX32) || defined(LINUX64)
	size_t l = (len != (size_t)-1 ? len : std::char_traits<char16_t>::length(pws));
	char* pns = new char[l * 2 + 1];
	//	l = wcstombs( pns, pws, l );

	std::mbstate_t state{};
	for (size_t n = 0; n < l; ++n)
	{
		int rc = c16rtomb(&pns[n], pws[n], &state); // TODO kann mehr als 4 Bytes sein?
	}

	l = l * 2; // ...
	//	if ( l == (size_t)-1 ) { // es gibt keine negativen Rückgabewerte...
	//		ASSERT( false );
	if (l == 0) { // Falls l == 0 beim Aufruf ist es ok, wenn 0 zurückkommt..
		delete[] pns;
		return "";
	}
	pns[l] = '\0';
	_tstring ns(pns);
	delete[] pns;
	return ns;
#else
	return _tstring((const wchar_t*)pws);
#endif
}
inline std::string ToNarrowString( const wchar_t* pws, size_t len )
{
	size_t l = (len != (size_t)-1 ? len : wcslen( pws ));
	char* pns = new char[l * 2 + 1];
//	l = wcstombs( pns, pws, l );
#if !defined(_WIN32) && !defined(_WIN64)
	l = wcstombs( pns, pws, l );
#else
	// Vorteil: wandelt auch, wenn nicht alle Zeichen wandelbar sind (fügt '?' ein)
	l = (size_t)win::WideCharToMultiByte( CP_OEMCP, 0, pws, (int)l, pns, (int)l * 2 + 1, nullptr, nullptr );
#endif
//	if ( l == (size_t)-1 ) { // es gibt keine negativen Rückgabewerte...
//		ASSERT( false );
	if ( l == 0 ) { // Falls l == 0 beim Aufruf ist es ok, wenn 0 zurückkommt..
		delete[] pns;
		return "";
	}
	pns[l] = '\0';
	std::string ns( pns );
	delete[] pns;
	return ns;
}

#if !defined(_WIN32) && !defined(_WIN64)
inline void _tsplitpath( const _TCHAR* path, _TCHAR* drive, _TCHAR* dir, _TCHAR* fname, _TCHAR* ext )
{
	_tstring s = path;
	size_t pos;
	pos = s.find( ':' );
	if ( drive ) {
		if ( pos != _tstring::npos ) {
			size_t l = std::min<size_t>( _MAX_DRIVE, pos + 1 );
			_tcsncpy( drive, s.c_str(), l );
			drive[l] = _T('\0');
		} else
			*drive = _T('\0');
	}
	if ( pos != _tstring::npos )
		s = s.substr( pos + 1 );

#if defined(_WIN32) || defined(_WIN64)
	pos = s.rfind( '\\' );
#else
	pos = s.rfind( '/' );
#endif
	if ( dir ) {
		if ( pos != _tstring::npos ) {
			size_t l = std::min<size_t>( _MAX_DIR, pos + 1 );
			_tcsncpy( dir, s.c_str(), l );
			dir[l] = _T('\0');
		} else
			*dir = _T('\0');
	}
	if ( pos != _tstring::npos )
		s = s.substr( pos + 1 );

	pos = s.rfind( '.' );
	if ( ext ) { // including "."
		if ( pos != _tstring::npos ) {
			size_t l = std::min<size_t>( _MAX_EXT, s.length() - pos + 1 );
			_tcsncpy( ext, s.c_str() + pos, l );
			ext[l] = _T('\0');
		} else
			*ext = _T('\0');
	}
	if ( pos != _tstring::npos )
		s = s.substr( 0, pos );

	if ( fname ) {
		_tcsncpy( fname, s.c_str(), _MAX_FNAME );
	}
}
inline _TCHAR* _tfullpath( _TCHAR* absPath, const _TCHAR* relPath, size_t maxLength )
{
	return realpath( relPath, absPath );
}
#endif // WIN

inline unsigned long ConvertExtended( const BYTE* buffer ) 
{
   unsigned long mantissa;
   unsigned long last = 0;
   BYTE exp;

   mantissa = FlipBytes( reinterpret_cast<const DWORD*>(buffer + 2) );
   exp = (BYTE)(30 - *(buffer + 1));
   while ( exp-- )
   {
     last = mantissa;
     mantissa >>= 1;
   }
   if (last & 0x00000001) ++mantissa;
   return mantissa;
}

inline DWORD GCD( DWORD a, DWORD b ) 
{
    for( ;; )
    {
        a = a % b;
		if( a == 0 )
			return b;
		b = b % a;
        if( b == 0 )
			return a;
    }
}

