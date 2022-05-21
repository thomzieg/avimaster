#pragma once

#include "Helper.h"
#include "IOBase.h"

#ifndef _CONSOLE
class CAVIMasterDlg;
#endif

class Settings {
public:
	Settings();
	// GUI exposed
	int nVerbosity;
	bool bCheckIndex; 
	DWORD nTraceMoviEntries;
	DWORD nTraceIndxEntries;
	bool bForceChunkSize;
	bool bTight;
#ifndef _CONSOLE
	static _tstring sExtAVI;
	static _tstring sExtMPG;
	static _tstring sExtMOV;
	static _tstring sExtRMF;
	static _tstring sExtRIFF;
	static _tstring sExtASF;
	static _tstring sExtMKV;
	static _tstring sExtOther; // no GUI
	bool bHexHex;
	bool bHexEdit;
#endif

	// GUI not exposed
	bool bIgnoreIndexOrder;
	DWORD nTraceIdx1Entries; // überflüssig?
	DWORD nFastmodeCount;
	enum class EFORCE { EM_FORCENONE, EM_FORCERIFFINTEL, EM_FORCERIFFNETWORK, EM_FORCEMPEG };
	EFORCE eForce;

	// per Write
	bool bFixAudio;
	double fSetVideoFPS;
	std::set<WORD> vStripStreams;

//private:
	DWORD nDumpJunkCount;
};

class CIOService
{
	friend class CIOServiceHelper;
public:
	CIOService(class Settings settings) : Settings(settings) { m_bAligned = false; m_bNetworkByteorder = false; }
	//	~CIOService();
	enum class EPARSERSTATE { PS_PARSING = 0, PS_INDEXCHECK, PS_CLEANUP, PS_WRITING, PS_READY };
	enum class EPRIORITY { EP_NONE, EP_INFO, EP_WARNING, EP_ERROR, EP_FATAL };
	// TODO CIOService::EPRIORITY::EP_INTERNAL, z.b. wenn Chunksize > MAXDWORD oder als FATAL

	void SetParams(bool bAligned, bool bNetworkByteorder) { m_bAligned = bAligned; m_bNetworkByteorder = bNetworkByteorder; }
	bool IsNetworkByteorder() const { return m_bNetworkByteorder; }
	bool IsAligned() const { return m_bAligned; }

	int OSNodeFileNew(int htiparent, const _tstring& sText) const;
	int OSNodeFileEnd(int htiparent, const _tstring& sText) const;
	int OSNodeCreate(int htiparent, QWORD nPos, const _tstring& sText, const _tstring& sToken1, QWORD nSize = -1, const _tstring& sToken2 = _T("")) const;
	int OSNodeCreate(int htiparent, QWORD nPos, const _tstring& sText, FOURCC fcc1 = 0, DWORD nSize = (DWORD)-1, FOURCC fcc2 = 0) const { return OSNodeCreate(htiparent, nPos, sText, fcc1 > 0 ? FCCToString(fcc1) : _T(""), (nSize == (DWORD)-1 ? (QWORD)-1 : nSize), fcc2 > 0 ? FCCToString(fcc2) : _T("")); }
	int OSNodeCreate(int htiparent, const _tstring& sText) const;
	int OSNodeAppendSize(int htiparent, QWORD nSize) const; // Größe nachträglich anhängen
	int OSNodeAppendImportant(int htiparent) const; // nachträglich setzen
	int OSNodeCreateValue(int hti, const _tstring& s) const;
	int OSNodeCreateValue(int hti, const _tstring& s, bool bBold) const;
	int OSNodeCreateValue(int hti, int nLevel, const _tstring& s) const { if (nLevel <= Settings.nVerbosity) return OSNodeCreateValue(hti, s); else return hti; }
	int OSNodeCreateAlert(int hti, EPRIORITY ePriority, const _tstring& sText) const;

	void OSStatsInit(QWORD nSize) const;
	void OSStatsSet(QWORD nPos) const;
	void OSStatsSet(EPARSERSTATE eParserState) const;

#ifndef _CONSOLE
	static void SetDialog(CAVIMasterDlg* pDialog);
#endif

	bool DumpBytes(const void* p, DWORD n, int hti) const;

	bool ReadOpen(const _tstring& sFilename) { return ior.Open(sFilename); }
	bool ReadClose() { return ior.Close(); }
	bool ReadSeek(__int64 n, bool bCount = true) { return ior.Seek(n, bCount); }
	bool ReadBytes(void* p, DWORD n, int hti, bool bCount = true);
	bool ReadMax(void* p, DWORD nActual, DWORD nMax, int hti);
	bool ReadSeekFwdDumpChecksum(QWORD n, QWORD& nChecksum, int hti = 0); // only forward...
	bool ReadSeekFwdDump(QWORD n, int hti = 0); // only forward...
	bool ReadDumpBytes(BYTE* p, DWORD n, int hti, bool bCount = true);
	bool ReadDumpErrorContext(DWORD n, int hti);
	//	bool ReadDWORD( DWORD& n, int hti ) const;
	bool ReadAlign(DWORD& nSize) { if (m_bAligned && (nSize & 1) != 0 && GetReadTotalBytes() < GetReadFileSize()) { if (!ReadSeekFwdDump(1)) return false; ++nSize; } return true; };
	QWORD GetReadFileSize() const { return ior.GetFileSize(); }
	QWORD GetReadPos() const { return ior.GetPos(); }
	bool SetReadPos(QWORD n) { return ior.SetPos(n); }
	QWORD GetReadTotalBytes() const { return ior.GetTotalBytes(); }
	void ReadAddTotalBytes(QWORD n) { ior.AddTotalBytes(n); }
	bool ReadN(void* p, DWORD n) { return ior.ReadN(p, n); }

