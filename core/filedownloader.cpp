#include "filedownloader.h"
#include <QDebug>

FileDownloader::FileDownloader(QUrl url, QObject *parent) :
    QObject(parent)
{
    connect(&m_nam, &QNetworkAccessManager::finished,
            this, &FileDownloader::fileDownloaded);

    QNetworkRequest request(url);
    m_nam.get(request);
}

FileDownloader::~FileDownloader() {}

void FileDownloader::fileDownloaded(QNetworkReply* reply) {
    QNetworkReply::NetworkError error = reply->error();

    if (error == QNetworkReply::NoError) {
        m_data = reply->readAll();
        m_contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    }

    reply->deleteLater();
    emit downloaded(error);
}

QByteArray FileDownloader::downloadedData() const {
    return m_data;
}

QString FileDownloader::contentType() const {
    return m_contentType;
}

