#ifndef VIDEOSCALER_H
#define VIDEOSCALER_H

#include "common.h"

/*
 * Class for convenient scaling video frames
 * Performs on-the-fly sws_context allocaltion
 * Can be initialized with no parameters
*/

class VideoScaler
{
public:
    VideoScaler();
    ~VideoScaler();

    void  scaleFrame(AVFrame* pInFrame, AVFrame *pOutFrame);

private:
    SwsContext*         m_pSwsContext;      /// Scaling context
    int                 m_dstWidth;         /// Last width
    int                 m_dstHeight;        /// Last height
};

#endif // VIDEOSCALER_H
