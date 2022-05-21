#include "Helper.h"
#include "Main.h" // nur für sProgramTitle
#include "Parser.h"
#include "AVIParser.h"
#include "AVIWriter.h"
#pragma warning(push, 3)
#include "stddef.h"
#pragma warning(pop)

#if (defined(_WIN32) || defined(_WIN64)) && defined(_DEBUG)
#ifdef _CONSOLE
//#define new _NEW_CRT
#else
#define new DEBUG_NEW
#endif
#endif

#if 0

CAVIWriter::CAVIWriter( CIOService& ioservice, CAVIParser::AVIFileHeader* pAVIDesc )  : CIOServiceHelper( ioservice )
{
	m_nWriteWriteTotalFrames = 0;
	m_nWriteReadTotalFrames = 0;
	m_nSegment1Size = 0;
	m_pAVIDesc = pAVIDesc;
}

CAVIWriter::~CAVIWriter()
{
}


// -------------------------------------------------------------------------------------------------------------------------

bool CAVIWriter::WriteAVIFile( int hti, const _tstring& sOFilename )
{
	m_sOFilename = sOFilename;
	if ( Settings.bConvertType2to1 || Settings.bWriteUseIndex ) {
		Settings.vStripStreams.insert( 1 ); // TODO nur bestehende auds-Streams löschen, Problem, wenn dann noch ein anderer Stream kommt...
		Settings.vStripStreams.insert( 2 );
		Settings.vStripStreams.insert( 3 );
		Settings.vStripStreams.insert( 4 );
	}

	// Streams ohne Inhalt löschen
	// TODO auch Streams, die im -xs -xe nicht drin sind...
	for ( WORD j = 0; j < m_pAVIDesc->GetStreamCount(); ++j ) {
		if ( m_pAVIDesc->m_vpAVIStreamHeader[j]->GetCounterFrames() == 0 )
			Settings.vStripStreams.insert( j );
	}

	m_pAVIDesc->m_Streams.Erase( hti );
	m_pAVIDesc->m_Streams.Renumber();
	return Save( hti );
}

