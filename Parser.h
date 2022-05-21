#pragma once

#include "IOService.h"

class CParser : public CIOServiceHelper
{
public:
	CParser( class CIOService& ioservice );
	virtual ~CParser() {};

	//virtual void Parse( const _tstring& sIFilename, const _tstring& sOFilename ) = 0;
	void Parse(const _tstring& sIFilename);
	virtual bool ReadFile(int /*hti*/) { ASSERT(false); return false; }
	virtual bool ReadChunk(int /*hti*/, QWORD /*nSize*/) { ASSERT(false); return false; }
	QWORD GetChecksum() const { return m_nChecksum; }

protected:
	//bool GetProbe( BYTE* pBuffer, WORD nSize );

	struct DDEFV {
		DWORD nFieldCount;
		enum { VDT_INFIX, VDT_POSTFIX } eType; // One Value or multiple Values
		struct DESC {
			union {
				DWORD_PTR nValue;
				const char* sValue;
			};
			const char* sDescription;
			enum { VDTV_BITFIELD, VDTV_BITVALUE, VDTV_VALUE } eValue; // VALUE für INFIX, oder Bit oder Geundeter Value
		} desc[99];
	};

	struct DDEF {
		WORD nFieldCount;
		const char* sName;
		const char* sGUID;
		WORD nMinSize;
		WORD nMaxSize;
		struct DESC {
			enum { DT_BYTE, DT_SHORT, DT_WORD, DT_TRIPLE, DT_LONG, DT_DWORD, DT_QWORD, DT_UTF8STRING_LENGTHINBYTES, DT_UTF16STRING_LENGTHINBYTES,
					DT_UTF16STRING_LENGTHINWORDS, DT_MBCSSTRING, DT_BYTES, DT_BYTES_ADJUSTSIZE, DT_FILETIME1601, 
					DT_DURATION100NS, DT_APPLEDATE1904, DT_APPLEDATE1904_64, DT_APPLEFIXEDFLOAT16, DT_APPLEFIXEDFLOAT32,
					DT_EXTENDED, DT_NODELOOP, DT_LOOP, DT_LOOPEND, DT_GUID, DT_CALLBACK, DT_FCC, DT_END } eType;
			short nLen; 
			enum { DTL_HEX = 0x4000 }; // interpret as Hex Value, don't use high bit! Das ist noch nicht ok!!!
			short nVerbosity;
			const char* sDescription;
			const DDEFV* ddefv;
			void* pStoreValue;
		} desc[99];
	};

	struct value { value( const _tstring _s ) { s = _s; } value ( DWORD _dw ) { dw = _dw; } DWORD dw; _tstring s; }; // union geht nicht...
	typedef std::vector<value> VALUE_STACK;
	struct LOOP_STACK_ENTRY {
		int hti;
		DWORD nField;
		DWORD nCount;
		DWORD nSize;
		bool bNodeLoop;
	};
	typedef std::vector<LOOP_STACK_ENTRY> LOOP_STACK;
	
	virtual DWORD ParserCallback( const std::string& /*sGuid*/, DWORD /*nSize*/, const BYTE* /*buffer*/, DWORD /*pos*/, int /*hti*/, const VALUE_STACK& /*vstack*/, DWORD /*nField*/ ) const { ASSERT( false ); return 0; }

	// Hier bei nSize die Gesamtgröße, also bereits gelesen (nFileOffset) + noch zu lesen reinstecken 
	DWORD Dump( int hti, DWORD nFilePosOffset, DWORD nSize, const DDEF* ddef, DWORD nMaxCount = (DWORD)-1 ) { return Dump( hti, nFilePosOffset, nSize, nSize, ddef, nMaxCount ); }
	DWORD Dump( int hti, DWORD nFilePosOffset, DWORD nSize, DWORD nChunkSize, const DDEF* ddef, DWORD nMaxCount = (DWORD)-1 );
	// hier bei nSize nur was noch zu lesen ist reinstecken
	DWORD Dump( int hti, DWORD nFilePosOffset, const void* p, DWORD nSize, DWORD nChunkSize, const DDEF* ddef, DWORD nMaxCount = (DWORD)-1 ) const;
	template<typename T> inline DWORD X( T t, int& hti, const _tstring& sPrefix, const CParser::DDEF::DESC* ddef, DWORD nField, VALUE_STACK& nvValues, DWORD nLoopCount, DWORD nMaxCount ) const;
	template<typename T> inline _tstring& Values(bool bInfix, int hti, T t, _tstring& s, const CParser::DDEFV* ddef) const;
	template<typename T> inline _tstring& ValuesString( T t, _tstring& s, const CParser::DDEFV* ddef ) const;

	DWORD DumpBitmapinfo( DWORD& nWidth, DWORD& nHeight, DWORD nSize, const BYTE *buffer, DWORD pos, int hti, bool bDumpExtraData ) const;

	bool DumpGenericUTF8TextChunk(int hti, DWORD nSize, FOURCC fcc, const char* sName);
	bool DumpGenericUTF16TextChunk(int hti, DWORD nSize, FOURCC fcc, const char* sName);
	bool DumpGenericMBCSTextChunk(int hti, DWORD nSize, FOURCC fcc, const char* sName);

protected:
	_tstring m_sIFilename;
	QWORD m_nSizePayload = 0;
	QWORD m_nJunkSize = 0;
	QWORD m_nChecksum = 0;
	_tstring m_sSummary;
};

template<> inline DWORD CParser::X<GUID>(GUID t, int& hti, const _tstring& sPrefix, const CParser::DDEF::DESC* ddef, DWORD nField, VALUE_STACK& nvValues, DWORD nLoopCount, DWORD nMaxCount) const;
template<> inline DWORD CParser::X<BYTE>(BYTE t, int& hti, const _tstring& sPrefix, const CParser::DDEF::DESC* ddef, DWORD nField, VALUE_STACK& nvValues, DWORD nLoopCount, DWORD nMaxCount) const;
