/* Copyright (C) 2021-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "downloader.h"

#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

static const QByteArray userAgent = QByteArrayLiteral(
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_6) AppleWebKit/605.1.15 "
    "(KHTML, like Gecko) Version/12.1.2 Safari/605.1.15");

static void setRequestProps(QNetworkRequest &request) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    request.setRawHeader(QByteArrayLiteral("Range"),
                         QByteArrayLiteral("bytes=0-"));
    request.setRawHeader(QByteArrayLiteral("Connection"),
                         QByteArrayLiteral("close"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"), userAgent);
}

Downloader::Downloader(std::shared_ptr<QNetworkAccessManager> nam,
                       QObject *parent)
    : QObject{parent}, mProgress{0, 0}, nam{std::move(nam)} {}

Downloader::~Downloader() {}

bool Downloader::downloadToFile(const QUrl &url, const QString &outputPath,
                                int timeout) {
    if (busy()) {
        qWarning() << "downloaded is busy";
        return false;
    }

#ifdef QT_DEBUG
    qDebug() << "download to file:" << url.toString() << outputPath;
#endif

    mCancelRequested = false;

    QFile output{outputPath};
    if (!output.open(QIODevice::WriteOnly)) {
        qWarning() << "cannot open output file for writing:" << outputPath;
        return false;
    }

    QNetworkRequest request;
    request.setUrl(url);
    setRequestProps(request);

    std::unique_ptr<QNetworkAccessManager> priv_nam;

    if (nam) {
        mReply = nam->get(request);
    } else {
        priv_nam = std::make_unique<QNetworkAccessManager>();
        mReply = priv_nam->get(request);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeout);

    auto con1 = connect(
        &timer, &QTimer::timeout, this,
        [this] {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;
            reply->abort();
        },
        Qt::QueuedConnection);
    auto con2 = connect(
        mReply, &QNetworkReply::readyRead, this,
        [this, &output] {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;
            if (mCancelRequested ||
                QThread::currentThread()->isInterruptionRequested()) {
                qWarning() << "cancel was requested";
                reply->abort();
                return;
            }
            output.write(reply->readAll());
        },
        Qt::QueuedConnection);
    auto con3 = connect(
        mReply, &QNetworkReply::finished, this,
        [&loop, &timer] {
            timer.stop();
            loop.quit();
        },
        Qt::QueuedConnection);
    auto con4 = connect(
        mReply, &QNetworkReply::downloadProgress, this,
        [this](qint64 bytesReceived, qint64 bytesTotal) {
            mProgress = {bytesReceived, bytesTotal};
            emit progressChanged();
        },
        Qt::QueuedConnection);
    timer.start();
    loop.exec();
    timer.stop();
    disconnect(con1);
    disconnect(con2);
    disconnect(con3);
    disconnect(con4);

#ifdef QT_DEBUG
    qDebug() << "download finished";
#endif

    if (auto err = mReply->error(); err != QNetworkReply::NoError) {
        qWarning() << "error:" << err << mReply->url();
        mReply->deleteLater();
        mReply = nullptr;
        output.close();
        QFile::remove(outputPath);
        return false;
    }

    output.write(mReply->readAll());
    output.close();
    mReply->deleteLater();
    mReply = nullptr;

    mProgress.first = mProgress.second;
    emit progressChanged();

    if (output.size() == 0) {
        qWarning() << "downloaded file is empty";
        QFile::remove(outputPath);
        return false;
    }

    return true;
}

QByteArray Downloader::downloadData(const QUrl &url, int timeout) {
    if (busy()) {
        qWarning() << "downloaded is busy";
        return {};
    }

#ifdef QT_DEBUG
    qDebug() << "download data:" << url;
#endif

    mCancelRequested = false;
    QNetworkRequest request;
    request.setUrl(url);
    setRequestProps(request);

    std::unique_ptr<QNetworkAccessManager> priv_nam;

    if (nam) {
        mReply = nam->get(request);
    } else {
        priv_nam = std::make_unique<QNetworkAccessManager>();
        mReply = priv_nam->get(request);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeout);

    auto con1 = connect(
        &timer, &QTimer::timeout, this,
        [this] {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;
            reply->abort();
        },
        Qt::QueuedConnection);
    auto con2 = connect(
        mReply, &QNetworkReply::readyRead, this,
        [this] {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;
            if (mCancelRequested ||
                QThread::currentThread()->isInterruptionRequested()) {
                qWarning() << "cancel was requested";
                reply->abort();
            }
        },
        Qt::QueuedConnection);
    auto con3 = connect(
        mReply, &QNetworkReply::finished, this,
        [&loop, &timer] {
            timer.stop();
            loop.quit();
        },
        Qt::QueuedConnection);
    auto con4 = connect(
        mReply, &QNetworkReply::downloadProgress, this,
        [this](qint64 bytesReceived, qint64 bytesTotal) {
            mProgress = {bytesReceived, bytesTotal};
            emit progressChanged();
        },
        Qt::QueuedConnection);

    timer.start();
    loop.exec();
    timer.stop();
    disconnect(con1);
    disconnect(con2);
    disconnect(con3);
    disconnect(con4);

#ifdef QT_DEBUG
    qDebug() << "download finished";
#endif

    QByteArray data;

    if (auto err = mReply->error(); err != QNetworkReply::NoError)
        qWarning() << "error:" << err << mReply->url();
    else
        data = mReply->readAll();

    mProgress.first = mProgress.second;
    emit progressChanged();

    mReply->deleteLater();
    mReply = nullptr;

    return data;
}
