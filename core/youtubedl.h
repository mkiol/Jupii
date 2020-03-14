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
    static YoutubeDl* instance();
    bool enabled();
    void terminate(const QUrl &url);

public slots:
    bool findStream(const QUrl &url);
    void terminateAll();

signals:
    void newStream(QUrl url, QUrl streamUrl, QString title);

private:
    static YoutubeDl* m_instance;
    static const int httpTimeout = 100000;

    QString binPath;
    QHash<QProcess*, QUrl> procToUrl;
    QHash<QProcess*, QByteArray> procToData;
    QHash<QUrl, QProcess*> urlToProc;
    explicit YoutubeDl(QObject* parent = nullptr);
#ifdef SAILFISH
    void install();
    void update();
    void downloadBin();
#endif

private slots:
    void handleProcFinished(int exitCode);
    void handleProcError(QProcess::ProcessError error);
    void handleReadyRead();
#ifdef SAILFISH
    void handleBinDownloaded();
#endif
};

#endif // YOUTUBEDL_H
