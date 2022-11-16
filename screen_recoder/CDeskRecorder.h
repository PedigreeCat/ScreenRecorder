#pragma once

#include <string>
#include <fstream>
#include <thread>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/rational.h"

#ifdef __cplusplus
}
#endif


class CDeskRecorder
{
public:
    CDeskRecorder();
    ~CDeskRecorder();
    CDeskRecorder(const CDeskRecorder&) = delete;
    CDeskRecorder& operator =(const CDeskRecorder&) = delete;
    CDeskRecorder(const CDeskRecorder&&) = delete;
    CDeskRecorder& operator =(const CDeskRecorder&&) = delete;

    void setDumpH264(const std::string& filepath);
    int startRecord();
    void stopRecord();

    /* ÆÁÄ»Â¼ÖÆÏß³Ì */
    static void worker(CDeskRecorder& recorder);

public:
    enum error_code {
        ERR_SUCCESS = 0,
        ERR_FIND_RES_FAIL,
        ERR_OPEN_DEVICE_FAIL,
        ERR_ALLOC_RES_FAIL,
        ERR_SWSCALE_FAIL,
        ERR_ENCODE_FAIL
    };

private:
    int openDevice();
    void closeDevice();
    int initSwscaleCtx();
    int initEncoderCtx();
    void releaseAllCtx();

    void dumpH264(AVPacket* p);
    void scale_convert(AVPacket* pkt, AVFrame* frame);
    void encode(AVFrame* f, AVPacket* p);
private:
    std::string m_h264_filename;
    std::ofstream m_h264_file;

    std::thread m_worker;

    /* ffmpeg context */
    AVFormatContext*    m_inputFormatCtx;
    AVCodecContext*     m_encoderCtx;
    SwsContext*         m_swsCtx;

    bool m_recording;
};

