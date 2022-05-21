#pragma once

#include "Parser.h"

#pragma pack(push,1)
#undef WAVE_FORMAT_PCM
struct WAVEFORMAT {
	WORD    wFormatTag;        /* format type */
	WORD    nChannels;         /* number of channels (i.e. mono, stereo...) */
	DWORD   nSamplesPerSec;    /* sample rate */
	DWORD   nAvgBytesPerSec;   /* for buffer estimation */
	WORD    nBlockAlign;       /* block size of data */
};

struct WAVEFORMATEX : public WAVEFORMAT
{
	WORD    wBitsPerSample;    /* Number of bits per sample of mono data */
	WORD    cbSize;            /* The count in bytes of the size of extra information (after cbSize) */
};
#pragma pack(pop)


class CRIFFParser : public CParser
{
public:

#pragma pack(push,1)

#undef CF_TEXT
#undef CF_BITMAP
#undef CF_METAFILEPICT
#undef CF_SYLK
#undef CF_DIF
#undef CF_TIFF
#undef CF_OEMTEXT
#undef CF_DIB
#undef CF_PALETTE
#undef CF_PENDATA
#undef CF_RIFF
#undef CF_WAVE
#undef CF_UNICODETEXT
#undef CF_ENHMETAFILE
#undef CF_HDROP
#undef CF_LOCALE
#undef CF_DIBV5

	struct RIFFWaveDispChunk { // DispChunk
		DWORD dwType;
		enum { 
			CF_TEXT = 1,
			CF_BITMAP = 2,
			CF_METAFILEPICT = 3,
			CF_SYLK = 4,
			CF_DIF = 5,
			CF_TIFF = 6,
			CF_OEMTEXT = 7,
			CF_DIB = 8,
			CF_PALETTE = 9,
			CF_PENDATA = 10,
			CF_RIFF = 11,
			CF_WAVE = 12,
			CF_UNICODETEXT = 13,
			CF_ENHMETAFILE = 14,
			CF_HDROP = 15,
			CF_LOCALE = 16,
			CF_DIBV5 = 17
		};
		BYTE bData[];
	};

	struct DMUS_IO_VERSION
	{
		DWORD dwVersionMS;        /* Version # high-order 32 bits */
		DWORD dwVersionLS;        /* Version # low-order 32 bits  */
	};

	/* flags for wFormatTag field of WAVEFORMAT */
	// http://www.iana.org/assignments/wave-avi-codec-registry, http://kibus1.narod.ru/frames_eng.htm?sof/abcavi/twocc.htm
	// #define WAVE_FORMAT_PCM		0x0001
	#define WAVE_FORMAT_PCM 0x0001
	#define WAVE_FORMAT_ADPCM	0x0002
	#define WAVE_FORMAT_ALAW 0x0006
	#define WAVE_FORMAT_MULAW 0x0007
	#define WAVE_FORMAT_DVI_ADPCM 0x0011
	#define WAVE_FORMAT_G723_ADPCM 0x0014
	#define WAVE_FORMAT_DSPGROUP_TRUESPEECH 0x0022
	#define WAVE_FORMAT_DOLBY_AC2 0x0030
	#define WAVE_FORMAT_GSM610 0x0031
	#define WAVE_FORMAT_MSNAUDIO 0x0032
	#define WAVE_FORMAT_MSG723 0x0042
	#define WAVE_FORMAT_MPEG 0x0050
	#define WAVE_FORMAT_MPEGLAYER3 0x0055
	#define WAVE_FORMAT_VOXWARE 0x0062
	#define WAVE_FORMAT_VOXWARE_RT29 0x0075 /* Voxware Inc */
	#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
	#define WAVE_FORMAT_MSAUDIO1 0x0160	// (ALSO DIVX)	Microsoft Corporation
	#define WAVE_FORMAT_DIVX 0x0161 // (same as WMA)	DivX Networks
	#define WAVE_FORMAT_IMC 0x0401 // Intel Music Coder
	#define WAVE_FORMAT_LIGOS_INDEO 0x0402
	#define WAVE_FORMAT_AC3 0x2000 // Dolby Laboratories, Inc 
	#define MEDIA_SUBTYPE_DTS 0x2001 // Dolby Laboratories, Inc 
	#define WAVE_FORMAT_OGG_VORBIS_1 0x674f // Ogg Vorbis 
	#define WAVE_FORMAT_OGG_VORBIS_2 0x6750  // Ogg Vorbis 
	#define WAVE_FORMAT_OGG_VORBIS_3 0x6751 // Ogg Vorbis 
	#define WAVE_FORMAT_OGG_VORBIS_1_PLUS 0x676f // Ogg Vorbis 
	#define WAVE_FORMAT_OGG_VORBIS_2_PLUS 0x6770 // Ogg Vorbis 
	#define WAVE_FORMAT_OGG_VORBIS_3_PLUS 0x6771 // Ogg Vorbis 
	#define WAVE_FORMAT_EXTENSIBLE 0xFFFE


	/* specific waveform format structure for PCM data */
	struct PCMWAVEFORMAT : public WAVEFORMAT {
		WORD wBitsPerSample;
	};

