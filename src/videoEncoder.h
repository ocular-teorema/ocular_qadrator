#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <QObject>
#include "videoScaler.h"

class VideoEncoder : public QObject
{
    Q_OBJECT
public:
    VideoEncoder(int width, int height, int fps, int crf);
    ~VideoEncoder();

    void Initialize();

signals:
    void    PacketReady(QSharedPointer<AVPacket> pPacket);  // This packet should be unrefed inside VideoEncoder
    void    NewParameters(AVCodecParameters* pParams);

public slots:
    void    EncodeFrame(AVFrame* pFrame);

protected:
    int                 m_crf;              /// Crf of output stream
    int                 m_framerate;        /// Framerate (for timebase calculation)
    int                 m_frameWidth;
    int                 m_frameHeight;
    uint64_t            m_currentPts;

    bool                m_isOpen;

    AVCodecContext*     m_pCodecContext;
    AVCodecParameters*  m_pCodecParams;

    void                CloseCodecContext();
};

#endif // VIDEOENCODER_H