bool CAVIWriter::Save( int hti )
{
WORD nFileCount = 0;
_tstring sFileNameBuffer;
_tstring s;

r:	if ( Settings.nWriteFileSize > 0 ) {
		if ( !MakeSplitFilename( m_sOFilename, nFileCount++, sFileNameBuffer ) )
			return false;
	} else
		sFileNameBuffer = m_sOFilename;

	s = _T("Writing '") + sFileNameBuffer + _T("'");
	int htitmp = OSNodeCreate( hti, 0, s );
	OSStatsInit( m_pAVIDesc->m_vpFrameEntries.size() );

	if ( !Settings.bCheckIndex ) {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Index not checked (-c), can't write Keyframe information!") );
	}
	if ( m_pAVIDesc->GetStreamCount() == 0 ) {
		OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("Nothing to do, no stream left!") );
		return false;
	}
	if ( m_pAVIDesc->m_bMustUseIndex && !Settings.bIgnoreIndexOrder ) {
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_FATAL, _T("Index defines out-of-sequence/duplicate frames which is not supported for writing files; use Option -ri to ignore ordering and write file!") );
		return false;
	}

	if ( !WriteOpenCreate( sFileNameBuffer ) )
		return false;

	if ( Settings.bPreallocate ) {
		QWORD n = GetReadFileSize();
		n += n / 50; // + 2%
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_INFO, _T("Pre-allocate file, please wait...") );
		if ( !WritePreallocate( n ) ) // ACHTUNG schreibt nTotalCount...
			return false;
		//	CreateEmptyFile( 0, GetReadFileSize() );
	}


	m_nvFrameEntryPos = 0;
	m_nWriteExtHeaderPos = 0;
	WORD nSegmentCount = 1;

	m_pAVIDesc->m_vWriteIndexEntry.reserve( m_pAVIDesc->GetStreamCount() );
	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) { // reset
		AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[i];
		AVIStreamHeader::MCOUNTER::const_iterator it = pash->mFOURCCCounter.begin();
		while ( it != pash->mFOURCCCounter.end() && (LOFCC(it->first) == LOFCC(FCC('ix00')) || !CRIFFParser::ISHIFCC_NUM(it->first)) ) // TODO auch 'pc00', '00ix' usw. ausblenden...
			++it;
		if ( it != pash->mFOURCCCounter.end() ) {
//		for ( ; it != pash->mFOURCCCounter.end(); ++it ) { // TODO bisher nur für das erste FOURCC einen Index anlegen, später für alle und von vector auf map umstellen...
			CAVIParser::AVIWriteIndexEntry ie;
			ie.fcc = FCC('00wb') + ((i % 10) << 8) + (i / 10);
			((WORD*)&ie.fcc)[1] = ((WORD*)&it->first)[1];
			ie.vMetaIndex.reserve( it->second.frames / NUMINDEX(2) + 1 );

			m_pAVIDesc->m_vWriteIndexEntry.push_back( ie );
		}
		pash->nMaxElementSize = 0;
		pash->mFOURCCCounter.clear();
		pash->nStreamSize = 0;
		pash->nInitialFrames = 0;
		m_nSizePayload = 0;
	}

	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) {
		AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[i];

		switch ( pash->m_AVIStrhChunk.fccType ) {
		case FCC('auds'):
			if ( pash->m_AVIStrfChunk.m_size >= sizeof( WAVEFORMATEX ) 
					&& pash->m_AVIStrfChunk.m_size < pash->m_AVIStrfChunk.data.pwf->cbSize + sizeof( WAVEFORMATEX ) )
				pash->m_AVIStrfChunk.data.pwf->cbSize = (WORD)(pash->m_AVIStrfChunk.m_size - sizeof( WAVEFORMATEX ));
			if ( Settings.bFixAudio ) {
				if ( pash->m_AVIStrfChunk.data.pwf->wFormatTag == WAVE_FORMAT_MPEGLAYER3 ) {
	// TODO ???
				} else if ( pash->m_AVIStrfChunk.data.pwf->wFormatTag == WAVE_FORMAT_PCM ) {
					pash->m_AVIStrhChunk.dwScale = pash->m_AVIStrfChunk.data.pwf->nBlockAlign;
					pash->m_AVIStrhChunk.dwRate = pash->m_AVIStrfChunk.data.pwf->nSamplesPerSec * pash->m_AVIStrhChunk.dwScale;
					pash->m_AVIStrhChunk.dwSampleSize = pash->m_AVIStrfChunk.data.pwf->nBlockAlign;
				}
			}
			break;
		case FCC('vids'):
			if ( Settings.bConvertType2to1 ) {
				m_pAVIDesc->m_bIsInterleaved = false;
				if ( pash->m_AVIStrhChunk.fccType == FCC('vids') && (pash->m_AVIStrhChunk.fccHandler == 0 || pash->m_AVIStrhChunk.fccHandler == FCC('dvsd') || pash->m_AVIStrhChunk.fccHandler == FCC('dv25') || pash->m_AVIStrhChunk.fccHandler == FCC('dv50')) ) { 
					pash->m_AVIStrhChunk.fccType = FCC('iavs');
					if ( pash->m_AVIStrhChunk.fccHandler == 0 && (DWORD)pash->m_AVIStrfChunk.data.pbh->biCompression > FCC('    ') )
						pash->m_AVIStrhChunk.fccHandler = (DWORD)pash->m_AVIStrfChunk.data.pbh->biCompression;
					if ( pash->m_AVIStrfChunk.m_size > sizeof( my::BITMAPINFOHEADER ) + sizeof( DVINFO ) - 8 ) {
						memcpy( pash->m_AVIStrfChunk.data.pb, pash->m_AVIStrfChunk.data.pb + sizeof( my::BITMAPINFOHEADER ), pash->m_AVIStrfChunk.m_size - sizeof( my::BITMAPINFOHEADER ) );
						pash->m_AVIStrfChunk.m_size -= sizeof( my::BITMAPINFOHEADER );
						m_nWriteHeaderSize -= sizeof( my::BITMAPINFOHEADER );
					} else if ( m_pAVIDesc->m_pDVFrameSample ) {
						delete pash->m_AVIStrfChunk.data.pb;
						pash->m_AVIStrfChunk.data.pdi = new DVINFO;
						pash->m_AVIStrfChunk.m_size = sizeof( DVINFO );
						memset( pash->m_AVIStrfChunk.data.pb, '\0', sizeof( DVINFO ) );
						pash->m_AVIStrfChunk.data.pdi->bfDVAAuxSrc.smp = m_pAVIDesc->m_pDVFrameSample->aaux_as_pc4.smp;
						pash->m_AVIStrfChunk.data.pdi->bfDVAAuxSrc.pal_ntsc = m_pAVIDesc->m_pDVFrameSample->bPAL ? 0x01 : 0x00u;
						pash->m_AVIStrfChunk.data.pdi->bfDVAAuxSrc1.pal_ntsc = m_pAVIDesc->m_pDVFrameSample->bPAL ? 0x01 : 0x00u;
						pash->m_AVIStrfChunk.data.pdi->bfDVVAuxSrc.pal_ntsc = m_pAVIDesc->m_pDVFrameSample->bPAL ? 0x01 : 0x00u;
						pash->m_AVIStrfChunk.data.pdi->bfDVVAuxCtl.disp = m_pAVIDesc->m_pDVFrameSample->b16to9 ? 0x02 : 0x00u; // (16:9 = 2)
						pash->m_AVIStrfChunk.data.pdi->bfDVVAuxCtl.bcsys = m_pAVIDesc->m_pDVFrameSample->bPAL ? 0x01 : 0x00u;
					} else {
						delete pash->m_AVIStrfChunk.data.pb;
						pash->m_AVIStrfChunk.data.pdi = new DVINFO;
						pash->m_AVIStrfChunk.m_size = sizeof( DVINFO );
						memset( pash->m_AVIStrfChunk.data.pb, '\0', sizeof( DVINFO ) );
						pash->m_AVIStrfChunk.data.pdi->bfDVAAuxSrc.smp = 0; // 48, 2 = 32 KHz
						pash->m_AVIStrfChunk.data.pdi->bfDVAAuxSrc.pal_ntsc = 1;
						pash->m_AVIStrfChunk.data.pdi->bfDVAAuxSrc1.pal_ntsc = 1;
						pash->m_AVIStrfChunk.data.pdi->bfDVVAuxSrc.pal_ntsc = 1;
						pash->m_AVIStrfChunk.data.pdi->bfDVVAuxCtl.disp = 0; // (16:9 = 2)
						pash->m_AVIStrfChunk.data.pdi->bfDVVAuxCtl.bcsys = 1;
					}
				}
			}
			// fall thru...
		case FCC('iavs'): {
			DWORD nHeight, nWidth;
			if ( pash->m_AVIStrfChunk.m_size >= sizeof( my::BITMAPINFOHEADER ) ) { // DV Type 1 hat eigentlich nur DV Strukt...
				nHeight = (DWORD)pash->m_AVIStrfChunk.data.pbh->biHeight;
				nWidth = (DWORD)pash->m_AVIStrfChunk.data.pbh->biWidth;
				if ( pash->m_AVIStrfChunk.data.pbh->biSize > pash->m_AVIStrfChunk.m_size )
					pash->m_AVIStrfChunk.data.pbh->biSize = pash->m_AVIStrfChunk.m_size;
				if ( pash->m_AVIStrhChunk.fccHandler == 0 && (DWORD)pash->m_AVIStrfChunk.data.pbh->biCompression > FCC('    ') )
					pash->m_AVIStrhChunk.fccHandler = (DWORD)pash->m_AVIStrfChunk.data.pbh->biCompression;
			} else if ( m_pAVIDesc->m_AVIMainHeader.dwHeight > 0 ) {
				nHeight = m_pAVIDesc->m_AVIMainHeader.dwHeight;
				nWidth = m_pAVIDesc->m_AVIMainHeader.dwWidth;
			} else {
				nHeight = (DWORD)pash->m_AVIStrhChunk.rcFrame.bottom;
				nWidth = (DWORD)pash->m_AVIStrhChunk.rcFrame.right;
			}
			m_pAVIDesc->m_AVIMainHeader.dwHeight = nHeight;
			m_pAVIDesc->m_AVIMainHeader.dwWidth = nWidth;
			pash->m_AVIStrhChunk.rcFrame.bottom = (short)nHeight;
			pash->m_AVIStrhChunk.rcFrame.top = 0;
			pash->m_AVIStrhChunk.rcFrame.left = 0;
			pash->m_AVIStrhChunk.rcFrame.right = (short)nWidth;

			if ( Settings.bConvertType1to2 ) {
				if ( pash->m_AVIStrhChunk.fccType == FCC('iavs') ) { 
					pash->m_AVIStrhChunk.fccType = FCC('vids');
				}
			}
			break; }
		}
	}

	if ( Settings.nFrameStart > 0 && (m_pAVIDesc->GetStreamCount() > 1 || !m_pAVIDesc->m_vpAVIStreamHeader[0]->IsVideoStream()) )
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Cutting audio streams is not supported!") );
#if 0 // funktioniert nicht...
	if ( Settings.nFrameStart > m_pAVIDesc->m_vpAVIStreamHeader[0]->GetCounterFrames() ) {
		OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Nothing to do, no frames within cutting range!") );
	}
