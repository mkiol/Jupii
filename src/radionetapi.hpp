/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef RADIONETAPI_H
#define RADIONETAPI_H

#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <memory>
#include <optional>
#include <vector>

class RadionetApi : public QObject {
    Q_OBJECT
   public:
    struct Station {
        QString id;
        QString name;
        QString country;
        QString city;
        QStringList genres;
        std::optional<QUrl> imageUrl;
        QUrl streamUrl;
    };

    RadionetApi(std::shared_ptr<QNetworkAccessManager> nam = {},
          QObject *parent = nullptr);
    std::vector<Station> search(const QString &query) const;
    std::vector<Station> local();

   signals:
    void progressChanged(int n, int total);

   private:
    static const int maxSearch = 100;
    static const int maxLocal = 100;
    std::shared_ptr<QNetworkAccessManager> nam;

    static QUrl makeSearchUrl(const QString &phrase);
    static QUrl makeLocalUrl(int count, int offset);
    static QJsonDocument parseJsonData(const QByteArray &data);
    static std::optional<QUrl> parseStreams(const QJsonArray &json);
    static std::optional<Station> parsePlayable(const QJsonObject &json);
    static std::optional<QUrl> parseLogo(const QJsonObject &json);
};

#endif  // RADIONETAPI_H
