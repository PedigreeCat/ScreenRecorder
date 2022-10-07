#pragma once
class Ctool
{
public:
	static Ctool* getInstance() {
		return &m_instance;
	}
	void convertUTF8ToANSI(const char* utf8str, char* ansistr);
	void convertANSIToUTF8(const char* ansistr, char* utf8str);
	void convertANSIToUnicode(const char* ansistr, wchar_t* unicodestr);
	void convertUTF8ToUnicode(const char* utf8str, wchar_t* unicodestr);
	void convertUnicodeToUTF8(const wchar_t* unicodestr, char* utf8str);
	void convertUnicodeToANSI(const wchar_t* unicodestr, char* ansistr);

private:
	Ctool();
	~Ctool();
	Ctool(const Ctool& obj) = delete;
	Ctool& operator = (const Ctool& obj) = delete;
private:
	static Ctool m_instance;
	wchar_t* m_wcharBuf;
};

