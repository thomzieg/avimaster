#pragma once

#include "RIFFParser.h"

class CMOVParser : public CRIFFParser
{
public:
	CMOVParser( class CIOService& ioservice);

	static bool Probe( BYTE buffer[12] );

protected:
	virtual bool ReadFile( int hti) override;
	virtual bool ReadChunk( int hti, QWORD nChunkSize ) override;
	bool ReadMoovChunk( int hti, DWORD nParentSize );
	bool ReadMdatChunk( int hti, QWORD nParentSize );
	bool ReadTrakChunk( int hti, DWORD nParentSize );
	bool ReadMdiaChunk( int hti, DWORD nParentSize );
	bool ReadMinfChunk( int hti, DWORD nParentSize );
	bool ReadStblChunk( int hti, DWORD nParentSize );
	bool ReadUdtaChunk( int hti, DWORD nParentSize );
	bool ReadImapChunk( int hti, DWORD nParentSize );
	bool ReadGmhdChunk( int hti, DWORD nParentSize );

	bool ReadFtypChunk( int hti, DWORD nChunkSize );
	bool ReadMvhdChunk( int hti, DWORD nChunkSize );
	bool ReadStsdChunk( int hti, DWORD nChunkSize );
	bool ReadTkhdChunk( int hti, DWORD nChunkSize );
	bool ReadMdhdChunk( int hti, DWORD nChunkSize );
	bool ReadHdlrChunk( int hti, DWORD nChunkSize );
	bool ReadEdtsChunk( int hti, DWORD nChunkSize );
	bool ReadVmhdChunk( int hti, DWORD nChunkSize );
	bool ReadSmhdChunk( int hti, DWORD nChunkSize );
	bool ReadDinfChunk( int hti, DWORD nChunkSize );
	bool ReadIodsChunk( int hti, DWORD nChunkSize );
	bool ReadGminChunk( int hti, DWORD nChunkSize );

	bool ReadMetaChunk(int hti, DWORD nChunkSize);
	bool ReadMetaHdlrChunk(int hti, DWORD nChunkSize);
	bool ReadMetaIlstChunk(int hti, DWORD nChunkSize);

	bool PeekVersion( BYTE& nVersion );

	bool m_bMP4;

public:
	static const WORD m_nMinHeaderSize;
};
