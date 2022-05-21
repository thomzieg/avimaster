#pragma once

#include "RIFFParser.h"

#pragma pack(push,1)

/* The AVI File Header LIST chunk should be padded to this size */
#define AVI_HEADERSIZE  2048                    // size of AVI header list
#define AVI_MAX_LEN (UINT_MAX-(1<<20)*16-AVI_HEADERSIZE)

struct  AVIMAINHEADER {
	DWORD  dwMicroSecPerFrame;     // frame display rate (or 0L)
	DWORD  dwMaxBytesPerSec;       // max. transfer rate
	DWORD  dwPaddingGranularity;   // pad to multiples of this size; normally 2K.
	DWORD  dwFlags;                // the ever-present flags
	enum { 
		AVIF_HASINDEX = 0x00000010, // Index at end of file?
		AVIF_MUSTUSEINDEX = 0x00000020,
		AVIF_ISINTERLEAVED = 0x00000100,
		AVIF_TRUSTCKTYPE = 0x00000800, // Use CKType to find key frames
		AVIF_WASCAPTUREFILE = 0x00010000,
		AVIF_COPYRIGHTED = 0x00020000 
	};
	DWORD  dwTotalFrames;          // # frames in first movi list
	DWORD  dwInitialFrames;
	DWORD  dwStreams;
	DWORD  dwSuggestedBufferSize;
	DWORD  dwWidth;
	DWORD  dwHeight;
	DWORD  dwReserved[4];
	double GetFPS() const { return 1.0/((double)(dwMicroSecPerFrame == 0 ? 1 : dwMicroSecPerFrame) / 1000000); }
	DWORD GetLengthMSec() const { return (DWORD)((double)dwMicroSecPerFrame / 1000 * dwTotalFrames + .5); }
	_tstring GetLengthString() const { return FramesToLengthString( dwTotalFrames, GetFPS() ); }
};

struct AVIEXTHEADER {
	DWORD   dwGrandFrames;          // total number of frames in the file
	DWORD   dwFuture[61];           // to be defined later
};

struct AVISTREAMHEADER {
	FOURCC fccType;      // stream type codes
	FOURCC fccHandler;
	DWORD  dwFlags;
	enum { 
		AVISF_DISABLED = 0x00000001, 
		AVISF_VIDEO_PALCHANGES = 0x00010000 
	};
	WORD   wPriority;
	WORD   wLanguage;
	DWORD  dwInitialFrames; // Audio skew. This member specifies how much to skew the audio data ahead of the video frames in interleaved files. Typically, this is about 0.75 seconds.
	DWORD  dwScale; // Time scale applicable for the stream. Dividing dwRate by dwScale gives the playback rate in number of samples per second. For video streams, this rate should be the frame rate. For audio streams, this rate should correspond to the audio block size (the nBlockAlign member of the WAVEFORMAT or PCMWAVEFORMAT structure), which for PCM (Pulse Code Modulation) audio reduces to the sample rate.
	DWORD  dwRate;       // dwRate/dwScale is stream tick rate in ticks/sec Rate in an integer format. To obtain the rate in samples per second, divide this value by the value in dwScale.
	DWORD  dwStart; // Sample number of the first frame of the AVI file. The units are defined by dwRate and dwScale. Normally, this is zero, but it can specify a delay time for a stream that does not start concurrently with the file. 
	DWORD  dwLength;  // Length of this stream. The units are defined by dwRate and dwScale.
	DWORD  dwSuggestedBufferSize; // Recommended buffer size, in bytes, for the stream. Typically, this member contains a value corresponding to the largest chunk in the stream. Using the correct buffer size makes playback more efficient. Use zero if you do not know the correct buffer size.
	DWORD  dwQuality; // Quality indicator of the video data in the stream. Quality is represented as a number between 0 and 10,000. For compressed data, this typically represents the value of the quality parameter passed to the compression software. If set to  - 1, drivers use the default quality value.
	DWORD  dwSampleSize; // Size, in bytes, of a single data sample. If the value of this member is zero, the samples can vary in size and each data sample (such as a video frame) must be in a separate chunk. A nonzero value indicates that multiple samples of data can be grouped into a single chunk within the file. For video streams, this number is typically zero, although it can be nonzero if all video frames are the same size. For audio streams, this number should be the same as the nBlockAlign member of the WAVEFORMAT or WAVEFORMATEX structure describing the audio.
	struct {
		short left;
		short top;
		short right;
		short bottom;
	} rcFrame;
	double GetFPS() const { return (double)dwRate / (dwScale == 0 ? 1 : dwScale); }
	DWORD GetLengthMSec() const { double f = GetFPS(); if ( f < 0.01 ) return 0; return (DWORD)(dwLength / f * 1000 + .5); };
	_tstring GetLengthString() const { return FramesToLengthString( dwLength, GetFPS() ); }
	bool IsVideoStream() const { return fccType == FCC('vids') || fccType == FCC('iavs'); }
	AVISTREAMHEADER() { memset( this, '\0', sizeof( AVISTREAMHEADER ) ); } // ACHTUNG funktioniert nur, solange da drin kein map<> oder sowas vorkommt!
};

// -----------------------------------------------------------------------------------------------

struct DIVXSTREAMCHAPTERS {
	WORD			mode;			//Chapter Mode, always 0
	WORD			numChapters;		// Number of Chapters
	struct DIVXSTREAMCHAPTERENTRY {
		WORD	entrySize;		// Size of Chapter Entry (multiple of 2)
		DWORD	chapterStartPos;	// Start position of Chapter The starting frame position of the chapter. When associated with video streams, the chapter position is the starting frame number. Ideally the chapter start point is a keyframe, but this isn't a requirement. When used with audio streams, the starting position is the sample number. 
		char	chapterName[1];		// Display name of the chapter
	} chapterEntry[1];	// First Chapter Entry, size unknown
};

struct DIVXSINGLEINDEX { // idxx Index Entry
	DWORD chunkId;	// currently only 'sb##'
	DWORD flags;	// Interpretation depends on chunkId
	DWORD filePos;	// Position in file of Chunk
	DWORD size;	// Size of Chunk
};

