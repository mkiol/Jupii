/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QList>
#include <QTimer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <memory>

#include "bcmodel.h"
#include "utils.h"
#include "iconprovider.h"

BcModel::BcModel(QObject *parent) :
    SelectableItemModel(new BcItem, parent)
{
}

QVariantList BcModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto bcitem = dynamic_cast<BcItem*>(item);
        if (bcitem->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(bcitem->url()));
            map.insert("name", QVariant(bcitem->name()));
            map.insert("author", QVariant(bcitem->artist()));
            map.insert("icon", QVariant(bcitem->icon()));
            list << map;
        }
    }

    return list;
}

QUrl BcModel::makeBcUrl(const QString &phrase)
{
    QUrlQuery qurl;
    qurl.addQueryItem("q", phrase);
    QUrl url("https://bandcamp.com/api/fuzzysearch/1/mobile_autocomplete");
    url.setQuery(qurl);
    return url;
}

bool BcModel::parseData(const QByteArray &data)
{
    QJsonParseError err;
    auto json = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError) {
        auto res = json.object().value("auto").toObject().value("results");
        if (res.isArray()) {
            m_result = res.toArray();
            return true;
        } else {
            qWarning() << "No results array";
        }
    } else {
        qWarning() << "Error parsing json:" << err.errorString();
    }
    return false;
}

void BcModel::callBcApi(const QString &phrase)
{
    QNetworkRequest request;
    request.setUrl(makeBcUrl(phrase));
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    nam = std::unique_ptr<QNetworkAccessManager>(new QNetworkAccessManager());
    auto reply = nam->get(request);
    QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
    std::shared_ptr<QEventLoop> loop(new QEventLoop());
    connect(reply, &QNetworkReply::finished, [this, loop, reply]{
        if (!loop) {
            reply->deleteLater();
            qWarning() << "Loop doesn't exist";
            return;
        }

        //auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        //auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        auto err = reply->error();
        //qDebug() << "Response code:" << code << reason << err;

        if (err == QNetworkReply::NoError) {
            auto data = reply->readAll();
            if (data.isEmpty()) {
                qWarning() << "No data received";
                emit error();
            } else {
                if (!parseData(data)) {
                    emit error();
                }
            }
        } else {
            qWarning() << "Error:" << err;
            emit error();
        }

        reply->deleteLater();
        loop->quit();
    });

    loop->exec();
    loop.reset();
}

QList<ListItem*> BcModel::makeItems()
{
    QList<ListItem*> items;

    auto phrase = getFilter().simplified();

    if (!phrase.isEmpty()) {
        callBcApi(phrase);

        int count = m_result.count();
        for (int i = 0; i < count; ++i) {
            auto elm = m_result.at(i).toObject();
            if (elm.value("type").toString() == "t") { // track
                auto id = QString::number(elm.value("id").toInt());
                auto name = elm.value("name").toString();
                auto band = elm.value("band_name").toString();
                auto album = elm.value("album_name").toString();
                auto url = QUrl(elm.value("url").toString());
                auto img = QUrl(elm.value("img").toString());
                items << new BcItem(id, name, band, album, url, img);
            }
        }
    }

    return items;
}

BcItem::BcItem(const QString &id,
                   const QString &name,
                   const QString &artist,
                   const QString &album,
                   const QUrl &url,
                   const QUrl &icon,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_artist(artist),
    m_album(album),
    m_url(url),
    m_icon(icon)
{
}

QHash<int, QByteArray> BcItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[SelectedRole] = "selected";
    return names;
}

QVariant BcItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case ArtistRole:
        return artist();
    case AlbumRole:
        return album();
    case UrlRole:
        return url();
    case IconRole:
        return icon();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}
