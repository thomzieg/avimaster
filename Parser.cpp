#include "Parser.h"
#include "RIFFParser.h"
#include "MPGParser.h"
#include "AVIParser.h"
#include "MOVParser.h"
#include "ASFParser.h"
#include "RMFParser.h"
#include "RIFFSpecParser.h"
#include "time.h"
#ifndef _CONSOLE
#include "AVIMasterDlg.h"
#endif

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

CParser::CParser( class CIOService& ioservice ) : CIOServiceHelper( ioservice )
{ 
//	m_sIFilename = m_sIFilename;
//	m_sOFilename = m_sOFilename;
	if ( Settings.nVerbosity >= 5 ) 
		Settings.nDumpJunkCount = (DWORD)-1; 
	else if ( Settings.nVerbosity >= 4 ) 
		Settings.nDumpJunkCount = 640 / 3;
}

void CParser::Parse(const _tstring& sIFilename )
{
	m_sIFilename = sIFilename;
	int hti = OSNodeFileNew(0, sIFilename);
	if (ReadOpen(m_sIFilename)) {
		OSStatsSet(CIOService::EPARSERSTATE::PS_PARSING);
		OSStatsInit(GetReadFileSize());
		_tstringstream s;
		s << _T("Reading '") << FilenameGetNameExt(m_sIFilename) << _T("' (") << GetReadFileSize() << _T(")") << std::ends; // s.Format( "Reading '%s%s' (%I64d)", sFName, sExt, GetReadFileSize() ); 
		// TODO filetime ausgeben...
		hti = OSNodeCreate(hti, s.str());
		ReadFile(hti);
		ReadClose();
	}
	OSNodeFileEnd(hti, m_sSummary);
}


bool CParser::DumpGenericUTF8TextChunk(int hti, DWORD nSize, FOURCC fcc, const char* sName)
{
	/*static const*/ DDEF ddef = {
		1, "RIFF File Chunk", "'file'", 2, 0,
		DDEF::DESC::DT_UTF8STRING_LENGTHINBYTES, 0, 3, "Text", nullptr, nullptr,
	};
	ddef.sGUID = ToNarrowString(FCCToString(fcc)).c_str();
	ddef.sName = sName;
	return (Dump(hti, 8, nSize + 8, &ddef) == nSize);
}
bool CParser::DumpGenericUTF16TextChunk(int hti, DWORD nSize, FOURCC fcc, const char* sName)
{
	/*static const*/ DDEF ddef = {
		1, "RIFF File Chunk", "'file'", 2, 0,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, 0, 3, "Text", nullptr, nullptr,
	};
	ddef.sGUID = ToNarrowString(FCCToString(fcc)).c_str();
	ddef.sName = sName;
	return (Dump(hti, 8, nSize + 8, &ddef) == nSize);
}
bool CParser::DumpGenericMBCSTextChunk(int hti, DWORD nSize, FOURCC fcc, const char* sName)
{
	/*static const*/ DDEF ddef = {
		1, "", "", 2, 0,
		DDEF::DESC::DT_MBCSSTRING, 0, 3, "Text", nullptr, nullptr,
	};
	_tstring s = FCCToString(fcc);
	ddef.sGUID = ToNarrowString(s).c_str();
	ddef.sName = sName;
	return (Dump(hti, 8, nSize + 8, &ddef) == nSize);
}

template<typename T> _tstring& CParser::Values( bool bInfix, int hti, T t, _tstring& s, const CParser::DDEFV* ddefv ) const
{
	if ( !ddefv )
		return s;
	if ( ddefv->eType == DDEFV::VDT_INFIX ) {
		if ( !bInfix )
			return s;
		bool b = false;
		for ( WORD i = 0; i < ddefv->nFieldCount; ++i ) {
			ASSERT( ddefv->desc[i].eValue == DDEFV::DESC::VDTV_VALUE );
			if ( t == (T)ddefv->desc[i].nValue ) {
				s += _T(" (") + ToString( ddefv->desc[i].sDescription ) + _T(")");
				b = true;
				break;
			}
		}
		if ( !b )
			s += _T(" (undefined)");
	} else { // Postfix
		if ( bInfix )
			return s;
		for ( WORD i = 0; i < ddefv->nFieldCount; ++i ) {
			if ( ddefv->desc[i].eValue == DDEFV::DESC::VDTV_BITFIELD ) {
				OSNodeCreateValue( hti, ToString( ddefv->desc[i].sDescription ), ((t & ddefv->desc[i].nValue) != 0) );
			} else if ( ddefv->desc[i].eValue == DDEFV::DESC::VDTV_BITVALUE ) {
				OSNodeCreateValue( hti, ToString( ddefv->desc[i].sDescription ) + _T(": ") + ToString( (t & (T)ddefv->desc[i].nValue) ) );
			} else
				ASSERT( false );

		}
	}
	return s;
}
template<typename T> _tstring& CParser::ValuesString( T t, _tstring& s, const CParser::DDEFV* ddefv ) const
{
	if ( !ddefv )
		return s;
	if ( ddefv->eType == DDEFV::VDT_INFIX ) {
		bool b = false;
		for ( WORD i = 0; i < ddefv->nFieldCount; ++i ) {
			ASSERT( ddefv->desc[i].eValue == DDEFV::DESC::VDTV_VALUE );
			if ( t == ToString( ddefv->desc[i].sValue ) ) {
				s += _T(" (") + ToString( ddefv->desc[i].sDescription ) + _T(")");
				b = true;
				break;
			}
		}
		if ( !b ) // TODO Hier eine EP_WARNING erzeugen?
			s += _T(" (undefined)");
	} else
		ASSERT( false );
	return s;
}


