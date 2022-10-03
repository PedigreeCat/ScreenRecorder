#pragma once

#include <string>
#include <stdarg.h>
#include "elog.h"

/*
* 用于将应用日志以及FFmpeg内部日志输出到日志文件和对话框中
* 单例模式（饿汉模式）
*/
class Clogger
{
public:
	static Clogger* getInstance() {
		return &m_instance;
	}
	static void FFmpegLogCallback(void* ptr, int level, const char* format, va_list vl);
	void getLogCacheAndClean(std::string& str);
	void writeLog2Cache(const char* format, ...);
	void cleanUpCache();

private:
	Clogger();
	~Clogger();
	Clogger(const Clogger& obj) = delete;
	Clogger& operator = (const Clogger& obj) = delete;

private:
	char* m_cache_buf;
	char* m_cache_pos;
	static char m_utf8_buf[2048];	/* 用于暂存ffmpeg中的utf8日志 */
	static char m_ansi_buf[2048];	/* 用于暂存转换后的ansi日志 */
	static Clogger m_instance;
};

/* 重新定义函数宏 */
#define ELOG_A(...)    log_a(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_E(...)    log_e(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_W(...)    log_w(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_I(...)    log_i(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_D(...)    log_d(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