struct DIVXSUBTITLEFORMAT {
	WORD	mode;
	enum {
		DVX_SUBT_MODE_GRAPHIC_FULLRES = 0, // Graphic Subtitle Full Resolution 
		DVX_SUBT_MODE_GRAPHIC_HALFRES = 1, // Graphic Subtitle Half Resolution  
		DVX_SUBT_MODE_TEXT = 2 // Text Subtitle 
	};
	BYTE	remap[4];	// The four palette remap values
	DWORD	palette[4];	// Index 0 - Background Index 1 - Font, Index 2 - Outline, Index 3 - Antialias
};

struct DIVXSUBTITLESTREAM {
	DWORD	startingFrame;	// Frame to start displaying subtitle on
	WORD	duration;	// Number of frames to display subtitle.
	WORD	left;		// Left pixel pos of subtitle
	WORD	top;		// Top Line of subtitle
	WORD	right;		// right pixel pos of subtitle
	WORD	bot;		// Bottom Line of subtitle
	WORD	evenStartPos;	// Even line starting data pos
	WORD	oddStartPos;	// Odd line starting pos (can be nullptr)
	char	subtitleData[];	// Remainder of chunk is the subtitle data
};

// -----------------------------------------------------------------------------------------------


namespace my {
struct PALETTEENTRY
{
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};

#undef BI_RGB
#undef BI_RLE8
#undef BI_RLE4
#undef BI_BITFIELDS
#undef BI_JPEG
#undef BI_PNG

struct BITMAPINFOHEADER
{
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	//DWORD biCompression;
	enum struct BiCompression : DWORD { BI_RGB = 0L, BI_RLE8 = 1L, BI_RLE4 = 2L, BI_BITFIELDS = 3L, BI_JPEG  = 4L, BI_PNG = 5L } biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
};

typedef long FXPT2DOT30;

struct CIEXYZ
{
	FXPT2DOT30 ciexyzX;
	FXPT2DOT30 ciexyzY;
	FXPT2DOT30 ciexyzZ;
};

struct CIEXYZTRIPLE
{
	CIEXYZ  ciexyzRed;
	CIEXYZ  ciexyzGreen;
	CIEXYZ  ciexyzBlue;
};

struct BITMAPV4HEADER : public BITMAPINFOHEADER
{
	DWORD        bV4RedMask;
	DWORD        bV4GreenMask;
	DWORD        bV4BlueMask;
	DWORD        bV4AlphaMask;
	DWORD        bV4CSType;
	CIEXYZTRIPLE bV4Endpoints;
	DWORD        bV4GammaRed;
	DWORD        bV4GammaGreen;
	DWORD        bV4GammaBlue;
};

struct BITMAPV5HEADER : public BITMAPV4HEADER
{
	DWORD        bV5Intent;
	DWORD        bV5ProfileData;
	DWORD        bV5ProfileSize;
	DWORD        bV5Reserved;
}; 
} // end namespace my

struct AVIPALCHANGE 
{
    BYTE		bFirstEntry;	/* first entry to change */
    BYTE		bNumEntries;	/* # entries to change (0 if 256) */
    WORD		wFlags;		/* Mostly to preserve alignment... */
	my::PALETTEENTRY	peNew[];	/* New color specifications */
};

struct EXBMINFOHEADER : public my::BITMAPINFOHEADER 
{
	DWORD biExtDataOffset;
        /* Other stuff will go here */
 
        /* Format-specific information */
        /* biExtDataOffset points here */
};
 
struct JPEGINFOHEADER {
    /* compression-specific fields */
    /* these fields are defined for 'JPEG' and 'MJPG' */
	DWORD   JPEGSize;
	DWORD   JPEGProcess;

	/* Process specific fields */
	DWORD   JPEGColorSpaceID;
	enum {
		JPEG_Y = 1,        /* Y only component of YCbCr */
		JPEG_YCbCr = 2,        /* YCbCr as define by CCIR 601 */
		JPEG_RGB = 3        /* 3 component RGB */
	};
	DWORD   JPEGBitsPerSample;

	DWORD   JPEGHSubSampling;
	DWORD   JPEGVSubSampling;
};

struct HUFFYUV_BITMAPINFOHEADER : public my::BITMAPINFOHEADER {
	struct {
		BYTE method;
		BYTE bpp_override;
		BYTE unused[2];  
	} Extradata;
	BYTE Compressed_Huffmantables[];
};

struct ZLIBMSZH_BITMAPINFOHEADER : public my::BITMAPINFOHEADER { // http://home.pcisys.net/~melanson/codecs/lcl.txt
	BYTE unknown[4]; // always [4, 0, 0, 0]
 	BYTE imagetype;
	enum {
		yuv111 = 0,
		yuv422 = 1,
		rgb24 = 2,
		yuv411 = 3,
		yuv211 = 4,
		yuv420 = 5
	};
 	BYTE compression;
	enum {
		mszh_compression = 0,
		mszh_nocompression_zlib_hispeed_compression = 1,
		zlib_high_compression = 9,
		zlib_normal_compression = -1 // (zlib standard level)
	};
 	BYTE flags;
	enum { // Bitfeld
		multithread_used = 1,
		nullframe_insertion_used = 2,
		png_filter_used = 4 // (zlib only)
	};
 	BYTE codec;
	enum {
		MSZH = 1,
		ZLIB = 3
	};
};

//
// structure of old style AVI index
struct AVIINDEXENTRY {
	DWORD ckid;
	DWORD dwFlags;
	enum {
		AVIIF_LIST = 0x00000001,
		AVIIF_KEYFRAME = 0x00000010,
		AVIIF_FIRSTPART = 0x00000020L, // this frame is the start of a partial frame.
		AVIIF_LASTPART = 0x00000040L, // this frame is the end of a partial frame.
		AVIIF_MIDPART = (AVIIF_LASTPART|AVIIF_FIRSTPART),
		AVIIF_NO_TIME = 0x00000100,
		AVIIF_COMPRESSOR = 0x0FFF0000  // unused?// these bits are for compressor use
	};
	DWORD dwChunkOffset;		// Position of chunk
	DWORD dwChunkLength;		// Length of chunk
};

