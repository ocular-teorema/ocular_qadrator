#ifndef KVADRATOR_H
#define KVADRATOR_H

#include <QObject>
#include <QVector>
#include <QJsonDocument>

#include "stream.h"
#include "output.h"
#include "builder.h"


class Kvadrator : public QObject
{
    Q_OBJECT
public:
    Kvadrator();
    ~Kvadrator();

    struct CamDesc
    {
        bool    isPresent;
        QString name;
        QString streamUrl;
    };

    struct Parameters
    {
        Parameters() { parsed = false; }

        bool                parsed;

        int                 port;
        QString             outputUrl1;
        QString             outputUrl2;

        int                 numCamsX;
        int                 numCamsY;

        QVector<CamDesc>    camDescriptors;

        BuilderParameters   builder;
    };

    Output*     pOutput1;
    Output*     pOutput2;
    Builder*    pBuilder;

    QVector<Stream*>                        streams;
    QVector<QSharedPointer<FrameBuffer> >   frameBufferPtrs;
    QVector<QThread* >                      streamThreads;

    void    Start();
    bool    Initialize();
    void    Deinitialize();

    bool    ParseParams(QJsonDocument paramsJsonDoc);

signals:
    void    Stopped();

public slots:
    void    StopAll();

private:
    bool        m_initialized;
    Parameters  m_params;
};

#endif // KVADRATOR_H
