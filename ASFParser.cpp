#include "Helper.h"
#include "ASFParser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

CASFParser::CASFParser( class CIOService& ioservice) : CParser( ioservice )
{
	SetParams( false, false );
}

bool CASFParser::ReadFile( int hti )
{ 
	if ( ReadChunk( hti, GetReadFileSize()) )
		m_sSummary = _T("This is a Windows Media file."); // TODO
	else
		m_sSummary = _T("This might not be a valid Windows Media file.");
	return true; // all done
}

bool CASFParser::ReadChunk( int hti, QWORD nParentSize )
{
	int htitmp = hti; // ...
	while ( nParentSize >= m_nMinHeaderSize ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		GUID guid;
		QWORD nSize, nDeclaredSize;
		if ( !ReadGUIDSize( guid, nSize, hti ) )
			return false;
		_tstring s = ToString( guid );
		nDeclaredSize = nSize;

		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			nSize = nParentSize;
			nParentSize = 0;
		} else
			nParentSize -= nSize;

		ASSERT( s == _T("{75b22636-668e-11cf-a6d9-00aa0062ce6c}") || nSize <= MAXDWORD );

		if ( s == _T("{75b22630-668e-11cf-a6d9-00aa0062ce6c}") ) 
			DumpHeaderObject( htitmp, (DWORD)nSize );
		else if ( s == _T("{33000890-e5b1-11cf-89f4-00a0c90349cb}") ) 
			DumpSimpleIndexObject( htitmp, (DWORD)nSize );
		else if ( s == _T("{d6e229d3-35da-11d1-9034-00a0c90349be}") ) 
			DumpIndexObject( htitmp, (DWORD)nSize );
		else if ( s == _T("{75b22636-668e-11cf-a6d9-00aa0062ce6c}") ) 
			DumpDataObject( hti, nSize, nDeclaredSize );
		else if ( s == _T("{feb103f8-12ad-4c64-840f-2a1d2f7ad48c}") ) 
			DumpMediaObjectIndexObject( hti, (DWORD)nSize );
		else if ( s == _T("{be7acfcb-97a9-42e8-9c71-999491e3afac}") ) 
			DumpXMPObject( hti, (DWORD)nSize );
		else if ( nSize == 0 || s == _T("{00000000-0000-0000-0000-000000000000}")
			|| s == _T("{20202020-2020-2020-2020-202020202020}") ) // End?
			break;
		// 00370030-0035-0033-3200-390038003500, 00310034-0035-0038-3400-300035003500, 00330038-0037-0039-3600-300035003300 gesehen
		else {
			htitmp = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Undefined"), s, nSize );
			TRACE3(_T("%s (%s): Unknown GUID %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), s.c_str());
			if ( nSize > m_nMinHeaderSize && !ReadSeekFwdDump( nSize - m_nMinHeaderSize ) )
				return false;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CASFParser::DumpHeaderObject( int hti, DWORD nParentSize )
{
	static constexpr DDEFV ddefv2 = {
		1, DDEFV::VDT_INFIX,
		0x02, "Reserved2", DDEFV::DESC::VDTV_VALUE // values unknown
	};
	static constexpr DDEF ddef = {
		3, "Header", "{75b22630-668e-11cf-a6d9-00aa0062ce6c}", 6, 6,
		DDEF::DESC::DT_DWORD, 0, 2, "NumberHeaderObjects", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 4, "Alignment", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 4, "Architecture", &ddefv2, nullptr, // documented as Reserved2
	};
	Dump( hti, m_nMinHeaderSize, m_nMinHeaderSize + 4 + 1 + 1, (DWORD)nParentSize, &ddef ); 

	hti++;
	nParentSize -= 6;
	while ( nParentSize >= m_nMinHeaderSize + m_nMinHeaderSize ) {
		GUID guid;
		QWORD nSize;
		if ( !ReadGUIDSize( guid, nSize, hti ) )
			return false;
		_tstring s = ToString( guid );

		ASSERT( nSize < MAXDWORD );
		if ( nSize > nParentSize - m_nMinHeaderSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			nSize = nParentSize - m_nMinHeaderSize;
			nParentSize = 0;
		} else
			nParentSize -= (DWORD)nSize;

		if ( s == _T("{8cabdca1-a947-11cf-8ee4-00c00c205365}") )
			DumpFilePropertiesObject( hti, (DWORD)nSize );
		else if ( s == _T("{5fbf03b5-a92e-11cf-8ee3-00c00c205365}") )
			DumpHeaderExtensionObject( hti, (DWORD)nSize );
		else if ( s == _T("{75b22633-668e-11cf-a6d9-00aa0062ce6c}") )
			DumpContentDescriptionObject( hti, (DWORD)nSize );
		else if ( s == _T("{b7dc0791-a9b7-11cf-8ee6-00c00c205365}") )
			DumpStreamPropertiesObject( hti, (DWORD)nSize );
		else if ( s == _T("{86d15240-311d-11d0-a3a4-00a0c90348f6}") )
			DumpCodecListObject( hti, (DWORD)nSize );
		else if ( s == _T("{d2d0a440-e307-11d2-97f0-00a0c95ea850}") )
			DumpExtendedContentDescriptionObject( hti, (DWORD)nSize );
		else if ( s == _T("{7bf875ce-468d-11d1-8d82-006097c9a2b2}") )
			DumpStreamBitratePropertiesObject( hti, (DWORD)nSize );
		else if ( s == _T("{2211b3fb-bd23-11d2-b4b7-00a0c955fc6e}") )
			DumpContentEncryptionObject( hti, (DWORD)nSize );
		else if ( s == _T("{298ae614-2622-4c17-b935-dae07ee9289c}") )
			DumpExtendedContentEncryptionObject( hti, (DWORD)nSize );
		else if ( s == _T("{2211b3fc-bd23-11d2-b4b7-00a0c955fc6e}") )
			DumpDigitalSignatureObject( hti, (DWORD)nSize );
		else if ( s == _T("{1efb1a30-0b62-11d0-a39b-00a0c90348f6}") )
			DumpScriptCommandObject( hti, (DWORD)nSize );
		else if ( s == _T("{f487cd01-a951-11cf-8ee6-00c00c205365}") )
			DumpMarkerObject( hti, (DWORD)nSize );
		else if ( s == _T("{1806d474-cadf-4509-a4ba-9aabcb96aae8}") )
			DumpWrongPaddingObject( hti, (DWORD)nSize );
		else if ( s == _T("{d6e229dc-35da-11d1-9034-00a0c90349be}") )
			DumpBitrateMutualExclusionObject( hti, (DWORD)nSize );
		else if ( s == _T("{75b22635-668e-11cf-a6d9-00aa0062ce6c}") )
			DumpErrorCorrectionObject( hti, (DWORD)nSize );
		else if ( s == _T("{2211b3fa-bd23-11d2-b4b7-00a0c955fc6e}") )
			DumpContentBrandingObject( hti, (DWORD)nSize );
		else if (s == _T("{a08649cf-4775-4670-8a16-6e35357566cd}"))
			DumpAdvancedMutualExclusionObject(hti, (DWORD)nSize); // hier wahrscheinlich falsch, tritt auf...
		else if (s == _T("{3cb73fd0-0c4a-4803-953d-edf7b6228f0c}"))
			DumpTimecodeIndexObject(hti, (DWORD)nSize); // hier wahrscheinlich falsch, tritt auf...
		//		else if ( s == _T("{544091f3-7afb-441e-a7f4-8fe217beba2d}") ) // tritt auf, unknown
		// auch 3c20200a-454d-4154-2048-5454502d4551
		// und 786f6a64-6e43-3655-616d-56745a584a6a
		// und 786f6a64-6e43-3655-6148-4e7359585a6c
		// und 41434241-4342-4241-4341-424341424341
//			DumpXXXObject( hti, (DWORD)nSize );
		else {
			int htitmp = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Undefined"), s, nSize );
			TRACE3(_T("%s (%s): Unknown GUID %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), s.c_str());
			if ( !ReadSeekFwdDump( nSize - m_nMinHeaderSize, (Settings.nVerbosity >= 4 ? htitmp : 0) ) )
				return false;
		}
	}
	if ( nParentSize > m_nMinHeaderSize ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CASFParser::DumpIndexObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv1 =	{
		3, DDEFV::VDT_INFIX,
		1, "Nearest Past Data Packet", DDEFV::DESC::VDTV_VALUE,
		2, "Nearest Past Media Object", DDEFV::DESC::VDTV_VALUE,
		3, "Nearest Past Cleanpoint (standard)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		12, "Index", "{d6e229d3-35da-11d1-9034-00a0c90349be}", 26, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryTimeInterval", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexSpecifiersCount", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 2, "IndexBlocksCount", nullptr, nullptr,

		DDEF::DESC::DT_NODELOOP, -2, 0, "IndexSpecifiers", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexType", &ddefv1, nullptr, 
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 

		DDEF::DESC::DT_NODELOOP, -5, 0, "IndexBlocks", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "IndexEntryCount", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, -8, 4, "Position", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -2, 0, "IndexEntries", nullptr, nullptr, // sinnvoll???
		DDEF::DESC::DT_DWORD, -10, 4, "Offset", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
	};
	OSStatsSet( GetReadPos() );
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef, Settings.nTraceIndxEntries ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpMediaObjectIndexObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv1 =	{
		4, DDEFV::VDT_INFIX,
		1, "Nearest Past Data Packet", DDEFV::DESC::VDTV_VALUE,
		2, "Nearest Past Media Object", DDEFV::DESC::VDTV_VALUE,
		3, "Nearest Past Cleanpoint (standard)", DDEFV::DESC::VDTV_VALUE,
		0xFF, "Frame Number Offset", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		14, "Media Object Index", "{feb103f8-12ad-4c64-840f-2a1d2f7ad48c}", 10, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryCountInterval", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "IndexSpecifiersCount", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexBlocksCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -2, 0, "IndexSpecifiers", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 4, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "IndexType", &ddefv1, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
		DDEF::DESC::DT_NODELOOP, -5, 0, "IndexBlocks", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryCount", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, -8, 2, "BlockPositions", nullptr, nullptr, // Array, Anzahl in  Index Specifiers Count
		DDEF::DESC::DT_NODELOOP, -2, 0, "IndexEntries", nullptr, nullptr, // sinnvoll???
		DDEF::DESC::DT_DWORD, -10, 3, "Offsets", nullptr, nullptr, // Array, Anzahl in  Index Specifiers Count
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef, Settings.nTraceIndxEntries ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpTimecodeIndexObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 = {
		1, DDEFV::VDT_INFIX,
		1, "standard", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 =	{
		2, DDEFV::VDT_INFIX,
		2, "Nearest Past Media Object", DDEFV::DESC::VDTV_VALUE,
		3, "Nearest Past Cleanpoint (standard)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		16, "Timecode Index", "{3cb73fd0-0c4a-4803-953d-edf7b6228f0c}", 10, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "Reserved", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "IndexSpecifiersCount", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "IndexBlocksCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -2, 0, "IndexSpecifiers", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 4, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "IndexType", &ddefv2, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
		DDEF::DESC::DT_NODELOOP, -5, 0, "IndexBlocks", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "TimecodeRange", nullptr, nullptr, 
		DDEF::DESC::DT_QWORD, -9, 2, "BlockPositions", nullptr, nullptr, // Array, Anzahl in  Index Specifiers Count
		DDEF::DESC::DT_NODELOOP, -3, 0, "IndexEntries", nullptr, nullptr, // sinnvoll???
		DDEF::DESC::DT_DWORD, 0, 3, "Timecode", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, -12, 3, "Offsets", nullptr, nullptr, // Array, Anzahl in  Index Specifiers Count
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
	};
	OSStatsSet( GetReadPos() );
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef, Settings.nTraceIndxEntries ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpSimpleIndexObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 =	{
		1, DDEFV::VDT_POSTFIX,
		0x0002, "Fragmented", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		7, "Simple Index", "{33000890-e5b1-11cf-89f4-00a0c90349cb}", 56, 0,
		DDEF::DESC::DT_GUID, 0, 3, "FileID", nullptr, nullptr,
		DDEF::DESC::DT_DURATION100NS, 0, 2, "IndexEntryTimeInterval (100ns)", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "MaximumPacketCount", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 1, "IndexEntriesCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 0, "IndexEntries", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 4, "PacketNumber (closest keyframe)", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "PacketCount", &ddefv1, nullptr,
	};
	OSStatsSet( GetReadPos() );
	return ( Dump( hti, m_nMinHeaderSize, nSize, &ddef, Settings.nTraceIndxEntries ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpXMPObject(int hti, DWORD nSize)
{
	nSize -= m_nMinHeaderSize;
	BYTE* const p = new BYTE[nSize]; // TODO ACHTUNG!!!!
	if (!ReadBytes(p, nSize, hti))
		return false;
	hti = OSNodeCreate(hti, GetReadPos() - sizeof(GUID) - m_nMinHeaderSize, _T("XMP"), _T("{be7acfcb-97a9-42e8-9c71-999491e3afac}"), nSize);
	// nSize >= 50, oder 0, wenn Broadcast
	OSNodeCreateValue(hti, _T("00 Content"));
	if (!DumpBytes(p, nSize, hti))
		return false;
	delete[] p;
	return true;
}

bool CASFParser::DumpDataObject( int hti, QWORD nSize, QWORD nDeclaredSize ) 
{
	_tstring s;
	int htitmp;

	QWORD nFilePos = GetReadPos();
	nSize -= m_nMinHeaderSize;

	BYTE* const p = new BYTE[200]; // TODO ACHTUNG!!!!
	if ( !ReadBytes( p, sizeof( GUID ), hti ) )
		return false;
	hti = OSNodeCreate( hti, GetReadPos() - sizeof( GUID ) - m_nMinHeaderSize, _T("Data"), _T("{75b22636-668e-11cf-a6d9-00aa0062ce6c}"), nSize );
	// nSize >= 50, oder 0, wenn Broadcast
	OSNodeCreateValue( hti, _T("00 FileId: ") + ToString( *reinterpret_cast<const GUID*>( p ) ) );
	if ( !ReadBytes( p, sizeof( QWORD ), hti ) )
		return false;
	QWORD nTotalDataPackets = *reinterpret_cast<const QWORD*>( p );
	// muss File Properties Data Packet Count entsprechen
	OSNodeCreateValue( hti, _T("16 TotalDataPackets: ") + ToString( nTotalDataPackets ) );
	WORD nLengthCalc = (WORD)((nDeclaredSize - 26u) / nTotalDataPackets);
	OSNodeCreateValue( hti, _T("--> PayloadPacketSize: ") + ToString( nLengthCalc ) );
	if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
		return false;
	OSNodeCreateValue( hti, _T("24 Alignment: ") + ToHexString( *reinterpret_cast<const BYTE*>( p ) ) );
	// should be 0x01
	if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
		return false;
	OSNodeCreateValue( hti, _T("25 PacketAlignment: ") + ToHexString( *reinterpret_cast<const BYTE*>( p ) ) );
	// should be 0x01

	int htiorg = hti;
	for ( QWORD i = 0; i < nTotalDataPackets && i < Settings.nTraceMoviEntries; ++i ) {
		OSStatsSet( GetReadPos() );
		QWORD pos1 = GetReadPos();
		DWORD nLength = 0;
		hti = OSNodeCreate( htiorg, GetReadPos(), _T("DataPacket #") + ToString( i + 1 ) + _T("/") + ToString( nTotalDataPackets ), _T("Packets"), nLengthCalc );
		nLength += sizeof( BYTE );
		if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
			return false;
		if ( (*p & 0x80) != 0x00 ) {
			BYTE nErrorCorrectionLen = *p & 0x0fu;
			BYTE elt = *p & 0x60u;
			int htiorg = OSNodeCreateValue( hti, _T("ErrorCorrection") );
			htitmp = OSNodeCreateValue( htiorg, _T("ErrorCorrectionFlags: 0x") + ToHexString( *p ) );
			// Len is only valid, if "Error Correction Length Type" is == 0
			OSNodeCreateValue( htitmp, _T("ErrorCorrectionDataLength: ") + ToString( nErrorCorrectionLen ) );
			if ( (elt == 0 && nErrorCorrectionLen != 2) || (elt != 0 && nErrorCorrectionLen != 0) ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for ErrorCorrectionDataLength, expected 0 or 2!") );
//				ReadDumpErrorContext( 128, hti );
				TRACE3( _T("%s (%s): Undefined value for ErrorCorrectionDataLength %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nErrorCorrectionLen );
			}
			OSNodeCreateValue( htitmp, _T("OpaqueDataPresent"), (*p & 0x10) != 0 );
			if ( (*p & 0x10) != 0x00 ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for OpaqueDataPresent, expected 0!") );
				TRACE3( _T("%s (%s): Undefined value for OpaqueDataPresent %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0x10) );
			}
			OSNodeCreateValue( htitmp, _T("ErrorCorrectionLengthType: ") + ToString( elt ) );
			if ( elt != 0x00 ) { // defined for future use
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for ErrorCorrectionLengthType, expected 0!") );
				TRACE3( _T("%s (%s): Undefined value for ErrorCorrectionLengthType %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), elt );
			}
			OSNodeCreateValue( htitmp, _T("ErrorCorrectionPresent"), (*p & 0x80) != 0 );
			if ( nErrorCorrectionLen > 0 ) {
				htitmp = OSNodeCreateValue( htiorg, _T("ErrorCorrectionData (") + ToString( nErrorCorrectionLen ) + _T(")") );
				nLength += sizeof( BYTE );
				if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
					return false;
				s = _T("Type: ") + ToString( *p & 0x0f );
				bool bUncorrected = false;
				switch ( *p & 0x0f ) {
				case 0x00:
					s += _T(" (uncorrected)");
					bUncorrected = true;
					break;
				case 0x01:
					s += _T(" (xor'd)");
					break;
				case 0x02:
					s += _T(" (parity)");
					break;
				default:
					s += _T(" (undefined)");
					TRACE3( _T("%s (%s): Undefined value for EC Type %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0x0f) );
					break;
				}
				OSNodeCreateValue( htitmp, s );
				OSNodeCreateValue( htitmp, _T("Number: ") + ToString( (*p & 0xf0)>>4 ) );
				if ( bUncorrected && (*p & 0xf0) != 0x00 || !bUncorrected && (*p & 0xf0) == 0x00 ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for EC Number!") );
					TRACE3( _T("%s (%s): Undefined value for EC Number %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0xf0)>>4 );
				}
				nLength += sizeof( BYTE );
				if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
					return false;
				OSNodeCreateValue( htitmp, _T("Cycle: ") + ToString( *p ) );
				if ( bUncorrected && *p != 0x00 || !bUncorrected && *p == 0x00 ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for EC Cycle!") );
					TRACE3( _T("%s (%s): Undefined value for EC Cycle %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), *p );
				}
				if ( nErrorCorrectionLen > 2 ) {
					nLength += nErrorCorrectionLen - 2;
					BYTE* pError = new BYTE[nErrorCorrectionLen - 2u];
					htitmp = OSNodeCreateValue( htiorg, _T("Additional Data") );
					ReadDumpBytes( pError, nErrorCorrectionLen - 2u, (Settings.nVerbosity >= 5 ? htitmp : 0) );
					delete[] pError;	
				}
			} // Error Correction Data
			nLength += sizeof( BYTE );
			if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
				return false;
		}
		hti = OSNodeCreate( hti, GetReadPos(), _T("PayloadParsingInformation") );
		BYTE ltf = *p;
		s = _T("LengthTypeFlags: 0x") + ToHexString( *p );
		htitmp = OSNodeCreateValue( hti, s );
		OSNodeCreateValue( htitmp, _T("MultiplePayloadsPresent"), ((ltf & 0x01) != 0) );
		OSNodeCreateValue( htitmp, _T("SequenceType: ") + ToString( (ltf & 0x06)>>1 ) );
		if ( (ltf & 0x06)>>1 != 0x00 ) { 
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for SequenceType, expected 0!") );
			ReadDumpErrorContext( 128, hti );
			TRACE3( _T("%s (%s): Undefined value for SequenceType %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (ltf & 0x06)>>1 );
		}
		OSNodeCreateValue( htitmp, _T("PaddingLengthType: ") + ToString( (ltf & 0x18)>>3 ) );
		OSNodeCreateValue( htitmp, _T("PacketLengthType: ") + ToString( (ltf & 0x60)>>5 ) );
		OSNodeCreateValue( htitmp, _T("ErrorCorrectionPresent"), ((ltf & 0x80) != 0) );
		if ( (ltf & 0x80) != 0 ) { // soll eigentlich ignoriert werden, dieses Bit, wenn Error Correction Data vorhanden ist
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for ErrorCorrectionPresent, expected 0!") );
			TRACE3( _T("%s (%s): Undefined value for ErrorCorrectionPresent %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (ltf & 0x80) );
		}
		nLength += sizeof( BYTE );
		if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
			return false;
		BYTE pf = *p;
		s = _T("PropertyFlags: 0x") + ToHexString( *p );
		htitmp = OSNodeCreateValue( hti, s );
		OSNodeCreateValue( htitmp, _T("ReplicatedDataLengthType: ") + ToString( (*p & 0x03)>>0 ) );
		if ( (*p & 0x03)>>0 != 0x01 ) { 
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for ReplicatedDataLengthType, expected 1!") );
			TRACE3( _T("%s (%s): Undefined value for ReplicatedDataLengthType %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0x03)>>0 );
		}
		OSNodeCreateValue( htitmp, _T("OffsetIntoMediaObjectLengthType: ") + ToString( (*p & 0x0c)>>2 ) );
		if ( (*p & 0x0c)>>2 != 0x03 ) { 
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for OffsetIntoMediaObjectLengthType, expected 3!") );
			TRACE3( _T("%s (%s): Undefined value for OffsetIntoMediaObjectLengthType %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0x0c)>>2 );
		}
		OSNodeCreateValue( htitmp, _T("MediaObjectNumberLengthType: ") + ToString( (*p & 0x30)>>4 ) );
		if ( (*p & 0x30)>>4 != 0x01 ) { 
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for OffsetIntoMediaObjectNumberLengthType, expected 1!") );
			TRACE3( _T("%s (%s): Undefined value for Offset Into Media Object Number Length Type %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0x30)>>4 );
		}
		OSNodeCreateValue( htitmp, _T("StreamNumberLengthType: ") + ToString( (*p & 0xc0)>>6 ) );
		if ( (*p & 0xc0)>>6 != 0x01 ) { 
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for StreamNumberLengthType, expected 1!") );
			TRACE3( _T("%s (%s): Undefined value for StreamNumberLengthType %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (*p & 0xc0)>>6 );
		}
		DWORD nPacketLen = 0;
		switch ( (ltf & 0x60)>>5 ) {
		case 0: // does not exist
			s = _T("PacketLength: not existing");
			break;
		case 1: // BYTE
			nLength += sizeof( BYTE );
			if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
				return false;
			nPacketLen = *reinterpret_cast<const BYTE*>( p );
			s = _T("PacketLength: ") + ToString( nPacketLen );
			break;
		case 2: // WORD
			nLength += sizeof( WORD );
			if ( !ReadBytes( p, sizeof( WORD ), hti ) )
				return false;
			nPacketLen = *reinterpret_cast<const WORD*>( p );
			s = _T("PacketLength: ") + ToString( nPacketLen );
			break;
		case 3: // DWORD
			nLength += sizeof( DWORD );
			if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
				return false;
			nPacketLen = *reinterpret_cast<const DWORD*>( p );
			s = _T("PacketLength: ") + ToString( nPacketLen );
			break;
		}
		OSNodeCreateValue( hti, s );
		switch ( (ltf & 0x06)>>1 ) {
		case 0: // does not exist
			s = _T("Sequence: not existing");
			break;
		case 1: // BYTE
			nLength += sizeof( BYTE );
			if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
				return false;
			s = _T("Sequence: ") + ToString( *reinterpret_cast<const BYTE*>( p ) );
			break;
		case 2: // WORD
			nLength += sizeof( WORD );
			if ( !ReadBytes( p, sizeof( WORD ), hti ) )
				return false;
			s = _T("Sequence: ") + ToString( *reinterpret_cast<const WORD*>( p ) );
			break;
		case 3: // DWORD
			nLength += sizeof( DWORD );
			if ( !ReadBytes( p, sizeof( DWORD ), hti ) ) 
				return false;
			s = _T("Sequence: ") + ToString( *reinterpret_cast<const DWORD*>( p ) );
			break;
		}
		OSNodeCreateValue( hti, s );
		DWORD nPaddingLen = 0;
		switch ( (ltf & 0x18)>>3 ) {
		case 0: // does not exist
			s = _T("PaddingLength: not existing");
			break;
		case 1: // BYTE
			nLength += sizeof( BYTE );
			if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
				return false;
			nPaddingLen = *reinterpret_cast<const BYTE*>( p );
			s = _T("PaddingLength: ") + ToString( nPaddingLen );
			break;
		case 2: // WORD
			nLength += sizeof( WORD );
			if ( !ReadBytes( p, sizeof( WORD ), hti ) )
				return false;
			nPaddingLen = *reinterpret_cast<const WORD*>( p );
			s = _T("PaddingLength: ") + ToString( nPaddingLen );
			break;
		case 3: // DWORD
			nLength += sizeof( DWORD );
			if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
				return false;
			nPaddingLen = *reinterpret_cast<const DWORD*>( p );
			s = _T("PaddingLength: ") + ToString( nPaddingLen );
			break;
		}
		OSNodeCreateValue( hti, s );
		nLength += sizeof( DWORD );
		if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
			return false;
		s = _T("SendTime (1ms): ") + ToString( *reinterpret_cast<const DWORD*>( p ) );
		OSNodeCreateValue( hti, s );
		nLength += sizeof( WORD );
		if ( !ReadBytes( p, sizeof( WORD ), hti ) )
			return false;
		s = _T("Duration (1ms): ") + ToString( *reinterpret_cast<const WORD*>( p ) );
		OSNodeCreateValue( hti, s );

		hti = OSNodeCreate( hti, GetReadPos(), _T("Payload") );
		BYTE pl = 0;
		WORD nNumberOfPayloads = 1;
		if ( (ltf & 0x01) != 0x00 ) { // Multiple Payload
			nLength += sizeof( BYTE );
			if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
				return false;
			pl = *p;
			s = _T("PayloadFlags: 0x") + ToHexString( pl );
			htitmp = OSNodeCreateValue( hti, s );
			nNumberOfPayloads = pl & 0x3fu;
			OSNodeCreateValue( htitmp, _T("NumberOfPayloads: ") + ToString( nNumberOfPayloads ) );
			if ( nNumberOfPayloads == 0 ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for NumberOfPayloads, expected > 0!") );
				TRACE3( _T("%s (%s): Undefined value for NumberOfPayloads %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nNumberOfPayloads );
			}
			OSNodeCreateValue( htitmp, _T("PayloadLengthType: ") + ToString( (pl & 0xc0)>>6 ) );
			if ( (pl & 0xc0)>>6 != 2 ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for PayloadLengthType, expected 2!") );
				TRACE3( _T("%s (%s): Undefined value for PayloadLengthType %d.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), (pl & 0xc0)>>6 );
			}
		}

		ASSERT ( nLength == GetReadPos() - pos1 );

		int htiorg2 = hti;
		for ( WORD j = 0; j < nNumberOfPayloads; ++j ) {
			hti = OSNodeCreate( htiorg2, GetReadPos(), _T("Payload #") + ToString( j + 1 ) + _T("/") + ToString( nNumberOfPayloads ), _T("Payloads"), (nPacketLen > 0 ? nPacketLen : nLengthCalc/nNumberOfPayloads) ); // TODO das stimmt so nicht ganz: nLengthCalc/nNumberOfPayloads
			nLength += sizeof( BYTE );
			if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
				return false;
			s = _T("StreamNumber: 0x") + ToHexString( *reinterpret_cast<const BYTE*>( p ) );
			htitmp = OSNodeCreateValue( hti, s );
			OSNodeCreateValue( htitmp, _T("StreamNumber: ") + ToString( *p & 0x7f ) );
			OSNodeCreateValue( htitmp, _T("Keyframe"), ((*p & 0x80) != 0) );
			if ( (*p & 0x80) != 0 ) 
				OSNodeAppendImportant( hti );

			switch ( (pf & 0x30)>>4 ) {
			case 0: // does not exist
				s = _T("MediaObjectNumber: not existing");
				break;
			case 1: // BYTE
				nLength += sizeof( BYTE );
				if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
					return false;
				s = _T("MediaObjectNumber: ") + ToString( *reinterpret_cast<const BYTE*>( p ) );
				break;
			case 2: // WORD
				nLength += sizeof( WORD );
				if ( !ReadBytes( p, sizeof( WORD ), hti ) )
					return false;
				s = _T("MediaObjectNumber: ") + ToString( *reinterpret_cast<const WORD*>( p ) );
				break;
			case 3: // DWORD
				nLength += sizeof( DWORD );
				if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
					return false;
				s = _T("MediaObjectNumber: ") + ToString( *reinterpret_cast<const DWORD*>( p ) );
				break;
			}
			OSNodeCreateValue( hti, s );
			switch ( (pf & 0x0c)>>2 ) {
			case 0: // does not exist
				s = _T("OffsetIntoMediaObject: not existing");
				break;
			case 1: // BYTE
				nLength += sizeof( BYTE );
				if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
					return false;
				s = _T("OffsetIntoMediaObject: ") + ToString( *reinterpret_cast<const BYTE*>( p ) );
				break;
			case 2: // WORD
				nLength += sizeof( WORD );
				if ( !ReadBytes( p, sizeof( WORD ), hti ) )
					return false;
				s = _T("OffsetIntoMediaObject: ") + ToString( *reinterpret_cast<const WORD*>( p ) );
				break;
			case 3: // DWORD
				nLength += sizeof( DWORD );
				if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
					return false;
				s = _T("OffsetIntoMediaObject: ") + ToString( *reinterpret_cast<const DWORD*>( p ) );
				break;
			}
			OSNodeCreateValue( hti, s );
			DWORD rdl = 0;
			switch ( (pf & 0x03)>>0 ) {
			case 0: // does not exist
				rdl = 0;
				s = _T("ReplicatedDataLength: not existing");
				break;
			case 1: // BYTE
				nLength += sizeof( BYTE );
				if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
					return false;
				rdl = *reinterpret_cast<const BYTE*>( p );
				s = _T("ReplicatedDataLength: ") + ToString( rdl );
				break;
			case 2: // WORD
				nLength += sizeof( WORD );
				if ( !ReadBytes( p, sizeof( WORD ), hti ) )
					return false;
				rdl = *reinterpret_cast<const WORD*>( p );
				s = _T("ReplicatedDataLength: ") + ToString( rdl );
				break;
			case 3: // DWORD
				nLength += sizeof( DWORD );
				if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
					return false;
				rdl = *reinterpret_cast<const DWORD*>( p );
				s = _T("ReplicatedDataLength: ") + ToString( rdl );
				break;
			}
			nLength += rdl;
			if ( rdl == 1 )
				s += _T(" (compressed; OffsetIntoMediaObject is PresentationTime)");
			htitmp = OSNodeCreateValue( hti, s );
			if ( rdl > 1 && rdl < 8 ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for ReplicatedDataLength, expected 0,1,>=8!") );
				TRACE3( _T("%s (%s): Undefined value for ReplicatedDataLength %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), rdl );
			}
			DWORD nPayloadLen = 0; // must be DWORD!
			if ( rdl == 1 ) { // compressed
				if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
					return false;
				OSNodeCreateValue( htitmp, _T("PresentationTimeDelta: ") + ToString( *p ) );
			} else if ( rdl >= 8 ) {
				if ( !ReadBytes( &nPayloadLen, sizeof( DWORD ), hti ) )
					return false;
				OSNodeCreateValue( htitmp, _T("RD: SizeOfMediaObject: ") + ToString( nPayloadLen ) + (nPayloadLen + rdl > nLengthCalc ? _T(" (-") + ToString( rdl ) + _T(")") : _T("")) );
				if ( nPayloadLen + rdl > nLengthCalc )
					nPayloadLen -= rdl;
				if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
					return false;
				OSNodeCreateValue( htitmp, _T("RD: MediaObjectPresentationTime (1ms): ") + ToString( *reinterpret_cast<DWORD*>( p ) ) );
				if ( rdl - 8 > 0 ) {
					if ( !ReadSeekFwdDump( rdl - 8u, (Settings.nVerbosity >= 5 ? htitmp : 0) ) )
						return false;
				}
			} else if ( rdl > 1 ) { // unknown length...
				if ( !ReadSeekFwdDump( rdl, (Settings.nVerbosity >= 5 ? htitmp : 0) ) )
					return false;
			}
			if ( (ltf & 0x01) != 0 ) { // Multiple Payload
				switch ( (pl & 0xc0)>>6 ) {
				case 0: // does not exist
					s = _T("PayloadLength: not existing");
					break;
				case 1: // BYTE
					nLength += sizeof( BYTE );
					if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
						return false;
					nPayloadLen = *reinterpret_cast<const BYTE*>( p );
					s = _T("PayloadLength: ") + ToString( nPayloadLen );
					break;
				case 2: // WORD
					nLength += sizeof( WORD );
					if ( !ReadBytes( p, sizeof( WORD ), hti ) )
						return false;
					nPayloadLen = *reinterpret_cast<const WORD*>( p );
					s = _T("PayloadLength: ") + ToString( nPayloadLen );
					break;
				case 3: // DWORD
					nLength += sizeof( DWORD );
					if ( !ReadBytes( p, sizeof( DWORD ), hti ) )
						return false;
					nPayloadLen = *reinterpret_cast<const DWORD*>( p );
					s = _T("PayloadLength: ") + ToString( nPayloadLen );
					break;
				}
				OSNodeCreateValue( hti, s );
				if ( nPayloadLen == 0 || nPayloadLen > (WORD)-1 ) {
					OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Undefined value for PayloadLength, expected >0, <64k!") );
					TRACE3( _T("%s (%s): Undefined value for PayloadLength %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nPayloadLen );
				}
			}
			//	The number of bytes in this array can be calculated from the overall Packet Length field, and is equal to the Packet Length minus the packet header length, minus the payload header length (including Replicated Data), minus the Padding Length
			if ( nPayloadLen == 0 && nPacketLen > 0 )
				nPayloadLen = nPacketLen;
			if ( nPayloadLen > nLengthCalc || (nPayloadLen != nLengthCalc && (ltf & 0x01) == 0) )
				nPayloadLen = nLengthCalc - nLength - nPaddingLen;
			nLength += nPayloadLen;

			if ( rdl == 1 ) { // Compressed
				WORD k = 0;
				if ( nPacketLen > nPayloadLen )
					nPacketLen -= nPayloadLen;
				else {
					nPacketLen = 0;
				}
				while ( nPayloadLen > 0 ) {
					if ( !ReadBytes( p, sizeof( BYTE ), hti ) )
						return false;
					BYTE l = *p;
					htitmp = OSNodeCreate( hti, GetReadPos() - 1, _T("SubPacketData #") + ToString( k++ ), _T(""), l );
					ASSERT( l < nPayloadLen );
					if ( l >= nPayloadLen ) {
						break;
					}
					if ( !ReadSeekFwdDump( l, (Settings.nVerbosity >= 5 ? htitmp : 0) ) )
						return false;
					nPayloadLen -= l;
					nPayloadLen--; // length byte
				}
				if ( !ReadSeekFwdDump( nPacketLen ) )
					return false;
			} else { // Not Compressed
				htitmp = OSNodeCreate( hti, GetReadPos(), _T("PacketData"), _T("PAYLOAD"), nPayloadLen );

				if ( false ) {
					DWORD n = 4, w = 0;
					if ( !ReadBytes( &w, 4, hti ) )
						return false;
					while ( w != 0x08000082 ) { // 0x82 0x00 0x00 0x08 oder 0x09
						w >>= 8;
						if ( !ReadBytes( reinterpret_cast<BYTE*>( &w ) + 3, 1, hti ) )
							return false;
						++n;
					} 
					ReadDumpErrorContext( 64, hti );
					ReadSeek( -((__int64)nPaddingLen + 4) );
					OSNodeCreateValue( hti, ToString( n - (nPaddingLen + 4) ) + _T("/") + ToString( nPayloadLen ) );
				} 
				else
					if ( !ReadSeekFwdDumpChecksum( nPayloadLen, m_nChecksum, (Settings.nVerbosity >= 5 ? htitmp : 0) ) )
						return false;
			}
		} // Payloads

		ASSERT ( nLength == GetReadPos() - pos1 );

		if ( nLength < nLengthCalc ) 
			nPaddingLen = nLengthCalc - nLength;
		else if ( nLength > nLengthCalc )
			OSNodeCreateAlert( htiorg2, CIOService::EPRIORITY::EP_ERROR, _T("Length mismatch! (") +ToString( nLength ) + _T(", ") + ToString( nLengthCalc ) + _T(")")  );

#if 0 //def _DEBUG // TODO
		if ( (nLength & 1) == 0x01 && nPaddingLen == 0 && nNumberOfPayloads > 1 && nPacketLen == 0 && nLength > nLengthCalc ) // TODO heuristisch Padding bestimmen???
			nPaddingLen++;
//		if ( nLength > nLengthCalc ) 
	//		nLengthCalc = nLength;

		Siehe test.cmd im Debug64-Verzeichnis, keine Ahnung wie ich damit umgehen soll, Kommandozeilen-Option
#endif

		ASSERT( nLength <= nLengthCalc );
		OSNodeCreateValue( htiorg2, _T("PaddingBytes: ") + ToString( nPaddingLen ) );
		if ( !ReadSeekFwdDump( nPaddingLen ) )
			return false;
	}

	delete[] p;

	return ReadSeekFwdDump( nSize - (GetReadPos() - nFilePos) );
}

// --------------------------------------------------------------------------------------------------

bool CASFParser::DumpMarkerObject( int hti, DWORD nSize )
{
	static const DDEFV ddefv1 = {
		1, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{4cfedb20-75f6-11cf-9c0f-00a0c90349cb}", "Reserved4", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 = {
		1, DDEFV::VDT_INFIX,
		0, "standard", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		13, "Marker", "{f487cd01-a951-11cf-8ee6-00c00c205365}", m_nMinHeaderSize, 0,
		DDEF::DESC::DT_GUID, 0, 4, "MarkerID", &ddefv1, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MarkersCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "Alignment", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "NameLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -1, 1, "Name", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, 0, 0, "Markers", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, 0, 3, "Offset", nullptr, nullptr,
		DDEF::DESC::DT_FILETIME1601, 0, 3, "PresentationTime (100ns)", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "EntryLength", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "SendTime (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv2, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 4, "MarkerDescriptionLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINWORDS, -1, 1, "MarkerDescription", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef, Settings.nTraceMoviEntries ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpScriptCommandObject( int hti, DWORD nSize )
{
	static const DDEFV ddefv1 = {
		1, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{4b1acbe3-100b-11d0-a39b-00a0c90348f6}", "Reserved3", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		12, "Script Command", "{1efb1a30-0b62-11d0-a39b-00a0c90348f6}", 20, 0,
		DDEF::DESC::DT_GUID, 0, 4, "CommandID", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "CommandsCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "CommandTypesCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "CommandTypes", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "CommandTypeNameLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINWORDS, -1, 3, "CommandTypeName", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr, 
		DDEF::DESC::DT_NODELOOP, -5, 3, "Commands", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "PresentationTime", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "TypeIndex", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "CommandNameLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINWORDS, -1, 3, "CommandName", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef, Settings.nTraceMoviEntries ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpContentEncryptionObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		8, "Content Encryption", "{2211b3fb-bd23-11d2-b4b7-00a0c955fc6e}", 17, 0,
		DDEF::DESC::DT_DWORD, 0, 2, "SecretDataLength", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 2, "SecretData", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "ProtectionTypeLength", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 2, "ProtectionType", nullptr, nullptr, // TODO char
		DDEF::DESC::DT_DWORD, 0, 4, "KeyIDLength", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 3, "KeyID", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "LicenseURLLength", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 2, "LicenseURL", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpDigitalSignatureObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv = {
		1, DDEFV::VDT_INFIX,
		2, "standard", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		3, "Digital Signature", "{2211b3fc-bd23-11d2-b4b7-00a0c955fc6e}", 8, 0,
		DDEF::DESC::DT_DWORD, 0, 2, "SignatureType", &ddefv, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "SignatureDataLength", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 4, "Signature Data", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpExtendedContentEncryptionObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		2, "Extended Content Encryption", "{298ae614-2622-4c17-b935-dae07ee9289c}", 5, 0,
		DDEF::DESC::DT_DWORD, 0, 2, "DataSize", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, 0, 2, "Data", nullptr, nullptr,
//		DDEF::DESC::DT_BYTES, -1, 2, "Data", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpStreamBitratePropertiesObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv = {
		1, DDEFV::VDT_POSTFIX,
		0x007f, "StreamNumber", DDEFV::DESC::VDTV_BITVALUE
	};
	static constexpr DDEF ddef = {
		4, "Stream Bitrate Properties", "{7bf875ce-468d-11d1-8d82-006097c9a2b2}", 2, 0,
		DDEF::DESC::DT_WORD, 0, 3, "BitrateRecordsCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, 0, 0, "BitrateRecords", nullptr, nullptr,
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 2, "Flags", &ddefv, nullptr, // hex
		DDEF::DESC::DT_DWORD, 0, 0, "Average Bitrate", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

DWORD CASFParser::DumpTypeParserCallback( DWORD /*nSize*/, const BYTE* buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const
{ 
	_tstring s;
	switch ( vstack[nField - 2].dw ) {
	case 0x0000:
		OSNodeCreateValue( hti, _T("\"") + ToStringFromUTF16( reinterpret_cast<const char16*>( buffer + pos ), (size_t)vstack[nField - 1].dw ) + _T("\"") );
		break;
	case 0x0002:
		OSNodeCreateValue( hti, ToString( *reinterpret_cast<const bool*>( buffer + pos ) ) );
		break;
	case 0x0003:
		OSNodeCreateValue( hti, ToString( *reinterpret_cast<const DWORD*>( buffer + pos ) ) );
		break;
	case 0x0004:
		OSNodeCreateValue( hti, ToString( *reinterpret_cast<const QWORD*>( buffer + pos ) ) );
		break;
	case 0x0005:
		OSNodeCreateValue( hti, ToString( *reinterpret_cast<const WORD*>( buffer + pos ) ) );
		break;
	case 0x0006:
		OSNodeCreateValue( hti, ToString( *reinterpret_cast<const GUID*>( buffer + pos ) ) );
		break;
	default:
		TRACE3( _T("%s (%s): Undefined value for Dump Type %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), vstack[nField - 2].dw );
		// fall through...
	case 0x0001: // Byte Array
		DumpBytes( buffer + pos, vstack[nField - 1].dw, hti );
		break;
	}
	return vstack[nField - 1].dw;
}

bool CASFParser::DumpExtendedContentDescriptionObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		7, "Extended Content Description", "{d2d0a440-e307-11d2-97f0-00a0c95ea850}", 2, 0,
		DDEF::DESC::DT_WORD, 0, 3, "ContentDescriptorsCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, 0, 0, "ContentDescriptor", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "DescriptorNameLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -1, 2, "DescriptorName", nullptr, nullptr,
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 2, "DescriptorValueDataType", &ddefvType, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "DescriptorValueLength", nullptr, nullptr,
//		DDEF::DESC::DT_BYTES, -1, 2, "DescriptorValue", nullptr, nullptr,
		DDEF::DESC::DT_CALLBACK, 0, 2, "DescriptorValue", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpCodecListObject( int hti, DWORD nSize ) 
{
	static const DDEFV ddefv1 =	{
		1, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{86d15241-311d-11d0-a3a4-00a0c90348f6}", "Reserved2", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 =	{
		3, DDEFV::VDT_INFIX,
		0x0001, "WMT_CODECINFO_VIDEO", DDEFV::DESC::VDTV_VALUE,
		0x0002, "WMT_CODECINFO_AUDIO", DDEFV::DESC::VDTV_VALUE,
		0xffff, "Undefined", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		10, "Codec List", "{86d15240-311d-11d0-a3a4-00a0c90348f6}", 20, 0,
		DDEF::DESC::DT_GUID, 0, 4, "Codec D", &ddefv1, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "CodecEntriesCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, 0, 0, "CodecEntries", nullptr, nullptr,
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 2, "Type", &ddefv2, nullptr, 
		DDEF::DESC::DT_WORD, 0, 4, "CodecNameLength", nullptr, nullptr, 
		DDEF::DESC::DT_UTF16STRING_LENGTHINWORDS, -1, 2, "CodecName", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "CodecDescriptionLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINWORDS, -1, 2, "CodecDescription", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "CodecInformationLength", nullptr, nullptr, 
		DDEF::DESC::DT_BYTES, -1, 2, "CodecInformation (Format/FourCC)", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}


/*virtual*/ DWORD CASFParser::ParserCallback( const std::string& sGuid, DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const
{
	if ( sGuid == "{b7dc0791-a9b7-11cf-8ee6-00c00c205365}" ) 
		return DumpStreamPropertiesObjectParserCallback( nSize, buffer, pos, hti, vstack );
	else if ( sGuid == "{d2d0a440-e307-11d2-97f0-00a0c95ea850}" ) // Description Object
		return DumpTypeParserCallback( nSize, buffer, pos, hti, vstack, nField );
	else if ( sGuid == "{44231c94-9498-49d1-a141-1d134e457054}" || // Matadata Library
		sGuid == "{c5f8cbea-5baf-4877-8467-aa8c44fa4cca}" ) // Metadata
		return DumpTypeParserCallback( nSize, buffer, pos, hti, vstack, nField - 1 );
	else if ( sGuid == "{14e6a5cb-c672-4332-8399-a96952065b5a}" ) { // Extended Stream Properties
		if ( nSize == 0 )
			return 0;
		else if ( nSize > m_nMinHeaderSize )
			DumpStreamPropertiesObject( hti, nSize, buffer + pos + m_nMinHeaderSize );
		else
			ASSERT( nSize > m_nMinHeaderSize );
		return nSize;
	}
	TRACE3( _T("%s (%s): Undefined value for GUID %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), ToWideString(sGuid).c_str() );
	return 0;
}

DWORD CASFParser::DumpStreamPropertiesObjectParserCallback( DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& /*vstack*/ ) const
{
	_tstring guid = ToString( *reinterpret_cast<const GUID*>( buffer ) ); // StreamType GUID am Packetanfang abfragen...
	if ( guid == _T("{bc19efc0-5b4d-11cf-a8fd-00805f5c442b}") ) { // Video
		static constexpr DDEFV ddefv1 =	{
			1, DDEFV::VDT_INFIX,
			0x02, "standard", DDEFV::DESC::VDTV_VALUE,
		};
		static constexpr DDEF ddef = {
			4, "Video", "{bc19efc0-5b4d-11cf-a8fd-00805f5c442b}", 11, 11,
			DDEF::DESC::DT_DWORD, 0, 2, "EncodedImageWidth", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 2, "EncodedImageHeight", nullptr, nullptr,
			DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 4, "ReservedFlags", &ddefv1, nullptr,
			DDEF::DESC::DT_WORD, 0, 2, "FormatDataSize", nullptr, nullptr,
		};
		if ( Dump( hti, nSize, buffer + pos, 11, nSize, &ddef ) != 11 )
			return nSize;
		DWORD nWidth, nHeight;
		VERIFY(DumpBitmapinfo(nWidth, nHeight, nSize - 11, buffer, pos + 11, hti + 1, true) == nSize - 11);
		return nSize;
	} else if ( guid == _T("{f8699e40-5b4d-11cf-a8fd-00805f5c442b}") ) {
		static constexpr DDEF ddef = {
			8, "Audio", "{f8699e40-5b4d-11cf-a8fd-00805f5c442b}", 0, 0,
			DDEF::DESC::DT_WORD, 0, 4, "CodecID / FormatTag", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 4, "NumberOfChannels", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "SamplesPerSecond", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "AverageNumberOfBytesPerSecond", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 2, "BlockAlignment", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 2, "BitsPerSample", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 2, "CodecSpecificDataSize", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, -1, 4, "CodecSpecificData", nullptr, nullptr,
		};
		Dump(hti, nSize, buffer + pos, nSize, nSize, &ddef);
		return nSize;
	} else {
		hti = OSNodeCreate( hti, GetReadPos() - nSize + pos, _T("Undefined"), guid, nSize ); 
		if ( !DumpBytes( buffer + pos, nSize, hti ) )
			return nSize;
		return nSize;
	}
}

bool CASFParser::DumpStreamPropertiesObject( int hti, DWORD nSize ) 
{
	nSize -= m_nMinHeaderSize;
	BYTE* p = new BYTE[nSize];
	if ( !ReadBytes( p, nSize, hti ) )
		return false;
	bool b = DumpStreamPropertiesObject( hti, nSize + m_nMinHeaderSize, p );
	delete[] p;
	return b;
}

bool CASFParser::DumpStreamPropertiesObject( int hti, DWORD nSize, const BYTE* buffer ) const
{
	static const DDEFV ddefv1 =	{
		7, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{bc19efc0-5b4d-11cf-a8fd-00805f5c442b}", "Video", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{f8699e40-5b4d-11cf-a8fd-00805f5c442b}", "Audio", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{59dacfc0-59e6-11d0-a3ac-00a0c90348f6}", "Command", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{b61be100-5b4e-11cf-a8fd-00805f5c442b}", "JFIF", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{35907de0-e415-11cf-a917-00805f5c442b}", "Degradable JPEG", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{91bd222c-f21c-497a-8b6d-5aa86bfc0185}", "File Transfer", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{3afb65e2-47ef-40f2-ac2c-70a90d71d343}", "Binary", DDEFV::DESC::VDTV_VALUE,
		
	};
	static const DDEFV ddefv2 =	{
		3, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{20fb5700-5b55-11cf-a8fd-00805f5c442b}", "None", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{bfc3cd50-618f-11cf-8bb2-00aa00b4e220}", "Spread", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{49f1a440-4ece-11d0-a3ac-00a0c90348f6}", "Signature (undocumented)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv3 =	{
		2, DDEFV::VDT_POSTFIX,
		0x8000, "Encryption", DDEFV::DESC::VDTV_BITFIELD,
		0x007f, "StreamNumber", DDEFV::DESC::VDTV_BITVALUE,
	};
	if ( ToString( *(const GUID*)(buffer + 16) ) == _T("{bfc3cd50-618f-11cf-8bb2-00aa00b4e220}") ) {
		static constexpr DDEF ddef = {
			14, "Stream Properties", "{b7dc0791-a9b7-11cf-8ee6-00c00c205365}", 54, 0,
			DDEF::DESC::DT_GUID, 0, 2, "StreamType", &ddefv1, nullptr,
			DDEF::DESC::DT_GUID, 0, 4, "ErrorCorrectionType", &ddefv2, nullptr,
			DDEF::DESC::DT_QWORD, 0, 4, "TimeOffset", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "TypeSpecificDataLength", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "ErrorCorrectionDataLength", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 2, "Flags", &ddefv3, nullptr, 
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 4, "SecurityID", nullptr, nullptr, 
			DDEF::DESC::DT_CALLBACK, -4, 3, "TypeSpecificData", nullptr, nullptr, 

			// Wenn Audio, dann null oder ASF_SPREAD_AUDIO und ErrorCorrectionData ist dann
			DDEF::DESC::DT_NODELOOP, -4, 3, "ErrorCorrectionData", nullptr, nullptr, // Ist nicht die Anzahl da keine Loop, macht aber nichts...
			DDEF::DESC::DT_BYTE, 0, 4, "Span", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 4, "VirtualPacketLength", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 4, "VirtualChunkLength", nullptr, nullptr,
			DDEF::DESC::DT_WORD, 0, 4, "SilenceDataLength", nullptr, nullptr, // should be 1
			DDEF::DESC::DT_BYTES, -1, 4, "Data", nullptr, nullptr,
		};
		return (Dump( hti, nSize, buffer, nSize - m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
	} else {
		static constexpr DDEF ddef = {
			9, "Stream Properties", "{b7dc0791-a9b7-11cf-8ee6-00c00c205365}", 54, 0,
			DDEF::DESC::DT_GUID, 0, 2, "StreamType", &ddefv1, nullptr,
			DDEF::DESC::DT_GUID, 0, 4, "ErrorCorrectionType", &ddefv2, nullptr,
			DDEF::DESC::DT_QWORD, 0, 4, "TimeOffset", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "TypeSpecificDataLength", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 4, "ErrorCorrectionDataLength", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 2, "Flags", &ddefv3, nullptr, 
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 4, "SecurityID", nullptr, nullptr, 
			DDEF::DESC::DT_CALLBACK, -4, 3, "TypeSpecificData", nullptr, nullptr, 
			DDEF::DESC::DT_BYTES, 0, 3, "ErrorCorrectionData", nullptr, nullptr, 
		};
		return (Dump( hti, nSize, buffer, nSize - m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
	}
}

bool CASFParser::DumpContentDescriptionObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		10, "Content Description", "{75b22633-668e-11cf-a6d9-00aa0062ce6c}", 10, 0,
		DDEF::DESC::DT_WORD, 0, 4, "TitleLength", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "AuthorLength", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "CopyrightLength", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "DescriptionLength", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "RatingLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -5, 1, "Title", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -5, 1, "Author", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -5, 1, "Copyright", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -5, 1, "Description", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -5, 1, "Rating", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpHeaderExtensionObject( int hti, DWORD nParentSize )
{
	static const DDEFV ddefv1 =	{
		1, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{abd3d211-a9ba-11cf-8ee6-00c00c205365}", "Reserved1", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 =	{
		1, DDEFV::VDT_INFIX,
		6, "Reserved2", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		4, "Header Extension", "{5fbf03b5-a92e-11cf-8ee3-00c00c205365}", 22, 0,
		DDEF::DESC::DT_GUID, 0, 4, "ClockType", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 4, "ClockSize", &ddefv2, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "HeaderExtensionDataSize", nullptr, nullptr,
		DDEF::DESC::DT_END, 0, 4, "", nullptr, nullptr,
	};
	Dump( hti, m_nMinHeaderSize, m_nMinHeaderSize + 16 + 2 + 4, nParentSize, &ddef );
	hti++;

	nParentSize -= 16 + 2 + 4;
	while ( nParentSize >= m_nMinHeaderSize + m_nMinHeaderSize ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		GUID guid;
		QWORD nSize;
		if ( !ReadGUIDSize( guid, nSize, hti ) )
			return false;
		_tstring s = ToString( guid );

		ASSERT( nSize < MAXDWORD );
		if ( nSize > nParentSize - m_nMinHeaderSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			nSize = nParentSize - m_nMinHeaderSize;
			nParentSize = 0;
		} else
			nParentSize -= (DWORD)nSize;

		if (s == _T("{14e6a5cb-c672-4332-8399-a96952065b5a}"))
			DumpExtendedStreamPropertiesObject(hti, (DWORD)nSize);
		else if (s == _T("{7c4346a9-efe0-4bfc-b229-393ede415c85}"))
			DumpLanguageListObject(hti, (DWORD)nSize);
		else if (s == _T("{c5f8cbea-5baf-4877-8467-aa8c44fa4cca}"))
			DumpMetadataObject(hti, (DWORD)nSize);
		else if (s == _T("{44231c94-9498-49d1-a141-1d134e457054}"))
			DumpMetadataLibraryObject(hti, (DWORD)nSize);
		else if (s == _T("{d4fed15b-88d3-454f-81f0-ed5c45999e24}"))
			DumpStreamPrioritizationObject(hti, (DWORD)nSize);
		else if (s == _T("{d6e229df-35da-11d1-9034-00a0c90349be}"))
			DumpIndexParametersObject(hti, (DWORD)nSize);
		else if (s == _T("{a08649cf-4775-4670-8a16-6e35357566cd}"))
			DumpAdvancedMutualExclusionObject(hti, (DWORD)nSize);
		else if (s == _T("{d1465a40-5a79-4338-b71b-e36b8fd6c249}"))
			DumpGroupMutualExclusionObject(hti, (DWORD)nSize);
		else if (s == _T("{a69609e6-517b-11d2-b6af-00c04fd908e9}"))
			DumpBandwidthSharingObject(hti, (DWORD)nSize);
		else if (s == _T("{6b203bad-3f11-48e4-aca8-d7613de2cfa7}"))
			DumpMediaObjectIndexParametersObject(hti, (DWORD)nSize);
		else if (s == _T("{f55e496d-9797-4b5d-8c8b-604dfe9bfb24}"))
			DumpTimecodeIndexParametersObject(hti, (DWORD)nSize);
		//		else if ( s == _T("{75b22630-668e-11cf-a6d9-00aa0062ce6c}") ) das ist ein Fehler in der Doku
		//			DumpCompatibilityObject( hti, (DWORD)nSize );
		else if (s == _T("{1806d474-cadf-4509-a4ba-9aabcb96aae8}"))
			DumpPaddingObject(hti, (DWORD)nSize);
		else if (s == _T("{26f18b5d-4584-47ec-9f5f-0e651f0452c9}")) // das ist vermutlich richtig
			DumpCompatibilityObject(hti, (DWORD)nSize);
		else if (s == _T("{43058533-6981-49e6-9b74-ad12cb86d58c}"))
			DumpAdvancedContentEncryptionObject(hti, (DWORD)nSize);
		else if (s == _T("{d9aade20-7c17-4f9c-bc28-8555dd98e2a2}")) // undokumentiert
			DumpIndexParameterPlaceholderObject(hti, (DWORD)nSize);
//		else if (s == _T("{544091f3-7afb-441e-a7f4-8fe217beba2d}")) { // tritt auf, unknown
//		}
		else {
			if ( nSize == 0 ) {
				int htitmp = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Undefined"), s, nParentSize - m_nMinHeaderSize );
				if ( !ReadSeekFwdDump( nParentSize, (Settings.nVerbosity >= 4 ? htitmp : 0) ) )
					return false;
				if ( !ReadSeekFwdDump( m_nMinHeaderSize, (Settings.nVerbosity >= 4 ? htitmp : 0) ) )
					return false;
				if ( !ReadSeekFwdDump( m_nMinHeaderSize - 6, (Settings.nVerbosity >= 4 ? htitmp : 0) ) )
					return false;
				nParentSize = 0;
			} else {
				int htitmp = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Undefined"), s, nSize - m_nMinHeaderSize);
				TRACE3(_T("%s (%s): Unknown GUID %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), s.c_str());
				if ( !ReadSeekFwdDump( nSize - m_nMinHeaderSize, (Settings.nVerbosity >= 4 ? htitmp : 0) ) )
					return false;
			}
		}
	}
	if ( nParentSize > m_nMinHeaderSize ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}


bool CASFParser::DumpAdvancedMutualExclusionObject( int hti, DWORD nSize ) 
{
	static const DDEFV ddefv1 =	{
		4, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{d6e22a00-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Language", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d6e22a01-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Bitrate", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d6e22a02-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Presentation", DDEFV::DESC::VDTV_VALUE, // undocumented
		(DWORD_PTR)"{d6e22a02-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Unknown", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		3, "Advanced Mutual Exclusion", "{a08649cf-4775-4670-8a16-6e35357566cd}", 18, 0,
		DDEF::DESC::DT_GUID, 0, 3, "ExclusionType", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "StreamNumbersCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, -1, 2, "StreamNumbers", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpGroupMutualExclusionObject( int hti, DWORD nSize) 
{
	static const DDEFV ddefv1 =	{
		3, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{d6e22a00-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Language", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d6e22a01-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Bitrate", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d6e22a02-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Unknown", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		5, "Group Mutual Exclusion", "{d1465a40-5a79-4338-b71b-e36b8fd6c249}", 18, 0,
		DDEF::DESC::DT_GUID, 0, 3, "ExclusionType", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "RecordsCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "Records", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "StreamCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, -1, 2, "StreamNumbers", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpBandwidthSharingObject( int hti, DWORD nSize ) 
{
	static const DDEFV ddefv1 =	{
		2, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{af6060aa-5197-11d2-b6af-00c04fd908e9}", "ASF_Bandwidth_Sharing_Exclusive", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{af6060ab-5197-11d2-b6af-00c04fd908e9}", "ASF_Bandwidth_Sharing_Partial", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		5, "Bandwidth Sharing", "{a69609e6-517b-11d2-b6af-00c04fd908e9}", 26, 0,
		DDEF::DESC::DT_GUID, 0, 3, "SharingType", &ddefv1, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "DataBitrate", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "BufferSize", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "StreamNumbersCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, -1, 2, "StreamNumbers", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpMediaObjectIndexParametersObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 =	{
		4, DDEFV::VDT_INFIX,
		1, "Nearest Past Data Packet", DDEFV::DESC::VDTV_VALUE,
		2, "Nearest Past Media Object", DDEFV::DESC::VDTV_VALUE,
		3, "Nearest Past Cleanpoint (standard)", DDEFV::DESC::VDTV_VALUE,
		0xFF, "Frame Number Offset", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		5, "Media Object Index Parameters", "{6b203bad-3f11-48e4-aca8-d7613de2cfa7}", 10, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryCountInterval", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexSpecifiersCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "IndexSpecifiers", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexType", &ddefv1, nullptr, 
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpTimecodeIndexParametersObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 =	{
		2, DDEFV::VDT_INFIX,
		2, "Nearest Past Media Object", DDEFV::DESC::VDTV_VALUE,
		3, "Nearest Past Cleanpoint (standard)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		5, "Timecode Index Parameters", "{f55e496d-9797-4b5d-8c8b-604dfe9bfb24}", 10, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryTimeInterval", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexSpecifiersCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "IndexSpecifiers", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexType", &ddefv1, nullptr, 
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpIndexParameterPlaceholderObject( int hti, DWORD nSize ) 
{
	static constexpr DDEF ddef = {            
		1, "Index Parameter Placeholder", "{d9aade20-7c17-4f9c-bc28-8555dd98e2a2}", 0, 0,
		DDEF::DESC::DT_BYTES, 0, 3, "(Reserved)", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpCompatibilityObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 =	{
		1, DDEFV::VDT_INFIX,
		2, "standard", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 =	{
		1, DDEFV::VDT_INFIX,
		1, "standard", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {            
		2, "Compatibility", "{26f18b5d-4584-47ec-9f5f-0e651f0452c9}", 2, 0,
		DDEF::DESC::DT_BYTE, 0, 3, "Profile", &ddefv1, nullptr, 
		DDEF::DESC::DT_BYTE, 0, 3, "Mode", &ddefv2, nullptr, 
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpAdvancedContentEncryptionObject( int hti, DWORD nSize ) 
{
	static const DDEFV ddefv1 =	{
		1, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{7a079bb6-daa4-4e12-a5ca-91d38dc11a8d}", "ASF_Content_Encryption_System_Windows_Media_DRM_Network_Devices", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		13, "Advanced Content Encryption", "{43058533-6981-49e6-9b74-ad12cb86d58c}", 2, 0,
		DDEF::DESC::DT_WORD, 0, 3, "ContentEncryptionRecordsCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "ContentEncryptionRecords", nullptr, nullptr,
		DDEF::DESC::DT_GUID, 0, 3, "SystemID", &ddefv1, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "SystemVersion", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "Encrypted ObjectRecordCount", nullptr, nullptr, 
		DDEF::DESC::DT_NODELOOP, -1, 3, "EncryptedObjectRecords", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "EncryptedObjectIDType", nullptr, nullptr, // 1 = WORD. This value indicates that the Encrypted ObjectID is an ASF stream number. The ObjectID Length should be set to 0x0002 when this value type is set.
		DDEF::DESC::DT_WORD, 0, 4, "EncryptedObjectIDLength", nullptr, nullptr, // should be 2
		DDEF::DESC::DT_WORD, 0, 3, "EncryptedObjectID", nullptr, nullptr, // TODO Länge hängt von -1 ab...
		DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "DataSize", nullptr, nullptr, 
		DDEF::DESC::DT_BYTES, -1, 3, "Data", nullptr, nullptr, 
		DDEF::DESC::DT_LOOPEND, 0, 3, "", nullptr, nullptr, 
	};
	return ( Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpIndexParametersObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 = {
		3, DDEFV::VDT_INFIX,
		1, "Nearest Past Data Packet", DDEFV::DESC::VDTV_VALUE,
		2, "Nearest Past Media Object", DDEFV::DESC::VDTV_VALUE,
		3, "Nearest Past Cleanpoint (standard)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		5, "Index Parameters", "{d6e229df-35da-11d1-9034-00a0c90349be}", 10, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "IndexEntryTimeInterval", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexSpecifiersCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "IndexSpecifiers", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "IndexType", &ddefv1, nullptr, 
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpStreamPrioritizationObject( int hti, DWORD nSize ) 
{
	static constexpr DDEFV ddefv1 = {
		2, DDEFV::VDT_POSTFIX,
		0x0001, "Mandatory Flag", DDEFV::DESC::VDTV_BITFIELD,
		0xFFFE, "Reserved Flags", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		4, "Stream Prioritization", "{d4fed15b-88d3-454f-81f0-ed5c45999e24}", 6, 0,
		DDEF::DESC::DT_WORD, 0, 3, "PriorityRecordsCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "PriorityRecords", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "PriorityFlags", &ddefv1, nullptr, 
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpMetadataObject( int hti, DWORD nSize ) 
{
	static constexpr DDEF ddef = {
		9, "Metadata", "{c5f8cbea-5baf-4877-8467-aa8c44fa4cca}", 14, 0,
		DDEF::DESC::DT_WORD, 0, 2, "Description Records Count", nullptr, nullptr,

		DDEF::DESC::DT_NODELOOP, -1, 3, "DescriptionRecords", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "Reserved", nullptr, nullptr, // Todo must be 0
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 4, "NameLength", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "DataType", &ddefvType, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 4, "DataLength", nullptr, nullptr, 
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -3, 2, "Name", nullptr, nullptr,
//		DDEF::DESC::DT_BYTES, -2, 3, "Data", nullptr, nullptr, // TODO Type siehe oben...
		DDEF::DESC::DT_CALLBACK, 0, 2, "DescriptorValue", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpMetadataLibraryObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		11, "Metadata Library", "{44231c94-9498-49d1-a141-1d134e457054}", 14, 0, 
		DDEF::DESC::DT_WORD, 0, 2, "Description Records Count", nullptr, nullptr,

		DDEF::DESC::DT_NODELOOP, -1, 3, "DescriptionRecords", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "LanguageList", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 4, "NameLength", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "DataType", &ddefvType, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 4, "DataLength", nullptr, nullptr, 
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -3, 2, "Name", nullptr, nullptr,
//		DDEF::DESC::DT_BYTES, -2, 3, "Data", nullptr, nullptr, // TODO Type siehe oben...
		DDEF::DESC::DT_CALLBACK, 0, 2, "DescriptorValue", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,
		DDEF::DESC::DT_END, 0, 0, "", nullptr, nullptr, // often spurious (padding) data...
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);

#if 0 // TODO ???
	http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wmplay10/mmp_sdk/settingattributevaluesfortvcontent.asp
	http://msdn.microsoft.com/windowsmedia/techart/winmedform/default.aspx?pull=/library/en-us/dnwmt/html/wm_metadata_usage.asp?frame=true
	Type 6, DesriptorValue 
		db9830bd-3ab3-4fab-8a37-1a995f7ff74b: Video
		D1607DBC-E323-4BE2-86A1-48A42A28441E: Music
		01CD0F29-DA4E-4157-897B-6275D50C4F11: Audio (not music)
		FCF24A76-9A57-4036-990D-E35DD8B244E1: Other
PLAYLIST 1F4F1464-C965-4CF5-95CB-A1337A2AC9F8 May be used for playlist content. 
RADIO 1969ADFD-F555-4B72-BEF6-AF60CE0430FF May be used for radio content. 

Secondary:
AUDIO_AUDIO_BOOK E0236BEB-C281-4EDE-A36D-7AF76A3D45B5 Use for audio book files. 
AUDIO_SPOKEN_WORD 3A172A13-2BD9-4831-835B-114F6A95943F Use for audio files that contain speech, but are not audio books. For example, stand-up comedy routines. 
AUDIO_MEDIA_NEWS 6677DB9B-E5A0-4063-A1AD-ACEB52840CF1 Use for audio files related to news. 
AUDIO_MEDIA_TALKSHOW 1B824A67-3F80-4E3E-9CDE-F7361B0F5F1B Use for audio files with talk show content. 
VIDEO_MEDIA_NEWS 1FE2E091-4E1E-40CE-B22D-348C732E0B10 Use for video files related to news. 
VIDEO_MEDIA_SHOW D6DE1D88-C77C-4593-BFBC-9C61E8C373E3 Use for video files containing Web-based shows, short films, movie trailers, and so on. This is the general identifier for video entertainment that does not fit into another category. 
AUDIO_GAME_SOUNDS 00033368-5009-4AC3-A820-5D2D09A4E7C1 Use for audio files containing sound clips from games. 
MUSIC_GAME_SOUND_TRACK F24FF731-96FC-4D0F-A2F5-5A3483682B1A Use for audio files containing complete songs from game sound tracks. If only part of a song is encoded in the file, use the identifier for game sound clips instead. 
VIDEO_MUSIC_VIDEO E3E689E2-BA8C-4330-96DF-A0EEEFFA6876 Use for video files containing music videos. 
VIDEO_HOME_VIDEO B76628F4-300D-443D-9CB5-01C285109DAF Use for video files containing general home video. 
VIDEO_MOVIE A9B87FC9-BD47-4BF0-AC4F-655B89F7D868 Use for video files containing feature films. 
VIDEO_TV_SHOW BA7F258A-62F7-47A9-B21F-4651C42A000E Use for video files containing television shows. For Web-based shows, use VIDEO_MEDIA_SHOW. 
VIDEO_CORPORATE_VIDEO 44051B5B-B103-4B5C-92AB-93060A9463F0 Use for video files containing corporate video. For example, recorded meetings or training videos. 
VIDEO_HOME_PHOTOVIDEO 0B710218-8C0C-475E-AF73-4C41C0C8F8CE Use for video files containing home video made from photographs. This class applies specifically to content created by using the Windows Media Video Image codec. 


The following media class identifiers are used internally by Windows Media Player.

Secondary class name Secondary class GUID Description 
PLAYLIST_STATIC D0E20D5C-CAD6-4F66-9FA1-6018830F1DCC Used for static playlists. 
PLAYLIST_SMART EB0BAFB6-3C4F-4C31-AA39-95C7B8D7831D Used for smart playlists, which are created by Windows Media Player based on other content metadata. 
RADIO_FAVORITE 3C113A69-83D0-4E42-8DA2-88FA0C0F1C8F Used for the radio stations listed in the My Favorites radio list in Windows Media Player. 
RADIO_RECOMMENDED 99875E99-4A5B-41B7-A82B-775380CAB690 Used for the radio stations listed in the Featured Stations radio list in Windows Media Player. 
RADIO_RECENTLY_PLAYED 264436C7-4CEA-4A87-A09A-6C117229DA5C Used for the radio stations listed in the Recently Played radio list in Windows Media Player. 
#endif
}

bool CASFParser::DumpLanguageListObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		4, "Language List", "{7c4346a9-efe0-4bfc-b229-393ede415c85}", 3, 0,
		DDEF::DESC::DT_WORD, 0, 3, "LanguageIDRecordsCount", nullptr, nullptr,
		DDEF::DESC::DT_NODELOOP, -1, 3, "StreamNames", nullptr, nullptr,
		DDEF::DESC::DT_BYTE, 0, 4, "LanguageIDLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -1, 3, "LanguageID", nullptr, nullptr,
		// LOOPEND
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}


bool CASFParser::DumpBitrateMutualExclusionObject( int hti, DWORD nSize )
{
	static const DDEFV ddefv1 =	{
		4, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{d6e22a00-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Language", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d6e22a01-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Bitrate", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d6e22a02-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Presentation", DDEFV::DESC::VDTV_VALUE, // undocumented
		(DWORD_PTR)"{d6e22a02-35da-11d1-9034-00a0c90349be}", "ASF_Mutex_Unknown", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		3, "Bitrate Mutual Exclusion", "{d6e229dc-35da-11d1-9034-00a0c90349be}", 18, 0,
		DDEF::DESC::DT_GUID, 0, 3, "ExclusionType", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, 0, 2, "StreamNumbersCount", nullptr, nullptr,
		DDEF::DESC::DT_WORD, -1, 2, "StreamNumbers", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpErrorCorrectionObject( int hti, DWORD nSize )
{
	static constexpr DDEF ddef = {
		3, "Error Correction", "{75b22635-668e-11cf-a6d9-00aa0062ce6c}", 20, 0,
		DDEF::DESC::DT_GUID, 0, 3, "ErrorCorrectionType", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "ErrorCorrectionDataLength", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 4, "ErrorCorrectionData", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpContentBrandingObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv1 =	{
		4, DDEFV::VDT_INFIX,
		0, "No Image", DDEFV::DESC::VDTV_VALUE,
		1, "Bitmap", DDEFV::DESC::VDTV_VALUE,
		2, "JPEG", DDEFV::DESC::VDTV_VALUE,
		3, "GIF", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		7, "Content Branding", "{2211b3fa-bd23-11d2-b4b7-00a0c955fc6e}", 16, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "BannerImageType", &ddefv1, nullptr, // If this value is set to 0, then the Banner Image Data Size field must be set to 0, and the Banner Image Data field must be empty.
		DDEF::DESC::DT_DWORD, 0, 4, "BannerImageDataSize", nullptr, nullptr,
		DDEF::DESC::DT_BYTES, -1, 4, "BannerImageData", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "BannerImageURLLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "BannerImageURL", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 4, "CopyrightURLLength", nullptr, nullptr,
		DDEF::DESC::DT_MBCSSTRING, -1, 3, "CopyrightURL", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpPaddingObject( int hti, DWORD nSize )
{
	hti = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Padding"), _T("{1806d474-cadf-4509-a4ba-9aabcb96aae8}"), nSize );
	return ReadSeekFwdDump( nSize - m_nMinHeaderSize, (Settings.nVerbosity >= 5 ? hti : 0) );
}

bool CASFParser::DumpWrongPaddingObject( int hti, DWORD nSize )
{
	hti = OSNodeCreate( hti, GetReadPos() - m_nMinHeaderSize, _T("Padding (wrong position)"), _T("{1806d474-cadf-4509-a4ba-9aabcb96aae8}"), nSize );
	return ReadSeekFwdDump( nSize - m_nMinHeaderSize, (Settings.nVerbosity >= 5 ? hti : 0) );
}

bool CASFParser::DumpExtendedStreamPropertiesObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv1 =	{
		6, DDEFV::VDT_POSTFIX,
		0x0001, "Reliable Flag", DDEFV::DESC::VDTV_BITFIELD,
		0x0002, "Seekable Flag", DDEFV::DESC::VDTV_BITFIELD,
		0x0004, "No Cleanpoints Flag", DDEFV::DESC::VDTV_BITFIELD,
		0x0008, "Resend Live Cleanpoints Flag", DDEFV::DESC::VDTV_BITFIELD,
		0xFF10, "Language Index", DDEFV::DESC::VDTV_BITFIELD,
		0xFFE0, "Reserved Flags", DDEFV::DESC::VDTV_BITFIELD,
	};
	static const DDEFV ddefv2 =	{
		6, DDEFV::VDT_INFIX,
		(DWORD_PTR)"{399595ec-8667-4e2d-8fdb-98814ce76c1e}", "ASF_Payload_Extension_System_Timecode", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{e165ec0e-19ed-45d7-b4a7-25cbd1e28e9b}", "ASF_Payload_Extension_System_File_Name", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{d590dc20-07bc-436c-9cf7-f3bbfbf1a4dc}", "ASF_Payload_Extension_System_Content_Type", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{1b1ee554-f9ea-4bc8-821a-376b74e4c4b8}", "ASF_Payload_Extension_System_Pixel_Aspect_Ratio", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{c6bd9450-867f-4907-83a3-c77921b733ad}", "ASF_Payload_Extension_System_Sample_Duration", DDEFV::DESC::VDTV_VALUE,
		(DWORD_PTR)"{6698b84e-0afa-4330-aeb2-1c0a98d7a44d}", "ASF_Payload_Extension_System_Encryption_Sample_ID", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		27, "Extended Stream Properties", "{14e6a5cb-c672-4332-8399-a96952065b5a}", 64, 0,
		DDEF::DESC::DT_QWORD, 0, 3, "StartTime (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, 0, 3, "EndTime (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgDataBitrate (bps)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgBufferSize (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "AvgInitialBufferFullness (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxDataBitrate (bps)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxBufferSize (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaxInitialBufferFullness (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaximumObjectSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "Flags", &ddefv1, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "StreamNumber", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "StreamLanguageIDIndex", nullptr, nullptr, 
		DDEF::DESC::DT_DURATION100NS, 0, 3, "AverageTimePerFrame (100ns)", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "StreamNameCount", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "PayloadExtensionSystemCount", nullptr, nullptr, 

		DDEF::DESC::DT_NODELOOP, -2, 3, "StreamNames", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 3, "LanguageIDIndex", nullptr, nullptr,
		DDEF::DESC::DT_WORD, 0, 4, "StreamNameLength", nullptr, nullptr,
		DDEF::DESC::DT_UTF16STRING_LENGTHINBYTES, -1, 3, "StreamNames", nullptr, nullptr,
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,

		DDEF::DESC::DT_NODELOOP, -6, 3, "PayloadExtensionSystems", nullptr, nullptr,
		DDEF::DESC::DT_GUID, 0, 4, "ExtensionSystem", &ddefv2, nullptr, 
		DDEF::DESC::DT_WORD, 0, 4, "ExtensionDataSize", nullptr, nullptr, // TODO -1, ist variabel
		DDEF::DESC::DT_DWORD, 0, 4, "ExtensionSystemInfoLength", nullptr, nullptr, 
		DDEF::DESC::DT_BYTES, -1, 4, "ExtensionSystemInfo", nullptr, nullptr, 
		DDEF::DESC::DT_LOOPEND, 0, 0, "", nullptr, nullptr,

		DDEF::DESC::DT_CALLBACK, 0, 2, "StreamPropertiesObject", nullptr, nullptr, 
	};
// TODO Avg. Time / Frame 00:00.0422606, Avg. Frames / Second 23.66 ausrechnen
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);
}

bool CASFParser::DumpFilePropertiesObject( int hti, DWORD nSize )
{
	static constexpr DDEFV ddefv1 =	{
		8, DDEFV::VDT_POSTFIX,
		0x0001, "Broadcast", DDEFV::DESC::VDTV_BITFIELD,
		0x0002, "Seekable", DDEFV::DESC::VDTV_BITFIELD,
		0x0004, "Use Packet Template", DDEFV::DESC::VDTV_BITFIELD, // from here on undocumented
		0x0008, "Live", DDEFV::DESC::VDTV_BITFIELD,
		0x0010, "Reliable", DDEFV::DESC::VDTV_BITFIELD,
		0x0020, "Recordable", DDEFV::DESC::VDTV_BITFIELD,
		0x0040, "Unknown Data Size", DDEFV::DESC::VDTV_BITFIELD,
		0xFF80, "Reserved Flags", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		11, "File Properties (V2)", "{8cabdca1-a947-11cf-8ee4-00c00c205365}", 80, 0,
		DDEF::DESC::DT_GUID, 0, 4, "FileID", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, 0, 1, "FileSize", nullptr, nullptr, // invalid, if BroadcastFlag set
		DDEF::DESC::DT_FILETIME1601, 0, 1, "CreationDate", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, 0, 2, "DataPacketsCount", nullptr, nullptr, // invalid, if BroadcastFlag set
		DDEF::DESC::DT_DURATION100NS, 0, 2, "PlayDuration (100ns, incl Preroll)", nullptr, nullptr, // invalid, if BroadcastFlag set
		DDEF::DESC::DT_DURATION100NS, 0, 4, "SendDuration (100ns)", nullptr, nullptr,
		DDEF::DESC::DT_QWORD, 0, 3, "Preroll (1ms)", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 2, "Flags", &ddefv1, nullptr, // hex
		DDEF::DESC::DT_DWORD, 0, 3, "MinimumDataPacketSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 3, "MaximumDataPacketSize", nullptr, nullptr,
		DDEF::DESC::DT_DWORD, 0, 2, "MaximumBitrate (bps)", nullptr, nullptr,
	};
	return (Dump( hti, m_nMinHeaderSize, nSize, &ddef ) == nSize - m_nMinHeaderSize);

//	if ( (io->nFlags & FilePropertiesObject::BroadcastFlag) == 0 && io->nFileSize != GetReadFileSize() )
//		OSNodeCreateAlert( CIOService::EPRIORITY::EP_ERROR, _T("Wrong FileSize!"), htitmp );
//	s += _T(" (") + Length100nsToString( io->nPlayDuration ) + _T(")");
//	s += _T(" (") + Length1msToString( io->nPreroll ) + _T(")");
}


/*static*/ bool CASFParser::Probe( BYTE buffer[12] )
{
	DWORD* fcc = reinterpret_cast<DWORD*>( buffer ); 
	return (fcc[0] == 0x75b22630);
}

bool CASFParser::ReadSize( QWORD &n, int hti )
{ 
	if ( !ReadBytes( &n, sizeof( n ), hti ) )
		return false;
	if ( GetReadTotalBytes() + n - m_nMinHeaderSize > GetReadFileSize() ) 
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk size (") + ToString( n ) + _T(") greater File size (") + ToString( GetReadFileSize() + m_nMinHeaderSize - GetReadTotalBytes() ) + _T("), File may be truncated!") );
	return true;
}
