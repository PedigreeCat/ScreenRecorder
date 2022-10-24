#pragma once

#include <string>
#include <vector>
#include <functional>
#include <stdarg.h>
#include "elog.h"

/* 重新定义函数宏 */
#define ELOG_A(...)    log_a(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_E(...)    log_e(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_W(...)    log_w(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_I(...)    log_i(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)
#define ELOG_D(...)    log_d(__VA_ARGS__), Clogger::getInstance()->writeLog2Cache(__VA_ARGS__)

#define DUMP_ERR(str, err)	ELOG_E(str " [%d](%s)", AVERROR(err), Clogger::getInstance()->dumpErr(err)) 

/*
* 用于将应用日志以及FFmpeg内部日志输出到日志文件和对话框中
* 单例模式（饿汉模式）
*/
class Clogger
{
public:
	/**
	* @pnote: FFmpeg的日志回调函数会调用该钩子函数，将日志内容传递给第一个参数，
	* 第二个参数由调用者自行设置，用于取出日志内容
	*/
	using FFmpegLogHook = std::function<void(const char*, std::vector<std::string>*)>;
public:
	static Clogger* getInstance() {
		return &m_instance;
	}
	static void FFmpegLogCallback(void* ptr, int level, const char* format, va_list vl);
	/**
	 * 设置日子钩子函数，获取FFmpeg内部的日志
	 *
	 * @param hook 钩子函数对象
	 * @param logVec 用于存放日志的vector指针
	 * @note	1）不再使用时需要将两个参数都设置为nullptr;
	 *			2）重复调用会覆盖原本的设置
	 */
	static void setFFmpegLogHook(FFmpegLogHook hook, std::vector<std::string>* logVec) {
		m_ffmpegLogHook = hook;
		m_logVec = logVec;
	}

	void getLogCacheAndClean(std::string& str);
	void writeLog2Cache(const char* format, ...);
	const char* dumpErr(int err);

private:
	Clogger();
	~Clogger();
	Clogger(const Clogger& obj) = delete;
	Clogger& operator = (const Clogger& obj) = delete;

	void cleanUpCache();

private:
	char* m_cache_buf;
	char* m_cache_pos;
	static Clogger m_instance;
	static FFmpegLogHook m_ffmpegLogHook;		/* ffmpeg日志回调函数中的钩子函数 */
	static std::vector<std::string>* m_logVec;	/* 用于保存ffmpeg日志的数组 */
};

