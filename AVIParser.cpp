#include "Helper.h"
#include "AVIParser.h"
#include "AVIWriter.h"

#ifdef _DEBUG
//#include "omp.h"
#endif

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#pragma warning(push, 3)
#include "new.h"
#pragma warning(pop)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

#undef min
#undef max

/*static*/ const WORD CAVIParser::m_nMinHeaderSize = static_cast<WORD>( sizeof( DWORD ) + sizeof( DWORD ) );



CAVIParser::CAVIParser( class CIOService& ioservice) : CRIFFParser( ioservice )
{
	m_bIsAVI = false;
	m_bPaletteChange = false; 
	m_bHasSubtitles = false;
	m_pAVIDesc = new AVIFileHeader( this );
}


CAVIParser::~CAVIParser()
{
	OSStatsSet(CIOService::EPARSERSTATE::PS_CLEANUP );
	for (size_t i = 0; i < m_pAVIDesc->m_vpFrameEntries.size(); i++) 
		delete m_pAVIDesc->m_vpFrameEntries[i];
	m_pAVIDesc->m_vpFrameEntries.clear();
	// m_pAVIDesc->m_mvpIndexEntries ist bereits gelöscht im AssignFramesIndex() oder nicht????
	for ( mvAVIINDEXENTRIES::iterator pI = m_pAVIDesc->m_mvpIndexEntries.begin(); pI != m_pAVIDesc->m_mvpIndexEntries.end(); ++pI ) {
		for (size_t i = 0; i < pI->second->size(); i++) 
			delete pI->second->at( i );
		pI->second->clear();
		delete pI->second;
	}
	m_pAVIDesc->m_mvpIndexEntries.clear();
	for ( DWORD i = 0; i < m_pAVIDesc->m_vpAVIStreamHeader.size(); ++i ) // TODO in Destruktor schieben
		delete m_pAVIDesc->m_vpAVIStreamHeader[i];
	m_pAVIDesc->m_vpAVIStreamHeader.clear();
	delete m_pAVIDesc;
	OSStatsSet(CIOService::EPARSERSTATE::PS_READY );
}

void CAVIParser::Parse( const _tstring& sIFilename )
{
	m_sIFilename = sIFilename;
	int hti = OSNodeFileNew( 0, sIFilename );
	if ( ReadOpen( m_sIFilename ) )
	{
		OSStatsSet(CIOService::EPARSERSTATE::PS_PARSING );
		OSStatsInit(GetReadFileSize());
		_tstringstream s;
		s << _T("Reading '") << FilenameGetNameExt(m_sIFilename) << _T("' (") << GetReadFileSize() << _T(")") << std::ends; // s.Format( "Reading '%s%s' (%I64d)", sFName, sExt, GetReadFileSize() ); 
																															// TODO filetime ausgeben...
		hti = OSNodeCreate(hti, s.str());
		if (ReadFile(hti)) {
			;
		}
		ReadClose();
	}
	OSNodeFileEnd( hti, m_sSummary );
}

bool CAVIParser::ReadFile( int hti )
{ 
DWORD nSize;
DWORD fcc;
WORD nSegmentCount = 0;
int htiorg = hti, htitmp;
_tstringstream s;
bool bFlagRiff = false;

	while ( GetReadFileSize() > 8 && GetReadTotalBytes() < GetReadFileSize() - 8 ) { 
		if ( !ReadType( fcc, hti ) ) 
			return false; // does not happen
		if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFNETWORK || (Settings.eForce == Settings::EFORCE::EM_FORCENONE && fcc == FCC('RIFX')) ) 
			SetParams( true, true );
		else if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFINTEL || (Settings.eForce == Settings::EFORCE::EM_FORCENONE && fcc == FCC('RIFF')) ) 
			SetParams( true, false );

w:		if ( fcc == FCC('RIFF') || fcc == FCC('RIFX') ) { 
			if ( !ReadSize( nSize, hti ) ) 
				return false; 
			if ( nSize > GetReadFileSize() - GetReadTotalBytes() ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Size (") + ToString( nSize ) + _T(") exceeding File Size detected, corrected!") );
				nSize = (DWORD)(GetReadFileSize() - GetReadTotalBytes());
			}
			if ( nSize >= 1024 * 1024 * 1024 && nSegmentCount == 0 ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Size of first RIFF-chunk should be less than 1GB!") );
				nSize = (DWORD)(GetReadFileSize() - GetReadTotalBytes());
			}
			bFlagRiff = true;
			if ( RIFFReadRiffChunk( hti, nSize, fcc, nSegmentCount ) == CRIFFParser::ERET::ER_EOF )
				break;  
		} else if ( ISFCC_VALID( fcc ) ) {
			if ( !ReadSize( nSize, hti ) ) 
				return false; 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Spurious Chunk after RIFF Chunk"), fcc, nSize + 8 );
			if (RIFFReadHandleCommonChunks(htitmp, nSize, fcc) == CRIFFParser::ERET::ER_NOTCONSUMED) {
				ReadSeekFwdDump(GetReadFileSize() - GetReadTotalBytes(), htitmp);
				break;
			}
		} else {
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Spurious Data after RIFF Chunk"), fcc, GetReadFileSize() - GetReadTotalBytes() + 8 ); // TODO die size stimmt nicht, wenn filesize - totalbytes > 4 GB
			DWORD nSize = (DWORD)std::min( (QWORD)1024, GetReadFileSize() - GetReadTotalBytes() );
			DWORD nOffset = 0;
			if ( !RIFFReadAvi_MoviChunkSearchFCC( htitmp, nSize, fcc, nOffset ) ) {
				ReadSeekFwdDump( GetReadFileSize() - GetReadTotalBytes(), htitmp );
				break;
			}
			goto w;
		}
		ASSERT( GetReadTotalBytes() == GetReadPos() );
	}
	if ( GetReadTotalBytes() < GetReadFileSize() )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("File Size mismatch (" + ToString( GetReadFileSize() - GetReadTotalBytes() ) + _T(")!")) );
	else 
		ASSERT( GetReadTotalBytes() == GetReadFileSize() );
	if ( m_bIsAVI ) 
		CheckConsistency( htiorg ); 
	return true; // all done
}


CRIFFParser::ERET CAVIParser::RIFFReadRiffChunk( int hti, DWORD nChunkSize, FOURCC fcc, WORD& nSegmentCount )
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
	case FCC('AVI '):
		s << _T("RIFF AVI Chunk, Segment #") << nSegmentCount << std::ends; 
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, s.str(), fcc, nChunkSize + 8, fcc2 );
		if ( nSegmentCount != 1 || m_bIsAVI )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
		m_bIsAVI = true;
		return RIFFReadAvi_Chunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('AVIX'):
		s << _T("RIFF AVI Chunk, Segment #") << nSegmentCount << std::ends; 
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, s.str(), fcc, nChunkSize + 8, fcc2 );
		if ( nSegmentCount < 2 || !m_bIsAVI )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_FATAL, _T("Unexpected Token!") );
		return RIFFReadAvi_Chunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	case FCC('INDX'): // Matrox rt2000.avi uses this
	case FCC('menu'): // DIVX 6
		s << _T("RIFF Unexpected Chunk, Segment #") << nSegmentCount << std::ends; 
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, s.str(), fcc, nChunkSize + 8, fcc2 );
		return RIFFReadAvi_MoviChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount, GetReadPos() - 4 );
	default:
		htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Unknown Chunk"), fcc, nChunkSize + 8, fcc2 );
		TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc2 ).c_str() );
		return RIFFReadGenericChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
	}
}


