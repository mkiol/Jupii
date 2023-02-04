/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "radionetapi.hpp"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLatin1String>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <limits>

#include "downloader.h"

std::vector<RadionetApi::Item> RadionetApi::m_localItems{};
int RadionetApi::m_localItemsMaxCount = std::numeric_limits<int>::max();
int RadionetApi::m_localItemsOffset = 0;
const int RadionetApi::m_count = 50;

RadionetApi::RadionetApi(std::shared_ptr<QNetworkAccessManager> nam,
                         QObject *parent)
    : QObject{parent}, m_nam{std::move(nam)} {}

QJsonDocument RadionetApi::parseJsonData(const QByteArray &data) {
    QJsonParseError err{};

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError)
        qWarning() << "error parsing json:" << err.errorString();

    return json;
}

static auto hlsUrl(const QUrl &url) {
    return url.path().endsWith(QStringLiteral(".m3u8"));
}

static RadionetApi::Format formatFromStr(const QString &str) {
    if (str == QStringLiteral("MP3")) return RadionetApi::Format::Mp3;
    if (str == QStringLiteral("AAC")) return RadionetApi::Format::Aac;
    return RadionetApi::Format::Unknown;
}

std::optional<std::pair<QUrl, RadionetApi::Format>> RadionetApi::parseStreams(
    const QJsonArray &json) {
    decltype(parseStreams(json)) streamUrl;

    for (int i = 0; i < json.size(); ++i) {
        auto es = json.at(i).toObject();
        if (es.value(QLatin1String{"status"}).toString() ==
            QStringLiteral("VALID")) {
            auto url = QUrl{es.value(QLatin1String{"url"}).toString()};

            if (!url.isValid()) continue;

            auto format =
                hlsUrl(url)
                    ? Format::Hls
                    : formatFromStr(
                          es.value(QLatin1String{"contentFormat"}).toString());
            streamUrl.emplace(std::move(url), format);

            // preferring mp3 format
            if (streamUrl->second == Format::Mp3) break;
        }
    }

    return streamUrl;
}

std::optional<QUrl> RadionetApi::parseLogo(const QJsonObject &json) {
    auto logo630 = QUrl{json.value(QLatin1String{"logo630x630"}).toString()};
    if (logo630.isValid()) return logo630;

    auto logo300 = QUrl{json.value(QLatin1String{"logo300x300"}).toString()};
    if (logo300.isValid()) return logo300;

    return QUrl{json.value(QLatin1String{"logo100x100"}).toString()};
}

std::optional<RadionetApi::Item> RadionetApi::parsePlayable(
    const QJsonObject &json) {
    if (json.value(QLatin1String{"type"}).toString() !=
        QStringLiteral("STATION"))
        return std::nullopt;
    if (!json.value(QLatin1String{"hasValidStreams"}).toBool())
        return std::nullopt;

    Item station;

    station.id = json.value(QLatin1String{"id"}).toString();
    if (station.id.isEmpty()) return std::nullopt;

    station.name = json.value(QLatin1String{"name"}).toString();
    if (station.name.isEmpty()) return std::nullopt;

    auto streamUrl =
        parseStreams(json.value(QLatin1String{"streams"}).toArray());
    if (!streamUrl) return std::nullopt;
    station.streamUrl = std::move(streamUrl->first);
    station.format = std::move(streamUrl->second);

    station.imageUrl = parseLogo(json);

    station.city = json.value(QLatin1String{"city"}).toString();
    station.country = json.value(QLatin1String{"country"}).toString();

    return station;
}

std::vector<RadionetApi::Item> RadionetApi::search(const QString &query) const {
    std::vector<Item> items;

    auto data = Downloader{m_nam}.downloadData(makeSearchUrl(query));
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto json = parseJsonData(data.bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "failed to parse json data";
        return items;
    }

    auto res = json.object().value(QLatin1String{"playables"}).toArray();

    items.reserve(res.size());

    for (int i = 0; i < res.size(); ++i) {
        auto station = parsePlayable(res.at(i).toObject());
        if (station) items.push_back(std::move(*station));
    }

    return items;
}

bool RadionetApi::makeMoreLocalItems() {
    auto count =
        std::min<int>(m_count, m_localItemsMaxCount - m_localItemsOffset);

    if (count <= 0) return false;

    auto data = Downloader{m_nam}.downloadData(
        makeLocalUrl(count, m_localItems.size()));

    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return false;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return false;

    auto json = parseJsonData(data.bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "failed to parse json data";
        return false;
    }

    m_localItemsOffset += json.object().value(QLatin1String{"count"}).toInt();
    m_localItemsMaxCount =
        json.object().value(QLatin1String{"totalCount"}).toInt();

    auto res = json.object().value(QLatin1String{"playables"}).toArray();

    m_localItemsOffset += count;

    m_localItems.reserve(m_localItems.size() + res.size());

    for (int i = 0; i < res.size(); ++i) {
        auto station = parsePlayable(res.at(i).toObject());
        if (station) m_localItems.push_back(std::move(*station));
    }

    return true;
}

std::vector<RadionetApi::Item> RadionetApi::local() {
    if (m_localItems.empty()) makeMoreLocalItems();
    return m_localItems;
}

QUrl RadionetApi::makeSearchUrl(const QString &phrase) {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("query"), phrase);
    qurl.addQueryItem(QStringLiteral("count"), QString::number(m_count));
    QUrl url{QStringLiteral("https://prod.radio-api.net/stations/search")};
    url.setQuery(qurl);
    return url;
}

QUrl RadionetApi::makeLocalUrl(int count, int offset) {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("count"), QString::number(count));
    qurl.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    QUrl url{QStringLiteral("https://prod.radio-api.net/stations/local")};
    url.setQuery(qurl);
    return url;
}
