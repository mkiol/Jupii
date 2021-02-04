/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>

#include "settings.h"
#include "cdirmodel.h"
#include "contentdirectory.h"
#include "services.h"
#include "utils.h"
#include "contentserver.h"

#include "libupnpp/control/cdircontent.hxx"

const QHash<QString,CDirModel::Types> CDirModel::containerClasses {
    {"object.container.album.musicAlbum", CDirModel::MusicAlbumType},
    {"object.container.person.musicArtist", CDirModel::ArtistType},
    {"object.container.album.photoAlbum", CDirModel::PhotoAlbumType}
};

CDirModel::CDirModel(QObject *parent) :
    SelectableItemModel(new CDirItem, parent),
    m_id("0"), m_pid("0"), m_type(CDirModel::DirType)
{
    auto s = Settings::instance();
    m_queryType = s->getCDirQueryType();

    auto cd = Services::instance()->contentDir;
    m_title = cd->getDeviceFriendlyName();
    connect(cd.get(), &Service::initedChanged, this,
            &CDirModel::handleContentDirectoryInited, Qt::QueuedConnection);
    if (cd->getInited())
        updateModel();
}

void CDirModel::handleContentDirectoryInited()
{
    if (Services::instance()->contentDir->getInited())
        updateModel();
}

CDirModel::Types CDirModel::typeFromItemClass(const QString &upnpClass)
{
    switch (ContentServer::typeFromUpnpClass(upnpClass)) {
    case ContentServer::TypeMusic:
        return AudioType;
    case ContentServer::TypeVideo:
        return VideoType;
    case ContentServer::TypeImage:
        return ImageType;
    default:
        return UnknownType;
    }
}

CDirModel::Types CDirModel::typeFromContainerClass(const QString &upnpClass)
{
    if (containerClasses.contains(upnpClass))
        return containerClasses.value(upnpClass);
    return DirType;
}

