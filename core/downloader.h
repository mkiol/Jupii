/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QByteArray>
#include <memory>

class Downloader : public QObject
{
     Q_OBJECT
public:
    Downloader(std::shared_ptr<QNetworkAccessManager> nam = {},
               QObject *parent = nullptr);
    QByteArray downloadData(const QUrl &url);
private:
    static const int httpTimeout = 10000;
    std::shared_ptr<QNetworkAccessManager> nam;
};

#endif // DOWNLOADER_H
