#include <regex>
#include "CAudioRecorder.h"
#include "Clogger.h"
#include "Ctool.h"

constexpr int audio_sample_size = 16;
constexpr int audio_sample_rate = 44100;
constexpr int audio_channels = 2;

#define ADTS_HEADER_LEN (7)

const int sampleFrequencyTable[] = {
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350
};

int getADTSHeader(char* adtsHeader, int packetSize, int profile, int sampleRate, int channels)
{
	int sampleFrequencyIndex = 3; // 默认采样率对应48000Hz
	int adtsLength = packetSize + ADTS_HEADER_LEN;

	for (int i = 0; i < sizeof(sampleFrequencyTable) / sizeof(sampleFrequencyTable[0]); i++)
	{
		if (sampleRate == sampleFrequencyTable[i])
		{
			sampleFrequencyIndex = i;
			break;
		}
	}

	adtsHeader[0] = 0xff;               // syncword:0xfff                                  12 bits

	adtsHeader[1] = 0xf0;               // syncword:0xfff
	adtsHeader[1] |= (0 << 3);          // MPEG Version: 0 for MPEG-4, 1 for MPEG-2         1 bit
	adtsHeader[1] |= (0 << 1);          // Layer:00                                         2 bits
	adtsHeader[1] |= 1;                 // protection absent:1                              1 bit

	adtsHeader[2] = (profile << 6);     // profile:0 for Main, 1 for LC, 2 for SSR          2 bits
	adtsHeader[2] |= (sampleFrequencyIndex & 0x0f) << 2; // sampling_frequency_index        4 bits
	adtsHeader[2] |= (0 << 1);                           // private bit:0                   1 bit
	adtsHeader[2] |= (channels & 0x04) >> 2;             // channel configuration           3 bits

	adtsHeader[3] = (channels & 0x03) << 6;              // channel configuration
	adtsHeader[3] |= (0 << 5);                           // original_copy                   1 bit
	adtsHeader[3] |= (0 << 4);                           // home                            1 bit
	adtsHeader[3] |= (0 << 3);                           // copyright_identification_bit    1 bit
	adtsHeader[3] |= (0 << 2);                           // copytight_identification_start  1 bit
	adtsHeader[3] |= ((adtsLength & 1800) >> 11);        // AAC frame length               13 bits

	adtsHeader[4] = (uint8_t)((adtsLength & 0x7f8) >> 3);// AAC frame length
	adtsHeader[5] = (uint8_t)((adtsLength & 0x7) << 5);  // AAC frame length
	adtsHeader[5] |= 0x1f;                               // buffer fullness:0x7ff          11 bits
	adtsHeader[6] = 0xfc;                                // buffer fullness:0x7ff
	return 0;
}