CRIFFParser::ERET CAVIParser::RIFFReadAvi_Chunk( int hti, DWORD nParentSize, WORD nSegment )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
QWORD nReadFileMoviPos = 0;
bool bFlagHdrl = false, bFlagIdx1 = false, bFlagMovi = false;
	
	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( nSize + (nSize & 0x1) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size (") + ToString( nSize ) + _T(") greater Chunk Size (") + ToString( nParentSize - 8 ) + _T(")!") );
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
			case FCC('hdrl'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI Header List"), FCC('LIST'), nSize + 12, fcc );
				if ( bFlagHdrl || bFlagMovi || bFlagIdx1 || nSegment != 1 ) {
					if ( bFlagHdrl || nSegment != 1 ) {
						OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
						if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
							return CRIFFParser::ERET::ER_EOF;
						break;
					} else
						OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
				}
				bFlagHdrl = true;
				if ( RIFFReadAvi_HdrlChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				CalcVectors();

#if 0 // DV Type 1 to 2
				if ( Settings.bConvertType1to2 ) {
					m_pAVIDesc->m_AVIMainHeader.dwFlags |= AVIMAINHEADER::AVIF_ISINTERLEAVED;
					ASSERT( m_pAVIDesc->GetStreamCount() == 1 ); // TODO ....
					AVIStreamHeader* pash = new AVIStreamHeader;
					pash->m_AVIStrhChunk.dwQuality = (DWORD)-1;
					pash->m_AVIStrhChunk.dwRate = 48000; // TODO Sample_DVPayload
					pash->m_AVIStrhChunk.dwScale = 1;
					pash->m_AVIStrhChunk.dwSampleSize = 4;
					pash->m_AVIStrhChunk.fccType = FCC('auds');
					pash->m_AVIStrfChunk.m_size = sizeof( PCMWAVEFORMAT );
					PCMWAVEFORMAT* pwf = new PCMWAVEFORMAT;
					pwf->wBitsPerSample = 16;
					pwf->nChannels = 2;
					pwf->nBlockAlign = (WORD)pash->m_AVIStrhChunk.dwSampleSize;
					pwf->nSamplesPerSec = pash->m_AVIStrhChunk.dwRate;
					pwf->nAvgBytesPerSec = pwf->nSamplesPerSec * pwf->nBlockAlign;
					pwf->wFormatTag = WAVE_FORMAT_PCM;
					pash->m_AVIStrfChunk.data.pwf = reinterpret_cast<WAVEFORMATEX*>( pwf );
					m_pAVIDesc->m_Streams.Create( pash );
				}
#endif
				break;
			case FCC('movi'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI movi List"), FCC('LIST'), nSize + 12, fcc );
				if ( bFlagMovi || (nSegment == 1 && !bFlagHdrl) || bFlagIdx1 ) 
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate or out-of-sequence Token, recovered!") );
				bFlagMovi = true;
				nReadFileMoviPos = GetReadPos() - 4 /*fcc*/;
				switch ( RIFFReadAvi_MoviChunk( htitmp, nSize, nSegment, nReadFileMoviPos ) ) {
				case CRIFFParser::ERET::ER_OK:
					break;
				case CRIFFParser::ERET::ER_EOF:
					return CRIFFParser::ERET::ER_EOF;
				case CRIFFParser::ERET::ER_TOKEN_RIFF:
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					return CRIFFParser::ERET::ER_TOKEN_RIFF;
				case CRIFFParser::ERET::ER_TOKEN_idx1:
					if ( !ReadTypeSize( fcc, nSize, hti ) )
						return CRIFFParser::ERET::ER_EOF;
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					goto w;
					break;
				}
				AssignFramesIndex( hti );
				break;
			case FCC('UNFO'):
			case FCC('INFO'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Info List"), FCC('LIST'), nSize + 12, fcc );
				if ( RIFFReadInfoListChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				break;
			case FCC('PRMI'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI Adobe Premiere Info List"), FCC('LIST'), nSize + 8, fcc );
				if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
				break;
			case FCC('Tdat'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI Adobe Premiere Timecode List"), FCC('LIST'), nSize + 12, fcc );
				if ( RIFFReadGenericChunk( htitmp, nSize, nSegment ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
				break;
			case FCC('uvio'): // HUFFYUV???
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI \?\?\? List"), FCC('LIST'), nSize + 12, fcc );
#if 0
				if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
					return CRIFFParser::ERET::ER_EOF;
#else
				if ( RIFFReadGenericChunk( htitmp, nSize, nSegment ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
#endif
				m_nJunkSize += nSize + 8;
				break;
			case FCC('DXDT'): 
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI divx \?\?\? List"), FCC('LIST'), nSize + 12, fcc );
#if 0
				if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
					return CRIFFParser::ERET::ER_EOF;
#else
				if ( RIFFReadGenericChunk( htitmp, nSize, nSegment ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
#endif
				m_nJunkSize += nSize + 8;
				break;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T(""), FCC('LIST'), nSize + 12, fcc );
				if ( fcc == FCC('RIFF') ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					return CRIFFParser::ERET::ER_TOKEN_RIFF;
				} else if ( fcc == FCC('idx1') ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					if ( !ReadSize( nSize, hti ) )
						return CRIFFParser::ERET::ER_EOF;
					goto w;
				}
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				if ( RIFFReadGenericChunk( htitmp, nSize, nSegment ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				if ( !ReadAlign( nSize ) )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
				break;
			}
			break;
		case FCC('PKEY'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI \?\?\? Chunk"), fcc, nSize + 8 );
			if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('idxx'): // TODO handle separately, Vidomi enhanced AVI 
//		case FCC('iddx'): // Konami AVI 
		case FCC('idx1'):
w:			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Old Index Chunk"), fcc, nSize + 8 );
			if ( bFlagIdx1 || nSegment != 1 || !bFlagHdrl || !bFlagMovi ) 
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate or out-of-sequence Token, recovered!") );
			bFlagIdx1 = true;
			m_pAVIDesc->m_bHasIdx1 = true;
			switch ( RIFFReadAvi_Idx1Chunk( htitmp, nSize, nReadFileMoviPos ) ) {
			case CRIFFParser::ERET::ER_TOKEN_RIFF:
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_RIFF;
			case CRIFFParser::ERET::ER_EOF:
				return CRIFFParser::ERET::ER_EOF;
			}
			AssignFramesIndex( hti );
			break;
		case FCC('segm'): // AVI_IO/Virtual Dub segmented file
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI AVI_IO/Virtual Dub Segmented AVI"), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('Cr8r'): // 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Premiere Creator Chunk"), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('PrmL'): // 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Premiere \?\?\? Chunk"), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('PrmA'): // 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Premiere \?\?\? Chunk"), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('ULSC'): // ??? always 16 Bytes
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI \?\?\? Chunk"), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		default:
			switch( RIFFReadHandleCommonChunks( hti, nSize, fcc ) ) {
			case CRIFFParser::ERET::ER_OK:
				break;
			case CRIFFParser::ERET::ER_EOF:
				return CRIFFParser::ERET::ER_EOF;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
				if ( fcc == FCC('RIFF') ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_RIFF;
				}
				else if ( fcc == FCC('idx1') || fcc == FCC('movi') ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					break;
				}
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
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
	if ( !bFlagHdrl && nSegment == 1 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Missing 'hdrl'-Chunk!") );
	if ( !bFlagMovi )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Missing 'movi'-Chunk!") );
	return CRIFFParser::ERET::ER_OK;
}


CRIFFParser::ERET CAVIParser::RIFFReadAvi_HdrlChunk( int hti, DWORD nParentSize )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
_tstring s;
bool bFlagAvih = false; WORD nCountStrl = 0;

	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( nSize + (nSize & 0x1) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size (") + ToString( nSize ) + _T(") greater Chunk Size (") + ToString( nParentSize - 8 ) + _T(")!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('avih'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Main Header"), fcc, nSize + 8 );
			if ( bFlagAvih || nCountStrl > 0 || m_pAVIDesc->m_bExtHeader ) {
				if ( bFlagAvih ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
						return CRIFFParser::ERET::ER_EOF;
					break;
				} else
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
			}
			bFlagAvih = true;
			if ( !ReadMax( &m_pAVIDesc->m_AVIMainHeader, nSize, sizeof( AVIMAINHEADER ), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			RIFFDumpAvi_AvihHeader( htitmp, &m_pAVIDesc->m_AVIMainHeader, nSize );
			break;
		case FCC('TSIL'):
		case FCC('LIST'):
			nSize -= 4 /*fcc*/;
			if ( !ReadType( fcc, hti ) ) 
				return CRIFFParser::ERET::ER_EOF;
			
			switch ( fcc ) {
			case FCC('strl'): {
				s = _T("AVI Stream List #") + ToString( ++nCountStrl );
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, s, FCC('LIST'), nSize + 12, fcc );
				if ( !bFlagAvih || m_pAVIDesc->m_bExtHeader )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token!") );
				if ( nCountStrl > m_pAVIDesc->m_AVIMainHeader.dwStreams )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Stream count greater than AVI Main Header Streams!") );
				ERET ret;
				if ( (ret = RIFFReadAvi_StrhChunk( htitmp, nSize )) != CRIFFParser::ERET::ER_OK )
					return ret;
				break; }
			case FCC('odml'): // OpenDML
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("AVI OpenDML 1.02 Header List"), FCC('LIST'), nSize + 12, fcc );
				if ( m_pAVIDesc->m_bExtHeader || !bFlagAvih || nCountStrl == 0 ) {
					if ( m_pAVIDesc->m_bExtHeader ) {
						OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
						if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
							return CRIFFParser::ERET::ER_EOF;
						break;
					} else
						OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
				}
				m_pAVIDesc->m_bExtHeader = true;
				if ( !ReadTypeSize( fcc, nSize, htitmp ) ) 
					return CRIFFParser::ERET::ER_EOF;
				if ( fcc != FCC('dmlh') ) {
					htitmp = OSNodeCreate( htitmp, GetReadPos() - 4, _T(""), fcc, nSize + 4 );
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, expected 'dmlh'!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
						return CRIFFParser::ERET::ER_EOF;
					break;
				}
				htitmp = OSNodeCreate( htitmp, GetReadPos() - 12, _T("AVI ODML Header"), fcc, nSize + 12 );
				if ( nSize != sizeof( AVIEXTHEADER ) )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Token Size, expected ") + ToString( sizeof( AVIEXTHEADER ) ) + _T("!") );
				if ( !ReadMax( &m_pAVIDesc->m_AVIExtHeader, nSize, sizeof( AVIEXTHEADER ), htitmp ) )
					return CRIFFParser::ERET::ER_EOF;
				if ( nSize >= sizeof( DWORD ) ) {
					s = _T("Grand Frames: ") + ToString( m_pAVIDesc->m_AVIExtHeader.dwGrandFrames );
					OSNodeCreateValue( htitmp, s );
				}
				break;
			case FCC('INFO'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Info List"), FCC('LIST'), nSize + 12, fcc );
				if ( RIFFReadInfoListChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				break;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T(""), 'LIST', nSize + 12, fcc );
				switch ( fcc ) {
				case FCC('RIFF'):
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_RIFF;
				case FCC('idx1'):
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_idx1;
				case FCC('movi'):
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_movi;
				}
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				if ( RIFFReadGenericChunk( htitmp, nSize, 1 /*nSegment???*/ ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				if ( !ReadAlign( nSize ) )
					return CRIFFParser::ERET::ER_EOF;
				m_nJunkSize += nSize + 8;
				break;
			}
			break;
		case FCC('segm'): // AVI_IO/Virtual Dub segmented file
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI AVI_IO/Virtual Dub Segmented AVI"), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('vedt'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI MS VidEdit Chunk"), fcc, nSize  + 8);
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
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
				switch ( fcc ) {
				case FCC('RIFF'):
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_RIFF;
				case FCC('idx1'):
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_idx1;
				case FCC('movi'):
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
					ReadSeek( -8 /*fcc size*/ );
					return CRIFFParser::ERET::ER_TOKEN_movi;
				}
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
				TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
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
	if ( !bFlagAvih )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Missing 'avih'-Chunk!") );
	if ( nCountStrl == 0 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Missing 'strl'-Chunk!") );
	return CRIFFParser::ERET::ER_OK;
}

CRIFFParser::ERET CAVIParser::RIFFReadAvi_StrhChunk( int hti, DWORD nParentSize )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
_tstring s;
AVIStreamHeader* pash = nullptr;
bool bFlagStrh = false, bFlagStrf = false, bFlagStrn = false, bFlagStrd = false;

	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( nSize + (nSize & 0x1) + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size (") + ToString( nSize ) + _T(") greater Chunk Size (") + ToString( nParentSize - 8 ) + _T(")!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('strh'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI StreamHeader"), fcc, nSize + 8 );
			if ( bFlagStrh || bFlagStrf || bFlagStrn ) {
				if ( bFlagStrh ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
						return CRIFFParser::ERET::ER_EOF;
					break;
				} else
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
			}
			bFlagStrh = true;
			if ( pash == nullptr ) 
				pash = new AVIStreamHeader;
			if ( nSize == sizeof( AVISTREAMHEADER ) + 8 ) { // rcFrame as DWORD instead of WORD ("memory format")
				if ( !ReadBytes( &pash->m_AVIStrhChunk, sizeof( AVISTREAMHEADER ) - 8, htitmp ) ) 
					return CRIFFParser::ERET::ER_EOF;
				DWORD n;
				if ( !ReadSize( n, hti ) )
					return CRIFFParser::ERET::ER_EOF;
				pash->m_AVIStrhChunk.rcFrame.left = (short)n;
				if ( !ReadSize( n, hti ) )
					return CRIFFParser::ERET::ER_EOF;
				pash->m_AVIStrhChunk.rcFrame.top = (short)n;
				if ( !ReadSize( n, hti ) )
					return CRIFFParser::ERET::ER_EOF;
				pash->m_AVIStrhChunk.rcFrame.right = (short)n;
				if ( !ReadSize( n, hti ) )
					return CRIFFParser::ERET::ER_EOF;
				pash->m_AVIStrhChunk.rcFrame.bottom = (short)n;
			} else { // nSize != sizeof( AVISTREAMHEADER ) + 8
				if ( !ReadMax( &pash->m_AVIStrhChunk, nSize, sizeof( AVISTREAMHEADER ), htitmp ) )
					return CRIFFParser::ERET::ER_EOF;
			}
			RIFFDumpAvi_StrhHeader( htitmp, &pash->m_AVIStrhChunk, nSize );
			break;
		case FCC('strf'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI StreamFormat"), fcc, nSize + 8 );
			if ( bFlagStrf || !bFlagStrh ) {
				if ( bFlagStrf ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
						return CRIFFParser::ERET::ER_EOF;
					break;
				} else
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
			}
			bFlagStrf = true;
			if ( pash == nullptr ) 
				pash = new AVIStreamHeader;
			pash->m_AVIStrfChunk.m_size = nSize;
			pash->m_AVIStrfChunk.data.pb = new BYTE[nSize];
			if ( !ReadBytes( pash->m_AVIStrfChunk.data.pb, nSize, htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			RIFFDumpAvi_StrfHeader( htitmp, &pash->m_AVIStrhChunk, &pash->m_AVIStrfChunk );
			m_pAVIDesc->m_Streams.Create( pash );
			if ( (nSize & 0x1) && !ReadSeekFwdDump( 1 ) ) // don't touch nSize, as ReadAlign() would do
				return CRIFFParser::ERET::ER_EOF;
			break;
		case FCC('strc'): // TODO Vidomi Enhanced AVI Stream Chapter
#if 0 // TODO Unknown
			if ( !bFlagStrf || !bFlagStrh ) {
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
				if ( bFlagStrc ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
						return CRIFFParser::ERET::ER_EOF;
					break;
				} else
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
			}
			bFlagStrf = true;
#endif
//			pash->m_AVIStrfChunk.m_size = nSize;
//			pash->m_AVIStrfChunk.data.pb = new BYTE[nSize];

//			if ( !ReadBytes( pash->m_AVIStrfChunk.data.pb, nSize, htitmp ) )
//				return CRIFFParser::ERET::ER_EOF;
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Vidomi Enhanced AVI Stream Chapter"), fcc, nSize + 8 );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize, htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			// TODO DIVXSTREAMCHAPTERS ausgeben
// TODO			RIFFDumpAvi_StreamChapterHeader( htitmp, &pash->m_AVIStrhChunk, &pash->m_AVIStrfChunk );
//			StreamCreate( pash );
			m_nJunkSize += nSize + 8;
			break;
		case FCC('strd'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Additional Stream Data"), fcc, nSize + 8 );
			if ( bFlagStrd || !bFlagStrh ) {
				if ( bFlagStrd ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
						return CRIFFParser::ERET::ER_EOF;
					break;
				} else
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
			}
			bFlagStrd = true;
			if ( pash == nullptr ) 
				pash = new AVIStreamHeader;
			pash->m_AVIStrdChunk.m_size = nSize;
			pash->m_AVIStrdChunk.data.pb = new BYTE[nSize];
			if ( !ReadDumpBytes( pash->m_AVIStrdChunk.data.pb, nSize, htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			if ( (nSize & 0x1) && !ReadSeekFwdDump( 1 ) ) // don't touch nSize, as ReadAlign() would do
				return CRIFFParser::ERET::ER_EOF;
			break; 
		case FCC('indx'): {
			// Darf das nur einmal pro Stream auftreten?
			QWORD nFilePos = GetReadPos();
			htitmp = OSNodeCreate( hti, nFilePos - 8, _T("AVI Stream Index"), fcc, nSize + 8 );
			BYTE* p = new BYTE[nSize];
			if ( !ReadBytes( p, nSize, htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			pash->mFOURCCCounter[fcc].frames++;
			RIFFParseIndxChunk( htitmp, reinterpret_cast<const AVIMetaIndex*>( p ), nFilePos, nSize, m_pAVIDesc->GetStreamCount() - 1u, true );
			if ( (nSize & 0x1) && !ReadSeekFwdDump( 1 ) ) // don't touch nSize, as ReadAlign() would do
				return CRIFFParser::ERET::ER_EOF;
				delete[] p;
			break; }
		case FCC('vprp'): { // Video Property Header
			// checken, dass nur einmal im Video-Stream auftritt...
			// wird derzeit nicht mitgeschrieben!!!
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Stream Video Property Header"), fcc, nSize + 8 );
			BYTE* p = new BYTE[nSize];
			if ( !ReadBytes( p, nSize, htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			RIFFDumpAvi_VprpHeader( htitmp, reinterpret_cast<const VideoPropHeader*>( p ), nSize );
			delete[] p;
			break; }
		case FCC('strn'): {
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Stream Name"), fcc, nSize + 8 );
			if ( bFlagStrn || !bFlagStrh ) {
				if ( bFlagStrn ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected duplicate Token, ignored!") );
					if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) ) 
						return CRIFFParser::ERET::ER_EOF;
					break;
				} else
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected out-of-sequence Token, recovered!") );
			}
			bFlagStrn = true;
			BYTE* p = new BYTE[nSize + 2];
			if ( !ReadBytes( p, nSize + (nSize & 0x01), htitmp ) ) 
				return CRIFFParser::ERET::ER_EOF;
			p[nSize] = '\0';
			if ( pash == nullptr ) 
				pash = new AVIStreamHeader;
			pash->m_sName = reinterpret_cast<const char*>( p );
			s = _T("00 Name: \"") + ToString( pash->m_sName ) + _T("\"");
			OSNodeCreateValue( htitmp, s );
			delete[] p;
			break; }
		case FCC('JUNQ'):
		case FCC('JUNK'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Junk"), fcc, nSize + 8 );
			if ( fcc == FCC('JUNQ') )
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token (Premiere JUNK)!") );
			if ( nSize > 0 && !ReadSeekFwdDump( nSize + (nSize & 0x01), htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
			switch ( fcc ) {
			case FCC('RIFF'):
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_RIFF;
			case FCC('idx1'):
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_idx1;
			case FCC('movi'):
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token, recovered!") );
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_movi;
			}
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( !ReadSeekFwdDump( nSize + (nSize & 0x1), htitmp ) )
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
	if ( !bFlagStrh )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Missing 'strh'-Chunk!") );
	return CRIFFParser::ERET::ER_OK;
}

CRIFFParser::ERET CAVIParser::RIFFReadAvi_MoviChunk( int hti, DWORD nChunkSize, WORD nSegment, QWORD nReadFileMoviPos )
{
DWORD fcc = 0, nSize = 0;
DWORD nOffset = 0;
DWORD nCount1 = 0, // Entries
	nCount2 = 0,  // JUNK, LIST-Entries
	nCount3 = 0, // ##sb, ##pc
	nCount4 = 0, // ix##, ##ix
	nCount5 = 0; // Fehler
_tstring s;
int htitmp = 0;

	if ( nSegment == 1 && !Settings.bTight ) {
		if ( (nReadFileMoviPos + 4 /*fcc*/) % AVI_HEADERSIZE != 0 )
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("'movi' should start at n * AVI_HEADERSIZE(2048) - 4!") );
	}
	if ( nChunkSize < 4 )
		return CRIFFParser::ERET::ER_OK;
	while ( nOffset <= nChunkSize - 4 ) {
		if ( !ReadType( fcc, hti ) ) {
			OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), 0, 0 );
			return CRIFFParser::ERET::ER_EOF;
		}
		nOffset += 4;
		if ( HIFCC(fcc) == 0x0000 && ISLOFCC_NUM(fcc) ) { // part of fcc zeroed, seems to be DWORD aligned, hack!!!
			ReadSeek( -2, true );
			nOffset -= 2;
			if ( nCount5++ < Settings.nTraceMoviEntries ) {
				if ( nCount1 >= Settings.nTraceMoviEntries ) 
					htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ) );
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Odd size frame is DWORD-aligned instead of WORD!") );
			}
			continue;
		}

		bool bFlag = false;
		while ( !ISFCC_VALID(fcc) /*&& fcc != FCC('02tx')*/ ) {
			if ( nOffset >= nChunkSize )
				return CRIFFParser::ERET::ER_OK;
w3:			if ( !bFlag && m_pAVIDesc->m_vpFrameEntries.size() > 0 /* TODO && wenn der Fehler nach dem letzten erwarteten Bild auftritt, braucht es nicht gelöscht zu werden */   ) {
				delete m_pAVIDesc->m_vpFrameEntries[m_pAVIDesc->m_vpFrameEntries.size()-1];
				m_pAVIDesc->m_vpFrameEntries.pop_back(); // letzten Frame löschen, ist vermutlich ebenfalls defekt...
			}
			if ( bFlag ) {
				nOffset += 4; // verhindern dass bei mehrfachen Frames hintereinander eine Endlosschleife entsteht
				ReadSeek( 4 );
			}
			bFlag = true;
			htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T("'movi' Entry #") + ToString( nCount1 ), fcc, 0 );
			if ( !RIFFReadAvi_MoviChunkSearchFCC( htitmp, nChunkSize, fcc, nOffset ) )
				return CRIFFParser::ERET::ER_EOF;
		}

		if ( Settings.nFastmodeCount > 0 && nCount1 > Settings.nFastmodeCount && nCount1 > Settings.nTraceMoviEntries && nCount1 > 25 ) {
			if ( !ReadSeekFwdDump( nChunkSize - nOffset ) ) 
				return CRIFFParser::ERET::ER_EOF; 
			return CRIFFParser::ERET::ER_OK;
		}
		switch( fcc ) {
		case FCC('JUNQ'):
		case FCC('JUNK'): // ignore 'JUNK'
			if ( !ReadSize( nSize, hti ) ) return CRIFFParser::ERET::ER_EOF;
			if ( nCount2++ < Settings.nTraceMoviEntries ) { int htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Junk"), fcc, nSize + 8 ); if ( fcc == FCC('JUNQ') ) OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token (Premiere JUNK)!") ); }
			if ( (nOffset & 0x01) == 0 ) nSize += (nSize & 0x01);
			if ( nSize > 0 && !ReadSeekFwdDump( nSize, (nCount2 <= Settings.nTraceMoviEntries ? hti : 0) ) ) return CRIFFParser::ERET::ER_EOF;
			nOffset += nSize + 4 /*size*/;
			m_nJunkSize += nSize + 8;
			continue;
		case FCC('LIST'):  // ignore 'LIST' n 'rec '
			if ( !ReadSize( nSize, hti ) ) return CRIFFParser::ERET::ER_EOF;
			nOffset += 4 /*size*/;
			if ( !ReadType( fcc, hti ) ) return CRIFFParser::ERET::ER_EOF;
			if ( nCount2++ < Settings.nTraceMoviEntries ) { OSNodeCreate( hti, GetReadPos() - 12, _T(""), FCC('LIST'), nSize + 8, fcc ); }
			if ( fcc != FCC('rec ') ) {
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T(""), FCC('LIST'), nSize + 8, fcc );
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
				return CRIFFParser::ERET::ER_EOF; // ????
			}
			nOffset += 4 /*'rec '*/;
			continue;
		case FCC('RIFF'):
			ReadSeek( -4 /*RIFF*/ );
			return CRIFFParser::ERET::ER_TOKEN_RIFF;
		case FCC('idx1'):
			ReadSeek( -4 /*idx1*/ );
			return CRIFFParser::ERET::ER_TOKEN_idx1;
		default:
			break;
		}

		if ( !ReadSize( nSize, hti ) ) {
			OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, 0 );
			return CRIFFParser::ERET::ER_EOF;
		}
		nOffset += 4;
		OSStatsSet( GetReadPos() );
		if ( nCount1++ < Settings.nTraceMoviEntries || nSize > 1000000 )
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );

		if ( nReadFileMoviPos + nOffset + nSize > GetReadFileSize() ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("File ends prematurely!") );
			goto w3; // search for another frame
		}
		if ( (GetReadPos() & 0x01) != 0 ) {
			if ( nCount5++ < Settings.nTraceMoviEntries ) {
				if ( nCount1 >= Settings.nTraceMoviEntries ) 
					htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Frame starts at odd position!") );
			}
		}

		if ( nOffset + nSize > nChunkSize ) {
			if ( !Settings.bForceChunkSize ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Frame Size (") + ToString( nSize ) + _T(") greater Chunk Size (") + ToString( nChunkSize - nOffset ) + _T("), Frame ignored!") );
				goto w3; // search for another frame
			} else
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Frame Size (") + ToString( nSize ) + _T(") greater Chunk Size (") + ToString( nChunkSize - nOffset ) + _T("), recovering Frame!") );
		}

		if ( HIFCC(fcc) == HIFCC(FCC('ix##')) ) { // ODML Index, Caution "ix##"
			WORD nStream = LOFCC_NUM(fcc);
			if ( nStream >= m_pAVIDesc->GetStreamCount() ) {
				if ( nCount5++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries )
						htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Stream Number greater than defined Streams!") );
					if ( !ReadSeekFwdDump( nSize, htitmp ) ) return CRIFFParser::ERET::ER_EOF;
				} else
					if ( !ReadSeekFwdDump( nSize ) ) return CRIFFParser::ERET::ER_EOF;
				goto w;
			}

			QWORD nFilePos = GetReadPos();
			AVIMetaIndex* mi = (AVIMetaIndex*)new BYTE[nSize];
			if ( !ReadBytes( mi, nSize, hti ) ) 
				return CRIFFParser::ERET::ER_EOF; 

			AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[nStream];
			pash->mFOURCCCounter[fcc].frames++;
			if ( nSize == 0 )
				pash->mFOURCCCounter[fcc].droppedframes++;
			pash->mFOURCCCounter[mi->dwChunkId].odmlindex += mi->nEntriesInUse;

			if ( nCount4++ < Settings.nTraceMoviEntries ) {
				if ( nCount1 > Settings.nTraceMoviEntries )
					htitmp = OSNodeCreate( hti, nFilePos - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
				RIFFParseIndxChunk( htitmp, mi, nFilePos, nSize, nStream, false );
			}
			if ( Settings.bCheckIndex ) {
				for ( WORD i = 0; i < mi->nEntriesInUse; ++i ) {
					if ( mi->wLongsPerEntry == sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 2
						ASSERT( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_CHUNKS );
						mvAVIINDEXENTRIES::iterator pI = m_pAVIDesc->m_mvpIndexEntries.find( mi->dwChunkId );
						if ( pI != m_pAVIDesc->m_mvpIndexEntries.end() ) {
							AVIIndexEntry* pie = new AVIIndexEntry( nSegment, mi, i, nReadFileMoviPos );
							pI->second->push_back( pie );
						} else {
							std::pair<DWORD, vAVIINDEXENTRIES*> p;
							p.first = mi->dwChunkId;
							p.second = new vAVIINDEXENTRIES;
							p.second->reserve( mi->nEntriesInUse ); // ... zu wenig ...
							AVIIndexEntry* pie = new AVIIndexEntry( nSegment, mi, i, nReadFileMoviPos );
							p.second->push_back( pie );
							m_pAVIDesc->m_mvpIndexEntries.insert( p );
						}
					} 
				}
			}
			delete[] (BYTE*)mi;
		} else { // !='ix##'
			WORD nStream;
			if ( ISLOFCC_NUM(fcc) ) // 'xx##'
				nStream = LOFCC_NUM(fcc);
			else
				nStream = HIFCC_NUM(fcc);
			if ( nStream >= m_pAVIDesc->GetStreamCount() ) {
				if ( nCount5++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries )
						htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Stream Number greater than defined Streams!") );
					if ( !ReadSeekFwdDump( nSize, htitmp ) ) return CRIFFParser::ERET::ER_EOF;
				} else
					if ( !ReadSeekFwdDump( nSize ) ) return CRIFFParser::ERET::ER_EOF;
				goto w;
			}
			AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[nStream];
			if ( nSize > pash->nMaxElementSize )
				pash->nMaxElementSize = nSize;
			m_nSizePayload += nSize;
			pash->mFOURCCCounter[fcc].frames++;
			if ( nSize == 0 )
				pash->mFOURCCCounter[fcc].droppedframes++;

			if ( LOFCC(fcc) == LOFCC(FCC('##ix')) ) { // OpenDML Timecode, or false 'ix##' entries
				QWORD nFilePos = GetReadPos();
				AVIMetaIndex* mi = (AVIMetaIndex*)new BYTE[nSize];
				if ( !ReadBytes( mi, nSize, hti ) )  {
					delete[] (BYTE*)mi;
					return CRIFFParser::ERET::ER_EOF; 
				}

				pash->mFOURCCCounter[mi->dwChunkId].odmlindex += mi->nEntriesInUse;

				if ( nCount4++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries ) 
						htitmp = OSNodeCreate( hti, nFilePos - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					if (  mi->wLongsPerEntry == sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 2 AVI_INDEX_OF_CHUNKS
						ASSERT( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_CHUNKS );
						OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Should presumably be 'ix##'") );
					}
					RIFFParseIndxChunk( htitmp, mi, nFilePos, nSize, nStream, false );
				}

				if ( Settings.bCheckIndex ) {
					for ( WORD i = 0; i < mi->nEntriesInUse; ++i ) {
						if ( mi->wLongsPerEntry == sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 2, AVI_INDEX_OF_CHUNKS
							ASSERT( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_CHUNKS );
							mvAVIINDEXENTRIES::iterator pI = m_pAVIDesc->m_mvpIndexEntries.find( mi->dwChunkId );
							if ( pI != m_pAVIDesc->m_mvpIndexEntries.end() )
								pI->second->push_back( new AVIIndexEntry( nSegment, mi, i, nReadFileMoviPos ) );
							else {
								std::pair<DWORD, vAVIINDEXENTRIES*> p;
								p.first = mi->dwChunkId;
								p.second = new vAVIINDEXENTRIES;
								p.second->reserve( mi->nEntriesInUse ); // ... zu wenig...
								p.second->push_back( new AVIIndexEntry( nSegment, mi, i, nReadFileMoviPos ) );
								m_pAVIDesc->m_mvpIndexEntries.insert( p );
							}
						}
					}
				}
				delete[] (BYTE*)mi;
				goto w;
			}

			switch ( pash->m_AVIStrhChunk.fccType ) {
			case FCC('iavs'):
			case FCC('vids'): {
				if ( LOFCC(fcc) == LOFCC(FCC('##wb')) || LOFCC(fcc) == LOFCC(FCC('##tx')) ) {
					if ( !ReadSeekFwdDump( nSize, (Settings.nVerbosity >= 6 && nCount1 <= Settings.nTraceMoviEntries ? hti : 0) ) ) return CRIFFParser::ERET::ER_EOF;
					if ( pash->m_AVIStrhChunk.fccType != FCC('auds') ) {
						if ( nCount5++ < Settings.nTraceMoviEntries ) {
							if ( nCount1 > Settings.nTraceMoviEntries )
								htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
							OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Audio chunk in Video Stream found!") );
						}
						// TODO ignore, change....
					}
				} else if ( LOFCC(fcc) == LOFCC(FCC('##pc')) ) { // PaletteChange
					AVIPALCHANGE *pc = (AVIPALCHANGE *)new BYTE[sizeof( AVIPALCHANGE ) + 256 * sizeof( my::PALETTEENTRY )]; // max size
					if ( !ReadBytes( pc, nSize, hti ) ) 
						return CRIFFParser::ERET::ER_EOF;
					if ( nCount3++ < Settings.nTraceMoviEntries ) {
						if ( nCount1 > Settings.nTraceMoviEntries ) 
							htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
						RIFFDumpAvi_MoviPALChangeEntry( htitmp, pc, nSize );
					}
					m_bPaletteChange = true;
					delete[] (BYTE*)pc;
					break; 
#if 0
				}
				else if ( Settings.bConvertType1to2 ) {
					BYTE* pAudio;
					DWORD nAudioSize;
					ExtractAudioFromDVFrame( hti, nSize, &pAudio, nAudioSize );
					m_pAVIDesc->m_vpFrameEntries.push_back( new AVIFrameEntry( nSegment, FCC('01wb'), nAudioSize, pAudio ) ); 
					m_pAVIDesc->m_vpAVIStreamHeader[1]->mFOURCCCounter[FCC('01wb')].frames++; // ...
#endif
				} else if ( !m_pAVIDesc->m_pDVFrameSample && (pash->m_AVIStrhChunk.fccHandler == FCC('dvsd') || pash->m_AVIStrhChunk.fccHandler == FCC('dv25') || pash->m_AVIStrhChunk.fccHandler == FCC('dv50')) ) {
					BYTE* pFrame = new BYTE[nSize]; 
					if ( !ReadBytes( pFrame, nSize, htitmp ) ) return CRIFFParser::ERET::ER_EOF;
					if ( Settings.nVerbosity >= 6 )
						DumpBytes( pFrame, nSize, htitmp );
					if ( Settings.nVerbosity >= 2 )
						RIFFDumpAvi_DVPayload( htitmp, nSize, pFrame );
					m_pAVIDesc->m_pDVFrameSample = new DVFRAMEINFO;
					Sample_DVPayload( htitmp, m_pAVIDesc->m_pDVFrameSample, nSize, pFrame );
					delete[] pFrame;
				} else if ( Settings.nVerbosity >= 2 && nCount1 <= Settings.nTraceMoviEntries 
						&& (pash->m_AVIStrhChunk.fccHandler == FCC('dvsd') || pash->m_AVIStrhChunk.fccHandler == FCC('dv25') || pash->m_AVIStrhChunk.fccHandler == FCC('dv50')) ) {
					BYTE* pFrame = new BYTE[nSize]; 
					if ( !ReadBytes( pFrame, nSize, htitmp ) ) return CRIFFParser::ERET::ER_EOF;
					if ( Settings.nVerbosity >= 6 )
						DumpBytes( pFrame, nSize, htitmp );
					RIFFDumpAvi_DVPayload( htitmp, nSize, pFrame );
					delete[] pFrame;
				} else if ( Settings.nVerbosity >= 2 && nCount1 <= Settings.nTraceMoviEntries 
						&& pash->m_AVIStrhChunk.fccHandler == FCC('divx') ) { // DivX
							ParseMP4Frame( htitmp, nSize );
				} else
					if ( !ReadSeekFwdDump( nSize, (Settings.nVerbosity >= 6 && nCount1 <= Settings.nTraceMoviEntries ? htitmp : 0) ) ) return CRIFFParser::ERET::ER_EOF;
				break; }
			case FCC('auds'): { // Audio: '##wb'
				if ( !ReadSeekFwdDump( nSize, (Settings.nVerbosity >= 6 && nCount1 <= Settings.nTraceMoviEntries ? htitmp : 0) ) ) return CRIFFParser::ERET::ER_EOF;
				if ( LOFCC(fcc) != LOFCC(FCC('##wb')) ) {
					if ( nCount5++ < Settings.nTraceMoviEntries ) {
						if ( nCount1 > Settings.nTraceMoviEntries )
							htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
						OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected chunk in Audio Stream found!") );
					}
					// TODO ignore, change....
				}
				pash->nStreamSize += nSize;
				break; }
			case FCC('txts'): { // Text: '##tx', '##db'
				if ( LOFCC(fcc) != LOFCC(FCC('##tx')) && LOFCC(fcc) != LOFCC(FCC('##db')) && LOFCC(fcc) != LOFCC(FCC('##dc')) ) {
					TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				}
				BYTE *pst = new BYTE[nSize]; // max size
				if ( !ReadBytes( pst, nSize, hti ) ) 
					return CRIFFParser::ERET::ER_EOF;
				if ( nCount3++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries ) 
						htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					if ( HIFCC_NUM(fcc) < m_pAVIDesc->GetStreamCount() && Settings.nVerbosity >= 1 )
						DumpBytes( pst, nSize, hti );
					else
						; // TODO unknown
				}
				delete[] (BYTE*)pst;
				break; }
			case FCC('mids'): { // Midi: '##??'
//				if (LOFCC(fcc) != LOFCC(FCC('##??')) ) { // ???????
//					; // TODO Unexpected
//				}
				BYTE *pst = new BYTE[nSize]; // max size
				if ( !ReadBytes( pst, nSize, hti ) ) 
					return CRIFFParser::ERET::ER_EOF;
				if ( nCount3++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries ) 
						htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					if ( HIFCC_NUM(fcc) < m_pAVIDesc->GetStreamCount() && Settings.nVerbosity >= 1 )
						DumpBytes( pst, nSize, hti );
				}
				delete[] (BYTE*)pst;
				break; }
			case FCC('subs'): { // DivX Subtitles: '##sb'
				if (LOFCC(fcc) != LOFCC(FCC('##sb')) ) { 
					TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				}
				DIVXSUBTITLESTREAM *pst = (DIVXSUBTITLESTREAM *)new BYTE[nSize]; // max size
				if ( !ReadBytes( pst, nSize, hti ) ) 
					return CRIFFParser::ERET::ER_EOF;
				if ( nCount3++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries ) 
						htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					if ( HIFCC_NUM(fcc) < m_pAVIDesc->GetStreamCount() && m_pAVIDesc->m_vpAVIStreamHeader[HIFCC_NUM(fcc)]->m_AVIStrhChunk.fccType == FCC('subs') )
						RIFFDumpAvi_MoviSubtitleEntry( htitmp, pst, nSize, m_pAVIDesc->m_vpAVIStreamHeader[HIFCC_NUM(fcc)]->m_AVIStrfChunk.data.pst->mode );
					else
						; // TODO unknown
				}
				m_bHasSubtitles = true;
				delete[] (BYTE*)pst;
				break; }
			default:
				TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
				BYTE *pst = new BYTE[nSize]; // max size
				if ( !ReadBytes( pst, nSize, hti ) ) 
					return CRIFFParser::ERET::ER_EOF;
				if ( nCount3++ < Settings.nTraceMoviEntries ) {
					if ( nCount1 > Settings.nTraceMoviEntries ) 
						htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
					if ( Settings.nVerbosity >= 1 )
						DumpBytes( pst, nSize, hti );
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Stream found!") );
				}
			}
			if ( Settings.bCheckIndex ) {
				m_pAVIDesc->m_vpFrameEntries.push_back( new AVIFrameEntry( nSegment, fcc, nOffset - 4, nSize, GetReadPos() - nSize ) ); 
			}
		}
w:		if ( (nOffset & 0x01) == 0 && !ReadAlign( nSize ) ) return CRIFFParser::ERET::ER_EOF;
		nOffset += nSize;
		ASSERT( GetReadTotalBytes() == GetReadPos() );
	}
	if ( Settings.nTraceMoviEntries > 0 && nCount1 > Settings.nTraceMoviEntries )
		OSNodeCreate( hti, GetReadPos() - 8 - nSize, _T("'movi' Entry #") + ToString( nCount1 ), fcc, nSize + 8 );
	if ( nOffset < nChunkSize ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( /*!Settings.bForceChunkSize &&*/ !ReadSeekFwdDump( nChunkSize - nOffset, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}

CRIFFParser::ERET CAVIParser::RIFFParseIndxChunk( int hti, const AVIMetaIndex *mi, QWORD nFilePos, DWORD nSize, WORD nStream, bool bHeader ) // ODML 'ix##' 'indx'
{
	if ( nStream >= m_pAVIDesc->GetStreamCount() /*|| LOFCC(mi->dwChunkId) != nStream*/ ) { // Hier prüfen, wenn best. Header-Index dann muss Stream passen
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Stream Number greater than defined Streams!") );
		m_nJunkSize += nSize;
		return CRIFFParser::ERET::ER_NOTCONSUMED;
	} 

	if ( (nSize - 24) % sizeof( DWORD ) || ((int)nSize - 24) - ((DWORD)sizeof( DWORD ) * mi->wLongsPerEntry * mi->nEntriesInUse) < 0 || nSize > STDINDEXSIZE ) {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Index size!") );
		TRACE3( _T("%s (%s): Wrong Index Size %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nSize ); // TODO
//		ASSERT( false ); // TODO dieser fall wird noch nicht richtig behandelt
	}

	if ( Settings.nTraceIndxEntries > 0 )
		RIFFDumpAvi_IndxChunk( hti, mi, nFilePos, nSize );

	m_nJunkSize += (nSize - 24) - ((DWORD)sizeof( DWORD ) * mi->wLongsPerEntry * mi->nEntriesInUse);
	return (RIFFCheckAvi_IndxChunk( hti, mi, bHeader ) ? CRIFFParser::ERET::ER_OK : CRIFFParser::ERET::ER_SIZE);
}

CRIFFParser::ERET CAVIParser::RIFFReadAvi_Idx1Chunk( int hti, DWORD nParentSize, QWORD nReadFileMoviPos )
{
DWORD nCount1 = 0, nCount2 = 0, nCount3 = 0;
int nSkew = 0;
int htitmp = 0;

	if ( !Settings.bCheckIndex || Settings.nTraceIdx1Entries == 0 ) {
		if ( !ReadSeekFwdDump( nParentSize ) )
			return CRIFFParser::ERET::ER_EOF;
		return CRIFFParser::ERET::ER_OK;
	}

	AVIINDEXENTRY ie;
	bool bFlag = false;
	while( nParentSize >= sizeof( AVIINDEXENTRY ) ) {
		if ( !ReadBytes( &ie, sizeof( AVIINDEXENTRY ), hti ) ) {
			OSNodeCreate( hti, GetReadPos(), _T("'idx1' Entry #") + ToString( nCount1 ), 0, 0 );
			return CRIFFParser::ERET::ER_EOF;
		}
		nParentSize -= sizeof( AVIINDEXENTRY ); 

		if ( nCount1++ < Settings.nTraceIdx1Entries ) {
			htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
			if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
				OSNodeAppendImportant( hti );
			RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
		}

		OSStatsSet( GetReadPos() );

		if ( ie.ckid == FCC('RIFF') ) {
			ReadSeek( -(int)sizeof( AVIINDEXENTRY ) ); 
			return CRIFFParser::ERET::ER_TOKEN_RIFF;
		}

		WORD nStream = HIFCC_NUM(ie.ckid);

		// Das hier stimmt noch nicht - Textstreams haben beliebige Namen?
		if ( ie.ckid != FCC('rec ') && nStream >= m_pAVIDesc->GetStreamCount() ) {
			if ( nCount1 > Settings.nTraceIdx1Entries ) {
				if ( nCount2 < Settings.nTraceIdx1Entries ) {
					htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
					if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
						OSNodeAppendImportant( hti );
					RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
				}
			}
			if ( nCount2++ < Settings.nTraceIdx1Entries )
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Stream Number greater than defined Streams!") );
			continue;
		} 

		if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_LIST) != 0 && ie.ckid != FCC('rec ') ) {
			ie.dwFlags &= ~AVIINDEXENTRY::AVIIF_LIST;
//			m_nCountOldIndexRec++; pash->nCountOldIndexRec
			if ( nCount1 > Settings.nTraceIdx1Entries ) {
				if ( nCount2 < Settings.nTraceIdx1Entries ) {
					htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
					if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
						OSNodeAppendImportant( hti );
					RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
				}
			}
			if ( nCount2++ < Settings.nTraceIdx1Entries )
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Entry marked as AVIIF_LIST, but is not 'rec '!") );
		} else if ( ie.ckid == FCC('rec ') ) {
			if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_LIST) == 0 ) {
				ie.dwFlags |= AVIINDEXENTRY::AVIIF_LIST;
				if ( nCount1 > Settings.nTraceIdx1Entries ) {
					if ( nCount2 < Settings.nTraceIdx1Entries ) {
						htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
						if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
							OSNodeAppendImportant( hti );
						RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
					}
				}
				if ( nCount2++ < Settings.nTraceIdx1Entries )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("'rec ' Entry not marked as AVIIF_LIST!") );
			}
		} else {
			AVIStreamHeader* pash = (nStream >= m_pAVIDesc->GetStreamCount() ? nullptr : m_pAVIDesc->m_vpAVIStreamHeader[nStream]);
			if ( pash ) {
				pash->mFOURCCCounter[ie.ckid].oldindex++;
				if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
					pash->mFOURCCCounter[ie.ckid].keyframes++;
				if ( ie.dwChunkLength == 0 )
					pash->mFOURCCCounter[ie.ckid].droppedframes++;
			}

			if ( LOFCC(ie.ckid) == LOFCC(FCC('##pc')) ) {
				if ( nCount3++ < Settings.nTraceIdx1Entries && nCount1 > Settings.nTraceIdx1Entries )  {
					htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
					if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
						OSNodeAppendImportant( hti );
					RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
				}
			}
		} 

		if ( Settings.bCheckIndex && m_pAVIDesc->m_vpFrameEntries.size() > 0 && (ie.dwFlags & AVIINDEXENTRY::AVIIF_LIST) == 0 && (Settings.nFastmodeCount == 0 || nCount1 < Settings.nFastmodeCount) ) {
			if ( !bFlag ) {
				if ( ie.dwChunkOffset - nReadFileMoviPos == m_pAVIDesc->m_vpFrameEntries.front()->m_nOffset )
					nSkew = (int)(nReadFileMoviPos - 8);
				else if ( ie.dwChunkOffset == m_pAVIDesc->m_vpFrameEntries.front()->m_nOffset )
					nSkew = -8;
				else if ( ie.dwChunkOffset < m_pAVIDesc->m_vpFrameEntries.front()->m_nOffset || (ie.dwChunkOffset > nReadFileMoviPos && ie.dwChunkOffset - nReadFileMoviPos < m_pAVIDesc->m_vpFrameEntries.front()->m_nOffset) ) // Skip idx1 entries for indx entries
					continue;
				else {
// #pragma omp parallel for // break not allowd
					for ( DWORD i = 0; i < m_pAVIDesc->m_vpFrameEntries.size(); ++i ) {
						if ( ie.dwChunkOffset - nReadFileMoviPos == m_pAVIDesc->m_vpFrameEntries[i]->m_nOffset ) {
							nSkew = (int)(nReadFileMoviPos - 8);
							bFlag = true;
							break;
						} else if ( ie.dwChunkOffset == m_pAVIDesc->m_vpFrameEntries[i]->m_nOffset ) {
							nSkew = -8;
							bFlag = true;
							break;
						} else if ( (ie.dwChunkOffset > nReadFileMoviPos && ie.dwChunkOffset - nReadFileMoviPos < m_pAVIDesc->m_vpFrameEntries[i]->m_nOffset) || ie.dwChunkOffset < m_pAVIDesc->m_vpFrameEntries[i]->m_nOffset )
							break;
					}
					if ( !bFlag ) {
						if ( nCount1 > Settings.nTraceIdx1Entries ) {
							if ( nCount2 < Settings.nTraceIdx1Entries ) {
								htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
								if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
									OSNodeAppendImportant( hti );
								RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
							}
						}
						if ( nCount2++ < Settings.nTraceIdx1Entries )
							OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Skew for Offset!") );
						bFlag = true;
						continue;
					}
				}
				bFlag = true;
			}
			mvAVIINDEXENTRIES::iterator pI = m_pAVIDesc->m_mvpIndexEntries.find( ie.ckid );
			if ( pI != m_pAVIDesc->m_mvpIndexEntries.end() )
				pI->second->push_back( new AVIIndexEntry( &ie, nSkew, nReadFileMoviPos ) );
			else {
				std::pair<DWORD, vAVIINDEXENTRIES*> p;
				p.first = ie.ckid;
				p.second = new vAVIINDEXENTRIES;
				p.second->reserve( nParentSize / sizeof( AVIINDEXENTRY ) );
				p.second->push_back( new AVIIndexEntry( &ie, nSkew, nReadFileMoviPos ) );
				m_pAVIDesc->m_mvpIndexEntries.insert( p );
			}
		}
	}
	if ( Settings.nTraceIdx1Entries > 0 && nCount1 > Settings.nTraceIdx1Entries ) {
		htitmp = OSNodeCreate( hti, GetReadPos() - sizeof( AVIINDEXENTRY ), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
		if ( (ie.dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 )
			OSNodeAppendImportant( hti );
		RIFFDumpAvi_Idx1Entry( htitmp, &ie, nReadFileMoviPos, sizeof( AVIINDEXENTRY ) );
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
	}
	return CRIFFParser::ERET::ER_OK;
}

bool CAVIParser::RIFFReadAvi_MoviChunkSearchFCC( int hti, DWORD nChunkSize, FOURCC& fcc, DWORD& nOffset )
{
DWORD n = 0;
bool bRet = false;
	
	ASSERT( GetReadTotalBytes() == GetReadPos() );
	DWORD nOffsetOrg = nOffset;

	if ( nOffset >= 7 ) {
		ReadSeek( -7, true );
		nOffset -= 7;
		nChunkSize += 7;
	}
	nChunkSize = (DWORD)std::min<QWORD>( nChunkSize, nOffset + GetReadFileSize() - GetReadPos() );
	QWORD nPos = GetReadPos();
	hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Garbled Data found!") );

	fcc = 0;
	BYTE* p = new BYTE[512*1024];
	for ( ;; ) {
		DWORD nSize = std::min<DWORD>( 512*1024, nChunkSize - (n + nOffset) );
		const BYTE* t = p;
		if ( nSize == 0 ) {
			bRet = (GetReadFileSize() != GetReadPos()); // return "found", when eveything searched and nothing found and not at end of file
			goto x;
		}
		if ( !ReadBytes( p, nSize, 0, false ) ) {
			bRet = false;
			goto x;
		}
		OSStatsSet( GetReadPos() );
		while ( nSize-- ) {
			if ( n + nOffset >= nChunkSize )
				goto x;
			fcc >>= 8;
			fcc |= *t++ << 24;
			++n;
			if ( ISFCC_VALID(fcc) && (nOffset + n + 4 != nOffsetOrg) ) {
				bRet = true;
				goto x;
			}
		}
	}
x:	delete[] p;
	SetReadPos( nPos + n );
	ReadAddTotalBytes( (QWORD)n );
	
	if ( bRet && ISFCC_VALID(fcc) ) 
		OSNodeCreate( hti, GetReadPos() - 4, _T("Recovered (S:Offset)"), fcc, n + 4 );
	else
		OSNodeCreate( hti, GetReadPos() - 4, _T("Unrecoverable") );
	nOffset += n;
	m_nJunkSize += n;
	ASSERT( GetReadTotalBytes() == GetReadPos() );
	return bRet;
}


void CAVIParser::RIFFDumpAvi_AvihHeader( int hti, const AVIMAINHEADER* mh, DWORD nSize ) const
{
_tstring s;
int htitmp;

	if ( nSize != sizeof( AVIMAINHEADER ) )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Token Size, expected ") + ToString( sizeof( AVIMAINHEADER ) ) + _T("!") );
	s = _T("00 microsec/Frame: ") + ToString( mh->dwMicroSecPerFrame );
	OSNodeCreateValue( hti, s );
	if ( Settings.nVerbosity >= 2 ) {
	s = _T("04 Max Bytes/sec: ") + ToString( mh->dwMaxBytesPerSec );
	OSNodeCreateValue( hti, s );
	}
	if ( Settings.nVerbosity >= 2 ) {
	s = _T("08 Padding Granularity: ") + ToString( mh->dwPaddingGranularity );
	OSNodeCreateValue( hti, s );
	}
	s = _T("12 Flags: 0x") + ToHexString( mh->dwFlags );
	htitmp = OSNodeCreateValue( hti, s );
	OSNodeCreateValue( htitmp, _T("HasIndex"), (mh->dwFlags & AVIMAINHEADER::AVIF_HASINDEX) != 0 );
	OSNodeCreateValue( htitmp, _T("MustUseIndex"), (mh->dwFlags & AVIMAINHEADER::AVIF_MUSTUSEINDEX) != 0 );
	OSNodeCreateValue( htitmp, _T("IsInterleaved"), (mh->dwFlags & AVIMAINHEADER::AVIF_ISINTERLEAVED) != 0 );
	OSNodeCreateValue( htitmp, _T("WasCaptureFile"), (mh->dwFlags & AVIMAINHEADER::AVIF_WASCAPTUREFILE) != 0 ); // Interleaving may be weird
	OSNodeCreateValue( htitmp, _T("Copyrighted"), (mh->dwFlags & AVIMAINHEADER::AVIF_COPYRIGHTED) != 0 );
	OSNodeCreateValue( htitmp, _T("TrustCKType"), (mh->dwFlags & AVIMAINHEADER::AVIF_TRUSTCKTYPE) != 0 ); // für OpenDML Index, Keyframe-Falg gültig oder nicht...
	s = _T("16 Total Frames: ") + ToString( mh->dwTotalFrames );
	OSNodeCreateValue( hti, s ); 
	if ( mh->dwTotalFrames == 0 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("dwTotalFrames may not be 0!") );
	s = _T("20 Initial Frames: ") + ToString( mh->dwInitialFrames );
	OSNodeCreateValue( hti, s );
	s = _T("24 Streams: ") + ToString( mh->dwStreams );
	OSNodeCreateValue( hti, s );
	if ( mh->dwStreams == 0 || mh->dwStreams > 16 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("dwStreams may not be 0 or possibly greater 16!") );
	if ( Settings.nVerbosity >= 2 ) {
	s = _T("28 Suggested Buffer Size: ") + ToString( mh->dwSuggestedBufferSize );
	OSNodeCreateValue( hti, s );
	}
	s = _T("32 Width: ") + ToString( mh->dwWidth );
	OSNodeCreateValue( hti, s );
	if ( (int)m_pAVIDesc->m_AVIMainHeader.dwHeight < 16 || (int)m_pAVIDesc->m_AVIMainHeader.dwHeight > 4096 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Height out of range (16 - 4096)!") );
	s = _T("36 Height: ") + ToString( mh->dwHeight );
	htitmp = OSNodeCreateValue( hti, s );
	if ( (int)m_pAVIDesc->m_AVIMainHeader.dwWidth < 16 || (int)m_pAVIDesc->m_AVIMainHeader.dwWidth > 4096 )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Width out of range (16 - 4096)!") );
	if ( Settings.nVerbosity >= 2 && (mh->dwReserved[0] || mh->dwReserved[1] || mh->dwReserved[2] || mh->dwReserved[3]) ) {
		s = _T("40 Reserved1 (TimeScale): ") + ToString( mh->dwReserved[0] );
		OSNodeCreateValue( hti, s );
		s = _T("44 Reserved2 (DataRate): ") + ToString( mh->dwReserved[1] );
		OSNodeCreateValue( hti, s );
		s = _T("48 Reserved3 (StartTime): ") + ToString( mh->dwReserved[2] );
		OSNodeCreateValue( hti, s );
		s = _T("52 Reserved4 (DataLength): ") + ToString( mh->dwReserved[3] );
		OSNodeCreateValue( hti, s );
	}
	s = _T("-> Length: ") + mh->GetLengthString();
	OSNodeCreateValue( hti, s );
	if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 128 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 96 )
		s = _T("SQCIF 4:3");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 176 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 144 )
		s = _T("QCIF 11:9");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 352 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 240 )
		s = _T("NTSC-VCD, -DVD 11:7.5");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 352 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 288 )
		s = _T("CIF, PAL-VCD, -DVD 11:9");  
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 352 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 480 )
		s = _T("NTSC-DVD 11:15"); 
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 352 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 576 )
		s = _T("PAL-DVD 11:18");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 640 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 480 )
		s = _T("NTSC");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 704 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 480 )
		s = _T("NTSC-DVD 11:7,5");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 704 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 576 )
		s = _T("4CIF, PAL-DVD 11:9");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 720 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 480 )
		s = _T("NTSC-DVD 3:2");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 720 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 576 )
		s = _T("PAL-DVD 5:4");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 768 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 576 )
		s = _T("PAL 5:4");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 1280 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 720 )
		s = _T("HDTV 16:9");
	else if ( m_pAVIDesc->m_AVIMainHeader.dwWidth == 1920 && m_pAVIDesc->m_AVIMainHeader.dwHeight == 1080 )
		s = _T("HDTV 16:9");
	else
		s = _T("unknown");
	OSNodeCreateValue( hti, _T("-> Video Frame Size: ") + s );
}

void CAVIParser::RIFFDumpAvi_StrhHeader( int hti, const AVISTREAMHEADER* pash, DWORD nSize ) const
{
_tstring s;
int htitmp;

	if ( nSize == sizeof( AVISTREAMHEADER ) + 8 && pash->IsVideoStream() ) // Frame als DWORD statt WORD
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong AVIStreamHeader size (Frame item size)!") );
	else if ( nSize != sizeof( AVISTREAMHEADER ) )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Token Size, expected ") + ToString( sizeof( AVISTREAMHEADER ) ) + _T("!") );
	s = _T("00 Type: ") + FCCToString( pash->fccType );
	if ( pash->fccHandler == FCC('dvsd') || pash->fccHandler == FCC('dv25') || pash->fccHandler == FCC('dv50') ) {
		if ( pash->fccType == FCC('vids') ) 
			s+= _T(" DV Type 2 (redundant 'auds' stream, VfW compatible)");
		else if ( pash->fccType == FCC('iavs') )
			s += _T(" DV Type 1 (no 'auds' stream, not VfW compatible)");
		else
			s += _T(" (undefined)");
	}
	htitmp = OSNodeCreateValue( hti, s );
	if ( pash->fccType == FCC('iavs') && (pash->fccHandler != FCC('dvsd') && pash->fccHandler != FCC('DVSD') && pash->fccHandler != FCC('dv25') && pash->fccHandler != FCC('dv50') && pash->fccHandler != FCC('CDVC')) )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown DV Type 1 Handler!") );
	s = _T("04 Handler: 0x") + ToHexString( pash->fccHandler ) + _T(" ") + FCCToString( pash->fccHandler );
	s += GetVideoFormat( pash->fccHandler );
	OSNodeCreateValue( hti, s );
	s = _T("08 Flags: 0x") + ToHexString( pash->dwFlags );
	htitmp = OSNodeCreateValue( hti, s );
	OSNodeCreateValue( htitmp, _T("Disabled"), (pash->dwFlags & AVISTREAMHEADER::AVISF_DISABLED) != 0 );
	OSNodeCreateValue( htitmp, _T("VideoPaletteChanges"), (pash->dwFlags & AVISTREAMHEADER::AVISF_VIDEO_PALCHANGES) != 0 );
	s = _T("12 Priority: ") + ToString( pash->wPriority );
	OSNodeCreateValue( hti, s );
	if ( Settings.nVerbosity >= 2 ) {
		s = _T("14 Language: ") + ToString( pash->wLanguage );
		OSNodeCreateValue( hti, s );
		s = _T("16 InitalFrames: ") + ToString( pash->dwInitialFrames );
		htitmp = OSNodeCreateValue( hti, s );
		if ( pash->IsVideoStream() && m_pAVIDesc->m_AVIMainHeader.dwInitialFrames != pash->dwInitialFrames )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader InitalFrames: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwInitialFrames ) + _T("!") );
	}
	s = _T("20 Scale: ") + ToString( pash->dwScale );
	OSNodeCreateValue( hti, s );
	if ( pash->dwScale == 0 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Scale may not be 0!") );
	s = _T("24 Rate: ") + ToString( pash->dwRate );
	OSNodeCreateValue( hti, s );
	if ( pash->dwRate == 0 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Rate may not be 0!") );
	s = _T("28 Start: ") + ToString( pash->dwStart );
	OSNodeCreateValue( hti, s );
	s = _T("32 Length: ") + ToString( pash->dwLength );
	htitmp = OSNodeCreateValue( hti, s );
	// Dies Prüfung findet über GetLengthMSec() weiter unten statt
//	if ( pash->IsVideoStream() && pash->dwLength != (DWORD)(((double)m_pAVIDesc->m_AVIMainHeader.dwMicroSecPerFrame / 1000000 * m_pAVIDesc->m_AVIMainHeader.dwTotalFrames) * pash->dwRate / pash->dwScale + .5) )
//		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, "Length mismatch MicrosecPerFrame, TotalFrames, Rate and Scale!" );
	if ( Settings.nVerbosity >= 3 ) {
		s = _T("36 SuggestedBufferSize: ") + ToString( pash->dwSuggestedBufferSize );
		OSNodeCreateValue( hti, s );
		s = _T("40 Quality: ") + ToString( (int)pash->dwQuality );
		htitmp = OSNodeCreateValue( hti, s );
		if ( pash->dwQuality > 10000 && pash->dwQuality != (DWORD)-1 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Quality should be between 0 and 10000 or -1!") );
	}
	s = _T("44 SampleSize: ") + ToString( pash->dwSampleSize );
	if ( pash->dwSampleSize == 0 )
		s += _T(" (Variable)");
	OSNodeCreateValue( hti, s );
	if ( pash->fccType != FCC('auds') && pash->fccType != FCC('mids') && nSize >= 56 ) {
		s = _T("48 Frame (l t r b): ") + ToString( pash->rcFrame.left ) + _T(" ") + ToString( pash->rcFrame.top ) + _T(" ") + ToString( pash->rcFrame.right ) + _T(" ") + ToString( pash->rcFrame.bottom );
		htitmp = OSNodeCreateValue( hti, s );
		if ( (int)m_pAVIDesc->m_AVIMainHeader.dwWidth < pash->rcFrame.right || (int)m_pAVIDesc->m_AVIMainHeader.dwWidth < pash->rcFrame.left || pash->rcFrame.right == 0 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader Width: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwWidth ) + _T("!") );
		if ( (int)m_pAVIDesc->m_AVIMainHeader.dwHeight < pash->rcFrame.top || (int)m_pAVIDesc->m_AVIMainHeader.dwHeight < pash->rcFrame.bottom || pash->rcFrame.bottom == 0 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader Height: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwHeight ) + _T("!") );
	}
	s = _T("-> Length: ") + pash->GetLengthString();
	htitmp = OSNodeCreateValue( hti, s );
	if ( pash->IsVideoStream() && m_pAVIDesc->m_AVIMainHeader.dwMicroSecPerFrame && (int)(1000000.0 / m_pAVIDesc->m_AVIMainHeader.dwMicroSecPerFrame + .5) != (int)(pash->GetFPS() + .5) )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader microsecs/Frame: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwMicroSecPerFrame ) + _T("!") );
	if ( std::abs( (int)(m_pAVIDesc->m_AVIMainHeader.GetLengthMSec() - pash->GetLengthMSec()) ) > 500 ) { // weniger als eine halbe Sekunde Differenz
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader Length ") + m_pAVIDesc->m_AVIMainHeader.GetLengthString() + _T("!") );
	}
}

void CAVIParser::RIFFDumpAvi_StrfHeader( int hti, const AVISTREAMHEADER* pash, const AVIStreamElement *sf ) const
{
_tstring s;
int htitmp;

	if ( pash->IsVideoStream() ) {
		const my::BITMAPINFOHEADER* bh = sf->data.pbh;
		if ( pash->fccType == FCC('iavs') ) { // DV Type 1
			if ( sf->m_size > sizeof( DVINFO ) ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected DVINFO size, expected ") + ToString( sizeof( DVINFO ) ) + _T("!") );
				if ( sf->m_size >= sizeof( my::BITMAPINFOHEADER ) ) {
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected BITMAPINFO header, expected DVINFO!") );
					RIFFDumpAvi_StrfVidsHeaderBitmapinfo( hti, sf->data.pbh, sizeof( my::BITMAPINFOHEADER ) );
				}
				if ( sf->m_size >= sizeof( my::BITMAPINFOHEADER ) + sizeof( DVINFO ) - 8 )
					RIFFDumpAvi_DVINFO( hti, (DVINFO *)(sf->data.pb + sizeof( my::BITMAPINFOHEADER )), sf->m_size - bh->biSize );
			} else 
				RIFFDumpAvi_DVINFO( hti, sf->data.pdi, sf->m_size );
			return;
		} else if ( sf->m_size >= sizeof( DVINFO ) - 8 && sf->m_size <= sizeof( DVINFO ) ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected DVINFO header, expected BITMAPINFO!") );
			RIFFDumpAvi_DVINFO( hti, sf->data.pdi, sf->m_size );
			return;
		} else if ( pash->fccHandler == FCC('dvsd') || pash->fccHandler == FCC('dv25') || pash->fccHandler == FCC('dv50') ) { // DV Type 2
			if ( sf->m_size >= sizeof( DVINFO ) - 8 ) {
				if ( sf->m_size >= sizeof( my::BITMAPINFOHEADER ) + sizeof( DVINFO ) - 8 ) {
					RIFFDumpAvi_StrfVidsHeaderBitmapinfo( hti, sf->data.pbh, sizeof( my::BITMAPINFOHEADER ) );
					RIFFDumpAvi_DVINFO( hti, (DVINFO *)(sf->data.pb + sizeof( my::BITMAPINFOHEADER )), sf->m_size - bh->biSize );
				}
				else if ( sf->m_size >= sizeof( my::BITMAPINFOHEADER ) ) {
					RIFFDumpAvi_StrfVidsHeaderBitmapinfo( hti, sf->data.pbh, sf->m_size );
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("DVINFO missing!") );
				}
			}
			return;
		}
		if ( sf->m_size != bh->biSize )
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Mismatching size in AVIStreamFormat") );
		RIFFDumpAvi_StrfVidsHeaderBitmapinfo( hti, sf->data.pbh, sf->m_size );
		if ( Settings.nVerbosity >= 2 && (sf->m_size == sizeof( my::BITMAPV4HEADER ) || sf->m_size == sizeof( my::BITMAPV5HEADER )) )
			RIFFDumpAvi_StrfVidsHeaderExtBitmapinfo( hti, (my::BITMAPV5HEADER *)sf->data.pb, sf->m_size );
		else if ( pash->fccHandler == FCC('MJPG') && sf->m_size > sizeof ( my::BITMAPINFOHEADER ) ) {
			if ( sf->m_size != sizeof( EXBMINFOHEADER ) + sizeof( JPEGINFOHEADER ) 
					|| ((EXBMINFOHEADER*)bh)->biExtDataOffset != sizeof( EXBMINFOHEADER ) )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown EXBMINFOHEADER size!") );
			s = _T("40 ExtDataOffset: ") + ToString( ((EXBMINFOHEADER*)bh)->biExtDataOffset );
			OSNodeCreateValue( hti, s );
			const JPEGINFOHEADER* pjeh = (JPEGINFOHEADER*)((const BYTE*)bh + ((EXBMINFOHEADER*)bh)->biExtDataOffset);
			s = _T("44 JPEGSize: ") + ToString( pjeh->JPEGSize ); // >= sizeof( JPEGINFOHEADER )
			OSNodeCreateValue( hti, s );
			s = _T("48 JPEGProcess: ") + ToString( pjeh->JPEGProcess );
			OSNodeCreateValue( hti, s );
			s = _T("52 JPEGColorSpaceID: ") + ToString( pjeh->JPEGColorSpaceID );
			if ( pjeh->JPEGColorSpaceID == JPEGINFOHEADER::JPEG_Y )
				s += _T(" Y only component of YCbCr");
			else if ( pjeh->JPEGColorSpaceID == JPEGINFOHEADER::JPEG_YCbCr )
				s += _T(" YCbCr CCIR601");
			else if ( pjeh->JPEGColorSpaceID == JPEGINFOHEADER::JPEG_RGB )
				s += _T(" 3 component RGB");
			else
				s += _T(" (undefined)");
			OSNodeCreateValue( hti, s );
			s = _T("56 JPEGBitsPerSample: ") + ToString( pjeh->JPEGBitsPerSample );
			OSNodeCreateValue( hti, s );
			s = _T("60 JPEGHSubSampling: ") + ToString( pjeh->JPEGHSubSampling );
			OSNodeCreateValue( hti, s );
			s = _T("64 JPEGVSubSympling: ") + ToString( pjeh->JPEGVSubSampling );
			OSNodeCreateValue( hti, s );
		} else if ( pash->fccHandler == FCC('HFYU') && sf->m_size > sizeof ( my::BITMAPINFOHEADER ) && (bh->biBitCount & 0x07) == 0 ) {
			s = _T("40 ExtraData Method: ") + ToString( ((HUFFYUV_BITMAPINFOHEADER*)bh)->Extradata.method );
			OSNodeCreateValue( hti, s );
			s = _T("41 ExtraData BPPOverride: ") + ToString( (int)((HUFFYUV_BITMAPINFOHEADER*)bh)->Extradata.bpp_override );
			OSNodeCreateValue( hti, s );
			// 2 Byte reserved, danach Table mit Huffmancodes (ExtraData entfällt, wenn (biBitCount & 0x07) != 0 
		} else if ( (pash->fccHandler == FCC('MSZH') || pash->fccHandler == FCC('ZLIB')) && sf->m_size > sizeof ( my::BITMAPINFOHEADER ) ) {
			s = _T("40 ExtraData MagicValue: 0x") + ToHexString( *((DWORD*)((ZLIBMSZH_BITMAPINFOHEADER*)bh)->unknown) );
			htitmp = OSNodeCreateValue( hti, s );
			if ( *((DWORD*)((ZLIBMSZH_BITMAPINFOHEADER*)bh)->unknown) != 0x00000004 )
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Magic Value, expected 0x00000004!") );
			else {
				s = _T("44 ExtraData ImageType: ") + ToString( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype );
				if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype == ZLIBMSZH_BITMAPINFOHEADER::yuv111 )
					s += _T(" YUV111");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype == ZLIBMSZH_BITMAPINFOHEADER::yuv422 )
					s += _T(" YUV422");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype == ZLIBMSZH_BITMAPINFOHEADER::rgb24 )
					s += _T(" RGB24");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype == ZLIBMSZH_BITMAPINFOHEADER::yuv411 )
					s += _T(" YUV411");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype == ZLIBMSZH_BITMAPINFOHEADER::yuv211 )
					s += _T(" YUV211");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->imagetype == ZLIBMSZH_BITMAPINFOHEADER::yuv420 )
					s += _T(" YUV420");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("45 ExtraData Compression: ") + ToString( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->compression );
				if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->compression == ZLIBMSZH_BITMAPINFOHEADER::mszh_compression )
					s += _T(" MSZH Compression");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->compression == ZLIBMSZH_BITMAPINFOHEADER::mszh_nocompression_zlib_hispeed_compression )
					s += _T(" MSZH No Compression/ZLIB Highspeed Compression");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->compression == ZLIBMSZH_BITMAPINFOHEADER::zlib_high_compression )
					s += _T(" ZLIB High Compression");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->compression == ZLIBMSZH_BITMAPINFOHEADER::zlib_normal_compression )
					s += _T(" ZLIB Normal Compression");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
				s = _T("46 ExtraData Flags: 0x") + ToHexString( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->flags );
				if ( (((ZLIBMSZH_BITMAPINFOHEADER*)bh)->flags & ZLIBMSZH_BITMAPINFOHEADER::multithread_used) != 0 )
					s += _T(" Multithreaded used");
				if ( (((ZLIBMSZH_BITMAPINFOHEADER*)bh)->flags & ZLIBMSZH_BITMAPINFOHEADER::nullframe_insertion_used) != 0 )
					s += _T(" Nullframe Insertion used");
				if ( (((ZLIBMSZH_BITMAPINFOHEADER*)bh)->flags & ZLIBMSZH_BITMAPINFOHEADER::png_filter_used) != 0 )
					s += _T(" PNG Filter used");
				OSNodeCreateValue( hti, s );
				s = _T("47 ExtraData Codec: ") + ToString( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->codec );
				if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->codec == ZLIBMSZH_BITMAPINFOHEADER::MSZH )
					s += _T(" MSZH");
				else if ( ((ZLIBMSZH_BITMAPINFOHEADER*)bh)->codec == ZLIBMSZH_BITMAPINFOHEADER::ZLIB )
					s += _T(" ZLIB");
				else
					s += _T(" (undefined)");
				OSNodeCreateValue( hti, s );
			}
		} else if ( sf->m_size > sizeof( my::BITMAPINFOHEADER ) && Settings.nVerbosity > 2 ) {
				s = _T("40 ExtraData");
				htitmp = OSNodeCreateValue( hti, s );
			DumpBytes( reinterpret_cast<const BYTE*>( bh ) + sizeof( my::BITMAPINFOHEADER ), sf->m_size - sizeof( my::BITMAPINFOHEADER ), htitmp );
		}
	} else if ( pash->fccType == FCC('auds') ) {
		RIFFDumpAvi_StrfAudsHeader( hti, sf->data.pwf, sf->m_size, pash );
	} else if ( pash->fccType == FCC('txts') || pash->fccType == FCC('mids') ) {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_INFO, _T("Nothing known about 'txts'/'mids' streams!") );
		DumpBytes( sf->data.pb, sf->m_size, hti );
	} else if ( pash->fccType == FCC('subs') ) {
		const DIVXSUBTITLEFORMAT* pst = sf->data.pst;
		s = _T("00 Mode: ") + ToString( pst->mode );
		if ( pst->mode == DIVXSUBTITLEFORMAT::DVX_SUBT_MODE_GRAPHIC_FULLRES )
			s += _T(" Graphic Mode Full Resolution");
		if ( pst->mode == DIVXSUBTITLEFORMAT::DVX_SUBT_MODE_GRAPHIC_HALFRES )
			s += _T(" Graphic Mode Half Resolution");
		if ( pst->mode == DIVXSUBTITLEFORMAT::DVX_SUBT_MODE_TEXT )
			s += _T(" Text Mode");
		else
			s += _T(" (undefined)");
		OSNodeCreateValue( hti, s );
		if ( Settings.nVerbosity >= 2 ) {
			s = _T("02 Remap: ") + ToString( pst->remap[0] ) + _T(" ") + ToString( pst->remap[1] ) + _T(" ") + ToString( pst->remap[2] ) + _T(" ") + ToString( pst->remap[3] );
			OSNodeCreateValue( hti, s );
			s = _T("16 Palette: ") + ToString( pst->palette[0] ) + _T(" ") + ToString( pst->palette[1] ) + _T(" ") + ToString( pst->palette[2] ) + _T(" ") + ToString( pst->palette[3] ) + _T(" (Background, Font, Outline, Antialias)");
			OSNodeCreateValue( hti, s );
		}
	} else
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown StreamFormat") );
}

