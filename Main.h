#pragma once

#include "Helper.h"
#include "IOBase.h"

// ------------------------------------------------------------------------------------------------

#ifdef _CONSOLE
int __cdecl _tmain( int argc, _TCHAR* argv[] );
#endif

class CommandLine {
public:
	typedef std::vector<_tstring> WorkList;
	enum class CLError { CL_OK, CL_MESSAGE, CL_USAGE };

#if defined(_WIN32) || defined(_WIN64)
	CommandLine( class Settings& Settings, win::LPTSTR lpCommandLine );
#endif
	CommandLine( class Settings& Settings, int argc, _TCHAR* argv[] );

	WorkList GetWorkList() const { return m_vWorkList; }
	CLError GetError() const { return m_eCLError; }
	_tstring GetMessage() const { return m_sMessage; }
protected:
	void Parse( class Settings& settings, int argc, _TCHAR* argv[] );

	WorkList m_vWorkList;
	CLError m_eCLError;
	_tstring m_sMessage;
};

extern std::string sProgramTitle;
