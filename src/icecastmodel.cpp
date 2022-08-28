/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "icecastmodel.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QTimer>

#include "downloader.h"
#include "iconprovider.h"
#include "utils.h"

const QUrl IcecastModel::m_dirUrl{QStringLiteral("http://dir.xiph.org/yp.xml")};
const QString IcecastModel::m_dirFilename{QStringLiteral("icecast.xml")};

IcecastModel::IcecastModel(QObject *parent)
    : SelectableItemModel(new IcecastItem, parent) {
    if (QFileInfo{Utils::pathToCacheFile(m_dirFilename)}.created().daysTo(
            QDateTime::currentDateTime()) > 0) {
        qDebug() << "cache expired";
        Utils::removeFromCacheDir({m_dirFilename});
    }
}

IcecastModel::~IcecastModel() { m_worker.reset(); }

bool IcecastModel::parseData() {
    QByteArray data;
    if (Utils::readFromCacheFile(m_dirFilename, data)) {
        qDebug() << "parsing icecast directory";
        QDomDocument doc{};
        if (QString error; !doc.setContent(data, false, &error)) {
            qWarning() << "parse error:" << error;
            return false;
        }

        m_entries = doc.elementsByTagName(QStringLiteral("entry"));
        return true;
    }

    qWarning() << "cannot read icecast data";
    return false;
}

void IcecastModel::downloadDir() {
    if (!Utils::cacheFileExists(m_dirFilename)) {
        setRefreshing(true);

        auto data = Downloader{}.downloadData(m_dirUrl, 30000);

        if (QThread::currentThread()->isInterruptionRequested()) return;

        if (data.bytes.isEmpty()) {
            qWarning() << "no data received";
            emit error();
            setRefreshing(false);
            return;
        }

        Utils::writeToCacheFile(m_dirFilename, data.bytes, true);
        setRefreshing(false);
    }

    if (!parseData()) {
        emit error();
        Utils::removeFromCacheDir({m_dirFilename});
    }
}

void IcecastModel::refresh() {
    Utils::removeFromCacheDir({m_dirFilename});
    m_entries = {};
    updateModel();
}

void IcecastModel::setRefreshing(bool value) {
    if (value != m_refreshing) {
        m_refreshing = value;
        emit refreshingChanged();
    }
}

QVariantList IcecastModel::selectedItems() {
    QVariantList list;

    list.reserve(selectedCount());

    std::for_each(m_list.cbegin(), m_list.cend(), [&](const auto *item) {
        const auto *station = qobject_cast<const IcecastItem *>(item);
        if (station->selected()) {
            list.push_back(QVariantMap{
                {QStringLiteral("url"), station->url()},
                {QStringLiteral("name"), station->name()},
                {QStringLiteral("icon"),
                 IconProvider::urlToNoResId(QStringLiteral("icon-icecast"))},
                {QStringLiteral("author"), QStringLiteral("Icecast")},
                {QStringLiteral("app"), QStringLiteral("icecast")}});
        }
    });

    return list;
}

QList<ListItem *> IcecastModel::makeItems() {
    if (m_entries.isEmpty()) downloadDir();

    QList<ListItem *> items;

    const auto &filter = getFilter();

    int l = filter.isEmpty() ? 1000 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull()) {
            auto name = entry.elementsByTagName(QStringLiteral("server_name"))
                            .at(0)
                            .toElement()
                            .text()
                            .trimmed();
            auto genre = entry.elementsByTagName(QStringLiteral("genre"))
                             .at(0)
                             .toElement()
                             .text();
            auto url = entry.elementsByTagName(QStringLiteral("listen_url"))
                           .at(0)
                           .toElement()
                           .text();

            if (!url.isEmpty() && !name.isEmpty()) {
                if (name.contains(filter, Qt::CaseInsensitive) ||
                    genre.contains(filter, Qt::CaseInsensitive) ||
                    QUrl{url}.path().contains(filter, Qt::CaseInsensitive)) {
                    auto mime =
                        entry.elementsByTagName(QStringLiteral("server_type"))
                            .at(0)
                            .toElement()
                            .text();
                    auto bitrate =
                        entry.elementsByTagName(QStringLiteral("bitrate"))
                            .at(0)
                            .toElement()
                            .text()
                            .toInt();
                    QString type =
                        mime.contains(QStringLiteral("audio/mpeg"),
                                      Qt::CaseInsensitive)
                            ? QStringLiteral("MP3")
                        : mime.contains(QStringLiteral("audio/aac"),
                                        Qt::CaseInsensitive)
                            ? QStringLiteral("AAC")
                        : mime.contains(QStringLiteral("audio/aacp"),
                                        Qt::CaseInsensitive)
                            ? QStringLiteral("AAC+")
                        : mime.contains(QStringLiteral("application/ogg"),
                                        Qt::CaseInsensitive)
                            ? QStringLiteral("OGG")
                        : mime.contains(QStringLiteral("video/webm"),
                                        Qt::CaseInsensitive)
                            ? QStringLiteral("WEBM")
                        : mime.contains(QStringLiteral("video/nsv"),
                                        Qt::CaseInsensitive)
                            ? QStringLiteral("NSV")
                            : QString{};

                    auto desc =
                        (genre.isEmpty()
                             ? (type.isEmpty() ? QString{}
                                               : type + QStringLiteral(" "))
                             : genre + " · " +
                                   (type.isEmpty()
                                        ? QString{}
                                        : type + QStringLiteral(" "))) +
                        (bitrate > 0
                             ? QString::number(bitrate) + QStringLiteral("kbps")
                             : QString{});

                    items << new IcecastItem{/*id=*/url, name, desc,
                                             /*url=*/QUrl{url},
                                             ContentServer::typeFromMime(mime)};
                }
            }
        }

        if (items.length() > 999) break;
    }

    // Sorting
    std::sort(items.begin(), items.end(),
              [](const ListItem *a, const ListItem *b) {
                  return qobject_cast<const IcecastItem *>(a)->name().compare(
                             qobject_cast<const IcecastItem *>(b)->name(),
                             Qt::CaseInsensitive) < 0;
              });

    return items;
}

IcecastItem::IcecastItem(const QString &id, const QString &name,
                         const QString &description, const QUrl &url,
                         ContentServer::Type type, QObject *parent)
    : SelectableItem(parent),
      m_id{id},
      m_name{name},
      m_description{description},
      m_url{url},
      m_type{type} {}

QHash<int, QByteArray> IcecastItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[DescriptionRole] = "description";
    names[UrlRole] = "url";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    return names;
}

QVariant IcecastItem::data(int role) const {
    switch (role) {
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