_tstring CAVIParser::GetVideoFormat( FOURCC fcc ) const
{
_tstring s;

	switch ( fcc ) {
		case 0:
			s += _T(" (undefined)");
			break;
		case FCC('ffds'):
			s += _T(" (FFDShow)");
			break;
		case FCC('cvid'):
		case FCC('CVID'):
			s += _T(" (Radius Cinepak)");
			break;
		case FCC('DVSD'):
		case FCC('dvsd'): // IEC-61834 SDL-DVCR 525-60 or SDL-DVCR 625-50
			s += _T(" (DV Standard Definition 25Mb/s)");
			break;
		case FCC('dv25'): // DVCPRO 25 (525-60 or 625-50)
			s += _T(" (DVCPRO25 25Mb/s, 4:1:1)");
			break;
		case FCC('dv50'): // DVCPRO 50 (525-60 or 625-50)
			s += _T(" (DVCPRO50 50Mb/s, 4:2:2)");
			break;
		case FCC('dvh1'): // DVCPRO 100 (1080/60i, 1080/50i, or 720/60P)
			s += _T(" (DV High Definition 100Mb/s)");
			break;
		case FCC('dvhd'): // IEC-61834 HD-DVCR 1125-60 or HD-DVCR 1250-50
			s += _T(" (DV High Definition 50Mb/s)");
			break;
		case FCC('dvsl'): // IEC-61834 SD-DVCR 525-60 or SD-DVCR 625-50
			s += _T(" (DV Low Definition 12.5Mb/s)");
			break;
		case FCC('mjpg'):
		case FCC('MJPG'):
			s += _T(" (Motion JPEG)");
			break;
		case FCC('tscc'):
			s += _T(" (TechSmith Camtasia)");
			break;
		case FCC('VCR1'):
		case FCC('VCR2'):
			s += _T(" (ATI Video)");
			break;
		case FCC('RLE4'):
		case FCC('RLE8'):
		case FCC('RLE '):
		case FCC('mrle'):
			s += _T(" (MS RLE)");
			break;
		case FCC('cram'):
		case FCC('CRAM'):
		case FCC('wham'):
		case FCC('WHAM'):
		case FCC('msvc'):
		case FCC('MSVC'): // Compression CRAM (auch DIV3?)
			s += _T(" (MS Video 1)");
			break;
		case FCC('MP42'):
		case FCC('MP43'): 
		case FCC('MPG4'): 
			s += _T(" (MS MPEG-4)");
			break;
		case FCC('wmv1'):
			s += _T(" (MS Windows Media 7)");
			break;
		case FCC('wmv2'):
			s += _T(" (MS Windows Media 8)");
			break;
		case FCC('wmv3'):
			s += _T(" (MS Windows Media 9)");
			break;
		case FCC('davc'):
			s += _T(" (H.264/MPEG-4 AVC base profile)");
			break;
		case FCC('CDVC'):
			s += _T(" (Canopus DV24)");
			break;
		case FCC('52vd'):
			s += _T(" (Matros Digisuite DV25)");
			break;
		case FCC('AVRn'):
		case FCC('ADVJ'):
			s += _T(" (AVID M-JPEG)");
			break;
		case FCC('3IV1'):
		case FCC('3IV2'):
			s += _T(" (3ivx Delta MPEG-4)");
			break;
		case FCC('VSSH'):
			s += _T(" (Vanguard Software Solutions MPEG-4)");
			break;
		case FCC('DAVC'):
			s += _T(" (mpegable MPEG-4)");
			break;
		case FCC('AP41'):
			s += _T(" (AngelPotion MS MP43 Hack)");
			break;
		case FCC('MMES'):
			s += _T(" (Matrox MPEG I-frame)");
			break;
		case FCC('ASV1'):
		case FCC('ASV2'):
		case FCC('ASVX'):
			s += _T(" (Asus Video)");
			break;
		case FCC('divx'):
		case FCC('DIVX'):
		case FCC('DIV3'):
		case FCC('DIV4'):
		case FCC('DIV5'):
		case FCC('DX50'):
			s += _T(" (DivX/OpenDivX)");
			break;
		case FCC('DXSB'):
			s += _T(" (DivX Subtitles)");
			break;
		case FCC('IAN '): // ???
		case FCC('IV30'):
		case FCC('IV31'):
		case FCC('IV32'):
		case FCC('IV33'):
		case FCC('IV34'):
		case FCC('IV35'):
		case FCC('IV36'):
		case FCC('IV37'):
		case FCC('IV38'):
		case FCC('IV39'):
		case FCC('IV40'):
		case FCC('IV41'):
		case FCC('IV42'):
		case FCC('IV43'):
		case FCC('IV44'):
		case FCC('IV45'):
		case FCC('IV46'):
		case FCC('IV47'):
		case FCC('IV48'):
		case FCC('IV49'):
		case FCC('IV50'):
			s += _T(" (Intel/Ligos Indeo)");
			break;
	}
	return s;
}