CAudioRecorder::CAudioRecorder()
	: m_inputFormatCtx(NULL)
	, m_encoderCtx(NULL)
	, m_swrCtx(NULL)
	, m_recording(false)
	, m_worker()
{
	/* register all devicde and codec */
	avdevice_register_all();

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

std::vector<std::string> CAudioRecorder::getDeviceList() &
{
	int ret = 0;
	AVFormatContext* tmpFormatCtx = NULL;
	AVInputFormat* inputFormatCtx = av_find_input_format("dshow");
	AVDictionary* tmpDict = NULL;
	std::vector<std::string> logVec;
	bool findAudioFlag = false;
	ret = av_dict_set(&tmpDict, "list_devices", "true", 0);
	if (ret < 0) {
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
	}
	else {
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
	initEncoderCtx();
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
	if (m_pcm_file.is_open())
		m_pcm_file.close();
	if (m_audio_file.is_open())
		m_audio_file.close();
}

void CAudioRecorder::worker(CAudioRecorder& recorder)
{
	ELOG_I("audio recorder worker start");

	int ret = 0;
	SwrBuffer swr_buf;
	AVPacket* dev_pkt = NULL, * audio_pkt = NULL;
	AVFrame* frame = NULL;
	dev_pkt = av_packet_alloc();
	if (!dev_pkt) {
		ELOG_E("av_packet_alloc failed.");
		goto end;
	}
	audio_pkt = av_packet_alloc();
	if (!audio_pkt) {
		ELOG_E("av_packet_alloc failed.");
		goto end;
	}

	frame = av_frame_alloc();
	if (!frame) {
		ELOG_E("av_frame_alloc failed.");
		goto end;
	}
	frame->nb_samples = recorder.m_encoderCtx->frame_size;
	frame->format = AV_SAMPLE_FMT_FLTP;
	frame->channel_layout = AV_CH_LAYOUT_STEREO;
	av_frame_get_buffer(frame, 0);
	if (!frame->buf[0])
		goto end;

	/* 主循环 */
	int rest_size = 0; /* 前一次重采样的数据编码后还剩下的长度 */
	while (recorder.m_recording) {
		/* 采集数据 */
		if (ret = av_read_frame(recorder.m_inputFormatCtx, dev_pkt) != 0)
			DUMP_ERR("av_read_frame failed", ret);
		/* 将采集到的PCM数据写到文件中 */
		recorder.dumpPCM(dev_pkt);
		/* 重采样，设备采集的音频采样格式是S16，需要转成AAC支持的FLTP */
		ELOG_D("av_read_frame read packet.size: %d", dev_pkt->size);
		recorder.resample(dev_pkt, &swr_buf);

		/* 编码为AAC */
		int offset = 0;
		while (swr_buf.linesize - offset > frame->linesize[0]) {
			int cpy_len = frame->linesize[0] - rest_size;
			memcpy(frame->data[0], swr_buf.data[0] + offset, cpy_len);
			memcpy(frame->data[1], swr_buf.data[1] + offset, cpy_len);
			offset += frame->linesize[0];
			rest_size = 0;
			recorder.encode(frame, audio_pkt);
		}
		/* 将剩余的数据记录下来 */
		rest_size = swr_buf.linesize - offset;
		memcpy(frame->data[0], swr_buf.data[0] + offset, rest_size);
		memcpy(frame->data[1], swr_buf.data[1] + offset, rest_size);
	}
	/* 清空编码器 */
	if (rest_size > 0)
		recorder.encode(frame, audio_pkt);
	recorder.encode(NULL, audio_pkt);

end:
	if (dev_pkt)
		av_packet_free(&dev_pkt);
	if (audio_pkt)
		av_packet_free(&audio_pkt);
	if (swr_buf.data) {
		av_freep(&swr_buf.data[0]);
		av_freep(&swr_buf.data);
	}
	if (frame)
		av_frame_free(&frame);
	ELOG_I("audio recorder worker end");
}

void CAudioRecorder::dumpPCM(AVPacket *pkt)
{
	if (m_pcm_filename.empty())
		return;
	if (!m_pcm_file.is_open()) {
		m_pcm_file.open(m_pcm_filename,
			std::ios_base::out | std::ios_base::binary);
	}
	m_pcm_file.write((char*)pkt->data, pkt->size);
}

void CAudioRecorder::dumpAudio(AVPacket* pkt)
{
	if (m_audio_filename.empty())
		return;
	if (!m_audio_file.is_open()) {
		m_audio_file.open(m_audio_filename,
			std::ios_base::out | std::ios_base::binary);
	}
	char adts_header[ADTS_HEADER_LEN] = { 0 };
	getADTSHeader(adts_header, pkt->size, m_encoderCtx->profile, m_encoderCtx->sample_rate, m_encoderCtx->channels);
	m_audio_file.write(adts_header, ADTS_HEADER_LEN);
	m_audio_file.write((char*)pkt->data, pkt->size);
}

void CAudioRecorder::resample(AVPacket* packet, SwrBuffer* buf)
{
	int ret = 0;
	uint8_t* src_data[4] = { 0 };
	src_data[0] = packet->data;
	/* 计算采样点数 */
	int inSampleNum = packet->size / audio_channels / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	int outSampleNum = av_rescale_rnd(inSampleNum, audio_sample_rate, audio_sample_rate, AV_ROUND_UP);
	/* 更新重采样输出缓存区 */
	if (outSampleNum > buf->nb_samples) {
		buf->nb_samples = outSampleNum;
		if (buf->data) {
			av_freep(&buf->data[0]);
			av_freep(&buf->data);
		}
		if (ret = av_samples_alloc_array_and_samples(&buf->data, &buf->linesize,
					audio_channels, buf->nb_samples, AV_SAMPLE_FMT_FLTP, 0) < 0)
			DUMP_ERR("av_samples_alloc_array_and_samples failed", ret);
	}

	if (ret = swr_convert(m_swrCtx, buf->data, buf->nb_samples, (const uint8_t**)src_data, inSampleNum) < 0)
		DUMP_ERR("swr_convert failed", ret);
	return;
}

void CAudioRecorder::encode(AVFrame* frame, AVPacket* packet)
{
	int ret = 0;
	if (ret = avcodec_send_frame(m_encoderCtx, frame) < 0) {
		DUMP_ERR("avcodec_send_frame failed", ret);
		return;
	}
	while (ret >= 0) {
		ret = avcodec_receive_packet(m_encoderCtx, packet);
		if (ret < 0) {
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			DUMP_ERR("avcodec_receive_packet failed", ret);
		}
		dumpAudio(packet);
	}
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
		ret = ERR_OPEN_DEVICE_FAIL;
		goto end;
	}
	/* 打开音频输入设备，此时已经开始在采集音频数据了 */
	if (ret = avformat_open_input(&m_inputFormatCtx, m_devicename.c_str(), inFmt, &tmpDict) < 0) {
		DUMP_ERR("avformat_open_input failed", ret);
		ret = ERR_OPEN_DEVICE_FAIL;
		goto end;
	}

end:
	if (tmpDict)
		av_dict_free(&tmpDict);
	return ret;
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
	int ret = 0;
	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (NULL == codec) {
		ELOG_E("avcodec_find_encoder failed.");
		return ERR_FIND_RES_FAIL;
	}
	if ((m_encoderCtx = avcodec_alloc_context3(codec)) == NULL) {
		ELOG_E("avcodec_alloc_context3 failed.");
		return ERR_ALLOC_RES_FAIL;
	}
	/* 参数配置 */
	m_encoderCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
	m_encoderCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	m_encoderCtx->channels = audio_channels;
	m_encoderCtx->sample_rate = audio_sample_rate;
	m_encoderCtx->bit_rate = 0;	/* 设置为0，内部根据当前profile去设置 */
	m_encoderCtx->profile = FF_PROFILE_AAC_HE_V2;	/* AAC-LC */

	/* 打开编码器 */
	ret = avcodec_open2(m_encoderCtx, codec, NULL);
	if (ret < 0) {
		DUMP_ERR("avcodec_open2", ret);
		return ERR_ALLOC_RES_FAIL;
	}

	return ERR_SUCCESS;
}

void CAudioRecorder::releaseAllCtx()
{
	if (m_inputFormatCtx)
		avformat_close_input(&m_inputFormatCtx);
	if (m_swrCtx)
		swr_free(&m_swrCtx);
	if (m_encoderCtx)
		avcodec_free_context(&m_encoderCtx);
}

void CAudioRecorder::closeDevice()
{
	avformat_close_input(&m_inputFormatCtx);
}


