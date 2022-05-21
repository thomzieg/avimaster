#pragma once

#include "Parser.h"
#include "Bitstream.h"

class CMPGParser : public CParser
{
public:
	CMPGParser( class CIOService& ioservice);
	virtual ~CMPGParser();

	void Parse( const _tstring& sIFilename );
	virtual bool ReadFile(int hti) override;
	virtual bool ReadChunk(int hti, QWORD nChunkSize) override;
	static bool Probe( BYTE buffer[12] );
	bool ParseMuxFrame( int hti, DWORD& nSize );

protected:
	enum class MPEG_TYPE {
		MT_UNKNOWN,
		MT_ESV,
		MT_ESA,
		MT_PES1,
		MT_PES2,
		MT_PS1,
		MT_PS2,
		MT_TS,
		MT_TS_M2TS,
		MT_TS_DVB,
		MT_TS_ATSC,
		MT_AUDIO,
	};
	MPEG_TYPE m_eMPEG_TYPE;
	bool ProbeTransportPaket(int hti, enum MPEG_TYPE eType);

	class CMPGESParser
	{
	public:
		CMPGESParser( CMPGParser* pParser ) : Bitstream( pParser->Bitstream ) { m_pParser = pParser; nCountPackets = 0; nCountChunks = 0; pBuffer = nullptr; nBufferSize = 0; m_nIFrameSeen = 0; m_nGOPSize = 0; }
		virtual ~CMPGESParser() { delete pBuffer; }

		DWORD nCountPackets, nCountChunks;

		virtual bool Parse(int hti, BYTE nStreamId, DWORD nChunkSize, bool bChunk) = 0;
		BYTE* pBuffer;
		DWORD nBufferSize;
		_tstring m_sGOP; WORD m_nGOPSize; int m_nIFrameSeen;
		CMPGParser* m_pParser;
		class Bitstream& Bitstream;
	};

	class CMPGESVideoParser : public CMPGESParser
	{
	public:
		CMPGESVideoParser( CMPGParser* pParser ) : CMPGESParser( pParser ) {};
		virtual ~CMPGESVideoParser() {};

		virtual bool Parse( int hti, BYTE nStreamId, DWORD nChunkSize, bool bChunk ) override;
	};

	class CMPGESAudioParser : public CMPGESParser
	{
	public:
		CMPGESAudioParser( CMPGParser* pParser ) : CMPGESParser( pParser ) {};
		virtual ~CMPGESAudioParser() {};

		virtual bool Parse( int hti, BYTE nStreamId, DWORD nChunkSize, bool bChunk ) override;
		bool ParseAudioFrame( int hti, BYTE nStreamId, DWORD& nSize );

	protected:

	#pragma pack(push,1)
		struct MPEGAUDIOFRAMEHEADER {
			DWORD frame_sync1 : 8;

			DWORD protection_bit : 1;
			DWORD layer_description : 2; // TODO enum
			DWORD audio_version : 2; // TODO enum
			DWORD frame_sync2 : 3;

			DWORD private_bit : 1;
			DWORD padding_bit : 1;
			DWORD sampling_rate_frequency_index : 2;
			DWORD bitrate_index : 4;

			DWORD emphasis : 2; // TODO enum
			DWORD original : 1;
			DWORD copyright : 1;
			DWORD mode_extension : 2; // TODO enum
			DWORD channel_mode : 2; // TODO enum
		};

		struct MPEGAUDIOTAGID3V1 {
			char tag[3]; // "GAT"
			char title[30];
			char artist[30];
			char album[30];
			char year[4];
			char comment[30];
			BYTE genre[1];
		};

		struct MPEGAUDIOTAGID3V11 {
			char tag[3]; // "GAT"
			char title[30];
			char artist[30];
			char album[30];
			char year[4];
			char comment[29];
			BYTE trackno;
			BYTE genre;
		};

	#pragma pack(pop)
	};

	class CMPGESGenericParser : public CMPGESParser
	{
	public:
		CMPGESGenericParser( CMPGParser* pParser ) : CMPGESParser( pParser ) {};
		virtual ~CMPGESGenericParser() {};

		virtual bool Parse(int hti, BYTE /*nStreamId*/, DWORD nChunkSize, bool /*bChunk*/) override { ++nCountPackets; ++nCountChunks; if (m_pParser->Settings.nVerbosity > 4) { BYTE* p = new BYTE[nChunkSize]; if (!m_pParser->Bitstream.GetBytes(p, nChunkSize)) { delete [] p; return false; } m_pParser->DumpBytes(p, nChunkSize, hti); delete [] p; return true; } else return m_pParser->Bitstream.SkipBytes(nChunkSize); }
	};

	bool ParseTransportDump( int hti );
	bool SkipPacket( int hti, _tstring sTitle, BYTE bType, bool bHasExtension = false );
	WORD DumpPESExtension( int hti );
	bool ReadPSTimestamp( int hti, bool bWithExtension, QWORD& r ); // ProgramStream
	bool ReadTSTimestamp( int hti, QWORD& r ); // Transport Stream
	bool ReadTSTimestampMarker( int hti, QWORD& r ); 

	class ParserFactory {
	public:
		ParserFactory( CMPGParser* pParser ) { m_pParser = pParser; m_vESParser.resize( 256, nullptr ); }
		~ParserFactory() { for ( size_t i = 0; i < m_vESParser.size(); ++i ) delete m_vESParser[i]; }
		CMPGESParser* GetParser( BYTE nStreamId );
		void Statistics( int hti ) const;
	protected:
		typedef std::vector<CMPGESParser*> VESPARSER;
		VESPARSER m_vESParser;
		CMPGParser* m_pParser;
	} ParserFactory;

	Bitstream Bitstream;

public:
	static const WORD m_nMinHeaderSize;
};
