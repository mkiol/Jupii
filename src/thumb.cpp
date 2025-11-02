/* Copyright (C) 2022-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "thumb.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QTimer>
#include <memory>

#include "contentserver.h"
#include "exif.h"
#include "utils.h"

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

Thumb::ImageOrientation Thumb::imageOrientation(const QByteArray &data) {
    easyexif::EXIFInfo exif;
    if (exif.parseFrom(
            reinterpret_cast<const unsigned char *>(data.constData()),
            data.size())) {
        return ImageOrientation::R0;
    }

    switch (exif.Orientation) {
        case 3:
            return ImageOrientation::R180;
        case 6:
            return ImageOrientation::R90;
        case 8:
            return ImageOrientation::R270;
    }

    return ImageOrientation::R0;
}

bool Thumb::convert(const QByteArray &format, QByteArray &data,
                    ImageOrientation orientation) {
    QPixmap pixmap;
    if (!pixmap.loadFromData(data, format)) {
        qWarning() << "cannot load thumb";
        return false;
    }

    if (orientation == ImageOrientation::Unknown) {
        orientation = imageOrientation(data);
    }

    if (!convertPixmap(pixmap, orientation)) return true;

    data.clear();

    QBuffer b{&data};
    b.open(QIODevice::WriteOnly);
    if (!pixmap.save(&b, format)) {
        qWarning() << "cannot save thumb";
        return false;
    }

    return true;
}

bool Thumb::convertToJpeg(const QByteArray &format, QByteArray &data) {
    QPixmap p;
    if (!p.loadFromData(data, format)) {
        qWarning() << "cannot load format";
        return false;
    }

    data.clear();

    QBuffer b{&data};
    b.open(QIODevice::WriteOnly);

    if (!p.save(&b, "jpeg")) {
        qWarning() << "cannot save format";
        return false;
    }

    return true;
}

bool Thumb::convertPixmap(QPixmap &pixmap, ImageOrientation orientation) {
    bool modified = false;

    if (pixmap.width() != pixmap.height()) {
        if (pixmap.width() > pixmap.height()) {
            pixmap = pixmap.copy((pixmap.width() - pixmap.height()) / 2, 0,
                                 pixmap.height(), pixmap.height());
        } else {
            pixmap = pixmap.copy(0, (pixmap.height() - pixmap.width()) / 2,
                                 pixmap.width(), pixmap.width());
        }
        modified = true;
    }

    if (pixmap.width() > maxPixelSize) {
        pixmap = pixmap.scaled(maxPixelSize, maxPixelSize,
                               Qt::AspectRatioMode::IgnoreAspectRatio,
                               Qt::TransformationMode::SmoothTransformation);
        modified = true;
    }

    if (orientation != ImageOrientation::Unknown &&
        orientation != ImageOrientation::R0) {
        if (orientation == ImageOrientation::R90)
            pixmap = pixmap.transformed(QMatrix{}.rotate(90.0));
        else if (orientation == ImageOrientation::R180)
            pixmap = pixmap.transformed(QMatrix{}.rotate(180.0));
        else if (orientation == ImageOrientation::R270)
            pixmap = pixmap.transformed(QMatrix{}.rotate(270.0));
        modified = true;
    }

    return modified;
}

std::optional<QString> Thumb::path(const QUrl &url) {
    if (url.isEmpty()) return std::nullopt;

    auto pathForExt = [url = url.toString()](const QString &ext) {
        auto p = ContentServer::albumArtCachePath(url, ext);
        if (QFileInfo::exists(p)) return p;
        return QString{};
    };

    auto p = pathForExt(QStringLiteral("jpg"));
    if (p.isEmpty()) p = pathForExt(QStringLiteral("png"));

    if (p.isEmpty()) return std::nullopt;
    return p;
}

std::optional<QString> Thumb::download(const QUrl &url,
                                       ThumbDownloadQueue &queue) {
    if (url.isEmpty()) return std::nullopt;
    if (auto p = path(url)) return p;

    queue.download(url);

    return std::nullopt;
}

std::optional<QString> Thumb::save(QByteArray &&data, const QUrl &url,
                                   QString ext) {
    if (data.isEmpty()) {
        qWarning() << "thumb data is empty";
        return std::nullopt;
    }

    if (ext.isEmpty()) {
        qWarning() << "invalid thumb ext";
        return std::nullopt;
    }

    auto orientation = imageOrientation(data);

    if (ext != QStringLiteral("jpg") && ext != QStringLiteral("png")) {
        qDebug() << "converting thumb format";
        if (!convertToJpeg(ext.toLatin1(), data)) return std::nullopt;
        ext = QStringLiteral("jpg");
    }

    if (!convert(ext.toLatin1(), data, orientation)) return std::nullopt;

    return Utils::writeToCacheFile(
        ContentServer::albumArtCacheName(url.toString(), ext), data, true);
}

ThumbDownloader::ThumbDownloader(QObject *parent) : QObject{parent} {
    connect(this, &ThumbDownloader::downloadRequested, this,
            &ThumbDownloader::download);
}

void ThumbDownloader::requestDownload(const QUrl &url) {
    emit downloadRequested(url);
}

void ThumbDownloader::download(const QUrl &url) {
    std::lock_guard guard{m_mtx};
    QNetworkRequest request;
    request.setUrl(url);
    setRequestProps(request);
    ++m_request_counter;

    auto *reply = m_nam.get(request);
    reply->setProperty("url", url);

    auto *timer = new QTimer{reply};
    timer->setSingleShot(true);
    timer->setInterval(TIMEOUT);

    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this,
            &ThumbDownloader::handleDownloadFinished);

    timer->start();
}

void ThumbDownloader::handleDownloadFinished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    auto url = reply->property("url").toUrl();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "thumb download error:" << reply->error() << url
                   << m_request_counter;
    }

    emit downloaded(std::move(url), ContentServer::mimeFromReply(reply),
                    reply->readAll());

    reply->deleteLater();
    --m_request_counter;
}

void ThumbDownloadQueue::handleDownloaded(QUrl url, QString mime,
                                          QByteArray bytes) {
    {
        std::lock_guard guard{m_mtx};
        if (m_downloader && !m_downloader->active()) {
            quit();
        }
    }

    emit downloaded(std::move(url), std::move(mime), std::move(bytes));
}

void ThumbDownloadQueue::run() {
    {
        std::lock_guard guard{m_mtx};
        if (!m_downloader) {
            m_downloader = std::make_unique<ThumbDownloader>();
            connect(m_downloader.get(), &ThumbDownloader::downloaded, this,
                    &ThumbDownloadQueue::handleDownloaded);
            m_promise->set_value();
        }
    }
    exec();
    {
        std::lock_guard guard{m_mtx};
        m_downloader.reset();
    }
}

void ThumbDownloadQueue::download(QUrl url) {
    {
        std::unique_lock lock{m_mtx};
        if (!m_downloader) {
            std::promise<void> promise;
            auto future = promise.get_future();
            m_promise = &promise;
            start();
            lock.unlock();
            future.wait();
        }
    }

    {
        std::lock_guard guard{m_mtx};
        m_downloader->requestDownload(url);
    }
}

ThumbDownloadQueue::~ThumbDownloadQueue() {
    quit();
    wait();
}
