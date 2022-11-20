#include "CMP4Muxer.h"
#include "Clogger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/rational.h"

#ifdef __cplusplus
}
#endif

constexpr int video_framerate = 30;
constexpr int desktop_w = 1920;
constexpr int desktop_h = 1080;

CMP4Muxer::CMP4Muxer()
    : m_formatCtx(NULL)
{
    av_register_all();
    avcodec_register_all();
}

CMP4Muxer::~CMP4Muxer()
{
}

int CMP4Muxer::open()
{
    initMuxer();
    return 0;
}

int CMP4Muxer::push_packet(AVPacket* p)
{
    p->stream_index = m_formatCtx->streams[0]->index;

    int ret = av_interleaved_write_frame(m_formatCtx, p);
    if (ret < 0) {
        DUMP_ERR("av_interleaved_write_frame", ret);
        return ERR_WRITE_FAIL;
    }
    return 0;
}

void CMP4Muxer::close()
{
    int ret = av_write_trailer(m_formatCtx);
    if (ret < 0) {
        DUMP_ERR("av_write_trailer failed", ret);
        return;
    }
    releaseMuxer();
}

void CMP4Muxer::add_stream(media_type type)
{
    int ret = 0;
    AVStream* stream = NULL;
    AVCodec* codec = NULL;
    AVCodecContext* avctx = NULL;
    ELOG_D("add_stream, media type: %d", type);
    switch (type)
    {
        case CMP4Muxer::MEDIA_TYPE_VIDEO: {
            codec = avcodec_find_decoder_by_name("h264");
        } break;

        case CMP4Muxer::MEDIA_TYPE_AUDIO: {
            codec = avcodec_find_encoder_by_name("aac");
        } break;
        default:
            goto end;
    }
    if (NULL == codec) {
        ELOG_E("avcodec_find_encoder_by_name failed");
        goto end;
    }
    avctx = avcodec_alloc_context3(codec);
    if (NULL == avctx) {
        ELOG_E("avcodec_alloc_context3 failed");
        goto end;
    }
    stream = avformat_new_stream(m_formatCtx, NULL);
    if (NULL == stream) {
        ELOG_E("avformat_new_stream failed");
        goto end;
    }
    stream->id = m_formatCtx->nb_streams - 1;
    if (MEDIA_TYPE_VIDEO == type) {
        avctx->codec_id = codec->id;
        avctx->bit_rate = 400000;
        avctx->width = desktop_w;
        avctx->height = desktop_h;
        stream->time_base.num = 1;
        stream->time_base.den = video_framerate;
        avctx->time_base = stream->time_base;
        avctx->gop_size = video_framerate;
        avctx->pix_fmt = AV_PIX_FMT_YUV420P;
        avctx->max_b_frames = 0;
    }
    else {

    }
    //ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        DUMP_ERR("avcodec_open2", ret);
        goto end;
    }
    ret = avcodec_parameters_from_context(stream->codecpar, avctx);
    if (ret < 0) {
        DUMP_ERR("avcodec_parameters_from_context", ret);
        goto end;
    }

end:
    if (avctx)
        avcodec_free_context(&avctx);
    return;
}

void CMP4Muxer::initMuxer()
{
    int ret = 0;
    auto nowTM = std::chrono::system_clock::now();
    std::time_t tmNowTM = std::chrono::system_clock::to_time_t(nowTM);
    std::stringstream filename;
    filename << std::put_time(std::localtime(&tmNowTM), "%Y%m%d-%H%M%S") << ".mp4";
    /* init context */
    ret = avformat_alloc_output_context2(&m_formatCtx, NULL, NULL, "out.mp4");
    if (ret < 0) {
        DUMP_ERR("avformat_alloc_output_context2", ret);
        return;
    }

    /* add stream */
    add_stream(MEDIA_TYPE_VIDEO);
    // add_stream(MEDIA_TYPE_AUDIO);

    /* open file */
    ret = avio_open(&m_formatCtx->pb, filename.str().c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        DUMP_ERR("avio_open", ret);
        return;
    }

    /* write header */
    ret = avformat_write_header(m_formatCtx, NULL);
    if (ret < 0) {
        DUMP_ERR("avformat_write_header", ret);
        return;
    }
}

void CMP4Muxer::releaseMuxer()
{
    if (m_formatCtx->pb)
        avio_closep(&m_formatCtx->pb);
    if (m_formatCtx)
        avformat_free_context(m_formatCtx);
}
