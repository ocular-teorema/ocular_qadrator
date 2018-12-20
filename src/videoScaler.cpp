
#include "videoScaler.h"

VideoScaler::VideoScaler()
{
    m_pSwsContext = NULL;
    m_dstWidth = 0;
    m_dstHeight = 0;
}

VideoScaler::~VideoScaler()
{
    if(m_pSwsContext != NULL)
    {
        sws_freeContext(m_pSwsContext);
    }
}

void VideoScaler::scaleFrame(AVFrame *pInFrame, AVFrame *pOutFrame)
{
    if ((m_dstWidth != pOutFrame->width) || (m_dstHeight != pOutFrame->height))
    {
        // Reallocate scale context
        if (m_pSwsContext != NULL)
        {
            sws_freeContext(m_pSwsContext);
        }

        m_pSwsContext = sws_getContext(pInFrame->width,
                                       pInFrame->height,
                                       AV_PIX_FMT_YUV420P,
                                       pOutFrame->width,
                                       pOutFrame->height,
                                       AV_PIX_FMT_YUV420P,
                                       SWS_FAST_BILINEAR, NULL, NULL, NULL);

        m_dstWidth = pOutFrame->width;
        m_dstHeight = pOutFrame->height;
    }

    // Scale current frame
    sws_scale(m_pSwsContext,
              pInFrame->data,
              pInFrame->linesize,
              0,
              pInFrame->height,
              pOutFrame->data,
              pOutFrame->linesize);
}

