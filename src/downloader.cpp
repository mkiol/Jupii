/* Copyright (C) 2021-2025 Michal Kosciesza <michal@mkiol.net>
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
#include <QTime>
#include <QTimer>
#include <algorithm>

#include "contentserver.h"

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
                                bool abortWhenSlowDownload) {
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

    auto startTime = QTime::currentTime();

    if (nam) {
        mReply = nam->get(request);
    } else {
        priv_nam = std::make_unique<QNetworkAccessManager>();
        mReply = priv_nam->get(request);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(startTimeout);

    auto con1 = connect(
        &timer, &QTimer::timeout, this,
        [this, &output] {
            if (mReply && output.size() == 0) {
                qWarning() << "start timeout => aborting";
                mReply->abort();
            }
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
        [this, &startTime, abortWhenSlowDownload](qint64 bytesReceived,
                                                  qint64 bytesTotal) {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;

            if (abortWhenSlowDownload) {
                if (auto sec = startTime.secsTo(QTime::currentTime());
                    sec > timeRateCheck) {
                    auto rate = static_cast<int>(bytesReceived / sec);

                    if (rate < minRate) {
                        qWarning() << "slow download => aborting:" << rate;
                        reply->abort();
                    } else if (bytesTotal > 0) {
                        auto eta = bytesTotal / rate;
                        if (eta > maxEtaSec) {
                            qWarning()
                                << "download eta too long => aborting:" << eta;
                            reply->abort();
                        }
                    }
                }
            }

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

Downloader::Data Downloader::downloadData(const QUrl &url, const Data &postData,
                                          int timeout) {
    Data result;

    if (busy()) {
        qWarning() << "downloaded is busy";
        return result;
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
        if (postData.bytes.isEmpty()) {
            mReply = nam->get(request);
        } else {
            request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                              postData.mime);
            mReply = nam->post(request, postData.bytes);
        }
    } else {
        priv_nam = std::make_unique<QNetworkAccessManager>();
        if (postData.bytes.isEmpty()) {
            mReply = priv_nam->get(request);
        } else {
            request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                              postData.mime);
            mReply = priv_nam->post(request, postData.bytes);
        }
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeout);

    auto con1 = connect(
        &timer, &QTimer::timeout, this,
        [this] {
            if (mReply) {
                qWarning() << "timeout => aborting";
                mReply->abort();
            }
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
    QString mime;

    if (auto err = mReply->error(); err != QNetworkReply::NoError) {
        qWarning() << "error:" << err << mReply->url();
    } else {
        data = mReply->readAll();
        mime = ContentServer::mimeFromReply(mReply);
    }

    mProgress.first = mProgress.second;
    emit progressChanged();

    result.mime = std::move(mime);
    result.bytes = std::move(data);
    result.finalUrl = mReply->url();

    mReply->deleteLater();
    mReply = nullptr;

    return result;
}

QHash<QUrl, Downloader::Data> Downloader::downloadData(
    const std::vector<QUrl> &urls, const QHash<QUrl, Data> &postDataMap,
    int timeout) {
    QHash<QUrl, Downloader::Data> result;

    mCancelRequested = false;

    if (urls.empty()) {
        qWarning() << "urls to download list is empty";
        return result;
    }

    auto local_nam{nam};
    if (!local_nam) {
        local_nam = std::make_shared<QNetworkAccessManager>();
    }

    auto unfinishedDownloadsCount = urls.size();
    QEventLoop loop;

    for (const auto &url : urls) {
#ifdef QT_DEBUG
        qDebug() << "download data:" << url;
#endif
        QNetworkRequest request;
        request.setUrl(url);
        setRequestProps(request);

        auto reply = [&] {
            auto postDataIt = postDataMap.find(url);
            if (postDataIt == postDataMap.end()) {  // no post data
                return local_nam->get(request);
            } else {
                request.setHeader(
                    QNetworkRequest::KnownHeaders::ContentTypeHeader,
                    postDataIt.value().mime);
                return local_nam->post(request, postDataIt.value().bytes);
            }
        }();

        reply->setProperty("url", url);

        auto timer = new QTimer{reply};
        timer->setSingleShot(true);
        timer->setInterval(timeout);

        connect(timer, &QTimer::timeout, this, [&] {
            auto *timer = qobject_cast<QTimer *>(sender());
            auto *reply = qobject_cast<QNetworkReply *>(timer->parent());
            qWarning() << "timeout => aborting:"
                       << reply->property("url").toUrl();
            reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [&] {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            auto url = reply->property("url").toUrl();
#ifdef QT_DEBUG
            qDebug() << "download finished:" << url;
#endif
            if (reply->error() == QNetworkReply::NoError) {
                result.insert(url, {ContentServer::mimeFromReply(reply),
                                    reply->readAll(), reply->url()});
            } else {
                qWarning() << "download error:" << url << reply->error();
            }

            reply->deleteLater();

            --unfinishedDownloadsCount;
            if (unfinishedDownloadsCount == 0) {
                loop.quit();
            }
        });

        timer->start();
    }

    loop.exec();

#ifdef QT_DEBUG
    qDebug() << "all downloads finished";
#endif

    return result;
}

void Downloader::cancel() {
    mCancelRequested = true;
    if (mReply) {
        qWarning() << "cancel requested";
        mReply->abort();
    }
}

void Downloader::reset() {
    mCancelRequested = false;
    mProgress = {0, 0};
    nam.reset();
}
