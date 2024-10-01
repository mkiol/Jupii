/* Copyright (C) 2021-2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>
#include <memory>
#include <utility>

class Downloader final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

   public:
    struct Data {
        QString mime;
        QByteArray bytes;
    };
    Downloader(std::shared_ptr<QNetworkAccessManager> nam = {},
               QObject *parent = nullptr);
    ~Downloader() final;
    Data downloadData(const QUrl &url, const Data &postData = {},
                      int timeout = httpTimeout);
    bool downloadToFile(const QUrl &url, const QString &outputPath, bool abortWhenSlowDownload);
    void cancel();
    void reset();
    inline auto canceled() const { return mCancelRequested; }
    inline bool busy() const { return mReply && mReply->isRunning(); }
    inline auto progress() const { return mProgress; }

   signals:
    void busyChanged();
    void progressChanged();

   private:
    static const int httpTimeout = 10000;  // 10s
    static const int startTimeout = 5000;  // 5s
    static const int timeRateCheck = 3;    // 3s
    static const int minRate = 30000;      // 30 kBps
    bool mCancelRequested = false;
    QNetworkReply *mReply = nullptr;
    std::pair<int64_t, int64_t> mProgress;
    std::shared_ptr<QNetworkAccessManager> nam;
};

#endif  // DOWNLOADER_H