	bool WriteOpenExisting(const _tstring& sFilename) { return iow.OpenExisting(sFilename); }
	bool WriteOpenCreate(const _tstring& sFilename) { return iow.OpenCreate(sFilename); }
	bool WriteClose() { return iow.Close(); }
	bool WriteSeek(__int64 n, bool bCount = true) { return iow.Seek(n, bCount); }
	bool WriteBytes(const void* p, DWORD n, bool bCount = true) { return iow.Write(p, n, false, bCount); }
	bool WriteExtendNull(DWORD nSize) { return iow.ExtendNull(nSize); }
	bool WritePreallocate(DWORD nSize) { return iow.Preallocate(nSize); }
	bool WriteSeekRead(QWORD nPos, DWORD nSize);
	bool WriteExtendAlign(DWORD& nSize) { if (nSize & 0x1) { if (!WriteExtendNull(1)) return false; ++nSize; } return true; };
	bool WriteString(const std::string& s, bool bCount = true) { return iow.Write(s.c_str(), (DWORD)s.length(), false, bCount); }
	QWORD GetWriteTotalBytes() const { return iow.GetTotalBytes(); }
	QWORD GetWritePos() const { return iow.GetPos(); }
	bool SetWritePos(QWORD n) { return iow.SetPos(n); }
	bool SetWritePosStart() { return iow.SetPosStart(); }
	bool SetWritePosEnd() { return iow.SetPosEnd(); }

	inline DWORD Align(DWORD& n) const { if (m_bAligned) n += (n & 0x01); return n; }
	inline DWORD Aligned(DWORD n) const { return (m_bAligned ? n + (n & 0x01) : n); }


#ifdef _UNICODE
	static void HandleUnicodeError(_tostream& o);
#else
	static void HandleUnicodeError(const _tostream&) {}
#endif

protected:
	bool m_bAligned;
	bool m_bNetworkByteorder;
	mutable DWORD m_nStatCount;
#ifndef _CONSOLE
	static CAVIMasterDlg* m_pDialog;
#endif

public:
	class Settings Settings;
protected:
	class ior ior; // gcc needs class
	class iow iow; // gcc needs class
};

class CIOServiceHelper
{
protected:
	CIOServiceHelper(class CIOService& ioservice) : Settings(ioservice.Settings), io(ioservice) {}

	void SetParams( bool bAligned, bool bNetworkByteorder ) { io.SetParams( bAligned, bNetworkByteorder ); }
	bool IsNetworkByteorder() const { return io.m_bNetworkByteorder; }
	bool IsAligned() const { return io.m_bAligned; }

	int OSNodeFileNew(int htiparent, const _tstring& sText) const { return io.OSNodeFileNew(htiparent, sText); }
	int OSNodeFileEnd(int htiparent, const _tstring& sText) const { return io.OSNodeFileEnd(htiparent, sText); }
	int OSNodeCreate( int htiparent, QWORD nPos, const _tstring& sText, const _tstring& sToken1, QWORD nSize = -1, const _tstring& sToken2 = _T("") ) const { return io.OSNodeCreate(htiparent, nPos, sText, sToken1, nSize, sToken2); }
	int OSNodeCreate( int htiparent, QWORD nPos, const _tstring& sText, FOURCC fcc1 = 0, QWORD nSize = (QWORD)-1, FOURCC fcc2 = 0 ) const { return OSNodeCreate( htiparent, nPos, sText, fcc1 > 0 ? FCCToString( fcc1 ) : _T(""), nSize, fcc2 > 0 ? FCCToString( fcc2 ) : _T("") ); }
	int OSNodeCreate( int htiparent, const _tstring& sText ) const { return io.OSNodeCreate(htiparent, sText); }
	int OSNodeAppendSize( int htiparent, QWORD nSize ) const { return io.OSNodeAppendSize(htiparent, nSize); } // Größe nachträglich anhängen
	int OSNodeAppendImportant( int htiparent ) const { return io.OSNodeAppendImportant(htiparent); } // nachträglich setzen
	int OSNodeCreateValue( int hti, const _tstring& s ) const { return io.OSNodeCreateValue(hti, s); }
	int OSNodeCreateValue( int hti, const _tstring& s, bool bBold ) const { return io.OSNodeCreateValue(hti, s, bBold); }
	int OSNodeCreateValue( int hti, int nLevel, const _tstring& s ) const { return io.OSNodeCreateValue(hti, nLevel, s); } 
	int OSNodeCreateAlert( int hti, CIOService::EPRIORITY ePriority, const _tstring& sText ) const { return io.OSNodeCreateAlert(hti, ePriority, sText); }

