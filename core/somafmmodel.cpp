/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QList>

#ifdef SAILFISH
#include <sailfishapp.h>
#endif

#include "somafmmodel.h"

SomafmModel::SomafmModel(QObject *parent) :
    SelectableItemModel(new SomafmItem, parent)
{
    QString jfile;

#ifdef SAILFISH
    m_dir = QDir(SailfishApp::pathTo("somafm").toLocalFile());
    jfile = SailfishApp::pathTo("somafm/somafm.json").toLocalFile();
#endif

    QFile f(jfile);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        qWarning() << "File" << jfile << "cannot be opened";
        return;
    }

    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isEmpty() || !doc.isObject()) {
        qWarning() << "Cannot parse json file" << jfile;
        return;
    }

    auto obj = doc.object();
    if (!obj.contains("channels") || !obj["channels"].isArray()) {
        qWarning() << "No channels";
        return;
    }

    m_channels = obj["channels"].toArray();
}

QVariantList SomafmModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto channel = dynamic_cast<SomafmItem*>(item);
        if (channel->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(channel->url()));
            map.insert("name", QVariant(channel->name()));
            map.insert("icon", QVariant(channel->icon()));
            map.insert("author", QVariant("SomaFM"));
            list << map;
        }
    }

    return list;
}

QList<ListItem*> SomafmModel::makeItems()
{
    QList<ListItem*> items;

    for (const auto c : m_channels) {
        if (c.isObject()) {
            auto obj = c.toObject();
            QString title = obj["title"].toString();
            auto filter = getFilter();
            if (filter.isEmpty() || title.contains(filter, Qt::CaseInsensitive)) {
                auto icon = QUrl::fromLocalFile(m_dir.filePath(obj["icon"].toString()));
                items << new SomafmItem(
                                obj["id"].toString(),
                                title,
                                obj["description"].toString(),
                                QUrl(obj["url"].toString()),
#ifdef SAILFISH
                                icon
#else
                                QIcon(icon.toLocalFile())
#endif
                            );
            }
        }
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<SomafmItem*>(a);
        auto bb = dynamic_cast<SomafmItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

SomafmItem::SomafmItem(const QString &id,
                   const QString &name,
                   const QString &description,
                   const QUrl &url,
#ifdef SAILFISH
                   const QUrl &icon,
#else
                   const QIcon &icon,
#endif
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_description(description),
    m_url(url),
    m_icon(icon)
{
}

QHash<int, QByteArray> SomafmItem::roleNames() const
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

QVariant SomafmItem::data(int role) const
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
