#pragma once
class Ctool
{
public:
	static Ctool* getInstance() {
		return &m_instance;
	}
	void convertUTF8toANSI(const char* utf8str, char* ansistr);

private:
	Ctool();
	~Ctool();
	Ctool(const Ctool& obj) = delete;
	Ctool& operator = (const Ctool& obj) = delete;
private:
	static Ctool m_instance;
	wchar_t* m_wcharBuf;
};