#endif

	WriteHeader_Phase1( htitmp );
	int nRet = WriteChunkMovi( htitmp, nSegmentCount ); // ASSERT only one segment then idx1, muss WriteChunkMovi machen
	ASSERT( GetWriteTotalBytes() < (DWORD)-1 );
	m_nSegment1Size = (DWORD)(GetWriteTotalBytes() - 8 /*RIFF size*/);

	if ( nRet > 0 ) {
		if ( nRet == 1 ) {
			if ( !Settings.bWithODML ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_FATAL, _T("Write Segment #2 - Without ODML 1.02 only one Segment possible - don't use -ro switch!") );
				return false;
			}
			if ( Settings.bWithIdx1 ) {
				OSNodeCreateAlert( htitmp, CIOService::EPRIORITY::EP_WARNING, _T("Write Segment #2 - Idx1 Index superfluous with ODML 1.02 compliant multisegment AVI!") );
			}
		}
		do {
			if ( nRet == 2 ) {
				WriteHeader_Phase2( htitmp );
				WriteClose();
				goto r;
			}
		} while ( (nRet = WriteChunkMovi( htitmp, ++nSegmentCount )) != 0 );
	}
	ASSERT( m_nWriteReadTotalFrames >= m_nWriteWriteTotalFrames );
	WriteHeader_Phase2( htitmp );
	WriteClose(); 
	return true;
}

static constexpr bool m_bIndexStrict = false; // TRUE sollte nur '00db' chunks in den Index schreiben, funktioniert aber nicht (also genau die FCCs, für die der Index ist)

int CAVIWriter::WriteChunkMovi( int hti, WORD nSegmentCount )
{
DWORD nSize = 0;
DWORD nCount1 = 0, nCount3 = 0;
DWORD nChunkHeaderSize = 0;

	if ( !SetWritePosEnd() )
		return -1;  
	if ( nSegmentCount > 1 ) {
		if ( !WriteType( FCC('RIFF') ) )
			return -1;
		if ( !WriteSize( 0 ) )
			return -1;
		if ( !WriteType( FCC('AVIX') ) )
			return -1;
		OSNodeCreate( hti, GetWritePos(), _T("(Segment #") + ToString( nSegmentCount ) + _T(")"), FCC('RIFF'), 0, FCC('AVIX') );
		nChunkHeaderSize = 12; // this is not exactly true
	}
	if ( !WriteType( FCC('LIST') ) )
		return -1;
	const QWORD nPos1 = GetWritePos();
	if ( !WriteSize( 0 ) )
		return -1;
	const QWORD nPosMovi = GetWritePos();
	if ( !WriteType( FCC('movi') ) )
		return -1;
	nChunkHeaderSize += 12; // this is not exactly true
	OSNodeCreate( hti, GetWritePos() - 12, _T("(Segment #") + ToString( nSegmentCount ) + _T(")"), FCC('LIST'), 0, FCC('movi') );

	int bRet = 0;
	bool bFlag = false;
	DWORD nWriteEntryStartIdx1 = m_nvFrameEntryPos; // First frame for next indx-chunk
	CAVIParser::vAVIFRAMEENTRIES::const_iterator feit; 
	for ( feit = m_pAVIDesc->m_vpFrameEntries.begin() + m_nvFrameEntryPos; feit != m_pAVIDesc->m_vpFrameEntries.end(); ++feit, ++m_nvFrameEntryPos ) {
		OSStatsSet( m_nvFrameEntryPos );
		WORD nStream = CRIFFParser::HIFCC_NUM((*feit)->m_fcc);
		if ( nStream >= m_pAVIDesc->GetStreamCount() )
			continue;
		AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[nStream];
		if ( pash->IsVideoStream() )
			++m_nWriteReadTotalFrames;
		if ( m_nWriteReadTotalFrames < Settings.nFrameStart ) {
			++nWriteEntryStartIdx1;
			continue;
		} else if ( m_nWriteReadTotalFrames > Settings.nFrameEnd )
			continue;

		if ( !bFlag && nSegmentCount == 1 ) {
			if ( Settings.bCheckIndex && ((*feit)->m_nFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) == 0 )
				OSNodeCreateAlert( hti, CIOService::EPRIORITY::EP_WARNING, _T("First frame is not a Keyframe (will get forced to it)!") );
			(*feit)->m_nFlags |= AVIINDEXENTRY::AVIIF_KEYFRAME; // der erste Frame ist immer Keyframe
			for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) {
                for ( DWORD j = m_nvFrameEntryPos; j < m_pAVIDesc->m_vpFrameEntries.size() && j < Settings.nFrameEnd; ++j ) {
					if ( CRIFFParser::HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[j]->m_fcc) == i )
						break;
					m_pAVIDesc->m_vpAVIStreamHeader[i]->nInitialFrames++;
				}
			}
			bFlag = true;
		}

		if ( /*!Settings.bCheckIndex &&*/ pash->IsVideoStream() && (pash->m_AVIStrhChunk.fccHandler == FCC('dvds') || pash->m_AVIStrhChunk.fccHandler == FCC('MJPG')) )
			(*feit)->m_nFlags |= AVIINDEXENTRY::AVIIF_KEYFRAME; // every frame is a key frame

//		if ( Settings.bConvertType2to1 ) {
//			((WORD*)&(*feit)->m_fcc)[1] = HIFCC('##__');
//		}

		QWORD nTotalSize = nChunkHeaderSize + nSize + (*feit)->m_nSize + 8 /*fcc size*/;
		if ( nSegmentCount == 1 ) {
			nTotalSize += m_nWriteHeaderSize;
			if ( Settings.bWithIdx1 )
				nTotalSize += nCount1 * sizeof( AVIINDEXENTRY ) + 8 /*idx1 size*/;
		}
		if ( nTotalSize > Settings.nWriteMoviSize ) {
			bRet = 1;
			goto w;
		}
		if ( (Settings.nWriteFileSize > 0 && nTotalSize > Settings.nWriteFileSize) ) {
			bRet = 2;
			goto w;
		}
		if ( Settings.bWithODML ) {
			for ( WORD i = 0; i < m_pAVIDesc->m_vWriteIndexEntry.size(); ++i ) {
				if ( m_pAVIDesc->m_vWriteIndexEntry[i].bInit ) {
					nTotalSize += STDINDEXSIZE;
					if ( nTotalSize > Settings.nWriteMoviSize ) {
						bRet = 1;
						goto w;
					}
					if ( (Settings.nWriteFileSize > 0 && nTotalSize > Settings.nWriteFileSize) ) {
						bRet = 2;
						goto w;
					}
//					if ( nCount2++ < Settings.nTraceIndxEntries ) {
//						DWORD f = (((m_pAVIDesc->m_vWriteIndexEntry[i].fcc & 0x000000FF) << 16) | ((m_pAVIDesc->m_vWriteIndexEntry[i].fcc & 0x0000FF00) <<  16) | ('x' << 8) | 'i');
//						OSNodeCreate( hti, GetWritePos(), _T("'movi' Entry #") + ToString( ++nCount1 ), f, STDINDEXSIZE - 8 /*fcc size*/, m_pAVIDesc->m_vWriteIndexEntry[i].fcc );
//					}
					m_pAVIDesc->m_vWriteIndexEntry[i].nFilePos = GetWritePos();
					m_pAVIDesc->m_vWriteIndexEntry[i].nEntryStartIndx = m_nvFrameEntryPos;
					m_pAVIDesc->m_vWriteIndexEntry[i].bInit = false;
					m_pAVIDesc->m_vWriteIndexEntry[i].nCount = 0;
					CAVIParser::AVIWriteIndexEntry::MetaIndex mie;
					mie.nFilePos = GetWritePos();
					mie.nCount = 0;
					m_pAVIDesc->m_vWriteIndexEntry[i].vMetaIndex.push_back( mie );
					if ( !WriteExtendNull( STDINDEXSIZE ) )
						return -1;
					ASSERT( nSize + STDINDEXSIZE < Settings.nWriteMoviSize );
					nSize += STDINDEXSIZE; 
				}
			}
		}
		if ( nCount1++ < Settings.nTraceMoviEntries ) 
			OSNodeCreate( hti, GetWritePos(), _T("'movi' Entry #") + ToString( nCount1 ), (*feit)->m_fcc, (*feit)->m_nSize );

