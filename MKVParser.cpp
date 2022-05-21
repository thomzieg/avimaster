#include "Helper.h"
#include "MKVParser.h"
//#include "AVIParser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif



/*static*/ const WORD CMKVParser::m_nMinHeaderSize = static_cast<WORD>(sizeof(DWORD) + sizeof(DWORD) + sizeof(WORD));


CMKVParser::CMKVParser(class CIOService& ioservice) : CParser( ioservice )
{
//	SetParams( false, true );
}

bool CMKVParser::ReadFile(int hti )
{ 
	if ( ReadChunk( hti, GetReadFileSize()) )
		m_sSummary = _T("This is a Matroska Media file."); 
	else
		m_sSummary = _T("This might not be a valid Matroska Media file.");
	return true; // all done
}

// TODO Positionen kontrollieren, ChunkSize Modus Parent gibt vor oder wie Subelemente...
// TODO Level prüfen

bool CMKVParser::ReadChunk(int hti, QWORD nParentSize)
{
int htitmp;
	// TODO der master-Header kann mehrfach in einem File vorkommen (wenn konkateniert wurde)
	while (nParentSize >= 6) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if ( !GetIDSize(hti, nID, nSize, nLength) )
			return false;
		ASSERT(nLength + nSize <= nParentSize);
		nParentSize -= nLength + nSize;
		switch (nID)
		{
		case 0xa3df451a: // Header
			ASSERT(hti == 2);
			ASSERT(nSize == 19 || nSize == 31 || nSize == 35);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EBMLHeader"), ToHexString(nID), nSize);
			if (!ReadEBMLHeader(htitmp, (WORD) nSize))
				return false;
			break;
		case 0x67805318: // Segment
			ASSERT(hti == 2);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EBMLSegment"), ToHexString(nID), nSize);
			if (!ReadEBMLSegment(htitmp, nSize))
				return false;
			break;
		default: // unknown
			ASSERT(hti == 2);
			hti = OSNodeCreate(hti, GetReadPos() - nLength, _T("EBML master header Chunk"), ToHexString(nID), nSize);
			OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!"));
			if (!ReadSeekFwdDump(nSize, hti))
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

bool CMKVParser::ReadEBMLHeader(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		QWORD n;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		switch (nID)
		{
		case 0x8642: // EBMLVersion, 1
			n = GetInt(hti, (WORD) nSize);
			ASSERT(nSize == 1 && n == 0x01);
			ASSERT(hti == 3);
			OSNodeCreate(hti, GetReadPos() - nLength, _T("EBMLVersion ") + ToString(n), ToHexString((WORD) nID), nSize);
			break;
		case 0xF742: // EBMLREADVERSION, 1
			n = GetInt(hti, (WORD) nSize);
			ASSERT(nSize == 1 && n == 0x01);
			ASSERT(hti == 3);
			OSNodeCreate(hti, GetReadPos() - nLength, _T("EBMLREADVERSION ") + ToString(n), ToHexString((WORD) nID), nSize);
			break;
		case 0xF242: // EBMLMAXIDLENGTH, 4
			n = GetInt(hti, (WORD) nSize);
			ASSERT(nSize == 1 && n == 0x04);
			ASSERT(hti == 3);
			OSNodeCreate(hti, GetReadPos() - nLength, _T("EBMLMAXIDLENGTH ") + ToString(n), ToHexString((WORD) nID), nSize);
			break;
		case 0xF342: // EBMLMAXSIZELENGTH, 8
			n = GetInt(hti, (WORD)nSize);
			ASSERT(nSize == 1 && n == 0x08);
			ASSERT(hti == 3);
			OSNodeCreate(hti, GetReadPos() - nLength, _T("EBMLMAXSIZELENGTH ") + ToString(n), ToHexString((WORD) nID), nSize);
			break;
		case 0x8242: { // DOCTYPE (matroska)
			ASSERT(nSize == 4 || nSize == 8);
			ASSERT(hti == 3);
			BYTE buff[12]; // TODO
			if (!ReadBytes(buff, (DWORD)nSize, hti))
				return false;
			OSNodeCreate(hti, GetReadPos() - nLength, _T("DOCTYPE ") + ToStringFromMBCS((const char*) buff, (size_t)nSize), ToHexString((WORD) nID), nSize); // TODO UTF-8?
			break; }
		case 0x8742: // DOCTYPEVERSION, 1
			n = GetInt(hti, (WORD) nSize);
			ASSERT(nSize == 1 && n <= 0x04);
			ASSERT(hti == 3);
			OSNodeCreate(hti, GetReadPos() - nLength, _T("DOCTYPEVERSION ") + ToString(n), ToHexString((WORD) nID), nSize);
			break;
		case 0x8542: // DOCTYPEREADVERSION, 1
			n = GetInt(hti, (WORD) nSize);
			ASSERT(nSize == 1 && n <= 0x04);
			ASSERT(hti == 3);
			OSNodeCreate(hti, GetReadPos() - nLength, _T("DOCTYPEREADVERSION ") + ToString(n), ToHexString((WORD) nID), nSize);
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}

bool CMKVParser::ReadEBMLSegment(int hti, QWORD nChunkSize)
{
	int htitmp;
	DWORD nCount = 0;

	// TODO der master-Header kann mehrfach in einem File vorkommen (wenn konkateniert wurde)
	while (nChunkSize >= 6) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		switch (nID) // Hier sind alle IDs 4-stellig
		{
		case 0x749b4d11: // SeekHead
			ASSERT(hti == 3);
			//ASSERT(nSize == 35);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SeekHead"), ToHexString(nID), nSize);
			if (!ReadEBMLSeek(htitmp, nSize))
				return false;
			break;
		case 0x66A94915: // Segment Info
			ASSERT(hti == 3);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Segment Info"), ToHexString(nID), nSize);
			if (!ReadEBMLSegmentInfo(htitmp, nSize))
				return false;
			break;
		case 0x75B6431F: // Cluster
			ASSERT(hti == 3);
			if (nCount > Settings.nTraceMoviEntries && nCount > 25 && nChunkSize >= 6) { // TODO das funktioniert hier nicht den letzten Cluster anzuzeigen, da danach ja noch andere Chunks kommen, man müsste entweder zurückspulen oder vorspulen und schauen was als nächstes kommt...
				if (!ReadSeekFwdDump(nSize))
					return false;
				nCount++;
				break;
			}
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Cluster #") + ToString(nCount++), ToHexString(nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0x6BAE5416: // Tracks
			ASSERT(hti == 3);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Tracks"), ToHexString(nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0x6Bbb531C: // Cues
			ASSERT(hti == 3);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Cues"), ToHexString(nID), nSize);
			if (!ReadEBMLCues(htitmp, nSize))
				return false;
			break;
		case 0x69a44119: // Attachments
			ASSERT(hti == 3);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Attachments"), ToHexString(nID), nSize);
			if (!ReadEBMLAttachments(htitmp, nSize))
				return false;
			break;
		case 0x70a74310: // Chapters
			ASSERT(hti == 3);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Chapters"), ToHexString(nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;
		case 0x67c35412: // Tags
			ASSERT(hti == 3);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Tags"), ToHexString(nID), nSize);
			if (!ReadEBMLTags(htitmp, nSize))
				return false;
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLSeek(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		switch (nID)
		{
		case 0xbb4D: // Seek (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Seek"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLSeek(htitmp, nSize))
				return false;
			break;

		case 0xAB53: // SeekID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SeekID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xAC53: // SeekPosition
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SeekPosition ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLSegmentInfo(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		DWORD nDSize = (DWORD)nSize;
		switch (nID)
		{
		case 0xa473: // SegmentUID
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SegmentUID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x8473: { // SegmentFilename
			ASSERT(hti == 4);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SegmentFilename: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize);
			delete[] buff;
			break; }
		case 0x23b93c: // PrevUID
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PrevUID"), ToHexString(nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xab833c: { // PrevFilename
			ASSERT(hti == 4);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PrevFilename: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // TODO UTF-8
			delete[] buff;
			break; }
		case 0x23b93e: // NextUID
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("NextUID"), ToHexString(nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xbb833e: { // NextFilename
			ASSERT(hti == 4);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("NextFilename: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // TODO UTF-8
			delete[] buff;
			break; }
		case 0x4444: // SegmentFamily
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SegmentFamily"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x2469: // ChapterTranslate (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTranslate"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLSegmentInfo(htitmp, nSize))
				return false;
			break;
		case 0xfc69: // ChapterTranslateEditionUID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTranslateEditionUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xbf69: // ChapterTranslateCodec
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTranslateCodec ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xa569: // ChapterTranslateID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTranslateID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xb1d72a: // TimecodeScale
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TimecodeScale ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString(nID), nSize); // in nanoseconds
			break;
		case 0x8944: // Duration
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Duration"), ToHexString((WORD) nID), nSize); // TODO float
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x6144: // DateUTC
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DateUTC"), ToHexString((WORD) nID), nSize); // TODO Date
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xa97b: { // Title
			ASSERT(hti == 4);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Title: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); 
			delete[] buff;
			break; }
		case 0x804d: { // MuxingApp
			ASSERT(hti == 4);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("MuxingApp: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); 
			delete[] buff;
			break; }
		case 0x4157: { // WritingApp
			ASSERT(hti == 4);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("WritingApp: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); 
			delete[] buff;
			break; }
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLTracks(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 3) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		DWORD nDSize = (DWORD)nSize;
		switch (nID)
		{
		case 0xae: // TrackEntry (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackEntry"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;

		case 0xe0: // Video (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Video"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0xe1: // Audio (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Audio"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0xe2: // TrackOperation (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackOperation"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0x806d: // ContentEncodings (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncodings"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0x4062: // ContentEncodings ContentEncoding (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncoding"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0x3450: // ContentEncodings ContentEncoding ContentCompression (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentCompression"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0x3550: // ContentEncodings ContentEncoding ContentEncryption (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncryption"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0x2466: // TrackTranslate (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackTranslate"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0xe3: // TrackOperation TrackCombinePlanes (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackCombinePlanes"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0xe4: // TrackOperation TrackCombinePlanes TrackPlane (Master)
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackPlane"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;
		case 0xe9: // TrackOperation TrackJoinBlocks (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackJoinBlocks"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLTracks(htitmp, nSize))
				return false;
			break;

		case 0xc0: // TrickTrackUID 
		case 0xc1: // TrickTrackSegmentUID 
		case 0xc4: // TrickMasterTrackSegmentUID 
		case 0xc6: // TrickTrackFlag 
		case 0xc7: // TrickMasterTrackUID 
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DivX Tricks"), ToHexString((BYTE) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xd7: // TrackNumber
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xc573: // TrackUID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x83: // TrackType
			ASSERT(hti == 5); // (1: video, 2: audio, 3: complex, 0x10: logo, 0x11: subtitle, 0x12: buttons, 0x20: control).
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackType ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xb9: // FlagEnabled
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FlagEnabled ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x88: // FlagDefault
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FlagDefault ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xaa50: // FlagForced
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FlagForced ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x9c: // FlagLacing
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FlagLacing ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xe76d: // MinCache
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("MinCache ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xf86d: // MaxCache
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("MaxCache ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x83e323: // DefaultDuration
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DefaultDuration ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString(nID), nSize); // TODO unsigned
			break;
		case 0x473123: // TrackTimecodeScale
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackTimecodeScale (DEPRECATED)"), ToHexString(nID), nSize); // TODO float
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x7f53: // TrackOffset
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackOffset ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); 
			break;
		case 0xee55: // MaxBlockAdditionID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("MaxBlockAdditionID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x6e53: { // Name
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Name: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); // TODO UTF-8
			delete[] buff;
			break; }
		case 0x9cb522: { // Language
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Language: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // string
			delete[] buff;
			break; }
		case 0x86: { // CodecID
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecID: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((BYTE)nID), nSize); // string
			delete[] buff;
			break; }
		case 0xa263: // CodecPrivate
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecPrivate"), ToHexString((WORD) nID), nSize); 
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x888625: { // CodecName
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecName: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // UTF-8
			delete[] buff;
			break; }
		case 0x4674: // AttachmentLink
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("AttachmentLink ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0x97963a: { // CodecSettings
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecSettings: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // UTF-8
			delete[] buff;
			break; }
		case 0x40403b: { // CodecInfoURL
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecInfoURL: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // string
			delete[] buff;
			break; }
		case 0x40b226: { // CodecDownloadURL
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecDownloadURL: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString(nID), nSize); // string
			delete[] buff;
			break; }
		case 0xaa: // CodecDecodeAll
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecDecodeAll ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xab6f: // TrackOverlay
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackOverlay ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xaa56: // CodecDelay
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecDelay ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xbb56: // SeekPreRoll
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SeekPreRoll ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xfc66: // TrackTranslateEditionUID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackTranslateEditionUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xbf66: // TrackTranslateCodec
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackTranslateCodec ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xa566: // TrackTranslateTrackID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackTranslateTrackID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x9a: // FlagInterlaced
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FlagInterlaced ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xb853: // StereoMode
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("StereoMode ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xc053: // AlphaMode
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("AlphaMode ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xb953: // OldStereoMode
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("OldStereoMode (DEPRECATED) ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xb0: // Video PixelWidth
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PixelWidth ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xba: // Video PixelHeight
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PixelHeight ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xaa54: // Video PixelCropBottom
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PixelCropBottom ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xbb54: // Video PixelCropTop
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PixelCropTop ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xcc54: // Video PixelCropLeft
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PixelCropLeft ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xdd54: // Video PixelCropRight
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PixelCropRight ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xb054: // Video DisplayWidth
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DisplayWidth ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xba54: // Video DisplayHeight
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DisplayHeight ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xb254: // Video DisplayUnit
			ASSERT(hti == 6); // (0: pixels, 1: centimeters, 2: inches, 3: Display Aspect Ratio).
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DisplayUnit ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0xb354: // Video AspectRatioType
			ASSERT(hti == 6); // (0: free resizing, 1: keep aspect ratio, 2: fixed).
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("AspectRatioType ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize);
			break;
		case 0x24b52e: // Video ColourSpace
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ColourSpace"), ToHexString(nID), nSize); 
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;		
		case 0x23b52f: // Video GammaValue, optional
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("GammaValue"), ToHexString(nID), nSize); // TODO float
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;		
		case 0xe38323: // Video FrameRate, optional
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FrameRate"), ToHexString(nID), nSize); // TODO float
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xb5: // Audio SamplingFrequency
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SamplingFrequency"), ToHexString((BYTE)nID), nSize); // TODO float
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xb578: // Audio OutputSamplingFrequency
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("OutputSamplingFrequency"), ToHexString((WORD)nID), nSize); // TODO float
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x9f: // Audio Channels
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Channels ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x7b7d: // Audio ChannelPositions
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChannelPositions"), ToHexString((WORD)nID), nSize); 
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x6462: // Audio BitDepth
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BitDepth ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xe5: // TrackOperation TrackCombinePlanes TrackPlane TrackPlaneUID
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackPlaneUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xe6: // TrackOperation TrackCombinePlanes TrackPlane TrackPlaneType
			ASSERT(hti == 8); // (0: left eye, 1: right eye, 2: background).
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackPlaneType ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xed: // TrackOperation TrackJoinBlocks TrackJoinUID
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TrackJoinUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x3150: // ContentEncodings ContentEncoding ContentEncodingOrder
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncodingOrder ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x3250: // ContentEncodings ContentEncoding ContentEncodingScope
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncodingScope ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x3350: // ContentEncodings ContentEncoding ContentEncodingType
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncodingType ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x5442: // ContentEncodings ContentEncoding ContentCompression ContentCompAlgo
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentCompAlgo ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x5542: // ContentEncodings ContentEncoding ContentCompression ContentCompSettings
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentCompSettings"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xe147: // ContentEncodings ContentEncoding ContentEncryption ContentEncAlgo
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncAlgo ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xe247: // ContentEncodings ContentEncoding ContentEncryption ContentEncKeyID
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentEncKeyID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xe347: // ContentEncodings ContentEncoding ContentEncryption ContentSignature
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentSignature"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xe447: // ContentEncodings ContentEncoding ContentEncryption ContentSigKeyID
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentSigKeyID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xe547: // ContentEncodings ContentEncoding ContentEncryption ContentSigAlgo
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentSigAlgo ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xe647: // ContentEncodings ContentEncoding ContentEncryption ContentSigHashAlgo
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ContentSigHashAlgo ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize)) // TODO chunk name mitgeben?
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLTags(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		DWORD nDSize = (DWORD)nSize;
		switch (nID)
		{
		case 0x7373: // Tag (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Tag"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTags(htitmp, nSize))
				return false;
			break;
		case 0xc063: // Targets (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Targets"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTags(htitmp, nSize))
				return false;
			break;
		case 0xc867: // SimpleTag (Master)
			ASSERT(hti >= 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SimpleTag"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLTags(htitmp, nSize))
				return false;
			break;

		case 0xca68: // TargetTypeValue
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TargetTypeValue ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xca63: { // TargetType
			ASSERT(hti == 6);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TargetType: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); // string
			delete[] buff;
			break; }
		case 0xc563: // TagTrackUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagTrackUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xc963: // TagEditionUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagEditionUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xc463: // TagChapterUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagChapterUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xc663: // TagAttachmentUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagAttachmentUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xA345: { // TagName
			ASSERT(hti == 6);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagName: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize);
			delete[] buff;
			break; }
		case 0x7a44: // TagLanguage
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagLanguage"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp)) // TOSO string
				return false;
			break;
		case 0x8444: // TagDefault
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagDefault ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x8744: { // TagString
			ASSERT(hti == 6);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagString: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize);
			delete[] buff;
			break; }
		case 0x8544: { // TagBinary
			ASSERT(hti == 6);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TagBinary: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); // string
			delete[] buff;
			break; }
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLChapters(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		DWORD nDSize = (DWORD)nSize;
		switch (nID)
		{
		case 0xb945: // EditionEntry (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EditionEntry"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;
		case 0xb6: // ChapterAtom (Master)
			ASSERT(hti >= 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterAtom"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;
		case 0x8f: // ChapterTrack (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTrack"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;
		case 0x80: // ChapterDisplay (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterDisplay"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;
		case 0x4469: // ChapProcess (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapProcess"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;
		case 0x1169: // ChapProcessCommand (Master)
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapProcessCommand"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLChapters(htitmp, nSize))
				return false;
			break;

		case 0xbc45: // EditionUID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EditionUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xbd45: // EditionFlagHidden
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EditionFlagHidden ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xdb45: // EditionFlagDefault
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EditionFlagDefault ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xdd45: // EditionFlagOrdered
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EditionFlagOrdered ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;

		case 0xc473: // ChapterUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x5456: { // ChapterStringUID
			ASSERT(hti == 6);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterStringUID: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize);
			delete[] buff;
			break; }
		case 0x91: // ChapterTimeStart
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTimeStart ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x92: // ChapterTimeEnd
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTimeEnd ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x98: // ChapterFlagHidden
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterFlagHidden ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x9845: // ChapterFlagEnabled
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterFlagEnabled ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x676e: // ChapterSegmentUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterSegmentUID"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp)) 
				return false;
			break;
		case 0xbc6e: // ChapterSegmentEditionUID
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterSegmentEditionUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xc363: // ChapterPhysicalEquiv
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterPhysicalEquiv ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x89: // ChapterTrackNumber
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapterTrackNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0x85: { // ChapString
			ASSERT(hti == 7);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapString: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((BYTE)nID), nSize);
			delete[] buff;
			break; }
		case 0x7c43: { // ChapLanguage
			ASSERT(hti == 7);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapLanguage: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); // string
			delete[] buff;
			break; }
		case 0x7e43: { // ChapCountry
			ASSERT(hti == 6);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapCountry: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); // string
			delete[] buff;
			break; } 
		case 0x5569: // ChapProcessCodecID
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapProcessCodecID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x0d45: // ChapProcessPrivate
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapProcessPrivate"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp)) 
				return false;
			break;
		case 0x2269: // ChapProcessTime
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapProcessTime ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x3369: // ChapProcessData
			ASSERT(hti == 8);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ChapProcessData"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLAttachments(int hti, QWORD nChunkSize)
{
	int htitmp;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		DWORD nDSize = (DWORD)nSize;
		switch (nID)
		{
		case 0xa761: // AttachedFile (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("AttachedFile"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLAttachments(htitmp, nSize))
				return false;
			break;

		case 0x7e46: { // FileDescription
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileDescription: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize);
			delete[] buff;
			break; }
		case 0x6e46: { // FileName
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileName: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize);
			delete[] buff;
			break; }
		case 0x6046: { // FileMimeType
			ASSERT(hti == 5);
			BYTE* buff = new BYTE[nDSize];
			if (!ReadBytes(buff, nDSize, hti))
				return false;
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileMimeType: ") + ToStringFromUTF8((const char08*)buff, nDSize), ToHexString((WORD)nID), nSize); // string
			delete[] buff;
			break; }
		case 0x5c46: // FileData
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileData"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xAe46: // FileUID
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileUID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x7546: // FileReferral
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileReferral"), ToHexString((WORD) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x6146: // FileUsedStartTime
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileUsedStartTime ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0x6246: // FileUsedEndTime
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FileUsedEndTime ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLCues(int hti, QWORD nChunkSize)
{
	int htitmp;
	DWORD nCount = 0;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		switch (nID)
		{
		case 0xbb: // CuePoint (Master)
			ASSERT(hti == 4);
			if (nCount > Settings.nTraceMoviEntries && nCount > 25 && nChunkSize >= 4) {
				if (!ReadSeekFwdDump(nSize))
					return false;
				nCount++;
				break;
			}
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CuePoint #") + ToString(nCount++), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCues(htitmp, nSize))
				return false;
			break;
		case 0xb7: // CuePoint CueTrackPositions (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueTrackPositions"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCues(htitmp, nSize))
				return false;
			break;
		case 0xdb: // CuePoint CueTrackPositions CueReference (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueReference"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCues(htitmp, nSize))
				return false;
			break;

		case 0xb3: // CueTime
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueTime ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0xf7: // CueTrack
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueTrack ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0xf1: // CueClusterPosition
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueClusterPosition ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0xf0: // CueRelativePosition
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueRelativePosition ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0xb2: // CueDuration
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueDuration ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0x7853: // CueBlockNumber
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueBlockNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xea: // CueCodecState
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueCodecState ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0x96: // CueRefTime
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueRefTime ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0x97: // CueRefCluster
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueRefCluster ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0x5f53: // CueRefNumber
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueRefNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xeb: // CueRefCodecState
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CueRefCodecState ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}
bool CMKVParser::ReadEBMLCluster(int hti, QWORD nChunkSize)
{
	int htitmp;
	DWORD nCount = 0;
	while (nChunkSize >= 4) {
		DWORD nID;
		QWORD nSize;
		WORD nLength = 0;
		QWORD nBlock;
		if (!GetIDSize(hti, nID, nSize, nLength))
			return false;
		ASSERT(nLength + nSize <= nChunkSize);
		nChunkSize -= nLength + nSize;
		switch (nID)
		{
		case 0x5458: // SilentTracks (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SilentTracks"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0xa0: // BlockGroup (Master)
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockGroup"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0xa175: // BlockAdditions (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockAdditions"), ToHexString((WORD) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0xa6: // BlockMore (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockMore"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0x8e: // Slices (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Slices"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0xe8: // TimeSlice (Master)
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("TimeSlice"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;
		case 0xc8: // DivX ReferenceFrame (Master)
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DivX ReferenceFrame"), ToHexString((BYTE) nID), nSize);
			if (!ReadEBMLCluster(htitmp, nSize))
				return false;
			break;

		case 0xe7: // Timecode
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Timecode ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xd758: // SilentTrackNumber
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SilentTrackNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); // TODO unsigned
			break;
		case 0xa7: // Position
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Position ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xab: // PrevSize
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("PrevSize ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xA3: // SimpleBlock
			ASSERT(hti == 4);
			if (nCount > Settings.nTraceMoviEntries && nCount > 25 && nChunkSize >= 4) {
				if (!ReadSeekFwdDump(nSize))
					return false;
				nCount++;
				break;
			}
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SimpleBlock #") + ToString(nCount++), ToHexString((BYTE) nID), nSize);
			nBlock = std::min<QWORD>(nSize, 128);
			if (!ReadSeekFwdDump(nBlock, htitmp))
				return false;
			if (nBlock < nSize && !ReadSeek(nSize - nBlock))
				return false;
			break;
		case 0xA1: // Block
			ASSERT(hti == 5);
			if (nCount > Settings.nTraceMoviEntries && nCount > 25 && nChunkSize >= 4) {
				if (!ReadSeekFwdDump(nSize))
					return false;
				nCount++;
				break;
			}
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Block #") + ToString(nCount++), ToHexString((BYTE) nID), nSize);
			nBlock = std::min<QWORD>(nSize, 128);
			if (!ReadSeekFwdDump(nBlock, htitmp))
				return false;
			if (nBlock < nSize && !ReadSeek(nSize - nBlock))
				return false;
			break;
		case 0xA2: // BlockVirtual
			ASSERT(hti == 5);
			if (nCount > Settings.nTraceMoviEntries && nCount > 25 && nChunkSize >= 4) {
				if (!ReadSeekFwdDump(nSize))
					return false;
				nCount++;
				break;
			}
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockVirtual #") + ToString(nCount++), ToHexString((BYTE) nID), nSize);
			nBlock = std::min<QWORD>(nSize, 128);
			if (!ReadSeekFwdDump(nBlock, htitmp))
				return false;
			if (nBlock < nSize && !ReadSeek(nSize - nBlock) )
				return false;
			break;
		case 0xee: // BlockAddID
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockAddID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xA5: // BlockAdditional
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockAdditional"), ToHexString((BYTE) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0x9b: // BlockDuration
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockDuration ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xfa: // ReferencePriority
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ReferencePriority ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xfb: // ReferenceBlock
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ReferenceBlock ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); 
			break;
		case 0xfd: // ReferenceVirtual
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("ReferenceVirtual ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize);
			break;
		case 0xA4: // CodecState
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("CodecState"), ToHexString((BYTE) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		case 0xA275: // DiscardPadding
			ASSERT(hti == 5);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DiscardPadding ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((WORD) nID), nSize); 
			break;
		case 0xcc: // LaceNumber
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("LaceNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xcd: // FrameNumber
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("FrameNumber ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xcb: // BlockAdditionID
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("BlockAdditionID ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xce: // Delay
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("Delay ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xcf: // SliceDuration
			ASSERT(hti == 7);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("SliceDuration ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xc9: // ReferenceOffset
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DivX ReferenceOffset ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xca: // ReferenceTimeCode
			ASSERT(hti == 6);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("DivX ReferenceTimeCode ") + ToString(GetInt(hti, (WORD) nSize)), ToHexString((BYTE) nID), nSize); // TODO unsigned
			break;
		case 0xAf: // EncryptedBlock
			ASSERT(hti == 4);
			htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EncryptedBlock"), ToHexString((BYTE) nID), nSize);
			if (!ReadSeekFwdDump(nSize, htitmp))
				return false;
			break;
		default: // unknown
			if (!ReadEBMLCommon(hti, nID, nLength, nSize))
				return false;
			break;
		}
	}
	if (nChunkSize > 0) {
		htitmp = OSNodeCreateAlert(hti, CIOService::EPRIORITY::EP_ERROR, _T("Chunk Size mismatch!"));
		if (!Settings.bForceChunkSize && !ReadSeekFwdDump(nChunkSize, htitmp))
			return false;
	}
	return true;
}

bool CMKVParser::ReadEBMLCommon(int hti, DWORD nID, WORD nLength, QWORD nSize)
{
	int htitmp;

	// TODO der master-Header kann mehrfach in einem File vorkommen (wenn konkateniert wurde)
	switch (nID)
	{
	case 0xec: // Void
		OSNodeCreate(hti, GetReadPos() - nLength, _T("Void"), ToHexString((BYTE) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	case 0xBF: // CRC-32
		ASSERT(nSize == 4);
		OSNodeCreate(hti, GetReadPos() - nLength, _T("CRC-32"), ToHexString((BYTE) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	case 0x6786531b: // SignatureSlot
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignatureSlot"), ToHexString(nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	case 0x8a7e: // SignatureAlgo
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignatureAlgo"), ToHexString((WORD)nID), nSize);
		// Signature algorithm used(1 = RSA, 2 = elliptic)
		if (!ReadSeekFwdDump(nSize, hti)) // TODO weglassen
			return false;
		break;
	case 0x9a7e: // SignatureHash
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignatureHash"), ToHexString((WORD) nID), nSize);
		// Hash algorithm used (1=SHA1-160, 2=MD5)
		if (!ReadSeekFwdDump(nSize, hti)) // TODO weglassen
			return false;
		break;
	case 0xa57e: // SignaturePublicKey
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignaturePublicKey"), ToHexString((WORD) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti)) 
			return false;
		break;
	case 0xb57e: // Signature
		OSNodeCreate(hti, GetReadPos() - nLength, _T("Signature"), ToHexString((WORD) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	case 0x5b7e: // SignatureElements
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignatureElements"), ToHexString((WORD) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	case 0x7b7e: // SignatureElementList
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignatureElementList"), ToHexString((WORD) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	case 0x3265: // SignedElement
		OSNodeCreate(hti, GetReadPos() - nLength, _T("SignedElement"), ToHexString((WORD) nID), nSize);
		if (!ReadSeekFwdDump(nSize, hti))
			return false;
		break;
	default: // unknown
		htitmp = OSNodeCreate(hti, GetReadPos() - nLength, _T("EBML common Chunk"), ToHexString(nID), nSize);
		OSNodeCreateAlert(htitmp, CIOService::EPRIORITY::EP_ERROR, _T("Unexpected Token!"));
		if (!ReadSeekFwdDump(nSize, htitmp))
			return false;
		break;
	}
	return true;
}


bool CMKVParser::GetSize(int hti, QWORD& nSize, WORD& nLength)
{
	BYTE n = 0;
	BYTE buff[sizeof(nSize)];
	*(QWORD*) buff = 0;
	if ( !ReadBytes(buff, 1, hti) )
		return false;
	if (buff[0] & 0x80) {
		n = 1;
		buff[0] &= 0x7f;
	} else if (buff[0] & 0x40) {
		n = 2;
		buff[0] &= 0x3f;
	}
	else if (buff[0] & 0x20) {
		n = 3;
		buff[0] &= 0x1f;
	}
	else if (buff[0] & 0x10) {
		n = 4;
		buff[0] &= 0x0f;
	}
	else if (buff[0] & 0x08) {
		n = 5;
		buff[0] &= 0x07;
	}
	else if (buff[0] & 0x04) {
		n = 6;
		buff[0] &= 0x03;
	}
	else if (buff[0] & 0x02) {
		n = 7;
		buff[0] &= 0x01;
	}
	else if (buff[0] & 0x01) {
		n = 8;
		buff[0] &= 0x00;
	}
	else {
		ASSERT(false);
		nLength += 1;
		return false;
	}
	if ( n > 1 && !ReadBytes(&buff[1], n - 1u, hti) )
		return false;
	nLength += n;
	nSize = 0;
	if (n == 1) {
		((BYTE*) &nSize)[0] = buff[0];
	}
	else if (n == 2) {
		((BYTE*) &nSize)[0] = buff[1];
		((BYTE*) &nSize)[1] = buff[0];
	}
	else if (n == 3) {
		((BYTE*) &nSize)[0] = buff[2];
		((BYTE*) &nSize)[1] = buff[1];
		((BYTE*) &nSize)[2] = buff[0];
	}
	else if (n == 4) {
		((BYTE*) &nSize)[0] = buff[3];
		((BYTE*) &nSize)[1] = buff[2];
		((BYTE*) &nSize)[2] = buff[1];
		((BYTE*) &nSize)[3] = buff[0];
	}
	else if (n == 5) {
		((BYTE*) &nSize)[0] = buff[4];
		((BYTE*) &nSize)[1] = buff[3];
		((BYTE*) &nSize)[2] = buff[2];
		((BYTE*) &nSize)[3] = buff[1];
		((BYTE*) &nSize)[4] = buff[0];
	}
	else if (n == 6) {
		((BYTE*) &nSize)[0] = buff[5];
		((BYTE*) &nSize)[1] = buff[4];
		((BYTE*) &nSize)[2] = buff[3];
		((BYTE*) &nSize)[3] = buff[2];
		((BYTE*) &nSize)[4] = buff[1];
		((BYTE*) &nSize)[5] = buff[0];
	}
	else if (n == 7) {
		((BYTE*) &nSize)[0] = buff[6];
		((BYTE*) &nSize)[1] = buff[5];
		((BYTE*) &nSize)[2] = buff[4];
		((BYTE*) &nSize)[3] = buff[3];
		((BYTE*) &nSize)[4] = buff[2];
		((BYTE*) &nSize)[5] = buff[1];
		((BYTE*) &nSize)[6] = buff[0];
	}
	else if (n == 8) {
		((BYTE*) &nSize)[0] = buff[7];
		((BYTE*) &nSize)[1] = buff[6];
		((BYTE*) &nSize)[2] = buff[5];
		((BYTE*) &nSize)[3] = buff[4];
		((BYTE*) &nSize)[4] = buff[3];
		((BYTE*) &nSize)[5] = buff[2];
		((BYTE*) &nSize)[6] = buff[1];
		((BYTE*) &nSize)[7] = buff[0];
	}
	return true;
}
QWORD CMKVParser::GetInt(int hti, WORD n)
{
	ASSERT(n > 0 && n <= 8);
	QWORD nSize;
	QWORD buff;
	buff = 0;
	if (!ReadBytes(&buff, n, hti))
		return 0;
	nSize = 0;
	if (n == 1) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[0];
	}
	else if (n == 2) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[0];
	}
	else if (n == 3) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[2];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[2] = ((BYTE*) &buff)[0];
	}
	else if (n == 4) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[3];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[2];
		((BYTE*) &nSize)[2] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[3] = ((BYTE*) &buff)[0];
	}
	else if (n == 5) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[4];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[3];
		((BYTE*) &nSize)[2] = ((BYTE*) &buff)[2];
		((BYTE*) &nSize)[3] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[4] = ((BYTE*) &buff)[0];
	}
	else if (n == 6) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[5];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[4];
		((BYTE*) &nSize)[2] = ((BYTE*) &buff)[3];
		((BYTE*) &nSize)[3] = ((BYTE*) &buff)[2];
		((BYTE*) &nSize)[4] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[5] = ((BYTE*) &buff)[0];
	}
	else if (n == 7) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[6];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[5];
		((BYTE*) &nSize)[2] = ((BYTE*) &buff)[4];
		((BYTE*) &nSize)[3] = ((BYTE*) &buff)[3];
		((BYTE*) &nSize)[4] = ((BYTE*) &buff)[2];
		((BYTE*) &nSize)[5] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[6] = ((BYTE*) &buff)[0];
	}
	else if (n == 8) {
		((BYTE*) &nSize)[0] = ((BYTE*) &buff)[7];
		((BYTE*) &nSize)[1] = ((BYTE*) &buff)[6];
		((BYTE*) &nSize)[2] = ((BYTE*) &buff)[5];
		((BYTE*) &nSize)[3] = ((BYTE*) &buff)[4];
		((BYTE*) &nSize)[4] = ((BYTE*) &buff)[3];
		((BYTE*) &nSize)[5] = ((BYTE*) &buff)[2];
		((BYTE*) &nSize)[6] = ((BYTE*) &buff)[1];
		((BYTE*) &nSize)[7] = ((BYTE*) &buff)[0];
	}
	return nSize;
}

bool CMKVParser::GetID(int hti, DWORD& nID, WORD& nLength)
{
	BYTE n = 0;
	BYTE buff[sizeof(nID)];
	*(DWORD*) buff = 0;
	if (!ReadBytes(buff, 1, hti))
		return false;
	if (buff[0] & 0x80) 
		n = 1;
	else if (buff[0] & 0x40) 
		n = 2;
	else if (buff[0] & 0x20) 
		n = 3;
	else if (buff[0] & 0x10) 
		n = 4;
	else {
		ASSERT(false);
		nLength += 1;
		return false;
	}
	if (n > 1 && !ReadBytes(&buff[1], n - 1, hti))
		return false;
	nID = *(DWORD*) buff;
	nLength += n;
	return true;
}

/*virtual*/ DWORD CMKVParser::ParserCallback(const std::string& /*sGuid*/, DWORD nSize, const BYTE * /*buffer*/, DWORD /*pos*/, int /*hti*/, const VALUE_STACK& /*vstack*/, DWORD /*nField*/) const
{
	return nSize;
}




/*static*/ bool CMKVParser::Probe(BYTE buffer[12])
{
	FOURCC* fcc = reinterpret_cast<FOURCC*>( buffer ); 
#ifdef _DEBUG
	_tstring s = FCCToString( fcc[0] );
#endif
	return (fcc[0] == 0xA3DF451A);
}

