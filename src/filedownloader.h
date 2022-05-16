#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QUrl url, QObject *parent = nullptr);
    virtual ~FileDownloader();
    QByteArray downloadedData() const;
    QString contentType() const;

signals:
    void downloaded(bool error);

private slots:
    void fileDownloaded(QNetworkReply* reply);

private:
    QNetworkAccessManager m_nam;
    QByteArray m_data;
    QString m_contentType;
};

#endif // FILEDOWNLOADER_H
