/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QIODevice>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>

#include "youtubedl.h"
#include "utils.h"
#include "connectivitydetector.h"

YoutubeDl* YoutubeDl::m_instance = nullptr;

YoutubeDl::YoutubeDl(QObject *parent) : QObject(parent)
{
    install();
    connect(ConnectivityDetector::instance(), &ConnectivityDetector::networkStateChanged,
            this, &YoutubeDl::terminateAll, Qt::QueuedConnection);
}

YoutubeDl* YoutubeDl::instance()
{
    if (YoutubeDl::m_instance == nullptr) {
        YoutubeDl::m_instance = new YoutubeDl();
    }

    return YoutubeDl::m_instance;
}

bool YoutubeDl::enabled()
{
    return !binPath.isEmpty();
}

void YoutubeDl::terminateAll()
{
    auto procs = procToUrl.keys();
    for (auto proc : procs) {
        proc->terminate();
    }
}

void YoutubeDl::terminate(const QUrl &url)
{
    if (urlToProc.contains(url)) {
        urlToProc.value(url)->terminate();
    }
}

void YoutubeDl::downloadBin()
{
    qDebug() << "Downloading youtube-dl";
    QNetworkRequest request(QUrl("https://yt-dl.org/downloads/latest/youtube-dl"));
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    auto reply = Utils::instance()->nam->get(request);
    QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, &YoutubeDl::handleBinDownloaded);
}

void YoutubeDl::handleBinDownloaded()
{
    auto reply = dynamic_cast<QNetworkReply*>(sender());
    if (reply) {
        auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        auto err = reply->error();
        auto data = reply->readAll();

        qDebug() << "Youtube-dl download finished:" << code << reason << err;

        if (err == QNetworkReply::NoError && code > 199 && code < 300 && !data.isEmpty()) {
            auto dataDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
            if (QDir{}.mkpath(dataDir)) {
                auto binPath = dataDir + "/youtube-dl";
                if (Utils::writeToFile(binPath, data, true)) {
                    qDebug() << "Youtube-dl bin successfully downloaded to:" << binPath;
                    QFile{binPath}.setPermissions(QFileDevice::ExeUser|QFileDevice::ExeGroup|
                                       QFileDevice::ExeOther|QFileDevice::ReadUser|
                                       QFileDevice::ReadGroup|QFileDevice::ReadOther|
                                       QFileDevice::WriteUser);
                    this->binPath = binPath;
                }
            }
        } else {
            qWarning() << "Cannot download youtube-dl";
            emit error(DownloadBin_Error);
        }

        reply->deleteLater();
    }
}

void YoutubeDl::update()
{
    if (!enabled()) {
        qWarning() << "Youtube-dl is not enabled";
        return;
    }

    auto proc = new QProcess();
    proc->start(binPath + " --update", QIODevice::ReadOnly);
    connect(proc, static_cast<void(QProcess::*)(int)>(&QProcess::finished),
            this, &YoutubeDl::handleUpdateProcFinished);
    connect(proc, &QProcess::errorOccurred, this, &YoutubeDl::handleUpdateProcError);

    qDebug() << "Youtube-dl update process started:" << proc;
}

void YoutubeDl::handleUpdateProcFinished(int exitCode)
{
    qDebug() << "Youtube-dl update process finished with exit code:" << exitCode;
    qobject_cast<QProcess*>(sender())->deleteLater();
    if (exitCode != 0) {
        emit error(UpdateBin_Error);
    }
}

void YoutubeDl::handleUpdateProcError(QProcess::ProcessError error)
{
    qDebug() << "Youtube-dl update process error:" << error;

    if (error == QProcess::FailedToStart) {
        qWarning() << "Youtube-dl update process failed to start";
        qobject_cast<QProcess*>(sender())->deleteLater();
        emit this->error(UpdateBin_Error);
    }
}

void YoutubeDl::install()
{
    binPath.clear();

    QFile bin{QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/youtube-dl"};
    if (!bin.exists()) {
        downloadBin();
        return;
    }

    binPath = bin.fileName();
    qDebug() << "Using youtube-dl bin:" << binPath;
    update();
}

void YoutubeDl::handleProcFinished(int exitCode)
{
    qDebug() << "Youtube-dl process finished with exit code:" << exitCode;

    auto proc = qobject_cast<QProcess*>(sender());
    if (procToUrl.contains(proc)) {
        auto url = procToUrl.value(proc);
        auto out = procToData[proc].split('\n');
        procToUrl.remove(proc);
        procToData.remove(proc);
        urlToProc.remove(url);

        if (exitCode > 0)
            qDebug() << "Youtube-dl std error:" << proc->readAllStandardError();

        if (out.size() < 2) {
            qWarning() << "Youtube-dl output is empty";
        } else {
            QUrl streamUrl(out.at(1));
            if (Utils::isUrlValid(streamUrl)) {
                emit newStream(url, streamUrl, QString(out.at(0)));
                proc->deleteLater();
                return; // ok
            }
        }

        // error
        emit newStream(url, QUrl(), QString());
        proc->deleteLater();
        emit error(FindStream_Error);
    }
}

void YoutubeDl::handleReadyRead()
{
    auto proc = qobject_cast<QProcess*>(sender());
    proc->setReadChannel(QProcess::StandardOutput);
    procToData[proc].push_back(proc->readAll());
    if (procToData[proc].split('\n').size() > 2) {
        qDebug() << "All data received from youtube-dl process";
        proc->terminate();
    }
}

void YoutubeDl::handleProcError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        qWarning() << "Youtube-dl process failed to start";
        auto proc = qobject_cast<QProcess*>(sender());
        const auto url = procToUrl.value(proc);
        procToUrl.remove(proc);
        procToData.remove(proc);
        urlToProc.remove(url);
        proc->deleteLater();
        emit newStream(url, {}, {});
        emit this->error(FindStream_Error);
    }
}

bool YoutubeDl::findStream(const QUrl &url)
{
    if (!enabled()) {
        qWarning() << "Youtube-dl is not enabled";
        return false;
    }

    auto proc = new QProcess();
    connect(proc, static_cast<void(QProcess::*)(int)>(&QProcess::finished),
            this, &YoutubeDl::handleProcFinished);
    connect(proc, &QProcess::errorOccurred, this, &YoutubeDl::handleProcError);
    connect(proc, &QProcess::readyReadStandardOutput, this, &YoutubeDl::handleReadyRead);
    procToUrl.insert(proc, url);
    urlToProc.insert(url, proc);

    const auto command = binPath + " --get-title --get-url --no-call-home --format best \"" + url.toString() + "\"";
#ifdef QT_DEBUG
    qDebug() << "youtube-dl command:" << command;
#endif
    proc->start(command, QIODevice::ReadOnly);

    qDebug() << "process started:" << proc;
    return true;
}