template<typename T> inline DWORD CParser::X( T t, int& hti, const _tstring& sPrefix, const CParser::DDEF::DESC* desc, DWORD nField, VALUE_STACK& nvValueStack, DWORD nLoopCount, DWORD nMaxCount ) const
{
	if ( IsNetworkByteorder() )
		t = FlipBytes<T>( &t );
	if ( desc->nVerbosity <= Settings.nVerbosity && nLoopCount <= nMaxCount ) {
		_tstring s;
		if ( desc->nLen & DDEF::DESC::DTL_HEX )
			s = sPrefix + ToString( desc->sDescription ) + _T(": 0x") + ToHexString( t );
		else
			s = sPrefix + ToString( desc->sDescription ) + _T(": ") + ToString( t );
		if ( desc->pStoreValue )
			*reinterpret_cast<T*>( desc->pStoreValue ) = t;
		Values( true, hti, t, s, desc->ddefv );
		hti = OSNodeCreateValue( hti, s );
		Values( false, hti, t, s, desc->ddefv );
	}
	if ( nField < nvValueStack.size() )
		nvValueStack[nField] = (DWORD)(t & (DWORD)-1);
	else
		nvValueStack.push_back( (DWORD)(t & (DWORD)-1) );
	return sizeof( t );
}
template<> inline DWORD CParser::X<BYTE>( BYTE t, int& hti, const _tstring& sPrefix, const CParser::DDEF::DESC* desc, DWORD nField, VALUE_STACK& nvValueStack, DWORD nLoopCount, DWORD nMaxCount ) const
{
	if ( desc->nVerbosity <= Settings.nVerbosity && nLoopCount <= nMaxCount ) {
		_tstring s;
		if ( desc->nLen == DDEF::DESC::DTL_HEX )
			s = sPrefix + ToString( desc->sDescription ) + _T(": 0x") + ToHexString( t );
		else
			s = sPrefix + ToString( desc->sDescription ) + _T(": ") + ToString( t );
		if ( desc->pStoreValue )
			*reinterpret_cast<BYTE*>( desc->pStoreValue ) = t;
		Values( true, hti, t, s, desc->ddefv );
		hti = OSNodeCreateValue( hti, s );
		Values( false, hti, t, s, desc->ddefv );
	}
	if ( nField < nvValueStack.size() )
		nvValueStack[nField].dw = (DWORD)(t & (DWORD)-1);
	else
		nvValueStack.push_back( (DWORD)(t & (DWORD)-1) );
	return sizeof( t );
}
template<> inline DWORD CParser::X<GUID>( GUID t, int& hti, const _tstring& sPrefix, const CParser::DDEF::DESC* desc, DWORD nField, VALUE_STACK& nvValueStack, DWORD nLoopCount, DWORD nMaxCount ) const
{
	if ( desc->nVerbosity <= Settings.nVerbosity && nLoopCount <= nMaxCount ) {
		_tstring s = sPrefix + ToString( desc->sDescription ) + _T(": ") + ToString( t );
		if ( desc->pStoreValue )
			*reinterpret_cast<GUID*>( desc->pStoreValue ) = t;
		ValuesString( ToString( t ), s, desc->ddefv );
		hti = OSNodeCreateValue( hti, s );
	}
	if ( nField >= nvValueStack.size() )
		nvValueStack.push_back( 0 );
	return sizeof( t );
}

DWORD CParser::Dump( int hti, DWORD nFilePosOffset, DWORD nSize, DWORD nChunkSize, const DDEF* ddef, DWORD nMaxCount ) 
{
	ASSERT( nSize - nFilePosOffset < 1024 * 1024 );
	BYTE* p = new BYTE[nSize - nFilePosOffset + 1];
	ASSERT( nSize >= nFilePosOffset );
	ASSERT( nSize <= nChunkSize );
	if ( !ReadBytes( p, Aligned( nSize ) - nFilePosOffset, hti ) ) 
		return 0;
	DWORD r = Dump( hti, Aligned( nSize ), p, Aligned( nSize ) - nFilePosOffset, Aligned( nChunkSize ), ddef, nMaxCount );
	delete[] p;
	return r;
}

