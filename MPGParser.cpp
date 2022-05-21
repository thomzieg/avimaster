#if 0
1) Elementary stream (ES): 
	is the stream that contains the actual compressed
      	audio and video data, like I, P, B-Frames
       	etc., with respective headers and some
       	other information.

2) Packetized elementary stream (PES): 
	is composed of Packets containing ES data. The packets have
        headers, that specify the respective ES data, like
        0x000001E0 for the first video stream. The header also
        holds the packet length and stuff like the PTS. 

3) Program Stream (PS): 
	mainly contains PES packets with some
        additional packets containing some more
        information for the decoder, like the SCR,
        buffer sizes and mux rates. PSs are used by
        DVDs and distributed .vob files.

4) Transport Stream (TS): 
        is composed of packets that all have the
        same size (188bytes). They all start with 0x47.
        The packets are identified by their PID.
        Some of them contain audio and video data
        in the form of PES packets which are spread
        over many packets of size 188 bytes. Other
        packets contain sections which contain various
        things like the PMT, the PAT or videotext
        data. A TS is meant for transporting data
        over networks. 
		188bytes/packet original TS 
		192bytes/packet m2ts (ts + 4bytes-timecode) 
		204bytes/packet DVB (ts + 16bytes-FEC) 
		208bytes/packet ATSC (ts + 20bytes-reed-solomon) 

5) AV_PES: 
	is the format used by the Siemens (Technotrend) DVB card internally.
        The audio AV_PES packets contain audio PES packets with a
        header that tells you wether there is a PTS in the
        packet. The video packets are video PES packets without the
        PES header (that really means the are ES streams) with a
        header that may contain a PTS. The data originally comes
        from a TS, but has already been processed by the decoder
        and is read from the decoders buffer.
#endif

#include "Helper.h"
#include "MPGParser.h"
#include "ID3Parser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

// -v4 -o -mt -rs0x200 "d:\testavis\mpeg\priv_dat.ts" output.0x200.mpg >priv_dat.txt


/*static*/ const WORD CMPGParser::m_nMinHeaderSize = 4;

#define TRANSPORT_SYNC_BYTE      0x47
#define PROGRAM_STREAM_MAP       0xBC
#define PRIVATE_STREAM_1         0xBD
#define PADDING_STREAM           0xBE
#define PRIVATE_STREAM_2         0xBF
#define ECM_STREAM               0xF0
#define EMM_STREAM               0xF1
#define PROGRAM_STREAM_DIRECTORY 0xFF
#define DSMCC_STREAM             0xF2
#define ITUTRECH222TYPEE_STREAM  0xF8

#define SUBSTREAM_AC3_0          0x80
#define SUBSTREAM_AC3_8          0x87
#define SUBSTREAM_DTS_0          0x88
#define SUBSTREAM_DTS_8          0x8F
#define SUBSTREAM_PCM_0          0xA0
#define SUBSTREAM_PCM_F          0xAF
#define SUBSTREAM_SUBPIC_0       0x20
#define SUBSTREAM_SUBPIC_1F      0x3F


CMPGParser::CMPGParser( class CIOService& ioservice) : CParser( ioservice ), ParserFactory( this ), Bitstream( ioservice )
{
	Bitstream.Reset();
}

CMPGParser::~CMPGParser()
{
	OSStatsSet(CIOService::EPARSERSTATE::PS_CLEANUP );
	Bitstream.ClearBuffer();
	OSStatsSet(CIOService::EPARSERSTATE::PS_READY );
}


void CMPGParser::Parse(const _tstring& sIFilename)
{
	m_sIFilename = sIFilename;
	int hti = OSNodeFileNew(0, sIFilename);
	if (ReadOpen(m_sIFilename)) {
		OSStatsSet(CIOService::EPARSERSTATE::PS_PARSING);
		OSStatsInit(GetReadFileSize());
		_tstringstream s;
		s << _T("Reading '") << FilenameGetNameExt(m_sIFilename) << _T("' (") << GetReadFileSize() << _T(")") << std::ends; // s.Format( "Reading '%s%s' (%I64d)", sFName, sExt, GetReadFileSize() ); 
																															// TODO filetime ausgeben...
		hti = OSNodeCreate(hti, s.str());
		/*bool b =*/ ReadFile(hti);
		ReadClose();
	}
	OSNodeFileEnd(hti, m_sSummary);
}

bool CMPGParser::ReadFile(int hti)
{
	m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_UNKNOWN;
	_tstring s;
	if (ReadChunk(hti, GetReadFileSize()))
		s = _T("This is a MPEG file of type: ");
	else
		s = _T("This might not be a valid MPEG file of type: ");

	switch (m_eMPEG_TYPE)
	{
	case CMPGParser::MPEG_TYPE::MT_UNKNOWN:
		s += _T("Unknown");
		break;
	case CMPGParser::MPEG_TYPE::MT_ESV:
		s += _T("Elementary Stream Video");
		break;
	case CMPGParser::MPEG_TYPE::MT_ESA:
		s += _T("Elementary Stream Audio");
		break;
	case CMPGParser::MPEG_TYPE::MT_PES1:
		s += _T("MPEG-1 Packetized Elementary Stream");
		break;
	case CMPGParser::MPEG_TYPE::MT_PES2:
		s += _T("MPEG-2 Packetized Elementary Stream");
		break;
	case CMPGParser::MPEG_TYPE::MT_PS1:
		s += _T("MPEG-1 Program Stream");
		break;
	case CMPGParser::MPEG_TYPE::MT_PS2:
		s += _T("MPEG-2 Program Stream");
		break;
	case CMPGParser::MPEG_TYPE::MT_TS:
		s += _T("TS: MPEG-2 Transport Stream (Paketsize: 188)");
		break;
	case CMPGParser::MPEG_TYPE::MT_TS_M2TS:
		s += _T("M2TS: MPEG-2 Transport Stream (Paketsize: 188+4 Timecode)");
		break;
	case CMPGParser::MPEG_TYPE::MT_TS_DVB:
		s += _T("DVB: MPEG-2 Transport Stream (Paketsize: 188+16 FEC)");
		break;
	case CMPGParser::MPEG_TYPE::MT_TS_ATSC:
		s += _T("ATSC: MPEG-2 Transport Stream (Paketsize: 188+20 Reed-Solomon)");
		break;
	case CMPGParser::MPEG_TYPE::MT_AUDIO:
		s += _T("MPEG Audio");
		break;
	default:
		ASSERT(false);
		break;
	}
	m_sSummary = s + _T("\n\n") + m_sSummary;
	return true; // all done
}

bool CMPGParser::ReadChunk( int hti, QWORD nParentSize )
{
	int htitmp = hti; // ...
	ASSERT( GetReadPos() == 0 );

	DWORD nSize = (DWORD)(nParentSize& 0xFFFF);
	if ( nSize != nParentSize )
		nSize = (DWORD)-1;

	if ( Settings.eForce != Settings::EFORCE::EM_FORCEMPEG ) { 
		QWORD w;
		BYTE* header = reinterpret_cast<BYTE*>( &w );
		if ( !Bitstream.GetBytes( header, sizeof( w ) ) )
			return false;
		ReadSeek(-4);
		ASSERT(GetReadPos() == 4); // 8 Bytes gelesen, an Position 4
		switch ( w ) {
		case 0xb3010000: { // MPEG Video, Elementary Stream
			ReadSeek( -4 );
//			CMPGESVideoParser p;
			bool bOk = ParserFactory.GetParser( 0xe0 )->Parse( hti, 0xe0, nSize, false ); 
//			bool bOk = p.Parse(  ); 
			ParserFactory.Statistics( hti );
			return bOk;
			}
		case 0xba010000: { // VOB MEDIATYPE_*_PACK, PES
			ReadSeek( -4 );
			bool bOk = ParseMuxFrame( hti, nSize ); 
			ParserFactory.Statistics( hti );
			return bOk;
			}
		case 0x05334449:  // MP3 (ID3V2.5)
		case 0x04334449:  // MP3 (ID3V2.4)
		case 0x03334449:  // MP3 (ID3V2.3)
		case 0x02334449:  // MP3 (ID3V2.2)
		case 0x01334449: { // MP3 (ID3V2.1) // TODO eigentlich versionsbyte ausblenden (immer ungleich 0xff)
//			CMPGESAudioParser p;
			CID3Parser* p = new CID3Parser( io );
			if (!p->ReadChunk(hti, (QWORD)-1)) { // TODO Filesize - GetReadPos()
				delete p;
				return false;
			}
			QWORD l1 = p->GetSize();
			delete p;
			// dann ist es mp3 und der Rest audio...
			bool bOk = ParserFactory.GetParser( 0xc0 )->Parse( hti, 0xc0, nSize - l1, false ); 
//			bool bOk = p.Parse( hti, 0x00, nSize ); 
			ParserFactory.Statistics( hti );
			return bOk;
			}
		default: 
			if ( header[0] == 0xff && (header[1] & 0xf0) == 0xf0 ) { 
				// MPEG Audio, Layer 1,2,3 http://flavor.sourceforge.net/samples/mpeg1as.htm
				ReadSeek( -4 );
//				CMPGESAudioParser p;
				bool bOk = ParserFactory.GetParser( 0xc0 )->Parse( hti, 0xc0, nSize, false ); 
//				bool bOk = p.Parse( hti, 0x00, nSize ); 
				ParserFactory.Statistics( hti );
				return bOk;
			}

			if (header[0] == TRANSPORT_SYNC_BYTE && header[4] != TRANSPORT_SYNC_BYTE) {
				if ( !ReadSeek( 188 - 4 ) )
					return false;

				ASSERT( GetReadPos() == 188 );
				BYTE b;
				if ( !Bitstream.GetBytes( &b, 1 ) )
					return false;
				ReadSeek( -(188 + 1 - 4) );
				if ( b == TRANSPORT_SYNC_BYTE ) {
					ReadSeek( -4 );
					m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_TS;
					bool bOk = ParseTransportDump( hti );
					ParserFactory.Statistics( hti );
					return bOk;
				}
			}
			else if (header[0] != TRANSPORT_SYNC_BYTE && header[4] == TRANSPORT_SYNC_BYTE) {
				if (!ReadSeek(192 - 4 + 4))
					return false;

				ASSERT(GetReadPos() == 192 + 4);
				BYTE b;
				if (!Bitstream.GetBytes(&b, 1))
					return false;
				ReadSeek(-(192 + 4 + 1 - 4));
				if (b == TRANSPORT_SYNC_BYTE) {
					ReadSeek(-4);
					m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_TS_M2TS;
					bool bOk = ParseTransportDump(hti);
					ParserFactory.Statistics(hti);
					return bOk;
				}
			}
		}
		return false;
	} else { // FORCE
		 // 0. ID3
		{
			DWORD w;
			BYTE* header = reinterpret_cast<BYTE*>(&w);
			if (!Bitstream.GetBytes(header, 4))
				return false;
			switch (w) {
			case 0x05334449:  // MP3 (ID3V2.5)
			case 0x04334449:  // MP3 (ID3V2.4)
			case 0x03334449:  // MP3 (ID3V2.3)
			case 0x02334449:  // MP3 (ID3V2.2)
			case 0x01334449: { // MP3 (ID3V2.1) // TODO eigentlich versionsbyte ausblenden (immer ungleich 0xff)
				CID3Parser* p = new CID3Parser(io);
				if (!p->ReadChunk(hti, (QWORD)-1)) { // TODO Filesize - GetReadPos()
					delete p;
					return false;
				}
				//QWORD l1 = p->GetSize();
				delete p;
				// dann ist es mp3 und der Rest audio...
			}
			}
		}
		QWORD nPos = GetReadPos();
		// 1. Transport Stream
		if ( ProbeTransportPaket(hti, CMPGParser::MPEG_TYPE::MT_TS) ) {
			m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_TS;
			bool bOk = ParseTransportDump( hti );
			ParserFactory.Statistics( hti );
			return bOk;
		}
		if (ProbeTransportPaket(hti, CMPGParser::MPEG_TYPE::MT_TS_M2TS)) {
			m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_TS_M2TS;
			bool bOk = ParseTransportDump(hti);
			ParserFactory.Statistics(hti);
			return bOk;
		}
		if (ProbeTransportPaket(hti, CMPGParser::MPEG_TYPE::MT_TS_DVB)) { // TODO unbekannt, ob die Pakete wirklich so aufgebaut sind...
			m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_TS_DVB;
			bool bOk = ParseTransportDump(hti);
			ParserFactory.Statistics(hti);
			return bOk;
		}
		if (ProbeTransportPaket(hti, CMPGParser::MPEG_TYPE::MT_TS_ATSC)) { // TODO unbekannt, ob die Pakete wirklich so aufgebaut sind...
			m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_TS_ATSC;
			bool bOk = ParseTransportDump(hti);
			ParserFactory.Statistics(hti);
			return bOk;
		}

		// 2. MPEG Video
		SetReadPos( nPos );
		for ( DWORD i = 0; i < 10000 && i < nSize; i++ ) {
			DWORD w;
			BYTE* header = reinterpret_cast<BYTE*>( &w );
			if ( !Bitstream.GetBytes( header, 4 ) )
				return false;
			switch ( w ) {
			case 0xb3010000: { // MPEG Video, Elementary Stream
				ReadSeek( -4 );
	//			CMPGESVideoParser p;
				bool bOk = ParserFactory.GetParser( 0xe0 )->Parse( hti, 0xe0, nSize - i, false ); 
	//			bool bOk = p.Parse(  ); 
				ParserFactory.Statistics( hti );
				return bOk;
				}
			case 0xba010000: { // VOB MEDIATYPE_*_PACK, PES
				ReadSeek( -4 );
				nSize -= i;
				bool bOk = ParseMuxFrame( hti, nSize ); 
				ParserFactory.Statistics( hti );
				return bOk;
				}
			}
			ReadSeek( -3 );
		}

		// 3. MPEG Audio
		SetReadPos( nPos );
		for ( DWORD i = 0; i < 10000 && i < nSize; i++ ) {
			DWORD w;
			BYTE* header = reinterpret_cast<BYTE*>( &w );
			if ( !Bitstream.GetBytes( header, 4 ) )
				return false;
			if ( header[0] == 0xff && (header[1] & 0xf0) == 0xf0 ) { 
				// MPEG Audio, Layer 1,2,3 http://flavor.sourceforge.net/samples/mpeg1as.htm
				ReadSeek( -4 );
//				CMPGESAudioParser p;
				bool bOk = ParserFactory.GetParser( 0xc0 )->Parse( hti, 0xc0, nSize - i, false ); 
//				bool bOk = p.Parse( hti, 0x00, nSize - i ); 
				ParserFactory.Statistics( hti );
				return bOk;
			}
			ReadSeek( -3 );
		}
		return false;
	}

#if 0	// TODO hier fehlt der Check ob am Schluss noch Daten übrigbleiben...
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
#endif
	return true;
}

