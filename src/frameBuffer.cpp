#include "frameBuffer.h"

FrameBuffer::FrameBuffer()
{
    m_readIndex = 0;
    m_writeIndex = 0;
    m_totalRead = 0;
    m_totalWritten = 0;
    m_size = FRAME_BUFFER_DEFAULT_SIZE;
    m_lastFrameTs = INT64_MAX;
    m_repeatedFrames = 0;

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
        int64_t  curTs = m_buffer[m_readIndex]->best_effort_timestamp;

        while (--numFramesInBuf && (curTs < (m_lastFrameTs + timeSpanMicros)))
        {
            m_readIndex = (m_readIndex + 1) % m_size;
            m_totalRead++;
            curTs = m_buffer[m_readIndex]->best_effort_timestamp;
        }

        if (m_lastFrameTs == curTs)
        {
            if (++m_repeatedFrames > REPEAT_FRAMES_MAX)
            {
                ERROR_MESSAGE0(ERR_TYPE_ERROR, "FrameBuffer", "Frame repeated too many times");
                return QSharedPointer<AVFrame>(NULL);
            }
        }
        else
        {
            m_repeatedFrames = 0;
            m_lastFrameTs = curTs;
        }
        return m_buffer[m_readIndex];
    }
    else
    {
        return QSharedPointer<AVFrame>(NULL);
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
