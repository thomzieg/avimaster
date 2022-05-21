#pragma once

#include "RIFFParser.h"

class CRMFParser : public CRIFFParser
{
public:
	CRMFParser( class CIOService& ioservice);

	static bool Probe( BYTE buffer[12] );

protected:
	virtual bool ReadFile(int hti) override;
	virtual bool ReadChunk(int hti, QWORD nChunkSize) override;
	bool ReadHrmfChunk( int hti, DWORD nChunkSize, WORD nVersion );
	bool ReadPropChunk( int hti, DWORD nChunkSize );
	bool ReadMdprChunk( int hti, DWORD nChunkSize );
	bool ReadContChunk( int hti, DWORD nChunkSize );
	bool ReadDataChunk( int hti, DWORD nChunkSize );
	bool ReadIndxChunk( int hti, DWORD nChunkSize );
	bool ReadRmmdChunk( int hti, DWORD nChunkSize );
	bool ReadRmmdPropertiesChunk( int hti, DWORD nChunkSize );

	bool ReadTypeSizeVersion( FOURCC& fcc, DWORD& nSize, WORD& nVersion, int hti );

	virtual DWORD ParserCallback( const std::string& sGuid, DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const override;

public:
	static const WORD m_nMinHeaderSize;
};
