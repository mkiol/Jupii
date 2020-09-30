/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef YOUTUBEDL_H
#define YOUTUBEDL_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QProcess>
#include <QHash>
#include <memory>

class YoutubeDl: public QObject
{
    Q_OBJECT
public:
    enum Errors {
        No_Error = 0,
        FindStream_Error,
        DownloadBin_Error,
        UpdateBin_Error
    };
    Q_ENUM(Errors)

    static YoutubeDl* instance();
    bool enabled();
    void terminate(const QUrl &url);

public slots:
    bool findStream(const QUrl &url);
    void terminateAll();

signals:
    void newStream(const QUrl &url, const QUrl &streamUrl, const QString &title);
    void error(Errors code);

private:
    static YoutubeDl* m_instance;
    static const int httpTimeout = 100000;

    QString binPath;
    QHash<QProcess*, QUrl> procToUrl;
    QHash<QProcess*, QByteArray> procToData;
    QHash<QUrl, QProcess*> urlToProc;
    explicit YoutubeDl(QObject* parent = nullptr);
    void install();
    void update();
    void downloadBin();

private slots:
    void handleProcFinished(int exitCode);
    void handleProcError(QProcess::ProcessError error);
    void handleReadyRead();
    void handleBinDownloaded();
    void handleUpdateProcFinished(int exitCode);
    void handleUpdateProcError(QProcess::ProcessError error);
};

#endif // YOUTUBEDL_H