#ifndef IO_ASYNC_WRITE
		ASSERT( (GetWritePos() & 0x1) == 0 ); 
#endif
		if ( !WriteType( (*feit)->m_fcc, false ) )
			return -1;
		nSize += 4;

#ifdef _DEBUG 
		{ 
			if ( (*feit)->m_pData == nullptr ) {
				VERIFY( SetReadPos( (*feit)->m_nFilePos - 8 ) );

				DWORD fcc;
				VERIFY( ReadBytes( &fcc, 4, hti, false ) );
				VERIFY( /*!Settings.bConvertType2to1 &&*/ /*(Settings.vStripStreams.size() == 0 && fcc != (*feit)->m_fcc) // Diese Bedingung ist nicht mehr unbedingt wahr (Renumbering!)
					 ||*/ (Settings.vStripStreams.size() == 0 || LOFCC(fcc) == LOFCC((*feit)->m_fcc) ) );
			}
		}
#endif
		if ( !WriteSize( (*feit)->m_nSize, false ) )
			return -1;
		nSize += 4;

		if ( (*feit)->m_pData ) {
			if ( !WriteBytes( (*feit)->m_pData, (*feit)->m_nSize ) )
				return -1; 
			delete[] (*feit)->m_pData;
			(*feit)->m_pData = nullptr;
		} else
			if ( !WriteSeekRead( (*feit)->m_nFilePos, (*feit)->m_nSize ) )
				return -1;
		ASSERT( nSize >=8 );
		(*feit)->m_nOffset = nSize - 8;
		(*feit)->m_nSegment = nSegmentCount;

		if ( (*feit)->m_nSize > pash->nMaxElementSize )
			pash->nMaxElementSize = (*feit)->m_nSize;
		m_nSizePayload += (*feit)->m_nSize;

		pash->mFOURCCCounter[(*feit)->m_fcc].frames++;

		pash->nStreamSize += (*feit)->m_nSize;
		if ( m_pAVIDesc->m_vpAVIStreamHeader[nStream]->IsVideoStream() ) 
			++m_nWriteWriteTotalFrames;

#ifdef _DEBUG
		// AVIFrameEntry *a = (*feit);
		if ( nCount1 > 2 ) { // den zweiten schauen wir uns auch noch nicht an
			for ( DWORD i = m_nvFrameEntryPos - 1; i > nWriteEntryStartIdx1; i-- ) { // nur begrenzt zurück gehen
				if ( m_pAVIDesc->m_vpFrameEntries[i]->m_nSegment > m_pAVIDesc->m_vpFrameEntries[m_nvFrameEntryPos]->m_nSegment ) {
					// AVIFrameEntry *b = m_pAVIDesc->m_vpFrameEntries[i];
					ASSERT( false ); // TODO
				} // Der Check oben stimmt nicht, wenn man mit -xs <frame im Semgent2> als Start arbeitet
				if ( m_pAVIDesc->m_vpFrameEntries[i]->m_fcc == m_pAVIDesc->m_vpFrameEntries[m_nvFrameEntryPos]->m_fcc ) {
					if ( m_pAVIDesc->m_vpFrameEntries[i]->m_nSegment < m_pAVIDesc->m_vpFrameEntries[m_nvFrameEntryPos]->m_nSegment )
						break;
					VERIFY(  m_pAVIDesc->m_vpFrameEntries[i]->m_nOffset < m_pAVIDesc->m_vpFrameEntries[m_nvFrameEntryPos]->m_nOffset );
					break;
				}
			}
		}
#endif
		ASSERT( nSize + (*feit)->m_nSize < Settings.nWriteMoviSize );
		nSize += (*feit)->m_nSize;
		if ( !WriteExtendAlign( nSize ) ) return -1;
#ifndef IO_ASYNC_WRITE
		ASSERT( nTotalSize + (nTotalSize&0x1) == GetWritePos() ); 
#endif

		if ( Settings.bWithODML && (!m_bIndexStrict && CRIFFParser::HIFCC_NUM((*feit)->m_fcc) == nStream && pash->IsVideoStream() || m_pAVIDesc->m_vWriteIndexEntry[nStream].fcc == (*feit)->m_fcc) ) {
			if ( ++m_pAVIDesc->m_vWriteIndexEntry[nStream].nCount >= NUMINDEX(2) ) {
				ASSERT( (*feit)->m_nSegment == nSegmentCount );
				if ( !WriteStandardIndex( hti, (*feit)->m_fcc, m_pAVIDesc->m_vWriteIndexEntry[nStream].nCount, m_pAVIDesc->m_vWriteIndexEntry[nStream].nEntryStartIndx, m_pAVIDesc->m_vWriteIndexEntry[nStream].nFilePos, nPosMovi, (nCount3++ < Settings.nTraceIndxEntries), false ) )
					return -1;
// TODO				pash->mFOURCCCounter[].frames++;
				pash->mFOURCCCounter[(*feit)->m_fcc].odmlindex += m_pAVIDesc->m_vWriteIndexEntry[nStream].nCount;
				m_pAVIDesc->m_vWriteIndexEntry[nStream].bInit = true;
				m_pAVIDesc->m_vWriteIndexEntry[nStream].vMetaIndex.back().nCount = m_pAVIDesc->m_vWriteIndexEntry[nStream].nCount;
				m_pAVIDesc->m_vWriteIndexEntry[nStream].nCount = 0;
			}
		} 
	}

