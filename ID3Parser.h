#pragma once

#include "Parser.h"

class CID3Parser : 	public CParser
{
public:
	CID3Parser(class CIOService& ioservice);
	~CID3Parser();

	//virtual bool ReadFile(int hti) override;
	virtual bool ReadChunk(int hti, QWORD nChunkSize) override;
	static bool Probe(BYTE buffer[12]);

	QWORD GetSize() const { return m_nSize; }
protected:

	DWORD GetSyncSize( int hti );
	QWORD m_nSize;
};

