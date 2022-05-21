#pragma once

#include "Parser.h"

class CASFParser : public CParser
{
public:
	CASFParser( class CIOService& ioservice );

	static bool Probe( BYTE buffer[12] );

protected:
	virtual bool ReadFile( int hti ) override;

	virtual DWORD ParserCallback( const std::string& sGuid, DWORD nSize, const BYTE* buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const override;

	inline _tstring Length100nsToString( QWORD length ) const { _tstringstream ss; ss.fill( '0' ); ss.flags( std::ios::right ); ss << (int)(length / 10000000 / 60 / 60) << ':' << std::setw(2) << (int)(length / 10000000 / 60)%60 << ':' << std::setw(2) << (int)(length / 10000000)%60; return ss.str(); }
	inline _tstring Length1msToString( QWORD length ) const { _tstringstream ss; ss.fill( '0' ); ss.flags( std::ios::right ); ss << (int)(length / 1000 / 60 / 60) << ':' << std::setw(2) << (int)(length / 1000 / 60)%60 << ':' << std::setw(2) << (int)(length / 1000)%60; return ss.str(); }

	virtual bool ReadChunk( int hti, QWORD nChunkSize ) override;
	bool DumpSimpleIndexObject( int hti, DWORD nSize );
	bool DumpIndexObject( int hti, DWORD nSize );
	bool DumpHeaderObject( int hti, DWORD nSize );
	bool DumpDataObject( int hti, QWORD nSize, QWORD nDeclaredSize );
	bool DumpMediaObjectIndexObject( int hti, DWORD nSize );
	bool DumpTimecodeIndexObject( int hti, DWORD nSize );
	bool DumpXMPObject(int hti, DWORD nSize);

	bool DumpExtendedStreamPropertiesObject( int hti, DWORD nSize );
	bool DumpLanguageListObject( int hti, DWORD nSize );
	bool DumpMetadataObject( int hti, DWORD nSize );
	bool DumpMetadataLibraryObject( int hti, DWORD nSize );
	bool DumpPaddingObject( int hti, DWORD nSize );
	bool DumpWrongPaddingObject( int hti, DWORD nSize );
	bool DumpStreamPrioritizationObject( int hti, DWORD nSize );
	bool DumpIndexParametersObject( int hti, DWORD nSize );
	bool DumpAdvancedMutualExclusionObject( int hti, DWORD nSize );
	bool DumpGroupMutualExclusionObject( int hti, DWORD nSize );
	bool DumpBandwidthSharingObject( int hti, DWORD nSize );
	bool DumpMediaObjectIndexParametersObject( int hti, DWORD nSize );
	bool DumpTimecodeIndexParametersObject( int hti, DWORD nSize );
	bool DumpCompatibilityObject( int hti, DWORD nSize );
	bool DumpAdvancedContentEncryptionObject( int hti, DWORD nSize );

	// Header Object
	bool DumpFilePropertiesObject( int hti, DWORD nSize );
	bool DumpHeaderExtensionObject( int hti, DWORD nSize );
	bool DumpContentDescriptionObject( int hti, DWORD nSize );
	bool DumpStreamPropertiesObject( int hti, DWORD nSize );
	bool DumpStreamPropertiesObject( int hti, DWORD nSize, const BYTE* buffer ) const;
	bool DumpCodecListObject( int hti, DWORD nSize );
	bool DumpExtendedContentDescriptionObject( int hti, DWORD nSize );
	bool DumpBitrateMutualExclusionObject( int hti, DWORD nSize );
	bool DumpErrorCorrectionObject( int hti, DWORD nSize );
	bool DumpStreamBitratePropertiesObject( int hti, DWORD nSize );
	bool DumpContentBrandingObject( int hti, DWORD nSize );
	bool DumpContentEncryptionObject( int hti, DWORD nSize );
	bool DumpExtendedContentEncryptionObject( int hti, DWORD nSize );
	bool DumpDigitalSignatureObject( int hti, DWORD nSize );
	bool DumpScriptCommandObject( int hti, DWORD nSize );
	bool DumpMarkerObject( int hti, DWORD nSize );
	bool DumpIndexParameterPlaceholderObject( int hti, DWORD nSize );

	DWORD DumpTypeParserCallback( DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& vstack, DWORD nField ) const;
	DWORD DumpStreamPropertiesObjectParserCallback( DWORD nSize, const BYTE *buffer, DWORD pos, int hti, const VALUE_STACK& vstack ) const;

	bool ReadGUID( GUID& guid, int hti ) { return ReadBytes( &guid, sizeof( guid ), hti ); }
	bool ReadSize( QWORD& n, int hti );
	bool ReadGUIDSize( GUID& guid, QWORD& nSize, int hti ) { return ReadGUID( guid, hti ) && ReadSize( nSize, hti ); }

public:
	static constexpr WORD m_nMinHeaderSize = static_cast<WORD>(sizeof(GUID) + sizeof(QWORD));
protected:
	static constexpr DDEFV ddefvType = {
		7, CASFParser::DDEFV::VDT_INFIX,
		0x0000, "UnicodeString", CASFParser::DDEFV::DESC::VDTV_VALUE,
		0x0001, "ByteArray", CASFParser::DDEFV::DESC::VDTV_VALUE,
		0x0002, "BOOL", CASFParser::DDEFV::DESC::VDTV_VALUE,
		0x0003, "DWORD", CASFParser::DDEFV::DESC::VDTV_VALUE,
		0x0004, "QWORD", CASFParser::DDEFV::DESC::VDTV_VALUE,
		0x0005, "WORD", CASFParser::DDEFV::DESC::VDTV_VALUE,
		0x0006, "GUID", CASFParser::DDEFV::DESC::VDTV_VALUE,
	};
};
