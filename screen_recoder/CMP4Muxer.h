#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavutil/avutil.h"

#ifdef __cplusplus
}
#endif

class CMP4Muxer
{
public:
    CMP4Muxer();
    CMP4Muxer(const CMP4Muxer&) = delete;
    CMP4Muxer& operator=(const CMP4Muxer&) = delete;
    CMP4Muxer(const CMP4Muxer&&) = delete;
    CMP4Muxer& operator=(const CMP4Muxer&&) = delete;
    ~CMP4Muxer();

    enum err_code {
        ERR_SUCCESS = 0,
        ERR_WRITE_FAIL
    };

    enum media_type {
        MEDIA_TYPE_NONE = 0,
        MEDIA_TYPE_VIDEO,
        MEDIA_TYPE_AUDIO
    };
    
    int open();
    int push_packet(AVPacket* p);
    void close();

private:
    void add_stream(media_type type);
    void initMuxer();
    void releaseMuxer();

private:
    AVFormatContext* m_formatCtx;
};

