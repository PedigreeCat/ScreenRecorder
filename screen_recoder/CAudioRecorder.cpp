#include <regex>
#include "CAudioRecorder.h"
#include "Clogger.h"
#include "Ctool.h"

constexpr int audio_sample_size = 16;
constexpr int audio_sample_rate = 48000;
constexpr int audio_channels = 2;

CAudioRecorder::CAudioRecorder()
	: m_inputFormatCtx(NULL)
	, m_encoderCtx(NULL)
	, m_swrCtx(NULL)
	, m_swrOutData(NULL)
	, m_swrOutSampleNum(0)
	, m_swrOutDataLinesize(0)
	, m_recording(false)
	, m_pause(false)
	, m_worker()
{
	/* register all devicde and codec */
	avdevice_register_all();
	avcodec_register_all();

	return;
}

CAudioRecorder::~CAudioRecorder()
{
	releaseAllCtx();
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
		DUMP_ERR("av_dict_set failed", ret);
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
	initResampleCtx();
	m_recording = true;
	m_worker = std::thread(worker, std::ref(*this));

	return ERR_SUCCESS;
}

void CAudioRecorder::stopRecord()
{
	m_recording = false;
	m_worker.join();
	if (m_inputFormatCtx)
		avformat_close_input(&m_inputFormatCtx);
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
		/* 采集数据 */
		if (ret = av_read_frame(recorder.m_inputFormatCtx, packet) != 0)
			DUMP_ERR("av_read_frame failed", ret);
		/* 重采样，设备采集的音频采样格式是S16，需要转成AAC支持的FLTP */
		ELOG_D("av_read_frame read packet.size: %d", packet->size);
		recorder.resample(packet);

		/* 将重采样后PCM数据写到文件中 */
		recorder.dumpPCM();
	}
end:
	if (packet)
		av_packet_free(&packet);
	ELOG_I("audio recorder worker end");
}

void CAudioRecorder::dumpPCM(void)
{
	if (m_pcm_filename.empty())
		return;
	if (!m_pcm_file.is_open()) {
		m_pcm_file.open(m_pcm_filename,
			std::ios_base::out | std::ios_base::binary);
	}
	m_pcm_file.write((char*)m_swrOutData[1], m_swrOutDataLinesize);
}

void CAudioRecorder::resample(AVPacket* packet)
{
	int ret = 0;
	uint8_t* src_data[4] = { 0 };
	src_data[0] = packet->data;
	/* 计算采样点数 */
	int inSampleNum = packet->size / audio_channels / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	int outSampleNum = av_rescale_rnd(inSampleNum, audio_sample_rate, audio_sample_rate, AV_ROUND_UP);
	/* 更新重采样输出缓存区 */
	if (outSampleNum > m_swrOutSampleNum) {
		m_swrOutSampleNum = outSampleNum;
		if (m_swrOutData) {
			av_freep(&m_swrOutData[0]);
			av_freep(&m_swrOutData);
		}
		if (ret = av_samples_alloc_array_and_samples(&m_swrOutData, &m_swrOutDataLinesize,
					audio_channels, m_swrOutSampleNum, AV_SAMPLE_FMT_FLTP, 0) < 0)
			DUMP_ERR("av_samples_alloc_array_and_samples failed", ret);
	}

	if (ret = swr_convert(m_swrCtx, m_swrOutData, m_swrOutSampleNum, (const uint8_t**)src_data, inSampleNum) < 0)
		DUMP_ERR("swr_convert failed", ret);
	return;
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

	av_dict_set_int(&tmpDict, "sample_rate", audio_sample_rate, 0);
	av_dict_set_int(&tmpDict, "sample_size", audio_sample_size, 0);
	av_dict_set_int(&tmpDict, "channels", audio_channels, 0);

	AVInputFormat* inFmt = av_find_input_format(short_name);
	if (!inFmt) {
		ELOG_E("Not found inputFormat: %s", short_name);
		goto end;
	}
	/* 打开音频输入设备，此时已经开始在采集音频数据了 */
	if (ret = avformat_open_input(&m_inputFormatCtx, m_devicename.c_str(), inFmt, NULL) < 0) {
		DUMP_ERR("avformat_open_input failed", ret);
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
	int ret = 0;
	m_swrCtx = swr_alloc_set_opts(NULL,
		AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLTP, audio_sample_rate, /* output PCM params */
		AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, audio_sample_rate,	/* input PCM params */
		0, NULL);
	if (NULL == m_swrCtx) {
		ELOG_E("swr_alloc_set_opts failed");
		return ERR_ALLOC_RES_FAIL;
	}
	if (ret = swr_init(m_swrCtx) < 0) {
		DUMP_ERR("swr_init failed", ret);
		return ERR_ALLOC_RES_FAIL;
	}
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
	if (m_swrCtx)
		swr_free(&m_swrCtx);
	if (m_encoderCtx)
		avcodec_free_context(&m_encoderCtx);
	if (m_swrOutData) {
		av_freep(&m_swrOutData[0]);
	}
	if (m_pcm_file.is_open())
		m_pcm_file.close();
	return 0;
}

void CAudioRecorder::closeDevice()
{
	avformat_close_input(&m_inputFormatCtx);
}


