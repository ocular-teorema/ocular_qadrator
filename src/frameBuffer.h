#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

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

    QSharedPointer<AVFrame>  GetFrame();

public slots:
    void  AddFrame(QSharedPointer<AVFrame> pNewFrame);
    void  Reset();

private:
    QMutex       m_mutex;
    QVector<QSharedPointer<AVFrame> >  m_buffer;  /// Buffer with frame shared pointers
};

#endif // FRAMEBUFFER_H