void CAVIParser::RIFFDumpAvi_StrfVidsHeaderBitmapinfo( int hti, const my::BITMAPINFOHEADER* bh, DWORD nSize ) const
{
	DWORD nWidth, nHeight;
	DumpBitmapinfo( nWidth, nHeight, nSize, reinterpret_cast<const BYTE*>( bh ), 0, hti, false );

	if ( nWidth != m_pAVIDesc->m_AVIMainHeader.dwWidth )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader Width: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwWidth ) + _T("!") );
	if ( nHeight != m_pAVIDesc->m_AVIMainHeader.dwHeight )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("AVIMainHeader Height: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwHeight ) + _T("!") );
}

void CAVIParser::RIFFDumpAvi_StrfVidsHeaderExtBitmapinfo( int hti, const my::BITMAPV5HEADER* bh, DWORD nSize ) const
{
	_tstring s;
	s = _T("BITMAPV4HEADER (") + ToString( nSize - 40 ) + _T(")");
	hti = OSNodeCreateValue( hti, s );
	s = _T("40 RedMask: ") + ToString( bh->bV4RedMask );
	OSNodeCreateValue( hti, s );
	s = _T("44 GreenMask: ") + ToString( bh->bV4GreenMask );
	OSNodeCreateValue( hti, s );
	s = _T("48 BlueMask: ") + ToString( bh->bV4BlueMask );
	OSNodeCreateValue( hti, s );
	s = _T("52 AlphaMask: ") + ToString( bh->bV4AlphaMask );
	OSNodeCreateValue( hti, s );
	s = _T("56 CSType: ") + ToString( bh->bV4CSType );
	OSNodeCreateValue( hti, s );
	s = _T("60 Endpoints: XYZR ") + ToString( bh->bV4Endpoints.ciexyzRed.ciexyzX ) + _T(" ") + ToString( bh->bV4Endpoints.ciexyzRed.ciexyzY ) + _T(" ") + ToString( bh->bV4Endpoints.ciexyzRed.ciexyzZ ) + _T(" XYZG ") + ToString(  bh->bV4Endpoints.ciexyzGreen.ciexyzX ) + _T(" ") + ToString( bh->bV4Endpoints.ciexyzGreen.ciexyzY ) + _T(" ") + ToString( bh->bV4Endpoints.ciexyzGreen.ciexyzZ ) + _T(" XYZB ") + ToString( bh->bV4Endpoints.ciexyzBlue.ciexyzX ) + _T(" ") + ToString( bh->bV4Endpoints.ciexyzBlue.ciexyzY ) + _T(" ") + ToString( bh->bV4Endpoints.ciexyzBlue.ciexyzZ );
	OSNodeCreateValue( hti, s );
	s = _T("96 GammaRed: ") + ToString( bh->bV4GammaRed );
	OSNodeCreateValue( hti, s );
	s = _T("00 GammaGreen: ") + ToString( bh->bV4GammaGreen );
	OSNodeCreateValue( hti, s );
	s = _T("04 GammaBlue: ") + ToString( bh->bV4GammaBlue );
	OSNodeCreateValue( hti, s );
	if ( nSize == sizeof( my::BITMAPV5HEADER ) ) {
		s = _T("BITMAPV5HEADER (") + ToString( nSize - 108 ) + _T(")");
		hti = OSNodeCreateValue( hti, s );
		s = _T("08 Intent: ") + ToString( bh->bV5Intent );
		OSNodeCreateValue( hti, s );
		s = _T("12 ProfileData: ") + ToString( bh->bV5ProfileData );
		OSNodeCreateValue( hti, s );
		s = _T("16 ProfileSize: ") + ToString( bh->bV5ProfileSize );
		OSNodeCreateValue( hti, s );
		s = _T("20 Reserved: ") + ToString( bh->bV5Reserved );
		OSNodeCreateValue( hti, s );
	}
}


// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directshow/htm/dvinfofieldsettingsinthemsdvdriver.asp
void CAVIParser::RIFFDumpAvi_DVINFO( int hti, const DVINFO* di, DWORD nSize ) const
{
_tstring s;
int htitmp;

	s = _T("DV Info Header (") + ToString( nSize ) + _T(")");
	hti = OSNodeCreateValue( hti, s );
	if ( nSize != sizeof( DVINFO ) && nSize != sizeof( DVINFO ) - 8 )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown token Size, expected ") + ToString( sizeof( DVINFO ) ) + _T("!") );

	s = _T("00 DVAAuxSrc: 0x") + ToHexString( di->dwDVAAuxSrc );
	htitmp = OSNodeCreateValue( hti, s );
	if ( Settings.nVerbosity >= 2 ) {
	if ( di->bfDVAAuxSrc.smp == 0x02 )
		OSNodeCreateValue( htitmp, _T("SMP 32kHz Sampling") );
	else if ( di->bfDVAAuxSrc.smp == 0x01 )
		OSNodeCreateValue( htitmp, _T("SMP 44.1kHz Sampling") );
	else if ( di->bfDVAAuxSrc.smp == 0x00 )
		OSNodeCreateValue( htitmp, _T("SMP 48kHz Sampling") );
	else	
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("SMP undefined Sampling") );
	if ( di->bfDVAAuxSrc.pal_ntsc == 0x01 )
		OSNodeCreateValue( htitmp, _T("FIELDS 50") );
	else 
		OSNodeCreateValue( htitmp, _T("FIELDS 60") );
	if ( di->bfDVAAuxSrc.stype == 0x01 )
		OSNodeCreateValue( htitmp, _T("STYPE 525-60 or 625-50, dvsl (IEC 61883-5)") );
	else if ( di->bfDVAAuxSrc.stype == 0x00 )
		OSNodeCreateValue( htitmp, _T("STYPE 525-60 or 625-50, dvsd (IEC 61834)") );
// != 'dvsd'		OSNodeCreateValue( htitmp, "STYPE 2 audio blocks per video frame (SMPTE 314M/SPMTE 370)" );
	else if ( di->bfDVAAuxSrc.stype == 0x02 )
		OSNodeCreateValue( htitmp, _T("STYPE 4 audio blocks per video frame (SMPTE 314M/SPMTE 370)") );
	else if ( di->bfDVAAuxSrc.stype == 0x03 )
		OSNodeCreateValue( htitmp, _T("STYPE 8 audio blocks per video frame (SMPTE 314M/SPMTE 370)") );
	else	
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("STYPE undefined") );
	}
	s = _T("04 DVAAuxCtl: 0x") + ToHexString( di->dwDVAAuxCtl );
	htitmp = OSNodeCreateValue( hti, s );
	s = _T("08 DVAAuxSrc1: 0x") + ToHexString( di->dwDVAAuxSrc1 );
	OSNodeCreateValue( hti, s );
	if ( Settings.nVerbosity >= 2 ) {
	if ( di->bfDVAAuxSrc1.smp == 0x02 )
		OSNodeCreateValue( htitmp, _T("SMP 32kHz Sampling") );
	else if ( di->bfDVAAuxSrc1.smp == 0x00 )
		OSNodeCreateValue( htitmp, _T("SMP 48kHz Sampling") );
	else	
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("SMP undefined Sampling") );
	if ( di->bfDVAAuxSrc1.pal_ntsc == 0x01 )
		OSNodeCreateValue( htitmp, _T("FIELDS 50") );
	else 
		OSNodeCreateValue( htitmp, _T("FIELDS 60") );
	if ( di->bfDVAAuxSrc1.stype == 0x01 )
		OSNodeCreateValue( htitmp, _T("STYPE 525-60 or 625-50, dvsl (IEC 61883-5)") );
	else if ( di->bfDVAAuxSrc1.stype == 0x00 )
		OSNodeCreateValue( htitmp, _T("STYPE 525-60 or 625-50, dvsd (IEC 61834)") );
// != 'dvsd'		OSNodeCreateValue( htitmp, "STYPE 2 audio blocks per video frame (SMPTE 314M/SPMTE 370)" );
	else if ( di->bfDVAAuxSrc1.stype == 0x02 )
		OSNodeCreateValue( htitmp, _T("STYPE 4 audio blocks per video frame (SMPTE 314M/SPMTE 370)") );
	else if ( di->bfDVAAuxSrc1.stype == 0x03 )
		OSNodeCreateValue( htitmp, _T("STYPE 8 audio blocks per video frame (SMPTE 314M/SPMTE 370)") );
	else	
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("STYPE undefined") );
	}
	s = _T("12 DVAAuxCtl1: 0x") + ToHexString( di->dwDVAAuxCtl1  );
	htitmp = OSNodeCreateValue( hti, s );
	s = _T("16 DVVAuxSrc: 0x") + ToHexString( di->dwDVVAuxSrc );

	htitmp = OSNodeCreateValue( hti, s );
	if ( Settings.nVerbosity >= 2 ) {
	if ( di->bfDVVAuxSrc.pal_ntsc == 0x01 )
		OSNodeCreateValue( htitmp, _T("FIELDS 50") );
	else 
		OSNodeCreateValue( htitmp, _T("FIELDS 60") );
	if ( di->bfDVVAuxSrc.stype == 0x01 )
		OSNodeCreateValue( htitmp, _T("STYPE 525-60 or 625-50, dvsl (IEC 61883-5)") );
	else if ( di->bfDVVAuxSrc.stype == 0x00 )
		OSNodeCreateValue( htitmp, _T("STYPE 525-60 or 625-50, dvsd (IEC 61834)") );
// != 'dvsd'	OSNodeCreateValue( htitmp, "STYPE 4:1:1 (SMPTE 314M)" );
	else if ( di->bfDVVAuxSrc.stype == 0x04 )
		OSNodeCreateValue( htitmp, _T("STYPE 4:2:2 compression (SMPTE 314M)") );
	else if ( di->bfDVVAuxSrc.stype == 0x14 )
		OSNodeCreateValue( htitmp, _T("STYPE 1080/60i (60 Hz) or 1080/50i (50 Hz) (SMPTE 370)") );
	else if ( di->bfDVVAuxSrc.stype == 0x18 )
		OSNodeCreateValue( htitmp, _T("STYPE 720/60p (SMPTE 370)") );
	else	
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("STYPE undefined") );
	}
	s = _T("20 DVVAuxCtl: 0x") + ToHexString( di->dwDVVAuxCtl );
	htitmp = OSNodeCreateValue( hti, s );
	if ( Settings.nVerbosity >= 2 ) {
	if ( di->bfDVVAuxCtl.bcsys == 0x01 )
		OSNodeCreateValue( htitmp, _T("BCSYS Type 1 PAL Widescreen Signalling (ETSI EN 300 294)") );
	else if ( di->bfDVVAuxCtl.bcsys == 0x00 )
		OSNodeCreateValue( htitmp, _T("BCSYS Type 0 NTSC Widescreen Signalling (IEC 61880)") );
	else 
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("BCSYS Type undefined") );
	if ( di->bfDVVAuxCtl.disp == 0x02 )
		OSNodeCreateValue( htitmp, _T("DISP 16:9") );
	else if ( di->bfDVVAuxCtl.disp == 0x00 )
		OSNodeCreateValue( htitmp, _T("DISP 4:3") );
	else
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("DISP Aspect Ratio undefined") );
	}
	if ( Settings.nVerbosity >= 2 && nSize == sizeof( DVINFO ) && (di->dwDVReserved[0] || di->dwDVReserved[1]) ) {
		s = _T("24 DVReserved[0]: 0x") + ToHexString( di->dwDVReserved[0] );
		OSNodeCreateValue( hti, s );
		s = _T("28 DVReserved[1]: 0x") + ToHexString( di->dwDVReserved[1] );
		OSNodeCreateValue( hti, s );
	}
}

