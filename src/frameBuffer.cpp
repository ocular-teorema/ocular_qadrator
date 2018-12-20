#include "frameBuffer.h"

FrameBuffer::FrameBuffer()
{
    m_readIndex = 0;
    m_writeIndex = 0;
    m_totalRead = 0;
    m_totalWritten = 0;
    m_size = FRAME_BUFFER_DEFAULT_SIZE;
    m_lastFrameTs = INT64_MAX;

    m_buffer.resize(m_size);
    m_buffer.setSharable(true);
}

FrameBuffer::~FrameBuffer()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "FrameBuffer", "~FrameBuffer() called");
    m_buffer.clear();
}

QSharedPointer<AVFrame> FrameBuffer::GetFrame(int64_t timeSpanMicros)
{
    // Lock this section
    QMutexLocker lock(&m_mutex);

    int numFramesInBuf = m_totalWritten - m_totalRead;

    if (numFramesInBuf > 0)
    {
        int64_t  curTs = (m_buffer[m_readIndex]->best_effort_timestamp == AV_NOPTS_VALUE) ?
                 m_buffer[m_readIndex]->pkt_dts : m_buffer[m_readIndex]->best_effort_timestamp;

        while (--numFramesInBuf && (curTs < (m_lastFrameTs + timeSpanMicros)))
        {
            m_readIndex = (m_readIndex + 1) % m_size;
            m_totalRead++;

            curTs = (m_buffer[m_readIndex]->best_effort_timestamp == AV_NOPTS_VALUE) ?
                     m_buffer[m_readIndex]->pkt_dts : m_buffer[m_readIndex]->best_effort_timestamp;
        }

        m_lastFrameTs = curTs;
        return m_buffer[m_readIndex];
    }
    else
    {
        if (m_totalWritten)
        {
            int lastWrittenIndex = (m_writeIndex == 0) ? (m_size - 1) : (m_writeIndex - 1);
            m_lastFrameTs = (m_buffer[lastWrittenIndex]->best_effort_timestamp == AV_NOPTS_VALUE) ?
                     m_buffer[lastWrittenIndex]->pkt_dts : m_buffer[lastWrittenIndex]->best_effort_timestamp;
            return m_buffer[lastWrittenIndex];
        }
        else
        {
            return QSharedPointer<AVFrame>(NULL);
        }
    }
}

void FrameBuffer::AddFrame(QSharedPointer<AVFrame> pNewFrame)
{
    // Lock this section
    QMutexLocker lock(&m_mutex);
    // Check, if buffer was overflowed
    if (m_totalWritten - m_totalRead >= m_size)
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "FrameBuffer", "Frame buffer overflow");
        // Skip frames
        m_readIndex = m_writeIndex;
        m_totalRead = m_totalWritten;
    }
    // Add frame
    m_buffer[m_writeIndex] = pNewFrame;
    m_writeIndex = (m_writeIndex + 1) % m_size;
    m_totalWritten++;
}

void FrameBuffer::Reset()
{
    // Lock this section
    QMutexLocker lock(&m_mutex);
    m_readIndex = 0;
    m_writeIndex = 0;
    m_totalRead = 0;
    m_totalWritten = 0;
    m_lastFrameTs = INT64_MAX - 100000000;
}