w:	if ( !WriteExtendAlign( nSize ) ) return -1;

	if ( m_nvFrameEntryPos > 0 ) {
		--m_nvFrameEntryPos, --feit;
		ASSERT( (GetWritePos() & 0x1) == 0 );
		if ( (Settings.nTraceMoviEntries > 0 && nCount1 > Settings.nTraceMoviEntries) ) 
			OSNodeCreate( hti, GetWritePos(), _T("'movi' Entry #") + ToString( nCount1 ), (*feit)->m_fcc, (*feit)->m_nSize );
	}

	if ( Settings.bWithODML ) {
		for ( WORD i = 0; i < m_pAVIDesc->m_vWriteIndexEntry.size(); ++i ) {
			if ( m_pAVIDesc->m_vWriteIndexEntry[i].nCount > 0 || !m_pAVIDesc->m_vWriteIndexEntry[i].bInit ) { // auch schreiben, wenn keine Einträge vorgenommen wurden
				WORD nStream = CRIFFParser::HIFCC_NUM(m_pAVIDesc->m_vWriteIndexEntry[i].fcc);
				AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[nStream];
				if ( !WriteStandardIndex( hti, m_pAVIDesc->m_vWriteIndexEntry[i].fcc, m_pAVIDesc->m_vWriteIndexEntry[i].nCount, m_pAVIDesc->m_vWriteIndexEntry[i].nEntryStartIndx, m_pAVIDesc->m_vWriteIndexEntry[i].nFilePos, nPosMovi, (Settings.nTraceIndxEntries > 0), false ) )
					return -1;
// TODO			pash->mFOURCCCounter[].frames++;
				pash->mFOURCCCounter[m_pAVIDesc->m_vWriteIndexEntry[i].fcc].odmlindex += m_pAVIDesc->m_vWriteIndexEntry[i].nCount;
				m_pAVIDesc->m_vWriteIndexEntry[i].vMetaIndex.back().nCount = m_pAVIDesc->m_vWriteIndexEntry[i].nCount;
				m_pAVIDesc->m_vWriteIndexEntry[i].nCount = 0;
			}
			m_pAVIDesc->m_vWriteIndexEntry[i].bInit = true;
		}
	}

	if ( nSegmentCount == 1 && Settings.bWithIdx1 )
		WriteChunkIdx1( hti, nWriteEntryStartIdx1, nPosMovi );

	if ( !SetWritePos( nPos1 ) )
		return -1;
	ASSERT( nSize < Settings.nWriteMoviSize );
	OSNodeCreate( hti, GetWritePos(), _T("(Segment #") + ToString( nSegmentCount ) + _T(")"), FCC('LIST'), nSize + 4 /*movi*/, FCC('movi') );
	if ( !WriteSize( nSize + 4 /*movi*/, false, false ) )
		return -1;
	if ( nSegmentCount > 1 ) {
		if ( !SetWritePos( nPos1 - 12 ) ) // RIFF-Header
			return -1;
		if ( !WriteSize( nSize + 12 + 4 /*movi*/, false, false ) )
			return -1;
	}
	if ( !SetWritePosEnd() )
		return -1;
	return bRet;
}

// Reserve Space for Main Header
bool CAVIWriter::WriteHeader_Phase1( int hti )
{
	m_nWriteHeaderSize = 0;
	m_nWriteHeaderSize += 8 /*RIFF size*/ + 4 /*AVI */ + 8 /*LIST size*/ + 4 /*hdrl*/ + 4 /*avih*/;
	m_nWriteHeaderSize += sizeof( AVIMAINHEADER ) + 4 /*size*/;
	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i )
		m_nWriteHeaderSize += CalcStreamHeaderSize( i ) + 8 /*LIST size*/;

	if ( Settings.bWithODML )
		m_nWriteHeaderSize += 8 /*LIST size*/ + sizeof( AVIEXTHEADER ) + 4 /*odml*/ + 8 /*dmlh size*/;

	if ( !Settings.bTight ) {
		DWORD n;
		for ( n = 1; n < 100; ++n ) {
			if ( m_nWriteHeaderSize == (n * AVI_HEADERSIZE - 12 /*LIST size movi*/) || m_nWriteHeaderSize + 8 /*JUNK size*/ <= (n * AVI_HEADERSIZE - 12) )
				break;
		}
		ASSERT( n < 100 );
		m_nWriteHeaderTotalSize = n * AVI_HEADERSIZE - 12 /*LIST size movi*/;
	} else
		m_nWriteHeaderTotalSize = m_nWriteHeaderSize;

	if ( !SetWritePosStart() )
		return false;
	OSNodeCreate( hti, GetWritePos(), _T("AVI Main Header Phase 1"), FCC('RIFF'), m_nWriteHeaderSize, FCC('AVI ') );
	if ( !WriteExtendNull( m_nWriteHeaderTotalSize - 4 ) )
		return false;
	if ( !WriteSize( 0, false ) ) // to fix file size...
		return false;
	
	return true;
}

