#include "CDeskRecorder.h"
#include "Clogger.h"
#include "Ctool.h"

constexpr int video_framerate = 30;
constexpr int desktop_w = 1920;
constexpr int desktop_h = 1080;

CDeskRecorder::CDeskRecorder()
    : m_inputFormatCtx(NULL)
    , m_decoderCtx(NULL)
    , m_encoderCtx(NULL)
    , m_swsCtx(NULL)
    , m_recording(false)
    , m_mp4Mxuer(NULL)
    , m_worker()
{
    /* register all devicde and codec */
    avdevice_register_all();
    avcodec_register_all();

    return;
}

CDeskRecorder::~CDeskRecorder()
{
    releaseAllCtx();
}

void CDeskRecorder::setDumpH264(const std::string& filepath)
{
    m_h264_filename = filepath;
}

void CDeskRecorder::setMP4Muxer(CMP4Muxer* muxer)
{
    m_mp4Mxuer = muxer;
}

int CDeskRecorder::startRecord()
{
    int ret = ERR_SUCCESS;
    (ret == ERR_SUCCESS) && (ret = openDevice());
    (ret == ERR_SUCCESS) && (ret = initDecoderCtx());
    (ret == ERR_SUCCESS) && (ret = initSwscaleCtx());
    (ret == ERR_SUCCESS) && (ret = initEncoderCtx());
    if (ret != ERR_SUCCESS) {
        releaseAllCtx();
        return ERR_ALLOC_RES_FAIL;
    }
    m_recording = true;
    m_worker = std::thread(worker, std::ref(*this));

    return ERR_SUCCESS;
}

void CDeskRecorder::stopRecord()
{
    m_recording = false;
    m_worker.join();
    if (m_inputFormatCtx)
        avformat_close_input(&m_inputFormatCtx);
    if (m_h264_file.is_open())
        m_h264_file.close();
}

void CDeskRecorder::worker(CDeskRecorder& recorder)
{
    ELOG_I("desktop recorder worker start");

    int ret = 0;
    AVPacket* dev_pkt = NULL, * video_pkt = NULL;
    AVFrame* frame_RGBA = NULL, * frame_YUV = NULL;
    dev_pkt = av_packet_alloc();
    if (!dev_pkt) {
        ELOG_E("av_packet_alloc failed.");
        goto end;
    }
    video_pkt = av_packet_alloc();
    if (!video_pkt) {
        ELOG_E("av_packet_alloc failed.");
        goto end;
    }

    frame_RGBA = av_frame_alloc();
    if (!frame_RGBA) {
        ELOG_E("av_frame_alloc failed.");
        goto end;
    }

    frame_YUV = av_frame_alloc();
    if (!frame_RGBA) {
        ELOG_E("av_frame_alloc failed.");
        goto end;
    }
    frame_YUV->width = desktop_w;
    frame_YUV->height = desktop_h;
    frame_YUV->format = AV_PIX_FMT_YUV420P;

    ret = av_frame_get_buffer(frame_YUV, 32);

    /* 主循环 */
    while (recorder.m_recording) {
        /* 采集数据 */
        if (ret = av_read_frame(recorder.m_inputFormatCtx, dev_pkt) != 0)
            DUMP_ERR("av_read_frame failed", ret);
        recorder.decode(dev_pkt, frame_RGBA);
        recorder.scale_convert(frame_RGBA, frame_YUV);
        frame_YUV->pts = dev_pkt->pts;
        recorder.encode(frame_YUV, video_pkt);
    }
    /* 清空编码器 */
   recorder.encode(NULL, video_pkt);
    
end:
    if (dev_pkt)
        av_packet_free(&dev_pkt);
    if (video_pkt)
        av_packet_free(&video_pkt);
    if (frame_RGBA)
        av_frame_free(&frame_RGBA);

    ELOG_I("desktop recorder worker end");
}

int CDeskRecorder::openDevice()
{
    std::string short_name = "gdigrab";
    int ret = 0;
    AVDictionary* tmpDict = NULL;
    av_dict_set_int(&tmpDict, "framerate", video_framerate, 0);
    av_dict_set_int(&tmpDict, "draw_mouse", 0, 0);

    AVInputFormat* inFmt = av_find_input_format(short_name.c_str());
    if (!inFmt) {
        ELOG_E("Not found inputFormat: %s", short_name.c_str());
        ret = ERR_OPEN_DEVICE_FAIL;
        goto end;
    }
    ret = avformat_open_input(&m_inputFormatCtx, "desktop", inFmt, &tmpDict);
    if (ret < 0) {
        DUMP_ERR("avformat_open_input failed", ret);
        ret = ERR_OPEN_DEVICE_FAIL;
        goto end;
    }

end:
    if (tmpDict)
        av_dict_free(&tmpDict);
    return ret;
}

