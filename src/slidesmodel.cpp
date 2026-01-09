/* Copyright (C) 2025-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "slidesmodel.hpp"

#include <qfileinfo.h>

#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>
#include <utility>

#include "contentserver.h"
#include "playlistparser.h"
#include "settings.h"
#include "utils.h"

SlidesModel::SlidesModel(QObject *parent)
    : SelectableItemModel(new SlidesItem, parent),
      m_dir{Settings::instance()->getSlidesDir()},
      m_queryType{Settings::instance()->getRecQueryType()} {}

QVariantList SlidesModel::selectedItems() {
    QVariantList list;

    foreach (auto item, m_list) {
        auto *i = qobject_cast<SlidesItem *>(item);
        if (i->selected()) {
            list << QVariantMap{
                {QStringLiteral("url"), QVariant(i->slidesUrl())}};
        }
    }

    return list;
}

void SlidesModel::deleteSelected() {
    bool removed = false;

    foreach (auto item, m_list) {
        auto *i = qobject_cast<SlidesItem *>(item);
        if (i->selected()) {
            if (QFile::remove(i->path())) removed = true;
        }
    }

    if (removed) {
        m_items.clear();
        updateModel();
    }
}

void SlidesModel::deleteItem(const QString &id) {
    auto *i = qobject_cast<SlidesItem *>(find(id));
    if (i && QFile::remove(i->path())) {
        m_items.clear();
        updateModel();
    }
}

void SlidesModel::createItem(const QString &id, QString name,
                             const QList<QUrl> &urls) {
    if (urls.empty()) {
        qWarning() << "urls is empty";
        return;
    }

    auto it =
        id.isEmpty()
            ? m_items.cend()
            : std::find_if(m_items.cbegin(), m_items.cend(),
                           [&id](const auto &item) { return item.path == id; });

    auto path = [&] {
        if (!id.isEmpty()) {
            return it == m_items.cend() ? QString{} : it->path;
        }
        for (int i = 1; i < 1000; ++i) {
            auto path = m_dir.absoluteFilePath("slides_%1.xspf")
                            .arg(Utils::instance()->randString());
            if (!QFileInfo::exists(path)) {
                return path;
            }
        }
        return QString{};
    }();

    if (path.isEmpty()) {
        qWarning() << "failed to set playlist path";
        return;
    }

    if (name.isEmpty()) {
        for (int i = 1; i < 1000; ++i) {
            name = tr("Slideshow %1").arg(i);
            if (std::find_if(m_items.cbegin(), m_items.cend(),
                             [&name](const auto &item) {
                                 return item.title == name;
                             }) == m_items.cend()) {
                break;
            }
        }
    }

    PlaylistParser::Playlist playlist;
    playlist.title = std::move(name);
    playlist.items.reserve(urls.size());
    for (const auto &url : urls) {
        if (!url.isLocalFile()) continue;
        playlist.items.push_back(
            {url.toLocalFile(), /*length=*/0, /*title=*/""});
    }

    PlaylistParser::saveAsXspf(playlist, path);

    m_items.clear();
    updateModel();
}

Q_INVOKABLE QVariantMap SlidesModel::parseItem(const QString &id) const {
    QVariantMap map;

    auto it = std::find_if(m_items.cbegin(), m_items.cend(),
                           [&id](const auto &item) { return item.path == id; });
    if (it == m_items.cend()) {
        qWarning() << "can't find slides item:" << id;
        return map;
    }

    auto playlist = PlaylistParser::parsePlaylistFile(it->path);
    if (!playlist) {
        qWarning() << "can't parse playlist:" << it->path;
        return map;
    }
    if (!PlaylistParser::slidesPlaylist(*playlist)) {
        qWarning() << "playlist is not slides playlist:" << it->path;
        return map;
    }

    QList<QVariant> urls;
    urls.reserve(playlist->items.size());
    for (auto &item : playlist->items) {
        urls.push_back(std::move(item.url));
    }

    map.insert("path", it->path);
    map.insert("name", it->title);
    map.insert("urls", urls);

    return map;
}