bool CMPGParser::ParseTransportDump( int hti )
{ // http://happy.emu.id.au/lab/tut/dttb/dtbtut4d.htm
	QWORD nCount = 0;
	_tstring s;

	struct STREAM {
		BYTE continuity_count;
		QWORD count;
	};

	typedef std::map<WORD, STREAM> MSTREAMS; 
	MSTREAMS m_streams;

	int nPaketSize = 0;
	if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS)
		nPaketSize = 188;
	else if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS_M2TS)
		nPaketSize = 188 + 4;
	else if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS_DVB)
		nPaketSize = 188 + 16;
	else if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS_ATSC)
		nPaketSize = 188 + 20;

	if ( Settings.nTraceMoviEntries > 0 && Settings.nTraceMoviEntries * nPaketSize < GetReadFileSize() )
		OSStatsInit( Settings.nTraceMoviEntries * nPaketSize ); // TRICK to get a smooth progress bar for the items parsed
	QWORD nStartPos = GetReadPos();
	while ( true ) {
		ASSERT( (GetReadPos() - nStartPos) % nPaketSize == 0 );

		// TODO natürlich Dumpen... // TODO unbekannt wie DVB und ATSC aufgebaut sind...
		if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS_M2TS)
			Bitstream.SkipBytes(4);
		else if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS_DVB)
			Bitstream.SkipBytes(16);
		if (m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_TS_ATSC)
			Bitstream.SkipBytes(20);

		if (!Bitstream.Announce(1 * 8) || nCount > Settings.nTraceMoviEntries)
			goto e; // File end here is ok...
		DWORD nSize = 4;
		BYTE sb = Bitstream.GetAnnouncedBits<BYTE>( 8 );
		int hticur = OSNodeCreate( hti, GetReadPos() - 1, _T("Sync Byte #") + ToString( nCount++ ), _T("0x") + ToHexString( sb ), nPaketSize ); // 188 inklusive Sync Byte
		OSStatsSet( GetReadPos() );
		if ( sb != TRANSPORT_SYNC_BYTE ) {
			OSNodeCreateAlert( hticur, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Sync Byte") );
			TRACE2( _T("%s (%s): Unexpected Sync Byte.\n"), _T(__FUNCTION__), m_sIFilename.c_str() );
		}
		if ( !Bitstream.Announce( 3 * 8 ) )
			goto e; // false
		OSNodeCreateValue( hticur, 3, _T("Transport Error Indicator: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
		bool payload_unit_start_indicator = Bitstream.GetAnnouncedBits<bool>();
		if ( payload_unit_start_indicator )
			OSNodeAppendImportant( hti );
		OSNodeCreateValue( hticur, 3, _T("Payload Unit Start Indicator (PES header or start of PSI table): ") + ToString( payload_unit_start_indicator ) );
		OSNodeCreateValue( hticur, 3, _T("Transport Priority: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
		WORD pid = Bitstream.GetAnnouncedBits<WORD>( 13 );
		switch ( pid ) {
		case 0x0000:
			s = _T(" (PAT)");
			break;
		case 0x0001:
			s = _T(" (CAT)");
			break;
		case 0x0002:
		case 0x0003:
		case 0x0004:
		case 0x0005:
		case 0x0006:
		case 0x0007:
		case 0x0008:
		case 0x0009:
		case 0x000a:
		case 0x000b:
		case 0x000c:
		case 0x000d:
		case 0x000e:
		case 0x000f:
			s = _T(" (reserved)");
			break;
		case 0x1fff:
			s = _T(" (Nullpacket)");
			break;
		default:
			s = _T("");
			break;
		}

		OSNodeCreateValue( hticur, 3, _T("PID: 0x") + ToHexString( pid ) + s );
		MSTREAMS::iterator it = m_streams.find( pid );
		OSNodeCreateValue( hticur, 3, _T("Transport Scrambling Control: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
		BYTE afc = Bitstream.GetAnnouncedBits<BYTE>( 2 );
		OSNodeCreateValue( hticur, 3, _T("Adaption Field Control: ") + ToString( afc ) );
		BYTE cc = Bitstream.GetAnnouncedBits<BYTE>( 4 );
		if ( it != m_streams.end() ) {
			// TODO Gilt nur, wenn Adaption Field.Discontinuity Indicator nicht gesetzt ist...
			if ( ((it->second.continuity_count + 1) &0x0f) == cc )
				s = _T(" (ok)");
			else if ( it->second.continuity_count == cc )
				s = _T(" (retry)");
			else
				s = _T(" (error)");
			it->second.continuity_count = cc;
			it->second.count++;
		} else {
			STREAM stream;
			stream.count = 1;
			stream.continuity_count = cc;
			m_streams.insert( MSTREAMS::value_type( pid, stream ) );
			s = _T(" (first)");
		}
		OSNodeCreateValue( hticur, 3, _T("Continuity Counter: ") + ToString( cc ) + s );

		if ( afc == 0x02  || afc == 0x03 )  { 
			if ( !Bitstream.Announce( 1 * 8 ) )
				goto e; // false
			int htitmp = OSNodeCreate( hticur, GetReadPos() - 1, _T("Adaption Field") );
			WORD nAdaptionSize = 0;
			BYTE adaption_field_length = Bitstream.GetAnnouncedBits<BYTE>( 8 );
			OSNodeCreateValue( htitmp, 3, _T("Adaption Field Length: ") + ToString( adaption_field_length ) );
			ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
			ASSERT( afc == 0x03 && adaption_field_length < 182 || afc == 0x02 && adaption_field_length == 183 );
			if ( adaption_field_length > 0 ) {
				if ( !Bitstream.Announce( 1 * 8 ) )
					goto e; // false
				OSNodeCreateValue( htitmp, 3, _T("Discontinuity Indicator: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				OSNodeCreateValue( htitmp, 3, _T("Random Access Indicator: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				OSNodeCreateValue( htitmp, 3, _T("Elem Stream Priority Indicator: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				bool bpcr_flag = Bitstream.GetAnnouncedBits<bool>();
				OSNodeCreateValue( htitmp, 3, _T("PCR_flag: ") + ToString( bpcr_flag ) );
				bool bopcr_flag = Bitstream.GetAnnouncedBits<bool>();
				OSNodeCreateValue( htitmp, 3, _T("OPCR_flag: ") + ToString( bopcr_flag ) );
				bool bsplicing_point_flag = Bitstream.GetAnnouncedBits<bool>();
				OSNodeCreateValue( htitmp, 3, _T("splicing_point_flag: ") + ToString( bsplicing_point_flag ) );
				bool btransport_private_data_flag = Bitstream.GetAnnouncedBits<bool>();
				OSNodeCreateValue( htitmp, 3, _T("transport_private_data_flag: ") + ToString( btransport_private_data_flag ) );
				bool badaptation_field_extension_flag = Bitstream.GetAnnouncedBits<bool>();
				OSNodeCreateValue( htitmp, 3, _T("adaptation_field_extension_flag: ") + ToString( badaptation_field_extension_flag ) );
				if ( bpcr_flag ) {
					nAdaptionSize += 6;
					if ( !Bitstream.Announce( 6 * 8 ) )
						goto e; // false
					QWORD pcr;
					if ( !ReadTSTimestamp( htitmp, pcr ) )
						goto e;
					OSNodeCreateValue( htitmp, 3, _T("PCR: ") + ToString( pcr ) );
				}
				if ( bopcr_flag ) {
					nAdaptionSize += 6;
					if ( !Bitstream.Announce( 6 * 8 ) )
						goto e; // false
					QWORD opcr;
					if ( !ReadTSTimestamp( htitmp, opcr ) )
						goto e;
					OSNodeCreateValue( htitmp, 3, _T("OPCR: ") + ToString( opcr ) );
				}
				if ( bsplicing_point_flag ) {
					nAdaptionSize += 1;
					if ( !Bitstream.Announce( 1 * 8 ) )
						goto e; // false
					OSNodeCreateValue( htitmp, 3, _T("Splice Countdown: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
				}
				if ( btransport_private_data_flag ) {
					nAdaptionSize += 1;
					if ( !Bitstream.Announce( 1 * 8 ) )
						goto e; // false
					BYTE b = Bitstream.GetAnnouncedBits<BYTE>( 8 );
					int htitmp = OSNodeCreate( hticur, GetReadPos() - 1, _T("Transport Private Data"), _T(""), b + 1 ); 
					OSNodeCreateValue( htitmp, 3, _T("Length: ") + ToString( b ) );
					nAdaptionSize += b;
					ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
					if ( !ReadSeekFwdDump( b, (Settings.nVerbosity > 2 ? htitmp : 0) ) )
						goto e; // false
				}
				ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
				if ( badaptation_field_extension_flag ) {
					if ( !Bitstream.Announce( 1 * 8 ) )
						goto e; // false
					BYTE adaption_extension_length = Bitstream.GetAnnouncedBits<BYTE>( 8 );
					BYTE nAdaptionExtensionSize = 1;
					int htitmp = OSNodeCreate( hticur, GetReadPos() - 1, _T("Adaption Field Extension"), _T(""), adaption_extension_length + 1 ); 
					OSNodeCreateValue( htitmp, 3, _T("Length: ") + ToString( adaption_extension_length ) );
					nAdaptionSize += adaption_extension_length + 1;
					if ( !Bitstream.Announce( 1 * 8 ) )
						goto e; // false
					nAdaptionExtensionSize += 1;
					bool ltw_flag = Bitstream.GetAnnouncedBits<bool>();
					OSNodeCreateValue( htitmp, 3, _T("ltw_flag: ") + ToString( ltw_flag ) );
					bool piecewise_rate_flag = Bitstream.GetAnnouncedBits<bool>();
					OSNodeCreateValue( htitmp, 3, _T("piecewise_rate_flag: ") + ToString( piecewise_rate_flag ) );
					bool seamless_splice_flag = Bitstream.GetAnnouncedBits<bool>();
					OSNodeCreateValue( htitmp, 3, _T("seamless_splice_flag: ") + ToString( seamless_splice_flag ) );
					Bitstream.GetAnnouncedBits<BYTE>( 5 ); // Reserved
					ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
					if ( ltw_flag ) {
						if ( !Bitstream.Announce( 2 * 8 ) )
							goto e; // false
						nAdaptionExtensionSize += 2;
						OSNodeCreateValue( htitmp, 3, _T("ltw_valid_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
						OSNodeCreateValue( htitmp, 3, _T("ltw_offset: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 15 ) ) );
					}
					ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
					if ( piecewise_rate_flag ) {
						if ( !Bitstream.Announce( 3 * 8 ) )
							goto e; // false
						nAdaptionExtensionSize += 3;
						Bitstream.GetAnnouncedBits<BYTE>( 2 ); // Reserved
						OSNodeCreateValue( htitmp, 3, _T("piecewise_rate: ") + ToString( Bitstream.GetAnnouncedBits<DWORD>( 22 ) ) );
					}
					ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
					if ( seamless_splice_flag ) {
						if ( !Bitstream.Announce( 5 * 8 ) )
							goto e; // false
						nAdaptionExtensionSize += 5;
						OSNodeCreateValue( htitmp, 3, _T("Splicetype: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
						QWORD dts;
						if ( !ReadTSTimestampMarker( htitmp, dts ) )
							goto e;
						OSNodeCreateValue( htitmp, 3, _T("DTS_next_AU: ") + ToString( dts ) );
					}

					int l = (int)adaption_extension_length - nAdaptionExtensionSize;
					if ( l > 0 ) {
						int htitmp = OSNodeCreate( hticur, GetReadPos(), _T("Stuffing1"), _T("") , l ); 
						ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
						if ( !ReadSeekFwdDump( l, (Settings.nVerbosity > 2 ? htitmp : 0) ) )
							goto e; // false
					}
				}
				int l = (int)adaption_field_length - nAdaptionSize - 1;
				if ( l > 0 ) {
					int htitmp2 = OSNodeCreate( htitmp, GetReadPos(), _T("Stuffing2"), _T("") , l ); 
					ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
					if ( !ReadSeekFwdDump( l, (Settings.nVerbosity > 2 ? htitmp2 : 0) ) )
						goto e; // false
					nAdaptionSize += l + 1u;
				}
				nSize += adaption_field_length + 1;
			}
		} else if ( afc == 0x00 ) {
			OSNodeCreateAlert( hticur, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Adaption Field Type: ") + ToString( afc ) );
			goto x; // discard packet
		}
		//int l = 188 - nSize;
		if ( (afc == 0x01 || afc == 0x03) && 188 - nSize > 0 ) {
			int l = 188 - nSize;
			nSize += l;
			int htitmp = OSNodeCreate( hticur, GetReadPos(), _T("Payload"), _T("") , l ); 
			if ( !Bitstream.Announce( 3 * 8 ) )
				goto e; // false
			DWORD h = Bitstream.GetAnnouncedBits<DWORD>( 3 * 8 );
			if ( payload_unit_start_indicator && pid > 0x000f )
			{
				if ( h != 0x00000001 ) {
					ReadSeek( -3 );
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Packet Startcode 0x000001 not found!") );
					goto y;
				}

				l -= 3;
				l -= 1;
				if ( !Bitstream.Announce( 1 * 8 ) )
					goto e; // false
				BYTE nStreamId = Bitstream.GetAnnouncedBits<BYTE>( 1 * 8 );
				l -= 2;
				if ( !Bitstream.Announce( 2 * 8 ) )
					goto e; // false
				WORD PES_packet_length = Bitstream.GetAnnouncedBits<WORD>( 16 );
				if ( nStreamId == PROGRAM_STREAM_MAP || nStreamId == ECM_STREAM || nStreamId == EMM_STREAM
						|| nStreamId == PROGRAM_STREAM_DIRECTORY  || nStreamId == DSMCC_STREAM
						|| nStreamId == ITUTRECH222TYPEE_STREAM || nStreamId == PRIVATE_STREAM_2
						|| nStreamId == PADDING_STREAM ) {
					goto y;
				}
				if ( !Bitstream.Announce( 3 * 8 ) )
					goto e; // false
				Bitstream.GetAnnouncedBits<BYTE>( 2 );
				Bitstream.GetAnnouncedBits<BYTE>( 2 );
	//                streams[PID]->stream_encrypted = getbits(2);   // PES_scrambling_control
				Bitstream.GetAnnouncedBits<WORD>( 12 );
				BYTE PES_header_data_length = Bitstream.GetAnnouncedBits<BYTE>( 1 * 8 );
				l -= (3 + PES_header_data_length);
				if ( !Bitstream.SkipBytes( PES_header_data_length ) )
					goto e; // false
				if ( l == 0 ) goto x;

				l -= 1;
				if ( !Bitstream.Announce( 1 * 8 ) )
					goto e; // false
				BYTE nSubStreamId = Bitstream.GetAnnouncedBits<BYTE>( 1 * 8 );
				ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
			} else {
				ReadSeek( -3 );
			}


y:			ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
			if ( !ReadSeekFwdDump( l, (Settings.nVerbosity > 2 ? htitmp : 0) ) )
				goto e; // false
			nSize = 188;
		}

x:		if ( nSize < 188 ) {
			int htitmp = OSNodeCreate( hticur, GetReadPos(), _T("Stuffing3"), _T("") , 188 - nSize ); 
			ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
			if ( !ReadSeekFwdDump( 188 - nSize, (Settings.nVerbosity > 2 ? htitmp : 0) ) )
				goto e; // false
		} else
			ASSERT( nSize == 188 );
	}

e:	for ( MSTREAMS::const_iterator pI = m_streams.begin(); pI != m_streams.end(); ++pI ) {
		m_sSummary += _T("PID: 0x") + ToHexString( pI->first ) + _T(", Packets: ") + ToString( pI->second.count ) + _T("\n");
	}
	return true;
}



// ba, bb, c0...df, e0...ef, 
bool CMPGParser::ParseMuxFrame( int hti, DWORD& nSize )
{
	int htitmp;
//	DWORD nCount = 0;
	DWORD nMaxSize = 0;
	DWORD nCount1 = 0, nCount2 = 0;
	DWORD w;
	if ( nSize > 0 )
		nMaxSize = nSize;

	nSize = 0;
 
	for ( ;; ) {
		nSize += 4;
		if ( nMaxSize > 0 && nSize >= nMaxSize )
			return true;
		BYTE* header = reinterpret_cast<BYTE*>( &w );
		if ( !Bitstream.GetBytes( header, 4 ) )
			return false;

		if ( header[0] || header[1] || (header[2] != 1) ) {
			if ( (!header[0] && !header[1] && !header[2]) || nCount2++ >= Settings.nTraceMoviEntries ) {
				// TODO ReadSeek( -3 ); geht nicht wegen Bitstream.Buffer...
				nSize += Bitstream.FindMarker( true );
				continue;
			}
			ReadDumpErrorContext( 0x100, hti );
			nSize += Bitstream.FindMarker( true );
			ReadDumpErrorContext( 0x100, hti );
			continue;
		}
		if ( nCount1++ >= Settings.nTraceMoviEntries /*&& m_nIFrameSeen > 1*/ )
			return true;
		WORD nwPacketLen; 
		switch ( header[3] ) {
		case 0xb0: // Reserved
		case 0xb1: 
		case 0xb6: {
			WORD n = Bitstream.FindMarker( false );
			htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T("Reserved"), _T("0x") + ToHexString( header[3] ), n + 4 );
			if ( !ReadSeekFwdDump( n, (Settings.nVerbosity > 3 ? htitmp : 0) ) )
				return false;
			TRACE3( _T("%s (%s): Reserved Header %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break; }
		case 0xb2: { // User Data
			WORD n = Bitstream.FindMarker( false );
			htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T("User Data Start Code"), _T("0xb2"), n + 4 );
			if ( !ReadSeekFwdDump( n, (Settings.nVerbosity > 2 ? htitmp : 0) ) )
				return false;
			TRACE3( _T("%s (%s): User Data Header %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break; }
		case 0xb9: // program end code
			htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T("Program End Code"), _T("0xb9"), 4 );
			TRACE3( _T("%s (%s): Programm End Header %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			return true;
		case 0xba: { // pack header
			htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T("Pack Header"), _T("0xba"), Bitstream.FindMarker( false ) + 4 );
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			QWORD scr;
			BYTE b = Bitstream.GetAnnouncedBits<BYTE>( 2 );
			if ( b == 0x01 ) { // MPEG-2
				if ( m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN )
					m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_PS2;
				else
					ASSERT( m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_PS2 );
				OSNodeCreateValue( htitmp, 4, _T("Type: ") + ToString( b ) + _T(" (MPEG-2)") );
				if ( !Bitstream.Announce( 9 * 8 ) )
					return false;
				if ( !ReadPSTimestamp( htitmp, true, scr ) )
					goto e;
				OSNodeCreateValue( htitmp, 3, _T("--> SCR (27Mhz): ") + ToString( scr ) );
				DWORD nProgramMuxRate = Bitstream.GetAnnouncedFlippedBits<DWORD>( 22 );
				OSNodeCreateValue( htitmp, 3, _T("Program_Mux_Rate (50 Bytes/sec): ") + ToString( nProgramMuxRate ) );
				if ( nProgramMuxRate == 0 )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Program_Mux_Rate") );
				if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0x03 )
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 11") );
				OSNodeCreateValue( htitmp, 5, _T("Reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
				WORD n = Bitstream.GetAnnouncedBits<BYTE>( 3 );
				OSNodeCreateValue( htitmp, 4, _T("pack_stuffing_length: ") + ToString( n ) );
				for ( WORD i = 0; i < n; ++i ) {
					if ( Bitstream.GetAnnouncedBits<BYTE>() != 0xff )
						OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 11111111") );
				}
			} else if ( b == 0x00 ) {
				if ( m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN )
					m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_PS1;
				else
					ASSERT( m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_PS1 );
				OSNodeCreateValue( htitmp, 4, _T("Type: ") + ToString( b ) + _T(" (MPEG-1)") );
				if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0x02 ) // MPEG-1
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 10") );
				if ( !Bitstream.Announce( 7 * 8 ) )
					return false;
				if ( !ReadPSTimestamp( htitmp, false, scr ) )
					goto e;
				if ( !Bitstream.GetAnnouncedBits<bool>() )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
				OSNodeCreateValue( htitmp, 3, _T("--> SCR (27Mhz): ") + ToString( scr ) );
				OSNodeCreateValue( htitmp, 3, _T("Program_Mux_Rate (50 Bytes/sec): ") + ToString( Bitstream.GetAnnouncedFlippedBits<DWORD>( 22 ) ) );
				if ( !Bitstream.GetAnnouncedBits<bool>() )
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			} else {
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Pack Header Type: ") + ToString( b ) );
				TRACE3( _T("%s (%s): Undefined Pack Header Type %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), b );
e:				Bitstream.FindMarker( true ); 
			}
			break; }
		case 0xbb: { // System Header
			if ( !Bitstream.GetFlippedWord( nwPacketLen ) )
				return false;
			htitmp = OSNodeCreate( hti, GetReadPos() - 6, _T("System Header"), _T("0xbb"), nwPacketLen + 6 );
			if ( !Bitstream.Announce( nwPacketLen * 8u ) )
				return false;
			if ( !Bitstream.GetAnnouncedBits<bool>() )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			OSNodeCreateValue( htitmp, 3, _T("rate_bound: ") + ToString( Bitstream.GetAnnouncedFlippedBits<DWORD>( 22 ) ) );
			// TODO ASSERT( rate_bound >= max(program_mux_rate) );
			if ( !Bitstream.GetAnnouncedBits<bool>() )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			OSNodeCreateValue( htitmp, 3, _T("audio_bound: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) ); // UB on the number of active audio streams
			OSNodeCreateValue( htitmp, 3, _T("fixed_flag: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) ); // fixed bitrate
			OSNodeCreateValue( htitmp, 3, _T("CSPS_flag: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) ); // Constrained Parameters
			OSNodeCreateValue( htitmp, 4, _T("system_audio_lock_flag: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
			OSNodeCreateValue( htitmp, 4, _T("system_video_lock_flag: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
			if ( !Bitstream.GetAnnouncedBits<bool>() )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			OSNodeCreateValue( htitmp, 3, _T("video_bound: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );// UB on the number of video streams 
			OSNodeCreateValue( htitmp, 4, _T("packet_rate_restriction_flag: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
			if ( Bitstream.GetAnnouncedBits<BYTE>( 7 ) != 0x7f ) // Reserved
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1111111") );
			// TODO ASSSERT nwPacketLen > 6
			int htitmp2 = OSNodeCreate( htitmp, GetReadPos() - nwPacketLen + 6, _T("Stream Buffers"), _T(""), nwPacketLen - 6 );
			WORD nCount;
			for ( nCount = 0; 6 + nCount * 3 < nwPacketLen; ++nCount ) { // Additional Data, geht um 1 bit nicht auf...
				bool bFlag =  Bitstream.GetAnnouncedBits<bool>();
				if ( !bFlag )
					break; // scheinbar ist das Flag gleichzeitig Teil der Stream-Id...
				BYTE bStreamId = (BYTE)(Bitstream.GetAnnouncedBits<BYTE>( 7 ) | 0x80);
				int htitmp3 = OSNodeCreate( htitmp2, GetReadPos() - nwPacketLen +  nCount * 3, _T("Stream #") + ToString( nCount ), _T("0x") + ToHexString( (WORD)bStreamId ), 3 ); // TODO???
				if ( bStreamId < 0xbc && (bStreamId != 0xb8 && bStreamId != 0xb9) )
					OSNodeCreateAlert( htitmp3, CIOService::EPRIORITY::EP_ERROR, _T("Wrong Stream Id!") );
				// b8 bedeutet Werte beziehen sich auf alle Audio, b9 alle Video streams
				if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0x3 )
					OSNodeCreateAlert( htitmp3, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 11") );
				OSNodeCreateValue( htitmp3, 4, _T("P_STD_buffer_bound_scale: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				OSNodeCreateValue( htitmp3, 4, _T("P_STD_buffer_size_bound: ") + ToString( Bitstream.GetAnnouncedFlippedBits<WORD>( 13 ) ) );
			}
//			OSNodeAppendSize( htitmp, ((nCount + 9) * 10) / 10 );
			Bitstream.Align();
			if ( nwPacketLen > 6 + nCount * 3 + 1 ) // nicht ganz korrekt, möglicherweise...
				Bitstream.SkipBytes( nwPacketLen - (6u + nCount * 3u + 1u) );
			break; }
		case 0xbc: // PES simple Program Stream Map
			SkipPacket( hti, _T("PES Program Stream Map"), header[3] );
			TRACE3( _T("%s (%s): PES Simple Program Stream Map %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
#if 0
class PSMapTable extends PesBase : const unsigned int(32) st_id = ST_PSMapTable 
{
    if (PES_packet_length > 1018) {
        %g{ 
        print_error(); %g}
    }

    bit(1) current_next_indicator;
    bit(2) reserved;
    unsigned int(5) program_stream_map_version;
    bit(7) reserved;
    bit(1) marker = 0b1;
    unsigned int(16) program_stream_info_length;

    unsigned int parsed;
    for (parsed=0; parsed < program_stream_info_length; ) {
        BaseProgramDescriptor program_descriptor;
        parsed += lengthof(program_descriptor);
    }
    
    unsigned int(16) elementary_stream_map_length;
    unsigned int stream_map_ct;
    for (stream_map_ct=0; stream_map_ct < elementary_stream_map_length; ) {
        BaseStreamDescriptor stream_descriptor;
        stream_map_ct += lengthof(stream_descriptor);
    }
    
    unsigned int(32) CRC_32;

    %.c{
    void print_error() { printf("Error in PSMapTable! The PES_packet_length is too high!"); } %.c}
    %.j{
    void print_error() { System.out.println("Error in PSMapTable! The PES_packet_length is too high!"); } %.j}
}

		case 0x02: { // Video Descriptor
			if ( !Bitstream.GetBytes( nbPacketLen ) ) // Byte
				return false;
			htitmp = OSNodeCreate( hti, GetReadPos()-4, _T("Video Descriptor"), _T("0x02"), nbPacketLen+4 );
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			OSNodeCreateValue( htitmp, 4, _T("multiple_frame_rate_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			OSNodeCreateValue( htitmp, 4, _T("frame_rate_code: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
			bool bMPEG1_only_flag = Bitstream.GetAnnouncedBits<BYTE>( 1 );
			OSNodeCreateValue( htitmp, 3, _T("MPEG1_only_flag: ") + ToString( bMPEG1_only_flag ) );
			OSNodeCreateValue( htitmp, 3, _T("constrained_parameter_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			OSNodeCreateValue( htitmp, 3, _T("still_picture_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			if ( bMPEG1_only_flag ) {
				if ( !Bitstream.Announce( 2 * 8 ) )
					return false;
				OSNodeCreateValue( htitmp, 3, _T("profile_and_level_indication: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
				OSNodeCreateValue( htitmp, 4, _T("chroma_format: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				OSNodeCreateValue( htitmp, 4, _T("frame_rate_extension_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				OSNodeCreateValue( htitmp, 5, _T("reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
			}
			break; }
		case 0x03: // Audio Descriptor
			if ( !Bitstream.GetBytes( nbPacketLen ) ) // Byte
				return false;
			htitmp = OSNodeCreate( hti, GetReadPos()-4, _T("Audio Descriptor"), _T("0x03"), nbPacketLen+4 ); 
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			OSNodeCreateValue( htitmp, 4, _T("free_format_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			OSNodeCreateValue( htitmp, 4, _T("ID: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			OSNodeCreateValue( htitmp, 4, _T("layer: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
			OSNodeCreateValue( htitmp, 3, _T("variable_rate_audio_indicator: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			OSNodeCreateValue( htitmp, 4, _T("reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 3 ) ) );
			VERIFY( nbPacketLen - 1 == Bitstream.FindMarker( true ) );
			break;
		case 0x04: { // Hierarchy Descriptor
#if 0
abstract class BaseStreamDescriptor : unsigned int(8) type = 0x00 
{
    unsigned int(8) elementary_stream_id;
    unsigned int(16) elementary_stream_info_length;

    unsigned int parsedBytes;
    for (parsedBytes=0; parsedBytes < elementary_stream_info_length; ) {
        BaseProgramDescriptor descriptor;
        parsedBytes += lengthof(descriptor);
    }
}
class MPEG2AudioStreamDescriptor
#endif
			ReadDumpErrorContext( 0x100, hti );
			htitmp = OSNodeCreate( hti, GetReadPos()-4, _T("MPEG2AudioStream Descriptor"), _T("0x04"), 3+4 ); // TODO
			if ( !Bitstream.Announce( 3 * 8 ) )
				return false;
			OSNodeCreateValue( htitmp, 5, _T("elementary_stream_id: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
			DWORD nInfoLength = Bitstream.GetAnnouncedFlippedBits<WORD>( 16 );
			OSNodeCreateValue( htitmp, 5, _T("elementary_stream_info_length: ") + ToString( nInfoLength ) );
			while ( nInfoLength ) {
				if ( !Bitstream.Announce( 1 * 8 ) )
					return false;
				OSNodeCreateValue( htitmp, 5, _T("type: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
				if ( !Bitstream.GetBytes( nbPacketLen ) ) // Byte
					return false;
				if ( !Bitstream.Announce( nbPacketLen * 8 ) )
					return false;
				OSNodeCreateValue( htitmp, 5, _T("reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				OSNodeCreateValue( htitmp, 4, _T("hierarchy_type: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				OSNodeCreateValue( htitmp, 5, _T("reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				OSNodeCreateValue( htitmp, 4, _T("hierarchy_layer_index: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) );
				OSNodeCreateValue( htitmp, 5, _T("reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				OSNodeCreateValue( htitmp, 4, _T("hierarchy_embedded_layer_index: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) );
				OSNodeCreateValue( htitmp, 5, _T("reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				OSNodeCreateValue( htitmp, 4, _T("hierarchy_channel: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) );
				nInfoLength -= 6;
			}
			break; }
		case 0x05: // Registration Descriptor 
			if ( !Bitstream.GetBytes( nbPacketLen ) ) // Byte
				return false;
			htitmp = OSNodeCreate( hti, GetReadPos()-4, _T("Registration Descriptor"), _T("0x05"), nbPacketLen+4 ); 
			if ( !Bitstream.Announce( nbPacketLen * 8 ) )
				return false;
			OSNodeCreateValue( htitmp, 4, _T("format_identifier: ") + ToString( Bitstream.GetAnnouncedFlippedBits<DWORD>( 32 ) ) );
			nbPacketLen -= 4;
			while ( nbPacketLen-- ) {
				OSNodeCreateValue( htitmp, 4, _T("additional_identification_info: ") + ToString( Bitstream.GetAnnouncedFlippedBits<BYTE>( 8 ) ) );
			}
			VERIFY( 0 == Bitstream.FindMarker( true ) );
			break;
		case 0x03: // Audio Descriptor
			if ( !Bitstream.GetBytes( nbPacketLen ) ) // Byte
				return false;
			htitmp = OSNodeCreate( hti, GetReadPos()-4, _T("Audio Descriptor"), _T("0x03"), nbPacketLen-4 ); 
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			OSNodeCreateValue( htitmp, 4, _T("free_format_flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			VERIFY( nbPacketLen == Bitstream.FindMarker( true ) );
			break;

#endif
			Bitstream.FindMarker( true ); // Sollte eigentlich nicht nötig sein, ist es aber doch...
			break; 
		case 0xbd: // PES complex Private Stream 1, SubPicture or Non-MPEG Audio
			SkipPacket( hti, _T("PES Private Stream 1 (SubPicture, Non-MPEG Audio)"), header[3], true );
			// TODO DVD/VOB, parsen
			break;
		case 0xbe: // PES simple Padding Stream
			SkipPacket( hti, _T("PES Padding Stream"), header[3] );
			break;
		case 0xbf: // PES simple Private Stream 2, Navigation Data
			SkipPacket( hti, _T("PES Private Stream 2 (Navigation)"), header[3] );
			// TODO DVD/VOB, parsen
			break;
		// c0-df PES Audio Stream 1, e0-ef PES Video Stream 1, f0-ff PES data streams
		case 0xf0: // PES simple Data Stream ECM
			SkipPacket( hti, _T("PES DataStream ECM"), header[3] );
			TRACE3( _T("%s (%s): PES Simple Data Stream ECM %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xf1: // PES simple Data Stream
			SkipPacket( hti, _T("PES DataStream EMM"), header[3] );
			TRACE3( _T("%s (%s): PES Simple Data Stream EMM %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xf2: // PES simple Data Stream
			SkipPacket( hti, _T("PES DataStream DSMCC"), header[3] );
			TRACE3( _T("%s (%s): PES Simple Data Stream DSMCC %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xf3: // PES complex Data Stream
			SkipPacket( hti, _T("PES DataStream ISO/IEC13522"), header[3] );
			TRACE3( _T("%s (%s): PES Complex Data Stream ISO/IEC13522 %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xf4: // PES complex Data Stream
		case 0xf5: // PES complex Data Stream
		case 0xf6: // PES complex Data Stream
		case 0xf7: // PES complex Data Stream
		case 0xf8: // PES simple Data Stream
			SkipPacket( hti, _T("PES DataStream ITU-T Rec. H.222.1 type A-E"), header[3] );
			TRACE3( _T("%s (%s): PES Complex Data Stream ITU-T Rec. H.222.1 type A-E %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xf9: // PES complex Data Stream
			SkipPacket( hti, _T("PES DataStream Ancillary"), header[3] ); 
			TRACE3( _T("%s (%s): PES Complex Data Stream ISO/IEC13522 %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xfa: // PES complex Data Stream
		case 0xfb: // PES complex Data Stream
		case 0xfc: // PES complex Data Stream
		case 0xfd: // PES complex Data Stream
		case 0xfe: // PES complex Data Stream
			SkipPacket( hti, _T("DataStream Reserved"), header[3] );
			TRACE3( _T("%s (%s): PES Complex Data Stream Reserved %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
			break;
		case 0xff: // PES simple Data Stream
			SkipPacket( hti, _T("PES DataStream Program Stream Directory"), header[3] );
			TRACE3( _T("%s (%s): PES Simple Data Stream Program Stream Directory %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
#if 0
class PSDirectory extends PesBase : const unsigned int(32) st_id = ST_PSDirectory 
{
    unsigned int(15) number_of_access_units;
    bit(1) marker = 0b1;
    unsigned int(15) previous_directory_offset_1;
    bit(1) marker = 0b1;
    unsigned int(15) previous_directory_offset_2;
    bit(1) marker = 0b1;
    unsigned int(15) previous_directory_offset_3;
    bit(1) marker = 0b1;
    unsigned int(15) previous_directory_offset_4;
    bit(1) marker = 0b1;
    unsigned int(15) previous_directory_offset_5;
    bit(1) marker = 0b1;
    unsigned int(15) previous_directory_offset_6;
    bit(1) marker = 0b1;

    unsigned int i;
    for (i=0; i < number_of_access_units; ++i) {
        unsigned int(8) packet_stream_id;
        int(1) PES_header_position_offset_sign;
        unsigned int(14) PES_header_position_offset_1;
        bit(1) marker = 0b1;
        unsigned int(15) PES_header_position_offset_2;
        bit(1) marker = 0b1;
        unsigned int(15) PES_header_position_offset_3;
        bit(1) marker = 0b1;
        unsigned int(16) reference_offset;
        bit(1) marker = 0b1;
        bit(3) reserved;
        unsigned int(3) PTS_1;
        bit(1) marker = 0b1;
        unsigned int(15) PTS_2;
        bit(1) marker = 0b1;
        unsigned int(15) PTS_3;
        bit(1) marker = 0b1; 
        unsigned int(15) bytes_to_read;
        bit(1) marker = 0b1;
        unsigned int(8) bytes_to_read;
        bit(1) marker = 0b1;
        bit(1) intra_coded_indicator;
        bit(2) coding_parameters_indicator;
        bit(4) reserved;
    }
}
#endif
			break;
		default:
			if ( (header[3] & 0xe0) == 0xc0 ) { // c0-df PES complex Audio Stream
				if ( !SkipPacket( hti, _T("PES Audio Stream ") + ToString( header[3] & 0x1f ), header[3], true ) )
					return false;
			} else if ( (header[3] & 0xf0) == 0xe0 ) { // e0-ef PES complex Video Stream
				if ( !SkipPacket( hti, _T("PES Video Stream ") + ToString( header[3] & 0x1f ), header[3], true ) )
					return false;
			} else { 
				DWORD n = Bitstream.FindMarker( false );
				htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T("Header"), _T("0x") + ToHexString( header[3] ), n + 4 );
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Marker in PES Packet") );
				TRACE3( _T("%s (%s): Undefined PES Header %x.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), header[3] );
				nSize += n;
				if ( !ReadSeekFwdDump( n, (Settings.nVerbosity > 3 ? htitmp : 0) ) )  
					return false;
			} 
		}
	}
}


// d:\media\diverses\movies\extrem\Scat_piss_lez_asspeeh.mpg"
// Das Problem ist, dass auch der Header abgeschnitten werden kann, außerdem ist die Bedingung
// 			WORD n = Bitstream.FindMarker( false );
//			ASSERT( nSize + n <= nMaxSize );
// zu schwach, weil man nicht weiss, ob der Header nicht im nächsten PES weitergeht...
// andersrum gesagt, nur wenn nSize + n < nMaxSize kann man überhaupt eine Aussage treffen, sonst nicht.


bool CMPGParser::CMPGESVideoParser::Parse( int hti, BYTE /*nStreamId*/, DWORD nMaxSize, bool bChunk )
{
	int htitmp;
//	DWORD nCount = 0;
	DWORD w;
	DWORD nCount1 = 0, nCount2 = 0;

	DWORD nSize = 0;
	++nCountChunks;


	if ( nBufferSize > 0 ) {
		WORD n = Bitstream.FindMarker( false );
		if ( n >= nMaxSize ) { // Der ganze Chunk besteht aus weiteren zu buffernden Daten
			BYTE* p = new BYTE[nBufferSize + n];
			memcpy( p, pBuffer, nBufferSize );
			delete[] pBuffer;
			pBuffer = p;
			int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Buffering"), _T(""), n + 4 );  // TODO???
			if ( !m_pParser->ReadBytes( pBuffer + nBufferSize, n, htitmp ) )
				return false;
			if ( m_pParser->Settings.nVerbosity > 4 )
				m_pParser->DumpBytes( pBuffer + nBufferSize, n, htitmp );
			nBufferSize += n;
			return true;
		}
		m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Buffered"), _T(""), nBufferSize );  // TODO???
		if ( !Bitstream.SetBuffer( pBuffer, nBufferSize ) )
			return false;
		nMaxSize += nBufferSize;
		nBufferSize = 0;
		pBuffer = nullptr;
	} else {
		WORD n = Bitstream.FindMarker( false );
		if ( n > 0 ) {
			int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Skipping"), _T(""), n );  // TODO???
			if ( !m_pParser->ReadSeekFwdDump( n, (m_pParser->Settings.nVerbosity > 4 ? htitmp : 0) ) )
				return false;
			nSize += n;
		}
	}

	while ( nSize < nMaxSize ) {
		if ( nCount1++ >= m_pParser->Settings.nTraceMoviEntries && (m_nIFrameSeen == 0 || m_nIFrameSeen > 1) && !bChunk ) {
			if ( m_sGOP.length() > 0 && m_sGOP.length() < 48 )
				m_sGOP += _T("...");
			if ( nSize < nMaxSize ) {
				ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
				int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Skipping"), _T(""), nMaxSize - nSize ); 
				Bitstream.ClearBuffer();
				return m_pParser->ReadSeekFwdDump( nMaxSize - nSize, (m_pParser->Settings.nVerbosity > 4 ? htitmp : 0) );
			}
			return true;
		}

		if ( nSize + 4 > nMaxSize ) {
			nBufferSize = nMaxSize - nSize;
			if ( nBufferSize > 0 ) {
				int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Buffering"), _T(""), nBufferSize ); 
				pBuffer = new BYTE[nBufferSize];
				if ( !m_pParser->ReadBytes( pBuffer, nBufferSize, htitmp ) )
					return false;
				if ( m_pParser->Settings.nVerbosity > 4 )
					m_pParser->DumpBytes( pBuffer, nBufferSize, htitmp );
				return true;
			}
			return true;
		}

		BYTE* header = reinterpret_cast<BYTE*>( &w );
		nSize += 4;
		if ( !Bitstream.GetBytes( header, 4 ) )
			return false;
//		w = FlipBytes( &w );

		if ( header[0] || header[1] || (header[2] != 1) ) {
			if ( (!header[0] && !header[1] && !header[2]) || nCount2++ >= m_pParser->Settings.nTraceMoviEntries ) {
				// TODO m_pParser->ReadSeek( -3 ); geht nicht wegen Bitstream.Buffer...
				nSize += Bitstream.FindMarker( true );
				continue;
			}
			m_pParser->ReadDumpErrorContext( 0x100, hti );
			nSize += Bitstream.FindMarker( true );
			m_pParser->ReadDumpErrorContext( 0x100, hti );
			continue;
		}

		WORD n = Bitstream.FindMarker( false );
		if ( nSize + n >= nMaxSize ) {
			m_pParser->ReadSeek( -4 );
			nBufferSize = nMaxSize - nSize + 4;
			int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Buffering"), _T(""), nBufferSize ); 
			pBuffer = new BYTE[nBufferSize]; 
			if ( !m_pParser->ReadBytes( pBuffer, nBufferSize, htitmp ) )
				return false;
			if ( m_pParser->Settings.nVerbosity > 4 )
				m_pParser->DumpBytes( pBuffer, nBufferSize, htitmp );
			return true;
		}

		m_pParser->OSStatsSet( m_pParser->GetReadPos() );
		switch ( header[3] ) {
		case 0x00: {
			++nCountPackets;
			WORD n = Bitstream.FindMarker( false );
			ASSERT( nSize + n <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Picture Start Code"), _T("0x00"), n + 4 );
			if ( !Bitstream.Announce( 4 * 8 ) )
				return false;
			m_pParser->OSNodeCreateValue( htitmp, 4, _T("Temporal Sequence Nr: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 10 ) ) );
			BYTE frame_type = Bitstream.GetAnnouncedBits<BYTE>( 3 );
			_tstring s;
			switch ( frame_type ) {
			case 0x01:
				s = _T(" (I-Frame)");
				if ( m_sGOP.length() < 50 )
					m_sGOP += _T(" I");
				++m_nIFrameSeen;
				m_pParser->OSNodeAppendImportant( hti );
				m_pParser->OSNodeAppendImportant( hti - 1 );
				break;
			case 0x02:
				s = _T(" (P-Frame)");
				if ( m_sGOP.length() < 50 )
					m_sGOP += _T("P");
				break;
			case 0x03:
				s = _T(" (B-Frame)");
				if ( m_sGOP.length() < 50 )
					m_sGOP += _T("B");
				break;
			default:
				s = _T(" (Undefined)");
				if ( m_sGOP.length() < 50 )
					m_sGOP += _T("?");
				break;
			}
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Frame Type: ") + ToString( frame_type ) + s );
			m_pParser->OSNodeCreateValue( htitmp, 4, _T("VBV Delay: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 16 ) ) );
			if ( frame_type == 2 || frame_type == 3 ) { // TODO enum P, B
				// the fields are mpeg1 only, for mpeg2 its 0111
				if ( !Bitstream.Announce( 4 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("full_pel_forward_vector: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("forward_f_code: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 3 ) ) );
				if ( frame_type == 3 ) { // TODO enum B
					if ( !Bitstream.Announce( 4 ) )
						return false;
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("full_pel_backward_vector: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("backward_f_code: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 3 ) ) );
				}
			} 
			for ( ;; ) { // Additional Data
				if ( Bitstream.IsDry() ) {
					if ( !Bitstream.Announce( 1 ) )
						return false;
				}
				bool bFlag =  Bitstream.GetAnnouncedBits<bool>();
				if ( !bFlag )
					break;
				if ( !Bitstream.Announce( 8 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Additional Data: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>() ) );
			}
			Bitstream.Align(); // ???
			// TODO hier sind noch irgendwelche Daten drin...
			nSize += n;
			Bitstream.FindMarker( true );
			break; }
		// 0x01-0xaf Slice Start Codes
		case 0xb0: // Reserved
		case 0xb1: 
		case 0xb6: {
			WORD n = Bitstream.FindMarker( false );
			ASSERT( nSize + n <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Reserved"), _T("0x") + ToHexString( header[3] ), n + 4 );
			if ( !m_pParser->ReadSeekFwdDump( n, (m_pParser->Settings.nVerbosity > 3 ? htitmp : 0) ) )
				return false;
			nSize += n;
			TRACE3( _T("%s (%s): Reserved Header %x.\n"), _T(__FUNCTION__), m_pParser->m_sIFilename.c_str(), header[3] );
			break; }
		case 0xb2: { // User Data
			WORD n = Bitstream.FindMarker( false );
			ASSERT( nSize + n <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("User Data Start Code"), _T("0xb2"), n + 4 );
			if ( !m_pParser->ReadSeekFwdDump( n, (m_pParser->Settings.nVerbosity > 2 ? htitmp : 0) ) )
				return false;
			nSize += n;
			break; }
		case 0xb3: { // Sequence Header (MPEG Video)
			if (m_pParser->m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN)
				; // TODO Kann ES? 1 oder 2 sein...
			WORD n = Bitstream.FindMarker( false );
			ASSERT( nSize + n <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Sequence Header Code"), _T("0xb3"), n + 4 ); 
			if ( !Bitstream.Announce( 8 * 8 ) )
				return false;
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Horizontal Size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 12 ) ) );
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Vertical Size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 12 ) ) );
			BYTE ar = Bitstream.GetAnnouncedBits<BYTE>( 4 );
			_tstring s;
			switch ( ar ) {
			case 0:
				s = _T(" (forbidden)");
				break;
			case 1:
				s = _T(" (1:1)");
				break;
			case 2:
				s = _T(" (4:3)");
				break;
			case 3:
				s = _T(" (16:9)");
				break;
			case 4:
				s = _T(" (2.21:1)");
				break;
			default:
				s = _T(" (reserved)");
				break;
			}
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Aspect Ratio: ") + ToString( ar ) + s ); 
			BYTE fr = Bitstream.GetAnnouncedBits<BYTE>( 4 );
			switch ( fr ) {
			case 0:
				s = _T(" (forbidden)");
				break;
			case 1:
				s = _T(" (24000/1001 (23.976 fps))");
				break;
			case 2:
				s = _T(" (24 fps)");
				break;
			case 3:
				s = _T(" (25 fps)");
				break;
			case 4:
				s = _T(" (30000/1001 (29.97 fps))");
				break;
			case 5:
				s = _T(" (30 fps)");
				break;
			case 6:
				s = _T(" (50 fps)");
				break;
			case 7:
				s = _T(" (60000/1001 (59.94 fps))");
				break;
			case 8:
				s = _T(" (60 fps)");
				break;
			default:
				s = _T(" (reserved)");
				break;
			}
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Frame Rate: ") + ToString( fr ) + s );
			m_pParser->OSNodeCreateValue( htitmp, 3, _T("Bit Rate: ") + ToString( Bitstream.GetAnnouncedBits<DWORD>( 18 ) ) ); // TODO
			if ( !Bitstream.GetAnnouncedBits<bool>() ) // Skip
				m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			m_pParser->OSNodeCreateValue( htitmp, 3, _T("VBV Buffer Size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 10 ) ) ); 
			m_pParser->OSNodeCreateValue( htitmp, 3, _T("Constrained Parameters Flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) ); 
			bool bFlag = Bitstream.GetAnnouncedBits<bool>();
			m_pParser->OSNodeCreateValue( htitmp, 4, _T("load_intra_quantiser_matrix: ") + ToString( bFlag ) ); 
			if ( bFlag ) {
				if ( !Bitstream.Announce( 8 * 8 * 8 ) )
					return false;
				for ( int i = 0; i < 8; ++i ) // Todo Matrix Index is zig_zag
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("QMatrix: ") + ToHexString( Bitstream.GetAnnouncedBits<QWORD>( 64 ) ) ); // keine Ahnung hier muss 64 stehen, damit nicht nur 1 Bit gelesen wird
			}
			bFlag = Bitstream.GetAnnouncedBits<bool>();
			m_pParser->OSNodeCreateValue( htitmp, 4, _T("load_non_intra_quantiser_matrix: ") + ToString( bFlag ) ); 
			if ( bFlag ) {
				if ( !Bitstream.Announce( 8 * 8 * 8 ) )
					return false;
				for ( int i = 0; i < 8; ++i ) // Todo Matrix Index is zig_zag
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("QMatrix: ") + ToHexString( Bitstream.GetAnnouncedBits<QWORD>( 64 ) ) ); // keine Ahnung hier muss 64 stehen, damit nicht nur 1 Bit gelesen wird
			}
			nSize += n;
			Bitstream.FindMarker( true ); 
			break; }
		case 0xb4: {
			if (m_pParser->m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN)
				m_pParser->m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_PS2;
			else
				ASSERT(m_pParser->m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_PS2);
			WORD n = Bitstream.FindMarker( false );
			ASSERT( nSize + n <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Sequence Error"), _T("0xb4"), n + 4 );
			TRACE3( _T("%s (%s): Sequence Error Header %x.\n"), _T(__FUNCTION__), m_pParser->m_sIFilename.c_str(), header[3] );
			if ( !m_pParser->ReadSeekFwdDump( n, (m_pParser->Settings.nVerbosity > 3 ? htitmp : 0) ) ) // Sollte eigentlich nicht nötig sein, ist es aber doch...
				return false;
			nSize += n;
			break; }
		case 0xb5: { // http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html

			// TODO siehe MPEG-2 Video is138182.pdf, Seite 40ff
			// Dieser Header muss unmittelbar auf 0xb3 "Sequence Header" folgen und gibt es wohl nur in MPEG-2

			WORD n = Bitstream.FindMarker( false );
			ASSERT( nSize + n <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4u, _T("Extension Start Code"), _T("0xb5"), n + 4 );
			if ( !Bitstream.Announce( 8 ) )
				return false;
			BYTE b = Bitstream.GetAnnouncedBits<BYTE>( 4 );
			switch ( b ) {
			case 0x01: { // Sequence_Extension
				if ( !Bitstream.Announce( 5 * 8 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Sequence Extension)") );
				int htitmp2 = m_pParser->OSNodeCreateValue( htitmp, 2, _T("Profile and Level:") );
				m_pParser->OSNodeCreateValue( htitmp2, 2, _T("Escape Bit: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				BYTE profile = Bitstream.GetAnnouncedBits<BYTE>( 3 );
				_tstring s;
				switch ( profile ) {
				case 0x01:
					s = _T(" (High)");
					break;
				case 0x02:
					s = _T(" (Spatially Scalable)");
					break;
				case 0x03:
					s = _T(" (SNR Scalable)");
					break;
				case 0x04:
					s = _T(" (Main)");
					break;
				case 0x05:
					s = _T(" (Simple)");
					break;
				default:
					s = _T(" (Reserved)");
					break;
				}
				m_pParser->OSNodeCreateValue( htitmp2, 2, _T("Profile: ") + ToString( profile ) + s );
				BYTE level = Bitstream.GetAnnouncedBits<BYTE>( 4 );
				switch ( level ) {
				case 0x04:
					s = _T(" (High)");
					break;
				case 0x06:
					s = _T(" (High 1440)");
					break;
				case 0x08:
					s = _T(" (Main)");
					break;
				case 0x0a:
					s = _T(" (Low)");
					break;
				default:
					s = _T(" (Reserved)");
					break;
				}
				m_pParser->OSNodeCreateValue( htitmp2, 2, _T("Level: ") + ToString( level ) + s );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Progressive Sequence: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Chroma Format: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Horizontal Size Extension: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Vertical Size Extension: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Bitrate Extension: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 12 ) ) );
				if ( !Bitstream.GetAnnouncedBits<bool>() ) // Skip
					m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("VBV Buffer Size Extension: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Low Delay: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Frame Rate Extension n: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Frame Rate Extension d: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				Bitstream.Align();
				break; }
			case 0x02: { // MPEG-2
				if ( !Bitstream.Announce( 5 * 8 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Sequence Display Extension)") );
				BYTE vf = Bitstream.GetAnnouncedBits<BYTE>( 3 ) ;
				_tstring s;
				switch( vf ) {
				case 0x1:
					s = _T(" (PAL)");
					break;
				case 0x2:
					s = _T(" (NTSC)");
					break;
				case 0x3:
					s = _T(" (SECAM)");
					break;
				case 0x4:
					s = _T(" (MAC)");
					break;
				case 0x5:
					s = _T(" (Unspecified video format)");
					break;
				default:
					s = _T(" (reserved)");
					break;
				}
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Video Format: ") + ToString( vf ) + s );
				bool bFlag = Bitstream.GetAnnouncedBits<bool>();
				m_pParser->OSNodeCreateValue( htitmp, 3, _T("Color Description Flag: ") + ToString( bFlag ) );
				if ( bFlag ) {
					if ( !Bitstream.Announce( 7 * 8 ) )
						return false;
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("Color Primaries: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("Transfer Characteristics: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("Matrix Coefficients: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
				}
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Display Horizontal Size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 14 ) ) );
				if ( !Bitstream.GetAnnouncedBits<bool>() ) // Skip
					m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Display Vertical Size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 14 ) ) );
				Bitstream.Align();
				break; }
			case 0x03: { // TODO
//				if ( !Bitstream.Announce( 2 * 8 ) )
//					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Quant Matrix Extension)") );
				ASSERT( false );
				Bitstream.FindMarker( true ); 
				break; }
			case 0x04: { // TODO
//				if ( !Bitstream.Announce( 2 * 8 ) )
//					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Copyright Extension)") );
				ASSERT( false );
#if 0
00 00 01 B5 - extension start code

48 04 04 00 

0100        - copyright extension ID
1           - copyright flag
000 0000 0  - copyright id
1           - original or copy
00 0000 0   - reserved data
100 0000 0000 (continue)

00 20 00 00 

100 0000 0000 0000 0000 0    - copyright number 1
010 0000 0000 0000 0000 000  - copyright number 2
0 (continue)

40 00 00

0 0100 0000 0000 0000 0000 0 - copyright number 3
000                          - byte stuffing

00 00 01 B5 - extension start code
#endif
				Bitstream.FindMarker( true ); 
				break; }
			case 0x05: {
				if ( !Bitstream.Announce( 2 * 8 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Sequence Scalable Extension)") );
				BYTE b = Bitstream.GetAnnouncedBits<BYTE>( 2 );
				m_pParser->OSNodeCreateValue( htitmp, 3, _T("scalable_mode: ") + ToString( b ) );
				m_pParser->OSNodeCreateValue( htitmp, 3, _T("layer: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				if ( b == 1 ) {
					if ( !Bitstream.Announce( 5 * 8 ) )
						return false;
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("lower_layer_prediction_h_size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 14 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("lower_layer_prediction_v_size: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 14 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("h_subsampling_facter_m: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("h_subsampling_facter_n: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("v_subsampling_facter_m: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("v_subsampling_facter_n: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
				}else if ( b == 3 ) {
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("picture_mux_enable: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("mux_to_progressive_sequence: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("pixture_mux_order: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 3 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 3, _T("pixture_mux_facter: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 3 ) ) );
				} else {
					ASSERT( false ); // TODO
				}
				break; }
			case 0x07: { // TODO
//				if ( !Bitstream.Announce( 2 * 8 ) )
//					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Picture Display Extension)") );
				ASSERT( false );
#if 0
00 00 01 B5 - extension start code

70 02 4F FF 

0111 picture display extension ID
0000 0000 0010 0100 1111 1111
#endif
				Bitstream.FindMarker( true );
				break; }
			case 0x08: { // MPEG-2
				if ( !Bitstream.Announce( 4 * 8 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Picture Coding Extension)") );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("f_code[0][0] (forward horizontal): ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("f_code[0][1] (forward vertical): ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("f_code[1][0] (backward horizontal): ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("f_code[1][1] (backward vertical): ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 4 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("intra_DC_precision: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("picture_structure: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 3, _T("Top_Field_First: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("frame_pred_frame_dct: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("concealment_motion_vectors: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("q_scale_type: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("intra_vlc_format: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("alternate_scan: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 3, _T("Repeat_First_Field: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("chroma_420_type: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				m_pParser->OSNodeCreateValue( htitmp, 3, _T("progressive_frame: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
				bool bFlag = Bitstream.GetAnnouncedBits<bool>();
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("composite_display: ") + ToString( bFlag ) );
				if ( bFlag ) {
					if ( !Bitstream.Announce( 2 * 8 ) )
						return false;
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("v_axis: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("field_sequence: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 3 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("sub_carrier: ") + ToString( Bitstream.GetAnnouncedBits<bool>( 1 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("burst_amplitude: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 7 ) ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("sub_carrier_phase: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
					if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0 )
						m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 00") );
				} else {
					if ( Bitstream.GetAnnouncedBits<BYTE>( 6 ) != 0 )
						m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 000000") );
				}
				break; }
			case 0x09: { // TODO
//				if ( !Bitstream.Announce( 2 * 8 ) )
//					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (Picture Spatial Scalable Extension)") );
				ASSERT( false );
				Bitstream.FindMarker( true ); 
				break; }
			case 0x0a: { // TODO
//				if ( !Bitstream.Announce( 2 * 8 ) )
//					return false;
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (A Picture Temporal Scalable Extension)") );
				ASSERT( false );
				Bitstream.FindMarker( true ); 
				break; }
			default: // 0, 6, B to F reserved 
				m_pParser->OSNodeCreateValue( htitmp, 2, _T("Extension Type: ") + ToString( b ) + _T(" (undefined)") );
				m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Extension Type: ") + ToString( b ) );
				TRACE3( _T("%s (%s): Reserved Extension Type %x.\n"), _T(__FUNCTION__), m_pParser->m_sIFilename.c_str(), b );
				Bitstream.FindMarker( true ); 
				break;
			}
			nSize += n;
			Bitstream.FindMarker( true );
			break; }
		case 0xb7: 
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Sequence End Code"), _T("0xb7"), 4 );
			break;
		case 0xb8: {
			ASSERT( nSize + 4 <= nMaxSize );
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("GOP Start Code"), _T("0xb8"), 4 + 4 ); 
			if ( !Bitstream.Announce( 4 * 8 ) )
				return false;
			m_pParser->OSNodeCreateValue( htitmp, 3, _T("Drop Frame Flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			_tstring s = ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) + _T(":") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) + _T(":");
			if ( !Bitstream.GetAnnouncedBits<bool>() ) // skip 1 bit
				m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			s += ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) + _T(":") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) );
			m_pParser->OSNodeCreateValue( htitmp, 3, _T("Timecode: ") + s );
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Closed GOP Flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			m_pParser->OSNodeCreateValue( htitmp, 2, _T("Broken GOP Flag: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			Bitstream.Align();
			nSize += 4;
			break; }
		// Stream Ids
		case 0xb9: // program end code
			htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Program End Code"), _T("0xb9"), 4 );
			TRACE3( _T("%s (%s): Programm End Code Header %x.\n"), _T(__FUNCTION__), m_pParser->m_sIFilename.c_str(), header[3] );
			return true;
		default:
			if ( header[3] < 0xb0 ) { // = Slice Vertical Position
				// Video Slice
				DWORD n = Bitstream.FindMarker( false );
				ASSERT( nSize + n <= nMaxSize + 1 ); // either within MaxSize or follwing immediately as next PES Packet Header
				htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Video Slice"), _T("0x") + ToHexString( header[3] ), n + 4 );
				if ( !Bitstream.Announce( 2 * 8 ) )
					return false;
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Slice Vertical Position: ") + ToString( header[3] ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Priority Breakpoint: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 7 ) ) );
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("quantiser_scale_code: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 5 ) ) );
				bool bFlag = Bitstream.GetAnnouncedBits<bool>();
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Intra Slice Flag: ") + ToString( bFlag ) );
				if ( bFlag ) {
					if ( !Bitstream.Announce( 1 * 8 ) )
						return false;
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("Intra Slice: ") + ToString( Bitstream.GetAnnouncedBits<bool>() ) );
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("Reserved Bits: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 7 ) ) ); // TODO 0
				}
				bFlag = Bitstream.GetAnnouncedBits<bool>();
				m_pParser->OSNodeCreateValue( htitmp, 4, _T("Extra Bit Slice: ") + ToString( bFlag ) );
				if ( bFlag ) { // TODO size of 8 bits may be wrong???
					// ASSERT( false );
					if ( !Bitstream.Announce( 1 * 8 ) )
						return false;
					m_pParser->OSNodeCreateValue( htitmp, 4, _T("Extra Information Slice: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) ); // TODO 0
				}
				Bitstream.Align();
				// TODO Skip  Makroblocks
				nSize += n;
				Bitstream.FindMarker( true );
			} else { 
				DWORD n = Bitstream.FindMarker( false );
				ASSERT( nSize + n <= nMaxSize );
				htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos() - 4, _T("Header"), _T("0x") + ToHexString( header[3] ), n + 4 );
				m_pParser->OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unknown Marker in ES Packet") );
				TRACE3( _T("%s (%s): Undefined Marker in ES Packet %x.\n"), _T(__FUNCTION__), m_pParser->m_sIFilename.c_str(), header[3] );
				nSize += n;
				if ( !m_pParser->ReadSeekFwdDump( n, (m_pParser->Settings.nVerbosity > 3 ? htitmp : 0) ) )  
					return false;
			} 
		}
	}
	return true;
}


bool CMPGParser::SkipPacket( int hti, _tstring sTitle, BYTE bType, bool bHasExtension ) 
{
	WORD nPacketLen;
	if ( !Bitstream.GetFlippedWord( nPacketLen ) )
		return false;
	int htitmp = OSNodeCreate( hti, GetReadPos() - 6, sTitle, _T("0x") + ToHexString( bType ), nPacketLen + 6 );
	WORD nPESLen = 0;
#if 0
        case 0xBC:  /* Program stream map */
        case 0xBE:  /* Padding */
        case 0xBF:  /* Private stream 2 */
        case 0xB0:  /* ECM */
        case 0xB1:  /* EMM */
        case 0xFF:  /* Program stream directory */
        case 0xF2:  /* DSMCC stream */
        case 0xF8:  /* ITU-T H.222.1 type E stream */
#endif
	ASSERT( !bHasExtension || (bType != 0xbc && bType != 0xbe && bType != 0xbf && bType != 0xb0 && bType != 0xb1 && bType != 0xff && bType != 0xf2 && bType != 0xf8) );
	if ( bHasExtension )
		nPESLen = DumpPESExtension( htitmp );
	if ( nPacketLen > 0 )
		nPacketLen -= nPESLen;
	else
		nPacketLen = Bitstream.FindMarker( false );

	return ParserFactory.GetParser( bType )->Parse( htitmp, bType, (DWORD)nPacketLen, true ); 
}


WORD CMPGParser::DumpPESExtension( int hti )
{
	WORD nSize = Bitstream.SkipPadding();
//	ASSERT( nSize == 0 );
	if ( !Bitstream.Announce( 1 * 8 ) )
		return false;
	BYTE bType = Bitstream.GetAnnouncedBits<BYTE>( 2 );
	if ( bType == 0x02 ) { // MPEG2
		if ( m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN )
			m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_PES2;
		hti = OSNodeCreateValue( hti, 2, _T("MPEG-2 PES Header: ") + ToString( bType ) );
		nSize += 3;
		if ( !Bitstream.Announce( 2 * 8 ) )
			return false;
		OSNodeCreateValue( hti, 3, _T("PES Scrambling Control: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 2 ) ) );
		OSNodeCreateValue( hti, 4, _T("PES Priority: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
		OSNodeCreateValue( hti, 4, _T("Data Alignment Indicator: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
		OSNodeCreateValue( hti, 4, _T("copyright: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
		OSNodeCreateValue( hti, 4, _T("Original or Copy: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );

		BYTE bPTSDTSFlags = Bitstream.GetAnnouncedBits<BYTE>( 2 );
		OSNodeCreateValue( hti, 3, _T("PTS DTS flags: ") + ToString( bPTSDTSFlags ) );
		bool bESCRFlag = Bitstream.GetAnnouncedBits<bool>();
		OSNodeCreateValue( hti, 4, _T("ESCR flag: ") + ToString( bESCRFlag ) );
		bool bESRateFlag = Bitstream.GetAnnouncedBits<bool>();
		OSNodeCreateValue( hti, 4, _T("ES rate flag: ") + ToString( bESRateFlag ) );
		bool bDSMTrickModeFlag = Bitstream.GetAnnouncedBits<bool>();
		OSNodeCreateValue( hti, 4, _T("DSM trick mode flag: ") + ToString( bDSMTrickModeFlag ) );
		bool bAdditionalCopyInfoFlag = Bitstream.GetAnnouncedBits<bool>();
		OSNodeCreateValue( hti, 4, _T("additional copy info flag: ") + ToString( bAdditionalCopyInfoFlag ) );
		bool bPESCRCFlag = Bitstream.GetAnnouncedBits<bool>();
		OSNodeCreateValue( hti, 4, _T("PES CRC flag: ") + ToString( bPESCRCFlag ) );
		bool bPESExtensionFlag = Bitstream.GetAnnouncedBits<bool>();
		OSNodeCreateValue( hti, 4, _T("PES extension flag: ") + ToString( bPESExtensionFlag ) );
		BYTE nPESDataLength = Bitstream.GetAnnouncedBits<BYTE>( 8 );
		OSNodeCreateValue( hti, 4, _T("PES header data length: ") + ToString( nPESDataLength ) );
		nPESDataLength += (BYTE)nSize;
		if ( bPTSDTSFlags == 0x00 ) {
			// nothing appended
		} else if ( bPTSDTSFlags == 0x01 ) {
			ASSERT( bPTSDTSFlags != 0x01 );
			// TODO illegal...
			ReadDumpErrorContext( 0x100, hti );
//			Bitstream.Align();
		} else if ( bPTSDTSFlags == 0x02 ) {
			nSize += 5;
			if ( !Bitstream.Announce( 5 * 8 ) )
				return false;
			if ( Bitstream.GetAnnouncedBits<BYTE>( 4 ) != 0x02 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 0010") );
			QWORD pts;
			if ( !ReadPSTimestamp( hti, false, pts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> PTS (90kHz): ") + ToString( pts ) );
		} else if ( bPTSDTSFlags == 0x03 ) {
			nSize += 10; 
			if ( !Bitstream.Announce( 10 * 8 ) )
				return false;
			if ( Bitstream.GetAnnouncedBits<BYTE>( 4 ) != 0x03 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 0011") );
			QWORD pts, dts;
			if ( !ReadPSTimestamp( hti, false, pts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> PTS (90kHz): ") + ToString( pts ) );
			if ( Bitstream.GetAnnouncedBits<BYTE>( 4 ) != 0x01 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 0001") );
			if ( !ReadPSTimestamp( hti, false, dts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> DTS (90kHz): ") + ToString( dts ) );
		}
		if ( bESCRFlag ) {
			nSize += 6; 
			if ( !Bitstream.Announce( 6 * 8 ) )
				return false;
			if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0x00 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 00") );
			QWORD escr = 0; 
			if ( !ReadPSTimestamp( hti, true, escr ) )
					goto e;
			OSNodeCreateValue( hti, 3, _T("--> ESCR (27Mhz): ") + ToString( escr ) );
		}
		if ( bESRateFlag ) {
			nSize += 3; 
			if ( !Bitstream.Announce( 3 * 8 ) )
				return false;
			if ( !Bitstream.GetAnnouncedBits<bool>() )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			OSNodeCreateValue( hti, 3, _T("ES Rate: ") + ToString( Bitstream.GetAnnouncedFlippedBits<DWORD>( 22 ) ) );
			if ( !Bitstream.GetAnnouncedBits<bool>() )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
		}
		if ( bDSMTrickModeFlag ) {
			nSize += 1; 
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			OSNodeCreateValue( hti, 3, _T("Trick Mode: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
			// TODO ist nochmal in 3 Bit und abhängig davon der Rest aufgespaltet....
		}
		if ( bAdditionalCopyInfoFlag ) {
			nSize += 1; 
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			if ( !Bitstream.GetAnnouncedBits<bool>() )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
			OSNodeCreateValue( hti, 4, _T("additional copy info: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 7 ) ) );
		}
		if ( bPESCRCFlag ) {
			nSize += 2; 
			if ( !Bitstream.Announce( 2 * 8 ) )
				return false;
			OSNodeCreateValue( hti, 4, _T("previous PES packet CRC: ") + ToString( Bitstream.GetAnnouncedBits<WORD>( 16 ) ) );
		}
		if ( bPESExtensionFlag ) {
			nSize += 1; 
			if ( !Bitstream.Announce( 1 * 8 ) )
				return false;
			bool bPESPrivateDataFlag =  Bitstream.GetAnnouncedBits<bool>();
			OSNodeCreateValue( hti, 4, _T("PES private data flag: ") + ToString( bPESPrivateDataFlag ) );
			bool bPackHeaderFieldFlag =  Bitstream.GetAnnouncedBits<bool>();
			OSNodeCreateValue( hti, 4, _T("pack header field flag: ") + ToString( bPackHeaderFieldFlag ) );
			bool bProgramPacketSequenceCounterFlag =  Bitstream.GetAnnouncedBits<bool>();
			OSNodeCreateValue( hti, 4, _T("program packet sequence counter flag: ") + ToString( bProgramPacketSequenceCounterFlag ) );
			bool bPStdBufferFlag =  Bitstream.GetAnnouncedBits<bool>();
			OSNodeCreateValue( hti, 4, _T("P-STD buffer flag: ") + ToString( bPStdBufferFlag ) );
			if ( Bitstream.GetAnnouncedBits<BYTE>( 3 ) != 0x07 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 111") );
			bool bPESExtensionFlag2 =  Bitstream.GetAnnouncedBits<bool>();
			OSNodeCreateValue( hti, 4, _T("PES extension flag 2: ") + ToString( bPESExtensionFlag2 ) );
			if ( bPESPrivateDataFlag  ) {
				nSize += 16; 
				if ( !Bitstream.Announce( 16 * 8 ) )
					return false;
				BYTE b[16];
				Bitstream.GetBytes( b, 16 );
				if ( Settings.nVerbosity >= 4 )
					DumpBytes( b, 16, hti ); // TODO....
			}
			if ( bPackHeaderFieldFlag ) {
				nSize += 1; 
				if ( !Bitstream.Announce( 1 * 8 ) )
					return false;
				OSNodeCreateValue( hti, 3, _T("Pack Header Field: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
			}
			if ( bProgramPacketSequenceCounterFlag ) {
				nSize += 2; 
				if ( !Bitstream.Announce( 2 * 8 ) )
					return false;
				if ( !Bitstream.GetAnnouncedBits<bool>() )
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
				OSNodeCreateValue( hti, 4, _T("Packet Sequence Counter: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 7 ) ) );
				if ( !Bitstream.GetAnnouncedBits<bool>() )
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
				OSNodeCreateValue( hti, 2, _T("MPEG1_MPEG2 identifier: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				OSNodeCreateValue( hti, 4, _T("original stuffing length: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 6 ) ) );
			}
			if ( bPStdBufferFlag ) {
				nSize += 2; 
				if ( !Bitstream.Announce( 2 * 8 ) )
					return false;
				if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0x01 )
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 01") );
				OSNodeCreateValue( hti, 4, _T("P-STD buffer scale (0 = 128 bytes, 1 = 1024 bytes): ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
				OSNodeCreateValue( hti, 4, _T("P-STD buffer size: ") + ToString( Bitstream.GetAnnouncedFlippedBits<WORD>( 13 ) ) );
			}
			if ( bPESExtensionFlag2 ) {
				nSize += 2; 
				if ( !Bitstream.Announce( 2 * 8 ) )
					return false;
				if ( !Bitstream.GetAnnouncedBits<bool>() )
					OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
				OSNodeCreateValue( hti, 4, _T("PES extension field length: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 7 ) ) );
				OSNodeCreateValue( hti, 5, _T("Reserved: ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 8 ) ) );
			}
		}
		// hier folgt optional Stuffing Bytes 0xff, das erste Byte Payload ist die sub-Streamnr
		if ( nSize < nPESDataLength ) {
			WORD n = (WORD)(nPESDataLength - nSize);
			int htitmp = OSNodeCreateValue( hti, 4, _T("Stuffing: ") + ToString( n ) );
			BYTE* p = new BYTE[n];
			Bitstream.GetBytes( p, n );
			if ( Settings.nVerbosity >= 4 )
				DumpBytes( p, n, htitmp ); 
			delete[] p;
			nSize = nPESDataLength;
		}
	} else if ( bType == 0x00 || bType == 0x01 ) { // MPEG1
		if ( m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN )
			m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_PES1;

		if ( bType == 0x01 ) { 
			hti = OSNodeCreateValue( hti, 2, _T("MPEG-1 PES Header Buffer: ") + ToString( bType ) );
			nSize += 2;
			if ( !Bitstream.Announce( 2 * 8 ) )
				return false;
			OSNodeCreateValue( hti, 4, _T("std_buffer_scale (0 = 128 bytes, 1 = 1024 bytes): ") + ToString( Bitstream.GetAnnouncedBits<BYTE>( 1 ) ) );
			OSNodeCreateValue( hti, 4, _T("std_buffer_size: ") + ToString( Bitstream.GetAnnouncedFlippedBits<WORD>( 13 ) ) );
			if ( Bitstream.GetAnnouncedBits<BYTE>( 2 ) != 0x00 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 00") );
		} else
			hti = OSNodeCreateValue( hti, 2, _T("MPEG-1 PES Header without Buffer: ") + ToString( bType ) );

		BYTE bPTSDTSFlags = Bitstream.GetAnnouncedBits<BYTE>( 2 ); 
		if ( bPTSDTSFlags == 0x00 ) {
			nSize += 1;
			if ( Bitstream.GetAnnouncedBits<BYTE>( 4 ) != 0x0f )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1111") );
			ASSERT( Bitstream.IsAligned() );
			// nothing appended
		} else if ( bPTSDTSFlags == 0x01 ) {
			ASSERT( bPTSDTSFlags != 0x01 ); // TODO dachte, das darf nicht vorkommen
			nSize += 5;
			if ( !Bitstream.Announce( 4 * 8 ) )
				return false;
			QWORD dts;
			if ( !ReadPSTimestamp( hti, false, dts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> DTS (90kHz): ") + ToString( dts ) );
		} else if ( bPTSDTSFlags == 0x02 ) {
			nSize += 5;
			if ( !Bitstream.Announce( 4 * 8 ) )
				return false;
			QWORD pts; 
			if ( !ReadPSTimestamp( hti, false, pts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> PTS (90kHz): ") + ToString( pts ) );
		} else if ( bPTSDTSFlags == 0x03 ) {
			nSize += 10; 
			if ( !Bitstream.Announce( 9 * 8 ) )
				return false;
			QWORD pts, dts;
			if ( !ReadPSTimestamp( hti, false, pts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> PTS (90kHz): ") + ToString( pts ) );
			if ( Bitstream.GetAnnouncedBits<BYTE>( 4 ) != 0x01 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 0001") );
			if ( !ReadPSTimestamp( hti, false, dts ) )
				goto e;
			OSNodeCreateValue( hti, 3, _T("--> DTS (90kHz): ") + ToString( dts ) );
		}
	} else {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unknown PES Header Type: ") + ToString( bType ) );
		ReadDumpErrorContext( 0x100, hti );
e:		Bitstream.Align();
	}

	return nSize;
}

// in kbit/s
static short bitrate_table[][5] = {
// bits V1,L1 V1,L2 V1,L3 V2,L1 V2,L2&L3 
/*0000*/ -1, -1, -1, -1, -1,
/*0001*/ 32, 32, 32, 32, 8, 
/*0010*/ 64, 48, 40, 48, 16, 
/*0011*/ 96, 56, 48, 56, 24, 
/*0100*/ 128, 64, 56, 64, 32, 
/*0101*/ 160, 80, 64, 80, 40, 
/*0110*/ 192, 96, 80, 96, 48, 
/*0111*/ 224, 112, 96, 112, 56, 
/*1000*/ 256, 128, 112, 128, 64, 
/*1001*/ 288, 160, 128, 144, 80, 
/*1010*/ 320, 192, 160, 160, 96, 
/*1011*/ 352, 224, 192, 176, 112, 
/*1100*/ 384, 256, 224, 192, 128, 
/*1101*/ 416, 320, 256, 224, 144, 
/*1110*/ 448, 384, 320, 256, 160, 
/*1111*/ -1, -1, -1, -1, -1,
};

static int sampling_rate_table[][3] =
{
//bits MPEG1 MPEG2 MPEG2.5 
/*00*/ 44100, 22050, 11025, 
/*01*/ 48000, 24000, 12000, 
/*10*/ 32000, 16000, 8000, 
/*11*/ -1, -1, -1,
};

bool CMPGParser::CMPGESAudioParser::Parse( int hti, BYTE nStreamId, DWORD nChunkSize, bool bChunk )
{
	++nCountChunks;
	DWORD nCount = 0;
	while ( nChunkSize > 0 ) {
		++nCountPackets;
		if ( nCount > m_pParser->Settings.nTraceMoviEntries && !bChunk ) {
			ASSERT( Bitstream.IsAligned() && Bitstream.IsDry() );
			int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Skipping"), _T(""), nChunkSize - 4 ); 
			return m_pParser->ReadSeekFwdDump( nChunkSize - 4, (m_pParser->Settings.nVerbosity > 4 ? htitmp : 0) );
		}
		DWORD nSize = nChunkSize;
		int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Audio Frame #") + ToString( nCount++ ), _T("0xfff#") ); 
		m_pParser->OSStatsSet( m_pParser->GetReadPos() );
		if ( !ParseAudioFrame( htitmp, nStreamId, nSize ) )
			return false;
		m_pParser->OSNodeAppendSize( hti, nSize );
		nChunkSize -= nSize;
	}
	return true;
}

bool CMPGParser::CMPGESAudioParser::ParseAudioFrame( int hti, BYTE /*nStreamId*/, DWORD& nMaxSize )
{
	_tstring s;
	DWORD nSizeCorr = nBufferSize;

	if ( pBuffer ) {
		int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Buffered"), _T(""), nBufferSize ); 
		if ( !Bitstream.SetBuffer( pBuffer, nBufferSize ) )
			return false;
//		nMaxSize += nBufferSize;
		nBufferSize = 0;
		pBuffer = nullptr;
	}

	MPEGAUDIOFRAMEHEADER fh;

	if ( nMaxSize < sizeof( fh ) ) {
		ASSERT( pBuffer == nullptr );
		nBufferSize = nMaxSize;
		int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Buffering"), _T(""), nBufferSize ); 
		pBuffer = new BYTE[nBufferSize];
		if ( !m_pParser->ReadBytes( pBuffer, nBufferSize, htitmp ) )
			return false;
		if ( m_pParser->Settings.nVerbosity > 4 )
			m_pParser->DumpBytes( pBuffer, nBufferSize, htitmp );
		return true;
	}


	if ( !Bitstream.GetBytes( (BYTE*)&fh, sizeof( fh ) ) )
		return false;
	if ( fh.frame_sync1 != 0xFF || fh.frame_sync2 != 0x7 ) {
		m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Skipping"), _T(""), nMaxSize );
		m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("No Audio Frame Header found") ); 
		return m_pParser->ReadSeekFwdDump( nMaxSize - 4, (m_pParser->Settings.nVerbosity >= 4 ? hti : 0) );

//		m_pParser->ReadDumpErrorContext( 0x100, hti );
//		return false;
	}

	if (m_pParser->m_eMPEG_TYPE == CMPGParser::MPEG_TYPE::MT_UNKNOWN)
		m_pParser->m_eMPEG_TYPE = CMPGParser::MPEG_TYPE::MT_AUDIO;
	s = _T("Audio Version: ");
	switch ( fh.audio_version ) {
	case 0x0: // Version 2.5
		s += _T("Version 2.5");
		break;
	case 0x1: // Reserved
		m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Audio Version Token!") );
		return false;
		break;
	case 0x2: // Version 2
		s += _T("Version 2");
		break;
	case 0x3: // Version 1
		s += _T("Version 1");
		break;
	}
	m_pParser->OSNodeCreateValue( hti, 2, s );

	s = _T("Layer Description: ");
	switch ( fh.layer_description ) {
	case 0x0: // Reserved
		m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Audio Layer Token!") );
		return false;
		break;
	case 0x1: // Layer III
		s += _T("Layer III");
		break;
	case 0x2: // Layer II
		s += _T("Layer II");
		break;
	case 0x3: // Layer I
		s += _T("Layer I");
		break;
	}
	m_pParser->OSNodeCreateValue( hti, 2, s );

// bits V1,L1 V1,L2 V1,L3 V2,L1 V2,L2&L3 
	int index;
	if ( fh.audio_version == 0x3 ) { // Version 1
		if ( fh.layer_description == 0x3 ) // Layer I
			index = 0;
		else if ( fh.layer_description == 0x2 )
			index = 1;
		else if ( fh.layer_description == 0x1 )
			index = 2;
		else
			index = -1;
	} else if ( fh.audio_version == 0x2 || fh.audio_version == 0x0 ) {
		if ( fh.layer_description == 0x3 ) // Layer I
			index = 4;
		else if ( fh.layer_description == 0x2 || fh.layer_description == 0x1 )
			index = 5;
		else
			index = -1;
	} else
		index = -1;

	if ( index == -1 ) {
		m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Audio Bitrate Token!") );
		return false;
	}
	short bitrate = bitrate_table[fh.bitrate_index][index];
	m_pParser->OSNodeCreateValue( hti, 3, _T("Bitrate: ") + ToString( bitrate ) );
	if ( bitrate == -1 )
		return false;

	if ( fh.audio_version == 0x3 ) // Version 1 
		index = 0;
	else if ( fh.audio_version == 0x2 ) 
		index = 2;
	else if ( fh.audio_version == 0x0 ) 
		index = 1;

	int samplerate = sampling_rate_table[fh.sampling_rate_frequency_index][index];
	m_pParser->OSNodeCreateValue( hti, 3, _T("Sample Rate: ") + ToString( samplerate ) );
	if ( samplerate == -1 ) {
		m_pParser->OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Audio Samplerate Token!") );
		return false;
	}

	WORD padding = 0;
	if ( fh.padding_bit ) {
		if ( fh.layer_description == 0x3 ) // Layer I
			padding = 4;
		else if ( fh.layer_description == 0x2 || fh.layer_description == 0x1 )
			padding = 1;
	}
	m_pParser->OSNodeCreateValue( hti, 4, _T("Padding (Bytes): ") + ToString( padding ) );

	s = _T("Channel Mode: ") + ToString (fh.channel_mode);
	switch ( fh.channel_mode ) {
		case 0x0:
			s += _T(" (Stereo)");
			break;
		case 0x1:
			s += _T(" (Joint Stereo)");
			break;
		case 0x2:
			s += _T(" (Dual Channel)");
			break;
		case 0x3:
			s += _T(" (Single Channel)");
			break;
	}
	m_pParser->OSNodeCreateValue( hti, 2, s );
	m_pParser->OSNodeCreateValue( hti, 3, _T("Emphasis: ") + ToString (fh.emphasis) ); // TODO
	m_pParser->OSNodeCreateValue( hti, 3, _T("Mode Extensions: ") + ToString (fh.mode_extension) ); // TODO
	m_pParser->OSNodeCreateValue( hti, 4, _T("Original: ") + ToString (fh.original) );
	m_pParser->OSNodeCreateValue( hti, 4, _T("Copyright: ") + ToString (fh.copyright) );
	m_pParser->OSNodeCreateValue( hti, 4, _T("Private Bit: ") + ToString (fh.private_bit) );
	m_pParser->OSNodeCreateValue( hti, 4, _T("Protection Bit: ") + ToString (fh.protection_bit) + (fh.protection_bit ? _T(" (no CRC)") : _T(" (16 Bit CRC)") ) );

	DWORD nSize = 0;
	if ( fh.layer_description == 0x3 ) // Layer I
		nSize = (12u * bitrate * 1000u / samplerate + padding) * 4u;
	else if ( fh.layer_description == 0x2 || fh.layer_description == 0x1 )
		nSize = 144u * bitrate * 1000u / samplerate + padding;
	else
		ASSERT( fh.layer_description >= 0x1 && fh.layer_description <= 0x3 );

	if ( nSize > nMaxSize + nSizeCorr ) {
		ASSERT( pBuffer == nullptr );
		nBufferSize = nMaxSize + nSizeCorr;
		pBuffer = new BYTE[nSize - nBufferSize];
		m_pParser->OSNodeCreateValue( hti, 3, _T("Frame Size (remaining): ") + ToString( nSize ) + _T("/") + ToString( nMaxSize ) + _T(" (incl. header)") );
		m_pParser->ReadSeek( -(int)sizeof( fh ) );
		int htitmp = m_pParser->OSNodeCreate( hti, m_pParser->GetReadPos(), _T("Buffering"), _T(""), nBufferSize ); // TODO??? 
		pBuffer = new BYTE[nBufferSize];
		if ( !m_pParser->ReadBytes( pBuffer, nBufferSize, htitmp ) )
			return false;
		if ( m_pParser->Settings.nVerbosity > 4 )
			m_pParser->DumpBytes( pBuffer, nBufferSize, htitmp );
		return true;
	} else
		m_pParser->OSNodeCreateValue( hti, 3, _T("Frame Size: ") + ToString( nSize + nSizeCorr ) + _T(" (incl. header)") );
	nMaxSize = nSize - nSizeCorr;

	WORD xingoffset = 0;
	if ( fh.audio_version == 0x3 ) { // Version 1 
		if ( fh.channel_mode == 0x3 ) // Mono
			xingoffset = 17;
		else
			xingoffset = 32;
	} else if ( fh.audio_version == 0x2 || fh.audio_version == 0x0 ) {
		if ( fh.channel_mode == 0x3 ) // Mono
			xingoffset = 9;
		else
			xingoffset = 17;
	}

	DWORD nPayloadSize = nSize - 4u;
	if (  fh.protection_bit == 0x0 )
		nPayloadSize -= sizeof( WORD );


	BYTE* p = new BYTE[nPayloadSize];
	if ( !Bitstream.GetBytes( p, nPayloadSize ) )
		return false;

	if ( xingoffset > 0 ) {
		DWORD xingtag = *(DWORD*)(p + xingoffset);
		if ( xingtag == 'gniX' || xingtag == 'ofnI' ) { // TODO FCC, XING VBR oder LAME CBR
			xingoffset += sizeof( DWORD );
			DWORD flags = FlipBytes( (DWORD*)(p + xingoffset) );
			m_pParser->OSNodeCreateValue( hti, 3, _T("Xing Flags: ") + ToString( flags ) );
			xingoffset += sizeof( DWORD );
			if ( flags & 0x01 ) { // Frames
				DWORD frames = FlipBytes( (DWORD*)(p + xingoffset) );
				m_pParser->OSNodeCreateValue( hti, 3, _T("Xing Frames: ") + ToString( frames ) );
				xingoffset += sizeof( DWORD );
			}
			if ( flags & 0x02 ) { // Bytes
				DWORD bytes = FlipBytes( (DWORD*)(p + xingoffset) );
				m_pParser->OSNodeCreateValue( hti, 4, _T("Xing Bytes: ") + ToString( bytes ) );
				xingoffset += sizeof( DWORD );
			}
			if ( flags & 0x04 ) { // TOC
				BYTE* bytes = (BYTE*)(p + xingoffset);
				m_pParser->OSNodeCreateValue( hti, 4, _T("Xing TOC:") );
				m_pParser->DumpBytes( bytes, 100, hti );
				xingoffset += 100 * sizeof( BYTE );
			}
			if ( flags & 0x08 ) { // Quality Indicator
				DWORD quality = FlipBytes( (DWORD*)(p + xingoffset) );
				m_pParser->OSNodeCreateValue( hti, 4, _T("QualityIndicator: ") + ToString( quality ) );
				xingoffset += sizeof( DWORD );
			}
			// TODO LAME Tags fehlen noch....
		}
	}

	if ( m_pParser->Settings.nVerbosity > 4 )
		m_pParser->DumpBytes( p, nPayloadSize, hti );
	delete[] p;

	if (  fh.protection_bit == 0x0 ) {
		WORD crc;
		if ( !m_pParser->ReadBytes( &crc, sizeof( crc ), hti ) )
			return false;
		m_pParser->OSNodeCreateValue( hti, 4, _T("CRC: 0x") + ToHexString( crc ) );
	}
	Bitstream.ClearBuffer();

	return true;
}


/*static*/ bool CMPGParser::Probe( BYTE buffer[12] )
{
	DWORD* fcc = reinterpret_cast<DWORD*>( buffer ); 
	// MPEG2 TS, mux, MPEG Video, MP3
	return ((buffer[0] == TRANSPORT_SYNC_BYTE  && buffer[4] != TRANSPORT_SYNC_BYTE)
		|| (buffer[0] != TRANSPORT_SYNC_BYTE  && buffer[4] == TRANSPORT_SYNC_BYTE) // M2TS 192 Bytes
		|| (buffer[0] == 0xff && (buffer[1] & 0xf0) == 0xf0) // MPEG Audio
		|| fcc[0] == 0xba010000 
		|| fcc[0] == 0xb3010000 
		|| fcc[0] == 0x01334449
		|| fcc[0] == 0x02334449
		|| fcc[0] == 0x03334449
		|| fcc[0] == 0x04334449
		|| fcc[0] == 0x05334449); // MP3 (ID3)
}

bool CMPGParser::ProbeTransportPaket(int hti, CMPGParser::MPEG_TYPE eType)
{
	int nPaketSize;
	if (eType == CMPGParser::MPEG_TYPE::MT_TS)
		nPaketSize = 188;
	else if (eType == CMPGParser::MPEG_TYPE::MT_TS_M2TS)
		nPaketSize = 188 + 4;
	else if (eType == CMPGParser::MPEG_TYPE::MT_TS_DVB)
		nPaketSize = 188 + 16;
	else if (eType == CMPGParser::MPEG_TYPE::MT_TS_ATSC)
		nPaketSize = 188 + 20;
	else {
		ASSERT(false);
		return false;
	}

	for (DWORD i = 0; i < 255 && i < nPaketSize; i++) {
		BYTE b;
		if (!ReadBytes(&b, 1, hti))
			return false;
		if (b == TRANSPORT_SYNC_BYTE) {
			if (!ReadSeek(nPaketSize - 1))
				return false;
			if (!ReadBytes(&b, 1, hti))
				return false;
			ReadSeek(-(nPaketSize + 1 - 1));
			if (b != TRANSPORT_SYNC_BYTE)
				continue;
			ReadSeek(-1);
			return true;
		}
	}
	return false;
}

bool CMPGParser::ReadPSTimestamp( int hti, bool bWithExtension, QWORD& r ) // ProgramStream
{
	WORD w = Bitstream.GetAnnouncedBits<BYTE>( 3 );
	OSNodeCreateValue( hti, 4, _T("32..30: ") + ToString( w ) );
	r = (QWORD)w << 30;
	if ( !Bitstream.GetAnnouncedBits<bool>() )
		goto e;
	w = Bitstream.GetAnnouncedBits<WORD>( 15 );
	OSNodeCreateValue( hti, 4, _T("29..15: ") + ToString( w ) );
	r |= (QWORD)w << 15;
	if ( !Bitstream.GetAnnouncedBits<bool>() )
		goto e;
	w = Bitstream.GetAnnouncedBits<WORD>( 15 );
	OSNodeCreateValue( hti, 4, _T("14..00: ") + ToString( w ) );
	r |= w;
	if ( bWithExtension ) {
		if ( !Bitstream.GetAnnouncedBits<bool>() )
			goto e;
		w = Bitstream.GetAnnouncedBits<WORD>( 9 );
		OSNodeCreateValue( hti, 4, _T("ext: ") + ToString( w ) );
		r *= 300; r += w;
	}
	if ( !Bitstream.GetAnnouncedBits<bool>() )
		goto e;
	return true;

e: 	OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Wrong marker, expected 1") );
	TRACE2( _T("%s (%s): Wrong Marker.\n"), _T(__FUNCTION__), m_sIFilename.c_str() );
	return false;
}

bool CMPGParser::ReadTSTimestamp( int hti, QWORD& r ) // Transport Stream PCR, OPCR
{
	WORD w = Bitstream.GetAnnouncedBits<BYTE>( 3 );
	OSNodeCreateValue( hti, 4, _T("32..30: ") + ToString( w ) );
	r = (QWORD)w << 30;
	w = Bitstream.GetAnnouncedBits<WORD>( 15 );
	OSNodeCreateValue( hti, 4, _T("29..15: ") + ToString( w ) );
	r |= (QWORD)w << 15;
	w = Bitstream.GetAnnouncedBits<WORD>( 15 );
	OSNodeCreateValue( hti, 4, _T("14..00: ") + ToString( w ) );
	r |= w;
	Bitstream.GetAnnouncedBits<BYTE>( 6 ); // reserved, 0x16?
	w = Bitstream.GetAnnouncedBits<WORD>( 9 );
	OSNodeCreateValue( hti, 4, _T("ext: ") + ToString( w ) );
	r *= 300; r += w;
	return true;
}

bool CMPGParser::ReadTSTimestampMarker( int hti, QWORD& r ) 
{
	WORD w = Bitstream.GetAnnouncedBits<BYTE>( 3 );
	OSNodeCreateValue( hti, 4, _T("32..30: ") + ToString( w ) );
	r = (QWORD)w << 30;
	Bitstream.GetAnnouncedBits<bool>(); // Marker 0?
	w = Bitstream.GetAnnouncedBits<WORD>( 15 );
	OSNodeCreateValue( hti, 4, _T("29..15: ") + ToString( w ) );
	r |= (QWORD)w << 15;
	Bitstream.GetAnnouncedBits<bool>(); //Marker 1?
	w = Bitstream.GetAnnouncedBits<WORD>( 15 );
	OSNodeCreateValue( hti, 4, _T("14..00: ") + ToString( w ) );
	r |= w;
	Bitstream.GetAnnouncedBits<bool>(); // Marker 0?
	return true;
}

CMPGParser::CMPGESParser* CMPGParser::ParserFactory::GetParser(BYTE nStreamId)
{
	if ( m_vESParser[nStreamId] == nullptr ) {
		if ( (nStreamId & 0xe0) == 0xc0 ) // c0-df PES complex Audio Stream
			m_vESParser[nStreamId] = new CMPGESAudioParser( m_pParser );
		else if ( (nStreamId & 0xf0) == 0xe0 ) // e0-ef PES complex Video Stream
			m_vESParser[nStreamId] = new CMPGESVideoParser( m_pParser );
		else
			m_vESParser[nStreamId] = new CMPGESGenericParser( m_pParser );
	}
	return m_vESParser[nStreamId];
}

void CMPGParser::ParserFactory::Statistics( int hti ) const
{
	for ( size_t i = 0; i < m_vESParser.size(); ++i ) {
		if ( m_vESParser[i] ) {
			int htitmp = m_pParser->OSNodeCreateValue( hti, 1, _T("Stream: 0x") + ToHexString( (BYTE)i ) + _T(" Chunks: ") + ToString( m_vESParser[i]->nCountChunks ) + _T(" Packets: ") + ToString( m_vESParser[i]->nCountPackets ) );
			if ( m_vESParser[i]->m_sGOP.length() > 0 )
				m_pParser->OSNodeCreateValue( htitmp, 1, _T("GOP:") + m_vESParser[i]->m_sGOP );
		}
	}
}