	/* general extended waveform format structure
	Use this for all NON PCM formats
	(information common to all formats)
	*/

	//
	//  New wave format development should be based on the
	//  WAVEFORMATEXTENSIBLE structure. WAVEFORMATEXTENSIBLE allows you to
	//  avoid having to register a new format tag with Microsoft. Simply
	//  define a new GUID value for the WAVEFORMATEXTENSIBLE.SubFormat field
	//  and use WAVE_FORMAT_EXTENSIBLE in the
	//  WAVEFORMATEXTENSIBLE.Format.wFormatTag field.
	//
	struct WAVEFORMATEXTENSIBLE : public WAVEFORMATEX {
		union {
			WORD wValidBitsPerSample;       /* bits of precision  */
			WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
			WORD wReserved;                 /* If neither applies, set to zero. */
		} Samples;
		DWORD dwChannelMask;      /* which channels are present in stream  */
		enum {
			SPEAKER_FRONT_LEFT = 0x1, 
			SPEAKER_FRONT_RIGHT = 0x2, 
			SPEAKER_FRONT_CENTER = 0x4, 
			SPEAKER_LOW_FREQUENCY = 0x8, 
			SPEAKER_BACK_LEFT = 0x10, 
			SPEAKER_BACK_RIGHT = 0x20, 
			SPEAKER_FRONT_LEFT_OF_CENTER = 0x40, 
			SPEAKER_FRONT_RIGHT_OF_CENTER = 0x80, 
			SPEAKER_BACK_CENTER = 0x100, 
			SPEAKER_SIDE_LEFT = 0x200, 
			SPEAKER_SIDE_RIGHT = 0x400, 
			SPEAKER_TOP_CENTER = 0x800, 
			SPEAKER_TOP_FRONT_LEFT = 0x1000, 
			SPEAKER_TOP_FRONT_CENTER = 0x2000, 
			SPEAKER_TOP_FRONT_RIGHT = 0x4000, 
			SPEAKER_TOP_BACK_LEFT = 0x8000, 
			SPEAKER_TOP_BACK_CENTER = 0x10000, 
			SPEAKER_TOP_BACK_RIGHT = 0x20000 
		};
		GUID SubFormat;
	};

	// http://www.microsoft.com/whdc/device/audio/Non-PCM.mspx


#define KSDATAFORMAT_SUBTYPE_ANALOG _T("{6dba3190-67bd-11cf-a0f7-0020afd156e4}")
#define KSDATAFORMAT_SUBTYPE_PCM _T("{00000001-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_IEEE_FLOAT _T("{00000003-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_DRM _T("{00000009-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_ALAW _T("{00000006-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_MULAW _T("{00000007-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_ADPCM _T("{00000002-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_MPEG _T("{00000050-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_WAVEFORMATEX _T("{00000000-0000-0010-8000-00aa00389b71}")
#define KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF _T("{00000092-0000-0010-8000-00aa00389b71}")

#define IS_VALID_WAVEFORMATEX_GUID(Guid) (!memcmp(((USHORT*)&KSDATAFORMAT_SUBTYPE_WAVEFORMATEX) + 1, ((USHORT*)(Guid)) + 1, sizeof(GUID) - sizeof(USHORT)))
#define EXTRACT_WAVEFORMATEX_ID(Guid)    (USHORT)((Guid)->Data1)


	// Microsoft MPEG audio WAV definition
	//
	/*  MPEG-1 audio wave format (audio layer only).   (0x0050)   */

	struct MPEG1WAVEFORMAT : public WAVEFORMATEX {
		WORD            fwHeadLayer;
		enum {
			ACM_MPEG_LAYER1 = (0x0001),
			ACM_MPEG_LAYER2 = (0x0002),
			ACM_MPEG_LAYER3 = (0x0004)
		};
		DWORD           dwHeadBitrate;
		WORD            fwHeadMode;
		enum { // ??? Richtig???
			ACM_MPEG_STEREO = (0x0001),
			ACM_MPEG_JOINTSTEREO = (0x0002),
			ACM_MPEG_DUALCHANNEL = (0x0004),
			ACM_MPEG_SINGLECHANNEL = (0x0008)
		};
		WORD            fwHeadModeExt;
		WORD            wHeadEmphasis;
		enum {
			ACM_HEAD_EMPHASIS_NONE = 1,
			ACM_HEAD_EMPHASIS_5015MS = 2,
			ACM_HEAD_EMPHASIS_RESERVED = 3,
			ACM_HEAD_EMPHASIS_CCITTJ17 = 4
		};
		WORD            fwHeadFlags;
		enum { // ??? Richtig???
			ACM_MPEG_PRIVATEBIT = (0x0001),
			ACM_MPEG_COPYRIGHT = (0x0002),
			ACM_MPEG_ORIGINALHOME = (0x0004),
			ACM_MPEG_PROTECTIONBIT = (0x0008),
			ACM_MPEG_ID_MPEG1 = (0x0010)
		};
		DWORD           dwPTSLow;
		DWORD           dwPTSHigh;
	};

