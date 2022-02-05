/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "icecastmodel.h"

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QList>
#include <QTimer>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>

#include "utils.h"
#include "iconprovider.h"
#include "downloader.h"

const QUrl IcecastModel::m_dirUrl{"http://dir.xiph.org/yp.xml"};
const QString IcecastModel::m_dirFilename{"icecast.xml"};

IcecastModel::IcecastModel(QObject *parent) :
    SelectableItemModel(new IcecastItem, parent)
{
}

IcecastModel::~IcecastModel()
{
    m_worker.reset();
}

bool IcecastModel::parseData()
{
    QByteArray data;
    if (Utils::readFromCacheFile(m_dirFilename, data)) {
        qDebug() << "Parsing Icecast directory";
        QDomDocument doc; QString error;
        if (doc.setContent(data, false, &error)) {
            m_entries = doc.elementsByTagName("entry");
            return true;
        } else {
            qWarning() << "Parse error:" << error;
            return false;
        }
    }

    qWarning() << "Cannot read icecast data";
    return false;
}

void IcecastModel::downloadDir()
{
    if (!Utils::cacheFileExists(m_dirFilename)) {
        auto data = Downloader{}.downloadData(m_dirUrl, 30000);

        if (QThread::currentThread()->isInterruptionRequested()) return;

        if (data.isEmpty()) {
            qWarning() << "No data received";
            emit error();
            return;
        }

        Utils::writeToCacheFile(m_dirFilename, data, true);
    }

    if (!parseData()) {
        emit error();
        Utils::removeFromCacheDir({m_dirFilename});
    }
}

void IcecastModel::refresh()
{
    Utils::removeFromCacheDir({m_dirFilename});
    m_entries = {};
    updateModel();
}

QVariantList IcecastModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto station = qobject_cast<IcecastItem*>(item);
        if (station->selected()) {
            list << QVariantMap{
                {"url", station->url()},
                {"name", station->name()},
                {"icon", IconProvider::urlToNoResId("icon-icecast")},
                {"author", "Icecast"},
                {"app", "icecast"}};
        }
    }

    return list;
}

QList<ListItem*> IcecastModel::makeItems()
{
    if (m_entries.isEmpty())
        downloadDir();

    QList<ListItem*> items;

    const auto &filter = getFilter();

    int l = filter.isEmpty() ? 1000 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull()) {
            auto name = entry.elementsByTagName("server_name").at(0).toElement().text().trimmed();
            auto genre = entry.elementsByTagName("genre").at(0).toElement().text();
            auto url = entry.elementsByTagName("listen_url").at(0).toElement().text();

            if (!url.isEmpty() && !name.isEmpty()) {
                if (name.contains(filter, Qt::CaseInsensitive) ||
                    genre.contains(filter, Qt::CaseInsensitive) ||
                    QUrl{url}.path().contains(filter, Qt::CaseInsensitive)) {

                    auto mime = entry.elementsByTagName("server_type").at(0).toElement().text();
                    auto bitrate = entry.elementsByTagName("bitrate").at(0).toElement().text().toInt();
                    QString type = mime.contains("audio/mpeg", Qt::CaseInsensitive) ? "MP3" :
                                mime.contains("audio/aac", Qt::CaseInsensitive) ? "AAC" :
                                mime.contains("audio/aacp", Qt::CaseInsensitive) ? "AAC+" :
                                mime.contains("application/ogg", Qt::CaseInsensitive) ? "OGG" :
                                mime.contains("video/webm", Qt::CaseInsensitive) ? "WEBM" :
                                mime.contains("video/nsv", Qt::CaseInsensitive) ? "NSV" : "";

                    auto desc = (genre.isEmpty() ?
                                (type.isEmpty() ? "" : type + " ") : genre + " · " +
                                (type.isEmpty() ? "" : type + " ")) +
                                (bitrate > 0 ? QString::number(bitrate) + "kbps" : "");

                    items << new IcecastItem{
                                    url, // id
                                    name, // name
                                    desc, // desc
                                    QUrl(url), // url
                                    ContentServer::typeFromMime(mime) // type
                                };
                }
            }
        }

        if (items.length() > 999) break;
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        const auto aa = qobject_cast<IcecastItem*>(a);
        const auto bb = qobject_cast<IcecastItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

IcecastItem::IcecastItem(const QString &id,
                   const QString &name,
                   const QString &description,
                   const QUrl &url,
                   ContentServer::Type type,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_description(description),
    m_url(url),
    m_type(type)
{
}

QHash<int, QByteArray> IcecastItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[DescriptionRole] = "description";
    names[UrlRole] = "url";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    return names;
}

QVariant IcecastItem::data(int role) const
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
    case TypeRole:
        return static_cast<int>(type());
    case SelectedRole:
        return selected();
    default:
        return {};
    }
}
