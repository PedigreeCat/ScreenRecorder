﻿#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <thread>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libswresample/swresample.h"

#ifdef __cplusplus
}
#endif

class CAudioRecorder
{
public:
	CAudioRecorder();
	~CAudioRecorder();
	CAudioRecorder(const CAudioRecorder&) = delete;
	CAudioRecorder& operator =(const CAudioRecorder&) = delete;

	void setDevice(const std::string& devicename);
	void setDumpPCM(const std::string &filepath);
	void setDumpAudioData(const std::string& filepath);
	std::vector<std::string> getDeviceList();
	int startRecord();
	void stopRecord();

	/* 音频录制线程 */
	static void worker(CAudioRecorder& recorder);
	/* 保存PCM数据 */
	void dumpPCM(void);
	void resample(AVPacket* packet);

public:
	enum error_code {
		ERR_SUCCESS		=	0,
		ERR_OPEN_DEVICE_FAIL,
		ERR_ALLOC_RES_FAIL,
		ERR_RESAMPLE_FAIL,
		ERR_ENCODE_FAIL
	};

private:
	//void getDeviceList();
	int openDevice();
	void closeDevice();
	int initResampleCtx();
	int initEncoderCtx();
	int releaseAllCtx();

private:
	std::string	m_devicename;
	std::string	m_pcm_filename;
	std::string	m_audio_filename;

	std::ofstream m_pcm_file;
	std::ofstream m_audio_file;

	std::thread m_worker;

	/* ffmpeg context */
	AVFormatContext*	m_inputFormatCtx;
	AVCodecContext*		m_encoderCtx;
	SwrContext*			m_swrCtx;

	uint8_t**			m_swrOutData;
	int					m_swrOutSampleNum;
	int					m_swrOutDataLinesize;

	bool m_recording;
	bool m_pause;
};

