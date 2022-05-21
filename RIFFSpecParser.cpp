#include "RIFFSpecParser.h"
#include "AVIParser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

// TODO AIFF PSTRING: Byte len, char string Gesamtlänge ist even, ggf folgt padding byte
// Chunk Size z.B. = 9, wenn letzter Chunk im File endet das file ohne Padding hier und die Länge des Chunks (Aligned) ist falsch

CRIFFSpecParser::CRIFFSpecParser( class CIOService& ioservice) : CRIFFParser( ioservice )
{
	m_sType = _T("(Unknown)");
}


CRIFFParser::ERET CRIFFSpecParser::RIFFReadRiffChunk( int hti, DWORD nChunkSize, FOURCC fcc, WORD& nSegmentCount )
{ 
FOURCC fcc2;
int htitmp;
_tstringstream s;
	
	if ( nChunkSize < 4 )
		return CRIFFParser::ERET::ER_EOF;
	if ( !ReadType( fcc2, hti ) )
		return CRIFFParser::ERET::ER_EOF;

	++nSegmentCount;

	switch( fcc2 ) {
	case FCC('WAVE'):
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF WAVE Chunk"), fcc, nChunkSize + 8, fcc2 );
		m_sType = _T(".WAV (Wave)");
		return RIFFReadWaveChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('RMID'):
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Midi Chunk"), fcc, nChunkSize + 8, fcc2 );
		m_sType = _T(".MID (Midi)");
		return RIFFReadMidiChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('AIFF'):
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("Audio Interchange File Format Chunk"), fcc, nChunkSize + 8, fcc2 );
		m_sType = _T("AIFF (Audio Interchange)");
		return RIFFReadAiffChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('AIFC'):
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("Audio Interchange File Format - Compressed Chunk"), fcc, nChunkSize + 8, fcc2 );
		m_sType = _T("AIFC (Audio Interchange Compressed)");
		return RIFFReadAiffChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('8SVX'):
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("Audio 8-bit Sampled Sound Format Chunk"), fcc, nChunkSize + 8, fcc2 );
		m_sType = _T("8SVX (8-Bit Sampled Sound)");
		return RIFFReadAiffChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('    '): 
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("Interchange File Format Bag Chunk"), fcc, nChunkSize + 8, fcc2 );
		return RIFFReadAiffChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('FORC'): // MS Force Feedback Direct Input, seltsames Format...
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF MS DirectInput Force Feedback Effect Chunk"), fcc, nChunkSize + 8, fcc2 );
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_FATAL, _T("This is not a standard RIFF File Format!") );
		if ( !ReadSeekFwdDump( 16, htitmp ) ) // Skip strange bytes ???
			return CRIFFParser::ERET::ER_EOF;
		return RIFFReadGenericChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	default:
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Unknown Chunk"), fcc, nChunkSize + 8, fcc2 );
		return RIFFReadGenericChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	}
}