void CDeskRecorder::closeDevice()
{
    avformat_close_input(&m_inputFormatCtx);
}

int CDeskRecorder::initSwscaleCtx()
{
    int ret = 0;
    m_swsCtx = sws_getContext(
        desktop_w, desktop_h, AV_PIX_FMT_BGRA,
        desktop_w, desktop_h, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (NULL == m_swsCtx) {
        ELOG_E("sws_getContext failed");
        return ERR_ALLOC_RES_FAIL;
    }
    return ERR_SUCCESS;
}

int CDeskRecorder::initDecoderCtx()
{
    int ret = 0;
    AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_BMP);
    if (NULL == codec) {
        ELOG_E("avcodec_find_decoder failed");
        return ERR_FIND_RES_FAIL;
    }
    if ((m_decoderCtx = avcodec_alloc_context3(codec)) == NULL) {
        ELOG_E("avcodec_alloc_context3 failed");
        return ERR_ALLOC_RES_FAIL;
    }

    ret = avcodec_open2(m_decoderCtx, codec, NULL);
    if (ret < 0) {
        DUMP_ERR("avcodec_open2 failed", ret);
        return ERR_ALLOC_RES_FAIL;
    }
    
    return ERR_SUCCESS;
}

int CDeskRecorder::initEncoderCtx()
{
    int ret = 0;
    AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (NULL == codec) {
        ELOG_E("avcodec_find_encoder failed");
        return ERR_FIND_RES_FAIL;
    }
    if ((m_encoderCtx = avcodec_alloc_context3(codec)) == NULL) {
        ELOG_E("avcodec_alloc_context3 failed");
        return ERR_ALLOC_RES_FAIL;
    }
    // SPS/PPS
    m_encoderCtx->profile = FF_PROFILE_H264_HIGH;
    m_encoderCtx->level = 40; // level是对视频规格的描述

    // 分辨率
    m_encoderCtx->width = desktop_w;
    m_encoderCtx->height = desktop_h;

    // GOP
    m_encoderCtx->gop_size = 50;
    m_encoderCtx->keyint_min = 25;
    
    // B frame
    m_encoderCtx->max_b_frames = 3;
    m_encoderCtx->has_b_frames = 1;

    // 参考帧数量
    m_encoderCtx->refs = 3;

    m_encoderCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    // 码率
    m_encoderCtx->bit_rate = 600000;

    m_encoderCtx->time_base.num = 1;
    m_encoderCtx->time_base.den = video_framerate;
    m_encoderCtx->framerate.num = video_framerate;
    m_encoderCtx->framerate.den = 1;

    ret = avcodec_open2(m_encoderCtx, codec, NULL);
    if (ret < 0) {
        DUMP_ERR("avcodec_open2", ret);
        return ERR_ALLOC_RES_FAIL;
    }
    
    return ERR_SUCCESS;
}

void CDeskRecorder::releaseAllCtx()
{
    if (m_inputFormatCtx)
        avformat_close_input(&m_inputFormatCtx);
    if (m_swsCtx)
        sws_freeContext(m_swsCtx);
    if (m_encoderCtx)
        avcodec_free_context(&m_encoderCtx);
    return;
}

void CDeskRecorder::dumpH264(AVPacket* p)
{
    if (!m_h264_file.is_open()) {
        m_h264_file.open(m_h264_filename, std::ios_base::out | std::ios_base::binary);
    }
    m_h264_file.write((char*)p->data, p->size);
}

void CDeskRecorder::decode(AVPacket* p, AVFrame* f)
{
    int ret = 0;
    ret = avcodec_send_packet(m_decoderCtx, p);
    if (ret < 0) {
        DUMP_ERR("avcodec_send_packet", ret);
        return;
    }

    ret = avcodec_receive_frame(m_decoderCtx, f);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        DUMP_ERR("avcodec_receive_frame", ret);
    }
}

void CDeskRecorder::scale_convert(AVFrame* rgba, AVFrame* yuv)
{
    int ret = 0;

    ret = sws_scale(m_swsCtx, rgba->data, rgba->linesize,
        0, rgba->height, yuv->data, yuv->linesize);
    if (ret < 0)
        DUMP_ERR("sws_scale", ret);
}

void CDeskRecorder::encode(AVFrame* f, AVPacket* p)
{
    int ret = 0;
    ret = avcodec_send_frame(m_encoderCtx, f);
    if (ret < 0) {
        DUMP_ERR("avcodec_send_frame failed", ret);
        return;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_encoderCtx, p);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return;
            DUMP_ERR("avcodec_send_frame failed", ret);
        }
        dumpH264(p);
        if (m_mp4Mxuer)
            m_mp4Mxuer->push_packet(p);
    }
}
