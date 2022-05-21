#pragma once

#include "RIFFParser.h"

class CRIFFSpecParser : public CRIFFParser
{
protected:

#pragma pack(push,1)

	// **************** WAVE ****************************

	struct RIFFMidiMthdChunk { // http://www.borg.com/~jglatt/tech/midifile/mthd.htm
		WORD Format;
		enum {
			MIDI_TYPE_0 = 0,
			MIDI_TYPE_1 = 1,
			MIDI_TYPE_2 = 2
		};
		WORD NumTracks; // Format 0 --> 1
		union {
		WORD wDivision; // PPQN (Pulses Per Quarter Note)
		BYTE bDivision[2]; // [0] negativ oder positiv ...
		};
	};

#pragma pack(pop)

public:
	CRIFFSpecParser( class CIOService& ioservice);

	static bool Probe( BYTE buffer[12] );

protected:
		_tstring m_sType;

protected:
	virtual bool ReadFile(int hti) override;
	//virtual bool ReadChunk(int hti, QWORD nChunkSize) override;

	ERET RIFFReadRiffChunk( int hti, DWORD nChunkSize, FOURCC fcc, WORD& nSegmentCount );

	ERET RIFFReadWaveChunk( int hti, DWORD nChunkSize, WORD nSegmentCount );
	ERET RIFFReadWaveAdtlChunk( int hti, DWORD nChunkSize );
	ERET RIFFReadWaveWavlChunk( int hti, DWORD nChunkSize );

	ERET RIFFReadAiffChunk( int hti, DWORD nChunkSize, WORD nSegmentCount );
	ERET RIFFReadMidiChunk( int hti, DWORD nChunkSize, WORD nSegmentCount );

	bool RIFFDumpAiffApplChunk( int hti, DWORD nSize );
	bool RIFFDumpAiffFverChunk( int hti, DWORD nSize );
	bool RIFFDumpAiffCommChunk( int hti, DWORD nSize );

	bool RIFFDumpWaveSmplChunk( int hti, DWORD nSize );
	bool RIFFDumpWavePlstChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveInstChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveCue_Chunk( int hti, DWORD nSize );
	bool RIFFDumpWaveLtxtChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveLablChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveNoteChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveWsmpChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveFileChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveWavhChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveMarkChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveCartChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveBextChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveFactChunk( int hti, DWORD nSize );
	bool RIFFDumpWaveSlntChunk( int hti, DWORD nSize );

	bool RIFFDumpMidiMthdChunk( int hti, DWORD nSize );

	void RIFFDumpAvi_StrfAudsHeader( int hti, const WAVEFORMATEX* wf, DWORD nSize ) const;
};
