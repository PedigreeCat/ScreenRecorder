#include <fstream>
#include <regex>
#include "CAudioRecorder.h"
#include "Clogger.h"
#include "Ctool.h"

#define MAX_ERR_BUF_SIZE	AV_ERROR_MAX_STRING_SIZE

CAudioRecorder::CAudioRecorder()
	: m_inputFormatCtx(NULL)
	, m_encoderCtx(NULL)
	, m_resampleCtx(NULL)
{
	m_errBuf = new char[MAX_ERR_BUF_SIZE];
	/* register all devicde and codec */
	avdevice_register_all();
	avcodec_register_all();

	return;
}

CAudioRecorder::~CAudioRecorder()
{
	delete[] m_errBuf;
}

void CAudioRecorder::setDevice(const std::string& devicename)
{
	char utf8str[1024];
	Ctool::getInstance()->convertANSIToUTF8(devicename.c_str(), utf8str);
	m_devicename = utf8str;
}

void CAudioRecorder::setDumpPCM(const std::string& filepath)
{
	m_pcm_filename = filepath;
}

void CAudioRecorder::setDumpAudioData(const std::string& filepath)
{
	m_audio_filename = filepath;
}

std::vector<std::string> CAudioRecorder::getDeviceList()
{
	int ret = 0;
	AVFormatContext* tmpFormatCtx = NULL;
	AVInputFormat* inputFormatCtx = av_find_input_format("dshow");
	AVDictionary* tmpDict = NULL;
	std::vector<std::string> logVec;
	bool findAudioFlag = false;

	if (ret = av_dict_set(&tmpDict, "list_devices", "true", 0) < 0) {
		dumpErr(ret);
		ELOG_E("av_dict_set failed: [%d](%s)", AVERROR(ret), m_errBuf);
		goto free_resource;
	}

	Clogger::getInstance()->setFFmpegLogHook([&findAudioFlag](const char* log, std::vector<std::string>* logList)->void
		{
			bool ret = false;
			/* 通过正则表达式将日志中的音频设备名过滤出来，
			   这部分内容需要阅读源码libavdevice/dshow.c才能知道 */

			/* libavdevice/dshow.c:dshow_read_header，在打印音频输入设备前会先打印"DirectShow audio devices\n" */
			std::regex reg_audio_device_start("DirectShow audio devices\n");
			/* libavdevice/dshow.c:dshow_cycle_devices，这条这则表达式是根据日志打印内容编写的，匹配"设备名 (设备描述)"格式的字符串 */
			std::regex reg_audio_device_name("^\\s*\\\"(.+\\(.+\\))\\\"\n$");
			std::cmatch match_list;	/* 结果存储为C字符串数组 */
			if (!findAudioFlag) {
				ret = std::regex_match(log, reg_audio_device_start);
				if (ret)
					findAudioFlag = true;
			} else {
				ret = std::regex_match(log, match_list, reg_audio_device_name);
				if (ret)
					logList->push_back(std::string(match_list[1]));
			}
		}, 
		&logVec);
	(void)avformat_open_input(&tmpFormatCtx, NULL, inputFormatCtx, &tmpDict);
	/* 音频输入设备列表获取完后清空钩子函数 */
	Clogger::getInstance()->setFFmpegLogHook(NULL, NULL);

free_resource:
	if (tmpFormatCtx)
		avformat_free_context(tmpFormatCtx);
	if (tmpDict)
		av_dict_free(&tmpDict);

	return logVec;
}

int CAudioRecorder::startRecord()
{
	openDevice();

	return 0;
}

void CAudioRecorder::dumpErr(int err)
{
	av_strerror(err, m_errBuf, MAX_ERR_BUF_SIZE);
}

int CAudioRecorder::openDevice()
{

	return ERR_SUCCESS;
}


