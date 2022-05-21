#include "Helper.h"
#include "ID3Parser.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#include "new.h"
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif


CID3Parser::CID3Parser( class CIOService& ioservice) : CParser( ioservice )
{
	m_nSize = 0;
}

CID3Parser::~CID3Parser()
{
}

bool CID3Parser::ReadChunk(int hti, QWORD nChunkSize)
{
	// entry with 4 bytes tag already consumed
	BYTE b;
	ReadSeek(-1); // one step back
	if (!ReadBytes(&b, 1, hti))
		return false;
	int nVersionMajor = b;
	if (!ReadBytes(&b, 1, hti))
		return false;
	int nVersionMinor = b;
	BYTE flags;
	if (!ReadBytes(&flags, 1, hti))
		return false;
	DWORD l1 = GetSyncSize( hti );
	if (l1 == 0)
		return false;
	if (flags & 0x10) // Footer present
		l1 += 10;
	ASSERT(l1 <= nChunkSize); // TODO
	m_nSize = l1 + 10;
	int htitmp = OSNodeCreate(hti, GetReadPos() - 10u, _T("ID3-Tag"), _T("'ID3 '"), l1); 
	_tstringstream s;
	s << _T("Id3 Version 2.") << nVersionMajor << "." << nVersionMinor;
	OSNodeCreateValue(htitmp, s.str());
	OSNodeCreateValue(htitmp, _T("Flag Unsynchronisation"), (flags & 0x80) != 0);
	OSNodeCreateValue(htitmp, _T("Flag Extended header"), (flags & 0x40) != 0);
	OSNodeCreateValue(htitmp, _T("Flag Experimental indicator"), (flags & 0x20) != 0);
	OSNodeCreateValue(htitmp, _T("Flag Footer present"), (flags & 0x10) != 0);
	if ((flags & 0x40) != 0) { // Extended header
		DWORD ehs = GetSyncSize(hti);
		if (ehs == 0)
			return false;
		int htitmpeh = OSNodeCreate(htitmp, GetReadPos() - 4u, _T("Extended header"), ehs);
		BYTE b2;
		if (!ReadBytes(&b2, 1, hti))
			return false;
		OSNodeCreateValue(htitmpeh, _T("Flag count ") + ToString( b2 ));
		for (int i = 0; i < b2; i++) {
			BYTE b3;
			if (!ReadBytes(&b3, 1, hti))
				return false;
			OSNodeCreateValue(htitmpeh, _T("Flag") + ToHexString( b3 ));
			l1 -= ehs;
		}
	}
	while ( l1 > 10)
	{
		FOURCC fcc;
		if (!ReadBytes(&fcc, sizeof(fcc), hti))
			return false;
		if (fcc == 0) {
			l1 -= 4u;
			break;
		}
		DWORD nSize = GetSyncSize(hti);
		BYTE flag1, flag2;
		if (!ReadBytes(&flag1, 1, hti))
			return false;
		if (!ReadBytes(&flag2, 1, hti))
			return false;
		l1 -= 10u;
		int htitmptg = OSNodeCreate(htitmp, GetReadPos() - 10u, _T("Tag"), fcc, nSize + 10u );
		OSNodeCreateValue(htitmptg, _T("Flag Tag alter preservation"), (flag1 & 0x40) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag File alter preservation"), (flag1 & 0x20) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Read only"), (flag1 & 0x10) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Grouping identity"), (flag2 & 0x40) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Tag alter preservation"), (flag2 & 0x40) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Compression"), (flag2 & 0x08) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Encryption"), (flag2 & 0x04) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Unsynchronisation"), (flag2 & 0x02) != 0);
		OSNodeCreateValue(htitmptg, _T("Flag Data length indicator"), (flag2 & 0x01) != 0);
		if ((fcc & 0xFF) == 'T') { // 'Txxx'
			BYTE te;
			if (!ReadBytes(&te, 1, hti))
				return false;
			OSNodeCreateValue(htitmptg, _T("Encoding: ") + ToHexString( te ) );
			nSize -= 1u;
			l1 -= 1u;
			BYTE* buffer = new BYTE[nSize];
			if (!ReadBytes(buffer, nSize, htitmptg)) {
				delete[] buffer;
				return false;
			}
			_tstring str;
			if (te == 0x00)
				str = ToStringFromMBCS(reinterpret_cast<const char08*>(buffer), nSize);
			else if (te == 0x01)
				str = ToStringFromUTF16(reinterpret_cast<const char16*>(buffer), nSize);
			else if (te == 0x02)
				str = ToStringFromUTF16(reinterpret_cast<const char16*>(buffer), nSize);
			else if (te == 0x03)
				str = ToStringFromUTF8(reinterpret_cast<const char08*>(buffer), nSize);
			else if (!DumpBytes(buffer, nSize, htitmptg))
				return false;
			delete[] buffer;
			OSNodeCreateValue(htitmptg, _T("Text: ") + str);
		}
		else if (nSize > 0 && !ReadSeekFwdDump(nSize, htitmptg) )
			return false; 
		l1 -= nSize;
	}
	return l1 == 0 || ReadSeekFwdDump(l1, htitmp); 
}

DWORD CID3Parser::GetSyncSize( int hti )
{
	DWORD n;
	if (!ReadBytes(&n, 4, hti))
		return 0;
	// "Synchronized length"
	n = FlipBytes(&n);
	return (n & 0x000000ff) + ((n & 0x0000ff00) >> 1) + ((n & 0x00ff0000) >> 2) + ((n & 0xff000000) >> 3);
}

bool CID3Parser::Probe([[maybe_unused]]BYTE buffer[12])
{
	// "TAG" // ID3v1
	// "ID3xy" // ID3v2.x.y
	return false;
}