union TIMECODE {
	enum { TIMECODE_RATE_30DROP = 0 }; // gcc doesn't want it in the struct, wFrameRate
	struct {
		WORD   wFrameRate; // this MUST be zero, 0 == 30 drop frame
		WORD   wFrameFract; // fractional frame. full scale is always 0x10000
		DWORD  dwFrames;
	};
	DWORDLONG  qw;
};

// struct for all the SMPTE timecode info
//
struct TIMECODEDATA {
	TIMECODE time;
	DWORD dwSMPTEflags;
	enum {
		TIMECODE_SMPTE_BINARY_GROUP = 0x07,
		TIMECODE_SMPTE_COLOR_FRAME = 0x08
	};
	DWORD dwUser;
};

struct VideoPropHeader {
	DWORD VideoFormatToken;
	enum {
		FORMAT_UNKNOWN, 
		FORMAT_PAL_SQUARE, 
		FORMAT_PAL_CCIR_601,
		FORMAT_NTSC_SQUARE, 
		FORMAT_NTSC_CCIR_601
	};
	DWORD              VideoStandard;
	enum {
		STANDARD_UNKNOWN, 
		STANDARD_PAL, 
		STANDARD_NTSC, 
		STANDARD_SECAM
	};
	DWORD              dwVerticalRefreshRate; // 60 NTSC, 50 PAL
	DWORD              dwHTotalInT;
	DWORD              dwVTotalInLines;
	DWORD              dwFrameAspectRatio;  
	DWORD              dwFrameWidthInPixels;
	DWORD              dwFrameHeightInLines;
	DWORD              nbFieldPerFrame;
	struct VIDEO_FIELD_DESC {
		DWORD   CompressedBMHeight;
		DWORD   CompressedBMWidth;
		DWORD   ValidBMHeight;
		DWORD   ValidBMWidth;
		DWORD   ValidBMXOffset;
		DWORD   ValidBMYOffset;
		DWORD   VideoXOffsetInT;
		DWORD   VideoYValidStartLine;
	} FieldInfo[];
};

//
// ============ structures for new style AVI indexes =================
//

// meta structure of all avi indexes
//

// #define necessary
#define STDINDEXSIZE 0x4000 // includes fcc size
#define NUMINDEX(wLongsPerEntry) ((STDINDEXSIZE-32)/4/(wLongsPerEntry))
#define NUMINDEXFILL(wLongsPerEntry) ((STDINDEXSIZE/4) - NUMINDEX(wLongsPerEntry))

struct AVIMetaIndex { 
	WORD   wLongsPerEntry;
	BYTE   bIndexSubType;
	enum { // Index Sub Types
		AVI_INDEX_SUB_DEFAULT = 0x00, // index subtype codes
		AVI_INDEX_SUB_2FIELD = 0x01 // INDEX_OF_CHUNKS subtype codes
	};
	BYTE   bIndexType;
	enum { // Index Types
		AVI_INDEX_OF_INDEXES = 0x00,
		AVI_INDEX_OF_CHUNKS = 0x01,
		AVI_INDEX_OF_TIMED_CHUNKS = 0x02,
		AVI_INDEX_OF_SUB_2FIELD = 0x03,
		AVI_INDEX_IS_DATA = 0x80
	};
	DWORD  nEntriesInUse;
	FOURCC  dwChunkId; // 'tcdl' for Timecode discontinuity index
	enum { // dwOffset of AVI_INDEX_OF_CHUNKS (StandardIndex)
		AVISTDINDEX_DELTAFRAME = ( 0x80000000), // Delta frames have the high bit set
		AVISTDINDEX_SIZEMASK   = (~0x80000000)
	};
	enum { // Timecode Discontinuity Index
		TIMECODE_RATE_30DROP = 0 // this MUST be zero, 0 == 30 drop frame
	};
	union {
		struct { // AVI_INDEX_OF_INDEXES (sizeof() == 16376 == STDINDEXSIZE - 8)
			DWORD    dwReserved[3];     // must be 0
			struct {
				DWORDLONG qwOffset;    // 64 bit offset to sub index chunk
				DWORD    dwSize;       // 32 bit size of sub index chunk
				DWORD    dwDuration;   // time span of subindex chunk (in stream ticks)
			} aIndex[NUMINDEX(4)];
		} SuperIndex;
		struct { // AVI_INDEX_OF_CHUNKS
			DWORDLONG qwBaseOffset;     // base offset that all index intries are relative to
			DWORD    dwReserved_3;      // must be 0
			struct {
				DWORD dwOffset;       // 32 bit offset to data (points to data, not riff header)
				DWORD dwSize;         // 31 bit size of data (does not include size of riff header), bit 31 is deltaframe bit
			} aIndex[NUMINDEX(2)];
		} StandardIndex;
		struct { // AVI_INDEX_OF_CHUNKS; AVI_INDEX_OF_SUB_2FIELD
			DWORDLONG qwBaseOffset;     // offsets in aIndex array are relative to this
			DWORD    dwReserved3;       // must be 0
			struct {
				DWORD    dwOffset;
				DWORD    dwSize;         // size of all fields (bit 31 set for NON-keyframes)
				DWORD    dwOffsetField2; // offset to second field
			} aIndex[NUMINDEX(3)];
		} FieldIndex;
		struct { // AVI_INDEX_OF_TIMED_CHUNKS
			DWORDLONG qwBaseOffset;     // base offset that all index intries are relative to
			DWORD    dwReserved_3;      // must be 0
			struct {
				DWORD dwOffset;       // 32 bit offset to data (points to data, not riff header)
				DWORD dwSize;         // 31 bit size of data (does not include size of riff header) (high bit is deltaframe bit)
				DWORD dwDuration;     // how much time the chunk should be played (in stream ticks)
			} aIndex[NUMINDEX(3)];
			DWORD adwTrailingFill[NUMINDEXFILL(3)]; // to align struct to correct size
		} TimedIndex;
		struct { // AVI_INDEX_IS_DATA
			DWORD    dwReserved[3];     // must be 0
			TIMECODEDATA aIndex[NUMINDEX(sizeof(TIMECODEDATA)/sizeof(DWORD))];
		} TimecodeIndex;
		struct {  // AVI_INDEX_IS_DATA Timecode Discontinuity Index
			DWORD    dwReserved[3];     // must be 0
			struct { // Das sieht im ODML-Dokument anders aus???!!!
				DWORD    dwTick;           // stream tick time that maps to this timecode value
				TIMECODE time;
				DWORD    dwSMPTEflags;
				DWORD    dwUser;
				char    szReelId[12];
#if 0 // so stehts im Dokument
				QUADWORD  qwTick;       // time in terms of this streams's tick rate
				TIMECODE  timecode;     // timecode
				DWORD     dwUser;       // timecode user data
				struct {
					DWORD  ColorFrame : 4;       // Which frame in color sequence
					DWORD  ColorSequence : 4;    // Duration in frames of complete sequence
					DWORD  FilmFrame : 5;        // Offset into pull-down sequence
					DWORD  FilmSequenceType : 3; // One of FILM_SEQUENCE_XXX defines
					DWORD  Reserved : 16;        // Future use - set to 0
				} Flags;
				char     szReelId[32]; // source id
#endif
			} aIndex[NUMINDEX(7)];
			DWORD adwTrailingFill[NUMINDEXFILL(7)]; // to align struct to correct size
		} TimecodeDiscontinuityIndex;
	};
};


