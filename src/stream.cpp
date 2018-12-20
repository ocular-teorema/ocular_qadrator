
#include "stream.h"

#include <QUrl>

Stream::Stream(QString name, QString inputUrl, int targetWidth, int targetHeight) :
    QObject(NULL),
    m_name(name),
    m_inputUrl(inputUrl),
    m_targetWidth(targetWidth),
    m_targetHeight(targetHeight),
    m_pInputContext(NULL),
    m_pCodecContext(NULL),
    m_pFrame(NULL)
{

}

Stream::~Stream()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "Stream", "~Stream() called");
}

bool Stream::Initialize()
{
    int             res;
    QUrl            testUrl(m_inputUrl);
    AVDictionary*   inputOptions(0);
    AVCodec*        pCodec;

    if(!testUrl.isValid())
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Invalid url");
        return false;
    }

    av_dict_set(&inputOptions, "rtsp_transport", "tcp", 0);
    res = avformat_open_input(&m_pInputContext, m_inputUrl.toLatin1().data(), NULL, &inputOptions);
    av_dict_free(&inputOptions);

    if (res < 0)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Failed to open stream");
        return false;
    }

    res = avformat_find_stream_info(m_pInputContext, NULL);
    if (res < 0)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Failed to get stream info");
        return false;
    }

    // Dump information about file onto standard error
    av_dump_format(m_pInputContext, 0, m_inputUrl.toLatin1().data(), 0);

    // Find the first video stream
    m_videoStreamIndex = av_find_best_stream(m_pInputContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (m_videoStreamIndex < 0)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Unable to find video stream");
        return false;
    }

    pCodec = avcodec_find_decoder(m_pInputContext->streams[m_videoStreamIndex]->codecpar->codec_id);

    // Create codec context
    m_pCodecContext = avcodec_alloc_context3(pCodec);
    if (NULL == m_pCodecContext)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Unable to allocate decoder context");
        return false;
    }

    // Copy input codec parameters
    avcodec_parameters_to_context(m_pCodecContext, m_pInputContext->streams[m_videoStreamIndex]->codecpar);

    // This field need to be set for cathing errors while decoding
    m_pCodecContext->err_recognition = AV_EF_EXPLODE;

    res = avcodec_open2(m_pCodecContext, pCodec, NULL);
    if (res < 0)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Failed to open video decoder");
        return false;
    }

    // Required for flv format output (to avoid "avc1/0x31637661 incompatible with output codec id 28")
    m_pInputContext->streams[m_videoStreamIndex]->codecpar->codec_tag = 0;

    m_pFrame = av_frame_alloc();
    if(m_pFrame == NULL)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Stream", "Failed to allocate AVFrame");
        return false;
    }

    // Frame capturing performed on capture timers' timeout event
    // This made to prevent event loop blocking with while(1)
    // This timer should be created here, because InitCapture function
    // Is called after Stream object moved to it's separate thread
    m_pCaptureTimer = new QTimer;
    m_pCaptureTimer->setTimerType(Qt::PreciseTimer);

    double fps = 60.0;//FFMIN(100.0, FFMAX(5.0, av_q2d(m_pInputContext->streams[m_videoStreamIndex]->avg_frame_rate)));
    m_pCaptureTimer->setInterval(990.0 / fps); // Make interval a bit lower than real value
    QObject::connect(m_pCaptureTimer, SIGNAL(timeout()), this, SLOT(CaptureNewFrame()));

    m_stop = false;
    m_readErrors = 0;
    m_decodeErrors = 0;

    return true;
}

void Stream::Deinitialize()
{
    m_pCaptureTimer->stop();
    delete m_pCaptureTimer;

    if(NULL != m_pFrame)
    {
        av_frame_free(&m_pFrame);
    }

    if(NULL != m_pCodecContext)
    {
        avcodec_close(m_pCodecContext);
        avcodec_free_context(&m_pCodecContext);
    }

    if(NULL != m_pInputContext)
    {
        avformat_close_input(&m_pInputContext);
    }
}

