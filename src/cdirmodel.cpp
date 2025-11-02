/* Copyright (C) 2020-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "cdirmodel.h"

#include <QDebug>

#include "contentdirectory.h"
#include "contentserver.h"
#include "libupnpp/control/cdircontent.hxx"
#include "services.h"
#include "settings.h"
#include "utils.h"

const QHash<QString, CDirModel::Types> CDirModel::containerClasses{
    {"object.container.album.musicAlbum", CDirModel::MusicAlbumType},
    {"object.container.person.musicArtist", CDirModel::ArtistType},
    {"object.container.album.photoAlbum", CDirModel::PhotoAlbumType}};

CDirModel::CDirModel(QObject *parent)
    : SelectableItemModel(new CDirItem, parent),
      m_title{Services::instance()->contentDir->getDeviceFriendlyName()},
      m_queryType{Settings::instance()->getCDirQueryType()} {
    connect(Services::instance()->contentDir.get(), &Service::initedChanged,
            this, &CDirModel::handleContentDirectoryInited,
            Qt::QueuedConnection);

    if (Services::instance()->contentDir->getInited()) updateModel();
}

void CDirModel::handleContentDirectoryInited() {
    if (Services::instance()->contentDir->getInited()) updateModel();
}

CDirModel::Types CDirModel::typeFromItemClass(const QString &upnpClass) {
    switch (ContentServer::typeFromUpnpClass(upnpClass)) {
        case ContentServer::Type::Type_Music:
            return AudioType;
        case ContentServer::Type::Type_Video:
            return VideoType;
        case ContentServer::Type::Type_Image:
            return ImageType;
        default:
            return UnknownType;
    }
}

CDirModel::Types CDirModel::typeFromContainerClass(const QString &upnpClass) {
    if (containerClasses.contains(upnpClass))
        return containerClasses.value(upnpClass);
    return DirType;
}

QList<ListItem *> CDirModel::makeItems() {
    QList<ListItem *> items;

    int maxItems = 500;

    auto cd = Services::instance()->contentDir;
    if (!cd->getInited()) {
        qWarning() << "content-directory is not inited";
        return items;
    }

    UPnPClient::UPnPDirContent content;

    if (m_id != m_pid) {
        const auto ppid = findPid(m_pid);
        // const auto ptitle = findTitle(m_pid);
        items.push_back(
            new CDirItem{m_pid, ppid, QStringLiteral(".."), BackType});
    }

    if (cd->readItems(m_id, content)) {
        auto did = cd->getDeviceId();

        QList<ListItem *> items_dir;

        for (const auto &item : content.m_containers) {
            if (items.size() > maxItems) {
                qWarning() << "max cdir items";
                break;
            }

            auto type = typeFromContainerClass(
                QString::fromStdString(item.getprop("upnp:class")));
            auto title = QString::fromStdString(item.m_title);

            if (type == MusicAlbumType || type == ArtistType) {
                auto artist = Utils::parseArtist(
                    QString::fromStdString(item.getprop("upnp:artist")));
                auto album = QString::fromStdString(item.getprop("upnp:album"));

                if (const auto &f = getFilter(); !f.isEmpty()) {
                    if (!title.contains(f, Qt::CaseInsensitive) &&
                        !artist.contains(f, Qt::CaseInsensitive) &&
                        !album.contains(f, Qt::CaseInsensitive)) {
                        // filtered out
                        continue;
                    }
                }

                items_dir.push_back(new CDirItem{
                    QString::fromStdString(item.m_id),
                    QString::fromStdString(item.m_pid), title, artist, album,
                    QUrl{},
                    QUrl{QString::fromStdString(
                        item.getprop("upnp:albumArtURI"))},
                    0,
                    Utils::parseDate(
                        QString::fromStdString(item.getprop("dc:date"))),
                    item.getDurationSeconds(), type});
            } else {
                if (const auto &f = getFilter(); !f.isEmpty()) {
                    if (!title.contains(f, Qt::CaseInsensitive)) {
                        // filtered
                        continue;
                    }
                }

                items_dir.push_back(
                    new CDirItem{QString::fromStdString(item.m_id),
                                 QString::fromStdString(item.m_pid),
                                 std::move(title), DirType});
            }
        }

        QList<ListItem *> items_file;

        for (UPnPClient::UPnPDirObject &item : content.m_items) {
            if (items.size() > maxItems) {
                qWarning() << "max cdir items";
                break;
            }

            if (item.m_resources.empty())
                continue;  // skip item without resource
            if (item.m_resources[0].m_uri.empty())
                continue;  // skip item with empty uri
            auto type = typeFromItemClass(
                QString::fromStdString(item.getprop("upnp:class")));
            if (type == UnknownType) continue;  // skip item with unknown type

            auto item_id = QString::fromStdString(item.m_id);
            auto item_pid = QString::fromStdString(item.m_pid);

            QUrl url = [&]() {
                if (!item.m_resources.empty() &&
                    !item.m_resources[0].m_uri.empty()) {
                    QUrl url{QString::fromStdString(item.m_resources[0].m_uri)};
                    if (url.host() != cd->getDeviceUrl().host()) {
                        qDebug() << "upnp url does not match device host "
                                    "address";
                        return url;
                    }
                }

                return QUrl{"jupii://upnp/" +
                            QUrl::toPercentEncoding(did.toLatin1()) + "/" +
                            QUrl::toPercentEncoding(item_id.toLatin1()) + "/" +
                            QUrl::toPercentEncoding(item_pid.toLatin1())};
            }();

            auto title = QString::fromStdString(item.m_title);
            auto artist = Utils::parseArtist(
                QString::fromStdString(item.getprop("upnp:artist")));
            auto album = QString::fromStdString(item.getprop("upnp:album"));

            if (const auto &f = getFilter(); !f.isEmpty()) {
                if (!title.contains(f, Qt::CaseInsensitive) &&
                    !artist.contains(f, Qt::CaseInsensitive) &&
                    !album.contains(f, Qt::CaseInsensitive)) {
                    continue;  // skip item not matched against filter
                }
            }

            items_file.push_back(new CDirItem{
                std::move(item_id), std::move(item_pid), std::move(title),
                std::move(artist), std::move(album), std::move(url),
                type == ImageType ? QUrl{ContentServer::minResUrl(item)}
                                  : QUrl{QString::fromStdString(
                                        item.getprop("upnp:albumArtURI"))},
                QString::fromStdString(item.getprop("upnp:originalTrackNumber"))
                    .toInt(),
                QDateTime::fromString(
                    QString::fromStdString(item.getprop("dc:date")),
                    Qt::ISODate),
                item.getDurationSeconds(), type});
        }

        // -- Sorting --
        // 0 - by title
        // 1 - by album
        // 2 - by artist
        // 3 - by track number
        // 4 - by date
        int queryType = getQueryType();
        bool contener_sort;
        auto sort_fun = [queryType, &contener_sort](ListItem *a, ListItem *b) {
            auto *aa = qobject_cast<CDirItem *>(a);
            auto *bb = qobject_cast<CDirItem *>(b);
            if (queryType == 1) {
                if (aa->album().isEmpty() && !bb->album().isEmpty())
                    return false;
                if (!aa->album().isEmpty() && bb->album().isEmpty())
                    return true;
                if (!aa->album().isEmpty() && !bb->album().isEmpty()) {
                    auto cmp =
                        aa->album().compare(bb->album(), Qt::CaseInsensitive);
                    if (cmp != 0) return cmp < 0;
                }
            } else if (queryType == 2) {
                if (aa->artist().isEmpty() && !bb->artist().isEmpty())
                    return false;
                if (!aa->artist().isEmpty() && bb->artist().isEmpty())
                    return true;
                if (!aa->artist().isEmpty() && !bb->artist().isEmpty()) {
                    auto cmp =
                        aa->artist().compare(bb->artist(), Qt::CaseInsensitive);
                    if (cmp != 0) return cmp < 0;
                }
            } else if (!contener_sort && queryType == 3) {
                if (aa->number() == 0 && bb->number() != 0) return false;
                if (aa->number() != 0 && bb->number() == 0) return true;
                if (aa->number() != 0 && bb->number() != 0) {
                    return aa->number() < bb->number();
                }
            } else if (queryType == 4) {
                if (!aa->date().isValid() && bb->date().isValid()) return false;
                if (aa->date().isValid() && !bb->date().isValid()) return true;
                if (aa->date().isValid() && bb->date().isValid()) {
                    return aa->date() > bb->date();
                }
            }
            return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
        };

        contener_sort = true;
        std::sort(items_dir.begin(), items_dir.end(), sort_fun);
        contener_sort = false;
        std::sort(items_file.begin(), items_file.end(), sort_fun);

        items << items_dir << items_file;
    }

    return items;
}

QVariantList CDirModel::selectedItems() {
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto citem = qobject_cast<CDirItem *>(item);
        if (citem->selected()) {
            QVariantMap map;
            map.insert(QStringLiteral("url"), citem->url());

            if (!Utils::isUrlUpnp(citem->url())) {
                map.insert(QStringLiteral("name"), citem->title());
                map.insert(QStringLiteral("author"), citem->artist());
                map.insert(QStringLiteral("icon"), citem->icon());
                // map.insert(QStringLiteral("album"), citem->album());
            }

            list.push_back(map);
        }
    }

    return list;
}

QString CDirModel::findTitle(const QString &id) {
    if (idToTitle.contains(id)) return idToTitle.value(id);

    return Services::instance()->contentDir->getDeviceFriendlyName();
}

QString CDirModel::findPid(const QString &id) {
    if (idToPid.contains(id)) return idToPid.value(id);

    return {QStringLiteral("0")};
}

CDirModel::Types CDirModel::findType(const QString &id) {
    if (idToType.contains(id)) return idToType.value(id);

    return DirType;
}

void CDirModel::setCurrentId(const QString &id) {
    if (m_id != id) {
        setAllSelected(false);
        setFilterNoUpdate(QString{});
        m_id = id;
        const auto item = qobject_cast<CDirItem *>(find(m_id));
        if (item) {
            m_title = item->title();
            m_pid = item->pid();
            m_type = item->type();
            idToTitle[m_id] = m_title;
            idToPid[m_id] = m_pid;
            idToType[m_id] = m_type;
        } else {
            m_title = findTitle(id);
            m_pid = findPid(id);
            m_type = findType(id);
        }
        emit currentDirChanged();
        updateModel();
    }
}

QString CDirModel::getCurrentId() const { return m_id; }

QString CDirModel::getCurrentTitle() const { return m_title; }

CDirModel::Types CDirModel::getCurrentType() const { return m_type; }

int CDirModel::getQueryType() const { return m_queryType; }

void CDirModel::setQueryType(int value) {
    if (value != m_queryType) {
        m_queryType = value;
        Settings::instance()->setCDirQueryType(m_queryType);
        emit queryTypeChanged();
        updateModel();
    }
}

void CDirModel::switchQueryType() { setQueryType((getQueryType() + 1) % 5); }

CDirItem::CDirItem(QString id, QString pid, QString title, QString artist,
                   QString album, QUrl url, QUrl icon, int number,
                   QDateTime date, int duration, CDirModel::Types type,
                   QObject *parent)
    : SelectableItem(parent),
      m_id{std::move(id)},
      m_pid{std::move(pid)},
      m_title{std::move(title)},
      m_artist{std::move(artist)},
      m_album{std::move(album)},
      m_icon{std::move(icon)},
      m_url{std::move(url)},
      m_number{number},
      m_date{std::move(date)},
      m_duration{duration},
      m_type{type} {
    m_selectable = type != CDirModel::DirType && type != CDirModel::BackType &&
                   type != CDirModel::MusicAlbumType &&
                   type != CDirModel::ArtistType;
}

CDirItem::CDirItem(QString id, QString pid, QString title,
                   CDirModel::Types type, QObject *parent)
    : SelectableItem(parent),
      m_id{std::move(id)},
      m_pid{std::move(pid)},
      m_title{std::move(title)},
      m_number{0},
      m_type{type} {
    m_selectable = false;
}

QHash<int, QByteArray> CDirItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[PidRole] = "pid";
    names[TitleRole] = "title";
    names[AlbumRole] = "album";
    names[ArtistRole] = "artist";
    names[UrlRole] = "url";
    names[NumberRole] = "number";
    names[TypeRole] = "type";
    names[DurationRole] = "duration";
    names[SelectedRole] = "selected";
    names[SelectableRole] = "selectable";
    names[IconRole] = "icon";
    names[DateRole] = "date";
    return names;
}

QVariant CDirItem::data(int role) const {
    switch (role) {
        case IdRole:
            return id();
        case PidRole:
            return pid();
        case TitleRole:
            return title();
        case AlbumRole:
            return album();
        case ArtistRole:
            return artist();
        case UrlRole:
            return url();
        case NumberRole:
            return number();
        case TypeRole:
            return type();
        case DurationRole:
            return duration();
        case SelectedRole:
            return selected();
        case SelectableRole:
            return selectable();
        case IconRole:
            return iconThumb();
        case DateRole:
            return Utils::friendlyDate(m_date);
        default:
            return {};
    }
}

QUrl CDirItem::iconThumb() const {
    auto url = ContentServer::instance()->thumbUrl(m_icon);
    if (url.isEmpty()) return m_icon;
    return url;
}
