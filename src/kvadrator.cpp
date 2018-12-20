
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
    streams.resize(m_params.numCamsX * m_params.numCamsY);
    streamThreads.resize(m_params.numCamsX * m_params.numCamsY);
    frameBufferPtrs.resize(m_params.numCamsX * m_params.numCamsY);

    qRegisterMetaType< QSharedPointer<AVFrame > >("QSharedPointer<AVFrame >");

    for (int i = 0; i < m_params.numCamsX * m_params.numCamsY; i++)
    {
        if (m_params.camDescriptors[i].isPresent)
        {
            QThread* thread = new QThread();
            Stream*  stream = new Stream(m_params.camDescriptors[i].name,
                                         m_params.camDescriptors[i].streamUrl,
                                         m_params.builder.camWidth,
                                         m_params.builder.camHeight);
            // Connect thread slots
            QObject::connect(thread, SIGNAL(started()),  stream, SLOT(StartCapture()));
            QObject::connect(thread, SIGNAL(finished()), stream, SLOT(Deinitialize()));

            // Move each stream to its own thread
            stream->moveToThread(thread);

            frameBufferPtrs[i] = QSharedPointer<FrameBuffer>(new FrameBuffer());

            QObject::connect(stream, SIGNAL(FrameReady(QSharedPointer<AVFrame>)), frameBufferPtrs[i].data(), SLOT(AddFrame(QSharedPointer<AVFrame>)));
            QObject::connect(stream, SIGNAL(Reinit()), frameBufferPtrs[i].data(), SLOT(Reset()));

            streams[i] = stream;
            streamThreads[i] = thread;
        }
        else
        {
            streams[i] = NULL;
            streamThreads[i] = NULL;
            frameBufferPtrs[i] = QSharedPointer<FrameBuffer>(NULL);
        }
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

    for (int i = 0; i < streamThreads.size(); i++)
        delete streamThreads[i];

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

    params.port = jsonObject["port"].toInt();
    params.outputUrl1 = jsonObject["outputUrl1"].toString();
    params.outputUrl2 = jsonObject["outputUrl2"].toString();
    params.numCamsX = jsonObject["numCamsX"].toInt();
    params.numCamsY = jsonObject["numCamsY"].toInt();
    params.builder.numCamsX = params.numCamsX;
    params.builder.numCamsY = params.numCamsY;

    params.builder.outWidth = jsonObject["outputWidth"].toInt();
    params.builder.outHeight = jsonObject["outputHeight"].toInt();
    params.builder.crf = jsonObject["outputCrf"].toInt();
    params.builder.fps = jsonObject["outputFps"].toInt();
    params.builder.borderWidth = jsonObject["borderWidth"].toInt();

    // Calculate real scaled size for each cam
//    params.builder.camWidth = (params.builder.outWidth - (params.builder.borderWidth * (params.numCamsX + 1))) / params.numCamsX;
//    params.builder.camHeight = (params.builder.outHeight - (params.builder.borderWidth * (params.numCamsY + 1))) / params.numCamsY;
    params.builder.camWidth = params.builder.outWidth / params.numCamsX;
    params.builder.camHeight = params.builder.outHeight / params.numCamsY;

    QJsonArray jsonArray = jsonObject["camList"].toArray();

    foreach (const QJsonValue & value, jsonArray) {
        QJsonObject obj = value.toObject();
        CamDesc desc;
        desc.name       = obj["name"].toString();
        desc.isPresent  = obj["isPresent"].toBool();
        desc.streamUrl  = obj["streamUrl"].toString();
        params.camDescriptors.append(desc);
    }

    if (params.camDescriptors.size() != (params.numCamsX * params.numCamsY))
    {
        ERROR_MESSAGE0(ERR_TYPE_ERROR, "Kvadrator", "Error. camList array size not equal to numCamsX*numCamsY");
        return false;
    }
    m_params = params;
    m_params.parsed = true;

    return true;
}

