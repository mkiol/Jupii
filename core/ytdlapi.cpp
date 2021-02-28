/* Copyright (C) 2020-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ytdlapi.h"

#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QIODevice>
#include <QTimer>
#include <QEventLoop>

#include "utils.h"
#include "settings.h"
#include "downloader.h"

bool YtdlApi::enabled = true;
QString YtdlApi::binPath;

YtdlApi::YtdlApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject(parent), nam(nam)
{
    init();
}

bool YtdlApi::ok() const
{
    return enabled && !binPath.isEmpty();
}

bool YtdlApi::downloadBin()
{
    qDebug() << "Downloading youtube-dl";

    auto data = Downloader{nam}.downloadData(QUrl{"https://yt-dl.org/downloads/latest/youtube-dl"});
    if (!data.isEmpty()) {
        auto binPath = defaultBinPath();
        if (QDir{}.mkpath(QFileInfo{binPath}.dir().absolutePath())) {
            if (Utils::writeToFile(binPath, data, true)) {
                qDebug() << "Ytdl bin successfully downloaded to:" << binPath;
                QFile{binPath}.setPermissions(QFileDevice::ExeUser|QFileDevice::ExeGroup|
                                   QFileDevice::ExeOther|QFileDevice::ReadUser|
                                   QFileDevice::ReadGroup|QFileDevice::ReadOther|
                                   QFileDevice::WriteUser);
                return true;
            }
        }
    }

    qWarning() << "Cannot download youtube-dl";

    return false;
}

bool YtdlApi::checkBin(const QString& binPath)
{
    auto proc = new QProcess();

    QTimer::singleShot(timeout, proc, &QProcess::terminate);

    QEventLoop loop;
    connect(proc, static_cast<void(QProcess::*)(int)>(&QProcess::finished), this, [&loop](int exitCode) {
        qDebug() << "Ytdl check process finished:" << exitCode;
        loop.exit(exitCode);
    });
    connect(proc, &QProcess::errorOccurred, this, [&loop](QProcess::ProcessError error) {
        qDebug() << "Ytdl check process error:" << error;
        loop.exit(1);
    });

    proc->start(binPath + " --version", QIODevice::ReadOnly);

    qDebug() << "Ytdl check process started";

    auto ret = loop.exec();

    proc->deleteLater();

    return ret == 0;
}

void YtdlApi::update()
{
    if (!ok())
        return;

    auto proc = new QProcess();

    QTimer::singleShot(timeout, proc, &QProcess::terminate);

    QEventLoop loop;
    connect(proc, static_cast<void(QProcess::*)(int)>(&QProcess::finished), this, [&loop](int exitCode) {
        qDebug() << "Ytdl update process finished:" << exitCode;
        loop.quit();
    });
    connect(proc, &QProcess::errorOccurred, this, [&loop](QProcess::ProcessError error) {
        qDebug() << "Ytdl update process error:" << error;
        loop.quit();
    });

    proc->start(binPath + " --update", QIODevice::ReadOnly);

    qDebug() << "Ytdl update process started";

    loop.exec();

    proc->deleteLater();
}

QString YtdlApi::defaultBinPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/youtube-dl";
}

bool YtdlApi::init()
{
    if (ok())
        return true;

    binPath.clear();

    if (Settings::instance()->globalYtdl() && checkBin("youtube-dl")) {
        enabled = true;
        binPath = "youtube-dl";
    } else {
        QFile bin{defaultBinPath()};
        if (!bin.exists() && !downloadBin()) {
            enabled = false;
            return false;
        }

        enabled = true;
        binPath = bin.fileName();
        update();
    }

    qDebug() << "Using ytdl bin:" << binPath;

    return true;
}

YtdlApi::Track YtdlApi::track(const QUrl &url)
{
    Track track;

    if (!ok())
        return track;

    auto proc = new QProcess();

    QTimer::singleShot(timeout, proc, &QProcess::terminate);

    QEventLoop loop;
    connect(proc, static_cast<void(QProcess::*)(int)>(&QProcess::finished), this, [&loop](int exitCode) {
        qDebug() << "Ytdl process finished:" << exitCode;
        loop.quit();
    });
    connect(proc, &QProcess::errorOccurred, this, [&loop](QProcess::ProcessError) {
        loop.quit();
    });

    QByteArray data;
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, &data]() {
        proc->setReadChannel(QProcess::StandardOutput);
        data.push_back(proc->readAll());
        if (data.split('\n').size() > 2) {
            qDebug() << "All data received from youtube-dl process";
            proc->terminate();
        }
    });

    const auto command = binPath + " --get-title --get-url --no-call-home --format best \"" + url.toString() + "\"";
#ifdef QT_DEBUG
    qDebug() << "Ytdl command:" << command;
#endif
    proc->start(command, QIODevice::ReadOnly);

    loop.exec();

    proc->deleteLater();

#ifdef QT_DEBUG
    qDebug() << "Ytdl data:" << data;
#endif
    if (auto out = data.split('\n'); out.size() >= 2) {
        if (QUrl streamUrl{out.at(1)}; Utils::isUrlValid(streamUrl)) {
            track.streamUrl = std::move(streamUrl);
            track.webUrl = url;
            track.title = {out.at(0)};
        }
    }

    return track;
}
