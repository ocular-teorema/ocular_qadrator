
#include <QJsonArray>
#include <QJsonObject>

#include "kvadrator.h"


Kvadrator::Kvadrator() :
    QObject(NULL),
    pOutput1(NULL),
    pOutput2(NULL),
    pBuilder(NULL),
    m_initialized(false)
{

}

Kvadrator::~Kvadrator()
{
    ERROR_MESSAGE0(ERR_TYPE_MESSAGE, "Kvadrator", "~Kvadrator() called");
    Deinitialize();
}

bool Kvadrator::Initialize()
{
    m_initialized = false;

    if (!m_params.parsed)
    {
        return false;
    }

    // 1. Create builder
    pBuilder = new Builder(m_params.builder);

    // 2. Create Output2
    pOutput1 = new Output(m_params.outputUrl1, m_params.builder.fps);
    pOutput2 = new Output(m_params.outputUrl2, m_params.builder.fps);

    // 3. Connect them
    qRegisterMetaType< QSharedPointer<AVPacket > >("QSharedPointer<AVPacket >");
    QObject::connect(pBuilder->pEncoder, SIGNAL(NewParameters(AVCodecParameters*)), pOutput1, SLOT(Open(AVCodecParameters*)));
    QObject::connect(pBuilder->pEncoder, SIGNAL(PacketReady(QSharedPointer<AVPacket>)), pOutput1, SLOT(WritePacket(QSharedPointer<AVPacket>)));
    QObject::connect(pOutput1, SIGNAL(Broken()), this, SLOT(StopAll()));

    QObject::connect(pBuilder->pEncoder, SIGNAL(NewParameters(AVCodecParameters*)), pOutput2, SLOT(Open(AVCodecParameters*)));
    QObject::connect(pBuilder->pEncoder, SIGNAL(PacketReady(QSharedPointer<AVPacket>)), pOutput2, SLOT(WritePacket(QSharedPointer<AVPacket>)));
    QObject::connect(pOutput2, SIGNAL(Broken()), this, SLOT(StopAll()));

    // 4. Initialize encoder (should be performed only after signals are connected)
    pBuilder->pEncoder->Initialize();

    // 5. Create streams and buffers
    streams.resize(m_params.numCams);
    streamThreads.resize(m_params.numCams);
    frameBufferPtrs.resize(m_params.numCams);

    qRegisterMetaType< QSharedPointer<AVFrame > >("QSharedPointer<AVFrame >");

    for (int i = 0; i < m_params.numCams; i++)
    {
        QThread* thread = new QThread();
        Stream*  stream = new Stream(m_params.builder.camList[i].streamUrl,
                                     m_params.builder.camList[i].width,
                                     m_params.builder.camList[i].height);
        // Connect thread slots
        QObject::connect(thread, SIGNAL(started()),  stream, SLOT(StartCapture()));
        QObject::connect(thread, SIGNAL(finished()), stream, SLOT(deleteLater()));
        QObject::connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

        // Move each stream to its own thread
        stream->moveToThread(thread);

        frameBufferPtrs[i] = QSharedPointer<FrameBuffer>(new FrameBuffer());

        QObject::connect(stream, SIGNAL(FrameReady(QSharedPointer<AVFrame>)), frameBufferPtrs[i].data(), SLOT(AddFrame(QSharedPointer<AVFrame>)));
        QObject::connect(stream, SIGNAL(Reinit()), frameBufferPtrs[i].data(), SLOT(Reset()));

        streams[i] = stream;
        streamThreads[i] = thread;
    }

    // Copy frame buffers to Builder's sources
    pBuilder->sources = frameBufferPtrs;

    return true;
}

void Kvadrator::Deinitialize()
{
    delete pBuilder;
    delete pOutput1;
    delete pOutput2;

    streams.clear();
    frameBufferPtrs.clear();
    streamThreads.clear();
}

void Kvadrator::Start()
{
    for (int i = 0; i < streams.size(); i++)
    {
        if (NULL != streamThreads[i])
        {
            streamThreads[i]->start();
        }
    }
    pBuilder->Start();
}

void Kvadrator::StopAll()
{
    for (int i = 0; i < streams.size(); i++)
    {
        if (NULL != streams[i])
        {
            QMetaObject::invokeMethod(streams[i], "StopCapture", Qt::QueuedConnection);
        }

        if (NULL != streamThreads[i])
        {
            streamThreads[i]->quit();
            streamThreads[i]->wait(1000);
        }
    }
    pBuilder->Stop();
    pOutput1->Close();
    pOutput2->Close();
    emit Stopped();
}

bool Kvadrator::ParseParams(QJsonDocument paramsJsonDoc)
{
    Parameters  params;

    QJsonObject jsonObject = paramsJsonDoc.object();

    params.outputUrl1 = jsonObject["outputUrl1"].toString();
    params.outputUrl2 = jsonObject["outputUrl2"].toString();

    params.builder.outWidth = jsonObject["outputWidth"].toInt();
    params.builder.outHeight = jsonObject["outputHeight"].toInt();
    params.builder.crf = jsonObject["outputCrf"].toInt();
    params.builder.fps = jsonObject["outputFps"].toInt();

    QJsonArray jsonArray = jsonObject["camList"].toArray();

    foreach (const QJsonValue & value, jsonArray) {
        QJsonObject obj = value.toObject();
        CamDescr desc;
        desc.posX = obj["posX"].toInt();
        desc.posY = obj["posY"].toInt();
        desc.width = obj["width"].toInt();
        desc.height = obj["height"].toInt();
        desc.streamUrl  = obj["streamUrl"].toString();

//        // Convert pos and size to real output resolution
//        desc.posX *= params.builder.outWidth >> 3;
//        desc.posY *= params.builder.outHeight >> 3;
//        desc.width *= params.builder.outWidth >> 3;
//        desc.height *= params.builder.outHeight >> 3;

        params.builder.camList.append(desc);
    }
    m_params = params;
    m_params.parsed = true;
    m_params.numCams = params.builder.camList.size();
    return true;
}

