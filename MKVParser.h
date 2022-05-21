#pragma once

#include "Parser.h"

class CMKVParser : public CParser
{
public:
	CMKVParser(class CIOService& ioservice);

	static bool Probe( BYTE buffer[12] );

protected:
	virtual bool ReadFile(int hti) override;
	virtual bool ReadChunk(int hti, QWORD nChunkSize) override;
	//bool ReadMKVChunk(int hti, DWORD nChunkSize, WORD nVersion);
	bool ReadEBMLHeader(int hti, QWORD nChunkSize);
	bool ReadEBMLSegment(int hti, QWORD nChunkSize);
	bool ReadEBMLSeek(int hti, QWORD nChunkSize);
	bool ReadEBMLSegmentInfo(int hti, QWORD nChunkSize);
	bool ReadEBMLTracks(int hti, QWORD nChunkSize);
	bool ReadEBMLTags(int hti, QWORD nChunkSize);
	bool ReadEBMLChapters(int hti, QWORD nChunkSize);
	bool ReadEBMLAttachments(int hti, QWORD nChunkSize);
	bool ReadEBMLCues(int hti, QWORD nChunkSize);
	bool ReadEBMLCluster(int hti, QWORD nChunkSize);
	bool ReadEBMLCommon(int hti, DWORD nID, WORD nLength, QWORD nChunkSize);

	virtual DWORD ParserCallback( const std::string& sGuid, DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const override;

	bool GetSize( int hti, QWORD& nSize, WORD& nLength );
	bool GetID(int hti, DWORD& nID, WORD& nLength);
	bool GetIDSize(int hti, DWORD& nID, QWORD& nSize, WORD& nLength) { return GetID(hti, nID, nLength) && GetSize(hti, nSize, nLength); }
	QWORD GetInt(int hti, WORD n);

public:
	static const WORD m_nMinHeaderSize;
};
