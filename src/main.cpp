
#include <QCoreApplication>
#include <QJsonDocument>
#include <QFile>

#include "common.h"
#include "kvadrator.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString iniFile = "params.json";

    if (argc > 1)
    {
        iniFile = argv[1];
    }

    QFile           jsonFile(iniFile);
    QJsonDocument   paramsJson;

    // Read json with parameters
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);

    if (jsonFile.isOpen())
    {
        paramsJson = QJsonDocument::fromJson(jsonFile.readAll());
        if (paramsJson.isEmpty())
        {
            ERROR_MESSAGE1(ERR_TYPE_CRITICAL, "Application", "Invalid parameters file %s", iniFile.toUtf8().constData());
            jsonFile.close();
            return -1;
        }
        jsonFile.close();
    }
    else
    {
        ERROR_MESSAGE1(ERR_TYPE_CRITICAL, "Application", "Unable to open parameters file %s", iniFile.toUtf8().constData());
        return -1;
    }

    // Do not needed since ffmpeg 4.x
    //av_register_all();

    // Create and run kvadrator
    Kvadrator kvadratorObject;

    if (!kvadratorObject.ParseParams(paramsJson))
    {
        ERROR_MESSAGE1(ERR_TYPE_CRITICAL, "Application", "Unable to parse parameters from %s", iniFile.toUtf8().constData());
        return -1;
    }

    if (!kvadratorObject.Initialize())
    {
        ERROR_MESSAGE0(ERR_TYPE_CRITICAL, "Application", "Unable to inititalize kvadrator");
        return -1;
    }

    QObject::connect(&kvadratorObject, SIGNAL(Stopped()), &a, SLOT(quit()));

    // Start processing
    kvadratorObject.Start();

    return a.exec();
}

