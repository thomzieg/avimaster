#include "RIFFParser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

CRIFFParser::CRIFFParser( class CIOService& ioservice) : CParser( ioservice )
{
}

CRIFFParser::ERET CRIFFParser::RIFFReadRiffChunk( int hti, DWORD nChunkSize, FOURCC fcc, WORD& nSegmentCount )
{ 
FOURCC fcc2;
int htitmp;
_tstringstream s;
	
	if ( nChunkSize < 4 )
		return CRIFFParser::ERET::ER_EOF;
	if ( !ReadType( fcc2, hti ) )
		return CRIFFParser::ERET::ER_EOF;

	++nSegmentCount;

	switch ( fcc2 ) {
		case FCC('MV93'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Macromedia ??? Chunk"), fcc, nChunkSize + 8, fcc2 );
			break;
		case FCC('MC95'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Macromedia Creator Chunk"), fcc, nChunkSize + 8, fcc2 );
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Unknown Chunk"), fcc, nChunkSize + 8, fcc2 );
			break;
	}

	return RIFFReadGenericChunk( htitmp, nChunkSize - 4 /*fcc*/, nSegmentCount );
}

CRIFFParser::ERET CRIFFParser::RIFFReadGenericChunk( int hti, DWORD nParentSize, WORD nSegmentCount )
{
FOURCC fcc;
DWORD nSize;
int htitmp;
_tstring s;
	
	while ( nParentSize >= 8 ) {
		OSStatsSet( GetReadPos() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		if ( nSize + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
		case FCC('FORM'): // IFF 
		case FCC('RIFF'): // RIFF
		case FCC('RIFX'): // RIFF
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

			switch( fcc ) {
			case FCC('UNFO'):
			case FCC('INFO'):
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T("RIFF Info List"), lfcc, nSize + 12, fcc );
				if ( RIFFReadInfoListChunk( htitmp, nSize ) == CRIFFParser::ERET::ER_EOF )
					return CRIFFParser::ERET::ER_EOF;
				break;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 12, _T(""), lfcc, nSize + 12, fcc );
				if ( RIFFReadGenericChunk( htitmp, nSize, nSegmentCount ) == CRIFFParser::ERET::ER_EOF ) // Recursion!!!
					return CRIFFParser::ERET::ER_EOF;
				break;
			}
			break; }
		default: 
			switch( RIFFReadHandleCommonChunks( hti, nSize, fcc ) ) {
			case CRIFFParser::ERET::ER_OK:
				break;
			case CRIFFParser::ERET::ER_EOF:
				return CRIFFParser::ERET::ER_EOF;
			default:
				htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
				if ( !ReadSeekFwdDump( nSize, htitmp ) ) 
					return CRIFFParser::ERET::ER_EOF;
				if ( !ReadAlign( nSize ) ) // besser extra, ist toleranter...
					return CRIFFParser::ERET::ER_EOF;
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

CRIFFParser::ERET CRIFFParser::RIFFReadHandleCommonChunks( int hti, DWORD nChunkSize, FOURCC fcc )
{
int htitmp;

r:	switch ( fcc ) {
	case FCC('INFO'):
		hti = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Info List"), fcc, nChunkSize + 8 );
		if ( !ReadTypeSize( fcc, nChunkSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		goto r;
	case FCC('UNFO'):
		if ( !DumpGenericUTF16TextChunk( hti, nChunkSize, fcc, "RIFF Info Chunk" ) )
			return CRIFFParser::ERET::ER_EOF;
		m_nJunkSize += nChunkSize + 8;
		break;
	case FCC('IARL'): case FCC('IART'): case FCC('ICMS'):
	case FCC('ICMT'): case FCC('ICOP'): case FCC('ICRD'):
	case FCC('ICRP'): case FCC('IDIM'): case FCC('IDPI'):
	case FCC('IENG'): case FCC('IGNR'): case FCC('IKEY'):
	case FCC('ILGT'): case FCC('IMED'): case FCC('INAM'):
	case FCC('IPLT'): case FCC('IPRD'): case FCC('ISBJ'):
	case FCC('ISFT'): case FCC('ISHP'): case FCC('ISRC'):
	case FCC('ISRF'): case FCC('ITCH'): case FCC('ISMP'):
	case FCC('IDIT'):
		if ( !DumpGenericMBCSTextChunk( hti, Aligned( nChunkSize ), fcc, "RIFF Info Chunk" ) )
			return CRIFFParser::ERET::ER_EOF;
		m_nJunkSize += nChunkSize + 8;
//		while ( ((BYTE*)&fcc)[0] != 'U' && nChunkSize > 0 && !isalnum( p[nChunkSize - 1] ) )
//			p[--nChunkSize] = '\0';
		break;
	case FCC('DISP'): // Display Chunk 
		if ( !RIFFDumpWaveDispChunk( hti, nChunkSize ) )
			return CRIFFParser::ERET::ER_EOF;
		break;
	case FCC('FXTC'): // Adobe After Effects
		htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("AVI Adobe After Effects Chunk"), fcc, nChunkSize + 8 );
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
		if ( nChunkSize > 0 && !ReadSeekFwdDump( nChunkSize, htitmp ) ) 
			return CRIFFParser::ERET::ER_EOF;
		m_nJunkSize += nChunkSize + 8;
		break;
	case FCC('JUNQ'):
	case FCC('JUNK'):
	case FCC('PAD '):
		htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Junk"), fcc, nChunkSize + 8 );
		if ( fcc == FCC('JUNQ') )
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token (Premiere JUNK)!") );
		if ( nChunkSize > 0 && !ReadSeekFwdDump( Aligned( nChunkSize ), htitmp ) ) 
			return CRIFFParser::ERET::ER_EOF;
		m_nJunkSize += nChunkSize + 8;
		break;
	case FCC('guid'): 
		if ( !RIFFDumpGuidChunk( hti, nChunkSize ) )
			return CRIFFParser::ERET::ER_EOF;
		break; 
	case FCC('file'):  
		if ( !DumpGenericUTF16TextChunk( hti, nChunkSize, FCC('file'), "RIFF File Chunk" ) )
			return CRIFFParser::ERET::ER_EOF;
		break; 
	case FCC('name'): 
		if ( !DumpGenericUTF16TextChunk( hti, nChunkSize, FCC('name'), "RIFF Name Chunk" ) )
			return CRIFFParser::ERET::ER_EOF;
		break; 
	case FCC('vers'): 
		if ( !RIFFDumpVersChunk( hti, nChunkSize ) )
			return CRIFFParser::ERET::ER_EOF;
		break; 
	default:
		return CRIFFParser::ERET::ER_NOTCONSUMED;
	}
	return CRIFFParser::ERET::ER_OK;
}


CRIFFParser::ERET CRIFFParser::RIFFReadInfoListChunk( int hti, DWORD nParentSize )
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return CRIFFParser::ERET::ER_EOF;
		if ( !ReadTypeSize( fcc, nSize, hti ) )
			return CRIFFParser::ERET::ER_EOF;
		Align( nSize );
		if ( nSize + 8 /*fcc size*/ > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = Aligned( nParentSize ) - 8 /*fcc size*/;
			else 
				nParentSize = Aligned( nSize ) + 8;
		} 
		nParentSize -= Aligned( nSize ) + 8 /*fcc size*/;

		switch ( fcc ) {
//		case FCC('PRMI'): /*Premiere Info*/ 
		case FCC('IARL'): case FCC('IART'): case FCC('ICMS'):
		case FCC('ICMT'): case FCC('ICOP'): case FCC('ICRD'):
		case FCC('ICRP'): case FCC('IDIM'): case FCC('IDPI'):
		case FCC('IENG'): case FCC('IGNR'): case FCC('IKEY'):
		case FCC('ILGT'): case FCC('IMED'): case FCC('INAM'):
		case FCC('IPLT'): case FCC('IPRD'): case FCC('ISBJ'):
		case FCC('ISFT'): case FCC('ISHP'): case FCC('ISRC'):
		case FCC('ISRF'): case FCC('ITCH'): case FCC('ISMP'):
		case FCC('IDIT'): case FCC('DATE'): case 0: // leeres Tag...
			if ( !DumpGenericMBCSTextChunk( hti, nSize, fcc, "Info" ) )
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('UARL'): case FCC('UART'): case FCC('UCMS'):
		case FCC('UCMT'): case FCC('UCOP'): case FCC('UCRD'):
		case FCC('UCRP'): case FCC('UDIM'): case FCC('UDPI'):
		case FCC('UENG'): case FCC('UGNR'): case FCC('UKEY'):
		case FCC('ULGT'): case FCC('UMED'): case FCC('UNAM'):
		case FCC('UPLT'): case FCC('UPRD'): case FCC('USBJ'):
		case FCC('USFT'): case FCC('USHP'): case FCC('USRC'):
		case FCC('USRF'): case FCC('UTCH'): case FCC('USMP'):
		case FCC('UDIT'): 
			if ( !DumpGenericUTF16TextChunk( hti, nSize, fcc, "Info" ) )
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		case FCC('JUNQ'):
		case FCC('JUNK'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Junk"), fcc, nSize + 8 );
			if ( fcc == FCC('JUNQ') )
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token (Premiere JUNK)!") );
			if ( nSize > 0 && !ReadSeekFwdDump( Align( nSize ), htitmp ) )
				return CRIFFParser::ERET::ER_EOF;
			m_nJunkSize += nSize + 8;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T(""), fcc, nSize + 8 );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") );
			switch ( fcc ) {
			case FCC('RIFF'):
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_RIFF;
			case FCC('idx1'):
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_idx1;
			case FCC('movi'):
				ReadSeek( -8 /*fcc size*/ );
				return CRIFFParser::ERET::ER_TOKEN_movi;
			}
			if ( !ReadSeekFwdDump( Align( nSize ), htitmp ) ) 
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

bool CRIFFParser::RIFFDumpWaveDispChunk( int hti, DWORD nSize ) 
{
	hti = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Display Chunk"), FCC('DISP'), nSize + 8 );
	BYTE* buffer = new BYTE[nSize + 1];
	if ( !ReadBytes( buffer, Aligned( nSize ), hti ) ) {
		delete[] buffer;
		return false;
	}

_tstring s;

	const RIFFWaveDispChunk* p = reinterpret_cast<const RIFFWaveDispChunk*>( buffer );
	s = _T("00 Type: 0x") + ToHexString( p->dwType ); 
	switch ( p->dwType ) {
	case RIFFWaveDispChunk::CF_TEXT:
		s += _T(" Text");
		break;
	case RIFFWaveDispChunk::CF_BITMAP:
		s += _T(" Bitmap");
		break;
	case RIFFWaveDispChunk::CF_METAFILEPICT:
		s += _T(" MetafilePict");
		break;
	case RIFFWaveDispChunk::CF_SYLK:
		s += _T(" SYLK");
		break;
	case RIFFWaveDispChunk::CF_DIF:
		s += _T(" DIF");
		break;
	case RIFFWaveDispChunk::CF_TIFF:
		s += _T(" TIFF");
		break;
	case RIFFWaveDispChunk::CF_OEMTEXT:
		s += _T(" OEMText");
		break;
	case RIFFWaveDispChunk::CF_DIB:
		s += _T(" DIB (Bitmap)");
		break;
	case RIFFWaveDispChunk::CF_PALETTE:
		s += _T(" Palette");
		break;
	case RIFFWaveDispChunk::CF_PENDATA:
		s += _T(" PenData");
		break;
	case RIFFWaveDispChunk::CF_RIFF:
		s += _T(" RIFF");
		break;
	case RIFFWaveDispChunk::CF_WAVE:
		s += _T(" WAVE");
		break;
	case RIFFWaveDispChunk::CF_UNICODETEXT:
		s += _T(" UnicodeText");
		break;
	case RIFFWaveDispChunk::CF_ENHMETAFILE:
		s += _T(" EnhMetafile");
		break;
	case RIFFWaveDispChunk::CF_HDROP:
		s += _T(" HDrop");
		break;
	case RIFFWaveDispChunk::CF_LOCALE:
		s += _T(" Locale");
		break;
	case RIFFWaveDispChunk::CF_DIBV5:
		s += _T(" DIBV5 (Bitmap)");
		break;
	default:
		s += _T(" (unknown)");
		break;
	}
	OSNodeCreateValue( hti, s );
	if ( p->dwType == RIFFWaveDispChunk::CF_TEXT || p->dwType == RIFFWaveDispChunk::CF_OEMTEXT ) {
		s = _T("04 Data: \"") + ToString( reinterpret_cast<const char*>( p->bData ) ) + _T("\""); // TODO strlen? ToStringFromMBCS
		OSNodeCreateValue( hti, s );
	}
	else if ( p->dwType == RIFFWaveDispChunk::CF_UNICODETEXT ) {
		s = _T("04 Data: \"") + ToStringFromUTF16( reinterpret_cast<const char16*>( p->bData ), (size_t)-1 ) + _T("\""); // TODO strlen?
		OSNodeCreateValue( hti, s );
	}
	delete[] buffer;
	return true;
}

bool CRIFFParser::RIFFDumpGuidChunk( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		1, "RIFF GUID Chunk", "'guid'", 16, 16,
		DDEF::DESC::DT_GUID, 0, 3, "SubFormat GUID", nullptr, nullptr,
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
}

bool CRIFFParser::RIFFDumpVersChunk( int hti, DWORD nSize )
{
#if 0
	static constexpr DDEF ddef = {
		2, "RIFF Version Chunk", "'vers'", 8, 8,
		DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "VersionMS", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "VersionLS", nullptr, nullptr,
	};
	return (Dump( hti, 8, nSize + 8, &ddef ) == nSize);
#else
	int htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("RIFF Version Chunk"), FCC('vers'), nSize + 8 );
	BYTE* p = new BYTE[nSize + 1];
	if ( !ReadBytes( p, Aligned( nSize ), htitmp ) ) 
		return false;
	if ( nSize != sizeof( DMUS_IO_VERSION ) )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Token Size, expected ") + ToString( sizeof( DMUS_IO_VERSION ) ) + _T("!") );
	else {
		_tstring s;
		if ( Settings.nVerbosity >= 3 ) {
			s = _T("00 : 0x") + ToHexString( ((DMUS_IO_VERSION*)p)->dwVersionMS ); 
			OSNodeCreateValue( htitmp, s );
			s = _T("04 VersionLS: 0x") + ToHexString( ((DMUS_IO_VERSION*)p)->dwVersionLS ); 
			OSNodeCreateValue( htitmp, s );
		}
		s = _T("-> Version: ") + ToString( ((DMUS_IO_VERSION*)p)->dwVersionMS >> 16 ) + _T(".") + ToString( (((DMUS_IO_VERSION*)p)->dwVersionMS & 0x0000ffff) ) + _T(".") + ToString( ((DMUS_IO_VERSION*)p)->dwVersionLS >> 16 ) + _T(".") + ToString( (((DMUS_IO_VERSION*)p)->dwVersionLS & 0x0000ffff) ); 
		OSNodeCreateValue( htitmp, s );
	}
	delete[] p;
	return true;
#endif
}


/*static*/ bool CRIFFParser::Probe( BYTE buffer[12] )
{
	FOURCC* fcc = reinterpret_cast<FOURCC*>( buffer ); 
	if ( fcc[0] != FCC('RIFF') && fcc[0] != FCC('FORM') 
			&& fcc[0] != FCC('LIST') && fcc[0] != FCC('CAT ') && fcc[0] != FCC('RIFX') )
		return false;
	return ISFCC_ALNUM( fcc[2] ) || fcc[2] == FCC('Egg!');
}

bool CRIFFParser::ReadFile( int hti )
{ 
DWORD nSize;
DWORD fcc;
WORD nSegmentCount = 0;
int htitmp;
_tstringstream s;
bool bFlagRiff = false;

	while ( GetReadFileSize() > 8 && GetReadTotalBytes() < GetReadFileSize() - 8 ) { 
		if ( !ReadType( fcc, hti ) ) 
			return false; // does not happen
		if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFNETWORK || (Settings.eForce == Settings::EFORCE::EM_FORCENONE && fcc == FCC('RIFX')) ) 
			SetParams( true, true );
		else if ( Settings.eForce == Settings::EFORCE::EM_FORCERIFFINTEL || (Settings.eForce == Settings::EFORCE::EM_FORCENONE && fcc == FCC('RIFF')) ) 
			SetParams( true, false );

		if ( Settings.eForce != Settings::EFORCE::EM_FORCENONE || fcc == FCC('RIFF') || fcc == FCC('FORM') 
				|| fcc == FCC('LIST') || fcc == FCC('CAT ') || fcc == FCC('RIFX') ) { 
			if ( !ReadSize( nSize, hti ) ) 
				return false; // does not happen
			if ( nSize > GetReadFileSize() - GetReadTotalBytes() ) {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Size (") + ToString( nSize ) + _T(") exceeding File Size detected, corrected!") );
				nSize = (DWORD)(GetReadFileSize() - GetReadTotalBytes());
			}
			bFlagRiff = true;
			RIFFReadRiffChunk( hti, nSize, fcc, nSegmentCount );  
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
			if ( nSize > 0 && !ReadSeekFwdDump( Align( nSize ), htitmp ) ) 
				return true;
		}
	}
	if ( GetReadTotalBytes() != GetReadFileSize() )
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("File Size mismatch (" + ToString( GetReadFileSize() - GetReadTotalBytes() ) + _T(")")) );

	if ( bFlagRiff ) {
		if ( IsNetworkByteorder() )
			m_sSummary = _T("This is a valid RIFF file in Network byte order.");
		else
			m_sSummary = _T("This is a valid RIFF file.");
	}

	return true; // all done
}

bool CRIFFParser::ReadSize( DWORD &n, int hti ) 
{ 
	if ( !ReadBytes( &n, sizeof( n ), hti ) )
		return false;
	if ( IsNetworkByteorder() ) 
		n = FlipBytes( &n );
	if ( !IsNetworkByteorder() && GetReadTotalBytes() + n > GetReadFileSize() ) // HACK m_bNetworkByteOrder implies size includes header
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk size greater File size, File may be truncated!") );
	return true;
}


/*static*/ _tstring CRIFFParser::GetAudioFormat( DWORD nFormat )
{
	_tstring s;
	switch ( nFormat ) {
	case WAVE_FORMAT_PCM:
		s += _T("(PCM)");
		break;
	case WAVE_FORMAT_GSM610:
		s += _T("(GSM610)");
		break;
	case WAVE_FORMAT_MPEGLAYER3:
		s += _T("(MP3)");
		break;
	case WAVE_FORMAT_DOLBY_AC3_SPDIF:
		s += _T("(Dolby AC2 SPDIF)");
		break;
	case WAVE_FORMAT_MPEG:
		s += _T("(MP1/2)");
		break;
	case WAVE_FORMAT_EXTENSIBLE:
		s += _T("(Extensible)");
		break;
	case WAVE_FORMAT_DIVX:
		s += _T("(MS Audio V2/DIVX)");
		break;
	case WAVE_FORMAT_MSAUDIO1:
		s += _T("(MS Audio V1/DIVX)");
		break;
	case WAVE_FORMAT_VOXWARE_RT29:
		s += _T("(Voxware RT29)");
		break;
	case WAVE_FORMAT_OGG_VORBIS_3_PLUS:
		break;
	case WAVE_FORMAT_OGG_VORBIS_2_PLUS:
		s += _T("(OGG Vorbis 2 Plus)");
		break;
	case WAVE_FORMAT_OGG_VORBIS_1_PLUS:
		s += _T("(OGG Vorbis 1 Plus)");
		break;
	case WAVE_FORMAT_OGG_VORBIS_3:
		s += _T("(OGG Vorbis 3)");
		break;
	case WAVE_FORMAT_OGG_VORBIS_2:
		s += _T("(OGG Vorbis 2)");
		break;
	case WAVE_FORMAT_OGG_VORBIS_1:
		s += _T("(OGG Vorbis 1)");
		break;
	case WAVE_FORMAT_AC3:
		s += _T("(AC3)");
		break;
	case WAVE_FORMAT_MSG723:
		s += _T("(MSG723)");
		break;
	case WAVE_FORMAT_DVI_ADPCM:
		s += _T("(Intel ADPCM)");
		break;
	case WAVE_FORMAT_VOXWARE:
		s += _T("(Voxware file-mode codec)");
		break;
	case WAVE_FORMAT_IMC:
		s += _T("(Intel Music Coder (IMC))");
		break;
	case WAVE_FORMAT_LIGOS_INDEO:
		s += _T("(Ligos Indeo Audio)");
		break;
	case WAVE_FORMAT_DSPGROUP_TRUESPEECH:
		s += _T("(DSPGroup TrueSpeech)");
		break;
	case WAVE_FORMAT_MSNAUDIO:
		s += _T("(MSNAUDIO)");
		break;
	case WAVE_FORMAT_DOLBY_AC2:
		s += _T("(Dolby AC2)");
		break;
	case WAVE_FORMAT_G723_ADPCM:
		s += _T("(G723 ADPCM)");
		break;
	case WAVE_FORMAT_MULAW:
		s += _T("(MuLaw)");
		break;
	case WAVE_FORMAT_ALAW:
		s += _T("(ALaw)");
		break;
	case WAVE_FORMAT_ADPCM:
		s += _T("(ADPCM)");
		break;
	default:
		s += _T("(http://www.iana.org/assignments/wave-avi-codec-registry)");
		break;
	}
	return s;
}