DWORD CParser::Dump( int hti, DWORD nFilePosOffset, const void* p, DWORD nSize, DWORD nChunkSize, const DDEF* ddef, DWORD nMaxCount ) const
{
	ASSERT( nFilePosOffset <= nChunkSize );
	ASSERT( nSize <= nChunkSize );
	ASSERT( nSize <= nFilePosOffset );
	DWORD nField = 0; 
	DWORD nPos = 0;
	DWORD nDispPos = 0;
	VALUE_STACK nvValueStack;
	LOOP_STACK nvLoopStack;
	DWORD nLoopField = (DWORD)-1; DWORD nLoopCount = 0; DWORD nLoopSize = 0; bool bNodeLoop = false;
	const void* const base = p;
	if ( ddef->sName && ddef->sGUID )
		hti = OSNodeCreate( hti, GetReadPos() - nFilePosOffset, ToString( ddef->sName ), ToString( ddef->sGUID ), nChunkSize );
	int htiorg = hti, hticur = hti;

	if ( ddef->nMinSize > 0 && nSize < ddef->nMinSize ) 
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Token Size<!") );
	if ( ddef->nMaxSize > 0 && nSize > ddef->nMaxSize ) 
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Token Size>!") );

	DWORD nRepeat = 0;
	while ( nPos < nSize ) {
		DWORD nFieldSize, len;
		int htitmp;
		_tstring s, sPrefix = ToString( nDispPos % 100 ) + _T(" "); // TODO hier fehlt noch der Offset in der Datenstruktur...
		if ( sPrefix.length() == 2 )
			sPrefix = _T("0") + sPrefix;
		if ( nRepeat == 0 && ddef->desc[nField].nLen < 0 && (ddef->desc[nField].eType == DDEF::DESC::DT_WORD || ddef->desc[nField].eType == DDEF::DESC::DT_DWORD || ddef->desc[nField].eType == DDEF::DESC::DT_QWORD) )
			nRepeat = nvValueStack[nField + ddef->desc[nField].nLen].dw;

		switch (ddef->desc[nField].eType) {
		case DDEF::DESC::DT_END:
			ASSERT(ddef->desc[nField].nLen == 0);
			ASSERT(nLoopField == (DWORD)-1);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);
			return nPos;
			break;
		case DDEF::DESC::DT_BYTE:
			ASSERT(ddef->desc[nField].nLen == 0 || ddef->desc[nField].nLen == DDEF::DESC::DTL_HEX);
			hti = hticur;
			nFieldSize = X(*reinterpret_cast<const BYTE*>(p), hti, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_SHORT:
			ASSERT(ddef->desc[nField].nLen <= 0);
			htitmp = hticur;
			nFieldSize = X(*reinterpret_cast<const SHORT*>(p), htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_WORD:
			ASSERT(ddef->desc[nField].nLen <= 0 || ddef->desc[nField].nLen & DDEF::DESC::DTL_HEX);
			htitmp = hticur;
			nFieldSize = X(*reinterpret_cast<const WORD*>(p), htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_TRIPLE: {
			ASSERT(ddef->desc[nField].nLen <= 0 || ddef->desc[nField].nLen & DDEF::DESC::DTL_HEX);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);
			htitmp = hticur;
			const BYTE* b = reinterpret_cast<const BYTE*>(p);
			DWORD t = (b[0] + 256 * b[1] + 256 * 256 * b[2]) * 256;
			nFieldSize = X(t, htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount) - 1;
			break; }
		case DDEF::DESC::DT_LONG:
			ASSERT(ddef->desc[nField].nLen <= 0);
			htitmp = hticur;
			nFieldSize = X(*reinterpret_cast<const LONG*>(p), htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_DWORD:
			ASSERT(ddef->desc[nField].nLen <= 0 || ddef->desc[nField].nLen & DDEF::DESC::DTL_HEX);
			htitmp = hticur;
			nFieldSize = X(*reinterpret_cast<const DWORD*>(p), htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_QWORD:
			ASSERT(ddef->desc[nField].nLen <= 0 || ddef->desc[nField].nLen & DDEF::DESC::DTL_HEX);
			htitmp = hticur;
			nFieldSize = X(*reinterpret_cast<const QWORD*>(p), htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_GUID:
			ASSERT(ddef->desc[nField].nLen == 0);
			htitmp = hticur;
			nFieldSize = X(*reinterpret_cast<const GUID*>(p), htitmp, sPrefix, &ddef->desc[nField], nField, nvValueStack, nLoopCount, nMaxCount);
			break;
		case DDEF::DESC::DT_APPLEFIXEDFLOAT16: {
			ASSERT(ddef->desc[nField].nLen <= 0);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);
			WORD t2 = FlipBytes<WORD>(reinterpret_cast<const WORD*>(p));
			double d = (double)t2 / 256.0;
			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount) {
				OSNodeCreateValue(hticur, sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": ") + ToString(d));
			}
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(0);
			nFieldSize = sizeof(WORD);
			break; }
		case DDEF::DESC::DT_APPLEFIXEDFLOAT32: {
			ASSERT(ddef->desc[nField].nLen <= 0);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);
			DWORD t2 = FlipBytes<DWORD>(reinterpret_cast<const DWORD*>(p));
			double d = (double)t2 / 65536.0;
			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount) {
				OSNodeCreateValue(hticur, sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": ") + ToString(d));
			}
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(0);
			nFieldSize = sizeof(DWORD);
			break; }
		case DDEF::DESC::DT_EXTENDED: {
			ASSERT(ddef->desc[nField].nLen <= 0);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);
			unsigned long l = ConvertExtended((const BYTE*)p);
#if 0
			_asm {
				fld TBYTE PTR xxx;
				fstp f;
			}
#endif
			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount) {
				OSNodeCreateValue(hticur, sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": ") + ToString(l));
			}
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(0);
			nFieldSize = 10;
			break; }
		case DDEF::DESC::DT_CALLBACK:
			if (ddef->desc[nField].nLen < 0)
				len = nvValueStack[nField + ddef->desc[nField].nLen].dw;
			else if (ddef->desc[nField].nLen == 0)
				len = (DWORD)-1; // is calculated by Callback function
			else
				len = (DWORD)ddef->desc[nField].nLen;
			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount) {
				htitmp = OSNodeCreateValue(hticur, sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": "));
				if (len == (DWORD)-1)
					len = ParserCallback(ddef->sGUID, nSize - nPos, (const BYTE*)base, static_cast<DWORD>(reinterpret_cast<const BYTE*>(p) - reinterpret_cast<const BYTE*>(base)), htitmp, nvValueStack, nField);
				else
					ParserCallback(ddef->sGUID, len, (const BYTE*)base, static_cast<DWORD>(reinterpret_cast<const BYTE*>(p) - reinterpret_cast<const BYTE*>(base)), htitmp, nvValueStack, nField);
			}
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(0);
			nFieldSize = len;
			break;
		case DDEF::DESC::DT_FCC: {
			ASSERT(ddef->desc[nField].nLen == 0 || ddef->desc[nField].nLen & DDEF::DESC::DTL_HEX);
			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount) {
				DWORD t = *reinterpret_cast<const DWORD*>(p);
				if (ddef->desc[nField].nLen == DDEF::DESC::DTL_HEX)
					s = sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": 0x") + ToHexString(t) + _T(" ") + FCCToString(t);
				else
					s = sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": ") + FCCToString(t);
				if (ddef->desc[nField].pStoreValue)
					*reinterpret_cast<FOURCC*>(ddef->desc[nField].pStoreValue) = t;
				Values(true, hti, t, s, ddef->desc[nField].ddefv);
				htitmp = OSNodeCreateValue(hticur, s);
				Values(false, htitmp, t, s, ddef->desc[nField].ddefv);
			}
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(0);
			nFieldSize = sizeof(DWORD);
			break; }
		case DDEF::DESC::DT_FILETIME1601:
			ASSERT(ddef->desc[nField].nLen == 0);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);
			{
#if defined(_WIN32) || defined(_WIN64)
				win::SYSTEMTIME stUTC;
				win::FileTimeToSystemTime(reinterpret_cast<const win::FILETIME*>(p), &stUTC);
				_tstringstream ss; ss.fill(_T('0')); ss.flags(std::ios::right);
				if (stUTC.wYear > 1601)
					ss << stUTC.wYear << _T("-") << std::setw(2) << stUTC.wMonth << _T("-") << std::setw(2) << stUTC.wDay << _T(" ");
				ss << std::setw(2) << stUTC.wHour << _T(":") << std::setw(2) << stUTC.wMinute << _T(":") << std::setw(2) << stUTC.wSecond;
				s = _T(" (") + ss.str() + _T(")");
#else
				time_t epochtime = WindowsTickToUnixSeconds(*reinterpret_cast<const long long*>(p));
				struct tm* tm = gmtime(&epochtime);
				_tstringstream ss; ss.fill(_T('0')); ss.flags(std::ios::right);
				ss << 1900 + tm->tm_year << _T("-") << std::setw(2) << 1 + tm->tm_mon << _T("-") << std::setw(2) << tm->tm_mday << _T(" ");
				ss << std::setw(2) << tm->tm_hour << _T(":") << std::setw(2) << tm->tm_min << _T(":") << std::setw(2) << tm->tm_sec;
				s = _T(" (") + ss.str() + _T(")");
#endif
			}
			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount ) 
				OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) + _T(": ") + ToString( *reinterpret_cast<const QWORD*>( p ) ) + _T(" ") + s );
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( 0 );
			nFieldSize = sizeof( QWORD );
			break;
		case DDEF::DESC::DT_DURATION100NS:
			ASSERT( ddef->desc[nField].nLen == 0 );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			{
#if defined(_WIN32) || defined(_WIN64)
				win::SYSTEMTIME stUTC;
				win::FileTimeToSystemTime(reinterpret_cast<const win::FILETIME*>(p), &stUTC);
				_tstringstream ss; ss.fill(_T('0')); ss.flags(std::ios::right);
				ss << std::setw(2) << stUTC.wHour << _T(":") << std::setw(2) << stUTC.wMinute << _T(":") << std::setw(2) << stUTC.wSecond << _T(".") << stUTC.wMilliseconds;
				s = _T(" (") + ss.str() + _T(")");
#else
				time_t epochtime = WindowsTickToUnixSeconds(*reinterpret_cast<const long long*>(p));
				struct tm* tm = gmtime(&epochtime);
				_tstringstream ss; ss.fill(_T('0')); ss.flags(std::ios::right);
				ss << std::setw(2) << tm->tm_hour << _T(":") << std::setw(2) << tm->tm_min << _T(":") << std::setw(2) << tm->tm_sec;
				s = _T(" (") + ss.str() + _T(")");
#endif
			}
			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount ) 
				OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) + _T(": ") + ToString( *reinterpret_cast<const QWORD*>( p ) ) + _T(" ") + s );
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( 0 );
			nFieldSize = sizeof( QWORD );
			break;
		case DDEF::DESC::DT_APPLEDATE1904: {
			ASSERT( ddef->desc[nField].nLen == 0 );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			time_t utime = FlipBytes<DWORD>( (const DWORD*)p ); 
			time_t erg = utime - 2082844800L;
			struct tm tm;
			gmtime_s( &tm, &erg );
			_tstringstream ss; ss.fill( _T('0') ); ss.flags( std::ios::right ); 
			ss << std::setw(2) << tm.tm_year + 1900 << _T("-") << std::setw(2) << tm.tm_mon + 1 << _T("-") << std::setw(2) << tm.tm_mday << _T(" ");
			ss << std::setw(2) << tm.tm_hour << _T(":") << std::setw(2) << tm.tm_min << _T(":") << std::setw(2) << tm.tm_sec;
			s = _T(" (") + ss.str() + _T(")");
			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount ) 
				OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) + _T(": ") + ToString( *reinterpret_cast<const DWORD*>( p ) ) + _T(" ") + s );
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( 0 );
			nFieldSize = sizeof( DWORD );
			break; }
		case DDEF::DESC::DT_APPLEDATE1904_64: {
			ASSERT( ddef->desc[nField].nLen == 0 );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			time_t utime = FlipBytes<QWORD>( (const QWORD*)p ); 
			time_t erg = utime - 2082844800L;
			struct tm tm;
			gmtime_s( &tm, &erg );
			_tstringstream ss; ss.fill( _T('0') ); ss.flags( std::ios::right ); 
			ss << std::setw(2) << tm.tm_year + 1900 << _T("-") << std::setw(2) << tm.tm_mon + 1 << _T("-") << std::setw(2) << tm.tm_mday << _T(" ");
			ss << std::setw(2) << tm.tm_hour << _T(":") << std::setw(2) << tm.tm_min << _T(":") << std::setw(2) << tm.tm_sec;
			s = _T(" (") + ss.str() + _T(")");
			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount ) 
				OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) + _T(": ") + ToString( *reinterpret_cast<const QWORD*>( p ) ) + _T(" ") + s );
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( 0 );
			nFieldSize = sizeof( QWORD );
			break; }
		case DDEF::DESC::DT_MBCSSTRING: {
			ASSERT( ddef->desc[nField].nLen != DDEF::DESC::DTL_HEX );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			
			short n = ddef->desc[nField].nLen;
			if ( n < 0 ) 
				len = nvValueStack[nField + n].dw;
			else if ( n == 0 ) 
				len = nSize - nPos;
			else 
				len = (DWORD)n;
			ASSERT(len >= 0);
			ASSERT(nPos + len <= nSize);
			if (nPos + len > nSize)
				len = nSize - nPos;

			_tstring str = ToStringFromMBCS(static_cast<const char08*>(p), len);

			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount ) 
				OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) + _T(": \"") + str + _T("\"") );
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( str );
//				nvValueStack.push_back( value( ToString( reinterpret_cast<const char*>( p ) ) ) );
			nFieldSize = len;
			break; }
		case DDEF::DESC::DT_UTF8STRING_LENGTHINBYTES: {
			ASSERT(ddef->desc[nField].nLen != DDEF::DESC::DTL_HEX);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);

			//std::wbuffer_convert<std::codecvt_utf16<char16_t>> utf16_conv;
			//std::wstring str = utf16_conv(reinterpret_cast<const char16_t*>(p));

			short n = ddef->desc[nField].nLen;
			if (n < 0)
				len = nvValueStack[nField + n].dw;
			else if (n == 0)
				len = nSize - nPos;
			else
				len = (DWORD)n;
			ASSERT(len < 0x8000);
			ASSERT(nPos + len <= nSize);
			if (nPos + len > nSize)
				len = nSize - nPos;

			_tstring str = ToStringFromUTF8(static_cast<const char08*>(p), len);

			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount)
				OSNodeCreateValue(hticur, sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": \"") + str + _T("\""));
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(str);
			//				nvValueStack.push_back( value( ToString( reinterpret_cast<const char08_t*>( p ) ) ) );
			nFieldSize = len;
			break; }
		case DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES: {
			ASSERT( ddef->desc[nField].nLen != DDEF::DESC::DTL_HEX );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );

			//std::wbuffer_convert<std::codecvt_utf16<char16_t>> utf16_conv;
			//std::wstring str = utf16_conv(reinterpret_cast<const char16_t*>(p));

			short n = ddef->desc[nField].nLen;
			if (n < 0)
				len = nvValueStack[nField + n].dw;
			else if (n == 0)
				len = nSize - nPos;
			else
				len = (DWORD)n;
			ASSERT(len < 0x8000);
			ASSERT(len % 2 == 0);
			ASSERT(nPos + len <= nSize);
			if (nPos + len > nSize)
				len = nSize - nPos;
			len /= sizeof(char16_t);

			_tstring str = ToStringFromUTF16(static_cast<const char16*>(p), len);

			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount )
				OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) + _T(": \"") + str + _T("\"") );
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( str );
//				nvValueStack.push_back( value( ToString( reinterpret_cast<const char16_t*>( p ) ) ) );
			nFieldSize = len * 2;
			break; }
		case DDEF::DESC::DT_UTF16STRING_LENGTHINWORDS: {	
			ASSERT(ddef->desc[nField].nLen != DDEF::DESC::DTL_HEX);
			ASSERT(ddef->desc[nField].pStoreValue == nullptr);

			short n = ddef->desc[nField].nLen;
			if (n < 0)
				len = nvValueStack[nField + n].dw;
			else if (n == 0)
				len = (nSize - nPos) / sizeof(char16_t);
			else
				len = (DWORD)n;
			ASSERT(len < 0x8000);
			ASSERT(nPos + len * sizeof(char16_t) <= nSize);
			if (nPos + len * sizeof(char16_t) > nSize)
				len = (nSize - nPos) / sizeof(char16_t);

			_tstring str = ToStringFromUTF16(static_cast<const char16*>(p), len);

			if (ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount)
				OSNodeCreateValue(hticur, sPrefix + ToString(ddef->desc[nField].sDescription) + _T(": \"") + str + _T("\""));
			if (nField >= nvValueStack.size())
				nvValueStack.push_back(str);
			//				nvValueStack.push_back( value( ToString( reinterpret_cast<const char16_t*>( p ) ) ) );
			nFieldSize = len * 2;
			break; }
		case DDEF::DESC::DT_LOOPEND:
