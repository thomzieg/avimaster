#include "MOVParser.h"
//#include "AVIParser.h"
#include "RIFFParser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

/*static*/ const WORD CMOVParser::m_nMinHeaderSize = static_cast<WORD>( sizeof( DWORD ) + sizeof( DWORD ) );

// TODO Dass ich hier mit Return false aus den Read...Chunk() rausspringe, nur weil ich u.u. ein paar Bytes zu wenig/zu viel gelesen habe ist wohl falsch
// Besser wäre nur bei fatalen Fehlern abzubrechen un sonst neu aufzusetzen

// TODO ReadSeekFwdDump müsste testen ddef->desc[nField].nVerbosity <= Settings.nVerbosity

CMOVParser::CMOVParser( class CIOService& ioservice) : CRIFFParser( ioservice )
{
	SetParams( false, true );
	m_bMP4 = false;
}

bool CMOVParser::ReadFile(int hti)
{ 
	if ( ReadChunk( hti, GetReadFileSize()) )
		m_sSummary = (m_bMP4 ? _T("This is a Apple Quicktime MPEG-4 file.") : _T("This is a Apple Quicktime file.")); // TODO
	else
		m_sSummary = _T("This might not be a valid Apple Quicktime file.");
	return true; // all done
}

bool CMOVParser::ReadChunk( int hti, QWORD nParentSize )
{
FOURCC fcc;
QWORD nQSize;
DWORD nDSize;
int htitmp;
_tstring s;
	
	while ( nParentSize >= 8 ) {
		ASSERT( GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nDSize, fcc, hti ) )
			return false;
		nQSize = nDSize;
		if ( nQSize == 0 )
			nQSize = nParentSize;
		else if ( nQSize == 1 ) {
			//ASSERT( false ); // size is in extended size field
			if ( !ReadBytes( &nQSize, sizeof( nQSize ), hti ) )
				return false;
		}
		if ( nQSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nQSize = nParentSize;
			else 
				nParentSize = nQSize;
		}
		nParentSize -= nQSize;
		switch ( fcc ) {
		case FCC('moov'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Movie"), fcc, nQSize );
			if ( !ReadMoovChunk( htitmp, (DWORD)nQSize - 8 ) ) 
				return false;
			break; 
		case FCC('mdat'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Movie Data"), fcc, nQSize );
			if ( !ReadMdatChunk( htitmp, nQSize - 8 ) ) 
				return false;
			break; 
		case FCC('ftyp'): // MP4
			if ( !ReadFtypChunk( hti, (DWORD)nQSize - 8 ) ) 
				return false;
//			m_bMP4 = true; das stimmt nur, wenn der Major Brand nicht 'qt  ' ist.
			break; 
		case FCC('free'): // TODO diese 4 können auch in Subatoms vorkommen
		case FCC('skip'): 
		case FCC('pnot'): 
		case FCC('wide'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Atom"), fcc, nQSize );
			if ( nQSize > 8 && !ReadSeekFwdDump( (DWORD)nQSize - 8, htitmp ) )
				return false;
			break;
		case FCC('meta'): // TODO MP4 MPEG 7 Meta Data
			if (!ReadMetaChunk(htitmp, (DWORD)nQSize - 8))
				return false;
			break;
		case FCC('xml '): // TODO MP4 MPEG 7 Meta Data
		case FCC('bxml'): // TODO MP4 MPEG 7 Meta Data
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("MP4 MPEG-7 Atom"), fcc, nQSize );
			if ( nQSize > 8 && !ReadSeekFwdDump( (DWORD)nQSize - 8, htitmp ) )
				return false;
			break;
		case FCC('pitm'): // TODO MP4 MPEG 7 Meta Data
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("ISO/IEC 14496-12 primary item"), fcc, nQSize );
			if ( nQSize > 8 && !ReadSeekFwdDump( (DWORD)nQSize - 8, htitmp ) )
				return false;
#if 0
   * 8+ bytes optional ISO/IEC 14496-12 primary item box
       = long unsigned offset + long ASCII text string 'pitm'
     -> 4 bytes version/flags = byte hex version + 24-bit hex flags
         (current = 0)
     -> 2 bytes main item reference = short unsigned id
#endif
			break;
		case FCC('iinf'): // TODO ??? stimmt das???
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("ISO/IEC 14496-12 item information"), fcc, nQSize );
			if ( nQSize > 8 && !ReadSeekFwdDump( (DWORD)nQSize - 8, htitmp ) )
				return false;
#if 0
   * 8+ bytes optional ISO/IEC 14496-12 item information box
       = long unsigned offset + long ASCII text string 'pitm'
     -> 4 bytes version/flags = byte hex version + 24-bit hex flags
         (current = 0)
     -> 2 bytes main item reference = short unsigned id
     -> 2 bytes encryption box array value = short unsigned index
     -> item name or URL string = UTF-8 text string
     -> 1 byte name or URL c string end = byte value set to zero
     -> item mime type string = UTF-8 text string
     -> 1 byte mime type c string end = byte value set to zero
     -> optional item transfer encoding string = UTF-8 text string
     -> 1 byte optional transfer encoding c string end = byte value set to zero

#endif
			break;
		case FCC('ipro'): // TODO MP4 MPEG 7 Meta Data
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("ISO/IEC 14496-12 item protection"), fcc, nQSize );
			if ( nQSize > 8 && !ReadSeekFwdDump( (DWORD)nQSize - 8, htitmp ) )
				return false;
#if 0
   * 8+ bytes optional ISO/IEC 14496-12 item encryption box
       = long unsigned offset + long ASCII text string 'ipro'
     -> 4 bytes version/flags = byte hex version + 24-bit hex flags
         (current = 0)
     -> 2 bytes number of encryption boxes = short unsigned index total

      * 8+ bytes ISO/IEC 14496-12 encryption scheme info box
          = long unsigned offset + long ASCII text string 'sinf'
       - if meta data encrypted to ISO/IEC 14496-12 standards

         * 8+ bytes ISO/IEC 14496-12 original format box
             = long unsigned offset + long ASCII text string 'frma'
           -> 4 bytes description format = long ASCII text string

         * 8+ bytes optional ISO/IEC 14496-12 IPMP info box
             = long unsigned offset + long ASCII text string 'imif'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> IPMP descriptors = hex dump from IPMP part of ES Descriptor box

         * 8+ bytes optional ISO/IEC 14496-12 scheme type box
             = long unsigned offset + long ASCII text string 'schm'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0 ; contains URI if flags = 0x000001)
           -> 4 bytes encryption type = long ASCII text string
                - types are 128-bit AES counter = 'ACM1' ; 128-bit AES FS = 'AFS1'
                - types are nullptr algorithm = 'ENUL' ; 160-bit HMAC-SHA-1 = 'SHM2'
                - types are RTCP = 'ANUL' ; private scheme = '    '
           -> 2 bytes encryption version = short unsigned version
           -> optional scheme URI string = UTF-8 text string
             (eg. web site)
           -> 1 byte optional scheme URI string end = byte padding set to zero

         * 8+ bytes ISO/IEC 14496-12 scheme data box
             = long unsigned offset + long ASCII text string 'schi'
           -> encryption related key = hex dump
#endif
		   break;
		case FCC('ABCA'): // TODO 
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("??? Atom"), fcc, nQSize);
			if (nQSize > 8 && !ReadSeekFwdDump(nQSize - 8, htitmp))
				return false;
			break;
		case FCC('udta'): // TODO ParseGeneric...
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Atom User Data"), fcc, nQSize );
			if ( !ReadUdtaChunk( htitmp, (DWORD)nQSize - 8 ) ) 
				return false;
			break; 
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nQSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nQSize > 8 && !ReadSeekFwdDump( nQSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}