// Write Main Header, write after first chunk
bool CAVIWriter::WriteHeader_Phase2( int hti )
{
	if ( !WritePreallocate( 0 ) ) // an dieser Stelle wurde das Max. geschrieben, das jetzt fixieren...
		return false;

	OSNodeCreate( hti, GetWritePos(), _T("AVI Main Header Phase 2"), FCC('RIFF'), m_nWriteHeaderSize, FCC('AVI ') );
	m_pAVIDesc->m_AVIMainHeader.dwTotalFrames = m_nWriteWriteTotalFrames;

	m_pAVIDesc->m_AVIMainHeader.dwFlags &= ~AVIMAINHEADER::AVIF_MUSTUSEINDEX; // Not supported
//	m_pAVIDesc->m_AVIMainHeader.dwFlags &= ~AVIMAINHEADER::AVIF_TRUSTCKTYPE; // useless...
	m_pAVIDesc->m_AVIMainHeader.dwFlags = (m_pAVIDesc->m_AVIMainHeader.dwFlags & ~AVIMAINHEADER::AVIF_HASINDEX) | (Settings.bWithIdx1 ? AVIMAINHEADER::AVIF_HASINDEX : 0);
	m_pAVIDesc->m_AVIMainHeader.dwFlags = (m_pAVIDesc->m_AVIMainHeader.dwFlags & ~AVIMAINHEADER::AVIF_ISINTERLEAVED) | (m_pAVIDesc->m_bIsInterleaved ? AVIMAINHEADER::AVIF_ISINTERLEAVED : 0);

	// AUTOKORREKT
	m_pAVIDesc->m_AVIMainHeader.dwStreams = m_pAVIDesc->GetStreamCount();
	m_pAVIDesc->m_AVIMainHeader.dwSuggestedBufferSize = 0;

	m_pAVIDesc->m_AVIMainHeader.dwMaxBytesPerSec = 0;
	m_pAVIDesc->m_AVIMainHeader.dwMaxBytesPerSec = 0; // TODO calculate
	m_pAVIDesc->m_AVIMainHeader.dwPaddingGranularity = 0; // TODO 2?

	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) { // Set Length...
		AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[i];
		pash->m_AVIStrhChunk.dwSuggestedBufferSize = pash->nMaxElementSize;
		pash->m_AVIStrhChunk.dwInitialFrames = pash->nInitialFrames;
		if ( pash->nMaxElementSize > m_pAVIDesc->m_AVIMainHeader.dwSuggestedBufferSize )
			m_pAVIDesc->m_AVIMainHeader.dwSuggestedBufferSize = pash->nMaxElementSize;
//		if ( ... > m_pAVIDesc->m_AVIMainHeader.dwMaxBytesPerSec ) // TODO
//			m_pAVIDesc->m_AVIMainHeader.dwMaxBytesPerSec = ...;
		if ( pash->IsVideoStream() ) {
			pash->m_AVIStrhChunk.dwLength = pash->GetCounterFrames();
			m_pAVIDesc->m_AVIMainHeader.dwTotalFrames = pash->m_AVIStrhChunk.dwLength;
			m_pAVIDesc->m_AVIMainHeader.dwInitialFrames = pash->m_AVIStrhChunk.dwInitialFrames;
			DWORD gcd = GCD( pash->m_AVIStrhChunk.dwRate, pash->m_AVIStrhChunk.dwScale );
			if ( gcd > 1 ) {
				pash->m_AVIStrhChunk.dwRate /= gcd;
				pash->m_AVIStrhChunk.dwScale /= gcd;
			}
			if ( Settings.fSetVideoFPS > 0.0 ) { 
				m_pAVIDesc->m_AVIMainHeader.dwMicroSecPerFrame = (DWORD) (1000000.0 / Settings.fSetVideoFPS);
				pash->m_AVIStrhChunk.dwRate = (DWORD)(Settings.fSetVideoFPS * 10000.0);
				pash->m_AVIStrhChunk.dwScale = 10000;
			} else {
				m_pAVIDesc->m_AVIMainHeader.dwMicroSecPerFrame = (DWORD) (1000000.0 / ((double)pash->m_AVIStrhChunk.dwRate / pash->m_AVIStrhChunk.dwScale));
			}
		}
		else if ( pash->m_AVIStrhChunk.fccType == FCC('auds') ) {
			if ( (pash->m_AVIStrhChunk.dwScale == 0 || Settings.bFixAudio) && pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec > 0 && pash->m_AVIStrfChunk.data.pwf->nBlockAlign > 0 ) {

				double newrate = (double)pash->nStreamSize / (m_pAVIDesc->m_AVIMainHeader.GetLengthMSec() / 1000);
				pash->m_AVIStrhChunk.dwScale /= (DWORD)(pash->m_AVIStrhChunk.dwRate / newrate);
				pash->m_AVIStrhChunk.dwRate = (DWORD)newrate;
				pash->m_AVIStrhChunk.dwLength = (DWORD)(pash->nStreamSize / pash->m_AVIStrhChunk.dwScale);
				pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec = (DWORD)((double)pash->m_AVIStrfChunk.data.pwf->nBlockAlign * pash->m_AVIStrhChunk.dwRate / pash->m_AVIStrhChunk.dwScale + .5);


#if 0
				double oldsize = pash->m_AVIStrhChunk.dwLength * pash->m_AVIStrhChunk.dwScale;
				pash->m_AVIStrhChunk.dwRate *= 100;
				pash->m_AVIStrhChunk.dwScale = (DWORD)(oldsize / pash->nStreamSize * pash->m_AVIStrhChunk.dwScale * 60 / 100 + .5);
				pash->m_AVIStrhChunk.dwLength = (DWORD)(pash->nStreamSize / pash->m_AVIStrhChunk.dwScale);
				pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec = (DWORD)((double)pash->m_AVIStrfChunk.data.pwf->nBlockAlign * pash->m_AVIStrhChunk.dwRate / pash->m_AVIStrhChunk.dwScale + .5);

				DWORD oldrate = pash->m_AVIStrhChunk.dwRate;
 				pash->m_AVIStrhChunk.dwRate = (DWORD)(pash->nStreamSize / oldsize * pash->m_AVIStrhChunk.dwRate + .5);
				pash->m_AVIStrhChunk.dwScale = (DWORD)(oldsize / pash->nStreamSize * pash->m_AVIStrhChunk.dwScale + .5);
				pash->m_AVIStrhChunk.dwLength = (DWORD)(pash->nStreamSize / pash->m_AVIStrhChunk.dwScale);

				double factor = (double)oldrate / pash->m_AVIStrhChunk.dwRate;
				pash->m_AVIStrhChunk.dwScale /= factor;
				pash->m_AVIStrhChunk.dwRate *= factor;
	//			pash->m_AVIStrhChunk.dwLength *= factor;


//
//				DWORD nNewScale = (DWORD)(((double)pash->m_AVIStrhChunk.dwRate / pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec * pash->m_AVIStrfChunk.data.pwf->nBlockAlign) + .5);
//				pash->m_AVIStrhChunk.dwRate = (DWORD)((double)pash->m_AVIStrhChunk.dwRate * pash->m_AVIStrhChunk.dwScale / nNewScale + .5);
//				pash->m_AVIStrhChunk.dwScale = nNewScale;
#endif
			} else {
				pash->m_AVIStrhChunk.dwLength = (DWORD)(pash->nStreamSize / pash->m_AVIStrhChunk.dwScale);

				double dLen = pash->GetLengthMSec() / 1000.0;
				if ( dLen >= 1.0 )
					pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec = (DWORD)(pash->nStreamSize / dLen + 0.5);
				else
					pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec = (DWORD)pash->nStreamSize;
			}
			if ( pash->m_AVIStrfChunk.data.pwf->wFormatTag == WAVE_FORMAT_PCM ) 
				ASSERT( pash->m_AVIStrfChunk.data.pwf->nAvgBytesPerSec == pash->m_AVIStrfChunk.data.pwf->nSamplesPerSec * pash->m_AVIStrfChunk.data.pwf->nBlockAlign );
			
		}
	}

	if ( !SetWritePosStart() )
		return false;
	OSNodeCreate( hti, GetWritePos(), _T("Segment #1"), FCC('RIFF'), m_nSegment1Size, FCC('AVI ') );
	if ( !WriteType( FCC('RIFF'), true, false ) )
		return false;
	ASSERT( m_nSegment1Size < Settings.nWriteMoviSize + m_nWriteHeaderSize );
	if ( !WriteSize( m_nSegment1Size, true, false ) )
		return false;
	if ( !WriteType( FCC('AVI '), true, false ) )
		return false;
	OSNodeCreate( hti, GetWritePos(), _T("Segment #1"), FCC('LIST'), m_nWriteHeaderSize - 20 /*RIFF size AVI LIST size*/, FCC('hdrl') );
	if ( !WriteType( FCC('LIST'), true, false ) )
		return false;
	if ( !WriteSize( m_nWriteHeaderSize - 20 /*RIFF size AVI LIST size*/, true, false ) )
		return false;
	if ( !WriteType( FCC('hdrl'), true, false ) )
		return false;
	if ( !WriteType( FCC('avih'), true, false ) )
		return false;
	if ( !WriteSize( sizeof( AVIMAINHEADER ), true, false ) )
		return false;
	if ( !WriteBytes( &m_pAVIDesc->m_AVIMainHeader, sizeof( AVIMAINHEADER ), false ) )
		return false;

	for ( WORD i = 0; i < m_pAVIDesc->GetStreamCount(); ++i ) {
		DWORD nHeaderSize = CalcStreamHeaderSize( i );
		const AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[i];
		OSNodeCreate( hti, GetWritePos(), _T("Segment #1"), FCC('LIST'), nHeaderSize, FCC('strl') );
		if ( !WriteType( FCC('LIST'), true, false ) )
			return false;
		if ( !WriteSize( nHeaderSize, true, false ) )
			return false;
		if ( !WriteType( FCC('strl'), true, false ) )
			return false;
		if ( !WriteType( FCC('strh'), true, false ) )
			return false;
		if ( !WriteSize( sizeof( AVISTREAMHEADER ), true, false ) )
			return false;
		if ( !WriteBytes( &pash->m_AVIStrhChunk, sizeof( AVISTREAMHEADER ), false ) )
			return false;
		OSNodeCreate( hti, GetWritePos(), _T("Stream #") + ToString( i ), pash->m_AVIStrhChunk.fccType );
		if ( !WriteType( FCC('strf'), true, false ) )
			return false;
		if ( !WriteSize( pash->m_AVIStrfChunk.m_size, true, false ) )
			return false;
		if ( !WriteBytes( pash->m_AVIStrfChunk.data.pb, pash->m_AVIStrfChunk.m_size, false ) )
			return false;
		if ( pash->m_AVIStrfChunk.m_size & 0x01 ) {
			if ( !WriteSeek( 1 ) )
				return false;
		}
		if ( !Settings.bTight && pash->m_sName.length() > 0 ) {
			if ( !WriteType( FCC('strn'), true, false ) )
				return false;
			if ( !WriteSize( (DWORD)pash->m_sName.length(), true, false ) )
				return false;
			if ( !WriteBytes( pash->m_sName.c_str(), (DWORD)pash->m_sName.length(), false ) )
				return false;
			if ( pash->m_sName.length() & 0x01 ) { 
				if ( !WriteSeek( 1 ) )
					return false;
			}
		}
		if ( pash->m_AVIStrdChunk.m_size > 0 ) {
			if ( !WriteType( FCC('strd'), true, false ) )
				return false;
			if ( !WriteSize( pash->m_AVIStrdChunk.m_size, true, false ) )
				return false;
			if ( !WriteBytes( pash->m_AVIStrdChunk.data.pb, pash->m_AVIStrdChunk.m_size, false ) )
				return false;
			if ( pash->m_AVIStrdChunk.m_size & 1 ) {
				if ( !WriteSeek( 1 ) )
					return false;
			}
		}
		if ( Settings.bWithODML && !WriteMetaIndex( hti, i, true ) )
			return false;
	}
	if ( Settings.bWithODML ) { 
		if ( !WriteType( FCC('LIST'), true, false ) )
			return false;
		if ( !WriteSize( sizeof( AVIEXTHEADER ) + 4 /*odml*/ + 8 /*dmlh size*/, true, false ) )
			return false;
		if ( !WriteType( FCC('odml'), true, false ) )
			return false;
		if ( !WriteType( FCC('dmlh'), true, false ) )
			return false;
		if ( !WriteSize( sizeof( AVIEXTHEADER ), true, false ) )
			return false;
		m_nWriteExtHeaderPos = GetWritePos();
		if ( !WriteBytes( &m_pAVIDesc->m_AVIExtHeader, sizeof( AVIEXTHEADER ), false ) )
			return false;
	}

	if ( m_nWriteHeaderTotalSize > m_nWriteHeaderSize ) {
		if ( !WriteType( FCC('JUNK'), true, false ) )
			return false;
		if ( !WriteSize( m_nWriteHeaderTotalSize - m_nWriteHeaderSize - 8 /*JUNK size*/, true, false ) )
			return false;
		if ( sProgramTitle.length() <= m_nWriteHeaderTotalSize - m_nWriteHeaderSize - 8 /*JUNK size*/ )
			WriteString( sProgramTitle, false ); 
		// muss nicht mehr geschrieben werden!!!
//		if ( !WriteNull( m_nWriteHeaderTotalSize - m_nWriteHeaderSize - 8 /*JUNK size*/ ) )
//			return false;
	}

	if ( m_nWriteExtHeaderPos ) {
		if ( !SetWritePos( m_nWriteExtHeaderPos + offsetof( AVIEXTHEADER, dwGrandFrames ) ) )
			return false;
		if ( !WriteSize( m_nWriteWriteTotalFrames, false, false ) )
			return false;
	}
	return true;
}