le:			ASSERT( nLoopField != (DWORD)-1 );
			ASSERT( ddef->desc[nField].nLen == 0 );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			nFieldSize = 0;
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( 0 );
			if (  nLoopCount >= nvValueStack[nLoopField].dw ) {
				if ( nvLoopStack.size() > 0 ) {
					LOOP_STACK_ENTRY v = nvLoopStack.back();
					nvLoopStack.pop_back();
					nLoopField = v.nField;
					nLoopCount = v.nCount;
					nLoopSize += v.nSize;
					bNodeLoop = v.bNodeLoop;
					htiorg = v.hti;
				} else {
					nLoopField = (DWORD)-1;
				}
				nField = (DWORD)nvValueStack.size() - 1;
				if ( bNodeLoop && nLoopCount <= nMaxCount && nLoopField != (DWORD)-1 && ddef->desc[nLoopField].nVerbosity <= Settings.nVerbosity )
					OSNodeAppendSize( htiorg, nLoopSize );
				break; // end switch...
			} else {
				if ( bNodeLoop && nLoopCount <= nMaxCount && ddef->desc[nLoopField].nVerbosity <= Settings.nVerbosity )
					OSNodeAppendSize( htiorg, nLoopSize );
				nField = nLoopField;
			}
			[[fallthrough]];
		case DDEF::DESC::DT_NODELOOP:
		case DDEF::DESC::DT_LOOP:
			ASSERT( nLoopField == (DWORD)-1 || nLoopField <= nField );
			ASSERT( ddef->desc[nField].nLen != DDEF::DESC::DTL_HEX );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			if ( nField > nLoopField ) { // verschachtelte LOOP
				LOOP_STACK_ENTRY v;
				v.hti = htiorg;
				v.nField = nLoopField;
				v.nCount = nLoopCount;
				v.nSize = nLoopSize;
				v.bNodeLoop = bNodeLoop;
				nvLoopStack.push_back( v );
				nLoopField = (DWORD)-1;
				htiorg = hticur;
			}

			nFieldSize = 0;
			nLoopSize = 0;
			nDispPos = 0;
			if ( nLoopField == (DWORD)-1 ) { // erstmalig
				if ( ddef->desc[nField].nLen < 0 )
					len = nvValueStack[nField + ddef->desc[nField].nLen].dw;
				else if ( ddef->desc[nField].nLen == 0 )
					len = (DWORD)-1; // loop until end of object
				else 
					len = (DWORD)ddef->desc[nField].nLen;
				nvValueStack.push_back( len );
				nLoopField = nField;
				nLoopCount = 0;
				if ( len == 0 ) {
					for ( /*nField*/; nField < ddef->nFieldCount; ++nField ) {
						if ( nField >= nvValueStack.size() )
							nvValueStack.push_back( 0 );
						if ( ddef->desc[nField].eType == DDEF::DESC::DT_LOOPEND )
							goto le;
					}
					goto x;
				}
			}
			if ( nLoopCount++ < nMaxCount && ddef->desc[nLoopField].nVerbosity <= Settings.nVerbosity ) {
				if ( ddef->desc[nField].eType == DDEF::DESC::DT_NODELOOP ) {
					hticur = OSNodeCreate( htiorg, GetReadPos() - nSize + nPos, ToString( ddef->desc[nField].sDescription ) + _T(" Item #") + ToString( nLoopCount ) ); // size wird nachträglich gesetzt
					bNodeLoop = true;
				}
				else {
					hticur = OSNodeCreateValue( htiorg, ToString( ddef->desc[nField].sDescription ) + _T(" Item #") + ToString( nLoopCount ) ); // ....
					ASSERT( !bNodeLoop );
				}
			}
			if ( nLoopCount > nvValueStack[nLoopField].dw ) {
				nLoopField = (DWORD)-1;
				ASSERT( false ); // sollte hier nicht wie ein paar Zeilen oben DT_LOOPEND gesucht werden???
				nField = (DWORD)nvValueStack.size(); 
			}
			break;
		case DDEF::DESC::DT_BYTES_ADJUSTSIZE: // Anzahl Bytes bis LoopStart von Länge abziehen
			ASSERT( ddef->desc[nField].nLen < 0 );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			len = nvValueStack[nField + ddef->desc[nField].nLen].dw;
			if ( nLoopField != (DWORD)-1 ) {
				ASSERT( nLoopSize <= len );
				len -= nLoopSize;
			} else {
				ASSERT( nPos <= len );
				len -= nPos;
			}
			goto c;
		case DDEF::DESC::DT_BYTES:
			ASSERT( ddef->desc[nField].nLen != DDEF::DESC::DTL_HEX );
			ASSERT( ddef->desc[nField].pStoreValue == nullptr );
			if ( ddef->desc[nField].nLen < 0 )
				len = nvValueStack[nField + ddef->desc[nField].nLen].dw;
			else if ( ddef->desc[nField].nLen == 0 )
				len = nSize - nPos;
			else 
				len = (DWORD)ddef->desc[nField].nLen;
