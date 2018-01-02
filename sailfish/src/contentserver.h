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
    bool getContentUrl(const QString &id, QUrl &url, QString &meta, QString cUrl = "") const;
    Type getContentType(const QString &path) const;
    Q_INVOKABLE QStringList getExtensions(int type) const;
    QString getContentMime(const QString &path) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString idFromUrl(const QUrl &url) const;

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
    static ContentServer* m_instance;

    static const QHash<QString,QString> m_imgExtMap;
    static const QHash<QString,QString> m_musicExtMap;
    static const QHash<QString,QString> m_videoExtMap;

    QHttpServer* m_server;

    QByteArray m_data;
    const static qint64 qlen = 100000;
    const static int threadWait = 1;

    explicit ContentServer(QObject *parent = nullptr);
    bool getContentMeta(const QString &path, const QUrl &url, QString &meta) const;
    QByteArray encrypt(const QByteArray& data) const;
    QByteArray decrypt(const QByteArray& data) const;
    bool makeUrl(const QString& id, QUrl& url) const;
    bool getCoverArtUrl(const QString &path, QUrl &url) const;
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    bool seqWriteData(QFile &file, qint64 size, QHttpResponse *resp);
};

#endif // CONTENTSERVER_H
