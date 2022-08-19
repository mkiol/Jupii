/* Copyright (C) 2020-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "tuneinmodel.h"

#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QList>
#include <QTimer>
#include <QUrlQuery>

#include "downloader.h"
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
    QUrl url{"http://opml.radiotime.com/Search.ashx"};
    url.setQuery(qurl);
    return url;
}

QDomNodeList TuneinModel::parseData(const QByteArray &data)
{
    QDomNodeList entries;

    QDomDocument doc;
    if (QString error; !doc.setContent(data, false, &error)) {
        qWarning() << "Cannot parse XML data:" << error;
    } else {
        entries = doc.elementsByTagName("body").item(0).toElement().elementsByTagName("outline");
    }

    return entries;
}

QVariantList TuneinModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto station = qobject_cast<TuneinItem*>(item);
        if (station->selected()) {
            list << QVariantMap{
                {"url", {station->url()}},
                {"name", {station->name()}},
                {"icon", {station->icon()}},
                {"author", {"TuneIn"}},
                {"app", {"tunein"}}};
        }
    }

    return list;
}

QList<ListItem*> TuneinModel::makeItems()
{
    QList<ListItem*> items;

    const auto& filter = getFilter();

    if (filter.isEmpty()) {
        return items;
    }

    auto data = Downloader{}.downloadData(makeSearchUrl(filter));
    if (data.bytes.isEmpty()) {
        qWarning() << "No data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return items;

    auto entries = parseData(data.bytes);
    if (entries.isEmpty())
        return items;

    for (int i = 0; i < entries.size(); ++i) {
        if (auto ol = entries.at(i).toElement(); ol.attribute("item") == "station") {
            const auto id = ol.attribute("preset_id");
            const auto url = QUrl{ol.attribute("URL")};
            const auto name = ol.attribute("text");

            if (id.isEmpty() || url.isEmpty() || name.isEmpty()) {
                continue;
            }

            auto icon = QUrl{ol.attribute("image")};
            if (icon.isEmpty())
                icon = IconProvider::urlToNoResId("icon-tunein"); // default icon

            items << new TuneinItem{
                           id,
                           name,
                           ol.attribute("subtext"),
                           url,
                           icon};
        }
    }

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
        return {};
    }
}
