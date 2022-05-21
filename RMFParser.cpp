#include "RMFParser.h"
//#include "AVIParser.h"
#include "RIFFParser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

// https://common.helixcommunity.org/nonav/2003/HCS_SDK_r5/htmfiles/rmff.htm
// TODO Man sieht die Version nirgends


/*static*/ const WORD CRMFParser::m_nMinHeaderSize = static_cast<WORD>( sizeof( DWORD ) + sizeof( DWORD ) + sizeof( WORD ) );


CRMFParser::CRMFParser( class CIOService& ioservice) : CRIFFParser( ioservice )
{
	SetParams( false, true );
}

bool CRMFParser::ReadFile( int hti )
{ 
	if ( ReadChunk( hti, GetReadFileSize()) )
		m_sSummary = _T("This is a Real Media file."); // TODO
	else
		m_sSummary = _T("This might not be a valid Real Media file.");
	return true; // all done
}



bool CRMFParser::ReadChunk( int hti, QWORD nParentSize )
{
FOURCC fcc;
DWORD nSize;
WORD nVersion;
int htitmp;
_tstring s;
	
	while ( nParentSize >= 8 ) {
		OSStatsSet( GetReadPos() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadTypeSizeVersion( fcc, nSize, nVersion, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = (DWORD)nParentSize;
			else 
				nParentSize = nSize;
		} 
		nParentSize -= nSize;

		switch ( fcc ) {
		case FCC('.RMF'): 
			if ( nVersion != 0 && nVersion != 1 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Version V") + ToString( nVersion ) );
			if ( !ReadHrmfChunk( hti, nSize - m_nMinHeaderSize, nVersion ) ) 
				return false;
			break; 
		case FCC('PROP'): 
			if ( nVersion != 0 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Version V") + ToString( nVersion ) );
			if ( !ReadPropChunk( hti, nSize - m_nMinHeaderSize ) ) 
				return false;
			break; 
		case FCC('MDPR'): 
			if ( nVersion != 0 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Version V") + ToString( nVersion ) );
			if ( !ReadMdprChunk( hti, nSize - m_nMinHeaderSize ) ) 
				return false;
			break; 
		case FCC('CONT'): 
			if ( nVersion != 0 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Version V") + ToString( nVersion ) );
			if ( !ReadContChunk( hti, nSize - m_nMinHeaderSize ) ) 
				return false;
			break; 
		case FCC('DATA'): 
			if ( nVersion != 0 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Version V") + ToString( nVersion ) );
			if ( !ReadDataChunk( hti, nSize - m_nMinHeaderSize ) ) 
				return false;
			break; 
		case FCC('INDX'): 
			if ( nVersion != 0 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Version V") + ToString( nVersion ) );
			if ( !ReadIndxChunk( hti, nSize - m_nMinHeaderSize ) ) 
				return false;
			break; 
		case FCC('RMMD'): // CAUTION does not have version!
			ReadSeek( -2 );
			if ( !ReadRmmdChunk( hti, nSize - m_nMinHeaderSize ) ) 
				return false;
			break; 
		case FCC('.SUB'): // Subtitles
//			if ( !ReadHsubChunk( hti, nSize - m_nMinHeaderSize ) ) 
//				return false;
//			break; 
			// TODO es kann am schluss ein ID3V1 Tag folgen 'TAG '
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("V") + ToString( nVersion ) + _T(" Chunk"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			if ( !ReadSeekFwdDump( nSize - m_nMinHeaderSize, htitmp ) )
				return false;
		}
		ASSERT( GetReadTotalBytes() == GetReadPos() );
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}


bool CRMFParser::ReadHrmfChunk( int hti, DWORD nChunkSize, WORD nVersion ) 
{
	if ( nVersion == 0 ) {
		static constexpr DDEF ddef = {
			2, "Real Media File (V0)", "'.RMF'", 8, 8,
			DDEF::DESC::DT_DWORD, 0, 3, "FileVersion", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "NumHeaders", nullptr, nullptr,
		};
		return (Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef ) >= nChunkSize);
	} else if ( nVersion == 1 ) {
		static constexpr DDEF ddef = {
			2, "Real Media File (V1)", "'.RMF'", 8, 8,
			DDEF::DESC::DT_DWORD, 0, 3, "FileVersion", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "NumHeaders", nullptr, nullptr,
		};
		return (Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef ) >= nChunkSize);
	} else
		TRACE3( _T("%s (%s): Wrong Version %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nVersion );
	return true;
}

bool CRMFParser::ReadPropChunk( int hti, DWORD nChunkSize ) 
{
	static constexpr DDEFV ddefv1 =	{
		3, DDEFV::VDT_POSTFIX,
		0x0001, "HX_SAVE_ENABLED", DDEFV::DESC::VDTV_BITFIELD,
		0x0002, "HX_PERFECT_PLAY_ENABLED", DDEFV::DESC::VDTV_BITFIELD,
		0x0004, "HX_LIVE_BROADCAST", DDEFV::DESC::VDTV_BITFIELD,
		0xFFF8, "Reserved Flags", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		11, "Properties (V0)", "'PROP'", 40, 40,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxBitRate", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgBitRate", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxPacketSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgPacketSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "NumPackets", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Duration [ms]", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Preroll [ms]", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexOffset", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "DataOffset", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "NumStreams", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0 | DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv1, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef ) >= nChunkSize);
}

bool CRMFParser::ReadMdprChunk( int hti, DWORD nChunkSize ) 
{
	static constexpr DDEF ddef = {
		14, "Media Properties (V0)", "'MDPR'", 36, 0,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxBitRate", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgBitRate", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxPacketSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgPacketSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "StartTime [ms]", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Preroll [ms]", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Duration [ms]", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, 0, 3, "StreamNameLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "StreamName", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, 0, 3, "MimeTypeLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "MimeType", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "TypeSpecificLength", nullptr, nullptr,
		DDEF::DESC::DT_CALLBACK, 0, 3, "TypeSpecificData", nullptr, nullptr,
//		DDEF::DESC::DT_BYTES, -1, 3, "type_specific_data", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef ) >= nChunkSize);
}

/*virtual*/ DWORD CRMFParser::ParserCallback( const std::string& /*sGuid*/, DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const
{
	if ( vstack[nField-2].s.substr( 0, 7 ) == _T("logical" /*"-fileinfo"*/) ) {
		static constexpr DDEF ddef = {
			22, "logical-fileinfo", "", 20, 0, // TODO, richtiger Name ausgeben
			DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "ObjectVersion", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "NumPhysicalStreams", nullptr, nullptr,
			DDEF::DESC::DT_LOOP, -1, 3, "PhysicalStreams", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "PhysicalStreamNumbers", nullptr, nullptr,
			DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr,
			DDEF::DESC::DT_LOOP, -4, 3, "PhysicalStreams", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "DataOffsets", nullptr, nullptr,
			DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "NumRules", nullptr, nullptr,
			DDEF::DESC::DT_LOOP, -1, 3, "RulesMap", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "RuleToPhysicalStreamMap", nullptr, nullptr,
			DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "NumProperties", nullptr, nullptr,
			DDEF::DESC::DT_NODELOOP, -1, 3, "Properties", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "ObjectVersion", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, 0, 3, "NameLength", nullptr, nullptr,
			DDEF::DESC::DT_MBCSSTRING, -1, 3, "Name", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Type", nullptr, nullptr, // 2 = String
			DDEF::DESC::DT_WORD, 0, 3, "ValueLength", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, -1, 3, "ValueData", nullptr, nullptr,
		};
		return Dump( hti, nSize, buffer + pos, nSize, nSize, &ddef );
	} else if ( vstack[nField-2].s == _T("audio/x-pn-multirate-realaudio") ) {
		if ( !DumpBytes( buffer + pos, nSize, hti ) )
			return 0;
	} else if ( vstack[nField-2].s == _T("audio/x-pn-realaudio") ) {
		static constexpr DDEF ddef = {
			17, "audio/x-pn-realaudio", "", 48, 0, 
			DDEF::DESC::DT_FCC, 0, 3, "FCC", nullptr, nullptr, //.ra\xfd
			DDEF::DESC::DT_WORD, 0, 3, "Version1", nullptr, nullptr, // 4 oder 5
			DDEF::DESC::DT_WORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_FCC, 0, 3, "Compression", nullptr, nullptr, // .ra4, .ra5
			DDEF::DESC::DT_DWORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Version2", nullptr, nullptr, // 4 oder 5
			DDEF::DESC::DT_DWORD, 0, 3, "HeaderSize", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Flavor", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "CodecFrameSize", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "SubPacketH", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "FrameSize", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "SubPacketSize", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_END, 0, 3, "", nullptr, nullptr,
		};
		DWORD ret = Dump( hti, nSize, buffer + pos, nSize, nSize, &ddef );
		if ( ret < 48 )
			return ret;
		if ( nSize > 48 ) {
			if ( !DumpBytes( buffer + pos + 48, nSize - 48, hti + 1 ) )
				return 0;
		}
		return nSize;
#if 0
struct rainfo
{
	DWORD fourcc1;             // '.', 'r', 'a', 0xfd
	WORD version1;            // 4 or 5
	WORD unknown1;            // 00 000
	DWORD fourcc2;             // .ra4 or .ra5
	DWORD unknown2;            // ???
	WORD version2;            // 4 or 5
	DWORD header_size;         // == 0x4e
	WORD flavor;              // codec flavor id
	DWORD coded_frame_size;    // coded frame size
	DWORD unknown3;            // big number
	DWORD unknown4;            // bigger number
	DWORD unknown5;            // yet another number
	WORD sub_packet_h;
	WORD frame_size;
	WORD sub_packet_size;
	WORD unknown6;            // 00 00
	void bswap();
};
struct rainfo4 : rainfo
{
	WORD sample_rate;
	WORD unknown8;            // 0
	WORD sample_size;
	WORD channels;
	void bswap();
};

struct rainfo5 : rainfo
{
	BYTE unknown7[6];          // 0, srate, 0
	WORD sample_rate;
	WORD unknown8;            // 0
	WORD sample_size;
	WORD channels;
	DWORD genr;                // "genr"
	DWORD fourcc3;             // fourcc
	void bswap();
};

#endif
//	} else if ( vstack[nField-2].s == _T("logical-audio/x-pn-multirate-realaudio") ) {
	} else if ( vstack[nField-2].s == _T("video/x-pn-realvideo") ) {
		static constexpr DDEF ddef = {
			11, "video/x-pn-realvideo", "", 34, 0, // TODO
			DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr,
			DDEF::DESC::DT_FCC, 0, 3, "FCC", nullptr, nullptr, // 'VIDO'
			DDEF::DESC::DT_FCC, 0, 3, "Compression", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Width", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "Height", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 3, "bpp", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Unknown", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "fps", nullptr, nullptr, // TODO if fps > 0x10000 fps =/ 0x10000
			DDEF::DESC::DT_FCC, 0, 3, "Type1", nullptr, nullptr,
			DDEF::DESC::DT_FCC, 0, 3, "Type2", nullptr, nullptr,
			DDEF::DESC::DT_END, 0, 3, "", nullptr, nullptr,
//			DDEF::DESC::DT_BYTES, 14, 3, "bytes", nullptr, nullptr,
		};
		DWORD ret = Dump( hti, nSize, buffer + pos, nSize, nSize, &ddef );
		if ( ret < 34 )
			return ret;
		if ( nSize > 34 ) {
			if ( !DumpBytes( buffer + pos + 34, nSize - 34, hti + 1 ) )
				return 0;
		}
		return nSize;
	} 
	else 
		TRACE3( _T("%s (%s): Unknown Type %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), vstack[nField-2].s.c_str() );
	return nSize;
}


bool CRMFParser::ReadContChunk( int hti, DWORD nChunkSize ) 
{
	static constexpr DDEF ddef = {
		8, "Content Description (V0)", "'CONT'", 8, 0,
		DDEF::DESC::DT_WORD, 0, 3, "TitleLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "Title", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "AuthorLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "Author", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "CopyrightLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "Copyright", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "CommentLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "Comment", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef ) >= nChunkSize);
}

bool CRMFParser::ReadIndxChunk( int hti, DWORD nChunkSize ) 
{
	static constexpr DDEF ddef = {
		8, "Index (V0)", "'INDX'", 10, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "NumIndices", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "NextIndexHeader", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -3, 3, "IndexEntries", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "Version", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Timestamp [ms]", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Offset", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "PacketCountForThisPacket", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef, Settings.nTraceIndxEntries ) >= nChunkSize);
}

bool CRMFParser::ReadRmmdChunk( int hti, DWORD nChunkSize ) 
{
	ASSERT( false ); // ungetestet
	static constexpr DDEF ddef = {
		2, "Metadata", "'RMMD'", 8, 8,
		DDEF::DESC::DT_FCC, 0, 3, "Tag", nullptr, nullptr, // 'RJMD'
		DDEF::DESC::DT_DWORD, 0, 3, "ObjectVersion", nullptr, nullptr,
	};
	if ( Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef, Settings.nTraceIndxEntries ) == nChunkSize ) {
		return CRMFParser::ReadRmmdPropertiesChunk( hti + 1, nChunkSize - 8 );
		// Und am Schluss noch ein Footer MetadataSectionFooter{  u_int32        object_id;  u_int32        object_version;  u_int32        size;}
	}
	return false;
}

bool CRMFParser::ReadRmmdPropertiesChunk( int hti, DWORD nChunkSize ) 
{
	static constexpr DDEFV ddefv1 =	{
		10, DDEFV::VDT_POSTFIX,
		{1, "MPT_TEXT", DDEFV::DESC::VDTV_VALUE,
		2, "MPT_TEXTLIST", DDEFV::DESC::VDTV_VALUE, // The value is a separated list of strings, separator specified as sub-property/type descriptor.
		3, "MPT_FLAG", DDEFV::DESC::VDTV_VALUE, // The value is a boolean flag—either 1 byte or 4 bytes, check size value.
		4, "MPT_ULONG", DDEFV::DESC::VDTV_VALUE, // The value is a four-byte integer.
		5, "MPT_BINARY", DDEFV::DESC::VDTV_VALUE, // The value is a byte stream.
		6, "MPT_URL", DDEFV::DESC::VDTV_VALUE, // String
		7, "MPT_DATE", DDEFV::DESC::VDTV_VALUE, // The value is a string representation of the date in the form: YYYYmmDDHHMMSS (m = month, M = minutes).
		8, "MPT_FILENAME", DDEFV::DESC::VDTV_VALUE, // String
		9, "MPT_GROUPING", DDEFV::DESC::VDTV_VALUE, // This property has subproperties, but its own value is empty.
		10, "MPT_REFERENCE", DDEFV::DESC::VDTV_VALUE, // The value is a large buffer of data, use sub-properties/type descriptors to identify mime-type.
		}
	};
	static constexpr DDEFV ddefv2 =	{
		3, DDEFV::VDT_POSTFIX,
		{1, "MPT_READONLY", DDEFV::DESC::VDTV_BITVALUE, // Read only, cannot be modified.
		2, "MPT_PRIVATE", DDEFV::DESC::VDTV_BITVALUE, // Private, do not expose to users.
		4, "MPT_TYPE_DESCRIPTOR", DDEFV::DESC::VDTV_BITVALUE, // Type descriptor used to further define type of value.
		}
	};

	DWORD nSize, nNumProperties;
	if ( !ReadSize( nSize, hti ) )
		return false;
	ReadSeek( -(int)sizeof( nSize ) );
	/*static const*/ DDEF ddef = {
		14, "Properties", "", 32, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr, // &nSize...
		DDEF::DESC::DT_DWORD, 0, 3, "Type", &ddefv1, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "Flags", &ddefv2, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "ValueOffset", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "SubpropertiesOffset", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "NumSubproperties", nullptr, &nNumProperties,
		DDEF::DESC::DT_DWORD, 0, 3, "NameLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "Name", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "ValueLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "Value", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -5, 3, "Subproperties", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Offset", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "NumPropsForName", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr, // TODO und hier folgen num_subproperties mal wieder Properties, also, rekursion!
	};
	ddef.nMaxSize = (WORD)nSize;
		
	if ( Dump( hti, m_nMinHeaderSize, nChunkSize + m_nMinHeaderSize, &ddef, Settings.nTraceIndxEntries ) >= 32 ) {
		for ( DWORD i = 0; i < nNumProperties; i++ )
			if ( !ReadRmmdPropertiesChunk( hti + 1, nSize ) )
				return false;
	}
	return true;
}


bool CRMFParser::ReadDataChunk( int hti, DWORD nChunkSize ) 
{
	// TODO OSNodeAppendImportant( hti ); wenn Keyframe
	static constexpr DDEFV ddefv1 =	{
		3, DDEFV::VDT_POSTFIX,
		0x0001, "HX_RELIABLE_FLAG", DDEFV::DESC::VDTV_BITFIELD,
		0x0002, "HX_KEYFRAME_FLAG", DDEFV::DESC::VDTV_BITFIELD,
		0xFFFC, "Reserved Flags", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		2, nullptr, nullptr, 8, 0, // Print no header
		DDEF::DESC::DT_DWORD, 0, 3, "NumPackets", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "NextDataHeader", nullptr, nullptr,
	};
	hti = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Data (V0)"), _T("'DATA'"), nChunkSize );
	if ( Dump( hti, m_nMinHeaderSize, 8 + m_nMinHeaderSize, &ddef ) < 8 )
		return false;

	nChunkSize -= 8;
	DWORD nCount = 0;
	while ( nChunkSize > 8 ) {
		if ( nCount++ > Settings.nTraceMoviEntries ) {
			return ReadSeekFwdDump( nChunkSize, 0 );
		}
		OSStatsSet( GetReadPos() );
		WORD nVersion, nSize;
		if ( !ReadBytes( &nVersion, 2, hti ) )
			return false;
		nVersion = FlipBytes( &nVersion );
		if ( !ReadBytes( &nSize, 2, hti ) )
			return false;
		nSize = FlipBytes( &nSize );
		ASSERT( nVersion == 0 || nVersion == 1 );
		if ( nSize + 4u > nChunkSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = (WORD)nChunkSize;
			else 
				nChunkSize = nSize;
		} 
		nChunkSize -= nSize;

		if ( nVersion == 0 ) {
			/*static*/ const DDEF ddef = { // darf nicht static sein, wegen nSize...
				5, nullptr, nullptr, 8, 0,
				{DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
				DDEF::DESC::DT_DWORD, 0, 3, "Timestamp [ms]", nullptr, nullptr,
				DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "PacketGroup", nullptr, nullptr,
				DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv1, nullptr,
				DDEF::DESC::DT_BYTES, nSize - 12, 5, "Data", nullptr, nullptr,}
			};
			ASSERT( ddef.desc[4].nLen == nSize - 12 );
			int htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T(""), _T("DataEntry (V0) #") + ToString( nCount ), nSize );
			if ( Dump( htitmp, 4, nSize, &ddef ) + 4 < nSize )
				return false;
		} else if ( nVersion == 1 ) {
			/*static*/ const DDEF ddef = { // darf nicht static sein, wegen nSize...
				5, nullptr, nullptr, 8, 0,
				{DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
				DDEF::DESC::DT_DWORD, 0, 3, "Timestamp [ms]", nullptr, nullptr,
				DDEF::DESC::DT_WORD, 0, 3, "AsmRule", nullptr, nullptr,
				DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "AsmFlags", nullptr, nullptr, // TODO undefined
				DDEF::DESC::DT_BYTES, nSize - 12, 5, "Data", nullptr,}
			};
			ASSERT( ddef.desc[4].nLen == nSize - 12 );
			int htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T(""), _T("DataEntry (V1) #") + ToString( nCount ), nSize );
			if ( Dump( htitmp, 4, nSize, &ddef ) + 4 < nSize )
				return false;
		} else {
			int htitmp = OSNodeCreate( hti, GetReadPos() - 4, _T(""), _T("DataEntry (V") + ToString( nVersion ) + _T(") #") + ToString( nCount ), nSize );
			if ( !ReadSeekFwdDump( nSize - 4, htitmp ) )
				return false;
			continue;
		}
	}
	return true;
}


/*static*/ bool CRMFParser::Probe( BYTE buffer[12] )
{
	FOURCC* fcc = reinterpret_cast<FOURCC*>( buffer ); 
#ifdef _DEBUG
	_tstring s = FCCToString( fcc[0] );
#endif
	return (fcc[0] == 0x464d522e /*'.RMF'*/ );
}

bool CRMFParser::ReadTypeSizeVersion( FOURCC& fcc, DWORD& nSize, WORD& nVersion, int hti )
{
	if ( !ReadTypeSize( fcc, nSize, hti ) )
		return false;
	if ( !ReadBytes( &nVersion, sizeof( WORD ), hti ) )
		return false;
	nVersion = FlipBytes( &nVersion );
	return true;
}