//#define Valid_SUPERINDEX(pi) (*(DWORD *)(&((pi)->wLongsPerEntry)) == (4 | (AVI_INDEX_OF_INDEXES << 24)))

#if 0
// structure of a super index (INDEX_OF_INDEXES)
//
typedef struct _avisuperindex {
//	FOURCC   fcc;               // 'indx'
//	UINT     cb;                // size of this structure
	WORD     wLongsPerEntry;    // ==4
	BYTE     bIndexSubType;     // ==0 (frame index) or AVI_INDEX_SUB_2FIELD 
	BYTE     bIndexType;        // ==AVI_INDEX_OF_INDEXES
	DWORD    nEntriesInUse;     // offset of next unused entry in aIndex
	DWORD    dwChunkId;         // chunk ID of chunks being indexed, (i.e. RGB8)
	DWORD    dwReserved[3];     // must be 0
	struct _avisuperindex_entry {
		DWORDLONG qwOffset;    // 64 bit offset to sub index chunk
		DWORD    dwSize;       // 32 bit size of sub index chunk
		DWORD    dwDuration;   // time span of subindex chunk (in stream ticks)
	} aIndex[NUMINDEX(4)];
} AVISUPERINDEX;


// struct of a standard index (AVI_INDEX_OF_CHUNKS)
//
typedef struct _avistdindex_entry {
   DWORD dwOffset;       // 32 bit offset to data (points to data, not riff header)
   DWORD dwSize;         // 31 bit size of data (does not include size of riff header), bit 31 is deltaframe bit
   } AVISTDINDEX_ENTRY;
#define AVISTDINDEX_DELTAFRAME ( 0x80000000) // Delta frames have the high bit set
#define AVISTDINDEX_SIZEMASK   (~0x80000000)

typedef struct _avistdindex {
//	FOURCC   fcc;               // 'indx' or '##ix'
//	UINT     cb;                // size of this structure
	WORD     wLongsPerEntry;    // ==2
	BYTE     bIndexSubType;     // ==0
	BYTE     bIndexType;        // ==AVI_INDEX_OF_CHUNKS
	DWORD    nEntriesInUse;     // offset of next unused entry in aIndex
	DWORD    dwChunkId;         // chunk ID of chunks being indexed, (i.e. RGB8)
	DWORDLONG qwBaseOffset;     // base offset that all index intries are relative to
	DWORD    dwReserved_3;      // must be 0
	AVISTDINDEX_ENTRY aIndex[NUMINDEX(2)];
} AVISTDINDEX;

// struct of a time variant standard index (AVI_INDEX_OF_TIMED_CHUNKS)
//
typedef struct _avitimedindex_entry {
   DWORD dwOffset;       // 32 bit offset to data (points to data, not riff header)
   DWORD dwSize;         // 31 bit size of data (does not include size of riff header) (high bit is deltaframe bit)
   DWORD dwDuration;     // how much time the chunk should be played (in stream ticks)
   } AVITIMEDINDEX_ENTRY;

typedef struct _avitimedindex {
//   FOURCC   fcc;               // 'indx' or '##ix'
//   UINT     cb;                // size of this structure
   WORD     wLongsPerEntry;    // ==3
   BYTE     bIndexSubType;     // ==0
   BYTE     bIndexType;        // ==AVI_INDEX_OF_TIMED_CHUNKS
   DWORD    nEntriesInUse;     // offset of next unused entry in aIndex
   DWORD    dwChunkId;         // chunk ID of chunks being indexed, (i.e. RGB8)
   DWORDLONG qwBaseOffset;     // base offset that all index intries are relative to
   DWORD    dwReserved_3;      // must be 0
   AVITIMEDINDEX_ENTRY aIndex[NUMINDEX(3)];
   DWORD adwTrailingFill[NUMINDEXFILL(3)]; // to align struct to correct size
   } AVITIMEDINDEX;

// structure of a timecode stream
//
typedef struct _avitimecodeindex {
//   FOURCC   fcc;               // 'indx' or '##ix'
//   UINT     cb;                // size of this structure
   WORD     wLongsPerEntry;    // ==4
   BYTE     bIndexSubType;     // ==0
   BYTE     bIndexType;        // ==AVI_INDEX_IS_DATA
   DWORD    nEntriesInUse;     // offset of next unused entry in aIndex
   DWORD    dwChunkId;         // 'time'
   DWORD    dwReserved[3];     // must be 0
   TIMECODEDATA aIndex[NUMINDEX(sizeof(TIMECODEDATA)/sizeof(LONG))];
   } AVITIMECODEINDEX;