	struct ADPCMWAVEFORMAT : public WAVEFORMATEX {
		WORD  wSamplesPerBlock;
		WORD  wNumCoef;
		struct ADPCMCOEFSET {
			short iCoef1;
			short iCoef2;
		} Coefficients[]; 
	}; 

	//
	// MPEG Layer3 WAVEFORMATEX structure
	// for WAVE_FORMAT_MPEGLAYER3 (0x0055)
	//
	#define MPEGLAYER3_WFX_EXTRA_BYTES   12

	// WAVE_FORMAT_MPEGLAYER3 format sructure
	//
	struct MPEGLAYER3WAVEFORMAT : public WAVEFORMATEX {
		WORD wID;
		enum {
			MPEGLAYER3_ID_UNKNOWN = 0,
			MPEGLAYER3_ID_MPEG = 1,
			MPEGLAYER3_ID_CONSTANTFRAMESIZE = 2
		};
		DWORD fdwFlags;
		enum {
			MPEGLAYER3_FLAG_PADDING_ISO = 0x00000000,
			MPEGLAYER3_FLAG_PADDING_ON = 0x00000001,
			MPEGLAYER3_FLAG_PADDING_OFF = 0x00000002
		};
		WORD nBlockSize;
		WORD nFramesPerBlock;
		WORD nCodecDelay;
	};
#pragma pack(pop)

	enum class ERET { ER_OK, ER_NOTCONSUMED, ER_SIZE, ER_EOF, ER_TOKEN_RIFF, ER_TOKEN_movi, ER_TOKEN_idx1 };

	CRIFFParser( class CIOService& ioservice);

	static bool Probe( BYTE buffer[12] );

protected:
	virtual bool ReadFile(int hti) override;
	//virtual bool ReadChunk(int hti, QWORD nChunkSize) override;

	//
	// #define's necessary because of case FCC():...
	//
public:
	#define FCC(fcc) ((((fcc) & 0x000000FF) << 24) | (((fcc) & 0x0000FF00) <<  8) | (((fcc) & 0x00FF0000) >>  8) | (((fcc) & 0xFF000000) >> 24))
	#define HIFCC(fcc) (WORD)((((fcc) & 0x000000FF) <<  8) | (((fcc) & 0x0000FF00) >>  8))
	#define LOFCC(fcc) (WORD)((((fcc) & 0x00FF0000) >>  8) | (((fcc) & 0xFF000000) >> 24))

	static inline WORD LOFCC_NUM( FOURCC fcc ) { return (WORD)(LOFCC(fcc)-0x3030); }
	static inline WORD HIFCC_NUM( FOURCC fcc ) { return (WORD)(HIFCC(fcc)-0x3030); }
	static inline bool ISLOFCC_NUM( FOURCC fcc ) { const BYTE* p = reinterpret_cast<const BYTE*>( &fcc ); return isdigit(p[2]) && isdigit(p[3]); }
	static inline bool ISHIFCC_NUM( FOURCC fcc ) { const BYTE* p = reinterpret_cast<const BYTE*>( &fcc ); return isdigit(p[0]) && isdigit(p[1]); }
	static inline bool ISFCC_ALNUM( FOURCC fcc ) { const BYTE* p = reinterpret_cast<const BYTE*>( &fcc ); return isprint(p[0]) && isalnum(p[1]) && isalnum(p[2]) && isalnum(p[3]); }
	static inline FOURCC SETHIFCC_NUM( FOURCC& fcc, WORD n ) { BYTE* p = reinterpret_cast<BYTE*>( &fcc ); p[1] = (BYTE)((n & 0x0f) + 0x30); p[0] = (BYTE)(((n & 0xf0) >> 4) + 0x30); return fcc; }

protected:
	bool ReadType( DWORD& fcc, int hti ) { return ReadBytes( &fcc, sizeof( fcc ), hti ); }
	bool ReadSize( DWORD& n, int hti );
	bool ReadTypeSize( FOURCC& fcc, DWORD& n, int hti ) { return ReadType( fcc, hti ) && ReadSize( n, hti ); }
	bool ReadSizeType( DWORD& n, FOURCC& fcc, int hti ) { return ReadSize( n, hti ) && ReadType( fcc, hti ); }

	ERET RIFFReadRiffChunk( int hti, DWORD nChunkSize, FOURCC fcc, WORD& nSegmentCount );

	ERET RIFFReadGenericChunk( int hti, DWORD nChunkSize, WORD nSegmentCount );
	ERET RIFFReadInfoListChunk( int hti, DWORD nChunkSize );
	ERET RIFFReadHandleCommonChunks( int hti, DWORD nChunkSize, FOURCC fcc );
	bool RIFFDumpWaveDispChunk( int hti, DWORD nSize );

	bool RIFFDumpGuidChunk( int hti, DWORD nSize );
//	bool RIFFDumpNameChunk( int hti, DWORD nSize );
//	bool RIFFDumpFileChunk( int hti, DWORD nSize );
	bool RIFFDumpVersChunk( int hti, DWORD nSize );

	static _tstring GetAudioFormat( DWORD nFormat );
};