QList<ListItem*> CDirModel::makeItems()
{
    QList<ListItem*> items;

    const int maxItems = 500;

    auto cd = Services::instance()->contentDir;
    if (!cd->getInited()) {
        qWarning() << "ContentDirectory is not inited";
        return items;
    }

    UPnPClient::UPnPDirContent content;

    if (m_id != m_pid) {
        auto ppid = findPid(m_pid);
        //auto ptitle = findTitle(m_pid);
        items << new CDirItem(m_pid, ppid, "..", BackType);
    }

    if (cd->readItems(m_id, content)) {
        auto did = cd->getDeviceId();
        QList<ListItem*> items_dir;
        for (const auto &item : content.m_containers) {
            if (items.size() > maxItems) {
                qWarning() << "Max cdir items";
                break;
            }

            auto type = typeFromContainerClass(QString::fromStdString(item.getprop("upnp:class")));
            auto title = QString::fromStdString(item.m_title);

            if (type == MusicAlbumType || type == ArtistType) {
                auto artist = Utils::parseArtist(QString::fromStdString(item.getprop("upnp:artist")));
                auto album = QString::fromStdString(item.getprop("upnp:album"));

                auto f = getFilter();
                if (!f.isEmpty()) {
                    if (!title.contains(f, Qt::CaseInsensitive) &&
                        !artist.contains(f, Qt::CaseInsensitive) &&
                        !album.contains(f, Qt::CaseInsensitive)) {
                        // filtered
                        continue;
                    }
                }

                items_dir << new CDirItem(
                                 QString::fromStdString(item.m_id),
                                 QString::fromStdString(item.m_pid),
                                 title,
                                 artist,
                                 album,
                                 QUrl(),
                                 QUrl(QString::fromStdString(item.getprop("upnp:albumArtURI"))),
                                 0,
                                 Utils::parseDate(QString::fromStdString(item.getprop("dc:date"))),
                                 type);
            } else {
                auto f = getFilter();
                if (!f.isEmpty()) {
                    if (!title.contains(f, Qt::CaseInsensitive)) {
                        // filtered
                        continue;
                    }
                }

                items_dir << new CDirItem(
                                 QString::fromStdString(item.m_id),
                                 QString::fromStdString(item.m_pid),
                                 title,
                                 DirType);
            }
        }

        QList<ListItem*> items_file;
        for (UPnPClient::UPnPDirObject &item : content.m_items) {
            if (items.size() > maxItems) {
                qWarning() << "Max cdir items";
                break;
            }

            if (item.m_resources.empty())
                continue; // skip item without resource
            if (item.m_resources[0].m_uri.empty())
                continue; // skip item with empty uri
            auto type = typeFromItemClass(QString::fromStdString(item.getprop("upnp:class")));
            if (type == UnknownType)
                continue; // skip item with unknown type

            auto item_id = QString::fromStdString(item.m_id);
            QUrl url = QUrl("jupii://upnp/" +
                       QUrl::toPercentEncoding(did.toLatin1()) + "/" +
                       QUrl::toPercentEncoding(item_id.toLatin1()));
            auto title = QString::fromStdString(item.m_title);
            auto artist = Utils::parseArtist(QString::fromStdString(item.getprop("upnp:artist")));
            auto album = QString::fromStdString(item.getprop("upnp:album"));

            auto f = getFilter();
            if (!f.isEmpty()) {
                if (!title.contains(f, Qt::CaseInsensitive) &&
                    !artist.contains(f, Qt::CaseInsensitive) &&
                    !album.contains(f, Qt::CaseInsensitive)) {
                    continue; // skip item not matched against filter
                }
            }

            items_file << new CDirItem(
                             item_id,
                             QString::fromStdString(item.m_pid),
                             title,
                             artist,
                             album,
                             url,
                             type == ImageType ? QUrl(ContentServer::minResUrl(item)) : QUrl(),
                             QString::fromStdString(item.getprop("upnp:originalTrackNumber")).toInt(),
                             QDateTime::fromString(QString::fromStdString(item.getprop("dc:date")), Qt::ISODate),
                             type);
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
            auto aa = dynamic_cast<CDirItem*>(a);
            auto bb = dynamic_cast<CDirItem*>(b);
            if (queryType == 1) {
                if (aa->album().isEmpty() && !bb->album().isEmpty())
                    return false;
                if (!aa->album().isEmpty() && bb->album().isEmpty())
                    return true;
                if (!aa->album().isEmpty() && !bb->album().isEmpty()) {
                    auto cmp = aa->album().compare(bb->album(), Qt::CaseInsensitive);
                    if (cmp != 0)
                        return  cmp < 0;
                }
            } else if (queryType == 2) {
                if (aa->artist().isEmpty() && !bb->artist().isEmpty())
                    return false;
                if (!aa->artist().isEmpty() && bb->artist().isEmpty())
                    return true;
                if (!aa->artist().isEmpty() && !bb->artist().isEmpty()) {
                    auto cmp = aa->artist().compare(bb->artist(), Qt::CaseInsensitive);
                    if (cmp != 0)
                        return  cmp < 0;
                }
            } else if (!contener_sort && queryType == 3) {
                if (aa->number() == 0 && bb->number() != 0)
                    return false;
                if (aa->number() != 0 && bb->number() == 0)
                    return true;
                if (aa->number() != 0 && bb->number() != 0) {
                    return aa->number() < bb->number();
                }
            } else if (queryType == 4) {
                if (!aa->date().isValid() && bb->date().isValid())
                    return false;
                if (aa->date().isValid() && !bb->date().isValid())
                    return true;
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

QVariantList CDirModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto citem = dynamic_cast<CDirItem*>(item);
        if (citem->selected()) {
            QVariantMap map;
            //map.insert("name", QVariant(citem->title()));
            //map.insert("author", QVariant(citem->artist()));
            map.insert("url", QVariant(citem->url()));
            list << map;
        }
    }

    return list;
}

QString CDirModel::findTitle(const QString &id)
{
    return idToTitle.contains(id) ?
                idToTitle.value(id) :
                Services::instance()->contentDir->getDeviceFriendlyName();
}

QString CDirModel::findPid(const QString &id)
{
    return idToPid.contains(id) ? idToPid.value(id) : "0";
}

CDirModel::Types CDirModel::findType(const QString &id)
{
    return idToType.contains(id) ? idToType.value(id) : DirType;
}

void CDirModel::setCurrentId(const QString &id)
{
    if (m_id != id) {
        setAllSelected(false);
        setFilterNoUpdate("");
        m_id = id;
        auto item = dynamic_cast<CDirItem*>(find(m_id));
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

QString CDirModel::getCurrentId()
{
    return m_id;
}

QString CDirModel::getCurrentTitle()
{
    return m_title;
}

CDirModel::Types CDirModel::getCurrentType()
{
    return m_type;
}

int CDirModel::getQueryType()
{
    return m_queryType;
}

void CDirModel::setQueryType(int value)
{
    if (value != m_queryType) {
        m_queryType = value;
        auto s = Settings::instance();
        s->setCDirQueryType(m_queryType);
        emit queryTypeChanged();
        updateModel();
    }
}

void CDirModel::switchQueryType()
{
    setQueryType((getQueryType() + 1) % 5);
}

CDirItem::CDirItem(const QString &id,
                   const QString &pid,
                   const QString &title,
                   const QString &artist,
                   const QString &album,
                   const QUrl &url,
                   const QUrl &icon,
                   int number,
                   QDateTime date,
                   CDirModel::Types type,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_pid(pid),
    m_title(title),
    m_artist(artist),
    m_album(album),
    m_icon(icon),
    m_url(url),
    m_number(number),
    m_date(date),
    m_type(type)
{
    m_selectable =
            type != CDirModel::DirType &&
            type != CDirModel::BackType &&
            type != CDirModel::MusicAlbumType &&
            type != CDirModel::ArtistType;
}

CDirItem::CDirItem(const QString &id,
                   const QString &pid,
                   const QString &title,
                   CDirModel::Types type,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_pid(pid),
    m_title(title),
    m_number(0),
    m_type(type)
{
    m_selectable = false;
}

QHash<int, QByteArray> CDirItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[PidRole] = "pid";
    names[TitleRole] = "title";
    names[AlbumRole] = "album";
    names[ArtistRole] = "artist";
    names[UrlRole] = "url";
    names[NumberRole] = "number";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    names[SelectableRole] = "selectable";
    names[IconRole] = "icon";
    names[DateRole] = "date";
    return names;
}

QVariant CDirItem::data(int role) const
{
    switch(role) {
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
    case SelectedRole:
        return selected();
    case SelectableRole:
        return selectable();
    case IconRole:
        return icon();
    case DateRole:
        return Utils::friendlyDate(m_date);
    default:
        return QVariant();
    }
}
