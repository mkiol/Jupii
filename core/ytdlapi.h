/* Copyright (C) 2020-2021 Michal Kosciesza <michal@mkiol.net>
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
#include <QByteArray>
#include <QNetworkAccessManager>
#include <memory>

class YtdlApi: public QObject
{
    Q_OBJECT
public:
    struct Track {
        QString title;
        QUrl webUrl;
        QUrl streamUrl;
    };

    explicit YtdlApi(std::shared_ptr<QNetworkAccessManager> nam = {},
                       QObject* parent = nullptr);
    [[nodiscard]] bool ok() const;
    Track track(const QUrl &url);
    bool init();

private:
    static const int httpTimeout = 10000;
    static const int timeout = 100000;
    static bool enabled;
    static QString binPath;
    std::shared_ptr<QNetworkAccessManager> nam;

    bool downloadBin();
    bool checkBin(const QString& binPath);
    void update();
    QString defaultBinPath() const;
};

#endif // YOUTUBEDL_H
