#include "videoEncoder.h"


VideoEncoder::VideoEncoder(int width, int height, int fps, int crf) :
    QObject(NULL),
    m_crf(crf),
    m_framerate(fps),
    m_frameWidth(width),
    m_frameHeight(height),
    m_isOpen(false),
    m_pCodecContext(NULL),
    m_pCodecParams(NULL)
{

}

VideoEncoder::~VideoEncoder()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "VideoEncoder", "~VideoEncoder() called");
    if(NULL != m_pCodecContext)
    {
        avcodec_close(m_pCodecContext);
        avcodec_free_context(&m_pCodecContext);
    }

    if(NULL != m_pCodecParams)
    {
        avcodec_parameters_free(&m_pCodecParams);
    }
    DEBUG_MESSAGE0("VideoEncoder", "~VideoEncoder() finished");
}

void VideoEncoder::Initialize()
{
    AVCodec* pCodec;

    m_isOpen = false;
    DEBUG_MESSAGE0("VideoEncoderH264", "Open() called");

    // Find encoder
    pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (NULL == pCodec)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "VideoEncoderH264", "H264 encoder not found");
        return;
    }

    // Allocate codec context
    m_pCodecContext = avcodec_alloc_context3(pCodec);
    if (NULL == m_pCodecContext)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "VideoEncoderH264", "Failed to allocate codec context");
        return;
    }

    m_pCodecContext->time_base = av_make_q(1, m_framerate);
    m_pCodecContext->gop_size = m_framerate;
    m_pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_pCodecContext->width = m_frameWidth;
    m_pCodecContext->height = m_frameHeight;
    m_pCodecContext->profile = FF_PROFILE_H264_BASELINE;

    // Open codec
    {
        AVDictionary*   options(0);

        av_dict_set(&options, "preset", "veryfast", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set_int(&options, "crf", m_crf, 0);

        if (0 > avcodec_open2(m_pCodecContext, pCodec, &options))
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "VideoEncoderH264", "Failed to open video codec");
            av_dict_free(&options);
            return;
        }
        av_dict_free(&options);
    }

    m_currentPts = 0;

    // Get current codec parameters and send them to subscribers
    m_pCodecParams = avcodec_parameters_alloc();
    avcodec_parameters_from_context(m_pCodecParams, m_pCodecContext);

    emit NewParameters(m_pCodecParams);

    m_isOpen = true;
    DEBUG_MESSAGE0("VideoEncoderH264", "Video encoder opened successfully");

}

void VideoEncoder::EncodeFrame(AVFrame *pFrame)
{
    // Encode frame
    if (m_isOpen)
    {
        int  encodeRes = 0;
        int  recvRes = 0;

        QSharedPointer<AVPacket> pPacket(av_packet_alloc(), [](AVPacket *pkt){av_packet_free(&pkt);});

        // Increment pts
        pFrame->pts = m_currentPts++;

        encodeRes = avcodec_send_frame(m_pCodecContext, pFrame);
        recvRes = avcodec_receive_packet(m_pCodecContext, pPacket.data()); // Generates ref-counted packet

        if (!encodeRes && !recvRes)
        {
            // Packet duration is always 1 in 1/fps timebase
            pPacket->duration = 1;

            av_packet_rescale_ts(pPacket.data(), m_pCodecContext->time_base, AV_TIME_BASE_Q);

            DEBUG_MESSAGE0("VideoEncoder", "PacketReady() emitted. packet");
            emit PacketReady(pPacket);
        }
        else
        {
            char    err1[255] = {0};
            char    err2[255] = {0};
            av_make_error_string(err1, 255, encodeRes);
            av_make_error_string(err2, 255, recvRes);
            ERROR_MESSAGE2(ERR_TYPE_ERROR, "VideoEncoder", "Error encoding video frame   " \
                           "avcodec_send_frame(): %s\tavcodec_receive_packet(): %s", err1, err2);
            return;
        }
    }
    else
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "VideoEncoder", "Video encoder has not been initialized properly");
    }
}