bool CMOVParser::ReadMoovChunk( int hti, DWORD nParentSize ) 
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT( GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('mvhd'): 
			if ( !ReadMvhdChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('trak'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Track"), fcc, nSize );
			if ( !ReadTrakChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('udta'): // TODO ParseGeneric...
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("User Data"), fcc, nSize );
			if ( !ReadUdtaChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('mdra'): // TODO Movie Data Reference
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Movie Data reference"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
#if 0 
  * 8+ bytes QUICKTIME movie data reference atom
       = long unsigned offset + long ASCII text string 'mdra'
    - if this is used no other atoms or boxes should be present at this level

      * 8+ bytes data reference atom
          = long unsigned offset + long ASCII text string 'dref'
        -> 4 bytes reference type name = long ASCII text string
             - types are file alias = 'alis' ; resource alias = 'rsrc' ;
             - types are url c string = 'url '
        -> 4 bytes reference version/flags
            = byte hex version (current = 0) + 24-bit hex flags
          - some flags are external data = 0x000000 ; internal data = 0x000001
        -> mac os file alias record structure
        OR
        -> mac os file alias record structure plus resource info
        OR
        -> url c string = ASCII text string
        -> 1 byte url c string end = byte value set to zero
#endif
		case FCC('cmov'): // TODO Compressed Movie 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Compressed Movie"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
#if 0
  * 8+ bytes QUICKTIME compressed moov atom
       = long unsigned offset + long ASCII text string 'cmov'
    - if this is used no other atoms should be present
       as this is for an entire compressed movie resource

      * 8+ bytes data compression atom
          = long unsigned offset + long ASCII text string 'dcom'
        -> 4 bytes compression code = long ASCII text string
          - compression codes are Deflate = 'zlib' ; Apple Compression = 'adec'

      * 8+ bytes compressed moov data atom
          = long unsigned offset + long ASCII text string 'cmvd'
        -> 4 bytes uncompressed size = long unsigned value
        -> entire compressed movie 'moov' resource = hex dump
#endif
		case FCC('rmra'): // TODO QUICKTIME reference movie record
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Reference Movie Record"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
#if 0
   * 8+ bytes QUICKTIME reference movie record atom
       = long unsigned offset + long ASCII text string 'rmra'
    - if this atom is used it must come first within the movie resource box

      * 8+ bytes reference movie descriptor atom
          = long unsigned offset + long ASCII text string 'rmda'

         * 8+ bytes reference movie data reference atom
             = long unsigned offset + long ASCII text string 'rdrf'
           -> 4 bytes reference version/flags
               = byte hex version (current = 0) + 24-bit hex flags
             - some flags are external data = 0x000000 ; internal data = 0x000001
           -> 4 bytes reference type name = long ASCII text string (if internal = 0)
             - types are file alias = 'alis' ; resource alias = 'rsrc' ;
             - types are url c string = 'url '
           -> 4+ bytes reference data = long unsigned length
           -> mac os file alias record structure
           OR
           -> mac os file alias record structure plus resource info
           OR
           -> url c string = ASCII text string
           -> 1 byte url c string end = byte value set to zero

         * 8+ bytes optional reference movie quality atom
             = long unsigned offset + long ASCII text string 'rmqu'
           -> 4 bytes queue position = long unsigned value from 100 to 0

         * 8+ bytes optional reference movie cpu rating atom
             = long unsigned offset + long ASCII text string 'rmcs'
           -> 4 bytes reserved flag = byte hex version + 24-bit hex flags (current = 0)
           -> 2 bytes speed rating = short unsigned value from 500 to 100

         * 8+ bytes optional reference movie version check atom
             = long unsigned offset + long ASCII text string 'rmvc'
           -> 4 bytes flags = byte hex version + 24-bit hex flags (current = 0)
           -> 4 bytes gestalt selector = long ASCII text string
               (eg. quicktime = 'qtim')

           -> 4 bytes gestalt min value = long hex value
               (eg. QT 3.02 mac file version = 0x03028000)
           -> 4 bytes gestalt no value = long value set to zero
           OR
           -> 4 bytes gestalt value mask = long hex mask
           -> 4 bytes gestalt value = long hex value

           -> 2 bytes gestalt check type = short unsigned value
               (min value = 0 or mask = 1)

         * 8+ bytes optional reference movie component check atom
             = long unsigned offset + long ASCII text string 'rmcd'
           -> 4 bytes flags = byte hex version + 24-bit hex flags (current = 0)
           -> 8 bytes component type/subtype
               = long ASCII text string + long ASCII text string
               (eg. Timecode Media Handler = 'mhlrtmcd')
           -> 4 bytes component manufacturer = long ASCII text string
               (eg. Apple = 'appl' or 0)
           -> 4 bytes component flags = long hex flags (none = 0)
           -> 4 bytes component flags mask = long hex mask (none = 0)
           -> 4 bytes component min version = long hex value (none = 0)

         * 8+ bytes optional reference movie data rate atom
             = long unsigned offset + long ASCII text string 'rmdr'
           -> 4 bytes flags = byte hex version + 24-bit hex flags (current = 0)
           -> 4 bytes data rate = long integer bit rate value
             - common analog modem rates are 1400; 2800; 3300; 5600
             - common broadband rates are 5600; 11200; 25600; 38400; 51200; 76800; 100000
             - common high end broadband rates are T1 = 150000; no limit/LAN = 0x7FFFFFFF

         * 8+ bytes optional reference movie language atom
             = long unsigned offset + long ASCII text string 'rmla'
           -> 4 bytes flags = byte hex version + 24-bit hex flags (current = 0)
           -> 2 bytes mac language = short unsigned language value (english = 0)

         * 8+ bytes optional reference movie alternate group atom
             = long unsigned offset + long ASCII text string 'rmag'
             (structure was not provided in MoviesFormat.h of the 4.1.2 win32 sdk)
           -> 4 bytes flags = long value set to zero
           -> 2 bytes alternate/other = short integer track id value (none = 0)
#endif
		case FCC('clip'): // TODO
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Clipping"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('ctab'): // TODO
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Color Table"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('iods'): 
			if ( !ReadIodsChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('meta'):
			if (!ReadMetaChunk(hti, nSize - 8))
				return false;
			break;
		case FCC('free'): // TODO 
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
				return false;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CMOVParser::ReadFtypChunk( int hti, DWORD nChunkSize )
{
#if 0
	- NOTE: All compatible with 'isom', vers. 1 uses no Scene Description Tracks,
       vers. 2 uses the full part one spec, M4A uses custom ISO 14496-12 info,
       qt means the format complies with the original Apple spec, 3gp uses sample
       descriptions in the same style as the original Apple spec.
#endif
	static constexpr DDEFV ddefv1 = { // http://www.mp4ra.org/filetype.html
		24, DDEFV::VDT_INFIX,
		FCC('3g2a'), "3GPP2", DDEFV::DESC::VDTV_VALUE, 
		FCC('3ge6'), "3GPP Release 6 extended-presentation Profile", DDEFV::DESC::VDTV_VALUE, 
		FCC('3gg6'), "3GPP Release 6 General Profile", DDEFV::DESC::VDTV_VALUE, 
		FCC('3gp4'), "3GPP Release 4", DDEFV::DESC::VDTV_VALUE,
		FCC('3gp5'), "3GPP Release 5", DDEFV::DESC::VDTV_VALUE,
		FCC('3gp6'), "3GPP Release 6 basic Profile", DDEFV::DESC::VDTV_VALUE,
		FCC('3gr6'), "3GPP Release 6 progressive-download Profile", DDEFV::DESC::VDTV_VALUE,
		FCC('3gs6'), "3GPP Release 6 streaming-server Profile", DDEFV::DESC::VDTV_VALUE,

		FCC('avc1'), "Advanced Video Coding extensions", DDEFV::DESC::VDTV_VALUE,
		FCC('iso2'), "ISO 14496-12 (2004) Base Media", DDEFV::DESC::VDTV_VALUE,
		FCC('isom'), "ISO 14496-1 Base Media", DDEFV::DESC::VDTV_VALUE,
		FCC('mj2s'), "Motion JPEG 2000 simple profile", DDEFV::DESC::VDTV_VALUE, 
		FCC('mjp2'), "Motion JPEG 2000, general profile", DDEFV::DESC::VDTV_VALUE, 

		FCC('mp21'), "MPEG-21", DDEFV::DESC::VDTV_VALUE,
		FCC('mp41'), "ISO 14496-1 vers. 1", DDEFV::DESC::VDTV_VALUE,
		FCC('mp42'), "ISO 14496-1 vers. 2", DDEFV::DESC::VDTV_VALUE,

		FCC('qt  '), "Quicktime Movie", DDEFV::DESC::VDTV_VALUE,
		FCC('sdv '), "SD Video", DDEFV::DESC::VDTV_VALUE,

		// not official?
		FCC('mmp4'), "3G Mobile MP4", DDEFV::DESC::VDTV_VALUE,
		FCC('M4A '), "Apple AAC audio w/ iTunes info", DDEFV::DESC::VDTV_VALUE,
		FCC('M4P '), "AES encrypted audio", DDEFV::DESC::VDTV_VALUE,
		FCC('M4B '), "Apple audio w/ iTunes position", DDEFV::DESC::VDTV_VALUE,
		FCC('mp71'), "ISO 14496-12 MPEG-7 meta data", DDEFV::DESC::VDTV_VALUE,
		FCC('ndxh'), "ISO 14496-12 MPEG-7 meta data", DDEFV::DESC::VDTV_VALUE,
	};
	/*static const*/ DDEF ddef = {
		4, "File Type", "'ftyp'", 12, 0,
		DDEF::DESC::DT_FCC, 0, 2, "MajorBrand (www.mp4ra.org)", &ddefv1, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 2, "MajorBrandVersion", nullptr, nullptr, 
		DDEF::DESC::DT_LOOP, 0, 2, "CompatibleBrands", nullptr, nullptr, 
		DDEF::DESC::DT_FCC, 0, 2, "CompatibleBrand", &ddefv1, nullptr, 
	};
	FOURCC fcc;
	ddef.desc[0].pStoreValue = &fcc;
	bool bRet = (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	m_bMP4 = (fcc != FCC('qt  '));
	return bRet;
}

bool CMOVParser::ReadMvhdChunk( int hti, DWORD nChunkSize )
{
	BYTE nVersion;
	if ( !PeekVersion( nVersion ) )
		return false;
	ASSERT( GetReadPos() == GetReadTotalBytes() );
	if ( nVersion == 0 ) {
		static constexpr DDEF ddef = {
			17, "Movie Header", "'mvhd'", 100, 100,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // 0
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // Reserved 0
			DDEF::DESC::DT_APPLEDATE1904, 0, 3, "CreationTime", nullptr, nullptr,
			DDEF::DESC::DT_APPLEDATE1904, 0, 3, "ModificationTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Timescale", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "Duration", nullptr, nullptr,
			DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "PreferredRate", nullptr, nullptr,
			DDEF::DESC::DT_APPLEFIXEDFLOAT16, 0, 3, "PreferredVolume", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 10, 4, "Reserved", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 36, 4, "Matrix", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PreviewTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PreviewDuration", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PosterTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SelectionTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SelectionDuration", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "CurrentTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "NextTrackID", nullptr, nullptr,
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else if ( nVersion == 1 ) {
		static constexpr DDEF ddef = {
			17, "Movie Header", "'mvhd'", 112, 112,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // 1
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // Reserved 0
			DDEF::DESC::DT_APPLEDATE1904_64, 0, 3, "CreationTime", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEDATE1904_64, 0, 3, "ModificationTime", nullptr, nullptr, 
			DDEF::DESC::DT_DWORD, 0, 3, "Timescale", nullptr, nullptr,
			DDEF::DESC::DT_QWORD, 0, 3, "Duration", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "PreferredRate", nullptr, nullptr,
			DDEF::DESC::DT_APPLEFIXEDFLOAT16, 0, 3, "PreferredVolume", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 10, 4, "Reserved", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 36, 4, "Matrix", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PreviewTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PreviewDuration", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "PosterTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SelectionTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "SelectionDuration", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "CurrentTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "NextTrackID", nullptr, nullptr,
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else {
		TRACE3( _T("%s (%s): Unexpected Version %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nVersion );
		return ReadSeekFwdDump( nChunkSize, hti );
	}
}


bool CMOVParser::ReadMdatChunk( int hti, QWORD nParentSize ) 
{
//FOURCC fcc;
//QWORD nQSize;
//DWORD nDSize;
//int htitmp;

//	if ( m_bMP4 ) 
		return ReadSeekFwdDumpChecksum( nParentSize, m_nChecksum, hti );

	/* bringt's irgendwie nicht, nur QT mov haben im MDAT noch einen MDAT verschachtelt
	while ( nParentSize >= 8 ) {
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nDSize, fcc, hti ) )
			return false;
		nQSize = nDSize;
		if ( nQSize == 0 )
			nQSize = nParentSize;
		else if ( nQSize == 1 ) {
			ASSERT( false ); // size is in extended size field
			if ( !ReadBytes( &nQSize, sizeof( nQSize ), hti ) )
				return false;
		}
		if ( nQSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nQSize = nParentSize;
			else 
				nParentSize = nQSize;
		}
		nParentSize -= nQSize;
		switch ( fcc ) {
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			ASSERT( false );
			// fall through...
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Atom"), fcc, nQSize );
			if ( nQSize > 8 && !ReadSeekFwdDump( nQSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
	*/
}

bool CMOVParser::ReadTrakChunk( int hti, DWORD nParentSize ) 
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT(GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('udta'): // TODO ParseGeneric...
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("User Data"), fcc, nSize );
			if ( !ReadUdtaChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('tkhd'): 
			if ( !ReadTkhdChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('mdia'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Media"), fcc, nSize );
			if ( !ReadMdiaChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('edts'):
			if ( !ReadEdtsChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('clip'): // TODO
		case FCC('matt'): // includes 'kmat'
		case FCC('load'):
#if 0
      * 8+ bytes QUICKTIME preload atom
          = long unsigned offset + long ASCII text string 'load'
        -> 8 bytes preload time
            = long unsigned start time + long unsigned time length (in time units)
        -> 4 bytes flags = long integer value
          - flags are PreloadAlways = 1 or TrackEnabledPreload = 2
        -> 4 bytes default hints flags = long hex data play options
          - flags are KeepInBuffer = 0x00000004 ; HighQuality = 0x00000100 ;
          - flags are SingleFieldPlayback = 0x00100000
          - flags are DeinterlaceFields = 0x04000000

#endif
		case FCC('tref'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Track Reference"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break; 
		case FCC('tapt'):
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("Track Aperture Mode Dimensions Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
				return false;
			break;
#if 0
      * 8+ bytes optional track references box
          = long unsigned offset + long ASCII text string 'tref'

         * 8+ bytes type of reference box
             = long unsigned offset + long ASCII text string
          -> vers. 1 box type is stream hint = 'hint'
          -> vers. 2 box types are other dependency = 'dpnd' ; IPI declarations = 'ipir'
          -> vers. 2 box types are elementary stream = 'mpod' ;
          -> vers. 2 box types are synchronization (video/audio) = 'sync
          -> QUICKTIME atom types are timecode = 'tmcd'; chapterlist = 'chap'
          -> QUICKTIME atom types are transcript (text) = 'scpt'
          -> QUICKTIME atom types are non-primary source (used in other track) = 'ssrc'
           -> 4+ bytes Track IDs = long integer track numbers (Disabled Track ID = 0)
#endif
		case FCC('imap'):
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Input Map"), fcc, nSize );
			if ( !ReadImapChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
				return false;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CMOVParser::ReadMdiaChunk( int hti, DWORD nParentSize ) 
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT( GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('mdhd'): 
			if ( !ReadMdhdChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('minf'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Media Info"), fcc, nSize );
			if ( !ReadMinfChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('hdlr'):
			if ( !ReadHdlrChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('udta'): // TODO ParseGeneric...
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("User Data"), fcc, nSize );
			if ( !ReadUdtaChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
		case FCC('elng'): // TODO 
			//ASSERT( false );
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
				return false;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CMOVParser::ReadMinfChunk( int hti, DWORD nParentSize ) 
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT(GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('stbl'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Sample Table"), fcc, nSize );
			if ( !ReadStblChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('hdlr'): 
			if ( !ReadHdlrChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('vmhd'): 
			if ( !ReadVmhdChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('smhd'): 
			if ( !ReadSmhdChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('dinf'): 
			if ( !ReadDinfChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('hmhd'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Hint Media Header"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('gmhd'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Base Media Information Header"), fcc, nSize );
			if ( !ReadGmhdChunk( htitmp, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('gmin'): // Das ist dokumentiert als Subelement von minf, kommt aber (auch?) als Subelement von gmhd vor
			if ( !ReadGminChunk( hti, nSize - 8 ) ) 
				return false;
			ASSERT( false ); // kommt das überhaupt vor???
			break;
		case FCC('code'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) ) // TODO
				return false;
			break;
		case FCC('nmhd'): // TODO
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("MP4 Null Media Header"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			m_bMP4 = true;
			break;
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			ASSERT( false );
			// fall through...
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CMOVParser::ReadStblChunk( int hti, DWORD nParentSize ) 
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT(GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('stsd'): 
			if ( !ReadStsdChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('stts'): // TODO http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_5.html#//apple_ref/doc/uid/TP40000939-CH204-BBCJEIIA 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Sample Time To Sample Duration"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('stss'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Key Frames (Sync Samples)"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('stsc'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Sample To Chunk"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('stsz'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Sample Size"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('stco'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Sample Chunk Offset"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('stsh'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Sample Shadow Sync"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('ctts'): // Composition Time to Samples, http://www.koders.com/c/fid03BE1609322A74DDDE1E39825A2E452ACA218795.aspx
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Composition Time To Samples"), fcc, nSize ); 
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;

		case FCC('stps'): // MP4???
		case FCC('sdtp'): 
		case FCC('cslg'): // ??? unknown
		case FCC('co64'): 
		case FCC('sgpd'): // Sample Group Description atom
		case FCC('sbgp'): // Sample-to-group atom
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Atom"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('free'): // TODO diese 4 können auch in Subatoms vorkommen
		case FCC('skip'): 
		case FCC('pnot'): 
		case FCC('wide'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Atom"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			ASSERT( false );
			// fall through...
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Unexpected Token!") ); // RODO ERROR, wenn sicher, dass ich alle behandle
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
#if 0
            * 8+ bytes sample (framing info) table box
                = long unsigned offset + long ASCII text string 'stbl'
			   
			   * stsd

               * 8+ bytes time to sample (frame timing) box
                   = long unsigned offset + long ASCII text string 'stts'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes number of times = long unsigned total
                 -> 8+ bytes time per frame
                      = long unsigned frame count + long unsigned duration
                   - multiple durations means variable framing rate
                   - single duration means fixed framing rate
                   - calculate framing (fps): media units / (average) duration

               * 8+ bytes optional sync sample (key/intra frame) box
                   = long unsigned offset + long ASCII text string 'stss'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes number of key frames = long unsigned total
                 -> 4+ bytes key/intra frame location = long unsigned framing time
                   - key/intra frame location according to sample/framing time

               * 8+ bytes sample/framing to chunk/block box
                   = long unsigned offset + long ASCII text string 'stsc'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes number of blocks = long unsigned total
                 -> 8+ bytes frames per block
                      = long unsigned first/next block + long unsigned # of frames
                 -> 4+ bytes samples description id
                      = long unsigned description number

               * 8+ bytes sample (block byte) size box
                   = long unsigned offset + long ASCII text string 'stsz'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes block byte size for all = 32-bit integer byte value
                     (different sizes = 0)
                 -> 4 bytes number of block sizes = long unsigned total
                 -> 4+ bytes block byte sizes = long unsigned byte values

               * 8+ bytes chunk/block offset box
                   = long unsigned offset + long ASCII text string 'stco'
                 -> 4 bytes number of offsets = long unsigned total
                 -> 4+ bytes block offsets = long unsigned byte values

               * 8+ bytes larger chunk/block offset box
                   = long unsigned offset + long ASCII text string 'co64'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes number of offsets = long unsigned total
                 -> 8+ bytes larger block offsets = 64-bit unsigned byte values
#endif
}

bool CMOVParser::ReadStsdChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_5.html#//apple_ref/doc/uid/TP40000939-CH204-BBCJEIIA
#if 0
               * 8+ bytes sample (frame encoding) description box
                   = long unsigned offset + long ASCII text string 'stsd'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes number of descriptions = long unsigned total
                     (default = 1)
 
                 -> 4 bytes description length = long unsigned length
                 -> 4 bytes description visual format = long ASCII text string 'mp4v'
                   - if encoded to ISO/IEC 14496-10 or 3GPP AVC standards then use:
                 -> 4 bytes description visual format = long ASCII text string 'avc1'
                   - if encrypted to ISO/IEC 14496-12 or 3GPP standards then use:
                 -> 4 bytes description visual format = long ASCII text string 'encv'
                   - if encoded to 3GPP H.263v1 standards then use:
                 -> 4 bytes description visual format = long ASCII text string 's263'
                 -> 6 bytes reserved = 48-bit value set to zero
                 -> 2 bytes data reference index
                     = short unsigned index from 'dref' box
                   - there are other sample descriptions
                      available in the Apple QT format dev docs
                 -> 2 bytes QUICKTIME video encoding version = short hex version
                   - default = 0 ; audio data size before decompression = 1
                 -> 2 bytes QUICKTIME video encoding revision level = byte hex version
                   - default = 0 ; video can revise this value
                 -> 4 bytes QUICKTIME video encoding vendor = long ASCII text string
                   - default = 0
                 -> 4 bytes QUICKTIME video temporal quality = long unsigned value (0 to 1024)
                 -> 4 bytes QUICKTIME video spatial quality = long unsigned value (0 to 1024)
                   - some quality values are lossless = 1024 ; maximum = 1023 ; high = 768
                   - some quality values are normal = 512 ; low = 256 ; minimum = 0
                 -> 4 bytes video frame pixel size
                     = short unsigned width + short unsigned height
                 -> 8 bytes video resolution
                     = long fixed point horizontal + long fixed point vertical
                   - defaults to 72.0 dpi
                 -> 4 bytes QUICKTIME video data size = long value set to zero
                 -> 2 bytes video frame count = short unsigned total (set to 1)
                 -> 1 byte video encoding name string length = byte unsigned length
                 -> 31 bytes video encoder name string
                 -> NOTE: if video encoder name string < 31 chars then pad with zeros
                 -> 2 bytes video pixel depth = short unsigned bit depth
                   - colors are 1 (Monochrome), 2 (4), 4 (16), 8 (256)
                   - colors are 16 (1000s), 24 (Ms), 32 (Ms+A)
                   - grays are 33 (B/W), 34 (4), 36 (16), 40(256)
                 -> 2 bytes QUICKTIME video color table id = short integer value
                     (no table = -1)
                 -> optional QUICKTIME color table data if above set to 0
                     (see color table atom below for layout)
                 OR
                 -> 4 bytes description length = long unsigned length
                 -> 4 bytes description audio format = long ASCII text string 'mp4a'
                   - if encrypted to ISO/IEC 14496-12 or 3GPP standards then use:
                 -> 4 bytes description audio format = long ASCII text string 'enca'
                   - if encoded to 3GPP GSM 6.10 AMR narrowband standards then use:
                 -> 4 bytes description audio format = long ASCII text string 'samr'
                   - if encoded to 3GPP GSM 6.10 AMR wideband standards then use:
                 -> 4 bytes description audio format = long ASCII text string 'sawb'
                 -> 6 bytes reserved = 48-bit value set to zero
                 -> 2 bytes data reference index
                     = short unsigned index from 'dref' box
                 -> 2 bytes QUICKTIME audio encoding version = short hex version
                   - default = 0 ; audio data size before decompression = 1
                 -> 2 bytes QUICKTIME audio encoding revision level
                     = byte hex version
                   - default = 0 ; video can revise this value
                 -> 4 bytes QUICKTIME audio encoding vendor
                     = long ASCII text string
                   - default = 0
                 -> 2 bytes audio channels = short unsigned count
                     (mono = 1 ; stereo = 2)
                 -> 2 bytes audio sample size = short unsigned value
                     (8 or 16)
                 -> 2 bytes QUICKTIME audio compression id = short integer value
                   - default = 0
                 -> 2 bytes QUICKTIME audio packet size = short value set to zero
                 -> 4 bytes audio sample rate = long unsigned fixed point rate
                 OR
                 -> 4 bytes description length = long unsigned length
                 -> 4 bytes description system format = long ASCII text string 'mp4s'
                   - if encrypted to ISO/IEC 14496-12 standards then use:
                 -> 4 bytes description system format = long ASCII text string 'encs'
                 -> 6 bytes reserved = 48-bit value set to zero
                 -> 2 bytes data reference index
                     = short unsigned index from 'dref' box

                  * 8+ bytes ISO/IEC 14496-12/3GPP encryption scheme info box
                      = long unsigned offset + long ASCII text string 'sinf'
                   - if stream encrypted to ISO/IEC 14496-12 standards

                     * 8+ bytes ISO/IEC 14496-12/3GPP/QUICKTIME original format box
                         = long unsigned offset + long ASCII text string 'frma'
                       -> 4 bytes description format = long ASCII text string
                         - formats are MPEG-4 visual = 'mp4v' ; MPEG-4 AVC = 'avc1'
                         - formats are MPEG-4 audio = 'mp4a' ; MPEG-4 system = 'mp4s'
                         - 3GPP formats are H.253 = 's263' ; AMR narrow = 'samr'
                         - 3GPP format is AMR wide = 'sawb'

                     * 8+ bytes optional ISO/IEC 14496-12 IPMP info box
                         = long unsigned offset + long ASCII text string 'imif'
                       -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                           (current = 0)
                       -> IPMP descriptors = hex dump from IPMP part of ES Descriptor box

                     * 8+ bytes optional ISO/IEC 14496-12/3GPP scheme type box
                         = long unsigned offset + long ASCII text string 'schm'
                       -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                           (current = 0 ; contains URI if flags = 0x000001)
                       -> 4 bytes encryption type = long ASCII text string
                         - types are 128-bit AES counter = 'ACM1' ; 128-bit AES FS = 'AFS1'
                         - types are nullptr algorithm = 'ENUL' ; 160-bit HMAC-SHA-1 = 'SHM2'
                         - types are RTCP = 'ANUL' ; private scheme = '    '
                       -> 2 bytes encryption version = short unsigned version
                       -> optional scheme URI string = UTF-8 text string
                         (eg. web site)
                       -> 1 byte optional scheme URI string end = byte padding set to zero

                     * 8+ bytes ISO/IEC 14496-12/3GPP scheme data box
                         = long unsigned offset + long ASCII text string 'schi'
                       -> encryption related key = hex dump

                  * 8+ bytes 3GPP H.263v1 decode config box
                      = long unsigned offset + long ASCII text string 'd263'
                    -> 4 bytes encoder vendor = long ASCII text string
                    -> 1 byte encoder version = 8-bit unsigned revision
                    -> 1 byte H.263 level = 8-bit unsigned stream level
                    -> 1 byte H.263 profile = 8-bit unsigned stream profile

                     * 8+ bytes optional 3GPP H.263v1 bit rate box
                         = long unsigned offset + long ASCII text string 'bitr'
                       -> 4 bytes average bit rate = 32-bit unsigned value
                       -> 4 bytes maximum bit rate = 32-bit unsigned value

                  * 8+ bytes 3GPP GSM 6.10 AMR decode config box
                      = long unsigned offset + long ASCII text string 'damr'
                    -> 4 bytes encoder vendor = long ASCII text string
                    -> 1 byte encoder version = 8-bit unsigned revision
                    -> 2 byte packet modes = 16-bit unsigned bit mode index
                    -> 1 byte number of packet mode changes = 8-bit unsigned value
                    -> 1 byte samples per packet = 8-bit unsigned value

                  * 8+ bytes ISO/IEC 14496-10 or 3GPP AVC decode config box
                      = long unsigned offset + long ASCII text string 'avcC'
                    -> 1 byte version = 8-bit hex version  (current = 1)
                    -> 1 byte H.264 profile = 8-bit unsigned stream profile
                    -> 1 byte H.264 compatible profiles = 8-bit hex flags
                    -> 1 byte H.264 level = 8-bit unsigned stream level
                    -> 1 1/2 nibble reserved = 6-bit unsigned value set to 63
                    -> 1/2 nibble NAL length = 2-bit length byte size type
                      - 1 byte = 0 ; 2 bytes = 1 ; 4 bytes = 3
                    -> 1 byte number of SPS = 8-bit unsigned total
                    -> 2+ bytes SPS length = short unsigned length
                    -> + SPS NAL unit = hexdump
                    -> 1 byte number of PPS = 8-bit unsigned total
                    -> 2+ bytes PPS length = short unsigned length
                    -> + PPS NAL unit = hexdump

                  * 8+ bytes vers. 2 ES Descriptor box
                      = long unsigned offset + long ASCII text string 'esds'
                   - if encoded to ISO/IEC 14496-10 AVC standards then optionally use:
                      = long unsigned offset + long ASCII text string 'm4ds'

                    -> 4 bytes version/flags = 8-bit hex version + 24-bit hex flags
                        (current = 0)

                    -> 1 byte ES descriptor type tag = 8-bit hex value 0x03
                    -> 3 bytes extended descriptor type tag string = 3 * 8-bit hex value
                      - types are Start = 0x80 ; End = 0xFE
                      - NOTE: the extended start tags may be left out
                    -> 1 byte descriptor type length = 8-bit unsigned length

                      -> 2 bytes ES ID = 16-bit unsigned value
                      -> 1 byte stream priority = 8-bit unsigned value
                        - Defaults to 16 and ranges from 0 through to 31

                        -> 1 byte decoder config descriptor type tag = 8-bit hex value 0x04
                        -> 3 bytes extended descriptor type tag string = 3 * 8-bit hex value
                          - types are Start = 0x80 ; End = 0xFE
                          - NOTE: the extended start tags may be left out
                        -> 1 byte descriptor type length = 8-bit unsigned length

                          -> 1 byte object type ID = 8-bit unsigned value
                            - type IDs are system v1 = 1 ; system v2 = 2
                            - type IDs are MPEG-4 video = 32 ; MPEG-4 AVC SPS = 33
                            - type IDs are MPEG-4 AVC PPS = 34 ; MPEG-4 audio = 64
                            - type IDs are MPEG-2 simple video = 96
                            - type IDs are MPEG-2 main video = 97
                            - type IDs are MPEG-2 SNR video = 98
                            - type IDs are MPEG-2 spatial video = 99
                            - type IDs are MPEG-2 high video = 100
                            - type IDs are MPEG-2 4:2:2 video = 101
                            - type IDs are MPEG-4 ADTS main = 102
                            - type IDs are MPEG-4 ADTS Low Complexity = 103
                            - type IDs are MPEG-4 ADTS Scalable Sampling Rate = 104
                            - type IDs are MPEG-2 ADTS = 105 ; MPEG-1 video = 106
                            - type IDs are MPEG-1 ADTS = 107 ; JPEG video = 108
                            - type IDs are private audio = 192 ; private video = 208
                            - type IDs are 16-bit PCM LE audio = 224 ; vorbis audio = 225
                            - type IDs are dolby v3 (AC3) audio = 226 ; alaw audio = 227
                            - type IDs are mulaw audio = 228 ; G723 ADPCM audio = 229
                            - type IDs are 16-bit PCM Big Endian audio = 230
                            - type IDs are Y'CbCr 4:2:0 (YV12) video = 240 ; H264 video = 241
                            - type IDs are H263 video = 242 ; H261 video = 243
                          -> 6 bits stream type = 3/4 byte hex value
                            - type IDs are object descript. = 1 ; clock ref. = 2
                            - type IDs are scene descript. = 4 ; visual = 4
                            - type IDs are audio = 5 ; MPEG-7 = 6 ; IPMP = 7
                            - type IDs are OCI = 8 ; MPEG Java = 9
                            - type IDs are user private = 32
                          -> 1 bit upstream flag = 1/8 byte hex value
                          -> 1 bit reserved flag = 1/8 byte hex value set to 1
                          -> 3 bytes buffer size = 24-bit unsigned value
                          -> 4 bytes maximum bit rate = 32-bit unsigned value
                          -> 4 bytes average bit rate = 32-bit unsigned value

                            -> 1 byte decoder specific descriptor type tag
                                = 8-bit hex value 0x05
                            -> 3 bytes extended descriptor type tag string
                                = 3 * 8-bit hex value
                              - types are Start = 0x80 ; End = 0xFE
                              - NOTE: the extended start tags may be left out
                            -> 1 byte descriptor type length
                                = 8-bit unsigned length

                              -> ES header start codes = hex dump

                        -> 1 byte SL config descriptor type tag = 8-bit hex value 0x06
                        -> 3 bytes extended descriptor type tag string = 3 * 8-bit hex value
                          - types are Start = 0x80 ; End = 0xFE
                          - NOTE: the extended start tags may be left out
                        -> 1 byte descriptor type length = 8-bit unsigned length

                          -> 1 byte SL value = 8-bit hex value set to 0x02

                  * 8+ bytes QUICKTIME video gamma atom
                      = long unsigned offset + long ASCII text string 'gama'
                    -> 4 bytes decimal level = long fixed point level
                 
                  * 8+ bytes QUICKTIME video field order atom
                      = long unsigned offset + long ASCII text string 'fiel'
                    -> 2 bytes field count/order = byte integer total + byte integer order
                 
                  * 8+ bytes QUICKTIME video m-jpeg quantize table atom
                      = long unsigned offset + long ASCII text string 'mjqt'
                    -> quantization table = hex dump
                   
                  * 8+ bytes QUICKTIME video m-jpeg huffman table atom
                      = long unsigned offset + long ASCII text string 'mjht'
                    -> huffman table = hex dump

#endif
	static constexpr DDEF ddef = {
		9, "Sample Description", "'stsd'", 8, 0,
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr,
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // Reserved 0
		DDEF::DESC::DT_DWORD, 0, 3, "NumberOfEntries", nullptr, nullptr,

		DDEF::DESC::DT_NODELOOP, -1, 3, "Descriptions", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr, 
		DDEF::DESC::DT_FCC, 0, 3, "DataFormat", nullptr, nullptr, 
		DDEF::DESC::DT_BYTES, 6, 3, "Reserved", nullptr, nullptr, // Reserved 0
		DDEF::DESC::DT_WORD, 0, 3, "DataReferenceIndex", nullptr, nullptr, 
//		DDEF::DESC::DT_BYTES, -4, 3, "Additional Data", nullptr, nullptr, // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap3/chapter_4_section_1.html#//apple_ref/doc/uid/TP40000939-CH205-BBCCBGDG
		DDEF::DESC::DT_BYTES_ADJUSTSIZE, -4, 3, "AdditionalData", nullptr, nullptr, // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap3/chapter_4_section_1.html#//apple_ref/doc/uid/TP40000939-CH205-BBCCBGDG
	};
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
}

bool CMOVParser::ReadTkhdChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_3.html#//apple_ref/doc/uid/TP40000939-CH204-25550
	static constexpr DDEFV ddefv1 = {
		4, DDEFV::VDT_POSTFIX,
		1, "Track Enabled", DDEFV::DESC::VDTV_BITFIELD, // MP4 only this valid
		2, "Track in Movie", DDEFV::DESC::VDTV_BITFIELD,
		4, "Track in Preview", DDEFV::DESC::VDTV_BITFIELD,
		8, "Track in Poster", DDEFV::DESC::VDTV_BITFIELD,
	};
	BYTE nVersion;
	if ( !PeekVersion( nVersion ) )
		return false;
	if ( nVersion == 0 ) {
		static constexpr DDEF ddef = {
			15, "Track Header", "'tkhd'", 84, 84,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // 0 
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv1, nullptr, 
			DDEF::DESC::DT_APPLEDATE1904, 0, 3, "CreationTime", nullptr, nullptr,
			DDEF::DESC::DT_APPLEDATE1904, 0, 3, "ModificationTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "TrackID", nullptr, nullptr, // > 0
			DDEF::DESC::DT_DWORD, 0, 4, "Reserved", nullptr, nullptr, // == 0
			DDEF::DESC::DT_DWORD, 0, 3, "Duration", nullptr, nullptr,
			DDEF::DESC::DT_BYTES, 8, 4, "Reserved", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 3, "Layer", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 3, "AlternateGroup", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT16, 0, 3, "Volume", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 4, "Reserved", nullptr, nullptr, // == 0
			DDEF::DESC::DT_BYTES, 36, 4, "MatrixStructure", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "TrackWidth", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "TrackHeight", nullptr, nullptr, 
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else if ( nVersion == 1 ) {
		static constexpr DDEF ddef = {
			15, "Track Header", "'tkhd'", 96, 96,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // 1
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv1, nullptr, 
			DDEF::DESC::DT_APPLEDATE1904_64, 0, 3, "CreationTime", nullptr, nullptr,
			DDEF::DESC::DT_APPLEDATE1904_64, 0, 3, "ModificationTime", nullptr, nullptr,
			DDEF::DESC::DT_DWORD, 0, 3, "TrackID", nullptr, nullptr, // > 0
			DDEF::DESC::DT_DWORD, 0, 4, "Reserved", nullptr, nullptr, // == 0
			DDEF::DESC::DT_QWORD, 0, 3, "Duration", nullptr, nullptr, 
			DDEF::DESC::DT_BYTES, 8, 4, "Reserved", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 3, "Layer", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 3, "AlternateGroup", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT16, 0, 3, "Volume", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 4, "Reserved", nullptr, nullptr, // == 0
			DDEF::DESC::DT_BYTES, 36, 4, "MatrixStructure", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "TrackWidth", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "TrackHeight", nullptr, nullptr, 
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else {
		TRACE3( _T("%s (%s): Unexpected Version %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nVersion );
		return ReadSeekFwdDump( nChunkSize, hti );
	}
}

bool CMOVParser::ReadImapChunk( int hti, DWORD nParentSize )
{ 
#if 0
     * 8+ bytes QUICKTIME non-primary source input map atom
          = long unsigned offset + long ASCII text string 'imap'

         * 8+ bytes input atom
             = long unsigned offset + long ASCII text string 0x0000 + 'in'
           -> 4 bytes atom ID = long integer atom reference (first ID = 1)
           -> 2 bytes reserved = short value set to zero
           -> 2 bytes number of internal atoms = short unsigned count
           -> 4 bytes reserved = long value set to zero

            * 8+ bytes input type atom
                = 32-bit integer unsigned + long ASCII text string 0x0000 + 'ty'
              -> 4 bytes type modifier name = long integer value
                -> name values are matrix = 1 ; clip = 2 ;
                -> name values are volume = 3; audio balance = 4
                -> name values are graphics mode = 5; matrix object = 6
                -> name values are graphics mode object = 7; image type = 'vide'

            * 8+ bytes object ID atom
                = long unsigned offset + long ASCII text string 'obid'
              -> 4 bytes object ID = long integer value

#endif
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT(GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('  in'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Input"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('  ty'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Input Type"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		case FCC('obid'): 
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("obid???"), fcc, nSize ); // TODO
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CMOVParser::ReadEdtsChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_3.html#//apple_ref/doc/uid/TP40000939-CH204-25579
	static constexpr DDEF ddef = {
		9, "Edit", "'edts'", 16, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr, 
		DDEF::DESC::DT_FCC, 0, 3, "Type", nullptr, nullptr, // 'elst' 
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // TODO if version is 1 then duration values are 8 bytes in length
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "NumberOfEntries", nullptr, nullptr, 
		DDEF::DESC::DT_NODELOOP, -1, 3, "EditList", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "TrackDuration", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 4, "MediaTime", nullptr, nullptr, 
		DDEF::DESC::DT_APPLEFIXEDFLOAT32, 0, 3, "MediaRate", nullptr, nullptr, 
	};
#if 0
        * 8+ bytes optional edit list box
             = long unsigned offset + long ASCII text string 'elst'
           -> 1 byte version = byte unsigned value
             - if version is 1 then duration values are 8 bytes in length
           -> 3 bytes flags = 24-bit hex flags (current = 0)
           -> 4 bytes number of edits = long unsigned total (default = 1)

           -> 8 bytes edit time
               = long unsigned time length + long unsigned start time (in time units)
           OR
           -> 16 bytes edit time
                = 64-bit unsigned time length + 64-bit unsigned start time (in time units)
             - if start time is -1, then that time length is edited out

           -> 4 bytes decimal playback speed = long fixed point rate (normal = 1.0)
#endif
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
}

bool CMOVParser::ReadMdhdChunk( int hti, DWORD nChunkSize )
{
// TODO http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap4/chapter_5_section_2.html#//apple_ref/doc/uid/TP40000939-CH206-BBCBIICE
	static constexpr DDEFV ddefv1 = {
		10, DDEFV::VDT_INFIX,
		0, "English", DDEFV::DESC::VDTV_VALUE,
		1, "French", DDEFV::DESC::VDTV_VALUE,
		2, "German", DDEFV::DESC::VDTV_VALUE,
		3, "Italian", DDEFV::DESC::VDTV_VALUE,
		4, "Dutch", DDEFV::DESC::VDTV_VALUE,
		5, "Swedish", DDEFV::DESC::VDTV_VALUE,
		6, "Spanish", DDEFV::DESC::VDTV_VALUE,
		7, "Danish", DDEFV::DESC::VDTV_VALUE,
		8, "Portuguese", DDEFV::DESC::VDTV_VALUE,
		9, "Norwegian", DDEFV::DESC::VDTV_VALUE,
		// ...
	};
	BYTE nVersion;
	if ( !PeekVersion( nVersion ) )
		return false;
	if ( nVersion == 0 ) {
		static constexpr DDEF ddef = {
			8, "Media Header", "'mdhd'", 24, 24,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // 1
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr,  // == 0
			DDEF::DESC::DT_APPLEDATE1904, 0, 3, "CreationTime", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEDATE1904, 0, 3, "ModificationTime", nullptr, nullptr, 
			DDEF::DESC::DT_DWORD, 0, 3, "Time Scale", nullptr, nullptr, 
			DDEF::DESC::DT_DWORD, 0, 3, "Duration", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 3, "Language", &ddefv1, nullptr, 
	#if 0
			   -> 1/8 byte ISO language padding = 1-bit value set to 0
			   -> 1 7/8 bytes content language = 3 * 5-bits ISO 639-2 language code less 0x60
				 - example code for english = 0x15C7
			   -> 2 bytes QUICKTIME quality = short integer playback quality value (normal = 0)
	#endif
			DDEF::DESC::DT_WORD, 0, 3, "Quality", nullptr, nullptr, 
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else if ( nVersion == 1 ) {
		static constexpr DDEF ddef = {
			8, "Media Header", "'mdhd'", 36, 36,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, // 1
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr,  // == 0
			DDEF::DESC::DT_APPLEDATE1904_64, 0, 3, "CreationTime", nullptr, nullptr, 
			DDEF::DESC::DT_APPLEDATE1904_64, 0, 3, "ModificationTime", nullptr, nullptr, // TODO
			DDEF::DESC::DT_DWORD, 0, 3, "TimeScale", nullptr, nullptr, 
			DDEF::DESC::DT_QWORD, 0, 3, "Duration", nullptr, nullptr, 
			DDEF::DESC::DT_WORD, 0, 3, "Language", &ddefv1, nullptr, 
	#if 0
			   -> 1/8 byte ISO language padding = 1-bit value set to 0
			   -> 1 7/8 bytes content language = 3 * 5-bits ISO 639-2 language code less 0x60
				 - example code for english = 0x15C7
			   -> 2 bytes QUICKTIME quality = short integer playback quality value (normal = 0)
	#endif
			DDEF::DESC::DT_WORD, 0, 3, "Quality", nullptr, nullptr, 
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else {
		TRACE3( _T("%s (%s): Unexpected Version %u.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), nVersion );
		return ReadSeekFwdDump( nChunkSize, hti );
	}
}

bool CMOVParser::ReadVmhdChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25626
	static constexpr DDEFV ddefv1 = {
		1, DDEFV::VDT_POSTFIX,
		0x000001, "No Lean Ahead (Quicktime 1.0 incompatible)", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEFV ddefv2 = {
		9, DDEFV::VDT_INFIX,
		0x0000, "Copy", DDEFV::DESC::VDTV_VALUE,
		0x0040, "Dither Copy", DDEFV::DESC::VDTV_VALUE,
		0x0020, "Blend (Opcolor)", DDEFV::DESC::VDTV_VALUE,
		0x0024, "Transparent (Opcolor)", DDEFV::DESC::VDTV_VALUE,
		0x0100, "Straight Alpha", DDEFV::DESC::VDTV_VALUE,
		0x0101, "Premul White Alpha", DDEFV::DESC::VDTV_VALUE,
		0x0102, "Premul Black Alpha", DDEFV::DESC::VDTV_VALUE,
		0x0104, "Straight Alpha Blend (Opcolor)", DDEFV::DESC::VDTV_VALUE,
		0x0103, "Composition (Dither Copy)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		6, "Video Media Information Header", "'vmhd'", 12, 12,
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv1, nullptr,
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "GraphicsMode", &ddefv2, nullptr, 
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "OpcolorRed", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "OpcolorGreen", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "OpcolorBlue", nullptr, nullptr, 
	};
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
#if 0
            * 8+ bytes visual media (stream) info header box
                = long unsigned offset + long ASCII text string 'vmhd'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                - version = 0 ; flags = 0x000001 for QUICKTIME or zero MPEG-4
              -> 2 bytes QuickDraw graphic mode = short hex type
                - mode types are copy = 0x0000 ; dither copy = 0x0040 ; straight alpha = 0x0100
                - mode types are composition dither copy = 0x0103 ; blend = 0x0020
                - mode premultipled types are white alpha = 0x101 ; black alpha = 0x102
                - mode color types are transparent = 0x0024; straight alpha blend = 0x0104
                - NOTE: MPEG-4 only uses copy mode and quicktime uses dither copy by default
              -> 6 bytes graphic mode color = 3 * short unsigned QuickDraw RGB color values
            OR
            * 8+ bytes sound media (stream) info header box
                = long unsigned offset + long ASCII text string 'smhd'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)
              -> 2 bytes audio balance = short fixed point value
               - balnce scale is left = negatives ; normal = 0.0 ; right = positives
              -> 2 bytes reserved = short value set to zero
            OR
            * 8+ bytes hint stream (stream) info header box
                = long unsigned offset + long ASCII text string 'hint'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)
              -> 2 bytes maximum packet delivery unit = short unsigned value
              -> 2 bytes average packet delivery unit = short unsigned value
              -> 4 bytes maximum bit rate = long unsigned value
              -> 4 bytes average bit rate = long unsigned value
              -> 4 bytes reserved = long value set to zero
            OR
            * 8+ bytes mpeg-4 media (stream) header box
                = long unsigned offset + long ASCII text string 'nmhd'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)

#endif
}

bool CMOVParser::ReadSmhdChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25626
	static constexpr DDEF ddef = {
		4, "Sound Media Information Header", "'smhd'", 8, 8,
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // == 0
 		DDEF::DESC::DT_APPLEFIXEDFLOAT16, 0, 3, "Balance", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "Reserved", nullptr, nullptr, // == 0
	};
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
}


bool CMOVParser::ReadGmhdChunk( int hti, DWORD nParentSize ) 
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25663
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT(GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch ( fcc ) {
		case FCC('gmin'): // TODO ParseGeneric...
			if ( !ReadGminChunk( hti, nSize - 8 ) ) 
				return false;
			break; 
		case FCC('tmcd'): // TODO Timecode, enthält tcmi
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) ) // TODO
				return false;
			break;
		case FCC('text'): // TODO 
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
				return false;
			break;
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize );
			OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!") );
			TRACE3( _T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString( fcc ).c_str() );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
}

bool CMOVParser::ReadMetaChunk(int hti, DWORD nParentSize)
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25663
	FOURCC fcc;
	DWORD nSize;
	int htitmp;
	
	static constexpr DDEF ddef = {
		2, "Meta", "'meta'", 4, 0,
		DDEF::DESC::DT_BYTE, 0, 3, "Version (0)", nullptr, nullptr,
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags (0)", nullptr, nullptr,
	};
	if (Dump(hti, 8, 4 + 8, &ddef) != 4)
		return false;

	nParentSize -= 4;
	hti++; // TODO!!!
	while (nParentSize >= 8) {
		ASSERT(GetReadPos() == GetReadTotalBytes());
		if (GetReadTotalBytes() >= GetReadFileSize())
			return false;
		if (!ReadSizeType(nSize, fcc, hti))
			return false;
		if (nSize > nParentSize) {
			OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!"));
			if (!Settings.bForceChunkSize)
				nSize = nParentSize;
			else
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch (fcc) {
		case FCC('hdlr'): 
			if (!ReadMetaHdlrChunk(hti, nSize - 8))
				return false;
			break;
		case FCC('ilst'):
			if (!ReadMetaIlstChunk(hti, nSize - 8))
				return false;
			break;
		case FCC('keys'): // TODO 
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("Keys Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp)) // TODO
				return false;
			break;
		case FCC('free'): // TODO 
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("Free Atom"), fcc, nSize);
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp)) // TODO
				return false;
			break;
		default:
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("??? Atom"), fcc, nSize);
			OSNodeCreateAlert(htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!"));
			TRACE3(_T("%s (%s): Unexpected FCC %s.\n"), _T(__FUNCTION__), m_sIFilename.c_str(), FCCToString(fcc).c_str());
			if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
				return false;
			break;
		}
	}
	if (nParentSize > 0) {
		hti = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nParentSize, hti))
			return false;
	}
	return true;
}
bool CMOVParser::ReadMetaHdlrChunk(int hti, DWORD nChunkSize)
{
	static constexpr DDEF ddef = {
		8, "Meta Handler", "'hdlr'", 25, 0,
		DDEF::DESC::DT_BYTE, 0, 3, "Version (0)", nullptr, nullptr,
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags (0)", nullptr, nullptr,
		DDEF::DESC::DT_FCC, 0, 3, "Predefined/QT type (0)", nullptr, nullptr,
		DDEF::DESC::DT_FCC, 0, 3, "Type", nullptr, nullptr, // 'mdta' 
		DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "Reserved1/QT manufacturer (0)", nullptr, nullptr,
		DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "Reserved2 (0)", nullptr, nullptr,
		DDEF::DESC::DT_FCC, DDEF::DESC::DTL_HEX, 3, "Reserved3 (0)", nullptr, nullptr,
		DDEF::DESC::DT_UTF8STRING_LENGTHINBYTES, 0, 3, "Name", nullptr, nullptr,
	};
#if 0
#endif
	return (Dump(hti, 8, nChunkSize + 8, &ddef) == nChunkSize);
}
bool CMOVParser::ReadMetaIlstChunk(int hti, DWORD nParentSize)
{
	FOURCC fcc;
	DWORD nSize;
	int htitmp;

	while (nParentSize >= 8) {
		ASSERT(GetReadPos() == GetReadTotalBytes());
		if (GetReadTotalBytes() >= GetReadFileSize())
			return false;
		if (!ReadSizeType(nSize, fcc, hti))
			return false;
		if (nSize > nParentSize) {
			OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!"));
			if (!Settings.bForceChunkSize)
				nSize = nParentSize;
			else
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		switch (fcc) {
		default:
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("Item Atom"), fcc, nSize);
			if (!ReadSizeType(nSize, fcc, hti))
				return false;
			if (fcc != FCC('data') || nSize > 0x7000) { // TODO nSize Problem der Scanner unten hat dann eine Len < 0 und bricht ab...
				if (nSize > 8 && !ReadSeekFwdDump(nSize - 8, htitmp))
					return false;
				continue;
			}
			/*static*/ const DDEF ddef = {
				5, "Data", "'data'", 9, 0,
				DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "Type type (0)", nullptr, nullptr,
				DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Type type (0)", nullptr, nullptr,
				DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "Locale country", nullptr, nullptr,
				DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "Locale language", nullptr, nullptr,
				DDEF::DESC::DT_BYTES, nSize - 8, 3, "Content", nullptr, nullptr, // TODO Achtung Problem siehe oben wenn > 0x8000
			};
			if(Dump(htitmp, 8, nSize, &ddef) != nSize)
				return false;
		}
	}
	if (nParentSize > 0) {
		hti = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nParentSize, hti))
			return false;
	}
	return true;



}

bool CMOVParser::ReadGminChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25663 
	static constexpr DDEFV ddefv2 = {
		9, DDEFV::VDT_INFIX,
		0x0000, "Copy", DDEFV::DESC::VDTV_VALUE,
		0x0040, "Dither Copy", DDEFV::DESC::VDTV_VALUE,
		0x0020, "Blend (Opcolor)", DDEFV::DESC::VDTV_VALUE,
		0x0024, "Transparent (Opcolor)", DDEFV::DESC::VDTV_VALUE,
		0x0100, "Straight Alpha", DDEFV::DESC::VDTV_VALUE,
		0x0101, "Premul White Alpha", DDEFV::DESC::VDTV_VALUE,
		0x0102, "Premul Black Alpha", DDEFV::DESC::VDTV_VALUE,
		0x0104, "Straight Alpha Blend (Opcolor)", DDEFV::DESC::VDTV_VALUE,
		0x0103, "Composition (Dither Copy)", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEF ddef = {
		8, "Base Media Info", "'gmin'", 16, 16,
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // == 0
 		DDEF::DESC::DT_WORD, 0, 3, "Graphics mode", &ddefv2, nullptr, 
 		DDEF::DESC::DT_WORD, 0, 3, "OpcolorRed", nullptr, nullptr, 
 		DDEF::DESC::DT_WORD, 0, 3, "OpcolorGreen", nullptr, nullptr, 
 		DDEF::DESC::DT_WORD, 0, 3, "OpcolorBlue", nullptr, nullptr, 
 		DDEF::DESC::DT_APPLEFIXEDFLOAT16, 0, 3, "Balance", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, DDEF::DESC::DTL_HEX, 3, "Reserved", nullptr, nullptr, // == 0
	};
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
}

bool CMOVParser::ReadDinfChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25626
	static constexpr DDEFV ddefv1 = {
		1, DDEFV::VDT_POSTFIX,
		0x000001, "Self Reference", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		11, "Data Information", "'dinf'", 16, 0,
		DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr, 
		DDEF::DESC::DT_FCC, 0, 3, "Type", nullptr, nullptr, // 'dref' 
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // == 0
 		DDEF::DESC::DT_DWORD, 0, 3, "NumberOfEntries", nullptr, nullptr, 
 		DDEF::DESC::DT_NODELOOP, -1, 3, "DataReferences", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "Size", nullptr, nullptr, 
		DDEF::DESC::DT_FCC, 0, 3, "Type", nullptr, nullptr, // 'ali', 'rsrc', 'url ' 
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", &ddefv1, nullptr, 
		DDEF::DESC::DT_BYTES_ADJUSTSIZE, -4, 3, "Data", nullptr, nullptr, 
	};
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
#if 0
            * 8+ bytes data (locator) information box
                = long unsigned offset + long ASCII text string 'dinf'

               * 8+ bytes data reference box
                   = long unsigned offset + long ASCII text string 'dref'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> 4 bytes number of references = long unsigned total
                     (minimum = 1)

                  * 8+ bytes reference type box
                      = long unsigned offset + long ASCII text string
                   - box types are url c string = 'url ' ; urn c strings = 'urn '
                   - QUICKTIME atom types are file alias = 'alis' ; resource alias = 'rsrc'
                    -> 4 bytes version/flags
                        = byte hex version (current = 0) + 24-bit hex flags
                      - some flags are external data = 0x000000 ; internal data = 0x000001

                    -> url c string = ASCII text string points to external data
                    -> 1 byte url c string end = byte value set to zero
                    OR
                    -> urn c string = ASCII text string points to external data
                    -> 1 byte urn c string end = byte value set to zero
                    -> url c string = ASCII text string points to external data
                    -> 1 byte url c string end = byte value set to zero
                    OR
                    -> QUICKTIME mac os file alias record structure
                        points to external data
                    OR
                    -> QUICKTIME mac os file alias record structure
                        plus resource info points to external data
              OR
               * 8+ bytes Data URL box
                   = long unsigned offset + long ASCII text string 'url '
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> url c string = ASCII text string points to external data
                 -> 1 byte url c string end = byte value set to zero
              OR
               * 8+ bytes Data URN box = long unsigned offset + long ASCII text string 'urn '
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)
                 -> urn c string = ASCII text string points to external data
                 -> 1 byte urn c string end = byte value set to zero
                 -> url c string = ASCII text string points to external data
                 -> 1 byte url c string end = byte value set to zero
#endif
}


bool CMOVParser::ReadIodsChunk( int hti, DWORD nChunkSize )
{ 
	hti = OSNodeCreate( hti, GetReadPos() - 8, _T("MP4 Initial Object Descriptor"), FCC('iods'), nChunkSize );
	BYTE b;
	TRIPLE flags;
	WORD w;
	DWORD d;
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("Version: ") + ToString( b ) ); // == 0x00
	ASSERT( b == 0x00 );
	if ( !ReadBytes( &flags, 3, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("Flags: 0x") + ToHexString( flags[0] ) + ToHexString( flags[1] ) + ToHexString( flags[2] ) ); // == 0x000000
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("IOD Type: 0x") + ToHexString( b ) ); // == 0x10
	ASSERT( b == 0x10 );
r1:	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	if ( b >= 0x80 ) {
		if ( b == 0x80 )
			OSNodeCreateValue( hti, _T("extended descriptor type: 0x") + ToHexString( b ) + _T(" (Start)") ); 
		else if ( b == 0xfe )
			OSNodeCreateValue( hti, _T("extended descriptor type: 0x") + ToHexString( b ) + _T(" (End)") ); 
		else
			OSNodeCreateValue( hti, _T("extended descriptor type: 0x") + ToHexString( b ) + _T(" (Unknown)") ); 
		goto r1; // max 3 mal
	}
	BYTE len = b;
	OSNodeCreateValue( hti, _T("DescriptorTypeLength: ") + ToString( len ) );
	ASSERT( len >= 7 );
	if ( !ReadBytes( &w, 2, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("OD ID: ") + ToString( FlipBytes( &w ) ) );
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("ODProfileLevel: ") + ToString( b ) );
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("SceneProfileLevel: ") + ToString( b ) );
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("AudioProfileLevel: ") + ToString( b ) );
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("VideoProfileLevel: ") + ToString( b ) );
	if ( !ReadBytes( &b, 1, hti ) )
		return false;
	OSNodeCreateValue( hti, _T("GraphicsProfileLevel: ") + ToString( b ) );
	len -= 7;
	while ( len >= 2 ) {
		if ( !ReadBytes( &b, 1, hti ) )
			return false;
		len--;
		OSNodeCreateValue( hti, _T("ES ID IncludedDescriptorTypeTag: 0x") + ToHexString( b ) ); // == 0x0e
		ASSERT( b == 0x0e );
r2:		if ( !ReadBytes( &b, 1, hti ) )
			return false;
		len--;
		if ( b >= 0x80 ) {
			if ( b == 0x80 )
				OSNodeCreateValue( hti, _T("extended descriptor type: 0x") + ToHexString( b ) + _T(" (Start)") ); 
			else if ( b == 0xfe )
				OSNodeCreateValue( hti, _T("extended descriptor type: 0x") + ToHexString( b ) + _T(" (End)") ); 
			else
				OSNodeCreateValue( hti, _T("extended descriptor type: 0x") + ToHexString( b ) + _T(" (Unknown)") ); 
			goto r2; // max 3 mal
		}
		BYTE len2 = b;
		OSNodeCreateValue( hti, _T("DescriptorTypeLength: ") + ToString( len2 ) );
		ASSERT( len2 == 4 );
		if ( len2 == 4 ) {
			if ( !ReadBytes( &d, 4, hti ) )
				return false;
			len -= 4;
			OSNodeCreateValue( hti, _T("TrackID: 0x") + ToHexString( FlipBytes( &d ) ) );
		} else {
			int htitmp = OSNodeCreateValue( hti, _T("TrackID: ") );
			if ( !ReadSeekFwdDump( len2, htitmp ) )
				return false;
			len -= len2;
		}
	}
	ASSERT( len == 0 );
	if ( len > 0 && !ReadSeekFwdDump( len, hti ) )
		return false;
	return true;
#if 0
//	return ReadSeekFwdDump( nChunkSize, hti );
	
	// TODO das hier stimmt noch nicht, würd ich sagen
	// http://www.geocities.com/xhelmboyx/quicktime/formats/mp4-layout.txt
	// http://svn.opentom.org/cgi-bin/viewcvs.cgi/trunk/21c3-video/cutting_tagging/tools/mpeg4ip-1.2/lib/mp4/iods.c?rev=664
	static constexpr DDEFV ddefv1 = {
		1, DDEFV::VDT_POSTFIX,
		0x000001, "Self Reference", DDEFV::DESC::VDTV_BITFIELD,
	};
	static constexpr DDEF ddef = {
		15, "MP4 Initial Object Descriptor", "'iods'", 18, 0,
		DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr, // == 0

		DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "IOD TypeFlag", nullptr, nullptr, // == 0x10
//		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "ExtendedDescriptorTypeTag", nullptr, nullptr, // == 0x80 = Start, 0xFE = End, the extended start tags may be left out
		DDEF::DESC::DT_BYTE, 0, 3, "DescriptorTypeLength", nullptr, nullptr, 
//		DDEF::DESC::DT_LOOP, -1, 2, "DescriptorTypes", nullptr, nullptr, 
		DDEF::DESC::DT_WORD, 0, 3, "OD ID", nullptr, nullptr, 
		DDEF::DESC::DT_BYTE, 0, 3, "ODProfileLevel", nullptr, nullptr, // 0xff means unused 
		DDEF::DESC::DT_BYTE, 0, 3, "SceneProfileLevel", nullptr, nullptr, // 0xff means unused 
		DDEF::DESC::DT_BYTE, 0, 3, "AudioProfileLevel", nullptr, nullptr, // 0xff means unused 
		DDEF::DESC::DT_BYTE, 0, 3, "VideoProfileLevel", nullptr, nullptr, // 0xff means unused 
		DDEF::DESC::DT_BYTE, 0, 3, "GraphicsProfileLevel", nullptr, nullptr, // 0xff means unused 
//		DDEF::DESC::DT_LOOPEND, 0, 2, "", nullptr, nullptr, 

		DDEF::DESC::DT_LOOP, 0, 2, "Tracks", nullptr, nullptr, 
		DDEF::DESC::DT_BYTE, DDEF::DESC::DTL_HEX, 3, "ES ID IncludedDescriptorTypeTag", nullptr, nullptr, // == 0x0e
//		DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "ExtendedDescriptorTypeTag", nullptr, nullptr, // == 0x80 = Start, 0xFE = End, the extended start tags may be left out
		DDEF::DESC::DT_BYTE, 0, 3, "DescriptorTypeLength", nullptr, nullptr, 
		DDEF::DESC::DT_DWORD, 0, 3, "TrackID", nullptr, nullptr, // refers to non-data system tracks
		DDEF::DESC::DT_LOOPEND, 0, 2, "", nullptr, nullptr, 
	};
	return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
#endif
}

bool CMOVParser::ReadHdlrChunk( int hti, DWORD nChunkSize )
{ // http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_4.html#//apple_ref/doc/uid/TP40000939-CH204-25621
	static constexpr DDEFV ddefv1 = {
		2, DDEFV::VDT_INFIX,
		FCC('mhlr'), "Media Handler", DDEFV::DESC::VDTV_VALUE,
		FCC('dhlr'), "Data Handler", DDEFV::DESC::VDTV_VALUE,
	};
	static constexpr DDEFV ddefv2 = {
		11, DDEFV::VDT_INFIX,
		FCC('vide'), "Video", DDEFV::DESC::VDTV_VALUE,
		FCC('soun'), "Audio", DDEFV::DESC::VDTV_VALUE,
		FCC('alis'), "Alias", DDEFV::DESC::VDTV_VALUE,
		FCC('odsm'), "Object Descriptor", DDEFV::DESC::VDTV_VALUE,
		FCC('crsm'), "Clock Reference", DDEFV::DESC::VDTV_VALUE,
		FCC('sdsm'), "Scene Description", DDEFV::DESC::VDTV_VALUE,
		FCC('m7sm'), "MPEG-7 Stream", DDEFV::DESC::VDTV_VALUE,
		FCC('ocsm'), "Object Content Info", DDEFV::DESC::VDTV_VALUE,
		FCC('ipsm'), "IPMP", DDEFV::DESC::VDTV_VALUE,
		FCC('mjsm'), "MPEG-J", DDEFV::DESC::VDTV_VALUE,
		FCC('tmcd'), "Timecode Media Handler", DDEFV::DESC::VDTV_VALUE,
  	};
	if ( m_bMP4 ) {
		static constexpr DDEF ddef = {
			8, "Handler Reference", "'hdlr'", 25, 0,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr,  // == 0
			DDEF::DESC::DT_FCC, 0, 3, "ComponentType", &ddefv1, nullptr, 
			DDEF::DESC::DT_FCC, 0, 3, "ComponentSubtype", &ddefv2, nullptr, 
			DDEF::DESC::DT_FCC, 0, 3, "ComponentManufacturer", nullptr, nullptr, // == 0, FCC('appl')
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "ComponentFlags", nullptr, nullptr, // == 0
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "ComponentFlagsMask", nullptr, nullptr, // == 0
			DDEF::DESC::DT_MBCSSTRING, 0, 3, "ComponentName", nullptr, nullptr, 
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	} else {
		static constexpr DDEF ddef = {
			9, "Handler Reference", "'hdlr'", 25, 0,
			DDEF::DESC::DT_BYTE, 0, 3, "Version", nullptr, nullptr, 
			DDEF::DESC::DT_TRIPLE, DDEF::DESC::DTL_HEX, 3, "Flags", nullptr, nullptr,  // == 0
			DDEF::DESC::DT_FCC, 0, 3, "ComponentType", &ddefv1, nullptr, 
			DDEF::DESC::DT_FCC, 0, 3, "ComponentSubtype", &ddefv2, nullptr, 
			DDEF::DESC::DT_FCC, 0, 3, "ComponentManufacturer", nullptr, nullptr, // == 0, FCC('appl')
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "ComponentFlags", nullptr, nullptr, // == 0
			DDEF::DESC::DT_DWORD, DDEF::DESC::DTL_HEX, 3, "ComponentFlagsMask", nullptr, nullptr, // == 0
			DDEF::DESC::DT_BYTE, 0, 4, "ComponentNameLength", nullptr, nullptr, 
			DDEF::DESC::DT_MBCSSTRING, -1, 3, "ComponentName", nullptr, nullptr, 
		};
		return (Dump( hti, 8, nChunkSize + 8, &ddef ) == nChunkSize);
	}
#if 0
            * 8+ bytes QUICKTIME handler reference atom
                = long unsigned offset + long ASCII text string 'hdlr'
             -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)
             -> 8 bytes type/subtype = long ASCII text string + long ASCII text string
                 (eg. Alias Data Handler = 'dhlralis' ; URL Data Handler = 'dhlrurl ')
             -> 4 bytes manufacturer reserved = long ASCII text string
                 (eg. Apple = 'appl' or 0)
             -> 4 bytes component reserved flags = long hex flags (none = 0)
             -> 4 bytes component reserved flags mask = long hex mask (none = 0)
             -> 1 byte component name string length = byte unsigned length
                 (no name = zero length string)
             -> component type name ASCII string (eg. "Data Handler")
#endif
}

bool CMOVParser::ReadUdtaChunk( int hti, DWORD nParentSize ) 
{
FOURCC fcc;
DWORD nSize;
int htitmp;

	while ( nParentSize >= 8 ) {
		ASSERT(GetReadPos() == GetReadTotalBytes() );
		if ( GetReadTotalBytes() >= GetReadFileSize() )
			return false;
		if ( !ReadSizeType( nSize, fcc, hti ) )
			return false;
		if ( nSize > nParentSize ) {
			OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Token Size greater Chunk Size!") );
			if ( !Settings.bForceChunkSize )
				nSize = nParentSize;
			else 
				nParentSize = nSize;
		}
		nParentSize -= nSize;
		if ( (fcc & 0x000000ff) == 0xa9 ) { // TODO wird nicht angezeigt ©
			static constexpr DDEF ddef = {
				4, nullptr, nullptr, 4, 0,
				DDEF::DESC::DT_LOOP, 0, 3, "Texts", nullptr, nullptr, // Anzahl der Loops implizit, funktioniert das?
				DDEF::DESC::DT_WORD, 0, 3, "Length", nullptr, nullptr, 
				DDEF::DESC::DT_WORD, 0, 3, "Language", nullptr, nullptr, // TODO
				DDEF::DESC::DT_MBCSSTRING, -2, 3, "Text", nullptr, nullptr, 
			};
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Text International"), fcc, nSize );
			if ( Dump( htitmp, 8, nSize, &ddef ) != nSize - 8 )
				return false;
		} else switch ( fcc ) {
#if 0
'name'
 Name of object.
 
'ptv '
 Print to video. Display movie in full screen mode. This atom contains a 16-byte structure, described in “Print to Video (Full Screen Mode)”.
 
'hnti'
 Hint info atom. Data used for real-time streaming of a movie or a track. For more information, see “Movie Hint Info Atom” and “Hint Track User Data Atom”.
 
'hinf'
 Hint track information. Statistical data for real-time streaming of a particular track. For more information, see “Hint Track User Data Atom”.

#endif
		case FCC('WLOC'): {
			static constexpr DDEF ddef = {
				2, "Window Location", "'WLOC'", 4, 4,
				DDEF::DESC::DT_WORD, 0, 3, "PosX", nullptr, nullptr, 
				DDEF::DESC::DT_WORD, 0, 3, "PosY", nullptr, nullptr,
			};
			if ( Dump( hti, 8, nSize, &ddef ) != nSize - 8 )
				return false;
			break; }
		case FCC('LOOP'): {
			static constexpr DDEF ddef = {
				1, "Loop", "'LOOP'", 4, 4, 
				DDEF::DESC::DT_DWORD, 0, 3, "Indicator", nullptr, nullptr, // TODO Values are 0 for normal looping, 1 for palindromic looping
			};
			if ( Dump( hti, 8, nSize, &ddef ) != nSize - 8 )
				return false;
			break; }
		case FCC('SelO'): {
			static constexpr DDEF ddef = {
				1, "Selection Only", "'SelO'", 1, 1, 
				DDEF::DESC::DT_BYTE, 0, 3, "Indicator", nullptr, nullptr, // TODO Byte indicating that only the selected area of the movie should be played.
			};
			if ( Dump( hti, 8, nSize, &ddef ) != nSize - 8 )
				return false;
			break; }
		case FCC('AllF'): {
			static constexpr DDEF ddef = {
				1, "All Frames", "'AllF'", 1, 1, 
				DDEF::DESC::DT_BYTE, 0, 3, "Indicator", nullptr, nullptr, // TODO Byte indicating that all frames of video should be played, regardless of timing.
			};
			if ( Dump( hti, 8, nSize, &ddef ) != nSize - 8 )
				return false;
			break; }
		case FCC('meta'): 
			if (!ReadMetaChunk(hti, nSize - 8))
				return false;
			break;
		case FCC('Xtra'): // Microsoft mp4 Tags
			htitmp = OSNodeCreate(hti, GetReadPos() - 8, _T("MP4 Microsoft Extra Tag Data"), fcc, (DWORD)nSize);
			if (nSize > 8 && !ReadSeekFwdDump((DWORD)nSize - 8, htitmp))
				return false;
			break; 
		case FCC('ptv '): // TODO "Print to Video" == Fullscreen
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Print To Video"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
#if 0
Display size 
A 16-bit little-endian (!!!) integer indicating the display size for the movie: 0 indicates that the movie should be played at its normal size; 1 indicates that the movie should be played at double size; 2 indicates that the movie should be played at half size; 3 indicates that the movie should be scaled to fill the screen; 4 indicates that the movie should be played at its current size (this last value is normally used when the print to video atom is inserted transiently and the movie has been temporarily resized).

Reserved1 
A 16-bit integer whose value should be 0.

Reserved2 
A 16-bit integer whose value should be 0.

Slide show 
An 8-bit boolean whose value is 1 for a slide show. In slide show mode, the movie advances one frame each time the right-arrow key is pressed. Audio is muted.

Play on open 
An 8-bit boolean whose value is normally 1, indicating that the movie should play when opened. Since there is no visible controller in full-screen mode, applications should always set this field to 1 to prevent user confusion.

#endif
		case FCC('uuid'): // TODO danach folgt eine 32 byte UUID
			ASSERT( false );
			// fall through...
		case FCC('name'):
		case FCC('hnti'):
		case FCC('hinf'):
			// weiters kommt vor: 'ndrm', 'VERS' + 4 Bytes, ...
		default:
			htitmp = OSNodeCreate( hti, GetReadPos() - 8, _T("Atom"), fcc, nSize );
			if ( nSize > 8 && !ReadSeekFwdDump( nSize - 8, htitmp ) )
				return false;
			break;
		}
	}
	if ( nParentSize == 4 ) { // The Udta Chunk has an optional 4 bytes with 0 
		if ( !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
		nParentSize -= 4;
	}
	if ( nParentSize > 0 ) {
		hti = OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!") );
		if ( !Settings.bForceChunkSize && !ReadSeekFwdDump( nParentSize, hti ) )
			return false;
	}
	return true;
#if 0
      * 8+ bytes optional user data (any custom info) atom
          = long unsigned offset + long ASCII text string 'udta'
         (copyright and MPEG-7 meta data related to element tracks)

         * 8+ bytes optional copyright notice box
             = long unsigned offset + long ASCII text string 'cprt'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> 1/8 byte ISO language padding = 1-bit value set to 0
           -> 1 7/8 bytes content language = 3 * 5-bits ISO 639-2 language code less 0x60
             - example code for english = 0x15C7
           -> annotation string = ASCII text string
           -> 1 byte annotation c string end = byte value set to zero

         * 8+ bytes optional ISO/IEC 14496-12 element meta data box
             = long unsigned offset + long ASCII text string 'meta'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)

            * 8+ bytes ISO/IEC 14496-12 handler reference box
                = long unsigned offset + long ASCII text string 'hdlr'
             - this box must be toward the start of the meta box
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)
              -> 4 bytes QUICKTIME type = long ASCII text string
                  (eg. Media Handler = 'mhlr')
              -> 4 bytes subtype/meta data type = long ASCII text string
                - types are MPEG-7 XML = 'mp7t' ; MPEG-7 binary XML = 'mp7b'
                - type is APPLE meta data iTunes reader = 'mdir'
              -> 4 bytes QUICKTIME manufacturer reserved = long ASCII text string
                  (eg. Apple = 'appl' or 0)
              -> 4 bytes QUICKTIME component reserved flags = long hex flags (none = 0)
              -> 4 bytes QUICKTIME component reserved flags mask = long hex mask (none = 0)
              -> component type name ASCII string
                  (eg. "Meta Data Handler" - no name = zero length string)
              -> 1 byte component name string end = byte padding set to zero
                - note: the quicktime spec uses a Pascal string
                        instead of the above C string

            * 8+ bytes optional ISO/IEC 14496-12 MPEG-7 XML box
                = long unsigned offset + long ASCII text string 'xml '
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                  (current = 0)
              -> MPEG-7 XML meta data = text dump

            * 8+ bytes optional ISO/IEC 14496-12 MPEG-7 binary XML box
                = long unsigned offset + long ASCII text string 'bxml'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                  (current = 0)
              -> MPEG-7 encoded XML meta data = hex dump

            * 8+ bytes optional ISO/IEC 14496-12 item location box
                = long unsigned offset + long ASCII text string 'iloc'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                  (current = 0)
              -> 1 nibble size of access offsets = 4 bits one byte multiples
                - 8-bit offset = 0 ; 32-bit offset = 4 ; 64-bit offset = 8  
              -> 1 nibble size of data lengths = 4 bits one byte multiples
                - 8-bit offset = 0 ; 32-bit offset = 4 ; 64-bit offset = 8  
              -> 1 nibble size of starting offset = 4 bits one byte multiples
                - 8-bit offset = 0 ; 32-bit offset = 4 ; 64-bit offset = 8  
              -> 1 nibble reserved = 4 bits set to zero
              -> 2 bytes number of locations = short unsigned index total
              -> 2+ bytes item reference = short unsigned id
              -> 2+ bytes stream data reference = short unsigned index from 'dref' box
                - if meta data item in same file set to zero  
              -> 1-8+ bytes starting offset =  byte - dlong unsigned offset
              -> 2+ bytes number of access points = short unsigned index total
              -> 1-8+ bytes access offset =  byte - dlong unsigned relative offset
                  (relative to starting offset)
              -> 1-8+ bytes data length =  byte - dlong unsigned length

            * 8+ bytes optional ISO/IEC 14496-12 primary item box
                = long unsigned offset + long ASCII text string 'pitm'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                  (current = 0)
              -> 2 bytes main item reference = short unsigned id

            * 8+ bytes optional ISO/IEC 14496-12 item encryption box
                = long unsigned offset + long ASCII text string 'ipro'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                  (current = 0)
              -> 2 bytes number of encryption boxes = short unsigned index total

               * 8+ bytes ISO/IEC 14496-12 encryption scheme info box
                   = long unsigned offset + long ASCII text string 'sinf'
                - if meta data encrypted to ISO/IEC 14496-12 standards

                  * 8+ bytes ISO/IEC 14496-12 original format box
                      = long unsigned offset + long ASCII text string 'frma'
                    -> 4 bytes description format = long ASCII text string

                  * 8+ bytes optional ISO/IEC 14496-12 IPMP info box
                      = long unsigned offset + long ASCII text string 'imif'
                    -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                        (current = 0)
                    -> IPMP descriptors = hex dump from IPMP part of ES Descriptor box

                  * 8+ bytes optional ISO/IEC 14496-12 scheme type box
                      = long unsigned offset + long ASCII text string 'schm'
                    -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                        (current = 0 ; contains URI if flags = 0x000001)
                    -> 4 bytes encryption type = long ASCII text string
                         - types are 128-bit AES counter = 'ACM1' ; 128-bit AES FS = 'AFS1'
                         - types are nullptr algorithm = 'ENUL' ; 160-bit HMAC-SHA-1 = 'SHM2'
                         - types are RTCP = 'ANUL' ; private scheme = '    '
                    -> 2 bytes encryption version = short unsigned version
                    -> optional scheme URI string = UTF-8 text string
                      (eg. web site)
                    -> 1 byte optional scheme URI string end = byte padding set to zero

                  * 8+ bytes ISO/IEC 14496-12 scheme data box
                      = long unsigned offset + long ASCII text string 'schi'
                    -> encryption related key = hex dump

            * 8+ bytes optional ISO/IEC 14496-12 item information box
                = long unsigned offset + long ASCII text string 'pitm'
              -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                  (current = 0)
              -> 2 bytes main item reference = short unsigned id
              -> 2 bytes encryption box array value = short unsigned index
              -> item name or URL string = UTF-8 text string
              -> 1 byte name or URL c string end = byte value set to zero
              -> item mime type string = UTF-8 text string
              -> 1 byte mime type c string end = byte value set to zero
              -> optional item transfer encoding string = UTF-8 text string
              -> 1 byte optional transfer encoding c string end = byte value set to zero

			  
			  -----------------

			  
	* 8+ bytes optional user data (any custom info) box
       = long unsigned offset + long ASCII text string 'udta'

      * 8+ bytes optional copyright notice box
          = long unsigned offset + long ASCII text string 'cprt'
        -> 4 bytes version/flags = byte hex version + 24-bit hex flags
            (current = 0)
        -> 1/8 byte ISO language padding = 1-bit value set to 0
        -> 1 7/8 bytes content language = 3 * 5-bits ISO 639-2 language code less 0x60
          - example code for english = 0x15C7
        -> annotation string = UTF text string
        -> 1 byte annotation c string end = byte value set to zero

      * 8+ bytes optional 3GPP notice box
          = long unsigned offset + long ASCII text string
       - box types are title = 'titl'; author = 'auth'; description = 'dscp'
       - box types are performers = 'perf'; genre = 'gnre'
        -> 4 bytes version/flags = byte hex version + 24-bit hex flags
            (current = 0)
        -> 1/8 byte ISO language padding = 1-bit value set to 0
        -> 1 7/8 bytes content language = 3 * 5-bits ISO 639-2 language code less 0x60
          - example code for english = 0x15C7
        -> annotation string = UTF text string
        -> 1 byte annotation c string end = byte value set to zero

      * 8+ bytes optional ISO/IEC 14496-12 presentation meta data box
          = long unsigned offset + long ASCII text string 'meta'
        -> 4 bytes version/flags = byte hex version + 24-bit hex flags
            (current = 0)

         * 8+ bytes ISO/IEC 14496-12 handler reference box
             = long unsigned offset + long ASCII text string 'hdlr'
          - this box must be toward the start of the meta box
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags (current = 0)
           -> 4 bytes QUICKTIME type = long ASCII text string
               (eg. Media Handler = 'mhlr')
           -> 4 bytes subtype/meta data type = long ASCII text string
             - types are MPEG-7 XML = 'mp7t' ; MPEG-7 binary XML = 'mp7b'
             - type is APPLE meta data for iTunes reader = 'mdir'
           -> 4 bytes QUICKTIME manufacturer reserved = long ASCII text string
               (eg. Apple = 'appl' or 0)
           -> 4 bytes QUICKTIME component reserved flags = long hex flags (none = 0)
           -> 4 bytes QUICKTIME component reserved flags mask = long hex mask (none = 0)
           -> component type name ASCII string
               (eg. "Meta Data Handler" - no name = zero length string)
           -> 1 byte component name string end = byte padding set to zero
             - note: the quicktime spec uses a Pascal string
                     instead of the above C string

         * 8+ bytes optional ISO/IEC 14496-12 MPEG-7 XML box
             = long unsigned offset + long ASCII text string 'xml '
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> MPEG-7 XML meta data = text dump

         * 8+ bytes optional ISO/IEC 14496-12 MPEG-7 binary XML box
             = long unsigned offset + long ASCII text string 'bxml'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> MPEG-7 encoded XML meta data = hex dump

         * 8+ bytes optional ISO/IEC 14496-12 item location box
             = long unsigned offset + long ASCII text string 'iloc'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> 1 nibble size of access offsets = 4 bits one byte multiples
             - 8-bit offset = 0 ; 32-bit offset = 4 ; 64-bit offset = 8  
           -> 1 nibble size of data lengths = 4 bits one byte multiples
             - 8-bit offset = 0 ; 32-bit offset = 4 ; 64-bit offset = 8  
           -> 1 nibble size of starting offset = 4 bits one byte multiples
             - 8-bit offset = 0 ; 32-bit offset = 4 ; 64-bit offset = 8  
           -> 1 nibble reserved = 4 bits set to zero
           -> 2 bytes number of locations = short unsigned index total
           -> 2+ bytes item reference = short unsigned id
           -> 2+ bytes stream data reference = short unsigned index from 'dref' box
             - if meta data item in same file set to zero  
           -> 1-8+ bytes starting offset =  byte - dlong unsigned offset
           -> 2+ bytes number of access points = short unsigned index total
           -> 1-8+ bytes access offset =  byte - dlong unsigned relative offset
               (relative to starting offset)
           -> 1-8+ bytes data length =  byte - dlong unsigned length

         * 8+ bytes optional ISO/IEC 14496-12 primary item box
             = long unsigned offset + long ASCII text string 'pitm'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> 2 bytes main item reference = short unsigned id

         * 8+ bytes optional ISO/IEC 14496-12 item encryption box
             = long unsigned offset + long ASCII text string 'ipro'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> 2 bytes number of encryption boxes = short unsigned index total

            * 8+ bytes ISO/IEC 14496-12 encryption scheme info box
                = long unsigned offset + long ASCII text string 'sinf'
             - if meta data encrypted to ISO/IEC 14496-12 standards

               * 8+ bytes ISO/IEC 14496-12 original format box
                   = long unsigned offset + long ASCII text string 'frma'
                 -> 4 bytes description format = long ASCII text string

               * 8+ bytes optional ISO/IEC 14496-12 IPMP info box
                   = long unsigned offset + long ASCII text string 'imif'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0)
                 -> IPMP descriptors = hex dump from IPMP part of ES Descriptor box

               * 8+ bytes optional ISO/IEC 14496-12 scheme type box
                   = long unsigned offset + long ASCII text string 'schm'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current = 0 ; contains URI if flags = 0x000001)
                 -> 4 bytes encryption type = long ASCII text string
                      - types are 128-bit AES counter = 'ACM1' ; 128-bit AES FS = 'AFS1'
                      - types are nullptr algorithm = 'ENUL' ; 160-bit HMAC-SHA-1 = 'SHM2'
                      - types are RTCP = 'ANUL' ; private scheme = '    '
                 -> 2 bytes encryption version = short unsigned version
                 -> optional scheme URI string = UTF-8 text string
                   (eg. web site)
                 -> 1 byte optional scheme URI string end = byte padding set to zero

               * 8+ bytes ISO/IEC 14496-12 scheme data box
                   = long unsigned offset + long ASCII text string 'schi'
                 -> encryption related key = hex dump

         * 8+ bytes optional ISO/IEC 14496-12 item information box
             = long unsigned offset + long ASCII text string 'pitm'
           -> 4 bytes version/flags = byte hex version + 24-bit hex flags
               (current = 0)
           -> 2 bytes main item reference = short unsigned id
           -> 2 bytes encryption box array value = short unsigned index
           -> item name or URL string = UTF-8 text string
           -> 1 byte name or URL c string end = byte value set to zero
           -> item mime type string = UTF-8 text string
           -> 1 byte mime type c string end = byte value set to zero
           -> optional item transfer encoding string = UTF-8 text string
           -> 1 byte optional transfer encoding c string end = byte value set to zero

         * 8+ bytes optional APPLE item list box
             = long unsigned offset + long ASCII text string 'ilst'

            * 8+ bytes optional APPLE annotation box
                = long unsigned offset + 0xA9 + 24-bit ASCII text string
             - box types are full name = 'nam' ; comment = 'cmt' ; content created year = 'day'
             - box types are artist = 'ART'; track = 'trk'; album = 'alb'; composer = 'com'
             - box types are composer = 'wrt'; encoder = 'too'; album = 'alb'; composer = 'com'
            OR
                = long unsigned offset + 32-bit ASCII text string
             - box types are genre = 'gnre' ; CD set number = 'disk' ; track number = 'trkn'
             - box types are beats per minute = 'tmpo' ; compilation = 'cpil'
             - box types are cover art = 'covr' ; itunes specific info = '----'

               * 8+ bytes APPLE item data box
                   = long unsigned offset + long ASCII text string 'data'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current version = 0 ; contains text flag = 0x000001
                      contains data flag = 0x000000 ; for tmpo/cpil flag = 0x000015
                      contains image data flag = 0x00000D)
                 -> 4 bytes reserved = 32-bit value set to zero
                 -> annotation text or data values = text or hex dump
                     (NOTE: Genre is either text or a 16-bit short ID3 value and
                        most other non-text data are short unsigned values
                        with the exception of compilation which is a byte flag)

               * 8+ bytes optional APPLE additional info box
                   = long unsigned offset + long ASCII text string
                - box types are Java style app name = 'mean' ; item name = 'name'
                 -> 4 bytes version/flags = byte hex version + 24-bit hex flags
                     (current version = 0 ; current flags = 0x000000)
                 -> string text = ASCII text dump

     -> 4 bytes compatibility utda end = long value set to zero

#endif
}



/*static*/ bool CMOVParser::Probe( BYTE buffer[12] )
{
	FOURCC* pfcc = reinterpret_cast<DWORD*>( buffer ); 
#ifdef _DEBUG
	_tstring s = FCCToString( pfcc[1] );
#endif
	return ( pfcc[1] == FCC('ftyp') || pfcc[1] == FCC('mdat') || pfcc[1] == FCC('wide') || pfcc[1] == FCC('moov') || pfcc[1] == FCC('pnot') /*preview node, could by anything MACish*/);
}


bool CMOVParser::PeekVersion( BYTE& nVersion )
{
	return ReadBytes( &nVersion, sizeof( nVersion ), 0, false ) && ReadSeek( -(int)sizeof( nVersion ), false );
}



/*
E:\Diverses\Videos\xcxcxccx>c:\tools\AVIMaster\AVIMaster.exe "04C8EB4E-7AB1-4E3C-B745-B6237B12E6CE-2929-000002575638C494_1.1 .mp4"
AVIMaster 1.4.3 (c)2004-2006 by thozie productions, Thomas Ziegler, M³nchen
Contact and Bug Reports: www.thozie.de/dnn/AVIMaster.aspx, avimaster@thozie.de

E:\Diverses\Videos\xccxc\04C8EB4E-7AB1-4E3C-B745-B6237B12E6CE-2929-000002575638C494_1.1*.mp4
  Reading '04C8EB4E-7AB1-4E3C-B745-B6237B12E6CE-2929-000002575638C494_1.1.mp4' (928829)
    'ftyp' (28): File Type
      00 MajorBrand (www.mp4ra.org): 'mp42' (ISO 14496-1 vers. 2)
      04 MajorBrandVersion: 1
      CompatibleBrands Item #1
        00 CompatibleBrand: 'mp41' (ISO 14496-1 vers. 1)
      CompatibleBrands Item #2
        00 CompatibleBrand: 'mp42' (ISO 14496-1 vers. 2)
      CompatibleBrands Item #3
        00 CompatibleBrand: 'isom' (ISO 14496-1 Base Media)
    'wide' (8): Atom
    'mdat' (925060): Movie Data
      "..@.....=.l.2H..t..j....x.n=.\.'S8Jo.....S.p3....q..-"
    'moov' (3733): Movie
      'mvhd' (108): Movie Header
      'trak' (1727): Track
        'tkhd' (92): Track Header
        'edts' (36): Edit
        'mdia' (1591): Media
          'mdhd' (32): Media Header
          'hdlr' (49): Handler Reference
          'minf' (1502): Media Info
            'vmhd' (20): Video Media Information Header
            'dinf' (36): Data Information
            'stbl' (1438): Sample Table
              'stsd' (158): Sample Description
              'stts' (112): Sample Time To Sample Duration
              'stss' (44): Key Frames (Sync Samples)
              'sdtp' (204): Atom
              'stsc' (64): Sample To Chunk
              'stsz' (788): Sample Size
              'stco' (60): Sample Chunk Offset
                "..........A...Hy..>/..........Cz..V....f.......1...`"
      'trak' (1704): Track
        'tkhd' (92): Track Header
        'edts' (36): Edit
        'mdia' (1568): Media
          'mdhd' (32): Media Header
          'hdlr' (49): Handler Reference
          'minf' (1479): Media Info
            'smhd' (16): Sound Media Information Header
            'dinf' (36): Data Information
            'stbl' (1419): Sample Table
              'stsd' (103): Sample Description
              'stts' (24): Sample Time To Sample Duration
              'stsc' (64): Sample To Chunk
              'stsz' (1160): Sample Size
              'stco' (60): Sample Chunk Offset
                "...........,..8;..-n...V......3...E...s....!...z...."
      'udta' (186): User Data
        'meta' (81): Atom
          "........hdlr........mdir...............%ilst.....nam."
        'Xtra' (97): Atom
          "...Y....WM/Category..........X.x.x.:.X.x.x.x.x......("
  This is a Apple Quicktime MPEG-4 file.
  */