bool CAVIWriter::WriteStandardIndex( int hti, FOURCC fcc, WORD nCount, DWORD nvFrameEntryStartPos, QWORD nPosIndx, QWORD nPosMovi, bool bDump, bool bTight )
{
DWORD nCount1 = 0;
DWORD nOffsetPrev = 0;

// bTight == true: Größe nur nach Anzahl Einträgen schreiben: ACHTUNG: Funktioniert so nicht, dann muss auch der
// reservierte Platz für den Index vorher angepasst worden sein und der Meta-Index einen entsprechenden Längeneintrag haben

	ASSERT( nCount <= NUMINDEX(2) );
	AVIMetaIndex mi;
	mi.bIndexType = AVIMetaIndex::AVI_INDEX_OF_CHUNKS;
	mi.dwChunkId = fcc;
	mi.nEntriesInUse = nCount;
	mi.wLongsPerEntry = 2;
	mi.bIndexSubType = AVIMetaIndex::AVI_INDEX_SUB_DEFAULT;
	mi.StandardIndex.qwBaseOffset = nPosMovi;
	mi.StandardIndex.dwReserved_3 = 0;
	WORD nSegment = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nSegment;
	for ( DWORD i = 0; i < nCount; ++i ) {
		while ( m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_fcc != fcc && (m_bIndexStrict || CRIFFParser::HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_fcc) != CRIFFParser::HIFCC_NUM(fcc) || !m_pAVIDesc->m_vpAVIStreamHeader[CRIFFParser::HIFCC_NUM(fcc)]->IsVideoStream()) ) {
			if ( ++nvFrameEntryStartPos >= m_pAVIDesc->m_vpFrameEntries.size() ) {
				ASSERT( nvFrameEntryStartPos < m_pAVIDesc->m_vpFrameEntries.size() );
				goto w;
			}
		}
		ASSERT( m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nOffset < (DWORD)-4 );
		ASSERT( nSegment == m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nSegment );
		mi.StandardIndex.aIndex[i].dwOffset = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nOffset + 4 + 8; // ????
		mi.StandardIndex.aIndex[i].dwSize = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nSize;
		if ( (m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nFlags & AVIINDEXENTRY::AVIIF_KEYFRAME) == 0 ) // DeltaFrame
			mi.StandardIndex.aIndex[i].dwSize |= AVIMetaIndex::AVISTDINDEX_DELTAFRAME;
		ASSERT( mi.StandardIndex.aIndex[i].dwOffset > nOffsetPrev );
		nOffsetPrev = mi.StandardIndex.aIndex[i].dwOffset;
		++nvFrameEntryStartPos;
	}
w:	if ( !SetWritePos( nPosIndx ) )
		return false;
	DWORD f = (((fcc & 0x000000FF) << 16) | ((fcc & 0x0000FF00) <<  16) | ('x' << 8) | 'i');
