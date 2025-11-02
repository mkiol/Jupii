/* Copyright (C) 2022-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef THUMB_H
#define THUMB_H

#include <QByteArray>
#include <QImage>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QString>
#include <QThread>
#include <QUrl>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

class ThumbDownloadQueue;

class Thumb {
   public:
    static std::optional<QString> path(const QUrl &url);
    static std::optional<QString> download(const QUrl &url,
                                           ThumbDownloadQueue &queue);
    static std::optional<QString> save(QByteArray &&data, const QUrl &url,
                                       QString ext);

   private:
    enum class ImageOrientation { Unknown, R0, R90, R180, R270 };
    static const int maxPixelSize = 512;

    static ImageOrientation imageOrientation(const QByteArray &data);
    static bool convertToJpeg(const QByteArray &format, QByteArray &data);
    static bool convert(
        const QByteArray &format, QByteArray &data,
        ImageOrientation orientation = ImageOrientation::Unknown);
    static bool convertPixmap(QPixmap &pixmap, ImageOrientation orientation);
};

class ThumbDownloader : public QObject {
    Q_OBJECT
   public:
    static const int TIMEOUT = 10000;  // 10s
    ThumbDownloader(QObject *parent = nullptr);
    void requestDownload(const QUrl &url);
    bool active() const { return m_request_counter > 0; }
   signals:
    void downloaded(QUrl url, QString mime, QByteArray bytes);
    void downloadRequested(QUrl url);

   private:
    QNetworkAccessManager m_nam;
    unsigned int m_request_counter = 0;
    std::mutex m_mtx;
    void download(const QUrl &url);
    void handleDownloadFinished();
};

class ThumbDownloadQueue : public QThread {
    Q_OBJECT
   public:
    ThumbDownloadQueue(QObject *parent = nullptr) : QThread{parent} {}
    ~ThumbDownloadQueue();
    void download(QUrl url);

   signals:
    void downloaded(QUrl url, QString mime, QByteArray bytes);

   private:
    std::unique_ptr<ThumbDownloader> m_downloader;
    std::mutex m_mtx;
    std::promise<void> *m_promise;
    std::queue<QUrl> m_queue;
    void handleDownloaded(QUrl url, QString mime, QByteArray bytes);
    void run() override;
};

#endif  // THUMB_H