void CAVIParser::RIFFDumpAvi_IndxChunk( int hti, const AVIMetaIndex* mi, QWORD nFilePos, DWORD /*nSize*/ ) const
{
_tstring s;
int htitmp;

	OSNodeCreateValue( hti, _T("00 LongsPerEntry: ") + ToString( mi->wLongsPerEntry ) );
	s = _T("02 IndexSubType: ") + ToString( mi->bIndexSubType );
	if ( mi->bIndexSubType == AVIMetaIndex::AVI_INDEX_SUB_DEFAULT )
		s += _T(" Default Frames");
	else if ( mi->bIndexSubType == AVIMetaIndex::AVI_INDEX_SUB_2FIELD )
		s += _T(" 2Field");
	else
		s += _T(" (undefined)");
	OSNodeCreateValue( hti, s );
	s = _T("03 IndexType: ") + ToString( mi->bIndexType );
	if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_CHUNKS ) {
		s += _T(" IndexOfChunks"); //  (each entry points to a chunk)
		if ( mi->wLongsPerEntry == sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) ) // 2
			s += _T(" Standard Frame");
		else if ( mi->wLongsPerEntry == sizeof( mi->FieldIndex.aIndex[0] ) / sizeof( DWORD ) ) // 3
			s += _T(" Standard Field");
		else
			s += _T(" (undefined)");
	} else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_IS_DATA ) {
		s += _T(" IndexIsData"); //  (each entry is index timecode data)
		if ( mi->wLongsPerEntry == sizeof( mi->TimecodeIndex.aIndex[0] ) / sizeof( DWORD ) ) // 4
			s += _T(" Timcode");
		else if ( mi->wLongsPerEntry == sizeof( mi->TimecodeDiscontinuityIndex.aIndex[0] ) / sizeof( DWORD ) ) // 7
			s += _T(" TimcodeDiscontinuity");
		else
			s += _T(" (undefined)");
	} else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_INDEXES )
		s += _T(" IndexOfIndexes"); //  (each entry points to an index)
	else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_TIMED_CHUNKS )
		s += _T(" IndexOfTimedChunks");
	else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_SUB_2FIELD )
		s += _T(" IndexOfSub2Field (Unknown\?\?\?)");
	else
		s += _T(" (undefined)");
	htitmp = OSNodeCreateValue( hti, s );
	switch ( mi->bIndexType ) {
	case AVIMetaIndex::AVI_INDEX_OF_CHUNKS:
		if ( mi->wLongsPerEntry != sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) && mi->wLongsPerEntry != sizeof( mi->FieldIndex.aIndex[0] ) / sizeof( DWORD ) ) // 2, 3
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown LongsPerEntry!") );
		break;
	case AVIMetaIndex::AVI_INDEX_IS_DATA:
		if ( mi->wLongsPerEntry != sizeof( mi->TimecodeIndex.aIndex[0] ) / sizeof( DWORD ) && mi->wLongsPerEntry != sizeof( mi->TimecodeDiscontinuityIndex.aIndex[0] ) / sizeof( DWORD ) ) // 4, 7
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown LongsPerEntry!") );
		if ( mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_DEFAULT )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown SubType!") );
		break;
	case AVIMetaIndex::AVI_INDEX_OF_INDEXES:
		if ( mi->wLongsPerEntry != sizeof( mi->SuperIndex.aIndex[0] ) / sizeof( DWORD ) ) // 4
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown LongsPerEntry!") );
		if ( mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_DEFAULT )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown SubType!") );
		break;
	case AVIMetaIndex::AVI_INDEX_OF_TIMED_CHUNKS:
		ASSERT( mi->bIndexType != AVIMetaIndex::AVI_INDEX_OF_TIMED_CHUNKS );
		if ( mi->wLongsPerEntry != sizeof( mi->TimedIndex.aIndex[0] ) / sizeof( DWORD ) ) // 3
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown LongsPerEntry!") );
		if ( mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_DEFAULT )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown SubType!") );
		break;
	case AVIMetaIndex::AVI_INDEX_OF_SUB_2FIELD:
		ASSERT( mi->bIndexType != AVIMetaIndex::AVI_INDEX_OF_SUB_2FIELD );
		if ( mi->wLongsPerEntry != sizeof( mi->FieldIndex.aIndex[0] ) / sizeof( DWORD ) ) // 3
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown LongsPerEntry!") );
		if ( mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_DEFAULT )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown SubType!") );
		break;
	}
	OSNodeCreateValue( hti, _T("04 EntriesInUse: ") + ToString( mi->nEntriesInUse ) );
	OSNodeCreateValue( hti, _T("08 ChunkId: ") + FCCToString( mi->dwChunkId ) );
	bool bFlag = false;
	DWORD i;
	switch ( mi->bIndexType ) {
		case AVIMetaIndex::AVI_INDEX_OF_CHUNKS: // StandardIndex
			OSNodeCreateValue( hti, _T("12 BaseOffset: ") + ToString( mi->StandardIndex.qwBaseOffset ) );
			if ( mi->StandardIndex.dwReserved_3 || Settings.nVerbosity > 3 ) {
				OSNodeCreateValue( hti, _T("20 Reserved3: ") + ToString( mi->StandardIndex.dwReserved_3 ) );
			}
			for ( i = 0; i < std::min( Settings.nTraceIndxEntries, mi->nEntriesInUse ); ++i ) {
				OSStatsSet( GetReadPos() );
r1:				if ( mi->wLongsPerEntry == sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 2
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->StandardIndex.aIndex[0] ) );
					if ( Settings.nVerbosity >= 2 ) {
						OSNodeCreateValue( htitmp, _T("00 Offset: ") + ToString( mi->StandardIndex.aIndex[i].dwOffset ) );
						s = _T("04 Size: ") + ToString( (mi->StandardIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_SIZEMASK) );
						if ( (mi->StandardIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_DELTAFRAME) == 0 ) {
							s += _T(" Keyframe");
							OSNodeAppendImportant( hti );
						}
						OSNodeCreateValue( htitmp, s );
						s = _T("-> Position: ") + ToString( mi->StandardIndex.qwBaseOffset + mi->StandardIndex.aIndex[i].dwOffset );
						OSNodeCreateValue( htitmp, s );
					}
				} else if ( mi->wLongsPerEntry == sizeof( mi->FieldIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 3
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->FieldIndex.aIndex[0] ) );
					if ( Settings.nVerbosity >= 2 ) {
						s = _T("00 Offset: ") + ToString( mi->FieldIndex.aIndex[i].dwOffset );
						OSNodeCreateValue( htitmp, s );
						s = _T("04 Size: ") + ToString( (mi->FieldIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_SIZEMASK) );
						if ( mi->FieldIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_DELTAFRAME ) {
							s += _T(" NON-Keyframe");
							OSNodeAppendImportant( hti );
						}
						OSNodeCreateValue( htitmp, s );
						OSNodeCreateValue( htitmp, _T("08 OffsetField2: ") + ToString( mi->FieldIndex.aIndex[i].dwOffsetField2 ) );
						OSNodeCreateValue( htitmp, _T("-> Position: ") + ToString( mi->FieldIndex.qwBaseOffset + mi->FieldIndex.aIndex[i].dwOffset ) );
					}
				} else { // unknown LongsPerEntry 
					TRACE3( _T("%s (%s): Undefined LongsPerEntry %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), mi->wLongsPerEntry );
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), mi->wLongsPerEntry * sizeof( DWORD ) );
					s = ToString( i * mi->wLongsPerEntry * sizeof( DWORD ) ) + _T(" Unknown: ");
					for ( int j = 0; j < mi->wLongsPerEntry; ++j ) {
						s += ToString( ((DWORD*)mi->StandardIndex.aIndex)[i * mi->wLongsPerEntry + j] ) + _T(" ");
					}
					OSNodeCreateValue( htitmp, s );
				}
			}
			if ( Settings.nTraceIndxEntries > 0 && !bFlag && i < mi->nEntriesInUse ) {
				i = mi->nEntriesInUse - 1;
				bFlag = true;
				goto r1;
			}
			break;
		case AVIMetaIndex::AVI_INDEX_IS_DATA: // TimecodeIndex
			if ( Settings.nVerbosity >= 2 && (mi->TimecodeIndex.dwReserved[0] || mi->TimecodeIndex.dwReserved[1] || mi->TimecodeIndex.dwReserved[2]) ) {
				OSNodeCreateValue( hti, _T("12 Reserved1: ") + ToString( mi->TimecodeIndex.dwReserved[0] ) );
				OSNodeCreateValue( hti, _T("16 Reserved2: ") + ToString( mi->TimecodeIndex.dwReserved[1] ) );
				OSNodeCreateValue( hti, _T("20 Reserved3: ") + ToString( mi->TimecodeIndex.dwReserved[2] ) );
			}
			for ( i = 0; i < std::min( Settings.nTraceIndxEntries, mi->nEntriesInUse ); ++i ) {
r2:				if ( mi->wLongsPerEntry == sizeof( mi->TimecodeIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 4
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->TimecodeIndex.aIndex[0] ) );
					s = _T("00 time.FrameRate: ") + ToString( mi->TimecodeIndex.aIndex[i].time.wFrameRate );
					if ( mi->TimecodeIndex.aIndex[i].time.wFrameRate == AVIMetaIndex::TIMECODE_RATE_30DROP )
						s += _T(" 30/Drop Frame");
					OSNodeCreateValue( htitmp, s );
					OSNodeCreateValue( htitmp, _T("02 time.FrameFract: ") + ToString( mi->TimecodeIndex.aIndex[i].time.wFrameFract ) );
					OSNodeCreateValue( htitmp, _T("04 time.Frames: ") + ToString( mi->TimecodeIndex.aIndex[i].time.dwFrames ) );
					s = _T("08 SMPTEFlags: 0x") + ToHexString( mi->TimecodeIndex.aIndex[i].dwSMPTEflags );
					if ( mi->TimecodeIndex.aIndex[i].dwSMPTEflags & TIMECODEDATA::TIMECODE_SMPTE_BINARY_GROUP )
						s += _T(" Binary Group");
					if ( mi->TimecodeIndex.aIndex[i].dwSMPTEflags & TIMECODEDATA::TIMECODE_SMPTE_COLOR_FRAME )
						s += _T(" Color Frame");
					OSNodeCreateValue( htitmp, s );
					OSNodeCreateValue( htitmp, _T("12 User: ") + ToString( mi->TimecodeIndex.aIndex[i].dwUser ) );
				} else if ( mi->wLongsPerEntry == sizeof( mi->TimecodeDiscontinuityIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 7
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->TimecodeDiscontinuityIndex.aIndex[0] ) );
					s = _T("00 Tick: ") + ToString( mi->TimecodeDiscontinuityIndex.aIndex[i].dwTick );
					OSNodeCreateValue( htitmp, s );
					s = _T("04 time.FrameRate: ") + ToString( mi->TimecodeDiscontinuityIndex.aIndex[i].time.wFrameRate );
					if ( mi->TimecodeDiscontinuityIndex.aIndex[i].time.wFrameRate == AVIMetaIndex::TIMECODE_RATE_30DROP )
						s += _T(" 30/Drop Frame");
					OSNodeCreateValue( htitmp, s );
					OSNodeCreateValue( htitmp, _T("06 time.FrameFract: ") + ToString( mi->TimecodeDiscontinuityIndex.aIndex[i].time.wFrameFract ) );
					OSNodeCreateValue( htitmp, _T("08 time.Frames: ") + ToString( mi->TimecodeDiscontinuityIndex.aIndex[i].time.dwFrames ) );
					s = _T("12 SMPTEFlags: 0x") + ToHexString( mi->TimecodeDiscontinuityIndex.aIndex[i].dwSMPTEflags );
					if ( mi->TimecodeDiscontinuityIndex.aIndex[i].dwSMPTEflags & TIMECODEDATA::TIMECODE_SMPTE_BINARY_GROUP )
						s += _T(" Binary Group");
					if ( mi->TimecodeDiscontinuityIndex.aIndex[i].dwSMPTEflags & TIMECODEDATA::TIMECODE_SMPTE_COLOR_FRAME )
						s += _T(" Color Frame");
					OSNodeCreateValue( htitmp, s );
					OSNodeCreateValue( htitmp, _T("16 User: ") + ToString( mi->TimecodeDiscontinuityIndex.aIndex[i].dwUser ) );
					OSNodeCreateValue( htitmp, _T("20 ReelId: ") + ToString( mi->TimecodeDiscontinuityIndex.aIndex[i].szReelId ) );
				} else { // unknown LongsPerEntry 
					TRACE3( _T("%s (%s): Undefined LongsPerEntry %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), mi->wLongsPerEntry );
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), mi->wLongsPerEntry * sizeof( DWORD ) );
					s = ToString( i * mi->wLongsPerEntry * sizeof( DWORD ) ) + _T(" Unknown: ");
					for ( int j = 0; j < mi->wLongsPerEntry; ++j ) {
						s += ToString( ((DWORD*)mi->TimecodeDiscontinuityIndex.aIndex)[j] ) + _T(" ");
					}
					OSNodeCreateValue( htitmp, s );
				}
			}
			if ( Settings.nTraceIndxEntries > 0 && !bFlag && i < mi->nEntriesInUse ) {
				i = mi->nEntriesInUse - 1;
				bFlag = true;
				goto r2;
			}
			break;
		case AVIMetaIndex::AVI_INDEX_OF_INDEXES: // SuperIndex
			if ( Settings.nVerbosity >= 2 && (mi->SuperIndex.dwReserved[0] || mi->SuperIndex.dwReserved[1] || mi->SuperIndex.dwReserved[2]) ) {
				OSNodeCreateValue( hti, _T("12 Reserved1: ") + ToString( mi->SuperIndex.dwReserved[0] ) );
				OSNodeCreateValue( hti, _T("16 Reserved2: ") + ToString( mi->SuperIndex.dwReserved[1] ) );
				OSNodeCreateValue( hti, _T("20 Reserved3: ") + ToString( mi->SuperIndex.dwReserved[2] ) );
			}
			for ( i = 0; i < std::min( Settings.nTraceIndxEntries, mi->nEntriesInUse ); ++i ) {
r3:				if ( mi->wLongsPerEntry == sizeof( mi->SuperIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 4
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->SuperIndex.aIndex[0] ) );
					if ( Settings.nVerbosity >= 2 ) {
						OSNodeCreateValue( htitmp, _T("00 Offset: ") + ToString( mi->SuperIndex.aIndex[i].qwOffset ) );
						int htitmp2 = OSNodeCreateValue( htitmp, _T("08 Size: ") + ToString( mi->SuperIndex.aIndex[i].dwSize ) );
						// Diese Größenvorgabe gibt es glaub ich nicht
						if ( mi->SuperIndex.aIndex[i].dwSize > STDINDEXSIZE )
							OSNodeCreateAlert( htitmp2, CIOService::EPRIORITY::EP_ERROR, _T("Index size greater than STDINDEXSIZE!") );
						s = _T("12 Duration: ") + ToString( mi->SuperIndex.aIndex[i].dwDuration );
						OSNodeCreateValue( htitmp, s );
					}
				} else { // unknown LongsPerEntry 
					TRACE3( _T("%s (%s): Undefined LongsPerEntry %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), mi->wLongsPerEntry );
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), mi->wLongsPerEntry * sizeof( DWORD ) );
					s = ToString( i * mi->wLongsPerEntry * sizeof( DWORD ) ) + _T(" Unknown: ");
					for ( int j = 0; j < mi->wLongsPerEntry; ++j ) 
						s += ToString( ((DWORD*)mi->SuperIndex.aIndex)[j] ) + _T(" ");
					OSNodeCreateValue( htitmp, s );
				}
			}
			if ( Settings.nTraceIndxEntries > 0 && !bFlag && i < mi->nEntriesInUse ) {
				i = mi->nEntriesInUse - 1;
				bFlag = true;
				goto r3;
			}
			break;
		case AVIMetaIndex::AVI_INDEX_OF_TIMED_CHUNKS: // TimedIndex
			s = _T("12 BaseOffset: ") + ToString( mi->TimedIndex.qwBaseOffset );
			OSNodeCreateValue( hti, s );
			if ( mi->TimedIndex.dwReserved_3 || Settings.nVerbosity > 3 ) {
				s = _T("20 Reserved3: ") + ToString( mi->TimedIndex.dwReserved_3 );
				OSNodeCreateValue( hti, s );
			}
			for ( i = 0; i < std::min( Settings.nTraceIndxEntries, mi->nEntriesInUse ); ++i ) {
r4:				if ( mi->wLongsPerEntry == sizeof( mi->TimedIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 3
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->TimedIndex.aIndex[0] ) );
					if ( Settings.nVerbosity >= 2 ) {
						OSNodeCreateValue( htitmp, _T("00 Offset: ") + ToString( mi->TimedIndex.aIndex[i].dwOffset ) );
						s = _T("04 Size: ") + ToString( (mi->TimedIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_SIZEMASK) );
						if ( mi->TimedIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_DELTAFRAME )
							s+= _T(" (DeltaFrame)");
						OSNodeCreateValue( htitmp, s );
						OSNodeCreateValue( htitmp, _T("08 Duration: ") + ToString( mi->TimedIndex.aIndex[i].dwDuration ) );
						OSNodeCreateValue( htitmp, _T("-> Position: ") + ToString( mi->TimedIndex.qwBaseOffset + mi->TimedIndex.aIndex[i].dwOffset ) );
					}
				} else { // unknown LongsPerEntry 
					TRACE3( _T("%s (%s): Undefined LongsPerEntry %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), mi->wLongsPerEntry );
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), mi->wLongsPerEntry * sizeof( DWORD ) );
					s = ToString( i * mi->wLongsPerEntry * sizeof( DWORD ) ) + _T(" Unknown: ");
					for ( int j = 0; j < mi->wLongsPerEntry; ++j ) 
						s += ToString( ((DWORD*)mi->TimedIndex.aIndex)[j] ) + _T(" ");
					OSNodeCreateValue( htitmp, s );
				}
			}
			if ( Settings.nTraceIndxEntries > 0 && !bFlag && i < mi->nEntriesInUse ) {
				i = mi->nEntriesInUse - 1;
				bFlag = true;
				goto r4;
			}
			break;
		case AVIMetaIndex::AVI_INDEX_OF_SUB_2FIELD: // FieldIndex
			s = _T("12 BaseOffset: ") + ToString( mi->FieldIndex.qwBaseOffset );
			OSNodeCreateValue( hti, s );
			if ( Settings.nVerbosity >= 2 ) {
				s = _T("20 Reserved3: ") + ToString( mi->FieldIndex.dwReserved3 );
				OSNodeCreateValue( hti, s );
			}
			for ( i = 0; i < std::min( Settings.nTraceIndxEntries, mi->nEntriesInUse ); ++i ) {
r5:				if ( mi->wLongsPerEntry == sizeof( mi->FieldIndex.aIndex[0] ) / sizeof( DWORD ) ) { // 3
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), sizeof( mi->FieldIndex.aIndex[0] ) );
					if ( Settings.nVerbosity >= 2 ) {
						s = _T("00 Offset: ") + ToString( mi->FieldIndex.aIndex[i].dwOffset );
						OSNodeCreateValue( htitmp, s );
						s = _T("04 Size: ") + ToString( (mi->FieldIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_SIZEMASK) );
						if ( mi->TimedIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_DELTAFRAME )
							s+= _T(" (DeltaFrame)");
						OSNodeCreateValue( htitmp, s );
						OSNodeCreateValue( htitmp, _T("08 OffsetField2: ") + ToString( mi->FieldIndex.aIndex[i].dwOffsetField2 ) );
						OSNodeCreateValue( htitmp, _T("-> Position: ") + ToString( mi->FieldIndex.qwBaseOffset + mi->FieldIndex.aIndex[i].dwOffset ) );
						OSNodeCreateValue( htitmp, _T("-> PositionField2: ") + ToString( mi->FieldIndex.qwBaseOffset + mi->FieldIndex.aIndex[i].dwOffsetField2 ) );
					}
				} else { // unknown LongsPerEntry 
					TRACE3( _T("%s (%s): Undefined LongsPerEntry %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), mi->wLongsPerEntry );
					s = _T("'indx' Entry #") + ToString( i + 1 );
					htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), mi->wLongsPerEntry * sizeof( DWORD ) );
					s = ToString( i * mi->wLongsPerEntry * sizeof( DWORD ) ) + _T(" Unknown: ");
					for ( int j = 0; j < mi->wLongsPerEntry; ++j )
						s += ToString( ((DWORD*)mi->TimedIndex.aIndex)[j] ) + _T(" ");
					OSNodeCreateValue( htitmp, s );
				}
			}
			if ( Settings.nTraceIndxEntries > 0 && !bFlag && i < mi->nEntriesInUse ) {
				i = mi->nEntriesInUse - 1;
				bFlag = true;
				goto r5;
			}
			break;
		default:
			TRACE3( _T("%s (%s): Undefined Index Type %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), mi->bIndexType );
			ASSERT( false );
			if ( Settings.nVerbosity >= 2 && (mi->SuperIndex.dwReserved[0] || mi->SuperIndex.dwReserved[1] || mi->SuperIndex.dwReserved[2]) ) {
				OSNodeCreateValue( hti, _T("12 Reserved1: ") + ToString( mi->SuperIndex.dwReserved[0] ) );
				OSNodeCreateValue( hti, _T("16 Reserved2: ") + ToString( mi->SuperIndex.dwReserved[1] ) );
				OSNodeCreateValue( hti, _T("20 Reserved3: ") + ToString( mi->SuperIndex.dwReserved[2] ) );
			}
			for ( i = 0; i < std::min( Settings.nTraceIndxEntries, mi->nEntriesInUse ); ++i ) {
r6:				s = _T("'indx' Entry #") + ToString( i + 1 );
				htitmp = OSNodeCreate( hti, nFilePos + 12 + 12 + i * mi->wLongsPerEntry * sizeof( DWORD ), s, _T(""), mi->wLongsPerEntry * sizeof( DWORD ) );
				s = ToString( i * mi->wLongsPerEntry * sizeof( DWORD ) ) + _T(" Unknown: ");
				for ( int j = 0; j < mi->wLongsPerEntry; ++j ) 
					s += ToString( ((DWORD*)mi->TimedIndex.aIndex)[j] ) + _T(" ");
				OSNodeCreateValue( htitmp, s );
			}
			if ( Settings.nTraceIndxEntries > 0 && !bFlag && i < mi->nEntriesInUse ) {
				i = mi->nEntriesInUse - 1;
				bFlag = true;
				goto r6;
			}
			break;
	}
}

void CAVIParser::RIFFDumpAvi_MoviPALChangeEntry( int hti, const AVIPALCHANGE* pc, DWORD nSize ) const
{
	static constexpr DDEF ddef = {
		4, nullptr, nullptr, 4, 0,
		DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "FirstEntry", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "NumEntries", nullptr, nullptr,
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // exists only for alignment?
		DDEF::DESC::DT_DWORD, -2 | DDEF::DESC::DTL_HEX, 5, "NumEntries", nullptr, nullptr, // TODO PALETTEENTRY
	};
	VERIFY( Dump( hti, nSize, pc, nSize, nSize, &ddef ) == nSize );
}

void CAVIParser::RIFFDumpAvi_MoviSubtitleEntry( int hti, const DIVXSUBTITLESTREAM* p, DWORD /*nSize*/, WORD nMode ) const
{
	_tstring s;
	s = _T("00 StartingFrame: ") + ToString( p->startingFrame );
	OSNodeCreateValue( hti, s );
	s = _T("02 Duration: ") + ToString( p->duration );
	OSNodeCreateValue( hti, s );
	s = _T("04 Left: ") + ToString( p->left );
	OSNodeCreateValue( hti, s );
	s = _T("06 Top: ") + ToString( p->top );
	OSNodeCreateValue( hti, s );
	s = _T("08 Right: ") + ToString( p->right );
	OSNodeCreateValue( hti, s );
	s = _T("10 Bottom: ") + ToString( p->bot );
	OSNodeCreateValue( hti, s );
	s = _T("12 EvenStartPos: ") + ToString( p->evenStartPos );
	OSNodeCreateValue( hti, s );
	s = _T("14 OddStartPos: ") + ToString( p->oddStartPos );
	OSNodeCreateValue( hti, s );
	if ( nMode == DIVXSUBTITLEFORMAT::DVX_SUBT_MODE_TEXT ) {
		s = _T("16 Data: \"") + ToString( reinterpret_cast<const char*>( p->subtitleData ) ) + _T("\""); // TODO strlen?
		OSNodeCreateValue( hti, s );
	}
}


