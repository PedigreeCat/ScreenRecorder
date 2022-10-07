#include "Windows.h"
#include "Ctool.h"

#define WCHAR_BUF_SIZE (2048)

Ctool Ctool::m_instance;

void Ctool::convertUTF8ToANSI(const char* utf8str, char* ansistr)
{
	/* UTF8 to Unicode */
	convertUTF8ToUnicode(utf8str, m_wcharBuf);
	
	/* Unicode to ANSI */
	convertUnicodeToANSI(m_wcharBuf, ansistr);
}

void Ctool::convertANSIToUTF8(const char* ansistr, char* utf8str)
{
	/* ANSI to Unicode */
	convertANSIToUnicode(ansistr, m_wcharBuf);

	/* Unicode to UTF8 */
	convertUnicodeToUTF8(m_wcharBuf, utf8str);
}

void Ctool::convertANSIToUnicode(const char* ansistr, wchar_t* unicodestr)
{
	int len = MultiByteToWideChar(CP_ACP, 0, ansistr, -1, NULL, 0);
	len = MultiByteToWideChar(CP_ACP, 0, ansistr, -1, unicodestr, len);
}

void Ctool::convertUTF8ToUnicode(const char* utf8str, wchar_t* unicodestr)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, NULL, 0);
	len = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, unicodestr, len);
}

void Ctool::convertUnicodeToUTF8(const wchar_t* unicodestr, char* utf8str)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, unicodestr, -1, NULL, 0, NULL, NULL);
	len = WideCharToMultiByte(CP_UTF8, 0, unicodestr, -1, utf8str, len, NULL, NULL);
}

void Ctool::convertUnicodeToANSI(const wchar_t* unicodestr, char* ansistr)
{
	int len = WideCharToMultiByte(CP_ACP, 0, unicodestr, -1, NULL, 0, NULL, NULL);
	len = WideCharToMultiByte(CP_ACP, 0, unicodestr, -1, ansistr, len, NULL, NULL);
}

Ctool::Ctool()
{
	m_wcharBuf = new wchar_t[WCHAR_BUF_SIZE];
}

Ctool::~Ctool()
{
	delete[] m_wcharBuf;
}
