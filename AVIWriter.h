#pragma once

#include "IOService.h"
#include "AVIParser.h"

class CAVIWriter : public CIOServiceHelper
{
public:
	CAVIWriter( class CIOService& ioservice, CAVIParser::AVIFileHeader* pAVIDesc );
	~CAVIWriter();

	bool WriteAVIFile( int hti, const _tstring& sOFilename );

protected:
	bool Save( int hti );
	DWORD CalcStreamHeaderSize( WORD nStream ) const;
	bool WriteHeader_Phase1( int hti );
	bool WriteHeader_Phase2( int hti );
	int  WriteChunkMovi( int hti, WORD nSegmentCount ); // -1 Fehler, 0 fertig, 1 nächster Chunk
	bool WriteChunkIdx1( int hti, DWORD nvFrameEntryStartPos, QWORD nWriteFileMoviPos );
	bool WriteStandardIndex( int hti, FOURCC fcc, WORD nCount, DWORD nFrameEntryStartPos, QWORD nPosIndx, QWORD nPosMovi, bool bDump, bool bTight );
	bool WriteMetaIndex( int hti, WORD nStream, bool bDump );

	bool WriteType( FOURCC fcc, bool /*bDump*/ = true, bool bCount = true ) { return WriteBytes( &fcc, sizeof( fcc ), bCount ); }
	bool WriteSize( DWORD nSize, bool /*bDump*/ = true, bool bCount = true ) { return WriteBytes( &nSize, sizeof( nSize ), bCount ); }

protected:
	QWORD m_nWriteExtHeaderPos;
	DWORD m_nWriteHeaderTotalSize, m_nWriteHeaderSize;
	DWORD m_nWriteWriteTotalFrames; // Counter for total number of frames (effectively written may be less due to -x)
	DWORD m_nWriteReadTotalFrames;
	DWORD m_nSegment1Size;
	DWORD m_nvFrameEntryPos;
	QWORD m_nSizePayload;
	_tstring m_sOFilename;

	CAVIParser::AVIFileHeader* m_pAVIDesc;

#if 0
	static bool CreateEmptyFile( int hti, QWORD nSize );
	static DWORD WINAPI ThreadProc( win::LPVOID lpParameter );
	static struct ThreadInfo {
		QWORD nSize;
	} ThreadInfo;
#endif
};
