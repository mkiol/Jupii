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

#include "downloader.h"

RadionetApi::RadionetApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent)
    : QObject{parent}, nam{std::move(nam)} {}

QJsonDocument RadionetApi::parseJsonData(const QByteArray &data) {
    QJsonParseError err{};

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "error parsing json:" << err.errorString();
    }

    return json;
}

std::optional<std::pair<QUrl, QString>> RadionetApi::parseStreams(
    const QJsonArray &json) {
    std::optional<std::pair<QUrl, QString>> streamUrl;

    for (int i = 0; i < json.size(); ++i) {
        auto es = json.at(i).toObject();
        if (es.value(QLatin1String{"status"}).toString() ==
            QStringLiteral("VALID")) {
            auto url = QUrl{es.value(QLatin1String{"url"}).toString()};

            if (!url.isValid()) continue;

            streamUrl.emplace(
                std::move(url),
                es.value(QLatin1String{"contentFormat"}).toString());

            // preferring mp3 format
            if (streamUrl->second == QStringLiteral("MP3")) break;
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

std::optional<RadionetApi::Station> RadionetApi::parsePlayable(
    const QJsonObject &json) {
    if (json.value(QLatin1String{"type"}).toString() !=
        QStringLiteral("STATION"))
        return std::nullopt;
    if (!json.value(QLatin1String{"hasValidStreams"}).toBool())
        return std::nullopt;

    Station station;

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

std::vector<RadionetApi::Station> RadionetApi::search(
    const QString &query) const {
    std::vector<Station> items;

    auto data = Downloader{nam}.downloadData(makeSearchUrl(query));
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto json = parseJsonData(data.bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
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

std::vector<RadionetApi::Station> RadionetApi::local() {
    std::vector<Station> items;

    auto data = Downloader{nam}.downloadData(makeLocalUrl(maxLocal, 0));
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto json = parseJsonData(data.bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
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

QUrl RadionetApi::makeSearchUrl(const QString &phrase) {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("query"), phrase);
    qurl.addQueryItem(QStringLiteral("count"), QString::number(maxSearch));
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