c:			if ( ddef->desc[nField].nVerbosity <= Settings.nVerbosity && nLoopCount < nMaxCount ) {
				htitmp = OSNodeCreateValue( hticur, sPrefix + ToString( ddef->desc[nField].sDescription ) );
				DumpBytes( p, (DWORD)len, htitmp );
			} 
			if ( nField >= nvValueStack.size() )
				nvValueStack.push_back( 0 );
			nFieldSize = len;
			break;
		default:
			ASSERT( false );
			break;
		}
		if ( nRepeat > 1 )
			nRepeat--;
		else {
			nRepeat = 0;
			++nField;
		}
		nLoopSize += nFieldSize;
		nPos += nFieldSize;
		nDispPos += nFieldSize;
		bool bAppendSize = true;
		if ( nField >= ddef->nFieldCount ) {
			if ( nLoopField != (DWORD)-1 ) {
				nField = nLoopField;
				if ( bNodeLoop && nLoopCount <= nMaxCount && ddef->desc[nLoopField].nVerbosity <= Settings.nVerbosity ) {
					OSNodeAppendSize( htiorg, nLoopSize );
					bAppendSize = false;
				}
			}
			else {
				break; // end parsing
			}
		}

		p = AddBytes( p, nFieldSize );
		if ( nPos >= nSize ) { 
			ASSERT( nPos == nSize );
			if ( nPos != nSize )
				OSNodeCreateAlert( htiorg, CIOService::EPRIORITY::EP_ERROR, _T("Offset " ) + ToString( nPos % 100 ) + _T(", Missing Data ") + ToString( nPos - nSize ) );
			if ( nLoopField != (DWORD)-1 ) {
				if ( bNodeLoop && nLoopCount <= nMaxCount && bAppendSize && ddef->desc[nLoopField].nVerbosity <= Settings.nVerbosity )
					OSNodeAppendSize( htiorg, nLoopSize );
			}
			break;
		}
	}