// structure of a timecode discontinuity list (when wLongsPerEntry == 7)
//
typedef struct _avitcdlindex_entry {
    DWORD    dwTick;           // stream tick time that maps to this timecode value
    TIMECODE time;
    DWORD    dwSMPTEflags;
    DWORD    dwUser;
    TCHAR    szReelId[12];
    } AVITCDLINDEX_ENTRY;

typedef struct _avitcdlindex {
   FOURCC   fcc;               // 'indx' or '##ix'
   UINT     cb;                // size of this structure
   WORD     wLongsPerEntry;    // ==7 (must be 4 or more all 'tcdl' indexes
   BYTE     bIndexSubType;     // ==0
   BYTE     bIndexType;        // ==AVI_INDEX_IS_DATA
   DWORD    nEntriesInUse;     // offset of next unused entry in aIndex
   DWORD    dwChunkId;         // 'tcdl'
   DWORD    dwReserved[3];     // must be 0
   AVITCDLINDEX_ENTRY aIndex[NUMINDEX(7)];
   DWORD adwTrailingFill[NUMINDEXFILL(7)]; // to align struct to correct size
   } AVITCDLINDEX;

typedef struct _avifieldindex_chunk {
//   FOURCC   fcc;               // 'ix##'
//   DWORD    cb;                // size of this structure
   WORD     wLongsPerEntry;    // must be 3 (size of each entry in
                               // aIndex array)
   BYTE     bIndexSubType;     // AVI_INDEX_2FIELD
   BYTE     bIndexType;        // AVI_INDEX_OF_CHUNKS
   DWORD    nEntriesInUse;     //
   DWORD    dwChunkId;         // '##dc' or '##db'
   DWORDLONG qwBaseOffset;     // offsets in aIndex array are relative to this
   DWORD    dwReserved3;       // must be 0
   struct _avifieldindex_entry {
      DWORD    dwOffset;
      DWORD    dwSize;         // size of all fields
                               // (bit 31 set for NON-keyframes)
      DWORD    dwOffsetField2; // offset to second field
   } aIndex[];
} AVIFIELDINDEX, * PAVIFIELDINDEX;
#endif


#undef DVINFO
struct DVINFO
{
	union {
		struct {
			DWORD af_size : 6; // Audio Frame size
			DWORD res1 : 1;
			DWORD lf : 1; // Audio Locked Flag (0: Locked, 1: Unlocked)

			DWORD audio_mode : 4;
			DWORD pa : 1;
			DWORD chn : 2; // Channels per Audio Block (0: 1 channel/block, 1: 2 channels/block)
			DWORD sm : 1; // Stereo Mode (0: Stereo, 1: Mono)

			DWORD stype : 5;
			DWORD pal_ntsc : 1; // (0: 60, 1: 50)
			DWORD ml : 1;
			DWORD res2 : 1;

			DWORD qu : 3; // Quantization (0: 16 bit linear, 1: 12 bit non-linear) 
			DWORD smp : 3; // Sampling Frequency (0: 48kHz, 2: 32kHz)
			DWORD tc : 1;
			DWORD ef : 1;
		} bfDVAAuxSrc;
		DWORD dwDVAAuxSrc;
	};
	union {
		struct {
			DWORD ss : 2;
			DWORD cmp : 2;
			DWORD isr : 2;
			DWORD cgms : 2; // Copy Generation Management System (0: Copy Permitted)

			DWORD insert_ch : 3;
			DWORD rec_mode : 3; // Recording Mode (1: Original)
			DWORD rec_end : 1;
			DWORD rec_st : 1;

			DWORD speed : 7; // VTR playback speed
			DWORD drf : 1;

			DWORD genre : 7;
			DWORD res1: 1;
		} bfDVAAuxCtl;
		DWORD dwDVAAuxCtl;
	};
	union {
		struct {
			DWORD af_size : 6;
			DWORD res1 : 1;
			DWORD lf : 1;

			DWORD audio_mode : 4;
			DWORD pa : 1;
			DWORD chn : 2;
			DWORD sm : 1;

			DWORD stype : 5;
			DWORD pal_ntsc : 1;
			DWORD ml : 1;
			DWORD res2 : 1;

			DWORD qu : 3;
			DWORD smp : 3;
			DWORD tc : 1;
			DWORD ef : 1;
		} bfDVAAuxSrc1;
		DWORD dwDVAAuxSrc1;
	};
	union {
		struct {
			DWORD ss : 2;
			DWORD cmp : 2;
			DWORD isr : 2;
			DWORD cgms : 2;

			DWORD insert_ch : 3;
			DWORD rec_mode : 3;
			DWORD rec_end : 1;
			DWORD rec_st : 1;

			DWORD speed : 7;
			DWORD drf : 1;

			DWORD genre : 7;
			DWORD res1: 1;
		} bfDVAAuxCtl1;
		DWORD dwDVAAuxCtl1;
	};
	union {
		struct {
			DWORD res1 : 8; // tv_channel

			DWORD res2 : 4; // tv_channel
			DWORD clf : 2;
			DWORD en : 1;
			DWORD black_white : 1; // (1: color)

			DWORD stype : 5; // Signal Type
			DWORD pal_ntsc : 1; // (0: 60, 1: 50)
			DWORD source_code : 2;

			DWORD tuner_category : 8;
		} bfDVVAuxSrc;
		DWORD dwDVVAuxSrc;
	};
	union {
		struct {
			DWORD ss : 2;
			DWORD cmp : 2;
			DWORD isr : 2;
			DWORD cgms : 2; // Copy Generation Management System (0: Copy Permitted)

