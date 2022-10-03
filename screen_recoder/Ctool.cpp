#include "Windows.h"
#include "Ctool.h"

#define WCHAR_BUF_SIZE (2048)

Ctool Ctool::m_instance;

void Ctool::convertUTF8toANSI(const char* utf8str, char* ansistr)
{
	/* UTF8 to Unicode */
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, NULL, 0);
	len = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, m_wcharBuf, len);
	
	/* Unicode to ANSI */
	len = WideCharToMultiByte(CP_ACP, 0, m_wcharBuf, -1, NULL, 0, NULL, NULL);
	len = WideCharToMultiByte(CP_ACP, 0, m_wcharBuf, -1, ansistr, len, NULL, NULL);
}

Ctool::Ctool()
{
	m_wcharBuf = new wchar_t[WCHAR_BUF_SIZE];
}

Ctool::~Ctool()
{
	delete[] m_wcharBuf;
}
