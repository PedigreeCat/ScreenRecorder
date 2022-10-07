#pragma once

#include <string>
#include <vector>

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
	//void stopRecord();


public:
	enum error_code {
		ERR_SUCCESS		=	0,
		ERR_OPEN_DEVICE	=	1,
		ERR_ALLOC_RES	=	2,
		ERR_RESAMPLE	=	3,
		ERR_ENCODE		=	4
	};

private:
	//void getDeviceList();
	void dumpErr(int err);
	int openDevice();

private:
	std::string	m_devicename;
	std::string	m_pcm_filename;
	std::string	m_audio_filename;

	/* ffmpeg context */
	AVFormatContext*	m_inputFormatCtx;
	AVCodecContext*		m_encoderCtx;
	SwrContext*			m_resampleCtx;

	char *m_errBuf;
};