	void OSStatsInit( QWORD nSize ) const { return io.OSStatsInit(nSize); }
	void OSStatsSet(QWORD nPos) const { return io.OSStatsSet(nPos); }
	void OSStatsSet(CIOService::EPARSERSTATE eParserState ) const { return io.OSStatsSet(eParserState); }

#ifndef _CONSOLE
	static void SetDialog(CAVIMasterDlg* pDialog);
#endif

	bool DumpBytes( const void* p, DWORD n, int hti ) const { return io.DumpBytes(p, n, hti); }

	bool ReadOpen( const _tstring& sFilename ) { return io.ior.Open( sFilename ); }
	bool ReadClose() { return io.ior.Close(); }
	bool ReadSeek( __int64 n, bool bCount = true ) { return io.ior.Seek( n, bCount ); }
	bool ReadBytes(void* p, DWORD n, int hti, bool bCount = true) { return io.ReadBytes(p, n, hti, bCount); }
	bool ReadMax( void* p, DWORD nActual, DWORD nMax, int hti ) { return io.ReadMax(p, nActual, nMax, hti); }
	bool ReadSeekFwdDumpChecksum(QWORD n, QWORD& nChecksum, int hti = 0) { return io.ReadSeekFwdDumpChecksum(n, nChecksum, hti); } // only forward...
	bool ReadSeekFwdDump( QWORD n, int hti = 0 ) { return io.ReadSeekFwdDump(n, hti); } // only forward...
	bool ReadDumpBytes( BYTE* p, DWORD n, int hti, bool bCount = true ) { return io.ReadDumpBytes(p, n, hti, bCount); }
	bool ReadDumpErrorContext( DWORD n, int hti ) { return io.ReadDumpErrorContext(n, hti); }
//	bool ReadDWORD( DWORD& n, int hti ) const;
	bool ReadAlign( DWORD& nSize ) { if (io.m_bAligned && (nSize & 1) != 0 && GetReadTotalBytes() < GetReadFileSize() ) { if ( !ReadSeekFwdDump( 1 ) ) return false; ++nSize; } return true; };
	QWORD GetReadFileSize() const { return io.ior.GetFileSize(); }
	QWORD GetReadPos() const { return io.ior.GetPos(); }
	bool SetReadPos( QWORD n ) { return io.ior.SetPos( n ); }
	QWORD GetReadTotalBytes() const { return io.ior.GetTotalBytes(); }
	void ReadAddTotalBytes( QWORD n ) { io.ior.AddTotalBytes( n ); }
	bool ReadN( void* p, DWORD n ) { return io.ior.ReadN( p, n ); }

	bool WriteOpenExisting( const _tstring& sFilename ) { return io.iow.OpenExisting( sFilename ); }
	bool WriteOpenCreate( const _tstring& sFilename ) { return io.iow.OpenCreate( sFilename ); }
	bool WriteClose() { return io.iow.Close(); }
	bool WriteSeek( __int64 n, bool bCount = true ) { return io.iow.Seek( n, bCount ); }
	bool WriteBytes( const void* p, DWORD n, bool bCount = true ) { return io.iow.Write( p, n, false, bCount ); }
	bool WriteExtendNull( DWORD nSize ) { return io.iow.ExtendNull( nSize ); }
	bool WritePreallocate( DWORD nSize ) { return io.iow.Preallocate( nSize ); }
	bool WriteSeekRead( QWORD nPos, DWORD nSize ) { return io.WriteSeekRead(nPos, nSize); }
	bool WriteExtendAlign( DWORD& nSize ) { if ( nSize & 0x1 ) { if ( !WriteExtendNull( 1 ) ) return false; ++nSize; } return true; };
	bool WriteString( const std::string& s, bool bCount = true ) { return io.iow.Write( s.c_str(), (DWORD)s.length(), false, bCount ); }
	QWORD GetWriteTotalBytes() const { return io.iow.GetTotalBytes(); }
	QWORD GetWritePos() const { return io.iow.GetPos(); }
	bool SetWritePos( QWORD n ) { return io.iow.SetPos( n ); }
	bool SetWritePosStart() { return io.iow.SetPosStart(); }
	bool SetWritePosEnd() { return io.iow.SetPosEnd(); }

	inline DWORD Align( DWORD& n ) const { if (io.m_bAligned ) n += (n & 0x01); return n; }
	inline DWORD Aligned( DWORD n ) const { return (io.m_bAligned ? n + (n & 0x01) : n); }

	
#ifdef _UNICODE
	static void HandleUnicodeError( _tostream& o );
#else
	static void HandleUnicodeError( const _tostream& ) {}
#endif

protected:
	class Settings& Settings;

protected: //private:
	class CIOService& io;
};