void CAVIParser::RIFFDumpAvi_VprpHeader( int hti, const VideoPropHeader* p, DWORD nSize ) const
{
_tstring s;

	TRACE2( _T("%s (%s): VPRP Header.\n"), _T(__FUNCTION__), m_sIFilename.c_str() );
	if ( nSize != sizeof( VideoPropHeader ) + p->nbFieldPerFrame * sizeof( VideoPropHeader::VIDEO_FIELD_DESC ) )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unknown Token Size!") );
	s = _T("00 VideoFormatToken: ") + ToString( p->VideoFormatToken ); 
	switch ( p->VideoFormatToken ) {
	case VideoPropHeader::FORMAT_PAL_SQUARE:
		s+= _T(" (PAL Square)");
		break;
	case VideoPropHeader::FORMAT_PAL_CCIR_601:
		s+= _T(" (PAL CCIR 601)");
		break;
	case VideoPropHeader::FORMAT_NTSC_SQUARE:
		s+= _T(" (NTSC Square)");
		break;
	case VideoPropHeader::FORMAT_NTSC_CCIR_601:
		s+= _T(" (NTSC CCIR 601)");
		break;
	case VideoPropHeader::FORMAT_UNKNOWN:
		s+= _T(" (Unknown)");
		break;
	default:
		s+= _T(" (undefined)");
		break;
	}
	OSNodeCreateValue( hti, s );
	s = _T("04 VideoStandard: ") + ToString( p->VideoStandard ); 
	switch ( p->VideoStandard ) {
	case VideoPropHeader::STANDARD_PAL:
		s+= _T(" (PAL)");
		break;
	case VideoPropHeader::STANDARD_NTSC:
		s+= _T(" (NTSC)");
		break;
	case VideoPropHeader::STANDARD_SECAM:
		s+= _T(" (SECAM)");
		break;
	case VideoPropHeader::STANDARD_UNKNOWN:
		s+= _T(" (Unknown)");
		break;
	default:
		s+= _T(" (undefined)");
		break;
	}
	OSNodeCreateValue( hti, s );
	s = _T("08 VerticalRefreshRate: ") + ToString( p->dwVerticalRefreshRate ); 
	OSNodeCreateValue( hti, s );
	s = _T("12 HTotalInT: ") + ToString( p->dwHTotalInT ); 
	OSNodeCreateValue( hti, s );
	s = _T("16 VTotalInLines: ") + ToString( p->dwVTotalInLines ); 
	OSNodeCreateValue( hti, s );
	s = _T("20 FrameAspectRatio: 0x") + ToHexString( p->dwFrameAspectRatio ) + _T(" ") + ToString( HIWORD(p->dwFrameAspectRatio) ) + _T(":") + ToString( LOWORD(p->dwFrameAspectRatio) ); 
	OSNodeCreateValue( hti, s );
	s = _T("24 FrameWidthInPixels: ") + ToString( p->dwFrameWidthInPixels ); 
	OSNodeCreateValue( hti, s );
	s = _T("28 FrameHeightInLines: ") + ToString( p->dwFrameHeightInLines ); 
	OSNodeCreateValue( hti, s );
	s = _T("32 FieldPerFrame: ") + ToString( p->nbFieldPerFrame ); 
	OSNodeCreateValue( hti, s );
	for ( DWORD i = 0; i < p->nbFieldPerFrame && nSize >= sizeof( VideoPropHeader ) + (i + 1) * sizeof( VideoPropHeader::VIDEO_FIELD_DESC ); ++i ) {
		s = _T("Video Field Descriptor Entry #") + ToString( i + 1 ) + _T(" ") + ToString( sizeof( VideoPropHeader::VIDEO_FIELD_DESC ) );
		int htitmp = OSNodeCreate( hti, GetReadPos(), s );
		s = _T("00 CompressedBMHeight: ") + ToString( p->FieldInfo[i].CompressedBMHeight );
		OSNodeCreateValue( htitmp, s );
		s = _T("04 CompressedBMWidth: ") + ToString( p->FieldInfo[i].CompressedBMWidth );
		OSNodeCreateValue( htitmp, s );
		s = _T("08 ValidBMHeight: ") + ToString( p->FieldInfo[i].ValidBMHeight );
		OSNodeCreateValue( htitmp, s );
		s = _T("12 ValidBMWidth: ") + ToString( p->FieldInfo[i].ValidBMWidth );
		OSNodeCreateValue( htitmp, s );
		s = _T("16 ValidBMXOffset: ") + ToString( p->FieldInfo[i].ValidBMXOffset );
		OSNodeCreateValue( htitmp, s );
		s = _T("20 ValidBMYOffset: ") + ToString( p->FieldInfo[i].ValidBMYOffset );
		OSNodeCreateValue( htitmp, s );
		s = _T("24 VideoXOffsetInT: ") + ToString( p->FieldInfo[i].VideoXOffsetInT );
		OSNodeCreateValue( htitmp, s );
		s = _T("28 VideoYValidStartLine: ") + ToString( p->FieldInfo[i].VideoYValidStartLine );
		OSNodeCreateValue( htitmp, s );
	}
}


void CAVIParser::RIFFDumpAvi_Idx1Entry( int hti, const AVIINDEXENTRY* ie, QWORD nReadFileMoviPos, DWORD /*nSize*/ ) const
{
_tstring s;
int htitmp;

	if ( Settings.nVerbosity >= 2 ) {
		s = _T("00 ckid: ") + FCCToString( ie->ckid );
		OSNodeCreateValue( hti, s );
		s = _T("04 Flags: 0x") + ToHexString( ie->dwFlags );
		htitmp = OSNodeCreateValue( hti, s );
		OSNodeCreateValue( htitmp, _T("'LIST'-chunk"), (ie->dwFlags & AVIINDEXENTRY::AVIIF_LIST) != 0 );
		OSNodeCreateValue( htitmp, _T("KeyFrame"), (ie->dwFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) != 0 );
		OSNodeCreateValue( htitmp, _T("Midpart"), (ie->dwFlags & AVIINDEXENTRY::AVIIF_MIDPART) != 0 );
		OSNodeCreateValue( htitmp, _T("Firstpart"), (ie->dwFlags & AVIINDEXENTRY::AVIIF_FIRSTPART) != 0 );
		OSNodeCreateValue( htitmp, _T("Lastpart"), (ie->dwFlags & AVIINDEXENTRY::AVIIF_LASTPART) != 0 );
		OSNodeCreateValue( htitmp, _T("NoTime"), (ie->dwFlags & AVIINDEXENTRY::AVIIF_NO_TIME) != 0 );
		s = _T("08 ChunkOffset: ") + ToString( ie->dwChunkOffset );
		OSNodeCreateValue( hti, s );
		s = _T("12 ChunkLength: ") + ToString( ie->dwChunkLength );
		OSNodeCreateValue( hti, s );
		s = _T("-> Position: ") + ToString( nReadFileMoviPos + ie->dwChunkOffset );
		OSNodeCreateValue( hti, s );
	}
}

bool CAVIParser::CheckConsistency( int hti ) const
{
#ifdef _CONSOLE
	_tcout << std::endl;
#endif

	// Initial Frames
	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) {
		AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[i];
		pash->nInitialFrames = 0;
		for ( DWORD j = 0; j < m_pAVIDesc->m_vpFrameEntries.size(); ++j ) {
			if ( HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[j]->m_fcc) == i )
				break;
			pash->nInitialFrames++;
		}
	}

	// Interleaved
	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) {
		const AVIStreamHeader* pasha = m_pAVIDesc->m_vpAVIStreamHeader[i];
		if ( pasha->m_AVIStrhChunk.fccType == FCC('auds') ) {
			const AVIStreamHeader* pashv = nullptr;
			for ( WORD j = 0; j < m_pAVIDesc->GetStreamCount(); ++j ) {
				if ( m_pAVIDesc->m_vpAVIStreamHeader[j]->IsVideoStream() ) {
					pashv = m_pAVIDesc->m_vpAVIStreamHeader[j];
					break;
				}
			}
			if ( !pashv )
				break;
			const DWORD nca = pasha->GetCounterFrames();
			const DWORD ncv = pashv->GetCounterFrames();
			if ( nca > 1 && ncv > 1 ) {
				DWORD npa = 0, npv = 0;
				for ( DWORD k = 0; k < m_pAVIDesc->m_vpFrameEntries.size(); ++k ) {
					if ( HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[k]->m_fcc) == i ) {
						++npa;
						if ( npa == nca )
							break;
						if ( npv > 0 && npv < ncv ) {
							m_pAVIDesc->m_bIsInterleaved = true;
							break;
						}
					} else if ( HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[k]->m_fcc) == pashv->nSrcStreamNr ) {
						++npv;
						if ( npv == ncv )
							break;
						if ( npa > 0 && npa < nca ) {
							m_pAVIDesc->m_bIsInterleaved = true;
							break;
						}
					}
				}
			}
			break;
		}
	}

#if 0 // I-Frames bestimmen anhand der Größe... funktioniert nicht

	const AVIStreamHeader* pashv = nullptr;
	for ( WORD j = 0; j < m_pAVIDesc->GetStreamCount(); ++j ) {
		if ( m_pAVIDesc->m_vpAVIStreamHeader[j]->IsVideoStream() ) {
			pashv = m_pAVIDesc->m_vpAVIStreamHeader[j];
			break;
		}
	}

	DWORD nMin = -1, nMax = 0;
	for ( DWORD k = 0; k < m_pAVIDesc->m_vpFrameEntries.size(); ++k ) {
		if ( HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[k]->m_fcc) == pashv->nSrcStreamNr ) {
			if ( m_pAVIDesc->m_vpFrameEntries[k]->m_nSize > 4 && m_pAVIDesc->m_vpFrameEntries[k]->m_nSize < nMin ) {
				nMin = m_pAVIDesc->m_vpFrameEntries[k]->m_nSize;
				TRACE2( _T("Min %d: %d\n"), k, m_pAVIDesc->m_vpFrameEntries[k]->m_nSize );
			} 
			if ( m_pAVIDesc->m_vpFrameEntries[k]->m_nSize > nMax ) {
				nMax = m_pAVIDesc->m_vpFrameEntries[k]->m_nSize;
				TRACE2( _T("Max %d: %d\n"), k, m_pAVIDesc->m_vpFrameEntries[k]->m_nSize );
			}
		}
	}

	QWORD nClust1 = 0, nClust2 = 0;
	DWORD nCount1 = 0, nCount2 = 0;
	for ( DWORD k = 0; k < m_pAVIDesc->m_vpFrameEntries.size(); ++k ) {
		if ( HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[k]->m_fcc) == pashv->nSrcStreamNr ) {
			DWORD nSize = m_pAVIDesc->m_vpFrameEntries[k]->m_nSize;
			if ( nSize - nMin <= nMax - nSize ) {
				nCount1++, nClust1 += nSize;
				if ( m_pAVIDesc->m_vpFrameEntries[k]->m_nFlags & AVIINDEXENTRY::AVIIF_KEYFRAME )
					TRACE2( _T("min mismatch %d: %d\n"), k, nSize );
			} else {
				nCount2++, nClust2 += nSize;
				if ( (m_pAVIDesc->m_vpFrameEntries[k]->m_nFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) == 0 )
					TRACE2( _T("max mismatch %d: %d\n"), k, nSize );
			}
		}
	}

	if ( nCount1 > 0 )
		nClust1 /= nCount1;
	if ( nCount2 > 0 )
		nClust2 /= nCount2;


#endif

	_tstring s;
	hti = OSNodeCreate( hti, _T("Statistics") );
	if ( Settings.nVerbosity >= 3 ) {
		QWORD nSizeOverhead = GetReadFileSize() - m_nSizePayload;
		s = _T("Overhead Size ") + ToString( nSizeOverhead ) + _T(", approx. ") + ToString( (int)(((double)nSizeOverhead / GetReadFileSize()) * 100 + .5) ) + _T("% (including wasted ") + ToString( m_nJunkSize ) + _T(")");
		OSNodeCreateValue( hti, s );
	}
	if ( ((m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_ISINTERLEAVED) == 0) && m_pAVIDesc->m_bIsInterleaved )
		OSNodeCreateValue( hti, _T("Audio/Video interleaved, but ISINTERLEAVED not set!") );
	else if ( !m_pAVIDesc->m_bIsInterleaved && ((m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_ISINTERLEAVED)) ) 
		OSNodeCreateValue( hti, _T("Audio/Video not interleaved, but ISINTERLEAVED set!") );

	if ( !m_pAVIDesc->m_bMustUseIndex && (m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_MUSTUSEINDEX) 
			&& m_pAVIDesc->m_bIntraFrameIndex && !m_pAVIDesc->m_bHasIdx1 && m_pAVIDesc->m_bExtHeader /* kein idx1 und indx*/ ) {
		m_pAVIDesc->m_bMustUseIndex  = true;
		OSNodeCreateValue( hti, _T("This may be a low-overhead AVI (multiple frames in one chunk)!") );
		// TODO außerdem sind die # gezählte Frames < extheader.dwtotalframes, aber index frames == totalframes
	} else	if ( !m_pAVIDesc->m_bMustUseIndex && (m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_MUSTUSEINDEX) )
		OSNodeCreateValue( hti, _T("Index does not define out-of-sequence/duplicate frames, but AVIF_MUSTUSEINDEX is set in AVIMainHeader!") );
	else if ( m_pAVIDesc->m_bMustUseIndex && ((m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_MUSTUSEINDEX) == 0) )
		OSNodeCreateValue( hti, _T("Index defines out-of-sequence/duplicate frames, but AVIF_MUSTUSEINDEX is not set in AVIMainHeader!") );

	// Ist AVIINDEX_LIST gesetzt, dann müssen auch REC-Frames im movi vorkommen und die Anzahl gleich sein, wie ist das aber auf die Streams verteilt?
	// SegmentCount == 0 && !m_pAVIDesc->m_bExtHeader...

	bool bFlagVids = false, bFlagAuds = false;
	for ( size_t i = 0; i < m_pAVIDesc->m_vpAVIStreamHeader.size(); ++i ) {
		const AVIStreamHeader *pash = m_pAVIDesc->m_vpAVIStreamHeader[i];
		s = _T("AVIStreamHeader #") + ToString( (WORD)i ) + _T(" ") + FCCToString( pash->m_AVIStrhChunk.fccType );
		int htitmp = OSNodeCreateValue( hti, s );
//		const AVIStreamElement *sf = &pash->m_AVIStrfChunk;
//		if ( ((m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIF_ISINTERLEAVED) == 0) && sh->dwInitialFrames ) // glaub nicht, dass das stimmt...
//			o << _T("AVIMainHeader ISINTERLEAVED Flag not set, but AVIStreamHeader Initial Frames: ") << sh->dwInitialFrames << _T("!") << std::endl;
		if ( pash->nInitialFrames + 1 < m_pAVIDesc->m_vpFrameEntries.size() && pash->nInitialFrames != pash->m_AVIStrhChunk.dwInitialFrames ) {
			s = _T("Initial Frames detected ") + ToString( pash->nInitialFrames ) + _T(", AVIStreamHeader Initial Frames: ") + ToString( pash->m_AVIStrhChunk.dwInitialFrames ) + _T("!");
			OSNodeCreateValue( htitmp, s );
		}

		DWORD nCountIX = pash->GetCounterIXFrames();
		if ( pash->mFOURCCCounter.find(FCC('indx')) != pash->mFOURCCCounter.end() ) {
			if ( nCountIX == 0 )
				OSNodeCreateValue( htitmp, _T("'indx'-Entries detected, but no 'ix##'-Entries!") );
			if ( !m_pAVIDesc->m_bExtHeader ) 
				OSNodeCreateValue( htitmp, _T("'indx'-Entries detected, but ODML-Header not found!") );
		} else {
			if ( nCountIX > 0 ) 
				OSNodeCreateValue( htitmp, _T("No 'indx'-Entries detected, but 'ix##'-Entries!") );
			if ( m_pAVIDesc->m_bExtHeader ) 
				OSNodeCreateValue( htitmp, _T("No 'indx'-Entries detected, but ODML-Header found!") );
		}

		if ( pash->IsVideoStream() ) {
			if ( bFlagVids )
				OSNodeCreateValue( htitmp, _T("More than one 'vids'/'iavs'-Stream detected!") );
			bFlagVids = true;
			if ( pash->m_AVIStrhChunk.fccType == FCC('iavs') )
				bFlagAuds = true; // Bereits im Video enthalten...
			DWORD nVideoFrames = pash->GetCounterFrames();
			if ( Settings.nFastmodeCount == 0 ) {
				s = _T("Chunks detected, 'Idx1'-Index (KeyFrames) and ODML-Index Entries");
				int hti3 = OSNodeCreateValue( htitmp, s );
				AVIStreamHeader::MCOUNTER::const_iterator it = pash->mFOURCCCounter.begin();
				if ( it == pash->mFOURCCCounter.end() )
					OSNodeCreateValue( hti3, _T("Detected no chunks at all for this stream!") );
				for ( ; it != pash->mFOURCCCounter.end(); ++it ) {
					s = FCCToString( it->first ) + _T(": ") + ToString( it->second.frames ) + _T(" (Dropped: ") + ToString( it->second.droppedframes ) + _T("), 'Idx1': ") + ToString( it->second.oldindex ) + _T(" (Key: ") + ToString( it->second.keyframes ) + _T("), ODML: ") + ToString( it->second.odmlindex );
					OSNodeCreateValue( hti3, s );
				}

//				OSNodeCreateValue( hti3, _T("Stream Payload Total Size in Bytes: ") + ToString( pash->nStreamSize ) ); only countet for 'auds'
				if ( !m_pAVIDesc->m_bExtHeader && nVideoFrames != m_pAVIDesc->m_AVIMainHeader.dwTotalFrames ) {
					s = _T("Video Frames detected: ") + ToString( nVideoFrames ) + _T(" AVIMainHeader Frames: ") + ToString( m_pAVIDesc->m_AVIMainHeader.dwTotalFrames ) + _T("!");
					OSNodeCreateValue( htitmp, s );
				}
				if ( m_pAVIDesc->m_bExtHeader && nVideoFrames != m_pAVIDesc->m_AVIExtHeader.dwGrandFrames ) {
					s = _T("Video Frames detected: ") + ToString( nVideoFrames ) + _T(" AVIExtHeader Frames: ") + ToString( m_pAVIDesc->m_AVIExtHeader.dwGrandFrames ) + _T("!");
					OSNodeCreateValue( htitmp, s );
				}
				// TODO wieder aktivieren
//				if ( pash->nCountDB == 0 && ((m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_TRUSTCKTYPE)) )
//					OSNodeCreateValue( htitmp, _T("No KeyFrames detected, but TRUSTCKTYPE set in AVIMainHeader!") );
//				if ( pash->nCountOldIndexDB + pash->nCountOldIndexDC && ((m_pAVIDesc->m_AVIMainHeader.dwFlags & (AVIMAINHEADER::AVIF_HASINDEX | AVIMAINHEADER::AVIF_MUSTUSEINDEX)) == 0) )
//					OSNodeCreateValue( htitmp, _T("Video Index Frames detected, but HAS_INDEX not set in AVIMainHeader!") );
//				if ( Settings.bCheckIndex && (pash->nCountOldIndexDB + pash->nCountOldIndexDC == 0 && ((m_pAVIDesc->m_AVIMainHeader.dwFlags & (AVIMAINHEADER::AVIF_HASINDEX | AVIMAINHEADER::AVIF_MUSTUSEINDEX)) != 0)) )
//					OSNodeCreateValue( htitmp, _T("No Video Index Frames detected, but HAS_INDEX or MUSTUSEINDEX set in AVIMainHeader!") );

				if ( m_bPaletteChange && (pash->m_AVIStrhChunk.dwFlags & AVISTREAMHEADER::AVISF_VIDEO_PALCHANGES) == 0 )
					OSNodeCreateValue( htitmp, _T("Stream contains AVIPaletteChanges, but AVIStreamHeader-Flag is not set!") );
				else if ( !m_bPaletteChange && (pash->m_AVIStrhChunk.dwFlags & AVISTREAMHEADER::AVISF_VIDEO_PALCHANGES) )
					OSNodeCreateValue( htitmp, _T("Stream contains no AVIPaletteChanges, but AVIStreamHeader-Flag is set!") );
				if ( m_bPaletteChange && pash->m_AVIStrfChunk.data.pbh->biBitCount > 8 )
					OSNodeCreateValue( htitmp, _T("Stream contains AVIPaletteChanges, but there are more than 8-bit-colors in AVIStreamFormat-Header set!") );
				if ( nVideoFrames != pash->m_AVIStrhChunk.dwLength ) {
					s = _T("Video Frames detected: ") + ToString( nVideoFrames ) + _T(" AVIStreamHeader Length: ") + ToString( pash->m_AVIStrhChunk.dwLength ) + _T("!");
					OSNodeCreateValue( htitmp, s );
				}
			}
		}
		else if ( pash->m_AVIStrhChunk.fccType == FCC('auds') ) {
			// const WAVEFORMATEX *wf = sf->data.pwf;
			if ( bFlagAuds )
				OSNodeCreateValue( htitmp, _T("More than one 'auds'-Stream detected!") );
			bFlagAuds = true;
			if ( Settings.nFastmodeCount == 0 ) {
				s = _T("Chunks detected, 'Idx1'-Index (KeyFrames) and ODML-Index Entries");
				int hti3 = OSNodeCreateValue( htitmp, s );
				AVIStreamHeader::MCOUNTER::const_iterator it = pash->mFOURCCCounter.begin();
				if ( it == pash->mFOURCCCounter.end() )
					OSNodeCreateValue( hti3, _T("Detected no chunks at all for this stream!") );
				for ( ; it != pash->mFOURCCCounter.end(); ++it ) {
				// *** Info: 'ix00'-Chunks detected: xxxxxx, 'Idx1' Entries: xxxxxx (Key: xxxxxx), ODML-Index En
					s = FCCToString( it->first ) + _T(": ") + ToString( it->second.frames ) + _T(" (Dropped: ") + ToString( it->second.droppedframes ) + _T("), 'Idx1': ") + ToString( it->second.oldindex ) + _T(" (Key: ") + ToString( it->second.keyframes ) + _T("), ODML: ") + ToString( it->second.odmlindex );
					OSNodeCreateValue( hti3, s );
				}

				hti3 = OSNodeCreateValue( htitmp, _T("'auds'-Stream Payload Total Size in Bytes: ") + ToString( pash->nStreamSize ) );
				if ( pash->m_AVIStrhChunk.dwScale != 0 && pash->nStreamSize / pash->m_AVIStrhChunk.dwScale != pash->m_AVIStrhChunk.dwLength ) {
					s = _T("Mismatching 'strh' Length: ") + ToString( pash->m_AVIStrhChunk.dwLength ) + _T(" * Scale: ") + ToString( pash->m_AVIStrhChunk.dwScale ) + _T(" = ") + ToString( pash->m_AVIStrhChunk.dwLength * pash->m_AVIStrhChunk.dwScale ) + _T("!");
					OSNodeCreateValue( hti3, s );
				}
				double dLen = pash->GetLengthMSec() / 1000.0;
				if ( dLen < 1.0 ) dLen = 1.0;
				if ( std::abs((__int64)pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec - (DWORD)(pash->nStreamSize / dLen + 1) ) > 10 ) {
					s = _T("Avg Bytes/sec: ") + ToString( (DWORD)(pash->nStreamSize / dLen + 1) ) + _T(", StreamHeader Avg Bytes/sec: ") + ToString( pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec ) + _T("!");
					OSNodeCreateValue( htitmp, s );
				}

	//			if ( m_framesaudio != sh->dwLength )
	//				o << _T("Audio Frames detected: ") << m_framesaudio << _T(" AVIStreamHeader Length: ") << sh->dwLength << std::endl;
			}
			// Global den DV-Typ speichern und dann hier abfragen bze. umgekehrt auch...
//			if ( pash->fccHandler == FCC('dvsd' ) && pash->fccType == FCC('iavs' ) )
//				o << _T("DV Type 1 may  not have 'auds' stream!") << std::endl;
		} else if ( pash->m_AVIStrhChunk.fccType == FCC('txts') || pash->m_AVIStrhChunk.fccType == FCC('mids') ) {
			if ( Settings.nFastmodeCount == 0 ) {
				s = _T("Chunks detected, 'Idx1'-Index (KeyFrames) and ODML-Index Entries");
				int hti3 = OSNodeCreateValue( htitmp, s );
				AVIStreamHeader::MCOUNTER::const_iterator it = pash->mFOURCCCounter.begin();
				if ( it == pash->mFOURCCCounter.end() )
					OSNodeCreateValue( hti3, _T("Detected no chunks at all for this stream!") );
				for ( ; it != pash->mFOURCCCounter.end(); ++it ) {
					s = FCCToString( it->first ) + _T(": ") + ToString( it->second.frames ) + _T(" (Dropped: ") + ToString( it->second.droppedframes ) + _T("), 'Idx1': ") + ToString( it->second.oldindex ) + _T(" (Key: ") + ToString( it->second.keyframes ) + _T("), ODML: ") + ToString( it->second.odmlindex );
					OSNodeCreateValue( hti3, s );
				}
//				OSNodeCreateValue( htitmp, _T("Stream Payload Total Size in Bytes: ") + ToString( pash->nStreamSize ) ); only counted for auds
			}
			OSNodeCreateValue( htitmp, _T("Nothing else known about 'txts'/'mids' Streams") );
		} else
			OSNodeCreateValue( htitmp, _T("Unknown stream type") );
		if ( pash->m_AVIStrhChunk.dwSuggestedBufferSize < pash->nMaxElementSize || pash->m_AVIStrhChunk.dwSuggestedBufferSize > pash->nMaxElementSize + 16 ) {
			// 0 is ok for dwSuggestedBufferSize
			s = _T("Max Frame Size detected: ") + ToString( pash->nMaxElementSize ) + _T(" SuggestedBufferSize: ") + ToString( pash->m_AVIStrhChunk.dwSuggestedBufferSize ) + _T("!");
			OSNodeCreateValue( htitmp, /*(pash->m_AVIStrhChunk.dwSuggestedBufferSize < pash->nMaxElementSize  ? CIOService::EPRIORITY::EP_ERROR : CIOService::EPRIORITY::EP_WARNING),*/ s );
		}
	}
	if ( !bFlagVids )
		OSNodeCreateValue( hti, _T("No 'vids'-Stream detected!") );
	if ( !bFlagAuds )
		OSNodeCreateValue( hti, _T("No 'auds'-Stream detected!") );
	return true;
}