void Stream::CaptureNewFrame()
{
    int             readRes = -1;
    int             sendRes = -1;
    int             decodeRes = -1;

    AVPacket        packet;

    while (!m_stop && (readRes = av_read_frame(m_pInputContext, &packet)) >= 0) // while we have available frames in stream
    {
        if (packet.stream_index == m_videoStreamIndex)  // We need only video frames to be decoded
        {
            // Decode frame
            sendRes = avcodec_send_packet(m_pCodecContext, &packet);
            decodeRes = avcodec_receive_frame(m_pCodecContext, m_pFrame);

            if (sendRes || decodeRes)
            {
                char err1[255] = {0};
                char err2[255] = {0};
                av_make_error_string(err1, 255, sendRes);
                av_make_error_string(err2, 255, decodeRes);
                ERROR_MESSAGE2(ERR_TYPE_MESSAGE, "RTSPCapture",
                               "Error decoding video frame avcodec_send_packet(): %s\tavcodec_receive_frame(): %s",
                               err1, err2);
            }
            // Exit reading while
            av_packet_unref(&packet);
            break;
        }
        av_packet_unref(&packet);
    }

    // Add all decoded frames to frame buffer
    if (decodeRes == 0)
    {
        m_decodeErrors = 0;

        if (m_pFrame->format != AV_PIX_FMT_YUV420P && m_pFrame->format != AV_PIX_FMT_YUVJ420P)
        {
            ERROR_MESSAGE1(ERR_TYPE_DISPOSABLE, "Stream", "Stream %s unsupported frame format", m_name.toUtf8().constData());
            return;
        }

        AVRational inTimeBase = m_pInputContext->streams[m_videoStreamIndex]->time_base;

        m_pFrame->pkt_dts = av_rescale_q(m_pFrame->pkt_dts, inTimeBase, AV_TIME_BASE_Q);
        m_pFrame->best_effort_timestamp = av_rescale_q(m_pFrame->best_effort_timestamp, inTimeBase, AV_TIME_BASE_Q);

        emit FrameReady(ScaleFrame(m_pFrame));

        // Free decoded frame data
        av_frame_unref(m_pFrame);
    }
    else
    {
        if (++m_decodeErrors > 300 && !m_stop)
        {
            ERROR_MESSAGE1(ERR_TYPE_CRITICAL, "Stream", "Stream %s 300 decode frame errors in a row", m_name.toUtf8().constData());

            // Try to reonnect to the stream
            Deinitialize();
            Initialize();
            emit Reinit();
            m_pCaptureTimer->start();
        }
    }

    if (readRes < 0)
    {
        char err[255];
        av_make_error_string(err, 255, readRes);
        ERROR_MESSAGE2(ERR_TYPE_MESSAGE, "Stream", "Stream %s read frame error: %s", m_name.toUtf8().constData(), err);

        if (++m_readErrors > 300 && !m_stop)
        {
            ERROR_MESSAGE1(ERR_TYPE_CRITICAL, "Stream", "Stream %s 300 read frame errors in a row", m_name.toUtf8().constData());

            // Try to reonnect to the stream
            Deinitialize();
            Initialize();
            emit Reinit();
            m_pCaptureTimer->start();
        }
    }
    else
    {
        m_readErrors = 0;
    }
}

QSharedPointer<AVFrame> Stream::ScaleFrame(AVFrame *pInFrame)
{
    int targetWidth;
    int targetHeight;
    double scale = std::min((double)m_targetWidth / (double)pInFrame->width,
                            (double)m_targetHeight/ (double)pInFrame->height);

    targetWidth = (int)(pInFrame->width * scale + 0.5f)  & 0xFFFFFFFC; // width and height are better when even
    targetHeight = (int)(pInFrame->height * scale + 0.5f) & 0xFFFFFFFC;

    QSharedPointer<AVFrame> pOutFrame(av_frame_alloc(), [] (AVFrame *ptr) {av_frame_free(&ptr);});

    av_frame_copy_props(pOutFrame.data(), pInFrame);

    pOutFrame->format = AV_PIX_FMT_YUV420P;
    pOutFrame->width  = targetWidth;
    pOutFrame->height = targetHeight;

    if (0 > av_frame_get_buffer(pOutFrame.data(), 0))  // allocates aligned buffer for y,u,v planes
    {
        ERROR_MESSAGE1(ERR_TYPE_ERROR, "Stream", "Stream %s Failed to alocate frame buffer", m_name.toUtf8().constData());
        return QSharedPointer<AVFrame>(NULL);
    }

    m_scaler.scaleFrame(pInFrame, pOutFrame.data());

    return pOutFrame;
}
