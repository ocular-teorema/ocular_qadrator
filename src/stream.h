#ifndef STREAM_H
#define STREAM_H

#include "common.h"
#include "videoScaler.h"

#include <QTimer>
#include <QThread>
#include <QObject>

#define READ_TIMEOUT_MSEC 20000

class Stream : public QObject
{
    Q_OBJECT
public:
    Stream(QString inputUrl, int targetWidth, int targetHeight);
    ~Stream();

    bool    Initialize();

    int64_t lastFrameReadMs;

signals:
    void    FrameReady(QSharedPointer<AVFrame> pNewFrame);
    void    Reinit();

public slots:
    void    Deinitialize();
    void    CaptureNewFrame();
    void    StartCapture() { while (!Initialize() && !m_stop); m_pCaptureTimer->start(); }
    void    StopCapture() { m_stop = true; m_pCaptureTimer->stop(); }

private:
    QString     m_inputUrl;         /// Input stream url
    QTimer*     m_pCaptureTimer;    /// Main loop timer. It will call CaptureNewFrame()
                                    /// as often as possible and keep all events alive.
                                    /// Must be created within the same thread Capture instance is running in
    VideoScaler m_scaler;
    int         m_targetWidth;      /// Width of output (scaled) frames
    int         m_targetHeight;     /// Height of output (scaled) frames

    AVFormatContext*    m_pInputContext;
    AVCodecContext*     m_pCodecContext;
    AVFrame*            m_pFrame;           /// Decoded frame locates here
    int                 m_videoStreamIndex;
    int                 m_errorsInRow;      /// Number of read or decode errors in a row
    bool                m_stop;             /// Flag to exit from while loop

    QSharedPointer<AVFrame> ScaleFrame(AVFrame* pInFrame);
};

#endif // STREAM_H
