#include "frameBuffer.h"

FrameBuffer::FrameBuffer()
{
    m_buffer.resize(1);
    m_buffer.setSharable(true);
}

FrameBuffer::~FrameBuffer()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "FrameBuffer", "~FrameBuffer() called");
    m_buffer.clear();
}

QSharedPointer<AVFrame> FrameBuffer::GetFrame()
{
    // Lock this section
    QMutexLocker lock(&m_mutex);
    return m_buffer.empty() ? nullptr : m_buffer.front();
}

void FrameBuffer::AddFrame(QSharedPointer<AVFrame> pNewFrame)
{
    // Lock this section
    QMutexLocker lock(&m_mutex);
    m_buffer.clear();
    m_buffer.append(pNewFrame);
}

void FrameBuffer::Reset()
{
    // Lock this section
    QMutexLocker lock(&m_mutex);
}
