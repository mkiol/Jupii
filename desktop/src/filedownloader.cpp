#include "filedownloader.h"
#include <QDebug>

FileDownloader::FileDownloader(QUrl imageUrl, QObject *parent) :
    QObject(parent)
{
    connect(&m_WebCtrl, SIGNAL (finished(QNetworkReply*)),
            this, SLOT (fileDownloaded(QNetworkReply*)));

    QNetworkRequest request(imageUrl);
    m_WebCtrl.get(request);
}

FileDownloader::~FileDownloader() {}

void FileDownloader::fileDownloaded(QNetworkReply* pReply) {
    QNetworkReply::NetworkError error = pReply->error();

    if (error == QNetworkReply::NoError)
        m_DownloadedData = pReply->readAll();

    pReply->deleteLater();
    emit downloaded(error);
}

QByteArray FileDownloader::downloadedData() const {
    return m_DownloadedData;
}