//	DWORD f = SETHIFCC_NUM( FCC('##ix'), CRIFFParser::HIFCC_NUM(fcc) );
	WORD nSize = bTight ? 32 + sizeof( mi.StandardIndex.aIndex[0] ) * nCount : STDINDEXSIZE - 8/*fcc size*/;
	if ( nCount1++ < Settings.nTraceIndxEntries && bDump ) {
		/*int htitmp =*/ OSNodeCreate( hti, GetWritePos(), _T("StandardIndex #") + ToString( nCount1 ), f, nSize + 8u ); 
//		CAVIParser::RIFFDumpAvi_IndxChunk( htitmp, &mi, sizeof( AVIMetaIndex ) );
	}
	if ( !WriteType( f, bDump, false ) )
		return false;
	if ( !WriteSize( nSize, bDump, false ) )
		return false;
	if ( !WriteBytes( &mi, nSize, false ) )
		return false;
	if ( !SetWritePosEnd() )
		return false;
	return true;
}

bool CAVIWriter::WriteMetaIndex( int hti, WORD nStream, bool bDump )
{
DWORD nCount1 = 0;

	AVIMetaIndex mi;
	mi.bIndexType = AVIMetaIndex::AVI_INDEX_OF_INDEXES;
	mi.dwChunkId = m_pAVIDesc->m_vWriteIndexEntry[nStream].fcc;
	mi.nEntriesInUse = (DWORD)m_pAVIDesc->m_vWriteIndexEntry[nStream].vMetaIndex.size();
	mi.wLongsPerEntry = 4;
	mi.bIndexSubType = AVIMetaIndex::AVI_INDEX_SUB_DEFAULT;
	mi.SuperIndex.dwReserved[0] = 0;
	mi.SuperIndex.dwReserved[1] = 0;
	mi.SuperIndex.dwReserved[2] = 0;

	for ( DWORD i = 0; i < m_pAVIDesc->m_vWriteIndexEntry[nStream].vMetaIndex.size(); ++i ) {
		mi.SuperIndex.aIndex[i].qwOffset = m_pAVIDesc->m_vWriteIndexEntry[nStream].vMetaIndex[i].nFilePos;
		mi.SuperIndex.aIndex[i].dwSize = STDINDEXSIZE; // - 8/*fcc size*/;
		mi.SuperIndex.aIndex[i].dwDuration = m_pAVIDesc->m_vWriteIndexEntry[nStream].vMetaIndex[i].nCount;
	}

	if ( nCount1++ < Settings.nTraceIndxEntries && bDump ) {
		/*int htitmp =*/ OSNodeCreate( hti, GetWritePos(), _T("MetaIndex #") + ToString( nCount1 ), FCC('indx'), sizeof( AVIMetaIndex ) );
//		CAVIParser::RIFFDumpAvi_IndxChunk( htitmp, &mi, sizeof( AVIMetaIndex ) );
	}
	if ( !WriteType( FCC('indx'), bDump, false ) )
		return false;
	if ( !WriteSize( STDINDEXSIZE - 8/*fcc size*/, bDump, false ) )
		return false;
	if ( !WriteBytes( &mi, STDINDEXSIZE - 8/*fcc size*/, false ) )
		return false;
	return true;
}

bool CAVIWriter::WriteChunkIdx1( int hti, DWORD nvFrameEntryStartPos, QWORD nWriteFileMoviPos )
{
DWORD nSize = 0;
DWORD nCount1 = 0;
int htitmp;

	if ( !SetWritePosEnd() )
		return false;  
	if ( !WriteType( FCC('idx1') ) )
		return false;
	const QWORD nPos1 = GetWritePos();
	if ( !WriteSize( 0 ) )
		return false;

	AVIINDEXENTRY ie;
	bool bRet = true;

	for ( ; bRet && nvFrameEntryStartPos < m_pAVIDesc->m_vpFrameEntries.size() && nvFrameEntryStartPos <= m_nvFrameEntryPos; ++nvFrameEntryStartPos ) {
		ie.ckid = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_fcc;
		ie.dwChunkLength = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nSize;
		ie.dwChunkOffset = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nOffset + 4 /*idx1*/;
		ie.dwFlags = m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_nFlags;
		m_pAVIDesc->m_vpAVIStreamHeader[CRIFFParser::HIFCC_NUM(m_pAVIDesc->m_vpFrameEntries[nvFrameEntryStartPos]->m_fcc)]->mFOURCCCounter[ie.ckid].oldindex++;
		bRet = WriteBytes( &ie, sizeof( AVIINDEXENTRY ) );
		nSize += sizeof( AVIINDEXENTRY );
		if ( nCount1++ < Settings.nTraceIdx1Entries ) {
			htitmp = OSNodeCreate( hti, GetReadPos(), _T("'idx1' Entry #") + ToString( nCount1 ), ie.ckid, sizeof( AVIINDEXENTRY ) );
//			CAVIParser::RIFFDumpAvi_Idx1Entry( htitmp, &ie, nWriteFileMoviPos, sizeof( AVIINDEXENTRY ) );
		}
	}
	if ( !SetWritePos( nPos1 ) )
		return false;
	if ( !WriteSize( nSize, false, false ) )
		return false;
	if ( !SetWritePosEnd() )
		return false;
	return bRet;
}

DWORD CAVIWriter::CalcStreamHeaderSize( WORD nStream ) const
{
	DWORD nSize = 0;
	const AVIStreamHeader* pash = m_pAVIDesc->m_vpAVIStreamHeader[nStream];
	nSize += sizeof( AVISTREAMHEADER ) + 4 /*strl*/ + 4 /*strh*/ + 4 /*size aviheader*/ + 4 /*strf*/;
	nSize += pash->m_AVIStrfChunk.m_size + 4 /*size*/;
	nSize += nSize & 0x01;
	if ( !Settings.bTight && pash->m_sName.length() > 0 ) {
		nSize += (DWORD)pash->m_sName.length() + 8 /*strn size*/;
		nSize += nSize & 0x01;
	}
	if ( pash->m_AVIStrdChunk.m_size > 0 )
		nSize += pash->m_AVIStrdChunk.m_size + 8 /*strd size*/;
	nSize += nSize & 0x01;
	if ( Settings.bWithODML )
		nSize += STDINDEXSIZE;
	return nSize;
}

#if 0

/*static*/ struct CAVIWriter::ThreadInfo CAVIWriter::ThreadInfo;

/*static*/ DWORD WINAPI CAVIWriter::ThreadProc( win::LPVOID lpParameter )
{
	// TODO ACHTUNG ExtendNull zählt das jetzt zu nTotalCount...
	if ( !WriteExtendNull( reinterpret_cast<struct ThreadInfo*>( lpParameter )->nSize ) )
		return -1;
	return 0;
}

bool CAVIWriter::CreateEmptyFile( int hti, QWORD nSize )
{
	win::DWORD nThreadId;
	ThreadInfo.nSize = nSize;
	win::HANDLE hThread = win::CreateThread( nullptr, 0, reinterpret_cast<win::LPTHREAD_START_ROUTINE>( ThreadProc ), &ThreadInfo, 0, &nThreadId );
	ASSERT( hThread != nullptr && nThreadId > 0 );
	return true;
}

#endif

#endif
