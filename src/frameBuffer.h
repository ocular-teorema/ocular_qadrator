#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#define FRAME_BUFFER_DEFAULT_SIZE 60
#define REPEAT_FRAMES_MAX 100

#include <QMutex>
#include <QObject>
#include <QVector>

#include "common.h"

class FrameBuffer : public QObject
{
    Q_OBJECT
public:
    FrameBuffer();
    ~FrameBuffer();

    QSharedPointer<AVFrame>  GetFrame(int64_t timeSpanMicros);

public slots:
    void  AddFrame(QSharedPointer<AVFrame> pNewFrame);
    void  Reset();

private:
    unsigned int m_size;            /// Buffer size
    unsigned int m_readIndex;       /// Position for reading next frame
    unsigned int m_writeIndex;      /// Position for writing next frame
    unsigned int m_totalWritten;    /// How many frames already written
    unsigned int m_totalRead;       /// How many frames has been read
    unsigned int m_repeatedFrames;  /// How many frames has been repeated
    int64_t      m_lastFrameTs;     /// Time (in seconds) of last read frame
    int64_t      m_timespanAcc;     /// Accumulated timespan since last taken frame

    QMutex       m_mutex;

    QVector<QSharedPointer<AVFrame> >  m_buffer;  /// Buffer with frame shared pointers
};

#endif // FRAMEBUFFER_H
