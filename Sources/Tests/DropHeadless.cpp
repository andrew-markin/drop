#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "Crypto.h"
#include "LocalNetwork.h"
#include "Uploads.h"
#include "WebServer.h"

// The server part of the application without the window, for end-to-end tests of the web client.
// It prints the link with the secret, which the application itself never does, so it is never
// shipped: it is built only with the tests.
int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption addressOption("address", "Address to listen at.", "address", "127.0.0.1");
    QCommandLineOption downloadsOption("downloads", "Directory to create drop directories in.", "path");
    QCommandLineOption chunkSizeOption("chunk-size", "Chunk size in bytes.", "size",
                                       QString::number(Uploads::DefaultChunkSize));

    parser.addOptions({addressOption, downloadsOption, chunkSizeOption});
    parser.process(application);

    QTextStream output(stdout);
    QTextStream errors(stderr);

    bool chunkSizeValid = false;
    qint64 chunkSize = parser.value(chunkSizeOption).toLongLong(&chunkSizeValid);

    if (!parser.isSet(downloadsOption) || !chunkSizeValid || chunkSize <= 0)
    {
        errors << parser.helpText() << Qt::endl;
        return EXIT_FAILURE;
    }

    if (!Crypto::init())
    {
        errors << "Unable to initialize libsodium" << Qt::endl;
        return EXIT_FAILURE;
    }

    QByteArray secret = Crypto::randomBytes(Crypto::SecretSize);
    Uploads uploads(QDir(parser.value(downloadsOption)), chunkSize);
    WebServer server(":/Web", &uploads, secret);

    if (!server.start(QHostAddress(parser.value(addressOption))))
    {
        errors << "Unable to start: " << server.errorString() << Qt::endl;
        return EXIT_FAILURE;
    }

    QString link = LocalNetwork::makeLink(server.serverAddress(), server.serverPort(), secret);
    output << QJsonDocument(QJsonObject({{"link", link}})).toJson(QJsonDocument::Compact) << Qt::endl;

    return application.exec();
}
