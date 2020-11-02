/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QList>
#include <QTimer>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <memory>

#include "tuneinmodel.h"
#include "utils.h"
#include "iconprovider.h"

TuneinModel::TuneinModel(QObject *parent) :
    SelectableItemModel(new TuneinItem, parent)
{
}

QUrl TuneinModel::makeSearchUrl(const QString &phrase)
{
    QUrlQuery qurl;
    qurl.addQueryItem("query", phrase);
    qurl.addQueryItem("filter", "s:mp3");
    QUrl url("http://opml.radiotime.com/Search.ashx");
    url.setQuery(qurl);
    return url;
}

std::unique_ptr<QDomDocument> TuneinModel::parseXmlData(const QByteArray &data)
{
    auto doc = std::make_unique<QDomDocument>();
    QString error;
    if (!doc->setContent(data, false, &error)) {
        qWarning() << "Cannot parse XML data:" << error;
        doc.reset();
    }

    return doc;
}

QByteArray TuneinModel::downloadData(const QUrl &url)
{
    QByteArray data;

    QNetworkRequest request;
    request.setUrl(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkAccessManager nam;
    auto reply = nam.get(request);
    QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, this, [&loop, reply, &data] {
        auto err = reply->error();
        if (err == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            qWarning() << "Error:" << err;
            reply->deleteLater();
            loop.exit(1);
        }
        reply->deleteLater();
        loop.quit();
    });

    if (loop.exec() == 1) {
        qWarning() << "Cannot download data";
        emit error();
    }

    return data;
}

QVariantList TuneinModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto station = dynamic_cast<TuneinItem*>(item);
        if (station->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(station->url()));
            map.insert("name", QVariant(station->name()));
            map.insert("icon", QVariant(station->icon()));
            map.insert("author", QVariant("TuneIn"));
            map.insert("app", QVariant("tunein"));
            list << map;
        }
    }

    return list;
}

QList<ListItem*> TuneinModel::makeItems()
{
    QList<ListItem*> items;

    auto filter = getFilter();

    if (filter.isEmpty()) {
        return items;
    }

    auto data = downloadData(makeSearchUrl(filter));

    if (data.isEmpty()) {
        qWarning() << "No data received";
        return items;
    }

    auto doc = parseXmlData(data);

    if (!doc) {
        return items;
    }

    auto bs = doc->elementsByTagName("body");
    if (bs.isEmpty()) {
        qWarning() << "No body element";
        return items;
    }

    auto ols = bs.at(0).toElement().elementsByTagName("outline");

    for (int i = 0; i < ols.size(); ++i) {
        auto ol = ols.at(i).toElement();
        if (ol.attribute("item") == "station") {
            auto id = ol.attribute("preset_id");
            auto url = QUrl(ol.attribute("URL"));
            auto name = ol.attribute("text");

            if (id.isEmpty() || url.isEmpty() || name.isEmpty()) {
                continue;
            }

            auto icon = QUrl(ol.attribute("image"));
            if (icon.isEmpty())
                icon = IconProvider::urlToNoResId("icon-tunein"); // default icon

            items << new TuneinItem(
                           id,
                           name,
                           ol.attribute("subtext"),
                           url,
                           icon
                       );
        }
    }

    // Sorting
    /*std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<TuneinItem*>(a);
        auto bb = dynamic_cast<TuneinItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });*/

    return items;
}

TuneinItem::TuneinItem(const QString &id,
                   const QString &name,
                   const QString &description,
                   const QUrl &url,
                   const QUrl &icon,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_description(description),
    m_url(url),
    m_icon(icon)
{
}

QHash<int, QByteArray> TuneinItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[DescriptionRole] = "description";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[SelectedRole] = "selected";
    return names;
}

QVariant TuneinItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case DescriptionRole:
        return description();
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