bool CAVIParser::RIFFCheckAvi_IndxChunk( int hti, const AVIMetaIndex* mi, bool bHeader ) const
{
	if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_INDEXES ) { // SuperIndex
		// FOURCC == 'indx'; ChunkId == id, Header
		if ( !bHeader ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Location of IndexOfIndexes, should be in Header!") );
			return false;
		}
		if ( mi->wLongsPerEntry != 4 ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("LongsPerEntry != 4 for IndexOfIndexes!") );
			return false;
		}
		if ( mi->bIndexSubType != 0 && mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_2FIELD ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexSubType for IndexOfIndexes!") );
			return false;
		}
	} else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_CHUNKS ) { // StandardIndex
		// FOURCC == 'indx' or '##ix'; ChunkId == id
		if ( mi->wLongsPerEntry == 2 ) {
			if ( mi->bIndexSubType != 0 ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexSubType for IndexOfChunks!") );
				return false;
			}
		} else if ( mi->wLongsPerEntry == 3 ) {
			// FOURCC == '##ix'; VideoStream
			if ( mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_2FIELD ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong IndexSubType for IndexOfTimedChunks!") );
				return false;
			}
//			if ( LOFCC(FCC(mi->dwChunkId)) != LOFCC(FCC('##dc')) && LOFCC(FCC(mi->dwChunkId)) != LOFCC(FCC('##db')) && LOFCC(FCC(mi->dwChunkId)) != LOFCC(FCC('##iv')) && LOFCC(FCC(mi->dwChunkId)) != LOFCC(FCC('##IV')) ) {
//				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong FOURCC, should be '##dc' or '##db' for IndexOfDiscontinuedTimecode!") );
//				return false;
//			}
		} else {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexOfChunks!") );
			return false;
		}
	} else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_TIMED_CHUNKS ) { // TimedIndex
		if ( bHeader ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Location of IndexOfTimedChunks, should be in 'movi'!") );
			return false;
		}
		// FOURCC == 'indx' or '##ix'; ChunkId == id
		if ( mi->wLongsPerEntry != 3 ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("LongsPerEntry != 3 for IndexOfTimedChunks!") );
			return false;
		}
		if ( mi->bIndexSubType != 0 ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexSubType for IndexOfTimedChunks!") );
			return false;
		}
	} else if ( mi->bIndexType == AVIMetaIndex::AVI_INDEX_IS_DATA ) { // TimecodeIndex, DiscontinuedTimecode
		if ( bHeader ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Location of IndexOfTimecode, should be in 'movi'!") );
			return false;
		}
		// FOURCC == 'indx' or '##ix'; ChunkId == 'xxxx'
		if ( mi->wLongsPerEntry == 4 ) {
			if ( mi->bIndexSubType != 0 ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexSubType for IndexOfTimecode!") );
				return false;
			}
			if ( FCC(mi->dwChunkId) != FCC('time') ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong FOURCC, should be 'time' for IndexOfTimecode!") );
				return false;
			}
		} else if ( mi->wLongsPerEntry == 7 ) {
			if ( mi->bIndexSubType != 0 ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexSubType for IndexOfDiscontinuedTimecode!") );
				return false;
			}
			if ( FCC(mi->dwChunkId) != FCC('tcdl') ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong FOURCC, should be 'time' for IndexOfDiscontinuedTimecode!") );
				return false;
			}
		} else {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong IndexOfTimecode!") );
			return false;
		}
	}
	return true;
}



inline DWORD DVPOS( BYTE s, BYTE b ) /*const*/ { return ((WORD)s * 150u * 80u + (WORD)b * 80u); } // Sequence, Block
bool CAVIParser::ExtractAudioFromDVFrame( int hti, DWORD nFrameSize, BYTE** ppAudio, DWORD& nAudioSize )
{
	WORD nSequenceLength;
	if ( nFrameSize == 144000 ) // PAL
		nSequenceLength = 12;
	else if ( nFrameSize == 120000 ) // NTSC
		nSequenceLength = 10;
	else 
		return false;

	nAudioSize = nSequenceLength * 9u * 76u;
	BYTE* pFrame = new BYTE[nFrameSize]; 
	*ppAudio = new BYTE[nAudioSize];

	// struct DIFBlock { BYTE id[3], h, payload[76]; };
	ReadBytes( pFrame, nFrameSize, hti );
	WORD n = 0;
	for ( BYTE i = 0; i < nSequenceLength; ++i ) {
		for ( BYTE j = 0; j < 150; ++j ) {
			if ( j == 6 || ((j - 6) % 16) == 0 ) {
				const BYTE* as = pFrame + DVPOS(i, j);
				ASSERT( (*as & 0xf0) == 0x70 ); // TODO 
//				if ( as[3] == 0x50 || as[3] == 0x51 ) {
				if ( as[3] == 0x50 ) {
					memcpy( *ppAudio + n * 76, as + 4, 76 );
					++n;
				}
//				_tcout << std::endl << i << _T(", ") << j;
//				DumpBytes( p + i * 150 * 80 + j * 80, 80, hti );
			}
		}
	}
	delete[] pFrame;
	nAudioSize = n * 76u;
	return true;
}


bool CAVIParser::Sample_DVPayload( int hti, DVFRAMEINFO* pi, DWORD nFrameSize, const BYTE* pFrame ) const
{
	WORD nSequenceLength;
	if ( nFrameSize == 144000 ) // PAL
		nSequenceLength = 12;
	else if ( nFrameSize == 120000 ) // NTSC
		nSequenceLength = 10;
	else {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Frame size is not DV compliant (144000/120000)") );
		return false;
	}

	bool b1 = false, b2 = false, b3 = false, b4 = false, b5 = false;

	struct DIFBlock { BYTE id[3], h, payload[76]; };
	for ( BYTE s = 0; s < nSequenceLength; ++s ) {
		for ( BYTE b = 0; b < 150; ++b ) {
			if ( b1 && b2 && b3 && b4 && b5 )
				return true;
			const DIFBlock* p = reinterpret_cast<const DIFBlock*>( pFrame + DVPOS(s, b) );
			ASSERT( DVPOS(s, b) < nFrameSize );
			if ( (p->id[0] & 0xf0) == 0x70 && p->h == 0x50 && p->payload[2] != 0xff && !b5 ) { // AAUX
				if ( Settings.nVerbosity >= 5 ) {
					int htitmp = OSNodeCreate( hti, DVPOS(s, b), _T("SeqNo: ") + ToString( (int)s ) + _T(", BlockNo: ") + ToString( (int)b ) );
					DumpBytes( p, 80, htitmp );
				}
				pi->aaux_as_pc4.b = p->payload[3];
				b5 = true;
			}
#if 0
				switch(aaux_as_pc4 & 0x38) {
				case 0x00:
					samplingRate = 48000;
					mSamplesPerSet		= isPAL ? 19200 : 16016;
					mMinimumFrameSize	= isPAL ? 1896 : 1580;
					break;
				case 0x08:
					samplingRate = 44100;
					mSamplesPerSet		= isPAL ? 17640 : 14715;
					mMinimumFrameSize	= isPAL ? 1742 : 1452;
					break;
				case 0x10:
					samplingRate = 32000;
					mSamplesPerSet		= isPAL ? 12800 : 10677;
					mMinimumFrameSize	= isPAL ? 1264 : 1053;
					break;
				}
#endif
			if ( p->id[0] == 0x1f && !b4 ) {
				if ( Settings.nVerbosity >= 5 ) {
					int htitmp = OSNodeCreate( hti, DVPOS(s, b), _T("SeqNo: ") + ToString( (int)s ) + _T(", BlockNo: ") + ToString( (int)b ) );
					DumpBytes( p, 80, htitmp );
				}
				pi->bPAL = (p->h == 0xbf);
//				else if ( p->h == 0x3f )
//					o << _T("NTSC ");
//				else
//					o << _T("??? ");
				b4 = true;
			}
			if ( p->id[0] == 0x3f && !b1 ) {
				if ( Settings.nVerbosity >= 5 ) {
					int htitmp = OSNodeCreate( hti, DVPOS(s, b), _T("SeqNo: ") + ToString( (int)s ) + _T(", BlockNo: ") + ToString( (int)b ) );
					DumpBytes( p, 80, htitmp );
				}

				int i = 2; // skip 2 bytes
				if ( p->payload[i] == 0x13 && p->payload[i+1] != 0xff && !b1 ) {
					_tstringstream o;
					o.fill( '0' ); o.flags( std::ios::right );
					o << std::setw(2) << BCD2Int((BYTE)(p->payload[i+4] & 0x3f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+3] & 0x7f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+2] & 0x7f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+1] & 0x3f)) << std::ends;
					pi->sTimecode = o.str();
					b1 = true;
					i += 5;
					if ( p->payload[i] == 0x62 ) {
						// skip one
						int year = p->payload[i+4] & 0x3f;
						if ( year < 25 ) year += 2000; else year += 1900;
						_tstringstream o;
						o.fill( '0' ); o.flags( std::ios::right );
						o << year << _T("-") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+3] & 0x1f)) << _T("-") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+2] & 0x3f)) << std::ends;
						pi->sCreatedDate = o.str();
						i += 5;
						if ( p->payload[i] == 0x63 ) {
							// skip one
							_tstringstream o;
							o.fill( '0' ); o.flags( std::ios::right );
							o << std::setw(2) << BCD2Int((BYTE)(p->payload[i+4] & 0x3f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+3] & 0x7f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+2] & 0x7f)) << std::ends;
							pi->sCreatedTime = o.str();
						}
						b3 = true;
					}
				}

			}
			else if ( (p->id[0] & 0xf0) == 0x50 && !(b2 && b3) ) {
				if ( Settings.nVerbosity >= 5 ) {
					int htitmp = OSNodeCreate( hti, DVPOS(s, b), _T("SeqNo: ") + ToString( (int)s ) + _T(", BlockNo: ") + ToString( (int)b ) );
					DumpBytes( p, 80, htitmp );
				}

				int i = 44;
				if ( p->payload[i] == 0x60 && !b4 ) {
					pi->bPAL = (p->payload[i+3] == 0x20);
					b4 = true;
				}
				i += 5;
				if ( p->payload[i] == 0x61 && p->payload[i+1] != 0xff && !b2 ) {
					pi->b16to9 = ((p->payload[i+2] & 0x02) != 0);
					b2 = true;
				}
				i += 5;
				if ( p->payload[i] == 0x62 && !b3 ) {
					// skip one
					int year = p->payload[i+4] & 0x3f;
					if ( year < 25 ) year += 2000; else year += 1900;
					_tstringstream o;
					o.fill( '0' ); o.flags( std::ios::right );
					o << year << _T("-") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+3] & 0x1f)) << _T("-") << std::setw(2) << BCD2Int(BYTE(p->payload[i+2] & 0x3f)) << std::ends;
					pi->sCreatedDate = o.str();
					i += 5;
					if ( p->payload[i] == 0x63 ) {
						// skip one
						_tstringstream o;
						o.fill( '0' ); o.flags( std::ios::right );
						o << std::setw(2) << BCD2Int((BYTE)(p->payload[i+4] & 0x3f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+3] & 0x7f)) << _T(":") << std::setw(2) << BCD2Int((BYTE)(p->payload[i+2] & 0x7f)) << std::ends;
						pi->sCreatedTime = o.str();
					}
					b3 = true;
				}
			}
		}
	}
	return true;
}

void CAVIParser::RIFFDumpAvi_DVPayload( int hti, DWORD nFrameSize, const BYTE* pFrame ) const
{
	DVFRAMEINFO dvfi;
	if ( !Sample_DVPayload( hti, &dvfi, nFrameSize, pFrame ) )
		return;

	_tstring o = _T("DV: ");

	o += (dvfi.bPAL ? _T("PAL ") : _T("NTSC "));
	o += (dvfi.b16to9 ? _T("16:9 ") : _T("4:3 "));
	switch ( dvfi.aaux_as_pc4.smp ) {
	case 0x00:
		o += _T("48kHz ");
		break;
	case 0x01:
		o += _T("44.1kHz ");
		break;
	case 0x02:
		o += _T("32kHz ");
		break;
	}
	if ( dvfi.sTimecode.length() > 0 ) {
		o += _T(" TC: ");
		o += dvfi.sTimecode.c_str(); // keine Ahnung, warum es ohne c_str() nicht geht...
	}
	if ( dvfi.sCreatedDate.length() > 0 ) {
		o += _T(" Date: ");
		o += dvfi.sCreatedDate.c_str(); // keine Ahnung, warum es ohne c_str() nicht geht...
	}
	if ( dvfi.sCreatedTime.length() > 0 ) {
		o += _T(" Time: ");
		o += dvfi.sCreatedTime.c_str(); // keine Ahnung, warum es ohne c_str() nicht geht...
	}

	OSNodeCreateValue( hti, o );
}

const FOURCC CAVIParser::Streams::fccOrder[] = { FCC('iavs'), FCC('vids'), FCC('auds'), FCC('mids'), FCC('txts'), FCC('subs') };

bool CAVIParser::Streams::Create( AVIStreamHeader* pash )
{
	pash->nSrcStreamNr = (WORD)m_pAVIDesc->m_vpAVIStreamHeader.size();
	m_mmStreams[pash->m_AVIStrhChunk.fccType][(WORD)m_pAVIDesc->m_vpAVIStreamHeader.size()] = pash;
	m_pAVIDesc->m_vpAVIStreamHeader.push_back( pash );
	return true;
}

bool CAVIParser::Streams::Renumber()
{
	WORD nStreamNr = 0;
	for ( WORD i = 0; i < sizeof( fccOrder ) / sizeof( fccOrder[0] ); ++i ) {
		for ( mStreamNrAVIStreamHeader::iterator pI = m_mmStreams[fccOrder[i]].begin(); pI != m_mmStreams[fccOrder[i]].end(); ++pI ) {
			if ( pI->second->nSrcStreamNr != nStreamNr ) {
				for ( WORD j = 0; j < m_pAVIDesc->m_vpAVIStreamHeader.size(); ++j ) { // Wenn der Zielstream bereits existiert, diesen umbenennen
					if ( m_pAVIDesc->m_vpAVIStreamHeader[j]->nSrcStreamNr == nStreamNr ) {
						for_each(m_pAVIDesc->m_vpFrameEntries.begin(), m_pAVIDesc->m_vpFrameEntries.end(), [&](AVIFrameEntry* x) { if (HIFCC_NUM(x->m_fcc) == nStreamNr) SETHIFCC_NUM(x->m_fcc, (WORD)(99u - j)); });
						//=for_each(m_pAVIDesc->m_vpFrameEntries.begin(), m_pAVIDesc->m_vpFrameEntries.end(), std::bind2nd(std::mem_fn(&AVIFrameEntry::RenumberStream), std::pair<WORD, WORD>(nStreamNr, (WORD) (99u - j))));
						m_pAVIDesc->m_vpAVIStreamHeader[j]->nSrcStreamNr = 99u - j;
						break;
					}
				}
				for_each( m_pAVIDesc->m_vpFrameEntries.begin(), m_pAVIDesc->m_vpFrameEntries.end(), [&](auto x) { if (HIFCC_NUM(x->m_fcc) == pI->second->nSrcStreamNr) SETHIFCC_NUM(x->m_fcc, nStreamNr); });
				//=for_each(m_pAVIDesc->m_vpFrameEntries.begin(), m_pAVIDesc->m_vpFrameEntries.end(), std::bind2nd(std::mem_fn(&AVIFrameEntry::RenumberStream), std::pair<WORD, WORD>(pI->second->nSrcStreamNr, nStreamNr)));
				pI->second->nSrcStreamNr = 80;
			}
			++nStreamNr;
		}
	}

	// Und jetzt noch die Streamheader umhängen
	WORD j = 0; 
	for ( WORD i = 0; i < sizeof( fccOrder ) / sizeof( fccOrder[0] ); ++i ) {
		for ( mStreamNrAVIStreamHeader::const_iterator pI = m_mmStreams[fccOrder[i]].begin(); pI != m_mmStreams[fccOrder[i]].end(); ++pI ) {
			m_pAVIDesc->m_vpAVIStreamHeader[j++] = pI->second;
		}
	}

	return true;
}

bool CAVIParser::Streams::Erase( int hti )
{
	// OPTIM 
	// TODO delete frameentry->m_pData??? noch nötig, destruktor existiert

	std::set<WORD>::const_reverse_iterator rit;
	for ( rit = m_pParser->Settings.vStripStreams.rbegin(); rit != m_pParser->Settings.vStripStreams.rend(); ++rit ) {
		mmFCCTypeStreamNrAVIStreamHeader::iterator it;
		for ( it = m_mmStreams.begin(); it != m_mmStreams.end(); ++it ) {
			if ( it->second.find( *rit ) != it->second.end() ) {
				it->second.erase( *rit ); 
			}
		}

		// elegant aber funktioniert aber nicht ohne mem-leak
//		m_pAVIDesc->m_vpFrameEntries.erase( remove_if( m_pAVIDesc->m_vpFrameEntries.begin(), m_pAVIDesc->m_vpFrameEntries.end(), std::bind2nd(std::mem_fun(&AVIFrameEntry::IsStreamEqual), *rit) ), m_pAVIDesc->m_vpFrameEntries.end() );
		for ( int i = (int)m_pAVIDesc->m_vpFrameEntries.size() - 1; i >= 0; i-- ) { 
			if ( m_pAVIDesc->m_vpFrameEntries[i]->IsStreamEqual( *rit ) ) {
				delete m_pAVIDesc->m_vpFrameEntries[i];
				m_pAVIDesc->m_vpFrameEntries.erase( m_pAVIDesc->m_vpFrameEntries.begin() + i );
			}
		}

		if ( *rit < m_pAVIDesc->m_vpAVIStreamHeader.size() ) {
			m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_INFO, _T("Erasing Stream #") + ToString( *rit ) );
			delete m_pAVIDesc->m_vpAVIStreamHeader[*rit];
			m_pAVIDesc->m_vpAVIStreamHeader.erase( m_pAVIDesc->m_vpAVIStreamHeader.begin() + *rit );
		}
	}
	return true;
}



void CAVIParser::CalcVectors()
{
	DWORD nCount1 = 0, nCount2 = 0, nCount3 = 0;
	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) {
		if ( m_pAVIDesc->m_vpAVIStreamHeader[i]->m_AVIStrhChunk.fccType != FCC('auds') )
			nCount1 += m_pAVIDesc->m_vpAVIStreamHeader[i]->m_AVIStrhChunk.dwLength;
	}
	nCount2 = m_pAVIDesc->m_AVIMainHeader.dwTotalFrames;
	if ( m_pAVIDesc->m_bExtHeader)
		nCount3 = m_pAVIDesc->m_AVIExtHeader.dwGrandFrames;
	if ( nCount1 & 0x80000000 ) nCount1 = 0;
	if ( nCount2 & 0x80000000 ) nCount2 = 0;
	if ( nCount3 & 0x80000000 ) nCount3 = 0;
	DWORD nCount = std::max( std::max( nCount1, nCount2 ), nCount3 );
	m_pAVIDesc->m_vpFrameEntries.reserve( nCount * 2 ); // room for audio frames...
}

void CAVIParser::DumpWriteIndexEntries( int hti, const vAVIINDEXENTRIES *ie, DWORD nIndex ) const
{
	for ( DWORD i = nIndex > 1 ? nIndex - 1 : 0 ; i < ie->size() && i <= nIndex + 1; ++i ) {
		DumpWriteIndexEntry( hti, ie->at(i), i, false );
	}
}

