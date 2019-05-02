#ifndef BUILDER_H
#define BUILDER_H

#include <QTimer>
#include <QObject>
#include <QThread>
#include <QVector>

#include "common.h"
#include "frameBuffer.h"
#include "videoEncoder.h"

struct CamDescr
{
    int posX;
    int posY;
    int width;
    int height;
    QString streamUrl;
};

struct BuilderParameters
{
    int  outWidth;
    int  outHeight;
    int  fps;
    int  crf;
    QVector<CamDescr> camList;
};

class Builder : public QObject
{
    Q_OBJECT
public:
    Builder(BuilderParameters parameters);
    ~Builder();

    VideoEncoder*          pEncoder;

    QVector<QSharedPointer<FrameBuffer> >  sources;

signals:
    void FrameBuilt(AVFrame* pFrame);

public slots:
    void Start() { m_pProcessingTimer->start(); }
    void Stop()  { m_pProcessingTimer->stop();  }

private slots:
    void BuildFrame();

private:
    BuilderParameters   m_params;
    AVFrame*            m_pResultFrame;
    QTimer*             m_pProcessingTimer;

    void DrawFrameOnTarget(QSharedPointer<AVFrame> pFrame, int idx);
};

#endif // BUILDER_H
