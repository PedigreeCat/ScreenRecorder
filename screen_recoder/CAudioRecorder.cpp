#include <regex>
#include <thread>
#include "CAudioRecorder.h"
#include "Clogger.h"
#include "Ctool.h"

#define MAX_ERR_BUF_SIZE	AV_ERROR_MAX_STRING_SIZE

CAudioRecorder::CAudioRecorder()
	: m_inputFormatCtx(NULL)
	, m_encoderCtx(NULL)
	, m_resampleCtx(NULL)
	, m_recording(false)
	, m_pause(false)
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
	std::string audioDeviceName = "audio=" + devicename;
	char utf8str[1024];
	Ctool::getInstance()->convertANSIToUTF8(audioDeviceName.c_str(), utf8str);
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
	m_recording = true;
	std::thread recordThread(worker, std::ref(*this));
	recordThread.detach();
	return ERR_SUCCESS;
}

void CAudioRecorder::stopRecord()
{
	m_recording = false;
	if (m_inputFormatCtx)
		avformat_close_input(&m_inputFormatCtx);
	if (m_pcm_file.is_open())
		m_pcm_file.close();
}

void CAudioRecorder::worker(CAudioRecorder& recorder)
{
	ELOG_I("audio recorder worker start");

	int ret = 0;

	AVPacket* packet = av_packet_alloc();
	if (!packet) {
		ELOG_E("av_packet_alloc failed.");
		goto end;
	}

	/* 主循环 */
	while (recorder.m_recording) {
		if (ret = av_read_frame(recorder.m_inputFormatCtx, packet) != 0) {
			recorder.dumpErr(ret);
			ELOG_E("av_read_frame failed: [%d](%s)", AVERROR(ret), recorder.m_errBuf);
		}
		dumpPCM(recorder, packet);
	}
end:
	if (packet)
		av_packet_free(&packet);
	ELOG_I("audio recorder worker end");
}

void CAudioRecorder::dumpPCM(CAudioRecorder& recorder, AVPacket* packet)
{
	if (recorder.m_pcm_filename.empty())
		return;
	if (!recorder.m_pcm_file.is_open()) {
		recorder.m_pcm_file.open(recorder.m_pcm_filename,
			std::ios_base::out | std::ios_base::binary);
	}
	recorder.m_pcm_file.write((char*)packet->data, packet->size);
}

void CAudioRecorder::dumpErr(int err)
{
	av_strerror(err, m_errBuf, MAX_ERR_BUF_SIZE);
}

int CAudioRecorder::openDevice()
{
	const char short_name[] = "dshow";
	int ret = 0;
	AVDictionary* tmpDict = NULL;
	if (m_devicename.empty()) {
		ELOG_E("Not Set audio input device name.");
		goto end;
	}

	av_dict_set(&tmpDict, "sample_rate", "48000", 0);
	av_dict_set(&tmpDict, "sample_size", "16", 0);
	av_dict_set(&tmpDict, "channels", "2", 0);

	AVInputFormat* inFmt = av_find_input_format(short_name);
	if (!inFmt) {
		ELOG_E("Not found inputFormat: %s", short_name);
		goto end;
	}
	/* 打开音频输入设备，此时已经开始在采集音频数据了 */
	if (ret = avformat_open_input(&m_inputFormatCtx, m_devicename.c_str(), inFmt, NULL) < 0) {
		dumpErr(ret);
		ELOG_E("avformat_open_input failed: [%d](%s)", AVERROR(ret), m_errBuf);
		goto end;
	}
	return ERR_SUCCESS;
end:
	if (tmpDict)
		av_dict_free(&tmpDict);
	return ERR_OPEN_DEVICE_FAIL;
}

int CAudioRecorder::initResampleCtx()
{
	return 0;
}

int CAudioRecorder::initEncoderCtx()
{
	return 0;
}

int CAudioRecorder::releaseAllCtx()
{
	if (m_inputFormatCtx)
		avformat_close_input(&m_inputFormatCtx);
	if (m_resampleCtx)
		swr_free(&m_resampleCtx);
	if (m_encoderCtx)
		avcodec_free_context(&m_encoderCtx);
	return 0;
}

void CAudioRecorder::closeDevice()
{
	avformat_close_input(&m_inputFormatCtx);
}


