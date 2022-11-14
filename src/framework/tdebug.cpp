#include "pch.h"
#include "tdebug.h"

void ML(const TCHAR *pFormat, ...)
{
#if defined(_WIN32) && !defined(NDEBUG)
	const int LENGTH = 1024*2;
	std::unique_ptr<TCHAR[]> pMess(NNEW TCHAR[LENGTH+1]);
    va_list list;
    va_start(list, pFormat);
	_vstprintf_s(pMess.get(), LENGTH+1, pFormat, list);
	va_end(list);
 	OutputDebugString(pMess.get());
#endif
	return;
}