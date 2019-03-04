#ifndef OUTPUT_H
#define OUTPUT_H

#include <QObject>
#include <QString>
#include <QtWebSockets>

#include "common.h"

#define  DEFAULT_AVIO_BUFSIZE    (1024*1024*16)    // 16m buffer should be enough for 1 gop fragment

class Output : public QObject
{
    Q_OBJECT
public:
    Output(QString outputURL, int fps);
    ~Output();

    // Output to framented mp4 using websockets
    QByteArray          initialFragments;
    QWebSocketServer*   pWebSocketServer;
    QList<QWebSocket*>  clients;

signals:
    void    Broken();

public slots:
    void    Close();
    void    Open(AVCodecParameters* pCodecParams);
    void    WritePacket(QSharedPointer<AVPacket> pInPacket);

    void    OnWsConnected();
    void    OnWsDisconnected();

private:
    QString             m_outputUrl;            /// Output stream location (network, file, etc...)
    int                 m_framerate;            /// Output fps
    int64_t             m_firstDts;             /// First packets timestamp
    bool                m_outputInitialized;    /// indicates, if output format initialized correctly
    int                 m_numErrorsInRow;

    AVBSFContext*       m_pH264bsf;
    AVFormatContext*    m_pFormatCtx;
    AVStream*           m_pVideoStream;
    AVCodecParameters*  m_pEncoderParams;
    AVIOContext*        m_pAVIOCtx;
    uint8_t*            m_pAvioCtxBuffer;
};

#endif // OUTPUT_H