CRIFFParser::ERET CRIFFSpecParser::RIFFReadWaveChunk( int hti, DWORD nParentSize, WORD nSegment )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
_tstring s;
BYTE* p;
	
	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( Aligned( nSize ) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('LIST'):
			nSize -= 4 /*fcc*/;
			if ( !ReadType( fcc, hti ) ) 
				return CRIFFParser::ERET::ER_EOF;

			switch ( fcc ) {
			case FCC('wavl'): // WAVData List
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("WAVE Data List"), FCC('LIST'), nSize + 12, fcc );
				if ( RIFFReadWaveWavlChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				break;
			case FCC('adtl'): // Associated Data List
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("WAVE Associated Data List"), FCC('LIST'), nSize + 12, fcc );
				if ( RIFFReadWaveAdtlChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				break;
			case FCC('INFO'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Info List"), FCC('LIST'), nSize + 12, fcc );
				if ( RIFFReadInfoListChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				break;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T(""), FCC('LIST'), nSize + 12, fcc );
				if ( fcc == FCC('RIFF') ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					return CRIFFParser::ERET::ER_TOKEN_RIFF;
				}
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				if ( RIFFReadGenericChunk( htitmp, nSize, nSegment ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				if ( !ReadAlign( nSize ) )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
				break;
			}
			break;
		case FCC('fmt '): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("WAVE StreamFormat Chunk"), fcc, nSize + 8 );
			p = new BYTE[nSize + 1];
			if ( !ReadBytes( p, Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			RIFFDumpAvi_StrfAudsHeader( htitmp, reinterpret_cast<const WAVEFORMATEX*>( p ), nSize );
			delete[] p;
			break; 
		case FCC('wavh'): // Wave Header
			if ( !RIFFDumpWaveWavhChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('cue '): // Cue Chunk 
			if ( !RIFFDumpWaveCue_Chunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('plst'): // Playlist Chunk 
			if ( !RIFFDumpWavePlstChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('smpl'): // Sampler Chunk 
			if ( !RIFFDumpWaveSmplChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('inst'): // Instrument Chunk 
			if ( !RIFFDumpWaveInstChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('wsmp'): // wsmp 
			if ( !RIFFDumpWaveWsmpChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('wavu'): // ???
		case FCC('alia'): // ???
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("WAVE ??? Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('mark'): // Marker
			if ( !RIFFDumpWaveMarkChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('data'): // Data Chunk
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("WAVE Data Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), (Settings.nVerbosity >= 5 ? htitmp : 0) ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break;
		case FCC('bext'): // EBU BWF bext
			if ( !RIFFDumpWaveBextChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('mpeg'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("WAVE EBU MPEG-extension data Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( nSize, htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break;
		case FCC('cart'): // EBU BWF cart
			if ( !RIFFDumpWaveCartChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('fact'): // FACT Chunk
			if ( !RIFFDumpWaveFactChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		default:
			switch( RIFFReadHandleCommonChunks( hti, nSize, fcc ) ) {
			case CRIFFParser::ERET::ER_OK:
				break;
			case CRIFFParser::ERET::ER_EOF:
				return CRIFFParser::ERET::ER_EOF;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
			}
			break;
		}
	}		
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}


CRIFFParser::ERET CRIFFSpecParser::RIFFReadWaveAdtlChunk( int hti, DWORD nParentSize )
{
FOURCC fcc;
DWORD nSize;

	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( Aligned( nSize ) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('note'): // Note Chunk 
			if ( !RIFFDumpWaveNoteChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('labl'): // Label Chunk 
			if ( !RIFFDumpWaveLablChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('ltxt'): // LabelText Chunk 
			if ( !RIFFDumpWaveLtxtChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('file'): // File Chunk 
			if ( !RIFFDumpWaveFileChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		default:
			int htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}

CRIFFParser::ERET CRIFFSpecParser::RIFFReadWaveWavlChunk( int hti, DWORD nParentSize )
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( Aligned( nSize ) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('data'): // Data Chunk
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("WAVE Data Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( nSize /*+ (nSize & 0x01)*/, (Settings.nVerbosity >= 5 ? htitmp : 0) ) )
				return CRIFFParser::ERET::ER_EOF;
			break;
		case FCC('slnt'): // Silence Chunk 
			if ( !RIFFDumpWaveSlntChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( !ReadSeekFwdDump( nSize /*+ (nSize & 0x01)*/, htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}


CRIFFParser::ERET CRIFFSpecParser::RIFFReadAiffChunk( int hti, DWORD nParentSize, WORD nSegmentCount )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
	
	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( Aligned( nSize ) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('FORM'): // IFF 
			if ( RIFFReadRiffChunk( hti, nSize, fcc, nSegmentCount ) == CRIFFParser::ERET::ER_EOF ) // Recursion!!!
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('PROP'): // IFF 
		case FCC('CAT '): // IFF 
		case FCC('LIST'): {
			FOURCC lfcc = fcc;
			nSize -= 4 /*fcc*/;
			if ( !ReadType( fcc, hti ) ) 
				return CRIFFParser::ERET::ER_EOF;

			htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("IFF List"), lfcc, nSize + 12, fcc );
			if ( RIFFReadGenericChunk( htitmp, nSize, nSegmentCount ) == CRIFFParser::ERET::ER_EOF ) // Recursion!!!
				return CRIFFParser::ERET::ER_EOF;
//			if ( !ReadAlign( nSize ) ) ???
//				return CRIFFParser::ERET::ER_EOF;
			break; }
		case FCC('COMM'): 
			if ( !RIFFDumpAiffCommChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('SSND'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Sound Data Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('VHDR'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("8SVX Header Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('BODY'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("8SVX Body Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('MARK'):  
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Marker Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('INST'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Instrument Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('MIDI'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Midi Data Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('AESD'):  
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Audio Recording Chunk"), fcc, nSize + 12 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('CHAN'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Channel Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('COMT'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AIFF Comment Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('APPL'): 
			if ( !RIFFDumpAiffApplChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('FVER'): 
			if ( !RIFFDumpAiffFverChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('NAME'): 
			if ( !DumpGenericMBCSTextChunk( hti, nSize, FCC('NAME'), "AIFF Name Chunk" ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('AUTH'): 
			if ( !DumpGenericMBCSTextChunk( hti, nSize, FCC('AUTH'), "AIFF Author Chunk" ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('(c) '): 
			if ( !DumpGenericMBCSTextChunk( hti, nSize, FCC('(c) '), "AIFF Copyright Chunk" ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('ANNO'): 
			if ( !DumpGenericMBCSTextChunk( hti, nSize, FCC('ANNO'), "AIFF Annotation Chunk" ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		default:
			switch( RIFFReadHandleCommonChunks( hti, nSize, fcc ) ) {
			case CRIFFParser::ERET::ER_OK:
				break;
			case CRIFFParser::ERET::ER_EOF:
				return CRIFFParser::ERET::ER_EOF;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 12 );
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				if ( !ReadSeekFwdDump( Aligned( nSize ), htitmp ) )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
			}
			break;
		}
	}		
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}



bool CRIFFSpecParser::RIFFDumpWavePlstChunk( int hti, DWORD nSize )
{
#if 0
	struct RIFFWavePlstChunk { // PlaylistChunk
		DWORD dwSegments;
  		struct Segment {
			DWORD dwIdentifier;
			DWORD dwLength;
			DWORD dwRepeats;
		} Segments[];
	};
#endif
	static constexpr DDEF ddef = {
		5, "WAVE Playlist Chunk", "'plst'", 4, 0,
		{
			DDEF::DESC::DT_DWORD, 0, 2, "Segments", nullptr, nullptr,
			DDEF::DESC::DT_NODELOOP, -1, 3, "'plst' Entries", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Length", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Repeats", nullptr, nullptr,
			//		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveSmplChunk( int hti, DWORD nSize )
{
#if 0
	struct RIFFWaveSmplChunk { // SamplerChunk
		DWORD dwManufacturer;
		DWORD dwProduct;
		DWORD dwSamplePeriod;
		DWORD dwMIDIUnityNote;
		DWORD dwMIDIPitchFraction;
		DWORD dwSMPTEFormat;
		DWORD dwSMPTEOffset;
		DWORD cSampleLoops;
		DWORD cbSamplerData;
		struct SampleLoop {
			DWORD dwIdentifier;
			DWORD dwType;
			enum {
				SAMPLELOOP_TYPE_FORWARD = 0, // Loop forward (normal) 
				SAMPLELOOP_TYPE_ALTERNATING = 1, // Alternating loop (forward/backward) 
				SAMPLELOOP_TYPE_BACKWARD = 2 // Loop backward 
			};
			DWORD dwStart;
			DWORD dwEnd;
			DWORD dwFraction;
			DWORD dwPlayCount;
		} Loops[];
		BYTE Data[];
	};
#endif
	static constexpr DDEFV ddefv1 = {
		3, DDEFV::VDT_INFIX,
		{0, "Foward", DDEFV::DESC::VDTV_VALUE,
		1, "Alternating", DDEFV::DESC::VDTV_VALUE,
		2, "Backward", DDEFV::DESC::VDTV_VALUE,}
	};
	static constexpr DDEF ddef = {
		18, "WAVE Sampler Chunk", "'smpl'", 36, 0,
		{ 
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 2, "Manufacturer (www.midi.org)", nullptr, nullptr, // http://www.midi.org/about-mma/mfr_id.shtml
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Product", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "SamplePeriod", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "MIDIUnityNote", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "MIDIPitchFraction", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SMPTEFormat", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SMPTEOffset", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SampleLoops", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SamplerData", nullptr, nullptr,
			DDEF::DESC::DT_NODELOOP, -2, 3, "'smpl' Entries", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Type", &ddefv1, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Start", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "End", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Fraction", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PlayCount", nullptr, nullptr,
			DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, -9, 3, "Data", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveInstChunk( int hti, DWORD nSize )
{
#if 0
	struct RIFFWaveInstChunk { // InstrumentChunk
		BYTE UnshiftedNote;
		char FineTune;
		char Gain;
		BYTE LowNote;
		BYTE HighNote;
		BYTE LowVelocity;
		BYTE HighVelocity;
	};
#endif
	static constexpr DDEF ddef = {
		8, "WAVE Instrument Chunk", "'inst'", 7, 7,
		{
			DDEF::DESC::DT_BYTE, 0, 2, "UnshiftedNote", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, 0, 2, "FineTune", nullptr, nullptr, // TODO signed
			DDEF::DESC::DT_BYTE, 0, 2, "Gain", nullptr, nullptr, // TODO signed
			DDEF::DESC::DT_BYTE, 0, 2, "LowNote", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, 0, 2, "HighNote", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, 0, 2, "LowVelocity", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, 0, 2, "HighVelocity", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveCue_Chunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveCue_Chunk { // CueChunk
		DWORD dwCuePoints;
		struct CuePoint {
			DWORD dwIdentifier;
			DWORD dwPosition;
			FOURCC fccChunk;
			DWORD dwChunkStart;
			DWORD dwBlockStart;
			DWORD dwSampleOffset;
		} points[];
	};
#endif
	static constexpr DDEF ddef = {
		8, "WAVE Cue Chunk", "'cue '", 4, 0,
		{
			DDEF::DESC::DT_DWORD, 0, 3, "CuePoints", nullptr, nullptr,
			DDEF::DESC::DT_NODELOOP, -1, 3, "'cue ' Entries", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Position", nullptr, nullptr,
			DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "fccChunk", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "ChunkStart", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "BlockStart", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SampleOffset", nullptr, nullptr,
			//		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveLtxtChunk( int hti, DWORD nSize )
{
#if 0
	struct RIFFWaveLtxtChunk { // LabelTextChunk
		DWORD dwIdentifier;
		DWORD dwSampleLength;
		DWORD dwPurpose;
		WORD wCountry;
		WORD wLanguage;
		WORD wDialect;
		WORD wCodePage;
		char dwText[];
	};
#endif
	static constexpr DDEF ddef = {
		8, "WAVE LabelText Chunk", "'ltxt'", 21, 0,
		{
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SampleLength", nullptr, nullptr,
			DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "Purpose", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Country", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Language", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Dialect", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "CodePage", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 0, 3, "Text", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveLablChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveLablNoteChunks { // LabelNoteChunk
	DWORD dwIdentifier;
	char dwText[];
	};
#endif
	static constexpr DDEF ddef = {
		2, "WAVE Label Chunk", "'labl'", 5, 0,
		{
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 0, 3, "Text", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpAiffFverChunk( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 = {
		1, DDEFV::VDT_INFIX,
		{0xa2805140, "Standard", DDEFV::DESC::VDTV_VALUE,}
	};
	static constexpr DDEF ddef = {
		1, "AIFC File Version Chunk", "'fver'", 4, 4,
		{
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "FileVersion", &ddefv1, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpAiffApplChunk( int hti, DWORD nSize ) 
{
	static constexpr DDEF ddef = {
		1, "WAVE Fact Chunk", "'fact'", 4, 4,
		{
			DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "ApplicationSignature", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveSlntChunk( int hti, DWORD nSize ) 
{
	static constexpr DDEF ddef = {
		1, "WAVE Silence Chunk", "'slnt'", 4, 0,
		{
			DDEF::DESC::DT_DWORD, 0, 3, "Samples", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveFactChunk( int hti, DWORD nSize ) 
{
	static constexpr DDEF ddef = {
		1, "WAVE Fact Chunk", "'fact'", 4, 4,
		{
			DDEF::DESC::DT_DWORD, 0, 3, "FileSize", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveNoteChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveLablNoteChunks { // LabelNoteChunk
	DWORD dwIdentifier;
	char dwText[];
	};
#endif
	static constexpr DDEF ddef = {
		2, "WAVE Note Chunk", "'note'", 5, 0,
		{
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 0, 3, "Text", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveWsmpChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveWsmpChunk { // WSMPLChunk, http://safariexamples.informit.com/0672325969/DirectX%20SDK/Include/dls1.h
		DWORD cbSize;
		WORD usUnityNote; /* MIDI Unity Playback Note */ 
		SHORT sFineTune; /* Fine Tune in log tuning */ 
		LONG lAttenuation; /* Overall Attenuation to be applied to data */ 
		DWORD fulOptions; /* Flag options */ 
		enum {
			F_WSMP_NO_TRUNCATION = 0x0001l, 
			F_WSMP_NO_COMPRESSION = 0x0002l 
		};
		DWORD cSampleLoops; /* Count of Sample loops, 0 loops is one shot */ 
		struct WLOOP {
			DWORD cbSize;
			DWORD ulType;              /* Loop Type */
			enum {
				WLOOP_TYPE_FORWARD = 0 // Not more...
			};
			DWORD ulStart;             /* Start of loop in samples */
			DWORD ulLength;            /* Length of loop in samples */
		} Loops[];
	}; 

#endif
	static constexpr DDEFV ddefv1 = {
		2, DDEFV::VDT_INFIX,
		{1, "No Truncation", DDEFV::DESC::VDTV_VALUE,
		2, "No Compression", DDEFV::DESC::VDTV_VALUE,}
	};
	static constexpr DDEFV ddefv2 = {
		1, DDEFV::VDT_INFIX,
		{0, "Forward", DDEFV::DESC::VDTV_VALUE,}
	};
	static constexpr DDEF ddef = {
		11, "WAVE WSMPL Chunk", "'wsmp'", 20, 0,
		{
			DDEF::DESC::DT_DWORD, 0, 4, "Size", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "UnityNote", nullptr, nullptr,
			DDEF::DESC::DT_SHORT, 0, 3, "FineTune", nullptr, nullptr,
			DDEF::DESC::DT_LONG, 0, 3, "Attenuation", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Options", &ddefv1, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SampleLoops", nullptr, nullptr,
			DDEF::DESC::DT_NODELOOP, -2, 3, "'wsmp' Entries", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Type", &ddefv2, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Start", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Length", nullptr, nullptr,
			//		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveFileChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveFileChunk { // FileChunk
		DWORD dwIdentifier;
		DWORD dwMedType;
		BYTE fileData[];
	};
#endif
	static constexpr DDEF ddef = {
		3, "WAVE File Chunk", "'file'", 8, 0,
		{
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Identifier", nullptr, nullptr,
			DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "MedType", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 0, 3, "FileData", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveMarkChunk( int hti, DWORD nSize )
{
#if 0
	struct RIFFWaveMarkChunk { // MarkerChunk
		WORD  numMarkers;
		struct Marker {
			WORD id;
			DWORD  position;
			BYTE length; // Ein Pascal-String, so ein Schwachsinn??? 
			char markerName[1]; // padded to even length
		} Markers[1]; // size unknown
	};
#endif
	static constexpr DDEF ddef = {
		6, "WAVE Marker Chunk", "'mark'", 2, 0,
		{
			DDEF::DESC::DT_WORD, 0, 3, "NumMarkers", nullptr, nullptr,
			DDEF::DESC::DT_NODELOOP, -1, 3, "'mark' Entries", nullptr, nullptr,
			DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "Id", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Position", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, 0, 4, "MarkerNameLength", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, -1, 3, "MarkerName", nullptr, nullptr,
			//		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveWavhChunk( int hti, DWORD nSize )
{
#if 0
	typedef LONGLONG REFERENCE_TIME;
	struct DMUS_IO_WAVE_HEADER {
		REFERENCE_TIME rtReadAhead;
		DWORD     dwFlags;
		enum {
			DMUS_WAVEF_NOPREROLL = 0, // Preroll data is not downloaded with the wave. 
			DMUS_WAVEF_STREAMING = 1 // The wave is streamed.
		};
	};
#endif
	static constexpr DDEFV ddefv1 = {
		2, DDEFV::VDT_INFIX,
		{0, "DMUS_WAVEF_NOPREROLL", DDEFV::DESC::VDTV_VALUE,
		1, "DMUS_WAVEF_STREAMING", DDEFV::DESC::VDTV_VALUE,}
	};
	static constexpr DDEF ddef = {
		2, "WAVE DirectMusic Header", "'wavh'", 12, 12,
		{
			DDEF::DESC::DT_QWORD, 0, 3, "ReadAhead", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Flags", &ddefv1, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveBextChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveBextChunk { // EBU BEXT Chunk
		char Description[256]; /* ASCII : «Description of the sound sequence»*/
		char Originator[32]; /* ASCII : «Name of the originator»*/
		char OriginatorReference[32]; /* ASCII : «Reference of the originator»*/
		char OriginationDate[10]; /* ASCII : «yyyy:mm:dd» */
		char OriginationTime[8]; /* ASCII : «hh:mm:ss» */
		DWORD TimeReferenceLow; /*First sample count since midnight, low word */
		DWORD TimeReferenceHigh; /*First sample count since midnight, high word */
		WORD Version; /* Version of the BWF; unsigned binary number */
		char Reserved[254] ; /* Reserved for future use, set to “nullptr” */
		char CodingHistory[]; /* ASCII : « History coding » */
	};
#endif
	static constexpr DDEF ddef = {
		10, "WAVE EBU BWF Chunk", "'bext'", 620, 0,
		{
			DDEF::DESC::DT_MBCSSTRING, 256, 3, "Description", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 32, 3, "Originator", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 32, 3, "OriginatorReference", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 10, 3, "OriginationDate", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 8, 3, "OriginationTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "TimeReferenceLow", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "TimeReferenceHigh", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Version", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 254, 4, "Reserved", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, 0, 3, "CodingHistory", nullptr, nullptr,
		}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpWaveCartChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFWaveCartChunk { // EBU CART Extension http://www.cartchunk.org
		char Version[4]; // Version of the data structure
		char Title[64]; // ASCII title of cart audio sequence
		char Artist[64]; // ASCII artist or creator name
		char CutID[64]; // ASCII cut number identification
		char ClientID[64]; // ASCII client identification
		char Category[64]; // ASCII Category ID, PSA, NEWS, etc
		char Classification[64]; // ASCII Classification or auxiliary key
		char OutCue[64]; // ASCII out cue text
		char StartDate[10]; // ASCII YYYY-MM-DD
		char StartTime[8]; // ASCII hh:mm:ss
		char EndDate[10]; // ASCII YYYY-MM-DD
		char EndTime[8]; // ASCII hh:mm:ss
		char ProducerAppID[64]; // Name of vendor or application
		char ProducerAppVersion[64]; // Version of producer application
		char UserDef[64]; // User defined text
		DWORD dwLevelReference; // Sample value for 0 dB reference
		struct CART_TIMER // Post timer storage unit
		{
			FOURCC dwUsage; // FOURCC timer usage ID
			DWORD dwValue; // timer value in samples from head
		} PostTimer[8]; // 8 time markers after head
		char Reserved[276]; // Reserved for future expansion
		char URL[1024]; // Uniform resource locator
		char TagText[]; // Free form text for scripts or tags
	};
#endif
	static constexpr DDEF ddef = {
		24, "WAVE EBU Cart Information Chunk", "'cart'", 2112, 0,
		{DDEF::DESC::DT_WORD, 0, 3, "VersionMajor", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "VersionMinor", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "Title", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "Artist", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "Artist", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "CutID", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "Category", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "Classification", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "OutCut", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 10, 3, "StartDate", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 8, 3, "StartTime", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 10, 3, "EndDate", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 8, 3, "EndTime", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "ProducerAppID", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "ProducerAppVersion", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 64, 3, "UserDef", nullptr, nullptr,
		DDEF::DESC::DT_LONG, 0, 3, "LevelReference", nullptr, nullptr, // signed
		DDEF::DESC::DT_LOOP, 8, 3, "PostTimers", nullptr, nullptr,
		DDEF::DESC::DT_FCC, 0, 3, "Usage", nullptr, nullptr, // unbenutzt, wenn Usage oder Value == 0
		DDEF::DESC::DT_DWORD, 0, 3, "Value", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, 276, 4, "Reserved", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 1024, 3, "URL", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, 0, 3, "TagText", nullptr, nullptr,}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpAiffCommChunk( int hti, DWORD nSize ) 
{
#if 0
	struct RIFFAiffCommChunk { // CommonChunk
		WORD numChannels;
		DWORD numSampleFrames;
		WORD sampleSize;
		BYTE sampleRate[10]; // "extended" 10 Byte float
	};
#endif
	static constexpr DDEF ddef = {
		7, "AIFF Common Chunk", "'COMM'", 18, 0,
		{DDEF::DESC::DT_WORD, 0, 3, "Channels", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "SampleFrames", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "SampleSize", nullptr, nullptr,
		DDEF::DESC::DT_EXTENDED, 0, 3, "SampleRate", nullptr, nullptr,
		DDEF::DESC::DT_FCC, 0, 3, "CompressionType", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, 0, 4, "CompressionNameLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "CompressionName", nullptr, nullptr,}
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFSpecParser::RIFFDumpMidiMthdChunk( int hti, DWORD nSize )
{
	hti = OSNodeCreate( hti, GetReadPos() - 8, _T("Midi Header"), FCC('MThd'), nSize + 8 );
	BYTE* buffer = new BYTE[nSize];
	if ( !ReadBytes( buffer, nSize /*+ (nSize & 0x01)*/, hti ) ) 
		return false;
	const RIFFMidiMthdChunk* p = reinterpret_cast<const RIFFMidiMthdChunk*>( buffer );

_tstring s;
int htitmp;

	if ( nSize != sizeof( RIFFMidiMthdChunk ) )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Token Size!") );
	s = _T("00 Format: ") + ToString( FlipBytes( &p->Format ) ); 
	if ( FlipBytes( &p->Format ) == RIFFMidiMthdChunk::MIDI_TYPE_0 )
		s += _T(" Type: 1 Track/16 Midi Channels");
	else if ( FlipBytes( &p->Format ) == RIFFMidiMthdChunk::MIDI_TYPE_1 )
		s += _T(" Type: Simultaneous Multitrack");
	else if ( FlipBytes( &p->Format ) == RIFFMidiMthdChunk::MIDI_TYPE_2 )
		s += _T(" Type: Sequential Multitrack");
	else
		s += _T(" (undefined)");
	OSNodeCreateValue( hti, s );
	s = _T("02 Tracks: ") + ToString( FlipBytes( &p->NumTracks ) );
	htitmp = OSNodeCreateValue( hti, s );
	if ( FlipBytes( &p->Format ) == RIFFMidiMthdChunk::MIDI_TYPE_0 && FlipBytes( &p->NumTracks ) != 1 )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected number of Tracks for TYPE 0 Midi!") );
	if ( p->bDivision[0] >= 0x80 )
		s = _T("04 Division: ") + ToString( (WORD)p->bDivision[0] ) + _T(" frames per second/") + ToString( (WORD)p->bDivision[1] ) + _T(" subframes per frame");
	else 
		s = _T("04 Division: ") + ToString( FlipBytes( &p->wDivision ) ) + _T(" PPQN");
	OSNodeCreateValue( hti, s );
	delete[] p;
	return true;
}


// Midi-File ohne Header, ungerade Bytes werden NICHT ausgeglichen
CRIFFParser::ERET CRIFFSpecParser::RIFFReadMidiChunk( int hti, DWORD nParentSize, WORD nSegmentCount )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
	
	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( nSize /*+ (nSize & 0x01)*/ + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('data'): { // seen with 'RMID', the midi-data is network ordered packaged into 'data' chunk
			bool bAligned = IsAligned();
			bool bNetworkByteorder = IsNetworkByteorder();
			SetParams( false, true );
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RMID Data Chunk"), fcc, nSize + 8, fcc );
			if ( RIFFReadMidiChunk( htitmp, nSize, nSegmentCount ) == CRIFFParser::ERET::ER_EOF ) // Recursion!!!
				return CRIFFParser::ERET::ER_EOF;
			SetParams( bAligned, bNetworkByteorder );
			break; }
		case FCC('FORM'): // IFF 
			if ( RIFFReadRiffChunk( hti, nSize, fcc, nSegmentCount ) == CRIFFParser::ERET::ER_EOF ) // Recursion!!!
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('PROP'): // IFF 
		case FCC('CAT '): // IFF 
		case FCC('LIST'): {
			FOURCC lfcc = fcc;
			nSize -= 4 /*fcc*/;
			if ( !ReadType( fcc, hti ) ) 
				return CRIFFParser::ERET::ER_EOF;
			htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF List"), lfcc, nSize + 8, fcc );
			if ( RIFFReadGenericChunk( htitmp, nSize, nSegmentCount ) == CRIFFParser::ERET::ER_EOF ) // Recursion!!!
				return CRIFFParser::ERET::ER_EOF;
//			if ( !ReadAlign( nSize ) ) ???
//				return CRIFFParser::ERET::ER_EOF;
			break; }
		case FCC('MThd'):
			if ( !RIFFDumpMidiMthdChunk( hti, nSize ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('MTrk'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Midi Track Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( nSize /*+ (nSize & 0x01)*/, htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			break; 
		default:
			switch( RIFFReadHandleCommonChunks( hti, nSize, fcc ) ) {
			case CRIFFParser::ERET::ER_OK:
				break;
			case CRIFFParser::ERET::ER_EOF:
				return CRIFFParser::ERET::ER_EOF;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				if ( !ReadSeekFwdDump( nSize /*+ (nSize & 0x01)*/, htitmp ) )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
			}
			break;
		}
	}		
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}


/*static*/ bool CRIFFSpecParser::Probe( BYTE buffer[12] )
{
	FOURCC* fcc = reinterpret_cast<FOURCC*>( buffer ); 
	if ( fcc[0] == FCC('MThd') )
		return true;
	if ( fcc[0] != FCC('RIFF') && fcc[0] != FCC('FORM') && fcc[0] != FCC('FORC') 
			&& fcc[0] != FCC('LIST') && fcc[0] != FCC('CAT ') && fcc[0] != FCC('RIFX') )
		return false;
	if ( fcc[2] != FCC('WAVE') && fcc[2] != FCC('RMID') && fcc[2] != FCC('AIFF') && fcc[0] != FCC('FORC') 
		 && fcc[2] != FCC('AIFC') && fcc[2] != FCC('8SVX') && fcc[2] != FCC('    ') )
		return false;
	return true;
}

bool CRIFFSpecParser::ReadFile( int hti )
{ 
DWORD nSize;
DWORD fcc;
WORD nSegmentCount = 0;
int htitmp;
_tstringstream s;
bool bFlagRiff = false;

	while ( GetReadFileSize() > 8 && GetReadTotalBytes() < GetReadFileSize() - 8 ) { 
		OSStatsSet( GetReadPos() );
		if ( !ReadType( fcc, hti ) ) 
			return false; // does not happen
		if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFNETWORK || (Settings.eForce == Settings::EFORCE::EM_FORCENONE && (fcc == FCC('RIFX') || fcc == FCC('MThd'))) ) 
			SetParams( true, true );
		else if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFINTEL || (Settings.eForce == Settings::EFORCE::EM_FORCENONE && fcc == FCC('RIFF')) ) 
			SetParams( true, false );

		if ( fcc == FCC('RIFF') || fcc == FCC('LIST') || fcc == FCC('CAT ') ) { 
			if ( !ReadSize( nSize, hti ) ) 
				return false; // does not happen
			if ( nSize > GetReadFileSize() - GetReadTotalBytes() ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Size (") + ToString( nSize ) + _T(") exceeding File Size detected, corrected!") );
				nSize = (DWORD)(GetReadFileSize() - GetReadTotalBytes());
			}
			bFlagRiff = true;
			RIFFReadRiffChunk( hti, nSize, fcc, nSegmentCount );  
		} else if ( fcc == FCC('FORM') || fcc == FCC('RIFX') ) { 
			SetParams( true, true );
			if ( !ReadSize( nSize, hti ) ) 
				return false; // does not happen
			if ( nSize > GetReadFileSize() - GetReadTotalBytes() ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Size (") + ToString( nSize ) + _T(") exceeding File Size detected, corrected!") );
				nSize = (DWORD)(GetReadFileSize() - GetReadTotalBytes());
			}
			bFlagRiff = true;
			RIFFReadRiffChunk( hti, nSize, fcc, nSegmentCount );  
		} else if ( fcc == FCC('MThd') ) { // Midi File without RIFF header, odd byte counts get NOT aligned
			bFlagRiff = true; // ...
			SetParams( false, IsNetworkByteorder() );
			if ( !ReadSeek( -4 ) ) // back to start
				return false; // does not happen
			ASSERT( GetReadFileSize() < (DWORD)-1 );
			RIFFReadMidiChunk( hti, (DWORD)GetReadFileSize(), nSegmentCount );  
		} else if ( ISFCC_ALNUM(fcc) ) {
			if ( !ReadSize( nSize, hti ) ) 
				return false; // does not happen
			if ( nSize > GetReadFileSize() - GetReadTotalBytes() ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Size (") + ToString( nSize ) + _T(") exceeding File Size detected, corrected!") );
				nSize = (DWORD)(GetReadFileSize() - GetReadTotalBytes());
			}
			if ( fcc == FCC('JUNK') ) {
				m_nJunkSize += nSize + 8;
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Junk"), fcc, nSize + 8 );
			} else {
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
				m_nJunkSize += GetReadFileSize() - GetReadTotalBytes();
			}
			if ( nSize > 0 && !ReadSeekFwdDump( Aligned( nSize ), htitmp ) ) 
				return true;
		}
	}
	if ( GetReadTotalBytes() != GetReadFileSize() )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("File Size mismatch (" + ToString( GetReadFileSize() - GetReadTotalBytes() ) + _T(")!")) );

	if ( bFlagRiff ) {
		if ( IsNetworkByteorder() )
			m_sSummary = _T("This is a valid RIFF file in Network byte order of type ") + m_sType + _T(".");
		else
			m_sSummary = _T("This is a valid RIFF file of type ") + m_sType + _T(".");
	}

	return true; // all done
}


// Duplicate in CRIFFSpecParser and AVIParser
void CRIFFSpecParser::RIFFDumpAvi_StrfAudsHeader( int hti, const WAVEFORMATEX* wf, DWORD nSize ) const
{
_tstring s;
int htitmp;

	if ( nSize < sizeof( WAVEFORMATEX ) && nSize != sizeof( WAVEFORMAT ) && nSize != sizeof( PCMWAVEFORMAT ) )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Token Size!") );

	s = _T("00 FormatTag: ") + ToString( wf->wFormatTag ) + _T(" ");
	s += GetAudioFormat( wf->wFormatTag );
	htitmp = OSNodeCreateValue( hti, s );
	switch ( wf->wFormatTag ) {
	case WAVE_FORMAT_PCM:
		if ( nSize != sizeof( PCMWAVEFORMAT ) && nSize != sizeof( WAVEFORMATEX ) ) // WFEX == PCMWF + Size
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown PCMWAVEFORMAT Size, expected ") + ToString( sizeof( PCMWAVEFORMAT ) ) + _T("!") );
		break;
	case WAVE_FORMAT_MPEGLAYER3:
		if ( nSize != sizeof( MPEGLAYER3WAVEFORMAT ) || wf->cbSize != sizeof( MPEGLAYER3WAVEFORMAT ) - sizeof( WAVEFORMATEX ) )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown MPEGLAYER3WAVEFORMAT Size, expected ") + ToString( sizeof( MPEGLAYER3WAVEFORMAT ) ) + _T("!") );
		break;
	case WAVE_FORMAT_MPEG:
		if ( nSize != sizeof( MPEG1WAVEFORMAT ) || wf->cbSize != sizeof( MPEG1WAVEFORMAT ) - sizeof( WAVEFORMATEX ) )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown MPEG1WAVEFORMAT Size, expected ") + ToString( sizeof( MPEG1WAVEFORMAT ) ) + _T("!") );
		break;
	case WAVE_FORMAT_EXTENSIBLE:
		if ( nSize != sizeof ( WAVEFORMATEXTENSIBLE ) || wf->cbSize != sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX ) )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown WAVEFORMATEXTENSIBLE Size, expected ") + ToString( sizeof( WAVEFORMATEXTENSIBLE ) ) + _T("!") );
		break;
	case WAVE_FORMAT_ADPCM:
		if ( nSize != sizeof( ADPCMWAVEFORMAT ) + ((ADPCMWAVEFORMAT*)wf)->wNumCoef * sizeof( ADPCMWAVEFORMAT::ADPCMCOEFSET ) )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown ADPCMWAVEFORMAT Size, expected ") + ToString( sizeof( ADPCMWAVEFORMAT ) + ((ADPCMWAVEFORMAT*)wf)->wNumCoef * sizeof( ADPCMWAVEFORMAT::ADPCMCOEFSET ) ) + _T("!") );
	}
	s = _T("02 Channels: ") + ToString( wf->nChannels ); // number of channels (i.e. mono, stereo)
	OSNodeCreateValue( hti, s );
	s = _T("04 Samples/sec (fps): ") + ToString( wf->nSamplesPerSec ); // sample rate
	htitmp = OSNodeCreateValue( hti, s );
	s = _T("08 Avg Bytes/sec: ") + ToString( wf->nAvgBytesPerSec ); // for buffer estimation
	htitmp = OSNodeCreateValue( hti, s );
	if ( wf->wFormatTag == WAVE_FORMAT_PCM && wf->nAvgBytesPerSec != wf->nSamplesPerSec * wf->nBlockAlign ) // nAvgBytesPerSec should be equal to the product of nSamplesPerSec and nBlockAlign
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("PCM AvgBytesPerSec should be SamplesPerSec * BlockAlign!") );
	s = _T("12 BlockAlign: ") + ToString( wf->nBlockAlign ); // block size of data
	htitmp = OSNodeCreateValue( hti, s );
	if ( wf->nBlockAlign == 0 )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Illegal BlockAlign detected.") );
	if ( wf->wFormatTag == WAVE_FORMAT_MPEGLAYER3 && wf->nBlockAlign >= 960 ) {
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("Presumably variable Bitrate MP3.") );
	}
	if ( (wf->wFormatTag == WAVE_FORMAT_PCM || wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && wf->nBlockAlign != wf->nChannels * wf->wBitsPerSample / 8 ) // nBlockAlign must be equal to the product of nChannels and wBitsPerSample (aufgerundet auf volle 8/16/24... bits) divided by 8 (bits per byte). 
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("PCM BlockAlign should be Channels * BitsPerSample / 8!") );
	else if ( wf->wBitsPerSample > 0 && wf->nBlockAlign % (wf->nChannels * ((wf->wBitsPerSample + 7) / 8)) != 0 ) // entschärft, ein Vielfachses davon reicht
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("BlockAlign should be a multiple of Channels * BitsPerSample / 8!") );
	// The MS imaadpcm codec sets mux_a->h.dwScale = mux_a->h.dwSampleSize = mux_a->wf->nBlockAlign, which seems to work well for mp3 and ac3 too.
	if ( nSize > sizeof( WAVEFORMAT ) ) {
		s = _T("14 Bits/Sample: ") + ToString( wf->wBitsPerSample ); // number of bits per sample of mono data
		htitmp = OSNodeCreateValue( hti, s );
		if ( wf->wFormatTag == WAVE_FORMAT_PCM && wf->wBitsPerSample != 8 && wf->wBitsPerSample != 16 ) // wBitsPerSample should be equal to 8 or 16
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("PCM BitsPerSamples should be 8 or 16!") );
		else if ( wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wf->wBitsPerSample % 8 != 0 ) // wBitsPerSample should be a multiple of 8 for WAVE_FORMAT_EXTENSIBLE
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Ext. BitsPerSamples should be a multiple of 8!") );
		if ( nSize > sizeof( PCMWAVEFORMAT ) ) {
			s = _T("16 Size: ") + ToString( wf->cbSize ); // Size of Extra Information
			htitmp = OSNodeCreateValue( hti, s );
			if ( nSize != sizeof( WAVEFORMATEX ) + wf->cbSize ) // enthält die anderen Formate
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Wrong size in AVIHeaderFormat, should be ") + ToString( nSize - sizeof( WAVEFORMATEX ) ) + _T("!") );
			switch( wf->wFormatTag ) {
			case WAVE_FORMAT_ADPCM: {
				const ADPCMWAVEFORMAT* mwf = static_cast<const ADPCMWAVEFORMAT*>( wf );
				if ( nSize < sizeof( ADPCMWAVEFORMAT ) )
					break;
				s = _T("18 SamplesPerBlock: ") + ToString( mwf->wSamplesPerBlock ); 
				OSNodeCreateValue( hti, s );
				s = _T("20 NumCoef: ") + ToString( mwf->wNumCoef ); 
				htitmp = OSNodeCreateValue( hti, s );
				if ( mwf->wNumCoef < 7 )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("At least 7 coefficients expected!") );
				if ( Settings.nVerbosity > 2 ) {
					_tstring h;
					for ( DWORD i = 0; i < mwf->wNumCoef; ++i ) {
						s = _T(" ") + ToString( mwf->Coefficients[i].iCoef1 ) + _T("/") + ToString( mwf->Coefficients[i].iCoef2 );
						h += s; 
					}
					s = _T("22 Coefficients:") + h; 
					OSNodeCreateValue( hti, s );
				}
				break; }
			case WAVE_FORMAT_MPEGLAYER3: {
				const MPEGLAYER3WAVEFORMAT* mwf = static_cast<const MPEGLAYER3WAVEFORMAT*>( wf );
				s = _T("18 Id: ") + ToString( mwf->wID ); 
				if ( mwf->wID == MPEGLAYER3WAVEFORMAT::MPEGLAYER3_ID_UNKNOWN )
					s += _T(" (Unknown)");
				else if ( mwf->wID == MPEGLAYER3WAVEFORMAT::MPEGLAYER3_ID_MPEG )
					s += _T(" (MPEG)");
				else if ( mwf->wID == MPEGLAYER3WAVEFORMAT::MPEGLAYER3_ID_CONSTANTFRAMESIZE )
					s += _T(" (ConstantFrameSize)");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("20 Flags: 0x") + ToHexString( mwf->fdwFlags ); 
				if ( mwf->fdwFlags == MPEGLAYER3WAVEFORMAT::MPEGLAYER3_FLAG_PADDING_ISO )
					s += _T(" (Padding ISO)");
				else if ( mwf->fdwFlags == MPEGLAYER3WAVEFORMAT::MPEGLAYER3_FLAG_PADDING_ON )
					s += _T(" (Padding On)");
				else if ( mwf->fdwFlags == MPEGLAYER3WAVEFORMAT::MPEGLAYER3_FLAG_PADDING_OFF )
					s += _T(" (Padding Off)");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("24 BlockSize: ") + ToString( mwf->nBlockSize ); 
				OSNodeCreateValue( hti, s );
				s = _T("26 FramesPerBlock: ") + ToString( mwf->nFramesPerBlock ); 
				OSNodeCreateValue( hti, s );
				s = _T("28 CodecDelay: ") + ToString( mwf->nCodecDelay ); 
				OSNodeCreateValue( hti, s );
				break; }
			case WAVE_FORMAT_MPEG: { // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directx9_c/directx/htm/mpeg1waveformat.asp
				const MPEG1WAVEFORMAT* mwf = static_cast<const MPEG1WAVEFORMAT*>( wf );
				s = _T("18 headLayer: ") + ToString( mwf->fwHeadLayer ); 
				if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_LAYER1 )
					s += _T(" (Layer1)");
				else if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_LAYER2 )
					s += _T(" (Layer2)");
				else if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_LAYER3 )
					s += _T(" (Layer3)");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("20 HeadBitrate: ") + ToString( mwf->dwHeadBitrate ); 
				if ( mwf->dwHeadBitrate == 0 )
					s += _T(" Variable");
				OSNodeCreateValue( hti, s );
				s = _T("22 headMode: ") + ToString( mwf->fwHeadMode ); // Could be or'ed if stream contains various modes
				if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_STEREO )
					s += _T(" (Stereo)");
				else if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_JOINTSTEREO )
					s += _T(" (JointStereo)");
				else if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_DUALCHANNEL )
					s += _T(" (DualChannel)");
				else if ( mwf->fwHeadLayer == MPEG1WAVEFORMAT::ACM_MPEG_SINGLECHANNEL )
					s += _T(" (SingleChannel)");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("24 HeadModeExt: ") + ToString( mwf->fwHeadModeExt ); 
				htitmp = OSNodeCreateValue( hti, s );
				if ( mwf->fwHeadMode != MPEG1WAVEFORMAT::ACM_MPEG_JOINTSTEREO && mwf->fwHeadModeExt != 0 )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("HeadModeExt should only be set for ACM_MPEG_JOINTSTEREO!") );
				s = _T("26 HeadEmphasis: ") + ToString( mwf->wHeadEmphasis ); 
				if ( mwf->wHeadEmphasis == MPEG1WAVEFORMAT::ACM_HEAD_EMPHASIS_NONE )
					s += _T(" (None)");
				else if ( mwf->wHeadEmphasis == MPEG1WAVEFORMAT::ACM_HEAD_EMPHASIS_5015MS )
					s += _T(" (50/15 ms emphasis)");
				else if ( mwf->wHeadEmphasis == MPEG1WAVEFORMAT::ACM_HEAD_EMPHASIS_RESERVED )
					s += _T(" (Reserved)");
				else if ( mwf->wHeadEmphasis == MPEG1WAVEFORMAT::ACM_HEAD_EMPHASIS_CCITTJ17 )
					s += _T(" (CCITT J.17)");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("28 HeadFlags: 0x") + ToHexString( mwf->fwHeadFlags ); 
				htitmp = OSNodeCreateValue( hti, s );
				OSNodeCreateValue( htitmp, _T("PrivateBit"), (mwf->fwHeadFlags & MPEG1WAVEFORMAT::ACM_MPEG_PRIVATEBIT) != 0 );
				OSNodeCreateValue( htitmp, _T("Copyright"), (mwf->fwHeadFlags & MPEG1WAVEFORMAT::ACM_MPEG_COPYRIGHT) != 0 );
				OSNodeCreateValue( htitmp, _T("OriginalHome"), (mwf->fwHeadFlags & MPEG1WAVEFORMAT::ACM_MPEG_ORIGINALHOME) != 0 );
				OSNodeCreateValue( htitmp, _T("ProtectionBit"), (mwf->fwHeadFlags & MPEG1WAVEFORMAT::ACM_MPEG_PROTECTIONBIT) != 0 );
				OSNodeCreateValue( htitmp, _T("ID MPEG1"), (mwf->fwHeadFlags & MPEG1WAVEFORMAT::ACM_MPEG_ID_MPEG1) != 0 );
				s = _T("30 HeadModeExt: ") + ToString( mwf->fwHeadModeExt ); 
				OSNodeCreateValue( hti, s );
				s = _T("32 PTSLow: ") + ToString( mwf->dwPTSLow ); 
				OSNodeCreateValue( hti, s );
				s = _T("36 PTSHigh: ") + ToString( mwf->dwPTSHigh ); 
				OSNodeCreateValue( hti, s );
				break; }
			case WAVE_FORMAT_EXTENSIBLE: {
				const WAVEFORMATEXTENSIBLE* mwf = static_cast<const WAVEFORMATEXTENSIBLE*>( wf );
				if ( wf->wBitsPerSample ) {
					s = _T("18 ValidBitsPerSample: ") + ToString( mwf->Samples.wValidBitsPerSample ); 
				} else {
					s = _T("18 SamplesPerBlock: ") + ToString( mwf->Samples.wValidBitsPerSample ); 
					if ( mwf->Samples.wValidBitsPerSample == 0 )
						s += _T(" (Variable)");
				}
				htitmp = OSNodeCreateValue( hti, s );
				if ( wf->wBitsPerSample && (mwf->Samples.wValidBitsPerSample > wf->wBitsPerSample || wf->wBitsPerSample % 8 != 0) )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("ValidBitsPerSample must be less or equal BitsPerSample and BitsPerSample a multiple of 8 bits!") );
				s = _T("20 ChannelMask: 0x") + ToHexString( mwf->dwChannelMask ); 
				htitmp = OSNodeCreateValue( hti, s );
				if ( mwf->dwChannelMask == (DWORD)-1 )
					OSNodeCreateValue( htitmp, _T("All Channel Configurations supported") );
				else {
					OSNodeCreateValue( htitmp, _T("Speaker Front Right"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_FRONT_LEFT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Front Left"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_FRONT_RIGHT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Front Center"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_FRONT_CENTER) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Low Frequency"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_LOW_FREQUENCY) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Back Left"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_BACK_LEFT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Back Right"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_BACK_RIGHT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Back Center"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_BACK_CENTER) != 0 );
					if ( Settings.nVerbosity > 2 ) {
					OSNodeCreateValue( htitmp, _T("Speaker Front LeftOfCenter"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_FRONT_LEFT_OF_CENTER) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Front RightOfCenter"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_FRONT_RIGHT_OF_CENTER) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Side Left"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_SIDE_LEFT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Side Right"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_SIDE_RIGHT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Center"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_CENTER) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Front Left"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_FRONT_LEFT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Front Center"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_FRONT_CENTER) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Front Right"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_FRONT_RIGHT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Back Left"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_BACK_LEFT) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Back Center"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_BACK_CENTER) != 0 );
					OSNodeCreateValue( htitmp, _T("Speaker Top Back Right"), (mwf->dwChannelMask & WAVEFORMATEXTENSIBLE::SPEAKER_TOP_BACK_RIGHT) != 0 );
					}
				}
				_tstring guid =  ToString( mwf->SubFormat );
				s = _T("24 SubFormat GUID: ") + guid; 
				if ( guid == KSDATAFORMAT_SUBTYPE_PCM )
					s += _T(" PCM");
				else if ( guid == KSDATAFORMAT_SUBTYPE_ANALOG )
					s += _T(" Analog");
				else if ( guid == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT )
					s += _T(" IEEE FLOAT");
				else if ( guid == KSDATAFORMAT_SUBTYPE_DRM )
					s += _T(" DRM");
				else if ( guid == KSDATAFORMAT_SUBTYPE_ALAW )
					s += _T(" ALAW");
				else if ( guid == KSDATAFORMAT_SUBTYPE_MULAW )
					s += _T(" MULAW");
				else if ( guid == KSDATAFORMAT_SUBTYPE_ADPCM )
					s += _T(" ADPCM");
				else if ( guid == KSDATAFORMAT_SUBTYPE_MPEG )
					s += _T(" MPEG");
#if 0
				else if ( guid == KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_AC3_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_CC )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_DIRECTMUSIC )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_DSS_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_DTS_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_LPCM_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_MIDI )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_MPEG2_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_SDDS_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_STANDARD_AC3_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_STANDARD_MPEG1_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_STANDARD_MPEG2_AUDIO )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_RIFFWAVE )
					s += _T(" Dolby AC3 SPDIF");
				else if ( guid == KSDATAFORMAT_SUBTYPE_RAW8 )
#endif
				else 
				{
					if ( IS_VALID_WAVEFORMATEX_GUID( &mwf->SubFormat ) ) 
						s += GetAudioFormat( EXTRACT_WAVEFORMATEX_ID( &mwf->SubFormat ) );
				}
				OSNodeCreateValue( hti, s );
				break; }
//			case WAVE_FORMAT_DIVX: // oder MS Audio V2
				//wf->cbSize == 6,10,47; am Ende steht eine GUID
//			case WAVE_FORMAT_MSAUDIO1: // oder MS Audio V1
				//wf->cbSize == 4,41; am Ende steht eine GUID
			default:
				if ( Settings.nVerbosity > 2 && nSize > sizeof( WAVEFORMATEX ) && wf->cbSize > 0 ) {
					s = _T("18 Bytes:"); 
					htitmp = OSNodeCreateValue( hti, s );
					DumpBytes( reinterpret_cast<const BYTE*>( wf ) + sizeof( WAVEFORMATEX ), std::min<DWORD>( wf->cbSize, nSize - sizeof( WAVEFORMATEX ) ), htitmp );
				}
				break;
			}
		}
	}
}
