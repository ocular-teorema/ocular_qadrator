#include "builder.h"

#include <opencv/cv.h>
#include <opencv2/core/core.hpp>

Builder::Builder(BuilderParameters parameters) :
    QObject(NULL),
    m_params(parameters),
    m_pResultFrame(NULL)
{
    m_pResultFrame = av_frame_alloc();

    // Allocate memory for result frame
    m_pResultFrame->format = AV_PIX_FMT_YUV420P;
    m_pResultFrame->width = parameters.outWidth;
    m_pResultFrame->height = parameters.outHeight;

    if (0 > av_frame_get_buffer(m_pResultFrame, 32))  // allocates aligned buffer for y,u,v planes
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Builder", "Failed to alocate frame buffer");
    }

    m_pProcessingTimer = new QTimer;
    m_pProcessingTimer->setTimerType(Qt::PreciseTimer);
    m_pProcessingTimer->setInterval((int)(1000.0f / (float)parameters.fps + 0.5f));
    QObject::connect(m_pProcessingTimer, SIGNAL(timeout()), this, SLOT(BuildFrame()));

    pEncoder = new VideoEncoder(parameters.outWidth, parameters.outHeight, parameters.fps, parameters.crf);
    QObject::connect(this, SIGNAL(FrameBuilt(AVFrame*)), pEncoder, SLOT(EncodeFrame(AVFrame*)));
}

Builder::~Builder()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "Builder", "~Builder() called");
    if (m_pResultFrame)
    {
        av_frame_unref(m_pResultFrame);
        av_frame_free(&m_pResultFrame);
    }
    delete pEncoder;
    delete m_pProcessingTimer;
}

void Builder::BuildFrame()
{
    int64_t timeSpan = 1000000 / m_params.fps;

    // Make current frame black first
    memset(m_pResultFrame->data[0], 16, m_pResultFrame->height * m_pResultFrame->linesize[0]);
    memset(m_pResultFrame->data[1], 128,  (m_pResultFrame->height >> 1) * m_pResultFrame->linesize[1]);
    memset(m_pResultFrame->data[2], 128,  (m_pResultFrame->height >> 1) * m_pResultFrame->linesize[1]);

    for (int camY = 0; camY < m_params.numCamsY; camY++)
    {
        for (int camX = 0; camX < m_params.numCamsX; camX++)
        {
            // If we have source for current position - read frame from it
            if (!sources[camY*m_params.numCamsX + camX].isNull())
            {
                DrawFrameOnTarget(sources[camY*m_params.numCamsX + camX]->GetFrame(timeSpan), camX, camY);
            }
        }
    }
    emit FrameBuilt(m_pResultFrame);
}

void Builder::DrawFrameOnTarget(QSharedPointer<AVFrame> pFrame, int row, int col)
{
    if (pFrame.isNull())
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Builder", "Null frame received from frame buffer");
        return;
    }

//    int posX = m_params.camWidth  * row + m_params.borderWidth * (row + 1);
//    int posY = m_params.camHeight * col + m_params.borderWidth * (col + 1);
    int posX = m_params.camWidth  * row + ((m_params.camWidth - pFrame->width) >> 1);
    int posY = m_params.camHeight * col + ((m_params.camHeight - pFrame->height) >> 1);

    cv::Mat     dstY(m_pResultFrame->height,    m_pResultFrame->width,    CV_8UC1, m_pResultFrame->data[0], m_pResultFrame->linesize[0]);
    cv::Mat     dstU(m_pResultFrame->height>>1, m_pResultFrame->width>>1, CV_8UC1, m_pResultFrame->data[1], m_pResultFrame->linesize[1]);
    cv::Mat     dstV(m_pResultFrame->height>>1, m_pResultFrame->width>>1, CV_8UC1, m_pResultFrame->data[2], m_pResultFrame->linesize[1]);

    cv::Mat     srcY(pFrame->height,    pFrame->width,    CV_8UC1, pFrame->data[0], pFrame->linesize[0]);
    cv::Mat     srcU(pFrame->height>>1, pFrame->width>>1, CV_8UC1, pFrame->data[1], pFrame->linesize[1]);
    cv::Mat     srcV(pFrame->height>>1, pFrame->width>>1, CV_8UC1, pFrame->data[2], pFrame->linesize[1]);

    srcY.copyTo(dstY(cv::Rect(posX, posY, pFrame->width, pFrame->height)));
    srcU.copyTo(dstU(cv::Rect(posX>>1, posY>>1, pFrame->width>>1, pFrame->height>>1)));
    srcV.copyTo(dstV(cv::Rect(posX>>1, posY>>1, pFrame->width>>1, pFrame->height>>1)));
}