			DWORD disp : 3; // Display Mode (0: 4:3, 2: 16:9)
			DWORD res2 : 1;
			DWORD rec_mode : 2; // Recording Mode (1: Original)
			DWORD res1 : 1;
			DWORD rec_st : 1;

			DWORD bcsys : 2; // Broadcast System (Widescreen Signaling Information)
			DWORD sc : 1;
			DWORD st : 1;
			DWORD il : 1;
			DWORD fc : 1;
			DWORD fs : 1;
			DWORD ff : 1;

			DWORD genre : 7;
			DWORD res3: 1;
		} bfDVVAuxCtl;
		DWORD dwDVVAuxCtl;
	};
    DWORD dwDVReserved[ 2 ];
};

union dv_aaux_as_pc4_t {
	struct {
		BYTE qu        :3; // quantization: 0=16bits linear, 1=12bits non-linear, 2=20bits linear, others reserved
		BYTE smp       :3; // sampling frequency: 0=48kHz, 1=44,1 kHz, 2=32 kHz
		BYTE tc        :1; // time constant of emphasis: 1=50/15us, 0=reserved
		BYTE ef        :1; // emphasis: 0=on, 1=off
	};
	BYTE b;
};


// ************************************************************************************

struct AVIStreamElement {
	DWORD m_size;
	union {
		my::BITMAPINFOHEADER* pbh;
		WAVEFORMATEX* pwf;
		DVINFO *pdi;
		AVIMetaIndex* pmi;
		DIVXSUBTITLEFORMAT* pst;
		BYTE* pb;
	} data;
	AVIStreamElement() { m_size = 0; data.pb = nullptr; }
	~AVIStreamElement() { delete[] data.pb; }
};

struct AVIStreamHeader {
	AVISTREAMHEADER  m_AVIStrhChunk;
	AVIStreamElement m_AVIStrfChunk;
	AVIStreamElement m_AVIStrdChunk;
	const AVIMetaIndex* m_pAVIMetaIndex;
	std::string m_sName; // 'strn'
	struct Counter {
		DWORD frames; DWORD oldindex; DWORD odmlindex; DWORD keyframes; DWORD droppedframes;
	};
	typedef std::map< FOURCC /**/, Counter > MCOUNTER;
	MCOUNTER mFOURCCCounter;
	DWORD GetCounterFrames() const { // Get Counter for all Non-Index-frames (and non-##pc)
		DWORD nCount = 0;
		MCOUNTER::const_iterator it = mFOURCCCounter.begin();
		for ( ; it != mFOURCCCounter.end(); ++it ) {
			if ( it->first != FCC('indx') && LOFCC(it->first) != LOFCC(FCC('##ix')) && LOFCC(it->first) != LOFCC(FCC('##pc')) && HIFCC(it->first) != HIFCC(FCC('ix##')) )
				nCount += it->second.frames;
		}
		return nCount;
	}
	DWORD GetCounterIXFrames() const { // Get Counter for all 'ix##'-Index-frames
		DWORD nCount = 0;
		MCOUNTER::const_iterator it = mFOURCCCounter.begin();
		for ( ; it != mFOURCCCounter.end(); ++it ) {
			if ( HIFCC(it->first) == HIFCC(FCC('ix##')) )
				nCount += it->second.frames;
		}
		return nCount;
	}

	QWORD nStreamSize; 
	DWORD nInitialFrames;
	DWORD nMaxElementSize;
	WORD nSrcStreamNr; // After deleting this is the original stream number
	double GetFPS() const { return m_AVIStrhChunk.GetFPS(); }
	DWORD GetLengthMSec() const { return m_AVIStrhChunk.GetLengthMSec(); }
//	void GetLengthString( ostream& s ) const { m_AVIStrhChunk.GetLengthString(); }
	bool IsVideoStream() const { return m_AVIStrhChunk.IsVideoStream(); }
	AVIStreamHeader() { nStreamSize = 0; nInitialFrames = 0; nMaxElementSize = 0; nSrcStreamNr = 0; m_pAVIMetaIndex = nullptr; }
	~AVIStreamHeader() { delete m_pAVIMetaIndex; }
};

#pragma pack(pop)


class CAVIParser : public CRIFFParser
{
public:
	CAVIParser( class CIOService& ioservice);
	virtual ~CAVIParser();

	void Parse( const _tstring& sIFilename);
	static bool Probe( BYTE buffer[12] );

	struct DVFRAMEINFO {
		bool b16to9;
		bool bPAL;
		_tstring sTimecode;
		_tstring sCreatedDate, sCreatedTime;
		dv_aaux_as_pc4_t aaux_as_pc4;
		// BYTE vaux_vs_pc3;
	};

	bool Sample_DVPayload( int hti, DVFRAMEINFO* pi, DWORD nFrameSize, const BYTE* pFrame ) const;

public:
	struct AVIFileHeader;
	class Streams {
		static const FOURCC fccOrder[];
		AVIFileHeader* m_pAVIDesc;
	public:
		typedef std::map<WORD /*nStream*/, AVIStreamHeader*> mStreamNrAVIStreamHeader;
		typedef std::map<FOURCC /*fccType*/,  mStreamNrAVIStreamHeader> mmFCCTypeStreamNrAVIStreamHeader;
		mmFCCTypeStreamNrAVIStreamHeader m_mmStreams;
		const CAVIParser* m_pParser;
		bool Create( AVIStreamHeader* pash );
		bool Renumber();
		bool Erase( int hti );
		Streams( const CAVIParser* pParser, AVIFileHeader* pAVIDesc ) { m_pParser = pParser; m_pAVIDesc = pAVIDesc; }
	};

