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
#include <utility>
#include <vector>

class RadionetApi : public QObject {
    Q_OBJECT
   public:
    enum class Format { Unknown, Mp3, Aac, Hls };
    struct Item {
        QString id;
        QString name;
        QString country;
        QString city;
        Format format;
        std::optional<QUrl> imageUrl;
        QUrl streamUrl;
    };

    RadionetApi(std::shared_ptr<QNetworkAccessManager> nam = {},
          QObject *parent = nullptr);
    std::vector<Item> search(const QString &query) const;
    std::vector<Item> local();
    bool makeMoreLocalItems();
    static inline bool canMakeMoreLocalItems() {
        return m_localItemsOffset < m_localItemsMaxCount;
    }

   signals:
    void progressChanged(int n, int total);

   private:
    static const int m_count;
    static int m_localItemsMaxCount;
    static int m_localItemsOffset;
    static std::vector<Item> m_localItems;

    std::shared_ptr<QNetworkAccessManager> m_nam;

    static QUrl makeSearchUrl(const QString &phrase);
    static QUrl makeLocalUrl(int count, int offset);
    static QJsonDocument parseJsonData(const QByteArray &data);
    static std::optional<std::pair<QUrl, Format>> parseStreams(
        const QJsonArray &json);
    static std::optional<Item> parsePlayable(const QJsonObject &json);
    static std::optional<QUrl> parseLogo(const QJsonObject &json);
};

#endif  // RADIONETAPI_H
