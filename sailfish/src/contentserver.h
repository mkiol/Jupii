/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONTENTSERVER_H
#define CONTENTSERVER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QHash>
#include <QVector>
#include <QStringList>
#include <QFile>

#include <qhttpserver.h>
#include <qhttprequest.h>
#include <qhttpresponse.h>

#include "taskexecutor.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class ContentServer :
        public QObject,
        public TaskExecutor
{
    Q_OBJECT
public:
    enum Type {
        TypeUnknown = 0,
        TypeImage = 1,
        TypeMusic = 2,
        TypeVideo = 4,
        TypeDir = 128
    };

    static ContentServer* instance(QObject *parent = nullptr);
    static Type typeFromMime(const QString &mime);

    bool getContentUrl(const QString &id, QUrl &url, QString &meta, QString cUrl = "");
    Type getContentType(const QString &path);
    Q_INVOKABLE QStringList getExtensions(int type) const;
    QString getContentMime(const QString &path);
    Q_INVOKABLE QString idFromUrl(const QUrl &url) const;
    QString pathFromUrl(const QUrl &url) const;
    std::pair<QString,QString> pathAndTypeFromUrl(const QUrl &url) const;

signals:
    void do_sendEmptyResponse(QHttpResponse *resp, int code);
    void do_sendResponse(QHttpResponse *resp, int code, const QByteArray &data = "");
    void do_sendHead(QHttpResponse *resp, int code);
    void do_sendData(QHttpResponse *resp, QByteArray* data);
    void do_sendEnd(QHttpResponse *resp);

private slots:
    void asyncRequestHandler(QHttpRequest *req, QHttpResponse *resp);
    void sendEmptyResponse(QHttpResponse *resp, int code);
    void sendResponse(QHttpResponse *resp, int code, const QByteArray &data = "");
    void sendHead(QHttpResponse *resp, int code);
    void sendData(QHttpResponse *resp, QByteArray* data);
    void sendEnd(QHttpResponse *resp);

private:
    struct ItemMeta {
        QString trackerId;
        QUrl url;
        QString path;
        QString title;
        QString mime;
        QString comment;
        QString album;
        QString albumArt;
        QString artist;
        Type type;
        int duration;
        double bitrate;
        double sampleRate;
        int channels;
    };

    struct AvData {
        QString path;
        QString mime;
        QString type;
        QString extension;
        int bitrate;
        int channels;
        int64_t size;
    };

    static ContentServer* m_instance;

    static const QHash<QString,QString> m_imgExtMap;
    static const QHash<QString,QString> m_musicExtMap;
    static const QHash<QString,QString> m_videoExtMap;
    static const QString queryTemplate;
    static const QString dlnaOrgOpFlags;
    static const QString dlnaOrgCiFlags;
    static const QString dlnaOrgFlags;

    QHash<QString, ItemMeta> m_metaCache; // path => itemMeta

    QHttpServer* m_server;

    QByteArray m_data;
    const static qint64 qlen = 100000;
    const static int threadWait = 1;

    explicit ContentServer(QObject *parent = nullptr);
    bool getContentMeta(const QString &id, const QUrl &url, QString &meta);
    QByteArray encrypt(const QByteArray& data) const;
    QByteArray decrypt(const QByteArray& data) const;
    bool makeUrl(const QString& id, QUrl& url) const;
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    static bool extractAudio(const QString& path, AvData& data);
    static bool fillAvDataFromCodec(const AVCodecParameters* codec, const QString &videoPath, AvData &data);
    void stream(const QString& path, const QString &mime, QHttpRequest *req, QHttpResponse *resp);
    bool seqWriteData(QFile &file, qint64 size, QHttpResponse *resp);
    const QHash<QString, ItemMeta>::const_iterator makeItemMeta(const QString &path);
    const QHash<QString, ItemMeta>::const_iterator getMetaCacheIterator(const QString &path);
    QString getContentMimeByExtension(const QString &path);
    Type getContentTypeByExtension(const QString &path);
    void fillItemMeta(const QString& path, ItemMeta& item);
    static QString dlnaOrgPnFlags(const QString& mime);
    static QString dlnaContentFeaturesHeader(const QString& mime);
};

#endif // CONTENTSERVER_H
