
#include "output.h"

Output::Output(QString outputURL, int fps) :
    m_outputUrl(outputURL),
    m_framerate(fps),
    m_firstDts(AV_NOPTS_VALUE),
    m_outputInitialized(false),
    m_numErrorsInRow(0),
    m_pH264bsf(NULL),
    m_pFormatCtx(NULL),
    m_pVideoStream(NULL)
{
    m_pEncoderParams = avcodec_parameters_alloc();

    if (m_outputUrl.startsWith("ws"))
    {
        int port = QUrl(m_outputUrl).port();
        // Websocket server
        pWebSocketServer = new QWebSocketServer(QStringLiteral("WsServer"), QWebSocketServer::NonSecureMode, this);
        if (pWebSocketServer->listen(QHostAddress::AnyIPv4, port))
        {
            qDebug() << "Echoserver listening on port " << port;
            connect(pWebSocketServer, SIGNAL(newConnection()), this, SLOT(OnWsConnected()));
        }
    }
    else
    {
        pWebSocketServer = NULL;
    }
}

Output::~Output()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "Output", "~Output() called");
    if(NULL != m_pFormatCtx)
    {
        avformat_free_context(m_pFormatCtx);
        m_pFormatCtx = NULL;
    }
    if (NULL != m_pAVIOCtx) // note: the internal buffer could have changed, and be != pAvioCtxBuffer
    {
        av_freep(&m_pAVIOCtx->buffer);
        av_freep(&m_pAVIOCtx);
    }
    avcodec_parameters_free(&m_pEncoderParams);
    delete pWebSocketServer;
    DEBUG_MESSAGE0("Output", "~output() finished");
}

void Output::OnWsConnected()
{
    QWebSocket *pSocket = pWebSocketServer->nextPendingConnection();

    qDebug() << "Connection established from " << pSocket->peerAddress().toString();

    connect(pSocket, SIGNAL(disconnected()), this, SLOT(OnWsDisconnected()));

    clients << pSocket;

    // Sent initial data
    QByteArray array(4, 0);
    memcpy(array.data(), &m_framerate, 4);
    pSocket->sendBinaryMessage(array);

    qDebug() << "Number of active connections is " << clients.size();
}

void Output::OnWsDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug() << "WsDisconnected:" << pClient;

    if (pClient)
    {
        clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

static int WritePacketCallback(void* opaque, uint8_t* buf, int size)
{
    Output* pOutput = reinterpret_cast<Output*>(opaque);

    // Byte array with frame's actual pts
    QByteArray pts((char *)&pOutput->lastSentTimestamp, 8);

    Q_FOREACH (QWebSocket* client, pOutput->clients)
    {
        client->sendBinaryMessage(QByteArray((char *)buf, size).append(pts));
    }
    return size;
}

void Output::Open(AVCodecParameters* pCodecParams)
{
    DEBUG_MESSAGE0("Output", "ReallocContexts() called");
    m_outputInitialized = false;

    // Copy encoder parameters
    avcodec_parameters_copy(m_pEncoderParams, pCodecParams);

    if(NULL != m_pFormatCtx)
    {
        avformat_free_context(m_pFormatCtx);
        m_pFormatCtx = NULL;
    }

    // Trying to allocate output format context
    // Format should be .flv for streaming to rtmp
    ERROR_MESSAGE1(ERR_TYPE_MESSAGE, "Output", "Will try to open %s", m_outputUrl.toUtf8().constData());

    if (m_outputUrl.startsWith("rtp"))
    {
        avformat_alloc_output_context2(&m_pFormatCtx, NULL, "rtp", m_outputUrl.toUtf8().constData());
    }
    else if (m_outputUrl.startsWith("rtmp"))
    {
        avformat_alloc_output_context2(&m_pFormatCtx, NULL, "flv", m_outputUrl.toUtf8().constData());
    }
    else if (m_outputUrl.startsWith("ws"))
    {
        avformat_alloc_output_context2(&m_pFormatCtx, NULL, NULL, "out.h264");
    }
    else
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Unsupported output format");
        return;
    }

    if (NULL == m_pFormatCtx)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Failed to allocate output format context");
        return;
    }

    if (m_outputUrl.startsWith("ws"))
    {
        m_pAvioCtxBuffer = (uint8_t *)av_malloc(DEFAULT_AVIO_BUFSIZE);
        if (nullptr == m_pAvioCtxBuffer)
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Failed to allocate 256k avio internal buffer");
            return;
        }

        m_pAVIOCtx = avio_alloc_context(m_pAvioCtxBuffer, DEFAULT_AVIO_BUFSIZE, 1, this, nullptr, &WritePacketCallback, nullptr);
        if (nullptr == m_pAVIOCtx)
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Failed to allocate avio context");
            return;
        }

        m_pFormatCtx->pb = m_pAVIOCtx;
        m_pFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

        // Creating output stream
        m_pVideoStream = avformat_new_stream(m_pFormatCtx, NULL);
        if (NULL == m_pVideoStream)
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Could not create output video stream");
            return;
        }

        // Set video stream defaults
        m_pVideoStream->index = 0;
        m_pVideoStream->time_base = av_make_q(1, 90000);

        // Set encoding parameters
        avcodec_parameters_copy(m_pVideoStream->codecpar, pCodecParams);

        // Open bitstream filter
        if (0 != av_bsf_alloc(av_bsf_get_by_name("h264_mp4toannexb"), &m_pH264bsf))
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "ResultVideoOutput", "Could not allocate bitstream filter");
            return;
        }

        avcodec_parameters_copy(m_pH264bsf->par_in, pCodecParams);
        m_pH264bsf->time_base_in = m_pVideoStream->time_base;

        if (0 != av_bsf_init(m_pH264bsf))
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Could not initialize bitstream filter");
            return;
        }
    }
    else
    {
        // Creating output stream
        m_pVideoStream = avformat_new_stream(m_pFormatCtx, NULL);
        if (NULL == m_pVideoStream)
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Could not create output video stream");
            return;
        }

        // Set video stream defaults
        m_pVideoStream->index = 0;
        m_pVideoStream->time_base = av_make_q(1, 90000);

        // Set encoding parameters
        avcodec_parameters_copy(m_pVideoStream->codecpar, pCodecParams);

        // Open output file, if it is allowed by format
        if (0 > avio_open(&m_pFormatCtx->pb, m_outputUrl.toUtf8().constData(), AVIO_FLAG_WRITE))
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Failed to open avio for writing");
            return;
        }
    }

    // Printf format informtion
    av_dump_format(m_pFormatCtx, 0, m_outputUrl.toUtf8().constData(), 1);

    m_firstDts = AV_NOPTS_VALUE;

    m_outputInitialized = true;

    DEBUG_MESSAGE0("Output", "Context reallocated successfully");
}


