#include "CTool.h"
#include "Clogger.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/avutil.h"

#ifdef __cplusplus
}
#endif

/* 对EasyLogger进行封装 */

#define CACHE_SIZE	(2*1024*1024)	/*直接分配2M以免日志缓存移除*/
#define UTF8_BUF_SIZE (2048)

Clogger Clogger::m_instance;
char Clogger::m_utf8_buf[];
char Clogger::m_ansi_buf[];
Clogger::FFmpegLogHook Clogger::m_ffmpegLogHook = nullptr;
std::vector<std::string>* Clogger::m_logVec = nullptr;

void Clogger::FFmpegLogCallback(void* ptr, int level, const char* format, va_list vl)
{
	(void)ptr;
	(void)level;
	/* ffmpeg中的日志为UTF-8编码，需要转换 */
	vsprintf(m_utf8_buf, format, vl);
	Ctool::getInstance()->convertUTF8ToANSI(m_utf8_buf, m_ansi_buf);
	if (m_ffmpegLogHook)
		m_ffmpegLogHook(m_ansi_buf, m_logVec);
	ELOG_D("%s", m_ansi_buf);
}

void Clogger::getLogCacheAndClean(std::string& str)
{
	str += m_cache_buf;
	cleanUpCache();
}

void Clogger::writeLog2Cache(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	m_cache_pos += vsprintf(m_cache_pos, format, ap);
	m_cache_pos += sprintf(m_cache_pos, "\r\n");
	va_end(ap);
}

void Clogger::cleanUpCache()
{
	m_cache_pos = m_cache_buf;
	m_cache_pos[0] = '\0';
}

Clogger::Clogger()
	: m_cache_buf(NULL)
	, m_cache_pos(NULL)
{
	/* init EastKigger */
	elog_init();
	elog_set_filter_lvl(ELOG_LVL_DEBUG);
	elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL & ~ELOG_FMT_TAG & ~ELOG_FMT_P_INFO);
	elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_ALL & ~ELOG_FMT_TAG & ~ELOG_FMT_P_INFO);
	elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_ALL & ~ELOG_FMT_TAG & ~ELOG_FMT_P_INFO);
	elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_ALL & ~ELOG_FMT_TAG & ~ELOG_FMT_P_INFO);
	elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL & ~ELOG_FMT_TAG & ~ELOG_FMT_P_INFO);
	elog_start();

	/* init FFmpeg Log */
	av_log_set_level(AV_LOG_DEBUG);
	av_log_set_callback(Clogger::FFmpegLogCallback);

	m_cache_buf = new char[CACHE_SIZE];
	if (!m_cache_buf) log_a("new m_cache_buf failed.");
	cleanUpCache();
}

Clogger::~Clogger()
{
	delete[] m_cache_buf;
	elog_stop();
	elog_deinit();
}