void CAVIParser::DumpWriteIndexEntry( int hti, const AVIIndexEntry* ie, DWORD nIndex, bool bMessage ) const
{
	_tstring s;
	int htitmp = OSNodeCreate( hti, 0, _T("Index Entry #") + ToString( nIndex ) );
#ifdef _DEBUG
	s = _T("assigned: ") + ToString( ie->m_nAssigned ) + _T(", flags: 0x") + ToHexString( ie->m_nFlags ) + _T(", idx1: ") + ToString( ie->m_bIdx1 );
#else
	s = _T("assigned: ") + ToString( ie->m_nAssigned ) + _T(", flags: 0x") + ToHexString( ie->m_nFlags );
#endif
	OSNodeCreateValue( htitmp, s );
	s = _T("Segment: ") + ToString( ie->m_nSegment );
	OSNodeCreateValue( htitmp, s );
	s = _T("Offset: ") + ToString( ie->m_nOffset );
	OSNodeCreateValue( htitmp, s );
	s = _T("Size: ") + ToString( ie->m_nSize );
	OSNodeCreateValue( htitmp, s );
	s = _T("Position: ") + ToString( ie->m_nMoviFilePos );
	OSNodeCreateValue( htitmp, s );
	if ( bMessage ) {
		if ( ie->m_nAssigned == 0 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Entry not assigned!") );
		else if ( ie->m_nAssigned > 1 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Entry assigned more than once!") );
		if ( ie->m_sMessage.length() > 0 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, ie->m_sMessage.c_str() );
	}
}

// muss nach jedem idx1 und movi-Block aufgerufen werden...
bool CAVIParser::AssignFramesIndex( int hti )
{
	if ( !Settings.bCheckIndex )
		return true;
	hti = OSNodeCreateValue( hti, _T("Assign Index to 'movi'-Entries") );
	OSStatsSet(CIOService::EPARSERSTATE::PS_INDEXCHECK );

	// 1. Test if all Index Entries are ordered sequence
	mvAVIINDEXENTRIES::const_iterator pI;
	for ( pI = m_pAVIDesc->m_mvpIndexEntries.begin(); pI != m_pAVIDesc->m_mvpIndexEntries.end(); ++pI ) {
		const vAVIINDEXENTRIES *pv = pI->second;
		if ( Settings.nVerbosity >= 3 ) 
			OSNodeCreateValue( hti, FCCToString( pI->first ) + _T(": ") + ToString( (DWORD)pv->size() ) + _T(" Index Entries") );
		if ( pv->size() ) {
			DWORD nOffset = pv->at( 0 )->m_nOffset;
			for ( DWORD j = 1; j < pv->size(); ++j ) {
				if ( pv->at( j )->m_nOffset <= nOffset ) {
					m_pAVIDesc->m_bMustUseIndex = true;
					int htitmp = OSNodeCreateAlert( hti, (m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_MUSTUSEINDEX) ? CIOService::EPRIORITY::EP_INFO : CIOService::EPRIORITY::EP_ERROR, _T("Index Sequence out of order (") + FCCToString( pI->first ) + _T(")!") );
					DumpWriteIndexEntries( htitmp, pv, j );
					break;
				}
				nOffset = pv->at( j )->m_nOffset;
			}
		}
	}

#ifdef _DEBUG
	pI = m_pAVIDesc->m_mvpIndexEntries.begin();
#pragma omp parallel for 
	for ( int i = 0; i < m_pAVIDesc->m_mvpIndexEntries.size(); i++ ) {
		CheckIndexEntry( pI++ );
#else
	for ( pI = m_pAVIDesc->m_mvpIndexEntries.begin(); pI != m_pAVIDesc->m_mvpIndexEntries.end(); ++pI ) {
		CheckIndexEntry( pI );
#endif
	}

	// Macht in der Parallel-Schleife Probleme
	for ( pI = m_pAVIDesc->m_mvpIndexEntries.begin(); pI != m_pAVIDesc->m_mvpIndexEntries.end(); ++pI ) {
		vAVIINDEXENTRIES* pv = pI->second;
		//DWORD nStart = 0;
		DWORD j;

		bool b = false;
		for ( j = 0; j < pv->size(); ++j ) {
			const AVIIndexEntry* ie = pv->at( j );
			if ( ie->m_nAssigned != 1 || ie->m_sMessage.length() > 0 ) {
				b = true;
				break;
			}
		}
		if ( b ) {
			int htitmp = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Not all Index Entries matched a 'movi' Entry (") + FCCToString( pI->first ) + _T(")!") );
			if ( Settings.nVerbosity >= 3 ) {
				DWORD nCount = 0;
				for ( ; j < pv->size() && nCount < std::max( Settings.nTraceIdx1Entries, Settings.nTraceIndxEntries ); ++j ) {
					const AVIIndexEntry* ie = pv->at( j );
					if ( ie->m_nAssigned == 1 && ie->m_sMessage.length() == 0 )
						continue;
					DumpWriteIndexEntry( htitmp, ie, j + 1, true );
					++nCount;
				}
			}
		}
		for ( int i = (int)pI->second->size() - 1; i >= 0; i-- ) {
			delete pI->second->at( (size_t)i );
		}
		pI->second->clear();
		delete pI->second;
	}

	m_pAVIDesc->m_mvpIndexEntries.clear();
	OSStatsSet(CIOService::EPARSERSTATE::PS_PARSING );
	return true;
}


void CAVIParser::CheckIndexEntry( mvAVIINDEXENTRIES::const_iterator pI )
{
	vAVIINDEXENTRIES* pv = pI->second;
	DWORD nStart = 0;
	DWORD j;
	for ( j = 0; j < pv->size(); ++j ) {
		AVIIndexEntry* ie = pv->at( j );
		OSStatsSet( GetReadPos() );
		if ( ie->m_nOffset == 0 && ie->m_nSize == 0 ) { // TODO Dropped frame?
			ie->m_nAssigned++;
			continue;
		}
		if ( m_pAVIDesc->m_bMustUseIndex || (m_pAVIDesc->m_AVIMainHeader.dwFlags & AVIMAINHEADER::AVIF_MUSTUSEINDEX) != 0 ) // Dann muss man leider immer von vorne zu suchen anfangen...
			nStart = 0;
		for ( DWORD i = nStart; i < m_pAVIDesc->m_vpFrameEntries.size(); ++i ) { 
			AVIFrameEntry* fe = m_pAVIDesc->m_vpFrameEntries[i]; // Frame entries are ordered, index entries possibly not
			if ( fe->m_nSegment < ie->m_nSegment )
				continue;
			if ( fe->m_nSegment > ie->m_nSegment || ie->m_nOffset < fe->m_nOffset ) 
				break;
			ASSERT( m_pAVIDesc->m_bMustUseIndex || (std::abs( (__int64)fe->m_nOffset - ie->m_nOffset ) >= 16 || (fe->m_nOffset + 8u == ie->m_nOffset || fe->m_nOffset == ie->m_nOffset)) );
			if ( ie->m_nOffset > fe->m_nOffset && ie->m_nOffset < fe->m_nOffset + 8 ) {
				ie->m_sMessage = _T("Entry Offset (") + ToString( ie->m_nOffset ) + _T(") misaligned to 'movi' Offset (") + ToString( fe->m_nOffset ) + _T(")!");
				ie->m_nOffset = fe->m_nOffset;
			}
			// Matched der Index ein Frame?
			// fe->m_nOffset == ie->m_nOffset ist nicht korrekt, kommt aber vor für ix## ...
			if ( fe->m_nOffset + 8 == ie->m_nOffset || fe->m_nOffset == ie->m_nOffset ) {
				fe->m_nFlags = ie->m_nFlags;
				ie->m_nAssigned++;
				nStart = i + 1;
				if ( fe->m_fcc != pI->first )
					ie->m_sMessage = _T("Entry Type (") + FCCToString(pI->first) + _T(") mismatches 'movi' Type (") + FCCToString(fe->m_fcc) + _T(")!");
				if ( fe->m_nSize != ie->m_nSize && !m_pAVIDesc->m_bIntraFrameIndex )
					ie->m_sMessage = _T("Entry Size (") + ToString( ie->m_nSize ) + _T(") mismatches 'movi' Size (") + ToString( fe->m_nSize ) + _T(")!");
				break;
			} 
			// Zeigt der Index mitten in ein Frame und passt auch die Länge?
			if ( fe->m_nOffset + 8 < ie->m_nOffset && ie->m_nOffset + ie->m_nSize - 8 <= fe->m_nOffset + fe->m_nSize && fe->m_fcc == pI->first ) {
				m_pAVIDesc->m_bIntraFrameIndex = true; // low-overhead...
//					ASSERT( fe->m_nFlags == ie->m_nFlags );
				ie->m_nAssigned++;
				nStart = i;
				break;
			} 
			// Andernfalls nur Fehler, wenn Index ordered ist
			if ( fe->m_nOffset + fe->m_nSize > ie->m_nOffset && fe->m_fcc == pI->first ) {
				nStart = i;
				ie->m_sMessage = _T("Entry Pos/Size (") + ToString( ie->m_nMoviFilePos ) + _T("/") + ToString( ie->m_nSize ) + _T(") mismatches 'movi' Pos/Size (") + ToString( fe->m_nFilePos ) + _T("/") + ToString( fe->m_nSize ) + _T(")!");
				break;
			}
		}
	}
}

// Duplicate in CRIFFSpecParser and AVIParser
void CAVIParser::RIFFDumpAvi_StrfAudsHeader( int hti, const WAVEFORMATEX* wf, DWORD nSize, const AVISTREAMHEADER* pash ) const
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
	if ( wf->wFormatTag == WAVE_FORMAT_PCM && pash && (DWORD)(pash->GetFPS() + .5) != wf->nSamplesPerSec ) // Nur bei fester Bitrate relevant???
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Stream Rate / Scale: ") + ToString( pash->GetFPS() ) + _T("!") );
	s = _T("08 Avg Bytes/sec: ") + ToString( wf->nAvgBytesPerSec ); // for buffer estimation
	htitmp = OSNodeCreateValue( hti, s );
	if ( pash ) {
		DWORD t = (DWORD)((double)pash->dwLength / wf->nAvgBytesPerSec * 1000 * wf->nBlockAlign + .5);
		if ( t > 0 && t != pash->GetLengthMSec() )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("AvgBytesPerSec Length ") + DurationToLengthString( t, pash->GetFPS() ) + _T("!") );
	}
	if ( wf->wFormatTag == WAVE_FORMAT_PCM && wf->nAvgBytesPerSec != wf->nSamplesPerSec * wf->nBlockAlign ) // nAvgBytesPerSec should be equal to the product of nSamplesPerSec and nBlockAlign
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("PCM AvgBytesPerSec should be SamplesPerSec * BlockAlign!") );
	s = _T("12 BlockAlign: ") + ToString( wf->nBlockAlign ); // block size of data
	htitmp = OSNodeCreateValue( hti, s );
	if ( wf->nBlockAlign == 0 )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Illegal BlockAlign detected.") );
	if ( wf->wFormatTag == WAVE_FORMAT_MPEGLAYER3 && wf->nBlockAlign >= 960 ) {
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("Presumably variable Bitrate MP3.") );
		if ( pash && pash->dwSampleSize > 0 )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("StreamHeader SampleSize should be 0 for variable Bitrate MP3.") );
	}
	if ( (wf->wFormatTag == WAVE_FORMAT_PCM || wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && wf->nBlockAlign != wf->nChannels * wf->wBitsPerSample / 8 ) // nBlockAlign must be equal to the product of nChannels and wBitsPerSample (aufgerundet auf volle 8/16/24... bits) divided by 8 (bits per byte). 
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("PCM BlockAlign should be Channels * BitsPerSample / 8!") );
	else if ( wf->wBitsPerSample > 0 && wf->nBlockAlign % (wf->nChannels * ((wf->wBitsPerSample + 7) / 8)) != 0 ) // entschärft, ein Vielfachses davon reicht
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("BlockAlign should be a multiple of Channels * BitsPerSample / 8!") );
	if ( pash && pash->dwSampleSize > 0 && pash->dwSampleSize != wf->nBlockAlign ) // this rate should correspond to the audio block size nBlockAlign
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("BlockAlign should equal StreamHeader SampleSize!") );
	// The MS imaadpcm codec sets mux_a->h.dwScale = mux_a->h.dwSampleSize = mux_a->wf->nBlockAlign, which seems to work well for mp3 and ac3 too.
	if ( nSize > sizeof( WAVEFORMAT ) ) {
		s = _T("14 Bits/Sample: ") + ToString( wf->wBitsPerSample ); // number of bits per sample of mono data
		htitmp = OSNodeCreateValue( hti, s );
		if ( wf->wFormatTag == WAVE_FORMAT_PCM && wf->wBitsPerSample != 8 && wf->wBitsPerSample != 16 ) // wBitsPerSample should be equal to 8 or 16
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("PCM BitsPerSamples should be 8 or 16!") );
		else if ( wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wf->wBitsPerSample % 8 != 0 ) // wBitsPerSample should be a multiple of 8 for WAVE_FORMAT_EXTENSIBLE
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Ext. BitsPerSamples should be a multiple of 8!") );
		if ( pash ) {
			DWORD t = (DWORD)((double)pash->dwLength * wf->nBlockAlign * wf->wBitsPerSample / 8 / wf->nAvgBytesPerSec * wf->nChannels * 1000 + .5 );
			if ( t > 0 && t != pash->GetLengthMSec() )
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("BitsPerSample Length ") + DurationToLengthString( t, pash->GetFPS() ) + _T("!") );
		}
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

/*static*/ bool CAVIParser::Probe( BYTE buffer[12] )
{
	FOURCC* fcc = reinterpret_cast<FOURCC*>( buffer ); 
	return ((fcc[0] == FCC('RIFF') || fcc[0] == FCC('RIFX')) && fcc[2] == FCC('AVI '));
}

bool CAVIParser::ParseMP4Frame( int /*hti*/, DWORD nChunkSize )
{
	return ReadSeekFwdDump( nChunkSize );

	// TODO Das Problem hier ist, dass mehr Bytes verbraucht werden, als im Chunk sind...

#if 0


	int htitmp;
	DWORD w;
	DWORD nSize = 0;
	_tstring s;

	if ( nChunkSize < 4 )
		return ReadSeekFwdDump( nChunkSize );

	nSize += 4;
	BYTE* header = reinterpret_cast<BYTE*>( &w );
	if ( !Bitstream.GetBits( header, 4*8 ) )
		return false;
	if ( header[0] || header[1] || (header[2] != 1) || header[3] ) // Start MPEG4 0x00000100
		return ReadSeekFwdDump( nChunkSize - nSize );

	for ( ;; ) {
		nSize += 4;
		if ( nSize > nChunkSize )
			return ReadSeekFwdDump( nChunkSize - (nSize - 4) );
		BYTE* header = reinterpret_cast<BYTE*>( &w );
		if ( !Bitstream.GetBits( header, 4*8 ) )
			return false;
		if ( header[0] || header[1] || (header[2] != 1) ) {
			ReadDumpErrorContext( 0x100, hti );
			Bitstream.FindMarker( true );
			ReadDumpErrorContext( 0x100, hti );
			continue;
		}
		WORD n = Bitstream.FindMarker( false );
		if ( n + nSize > nChunkSize )
			n = nChunkSize - nSize;
		BYTE* b = new BYTE[n];
		switch ( header[3] ) {
		case 0x20: {
			htitmp = OSNodeCreate( hti, GetReadPos(), _T("VideoObjectLayer"), _T("0x20"), n );
			Bitstream.Announce( n * 8 );
			// Bitstream.GetBits<DWORD>( 32 ); // Unknown
			Bitstream.GetAnnouncedBits<WORD>( 9 ); // Unknown
			BYTE ver_id = 1;
			if ( Bitstream.GetAnnouncedBits<bool>( 1 ) ) { // Version
				ver_id = Bitstream.GetAnnouncedBits<BYTE>( 4 );
				OSNodeCreateValue( htitmp, _T("Version: ") + ToString( ver_id ) );
				Bitstream.GetAnnouncedBits<BYTE>( 3 ); // Unknown
			}
			BYTE ar = Bitstream.GetAnnouncedBits<BYTE>( 4 );
			switch ( ar ) {
			case 1:
				s = _T(" (VGA 1:1 Square Pixels)");
				break;
			case 2:
				s = _T(" (PAL 4:3 Pixels)");
				break;
			case 3:
				s = _T(" (NTSC 4:3 Pixels)");
				break;
			case 4:
				s = _T(" (PAL 16:9 Pixels)");
				break;
			case 5:
				s = _T(" (NTSC 16:9 Pixels)");
				break;
			case 15:
				s = _T(" (Custom)");
				break;
			default:
				s = _T(" (Undefined)");
				break;
			}
			OSNodeCreateValue( htitmp, _T("Aspect Ratio: ") + ToString( ar ) + s ); // TODO
			if ( ar == 0x0f ) {
				BYTE w = Bitstream.GetAnnouncedBits<BYTE>( 8 );
				BYTE h = Bitstream.GetAnnouncedBits<BYTE>( 8 );
				OSNodeCreateValue( htitmp, _T("Width: ") + ToString( w ) );
				OSNodeCreateValue( htitmp, _T("Height: ") + ToString( h ) );
			}
			if ( Bitstream.GetAnnouncedBits<bool>( 1 ) ) { // Unknown
				Bitstream.GetAnnouncedBits<BYTE>( 3 ); // Unknown
				if ( Bitstream.GetAnnouncedBits<bool>( 1 ) ) { // Unknown
					Bitstream.GetAnnouncedBits<QWORD>( 64 ); // 79 bits
					Bitstream.GetAnnouncedBits<WORD>( 15 );
				}
			}

			BYTE shape = Bitstream.GetAnnouncedBits<BYTE>( 2 );
			OSNodeCreateValue( htitmp, _T("Shape: ") + ToString( shape ) ); // TODO
			if ( ver_id != 1 && shape == 3 ) // Grayscale
				Bitstream.GetAnnouncedBits<BYTE>( 4 ); // Unknown
			Bitstream.GetAnnouncedBits<BYTE>( 1 ); // Unknown

			WORD time_inc_res = Bitstream.GetAnnouncedBits<WORD>( 16 ); 
			OSNodeCreateValue( htitmp, _T("Time Inc Res: ") + ToString( time_inc_res ) ); 

			WORD time_inc_bits = std::max( BitsUsed( time_inc_res - 1), 1 );
			Bitstream.GetAnnouncedBits<BYTE>( 1 ); // Unknown

			if ( Bitstream.GetAnnouncedBits<bool>() ) {
				if ( time_inc_bits <= sizeof( BYTE ) * 8 )
					Bitstream.GetAnnouncedBits<BYTE>( time_inc_bits ); 
				else if ( time_inc_bits <= sizeof( WORD ) * 8 )
					Bitstream.GetAnnouncedBits<WORD>( time_inc_bits ); 
				else if ( time_inc_bits <= sizeof( DWORD ) * 8 )
					Bitstream.GetAnnouncedBits<DWORD>( time_inc_bits ); 
				else
					Bitstream.GetAnnouncedBits<QWORD>( time_inc_bits ); 
			}

			bool interlaced = false, mpeg_quant = false, quarter_pel = false;
			BYTE sprite_enable;
			if ( shape != 2 ) { // BinaryOnly
				if ( shape == 0 ) // Rectangular
					Bitstream.GetAnnouncedBits<DWORD>( 29 ); 

				interlaced = Bitstream.GetAnnouncedBits<bool>(); 
				OSNodeCreateValue( htitmp, _T("Interlaced: ") + ToString( interlaced ) ); 
				Bitstream.GetAnnouncedBits<bool>();

				sprite_enable = Bitstream.GetAnnouncedBits<BYTE>( ver_id == 1 ? 1 : 2 ); 
				OSNodeCreateValue( htitmp, _T("Sprite Enable: ") + ToString( sprite_enable ) ); 
				if ( sprite_enable != 0 ) { // None
					Bitstream.GetAnnouncedBits<QWORD>( 56 ); 
					BYTE sprite_warping_points = Bitstream.GetAnnouncedBits<BYTE>( 6 ); 
					Bitstream.GetAnnouncedBits<BYTE>( 3 ); 
					if ( sprite_enable == 1 ) // Static
						Bitstream.GetAnnouncedBits<BYTE>( 1 ); 
				}
				if ( ver_id != 1 && shape != 0 ) // Rectangular
					Bitstream.GetAnnouncedBits<BYTE>( 1 ); 
				if ( Bitstream.GetAnnouncedBits<BYTE>( 1 ) )
					Bitstream.GetAnnouncedBits<BYTE>( 8 ); 
				if ( shape == 3 ) // Grayscale
					Bitstream.GetAnnouncedBits<BYTE>( 3 ); 
				mpeg_quant = Bitstream.GetAnnouncedBits<bool>();
				if ( mpeg_quant ) {
					if ( Bitstream.GetAnnouncedBits<BYTE>( 1 ) )
						Bitstream.GetAnnouncedBits<QWORD>( 64 ); 
					if ( Bitstream.GetAnnouncedBits<BYTE>( 1 ) )
						Bitstream.GetAnnouncedBits<QWORD>( 64 ); 
					if ( shape == 3 ) // Grayscale
						; // ERROR Grayscale Matrix not aupported
				}
				if ( ver_id != 1 ) {
					quarter_pel = Bitstream.GetAnnouncedBits<bool>();
					OSNodeCreateValue( htitmp, _T("Quarter Pel: ") + ToString( quarter_pel ) ); 
				}
				OSNodeCreateValue( htitmp, _T("Complexity Estimation: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) ); 
				Bitstream.GetAnnouncedBits<BYTE>( 1 );
				if ( Bitstream.GetAnnouncedBits<BYTE>( 1 ) )
					Bitstream.GetAnnouncedBits<BYTE>( 1 );

				bool newpred_enable = false, reduced_resolution_enable = false;
				if ( ver_id != 1 ) {
					newpred_enable = Bitstream.GetAnnouncedBits<bool>();
					OSNodeCreateValue( htitmp, _T("Newpred Enable: ") + ToString( newpred_enable ) ); 
					if ( newpred_enable )
						Bitstream.GetAnnouncedBits<BYTE>( 3 );
					reduced_resolution_enable = Bitstream.GetAnnouncedBits<bool>();
				}
				if ( Bitstream.GetAnnouncedBits<bool>() )
					; // ERROR Scalability not Suported
				
			} else {
				if ( ver_id != 1 ) {
					if ( Bitstream.GetAnnouncedBits<bool>() )
						; // ERROR Scalability not Suported
				}
				Bitstream.GetAnnouncedBits<BYTE>( 1 );
			}
			Bitstream.SkipAnnounced();
#if 0

			// Check padding (padding should be present even if the data already ends on
			// a byte boundary, but if there's no padding we will just ignore it)
			if ((bits.Remaining < 0) || (bits.Remaining > 8)) {
				throw new Exception("Invalid VOL");
			}
			bits.Copy(bits.Remaining);
#endif
			break; }
		case 0xb2: 
			htitmp = OSNodeCreate( hti, GetReadPos(), _T("UserData"), _T("0xb2"), n );
			Bitstream.GetBits( b, n * 8u );
			DumpBytes( b, n, htitmp );
			break; 
		case 0xb6: 
			htitmp = OSNodeCreate( hti, GetReadPos(), _T("VideoObjectPlane"), _T("0xb6"), n );
#if 0
			vop.coding_type = bits.Copy(2);

			while (bits.Copy(1) == 1) {
				vop.modulo_time_base++;
			}
			bits.Copy(1);

			vop.time_inc = bits.Copy(vol.time_inc_bits);
			bits.Copy(1);

			vop.coded = ToBool(bits.Copy(1));
			vop.vop_type = vop.coded ? (VOPType)vop.coding_type : VOPType.N_VOP;
			if (vop.coded) {
				if (vol.newpred_enable) {
					int vop_id_bits = Math.Min(vol.time_inc_bits + 3, 15);

					bits.Copy(vop_id_bits);
					if (bits.Copy(1) == 1) {
						bits.Copy(vop_id_bits);
					}
					bits.Copy(1);
				}

				if ( (vol.shape != (uint)VOLShape.BinaryOnly) &&
					( (vop.coding_type == (uint)VOPType.P_VOP) ||
					( (vop.coding_type == (uint)VOPType.S_VOP) && (vol.sprite_enable == (uint)Sprite.GMC) ) ) )
				{
					bits.Copy(1);
				}

				if ( vol.reduced_resolution_enable &&
					(vol.shape == (uint)VOLShape.Rectangular) &&
					( (vop.coding_type == (uint)VOPType.P_VOP) || (vop.coding_type == (uint)VOPType.I_VOP) ) )
				{

					bits.Copy(1);
				}

				if (vol.shape != (uint)VOLShape.Rectangular) {
					if ( !( (vol.sprite_enable == (uint)Sprite.Static) &&
						(vop.coding_type == (uint)VOPType.I_VOP) ) )
					{
						// 56 bits
						bits.Copy(32);
						bits.Copy(24);
					}
					bits.Copy(1);
					if (bits.Copy(1) == 1) {
						bits.Copy(8);
					}
				}

				if (vol.shape != (uint)VOLShape.BinaryOnly) {
					bits.Copy(3);
					if (vol.interlaced) {
						vop.top_field_first = ToBool(bits.Read(1));
						if (modify) {
							bitsOut.Write( (_tff ? 1U : 0U) , 1);
						}
						bits.Copy(1);
					}
				}
			}

			if (modify) {
				while (bits.Remaining > 0) {
					bits.Copy(Math.Min(bits.Remaining, 32));
				}
			}
#endif
			Bitstream.GetBits( b, n * 8u );
			DumpBytes( b, n, htitmp );
			break;
		default: 
			htitmp = OSNodeCreate( hti, GetReadPos(), _T("Unknown"), _T("0x") + ToHexString( header[3] ), n );
			Bitstream.GetBits( b, n * 8u );
			DumpBytes( b, n, htitmp );
			break;
		}
		delete[] b;
		nSize += n;
	}
#endif
}
#if 0
public enum VOPType : uint {
		I_VOP   = 0,
		P_VOP   = 1,
		B_VOP   = 2,
		S_VOP   = 3,
		N_VOP   = 4,
		N_VOP_D = 5
	}

	public enum VOLShape : uint {
		Rectangular = 0,
		Binary      = 1,
		BinaryOnly  = 2,
		Grayscale   = 3
	}

	public enum Sprite : uint {
		None   = 0,
		Static = 1,
		GMC    = 2
	}
#endif


#if 0
bool CAVIParser::GetFramesFromIndex()
{
	Settings.bWriteUseIndex = false;
	AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[0];
	if ( !pash )
		return false;
	const AVIMetaIndex* mi = pash->m_pAVIMetaIndex;
	if ( !mi || mi->bIndexType != AVIMetaIndex::AVI_INDEX_OF_INDEXES 
		|| mi->wLongsPerEntry != sizeof( mi->SuperIndex.aIndex[0] ) / sizeof( DWORD )
		|| mi->bIndexSubType != AVIMetaIndex::AVI_INDEX_SUB_DEFAULT )
		return false;
	QWORD nFilePos = GetReadPos();
	QWORD nOffset = 0; DWORD nFrames = 0;
	for ( int i = 0; i < mi->nEntriesInUse - 1u; i++ ) {
		if ( nFrames + mi->SuperIndex.aIndex[i+1].dwDuration > Settings.nFrameStart ) { //  && nFrames < Settings.nFrameEnd
			nOffset = mi->SuperIndex.aIndex[i].qwOffset;
			SetReadPos( nOffset );
			FOURCC fcc; DWORD nSize;
			if ( !ReadTypeSize( fcc, nSize, 0 ) )
				return false;
			auto* mi = (AVIMetaIndex*)new BYTE[nSize];
			if ( !ReadBytes( mi, nSize, 0 ) )
				return false;

			for ( WORD j = 0; j < mi->nEntriesInUse; ++j ) {
				if ( mi->wLongsPerEntry != sizeof( mi->StandardIndex.aIndex[0] ) / sizeof( DWORD ) ) // 2
					goto e;

				if ( mi->bIndexType != AVIMetaIndex::AVI_INDEX_OF_CHUNKS )
					goto e;

				if ( nFrames++ > Settings.nFrameStart ) {
					m_pAVIDesc->m_vpFrameEntries.push_back( new AVIFrameEntry( 1/*nSegment*/, mi->dwChunkId, 0, mi->StandardIndex.aIndex[j].dwSize, mi->StandardIndex.qwBaseOffset + mi->StandardIndex.aIndex[j].dwOffset ) );
					m_pAVIDesc->m_vpAVIStreamHeader[0]->mFOURCCCounter[mi->dwChunkId].frames++; // ...
				}
			}
			delete[] (BYTE*)mi;
		} else	
			nFrames += mi->SuperIndex.aIndex[i+1].dwDuration;
	}

	// TODO wenn kein indx, dannn idx1 durchsuchen und wenn der auch nicht, dann wenigstens nach dem letzten Cut-Frame aufhören
	
	Settings.nFrameStart = 0;
	Settings.nFrameEnd = (DWORD)-1;
	Settings.bWriteUseIndex	= true;
	SetReadPos( nFilePos );
	return true;


e:	SetReadPos( nFilePos );
	return false;
}
#endif