	struct AVIFrameEntry {
		FOURCC m_fcc;
		DWORD m_nOffset; // base 'movi'
		DWORD m_nSize;
		DWORD m_nFlags; // from 'idx1'
//		WORD  m_nCount; // how many Index-Entries refer here
		QWORD m_nFilePos;
		const BYTE* m_pData;
		WORD  m_nSegment;
//		const AVIStreamHeader* m_pAVIStreamHeader;
		AVIFrameEntry( WORD nSegment, FOURCC fcc, DWORD nOffset, DWORD nSize, QWORD nFilePos )
		{
			m_nSegment = nSegment;
			m_fcc = fcc;
			m_nFilePos = nFilePos; // Start Payload in File
			m_nOffset = nOffset; // Start Payload
			m_nSize = nSize; // Size Payload
			m_nFlags = 0; // ...
			m_pData = nullptr;
		}
		AVIFrameEntry( WORD nSegment, FOURCC fcc, DWORD nSize, const BYTE* pData )
		{
			m_nSegment = nSegment;
			m_fcc = fcc;
			m_nFilePos = 0; // Start Payload in File
			m_nOffset = 0; 
			m_nSize = nSize; // Size Payload
			m_nFlags = 0; // ...
			m_pData = pData;
		}
		~AVIFrameEntry() { delete m_pData; }
		bool IsStreamEqual( WORD nStream ) const { return HIFCC_NUM( m_fcc ) == nStream; }
		FOURCC RenumberStream( std::pair<WORD, WORD> StreamNr ) { if ( HIFCC_NUM(m_fcc) == StreamNr.first ) return SETHIFCC_NUM( m_fcc, StreamNr.second ); else return m_fcc; }
	};
	struct AVIIndexEntry {
		WORD  m_nSegment;
//		FOURCC m_fcc;
		DWORD m_nOffset; // base 'movi'
		DWORD m_nSize;
		DWORD m_nFlags; // from 'idx1'
		QWORD m_nMoviFilePos;
		int  m_nAssigned;
		_tstring m_sMessage;
#ifdef _DEBUG
		bool  m_bIdx1;
#endif
		AVIIndexEntry( WORD nSegment, const AVIMetaIndex *mi, WORD i, QWORD nFileMoviPos )
		{
			ASSERT( mi->StandardIndex.qwBaseOffset + mi->StandardIndex.aIndex[i].dwOffset - nFileMoviPos < ((QWORD)1 << 32) );
			ASSERT( mi->bIndexType == AVIMetaIndex::AVI_INDEX_OF_CHUNKS && mi->bIndexSubType == AVIMetaIndex::AVI_INDEX_SUB_DEFAULT );
			m_nOffset = (DWORD)(mi->StandardIndex.qwBaseOffset + mi->StandardIndex.aIndex[i].dwOffset - nFileMoviPos);
			m_nSize = mi->StandardIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_SIZEMASK;
			m_nFlags = (DWORD)((mi->StandardIndex.aIndex[i].dwSize & AVIMetaIndex::AVISTDINDEX_DELTAFRAME) == 0 ? AVIINDEXENTRY::AVIIF_KEYFRAME : 0);
			m_nSegment = nSegment;
			m_nAssigned = 0;
			m_nMoviFilePos = nFileMoviPos + m_nOffset;
#ifdef _DEBUG
			m_bIdx1 = false;
#endif
		}
		AVIIndexEntry( const AVIINDEXENTRY *ie, int nSkew, QWORD nFileMoviPos )
		{
			ASSERT( ie->dwChunkOffset >= 4 );
			ASSERT( ((DWORD)(ie->dwChunkOffset - nSkew)) < (DWORD)0x80000000 ); // stimmt nicht ganz...
			m_nOffset = ie->dwChunkOffset - nSkew;
			m_nSize = ie->dwChunkLength;
			m_nFlags = ie->dwFlags;
			m_nSegment = 1;
			m_nAssigned = 0;
			m_nMoviFilePos = nFileMoviPos + m_nOffset;
#ifdef _DEBUG
			m_bIdx1 = true;
#endif
		}
//		bool IsAssigned( const AVIIndexEntry* pie ) const { return pie->m_nAssigned > 0; }
	};

public:
	struct AVIWriteIndexEntry {
		struct MetaIndex {
			MetaIndex() { nCount = 0; nFilePos = 0; }
			QWORD nFilePos;
			WORD nCount;
		};
		std::vector< MetaIndex > vMetaIndex;
		AVIWriteIndexEntry() { fcc = 0; nCount = 0; nFilePos = 0; nEntryStartIndx = 0; bInit = true; }
		FOURCC fcc;
		WORD nCount;
		DWORD nEntryStartIndx;
		QWORD nFilePos;
		bool bInit;
	};

	typedef std::vector<AVIFrameEntry*> vAVIFRAMEENTRIES;
	typedef std::vector<AVIWriteIndexEntry> vAVIWRITEINDEXENTRIES;
	typedef std::vector<AVIIndexEntry*> vAVIINDEXENTRIES;
	typedef std::map<FOURCC/*fcc*/, vAVIINDEXENTRIES*> mvAVIINDEXENTRIES;

	struct AVIFileHeader {
		AVIFileHeader( const CAVIParser* pParser ) : m_Streams( pParser, this ) { m_bExtHeader = false; m_bMustUseIndex = false; m_bIsInterleaved = false; m_bIntraFrameIndex = false; m_bHasIdx1 = false; m_pDVFrameSample = nullptr; }
		~AVIFileHeader() { delete m_pDVFrameSample; }
		AVIMAINHEADER m_AVIMainHeader;
		AVIEXTHEADER  m_AVIExtHeader;
		bool m_bExtHeader, m_bIsInterleaved, m_bMustUseIndex, m_bIntraFrameIndex, m_bHasIdx1;
		std::vector<AVIStreamHeader *> m_vpAVIStreamHeader; // gehört nach class Streams?
		mvAVIINDEXENTRIES m_mvpIndexEntries;
		vAVIFRAMEENTRIES m_vpFrameEntries;
		//vAVIWRITEINDEXENTRIES m_vWriteIndexEntry; // per stream
		DVFRAMEINFO* m_pDVFrameSample;
		Streams m_Streams;
		WORD GetStreamCount() const { return (WORD)m_vpAVIStreamHeader.size(); };
	};


public:
	virtual bool ReadFile( int hti ) override;
	//virtual bool ReadChunk(int hti, QWORD nChunkSize) override;