x:	if ( Aligned( nPos ) == nSize && Aligned( nPos ) != nPos )
		nPos++;
	else if ( Aligned( nPos ) < nSize ) { // TODO && !Settings.bForceChunkSize
		hticur = OSNodeCreateAlert( htiorg, CIOService::EPRIORITY::EP_ERROR, _T("Additional Data " ) + ToString( nSize - nPos ) );
		DumpBytes( p, nSize - nPos, hticur );
		nPos = nSize;
	}
	return nPos;
}

DWORD CParser::DumpBitmapinfo( DWORD& nWidth, DWORD& nHeight, DWORD nSize, const BYTE *buffer, DWORD pos, int hti, bool bDumpExtraData ) const
{
	static constexpr DDEFV ddefv1 =	{
		83, DDEFV::VDT_INFIX,
		(DWORD_PTR)my::BITMAPINFOHEADER::BiCompression::BI_RGB, "RGB (uncompressed)", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)my::BITMAPINFOHEADER::BiCompression::BI_RLE8, "RLE8", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)my::BITMAPINFOHEADER::BiCompression::BI_RLE4, "RLE4", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)my::BITMAPINFOHEADER::BiCompression::BI_BITFIELDS, "Bitfields (uncompressed)", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)my::BITMAPINFOHEADER::BiCompression::BI_JPEG, "JPEG", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)my::BITMAPINFOHEADER::BiCompression::BI_PNG, "PNG", DDEFV::DESC::VDTV_VALUE,
		FCC('ffds'), "FFDShow", DDEFV::DESC::VDTV_VALUE,
		FCC('cvid'), "Radius Cinepak", DDEFV::DESC::VDTV_VALUE,
		FCC('CVID'), "Radius Cinepak", DDEFV::DESC::VDTV_VALUE,
		FCC('dvsd'), "DV Standard Definition 25Mb/s", DDEFV::DESC::VDTV_VALUE, // IEC-61834 SDL-DVCR 525-60 or SDL-DVCR 625-50
		FCC('DVSD'), "DV Standard Definition 25Mb/s", DDEFV::DESC::VDTV_VALUE,
		FCC('dv25'), "DVCPRO25 25Mb/s, 4:1:1", DDEFV::DESC::VDTV_VALUE, // DVCPRO 25 (525-60 or 625-50)
		FCC('dv50'), "DVCPRO50 50Mb/s, 4:2:2", DDEFV::DESC::VDTV_VALUE, // DVCPRO 50 (525-60 or 625-50)
		FCC('dvh1'), "DV High Definition 100Mb/s", DDEFV::DESC::VDTV_VALUE, // DVCPRO 100 (1080/60i, 1080/50i, or 720/60P)
		FCC('dvhd'), "DV High Definition 50Mb/s", DDEFV::DESC::VDTV_VALUE, // IEC-61834 HD-DVCR 1125-60 or HD-DVCR 1250-50
		FCC('dvsl'), "DV Low Definition 12.5Mb/s", DDEFV::DESC::VDTV_VALUE, // IEC-61834 SD-DVCR 525-60 or SD-DVCR 625-50
		FCC('mjpg'), "Motion-JPEG", DDEFV::DESC::VDTV_VALUE,
		FCC('MJPG'), "Motion-JPEG", DDEFV::DESC::VDTV_VALUE,
		FCC('tscc'), "TechSmith Camtasia", DDEFV::DESC::VDTV_VALUE,
		FCC('VCR1'), "ATI Video", DDEFV::DESC::VDTV_VALUE,
		FCC('VCR2'), "ATI Video", DDEFV::DESC::VDTV_VALUE,
		FCC('RLE4'), "MS RLE", DDEFV::DESC::VDTV_VALUE,
		FCC('RLE8'), "MS RLE", DDEFV::DESC::VDTV_VALUE,
		FCC('RLE '), "MS RLE", DDEFV::DESC::VDTV_VALUE,
		FCC('mrle'), "MS RLE", DDEFV::DESC::VDTV_VALUE,
		FCC('cram'), "MS Video 1", DDEFV::DESC::VDTV_VALUE,
		FCC('CRAM'), "MS Video 1", DDEFV::DESC::VDTV_VALUE,
		FCC('wham'), "MS Video 1", DDEFV::DESC::VDTV_VALUE,
		FCC('WHAM'), "MS Video 1", DDEFV::DESC::VDTV_VALUE,
		FCC('msvc'), "MS Video 1", DDEFV::DESC::VDTV_VALUE,
		FCC('MSVC'), "MS Video 1", DDEFV::DESC::VDTV_VALUE, // Compression CRAM (auch DIV3?)
		FCC('MP42'), "MS MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('MP43'), "MS MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('MP44'), "MS MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('wmv1'), "MS Windows Media 7", DDEFV::DESC::VDTV_VALUE,
		FCC('WMV1'), "MS Windows Media 7", DDEFV::DESC::VDTV_VALUE,
		FCC('wmv2'), "MS Windows Media 8", DDEFV::DESC::VDTV_VALUE,
		FCC('WMV2'), "MS Windows Media 8", DDEFV::DESC::VDTV_VALUE,
		FCC('wmv3'), "MS Windows Media 9", DDEFV::DESC::VDTV_VALUE,
		FCC('WMV3'), "MS Windows Media 9", DDEFV::DESC::VDTV_VALUE,
		FCC('davc'), "H.264/MPEG-4 AVC base profile", DDEFV::DESC::VDTV_VALUE,
		FCC('CDVC'), "Canopus DV24", DDEFV::DESC::VDTV_VALUE,
		FCC('52vd'), "Matros Digisuite DV25", DDEFV::DESC::VDTV_VALUE,
		FCC('AVRn'), "AVID M-JPEG", DDEFV::DESC::VDTV_VALUE,
		FCC('ADVJ'), "AVID M-JPEG", DDEFV::DESC::VDTV_VALUE,
		FCC('3IV1'), "3ivx Delta MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('3IV2'), "3ivx Delta MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('VSSH'), "Vanguard Software Solutions MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('DAVC'), "mpegable MPEG-4", DDEFV::DESC::VDTV_VALUE,
		FCC('AP41'), "AngelPotion MS MP43 Hack", DDEFV::DESC::VDTV_VALUE,
		FCC('MMES'), "Matrox MPEG I-frame", DDEFV::DESC::VDTV_VALUE,
		FCC('ASV1'), "Asus Video", DDEFV::DESC::VDTV_VALUE,
		FCC('ASV2'), "Asus Video", DDEFV::DESC::VDTV_VALUE,
		FCC('ASVX'), "Asus Video", DDEFV::DESC::VDTV_VALUE,
		FCC('divx'), "DivX/OpenDivX", DDEFV::DESC::VDTV_VALUE,
		FCC('DIVX'), "DivX/OpenDivX", DDEFV::DESC::VDTV_VALUE,
		FCC('DIV3'), "DivX/OpenDivX", DDEFV::DESC::VDTV_VALUE,
		FCC('DIV4'), "DivX/OpenDivX", DDEFV::DESC::VDTV_VALUE,
		FCC('DIV5'), "DivX/OpenDivX", DDEFV::DESC::VDTV_VALUE,
		FCC('DX50'), "DivX/OpenDivX", DDEFV::DESC::VDTV_VALUE,
		FCC('DXSB'), "DivX Subtitles", DDEFV::DESC::VDTV_VALUE,
		FCC('IAN '), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE, // ???
		FCC('IV30'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV31'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV32'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV33'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV34'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV35'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV36'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV37'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV38'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV39'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV40'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV41'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV42'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV43'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV44'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV45'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV46'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV47'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV48'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV49'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
		FCC('IV50'), "Intel/Ligos Indeo", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 =	{
		1, DDEFV::VDT_INFIX,
		0x0001, "standard", DDEFV::DESC::VDTV_VALUE,
	};
	DDEF ddef1 = {
		12, nullptr, nullptr, 40, 0,
		DDEF::DESC::DT_DWORD, 0, 4, "biSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 2, "biWidth", nullptr, &nWidth,
		DDEF::DESC::DT_DWORD, 0, 2, "biHeight", nullptr, &nHeight,
		DDEF::DESC::DT_WORD, 0, 4, "Reserved (Planes)", &ddefv2, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "biBitCount (bpp)", nullptr, nullptr,
		DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 2, "biCompression", &ddefv1, nullptr,
		DDEF::DESC::DT_DWORD, 0, 2, "biSizeImage", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "biXPelsPerMeter", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "biYPelsPerMeter", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "biClrUsed", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "biClrImportant", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, 0, 4, "CodecSpecificData", nullptr, nullptr,
	};
	DDEF ddef2 = {
		12, nullptr, nullptr, 40, 0,
		DDEF::DESC::DT_DWORD, 0, 4, "biSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 2, "biWidth", nullptr, &nWidth,
		DDEF::DESC::DT_DWORD, 0, 2, "biHeight", nullptr, &nHeight,
		DDEF::DESC::DT_WORD, 0, 4, "Reserved (Planes)", &ddefv2, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "biBitCount (bpp)", nullptr, nullptr,
		DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 2, "biCompression", &ddefv1, nullptr,
		DDEF::DESC::DT_DWORD, 0, 2, "biSizeImage", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "biXPelsPerMeter", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "biYPelsPerMeter", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "biClrUsed", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "biClrImportant", nullptr, nullptr,
		DDEF::DESC::DT_END, 0, 4, "", nullptr, nullptr,
	};
	return Dump( hti, nSize, buffer + pos, nSize, nSize, (bDumpExtraData ? &ddef1 : &ddef2) );
}
