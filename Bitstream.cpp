#include "Helper.h"
#include "Bitstream.h"

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

bool Bitstream::ReadBits( DWORD n )
{
	nFTotal += n;
	n = (n + 7u) / 8u;
	if ( nBufferSize > 0 ) {
		DWORD i;
		for ( i = 0; i < n && i < nBufferSize; ++i ) {
			BYTE b = pBuffer[i];
			for ( int j = 0; j < 8; ++j ) {
				push_back( (b & 0x80) != 0 );
				b <<= 1;
			}
		}
		if ( n >= nBufferSize ) {
			nBufferSize = 0;
			delete[] pBufferBase;
			pBuffer = nullptr;
		} else {
			nBufferSize -= n;
			pBuffer += n;
		}
		n -= i;
		if ( n == 0 )
			return true;
	}
	bool bRet = true;
	if ( GetReadPos() >= GetReadFileSize() )
		return false;
	if ( GetReadPos() + n > GetReadFileSize() ) {
		n = (DWORD)(GetReadFileSize() - GetReadPos());
		bRet = false;
	}
	BYTE* p = new BYTE[n];
	if ( !ReadN( p, n ) )
		return false;
	for ( DWORD i = 0; i < n; ++i ) {
		BYTE b = p[i];
		for ( int j = 0; j < 8; ++j ) {
			push_back( (b & 0x80) != 0 );
			b <<= 1;
		}
	}
	delete[] p;
	return bRet;
}

bool Bitstream::GetBytes( BYTE* p, DWORD n ) 
{
	ASSERT( size() == 0 );
	nCTotal += n * 8;

	if ( nBufferSize > 0 ) {
		if ( n <= nBufferSize ) {
			memcpy( p, pBuffer, n );
			nBufferSize -= n;
			if ( nBufferSize == 0 ) {
				delete[] pBufferBase;
				pBuffer = nullptr;
			} else {
				pBuffer += n;
			}
			return true;
		}
		memcpy( p, pBuffer, nBufferSize );
		DWORD o = nBufferSize;
		nBufferSize = 0;
		delete[] pBufferBase;
		pBuffer = nullptr;
		return ReadN( p + o, n - o );
	}

	return ReadN( p, n );
}

bool Bitstream::Announce( WORD n )
{
	if ( n > size() )
		return ReadBits( (DWORD)(n - (DWORD)size()) );
	return true;
}

WORD Bitstream::FindMarker( bool bSkip )
{ 
	ASSERT( size() == 0 );
	DWORD o = nBufferSize;

	// Wenn ein Buffer vorhanden ist, ist der Marker entwder an Position 0, oder keiner vorhanden, aber natürlich können ein oder zwei 0 am Ende sein vom nächstne Marker
	if ( nBufferSize > 0 && pBuffer[nBufferSize - 1] == 0x00 ) {
		if ( nBufferSize > 1 && pBuffer[nBufferSize - 2] == 0x00 ) {
			ASSERT( nBufferSize - 2 <= MAXWORD ); 
			BYTE b;
			if ( !ReadN( &b, 1 ) )
				return 0;
			ReadSeek( -1 );
			if ( b == 0x01 ) {
				if ( bSkip ) {
					ClearBuffer();
					BYTE* p = new BYTE[2];
					p[0] = 0x00;
					p[1] = 0x00;
					SetBuffer( p, 2 );
				}
				return (WORD)(o - 2);
			}
			goto w;
		} else {
w:			ASSERT( nBufferSize - 1 <= MAXWORD && o - 1 <= MAXWORD ); 
			BYTE b1, b2;
			if ( !ReadN( &b1, 1 ) )
				return 0;
			if ( !ReadN( &b2, 1 ) )
				return 0;
			ReadSeek( -2 );
			if ( b1 == 0x00 && b2 == 0x01 ) {
				if ( bSkip ) {
					ClearBuffer();
					BYTE* p = new BYTE[1];
					p[0] = 0x00;
					SetBuffer( p, 1 );
				}
				return (WORD)(o - 1);
			}
		}
	}

	if ( bSkip ) 
		ClearBuffer();

	DWORD w = 0x0000ffff;
	WORD n = 0;
	for( ;; ) {
		if ( !ReadN( reinterpret_cast<BYTE*>( &w ) + 2, 1 ) )
			break;
		++n;
		if ( (w & 0x00ffffff) == 0x00010000 )
			break;
		w >>= 8;
	}
	if ( !bSkip )
		ReadSeek( -(int)n );
	else {
		ReadSeek( -3 ); 
		nFTotal += (n - 3) * 8; 
		nCTotal += (n - 3) * 8; 
	}
	ASSERT( n - 3 + o <= MAXWORD );
	return (WORD)(n - 3 + o);
}

bool Bitstream::SkipBytes( DWORD n ) 
{ 
	ASSERT( IsAligned() ); 
	if ( n >= size()/8 ) {
		nFTotal += size(); 
		nCTotal += size(); 
		n -= (DWORD)size()/8u;
		clear();
		if ( n > 0 ) { 
			if ( !ReadSeek( n ) ) 
				return false; 
			nFTotal += n * 8; 
			nCTotal += n * 8; 
		} 
	} else {
		nFTotal += n * 8; 
		nCTotal += n * 8; 
		erase( begin(), begin() + n * 8u );
	}
	return true; 
} 
WORD Bitstream::SkipPadding() // Skip 0xff Padding Bytes
{ 
	ASSERT( IsAligned() );
	WORD n = 0;
	for( ;; ) {
		if ( size() < 8 )
			ReadBits( 8u - (DWORD)size() );
		for ( WORD i = 0; i < 8; ++i ) {
			if ( !at( i ) )
				return n;
		}
		SkipBytes( 1 );
		++n;
	}
} 