	inline bool ISFCC_VALID( FOURCC fcc ) const
	{
		return (fcc == FCC('LIST') || fcc == FCC('JUNK') || fcc == FCC('RIFF') || fcc == FCC('rec ') || fcc == FCC('idx1') 
				|| ((LOFCC(fcc) == LOFCC(FCC('##__')) || LOFCC(fcc) == LOFCC(FCC('##32')) && ISHIFCC_NUM(fcc) && HIFCC_NUM(fcc) < m_pAVIDesc->GetStreamCount())
#if 0
				|| LOFCC(fcc) == LOFCC(FCC('##wb')) || LOFCC(fcc) == LOFCC(FCC('##db'))
				|| LOFCC(fcc) == LOFCC(FCC('##ix')) || LOFCC(fcc) == LOFCC(FCC('##dc')) 
				|| LOFCC(fcc) == LOFCC(FCC('##IV')) || LOFCC(fcc) == LOFCC(FCC('##pc'))  
				|| LOFCC(fcc) == LOFCC(FCC('##iv')) || LOFCC(fcc) == LOFCC(FCC('##tx'))  
				|| LOFCC(fcc) == LOFCC(FCC('##HF')) || LOFCC(fcc) == LOFCC(FCC('##YV'))				
#else
				|| (isalpha( ((BYTE*)&fcc)[2] ) && isalpha( ((BYTE*)&fcc)[3] ) && ISHIFCC_NUM(fcc) && HIFCC_NUM(fcc) < m_pAVIDesc->GetStreamCount())) // '##wb'
				|| (isalpha( ((BYTE*)&fcc)[0] ) && isalpha( ((BYTE*)&fcc)[1] ) && ISLOFCC_NUM(fcc) && LOFCC_NUM(fcc) < m_pAVIDesc->GetStreamCount())); // 'ix##'
#endif
	}


protected:
	bool CheckConsistency( int hti ) const;
	void CalcVectors();

	ERET RIFFReadRiffChunk( int hti, DWORD nChunkSize, FOURCC fcc, WORD& nSegmentCount );
	ERET RIFFReadAvi_Chunk( int hti, DWORD nChunkSize, WORD nSegmentCount );
	ERET RIFFReadAvi_HdrlChunk( int hti, DWORD nChunkSize );
	ERET RIFFReadAvi_StrhChunk( int hti, DWORD nChunkSize );
	ERET RIFFReadAvi_MoviChunk( int hti, DWORD nChunkSize, WORD nSegment, QWORD nReadFileMoviPos );
	bool RIFFReadAvi_MoviChunkSearchFCC( int hti, DWORD nChunkSize, FOURCC& fcc, DWORD& nOffset );
	ERET RIFFReadAvi_Idx1Chunk( int hti, DWORD nChunkSize, QWORD nReadFileMoviPos );

	ERET RIFFParseIndxChunk( int hti, const AVIMetaIndex* mi, QWORD nFilePos, DWORD nSize, WORD nStream, bool bHeader ); // ODML 'ix##' 'indx'

	void RIFFDumpAvi_MoviPALChangeEntry( int hti, const AVIPALCHANGE *pc, DWORD nSize ) const;
	void RIFFDumpAvi_MoviSubtitleEntry( int hti, const DIVXSUBTITLESTREAM *p, DWORD nSize, WORD nMode ) const;
	void RIFFDumpAvi_AvihHeader( int hti, const AVIMAINHEADER* mh, DWORD nSize ) const;
	void RIFFDumpAvi_StrhHeader( int hti, const AVISTREAMHEADER* sh, DWORD nSize ) const;
	void RIFFDumpAvi_StrfHeader( int hti, const AVISTREAMHEADER* sh, const AVIStreamElement *sf ) const;
	void RIFFDumpAvi_VprpHeader( int htitmp, const VideoPropHeader* p, DWORD nSize ) const;
	void RIFFDumpAvi_DVINFO( int hti, const DVINFO* di, DWORD nSize ) const;
	void RIFFDumpAvi_StrfVidsHeaderBitmapinfo( int hti, const my::BITMAPINFOHEADER* bh, DWORD nSize ) const;
	void RIFFDumpAvi_StrfVidsHeaderExtBitmapinfo( int hti, const my::BITMAPV5HEADER* bh, DWORD nSize ) const;
	bool RIFFCheckAvi_IndxChunk( int hti, const AVIMetaIndex* mi, bool bHeader ) const;
	void RIFFDumpAvi_DVPayload( int hti, DWORD nFrameSize, const BYTE* pData ) const;
public:
	void RIFFDumpAvi_IndxChunk( int hti, const AVIMetaIndex *mi, QWORD nFilePos, DWORD nSize ) const;
	void RIFFDumpAvi_Idx1Entry( int hti, const AVIINDEXENTRY* ie, QWORD nReadFileMoviPos, DWORD nSize ) const;
	void RIFFDumpAvi_StrfAudsHeader( int hti, const WAVEFORMATEX* wf, DWORD nSize, const AVISTREAMHEADER* pash ) const;

protected:
	void DumpWriteIndexEntries( int hti, const vAVIINDEXENTRIES *ie, DWORD nIndex ) const;
	void DumpWriteIndexEntry( int hti, const AVIIndexEntry* ie, DWORD nIndex, bool bMessage ) const;

	_tstring GetVideoFormat( FOURCC fcc ) const;
	
	void CheckIndexEntry( mvAVIINDEXENTRIES::const_iterator pI );
	bool AssignFramesIndex( int hti );
	bool ExtractAudioFromDVFrame( int hti, DWORD nFrameSize, BYTE** ppAudio, DWORD& nAudioSize );
	bool ParseMP4Frame( int hti, DWORD nChunkSize );
	//bool GetFramesFromIndex();

public:
//protected:
	AVIFileHeader* m_pAVIDesc;

	bool m_bIsAVI, m_bPaletteChange, m_bHasSubtitles;

public:
	static const WORD m_nMinHeaderSize;
};
