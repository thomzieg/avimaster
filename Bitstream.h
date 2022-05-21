#pragma once

#include "IOService.h"

class Bitstream : public std::deque<bool>, CIOServiceHelper
{
public:
	Bitstream( CIOService& ioservice ) : CIOServiceHelper(ioservice) { Reset(); }
	void Reset() { clear(); nFTotal = 0; nCTotal = 0; /*reserve( 8*8*8 + 8);*/; nBufferSize = 0; pBuffer = nullptr; }

	bool GetBytes( BYTE* p, DWORD nBytes );
	bool Announce( WORD n );
	template<typename T>T GetAnnouncedBits( WORD n = (typeid( T ) == typeid( bool ) ? 1 : sizeof( T ) * 8) );
	template<typename T>T GetAnnouncedFlippedBits( WORD n = sizeof( T ) * 8 ); // WORD, DWORD, QWORD
	void Align() { BYTE b; while ( !IsAligned() ) GetBits( b, 1 ); } // to Byte
	bool SkipBytes( DWORD n );
	void SkipAnnounced() { if ( !IsAligned() ) Align(); if ( nFTotal ) SkipBytes( (DWORD)(nFTotal % 8) ); }
	WORD SkipPadding();
	bool IsAligned() const { return nCTotal % 8 == 0; }
	bool IsDry() const { return size() == 0; }
	WORD FindMarker( bool bSkip );
	bool GetWord( WORD& n ) { ASSERT( IsAligned() ); return GetBits( n ); }
	bool GetFlippedWord( WORD& n ) { if ( GetWord( n ) ) { n = FlipBytes( &n ); return true; } else return false; }

	bool SetBuffer( const BYTE* p, DWORD n ) { ASSERT( pBuffer == nullptr && nBufferSize == 0 ); pBufferBase = p; pBuffer = p; nBufferSize = n; return true; }
	bool ClearBuffer() { if ( pBuffer ) { delete[] pBufferBase; nBufferSize = 0; pBuffer = nullptr; } return true; }
	bool IsBufferEmpty() const { return nBufferSize == 0; }
protected:
	bool ReadBits( DWORD n );
	template<typename T> bool GetBits(T& t, WORD n = sizeof(T) * 8);

	const BYTE* pBuffer; 
	const BYTE* pBufferBase;
	DWORD nBufferSize;
	QWORD nCTotal; // Consumed Bits
	QWORD nFTotal; // Feed Bits
};

template<> bool Bitstream::GetAnnouncedBits<bool>(WORD n);

template<typename T> bool Bitstream::GetBits( T& t, WORD n )
{
	ASSERT( n <= sizeof( T ) * 8 );
	ASSERT( n > (sizeof( T ) == 1 ? 0 : sizeof( T ) * 8 / 2) );
	nCTotal += n;
	if ( n > size() ) {
		if ( !ReadBits( (DWORD)(n - (DWORD)size()) ) )
			return false;
	}
	t = 0;
	for ( int j = 0; j < n; ++j ) {
		ASSERT( size() > 0 );
		t <<= 1;
		t |= (BYTE)front();
		pop_front();
	}
	t = FlipBytes( &t );
	return true;
}

template<typename T>inline T Bitstream::GetAnnouncedBits( WORD n )
{
	ASSERT( n <= sizeof( T ) * 8 );
	ASSERT( n > (sizeof( T ) == 1 ? 0 : sizeof( T ) * 8 / 2) );
	ASSERT( n <= size() );

	T t = 0;
	if ( n > size() ) {
		if ( !ReadBits( (DWORD)(n - (DWORD)size()) ) )
			return t;
	}
	for ( int j = 0; j < n; ++j ) {
		ASSERT( size() > 0 );
		t <<= 1;
		t |= (BYTE)front();

#if 0
		if ( nVerbosity > 4 ) {
			if ( nCTotal++ % 8 == 0 ) // Achtung nCTotal += n; nicht vergessen wenn man das wieder rausnimmt
				_tcout << "-";

			_tcout << (int)front();
		} else
#endif
			++nCTotal;

		pop_front();
	}
	return t;
}
template<>inline bool Bitstream::GetAnnouncedBits<bool>( WORD n )
{
	ASSERT( n == 1 );
	ASSERT( 1 <= size() );

#if 0
	if ( nVerbosity > 4 ) {
		if ( nCTotal % 8 == 0 )
			_tcout << "-";
	}
#endif

	++nCTotal;
	if ( n > size() ) {
		if ( !ReadBits( (DWORD)(n - (DWORD)size()) ) )
			return false;
	}
	bool t = false;
	t = front();
	pop_front();

#if 0
	if ( nVerbosity > 4 ) {
		_tcout << (int)t;
	}
#endif

	return t;
}

template<typename T>T Bitstream::GetAnnouncedFlippedBits( WORD n )
{
	ASSERT( n <= sizeof( T ) * 8 );
	ASSERT( n > sizeof( T ) * 8 / 2 );
	ASSERT( n <= size() );
	T t = 0;
	if ( n > size() ) {
		if ( n % 8 == 0 && size() == 0 ) {
			ReadN( &t, n/8u );
			return t; // is also valid for failure of previous line
		}
		if ( !ReadBits( (DWORD)(n - (DWORD)size()) ) )
			return t;
	}
	for ( int j = 0; j < n; ++j ) {
		ASSERT( size() > 0 );
		t <<= 1;
		t |= (BYTE)front();

#if 0
		if ( Settings.nVerbosity > 4 ) {
			if ( nCTotal++ % 8 == 0 ) // Achtung nCTotal += n; nicht vergessen wenn man das wieder rausnimmt
				_tcout << "-";

			_tcout << (int)front();
		} else
#endif
			++nCTotal;

		pop_front();
	}
	return FlipBytes( &t );
}