static void FillSPSPPS(AVCodecParameters* pCodecContext, unsigned char* pCodedFrame, int frameLength)
{
    int i;
    int spsPpsDatalength = 0;
    int spsPpsStart = -1;
    int spsPpsEnd = -1;

    if (frameLength < 4)    // Frame is too short
    {
        return;
    }

    if (NULL != pCodecContext->extradata)   // sps and pps already filled
    {
        return;
    }

    // Find sps pps headers and mark their position
    for(i = 0; i < frameLength - 3; i++)
    {
        if ( (pCodedFrame[i + 0] == 0x0) &&
             (pCodedFrame[i + 1] == 0x0) &&
             (pCodedFrame[i + 2] == 0x1)
           )
        {
            // NAL header found

            if (pCodedFrame[i + 3] == 0x67) // SPS nal
            {
                spsPpsStart = i;
            }
            else
            {
                if (pCodedFrame[i + 3] != 0x68) // PPS should be also included
                {
                    spsPpsEnd = i;
                }
            }

            if ( spsPpsStart >= 0 && spsPpsEnd > 0)
            {
                break;  // Sps and pps found and can be copied to extradata
            }
        }
    }

    spsPpsDatalength = (spsPpsEnd - spsPpsStart);

    if ( (spsPpsEnd <= 0) || (spsPpsDatalength < 8))  // Invalid sps pps sequence size
    {
        return;
    }

    pCodecContext->extradata = (unsigned char *)malloc(spsPpsDatalength);
    pCodecContext->extradata_size = spsPpsDatalength;
    memcpy(pCodecContext->extradata, &pCodedFrame[spsPpsStart], spsPpsDatalength);
}

void Output::WritePacket(QSharedPointer<AVPacket> pInPacket)
{
    DEBUG_MESSAGE2("Output", "WritePacket() called, packet pts = %ld, size = %d", pInPacket->pts, pInPacket->size);

    if (!m_outputInitialized)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "Output context was not initialized properly");
        emit Broken();
        return;
    }

    // Waiting for key frame to start file
    if ((AV_NOPTS_VALUE == m_firstDts) && !(pInPacket->flags & AV_PKT_FLAG_KEY))
    {
        return;
    }

    if ((AV_NOPTS_VALUE == m_firstDts) && (pInPacket->flags & AV_PKT_FLAG_KEY))
    {
        // Write format header to file
        FillSPSPPS(m_pVideoStream->codecpar, pInPacket->data, pInPacket->size);

        if (0 > avformat_write_header(m_pFormatCtx, NULL))
        {
            ERROR_MESSAGE0(ERR_TYPE_ERROR, "Output", "avformat_write_header() failed");
            return;
        }
        m_firstDts = pInPacket->dts;
    }

    // Clone packet because it is ref-counted and will be unrefed in av_interleaved_write_frame
    AVPacket*   pPacket = av_packet_clone(pInPacket.data());
    pPacket->stream_index = m_pVideoStream->index;
    pPacket->dts -= m_firstDts;
    pPacket->pts -= m_firstDts;
    av_packet_rescale_ts(pPacket, AV_TIME_BASE_Q, m_pVideoStream->time_base);

    // Store frame timestamp
    // We are strongly counting on single call of WritePacketCallback
    // per each av_interleaved_write_frame() call
    lastSentTimestamp = pPacket->pts;

    if (m_pH264bsf != NULL)
    {
        av_bsf_send_packet(m_pH264bsf, pPacket);
        av_bsf_receive_packet(m_pH264bsf, pPacket);
    }

    int res = av_interleaved_write_frame(m_pFormatCtx, pPacket); // this call should also unref passed avpacket
    if (res < 0)
    {
        av_packet_unref(pPacket);

        char err[255] = {0};
        av_make_error_string(err, 255, res);
        ERROR_MESSAGE1(ERR_TYPE_ERROR, "Output", "av_interleaved_write_frame() error: %s", err);
        // Exit, if too many errors happened in a row
        if (m_numErrorsInRow++ > 300)
        {
            emit Broken();
        }
    }
    else
    {
        m_numErrorsInRow = 0;
    }

    DEBUG_MESSAGE0("Output", "WritePacket() finished");
}

void Output::Close()
{
    DEBUG_MESSAGE0("Output", "CloseOutput() called");
    if (m_outputInitialized)
    {
        av_write_trailer(m_pFormatCtx);
        if (m_pFormatCtx && !(m_pFormatCtx->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&m_pFormatCtx->pb);
        }
        m_outputInitialized = false;
    }
}