QList<ListItem *> SlidesModel::makeItems() {
    if (m_items.isEmpty()) {
        auto files = m_dir.entryInfoList(
            QStringList() << "*.pls" << "*.m3u" << "*.xspf", QDir::Files);

        for (int i = 0; i < std::min(files.size(), 1000); ++i) {
            auto path = files.at(i).absoluteFilePath();
            auto playlist = PlaylistParser::parsePlaylistFile(path);
            if (!playlist) {
                continue;
            }

            if (!PlaylistParser::slidesPlaylist(playlist.value())) {
                continue;
            }

            Item item;
            item.date = QFileInfo{path}.lastModified();
            item.path = std::move(path);
            item.title = std::move(playlist->title);
            item.size = playlist->items.size();
            item.slidesUrl = Utils::slidesUrl(item.path);

            m_items.push_back(std::move(item));
        }
    }

    QList<ListItem *> items;
    const auto &filter = getFilter();

    int nbAutoItems = 0;

#ifdef USE_SFOS
    if (filter.isEmpty()) {
        // insert auto items at the top
        auto todayUrl = Utils::slidesUrl(Utils::SlidesTime::Today);
        items.push_back(new SlidesItem(
            todayUrl.toString(), todayUrl, {},
            Utils::slidesTimeName(Utils::SlidesTime::Today), {}, 0));
        auto last7Url = Utils::slidesUrl(Utils::SlidesTime::Last7Days);
        items.push_back(new SlidesItem(
            last7Url.path(), last7Url, {},
            Utils::slidesTimeName(Utils::SlidesTime::Last7Days), {}, 0));
        auto last30Url = Utils::slidesUrl(Utils::SlidesTime::Last30Days);
        items.push_back(new SlidesItem(
            last30Url.path(), last30Url, {},
            Utils::slidesTimeName(Utils::SlidesTime::Last30Days), {}, 0));
        nbAutoItems = 3;
    }
#endif

    foreach (const auto item, m_items) {
        if (item.title.contains(filter, Qt::CaseInsensitive) ||
            item.path.contains(filter, Qt::CaseInsensitive)) {
            items.push_back(new SlidesItem(item.path, item.slidesUrl, item.path,
                                           item.title, item.date, item.size));
        }
    }

    auto startIt = filter.isEmpty() ? std::next(items.begin(), nbAutoItems)
                                    : items.begin();
    if (m_queryType == 0) {  // by date
        std::sort(startIt, items.end(), [](ListItem *a, ListItem *b) {
            auto *aa = qobject_cast<SlidesItem *>(a);
            auto *bb = qobject_cast<SlidesItem *>(b);
            if (aa->date().isNull() && !bb->date().isNull()) return false;
            if (!aa->date().isNull() && bb->date().isNull()) return true;
            return aa->date() > bb->date();
        });
    } else {  // by title
        std::sort(startIt, items.end(), [](ListItem *a, ListItem *b) {
            auto *aa = qobject_cast<SlidesItem *>(a);
            auto *bb = qobject_cast<SlidesItem *>(b);
            return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
        });
    }

    return items;
}

void SlidesModel::setQueryType(int value) {
    if (value != m_queryType) {
        m_queryType = value;
        emit queryTypeChanged();
        Settings::instance()->setRecQueryType(m_queryType);
        updateModel();
    }
}

SlidesItem::SlidesItem(const QString &id, const QUrl &slidesUrl,
                       const QString &path, const QString &title,
                       const QDateTime &date, unsigned int size,
                       QObject *parent)
    : SelectableItem(parent),
      m_id{id},
      m_slidesUrl{slidesUrl},
      m_path{path},
      m_title{title},
      m_date{date},
      m_size{size} {}

QString SlidesItem::friendlyDate() const { return Utils::friendlyDate(m_date); }

QHash<int, QByteArray> SlidesItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[PathRole] = "path";
    names[TitleRole] = "title";
    names[SelectedRole] = "selected";
    names[DateRole] = "date";
    names[FriendlyDateRole] = "friendlyDate";
    names[IconRole] = "icon";
    names[SizeRole] = "size";
    return names;
}

QVariant SlidesItem::data(int role) const {
    switch (role) {
        case IdRole:
            return id();
        case PathRole:
            return path();
        case TitleRole:
            return title();
        case SelectedRole:
            return selected();
        case DateRole:
            return date();
        case FriendlyDateRole:
            return friendlyDate();
        case IconRole:
            return iconThumb();
        case SizeRole:
            return size();
        default:
            return {};
    }
}

QUrl SlidesItem::iconThumb() const {
    if (size() == 0) return {};
    return ContentServer::instance()->thumbUrl(m_slidesUrl);
}
