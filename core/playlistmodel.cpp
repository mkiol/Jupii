/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>
#include <QFile>
#include <QDataStream>
#include <QUrlQuery>

#include "playlistmodel.h"
#include "utils.h"
#include "filemetadata.h"
#include "settings.h"

#ifdef DESKTOP
#include <QPixmap>
#include <QImage>
#include <QPalette>
#include <QApplication>
#include "filedownloader.h"
const int icon_size = 64;
#endif

PlayListModel::PlayListModel(QObject *parent) :
    ListModel(new PlaylistItem, parent)
{
}

void PlayListModel::save()
{
    QStringList ids;

    for (auto item : m_list) {
        ids << item->id();
    }

    Settings::instance()->setLastPlaylist(ids);
}

bool PlayListModel::isBusy()
{
    return m_busy;
}

void PlayListModel::setBusy(bool busy)
{
    //qDebug() << "setBusy" << m_busy << busy;

    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool PlayListModel::saveToFile(const QString& title)
{
    if (m_list.isEmpty()) {
        qWarning() << "Current playlist is empty";
        return false;
    }

    const QString dir = Settings::instance()->getPlaylistDir();

    if (dir.isEmpty())
        return false;

    QString name = title.trimmed();
    if (name.isEmpty()) {
        qWarning() << "Name is empty, so using default name";
        name = tr("Playlist");
    }

    const QString oname = name;

    QString path;
    for (int i = 0; i < 10; ++i) {
        name = oname + (i > 0 ? " " + QString::number(i) : "");
        path = dir + "/" + name + ".pls";
        if (!QFileInfo::exists(path))
            break;
    }

    Utils::instance()->writeToFile(path, makePlsData(name));

    return true;
}

QByteArray PlayListModel::makePlsData(const QString& name)
{
    QByteArray data;
    QTextStream sdata(&data);

    sdata << "[playlist]" << endl;
    sdata << "X-GNOME-Title=" << name << endl;
    sdata << "NumberOfEntries=" << m_list.size() << endl;

    int l = m_list.size();
    for (int i = 0; i < l; ++i) {
        auto pitem = dynamic_cast<PlaylistItem*>(m_list.at(i));
        QString url = Utils::urlWithTypeFromId(pitem->id()).toString();
        sdata << "File" << i + 1 << "=" << url << endl;
    }

    return data;
}

void PlayListModel::load()
{
    setBusy(true);

    QStringList ids = Settings::instance()->getLastPlaylist();
    QList<ListItem*> items;

    for (auto &id : ids) {
        auto item = makeItem(QUrl(id));
        if (item)
            items << item;
    }

    if (!items.isEmpty()) {
        appendRows(items);
        emit itemsLoaded();

        if (Settings::instance()->getRememberPlaylist())
            save();
    }

    setBusy(false);
}

int PlayListModel::getPlayMode() const
{
    return m_playMode;
}

void PlayListModel::setPlayMode(int value)
{
    if (value != m_playMode) {
        m_playMode = value;
        emit playModeChanged();
    }
}

void PlayListModel::togglePlayMode()
{
    switch(m_playMode) {
    case PlayListModel::PM_Normal:
        setPlayMode(PlayListModel::PM_RepeatAll);
        break;
    case PlayListModel::PM_RepeatAll:
        setPlayMode(PlayListModel::PM_RepeatOne);
        break;
    case PlayListModel::PM_RepeatOne:
        setPlayMode(PlayListModel::PM_Normal);
        break;
    default:
        setPlayMode(PlayListModel::PM_Normal);
    }
}

int PlayListModel::getActiveItemIndex() const
{
    return m_activeItemIndex;
}

void PlayListModel::addItemPaths(const QStringList& paths)
{
    QList<QUrl> urls;
    for (auto& path : paths)
        urls.push_back(QUrl::fromLocalFile(path));
    addItems(urls, false);
}

void PlayListModel::addItemUrls(const QList<QUrl>& urls)
{
    addItems(urls, false);
}

void PlayListModel::addItemUrl(const QUrl& url)
{
    QList<QUrl> urls;
    urls << url;
    addItems(urls, false);
}


void PlayListModel::addItemPathsAsAudio(const QStringList& paths)
{
    QList<QUrl> urls;
    for (auto& path : paths)
        urls.push_back(QUrl::fromLocalFile(path));
    addItems(urls, true);
}

void PlayListModel::addItems(const QList<QUrl>& urls, bool asAudio)
{
    qDebug() << "addItems:" << urls;

    setBusy(true);

    QList<QUrl> n_ids;

    for (const auto& url : urls) {
        QUrl id(url);
        QUrlQuery q(url);

        if (q.hasQueryItem(Utils::cookieKey))
            q.removeQueryItem(Utils::cookieKey);
        q.addQueryItem(Utils::cookieKey, Utils::randString());

        if (asAudio) {
            if (q.hasQueryItem(Utils::typeKey))
                q.removeQueryItem(Utils::typeKey);
            q.addQueryItem(Utils::typeKey, QString::number(ContentServer::TypeMusic));
        }

        id.setQuery(q);
        n_ids << id;
    }

    if (!n_ids.isEmpty()) {
        QList<ListItem*> items;

        for (auto &id : n_ids) {
            auto item = makeItem(id);
            if (item)
                items << item;
        }

        if (!items.isEmpty()) {
            appendRows(items);
            emit itemsAdded();

            if (Settings::instance()->getRememberPlaylist())
                save();
        }
    }

    setBusy(false);
}

PlaylistItem* PlayListModel::makeItem(const QUrl &id)
{
    qDebug() << "makeItem:" << id;

    if (find(id.toString())) {
        qWarning() << "Id" << id << "already added";
        emit error(E_FileExists);
        return nullptr;
    }

    int t = 0; QString cookie;
    if (!Utils::pathTypeCookieFromId(id, nullptr, &t, &cookie) ||
            cookie.isEmpty()) {
        qWarning() << "Invalid Id";
        return nullptr;
    }

    QUrl url = Utils::urlFromId(id);
    auto cs = ContentServer::instance();
    ContentServer::ItemMeta meta;
    const auto it = cs->getMetaCacheIterator(url);
    if (it == cs->metaCacheIteratorEnd()) {
        qWarning() << "No meta item found";
        return nullptr;
    } else {
        meta = it.value();
    }

    auto forcedType = static_cast<ContentServer::Type>(t);
    auto type = forcedType == ContentServer::TypeUnknown ?
                meta.type : forcedType;

#ifdef SAILFISH
    QString icon = meta.albumArt;
#endif
    QString name;
    if (!meta.title.isEmpty())
        name = meta.title;
    else if (!meta.filename.isEmpty() && meta.filename.length() > 1)
        name = meta.filename;
    else if (!meta.url.isEmpty())
        name = meta.url.toString();
    else
        name = tr("Unknown");

    auto item = new PlaylistItem(meta.url == url ?
                                   id : Utils::swapUrlInId(meta.url, id), // id
                               name, // name
                               meta.url, // url
                               type, // type
                               /*meta.title.isEmpty() ? tr("Unknown") : meta.title, // title
                               meta.artist.isEmpty() ? tr("Unknown") : meta.artist, // artist
                               meta.album.isEmpty() ? tr("Unknown") : meta.album, // album*/
                               meta.title, // title
                               meta.artist, // artist
                               meta.album, // album
                               "", // date
                               meta.duration, // duration
                               meta.size, // size
#ifdef SAILFISH
                               icon, // icon
#else
                               meta.albumArt.isEmpty() ?
                                   QIcon() :
                                   QIcon(meta.albumArt), // icon
#endif
                               false, // active
                               false // to be active
                               );

    return item;
}

bool PlayListModel::addId(const QUrl &id)
{
    auto item = makeItem(id);
    if (item)
        appendRow(item);
    else
        return false;

    return true;
}

void PlayListModel::setActiveId(const QString &id)
{
    if (id == activeId())
        return;

    const int len = m_list.length();
    for (int i = 0; i < len; ++i) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(i));
        if (!fi) {
            qWarning() << "Dynamic cast is null";
            return;
        }
        bool new_active = fi->id() == id;

        if (fi->active() != new_active) {
            fi->setActive(new_active);
            if(new_active)
                setActiveItemIndex(i);
            emit activeItemChanged();
        }

        fi->setToBeActive(false);
    }
}

void PlayListModel::resetToBeActive()
{
    const int len = m_list.length();
    for (int i = 0; i < len; ++i) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(i));
        if (!fi) {
            qWarning() << "Dynamic cast is null";
            return;
        }
        fi->setToBeActive(false);
    }
}

void PlayListModel::setToBeActiveIndex(int index)
{
    const int len = m_list.length();
    for (int i = 0; i < len; ++i) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(i));
        if (!fi) {
            qWarning() << "Dynamic cast is null";
            return;
        }
        fi->setToBeActive(i == index);
    }
}

void PlayListModel::setToBeActiveId(const QString &id)
{
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
        if (!fi) {
            qWarning() << "Dynamic cast is null";
            return;
        }
        fi->setToBeActive(fi->id() == id);
    }
}

void PlayListModel::setActiveUrl(const QUrl &url)
{
    if (!url.isEmpty()) {
        auto cs = ContentServer::instance();
        setActiveId(cs->idFromUrl(url));
    }
}

void PlayListModel::clear()
{
    bool active_removed = false;
    if (m_activeItemIndex > -1) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(m_activeItemIndex));
        if (!fi) {
            qWarning() << "Dynamic cast is null";
            return;
        }
        if (fi->active())
            active_removed = true;
    }

    if(rowCount() > 0) {
        removeRows(0, rowCount());
    }

    setActiveItemIndex(-1);

    if (Settings::instance()->getRememberPlaylist())
        save();

    if(active_removed)
        emit activeItemChanged();
}

QString PlayListModel::activeId() const
{
    if (m_activeItemIndex > -1) {
        auto fi = m_list.at(m_activeItemIndex);
        return fi->id();
    }

    return QString();
}

QString PlayListModel::firstId() const
{
    if (m_list.length() > 0)
        return m_list.first()->id();

    return QString();
}

QString PlayListModel::secondId() const
{
    if (m_list.length() > 1)
        return m_list.at(1)->id();

    return QString();
}

void PlayListModel::setActiveItemIndex(int index)
{
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemIndexChanged();
    }
}

bool PlayListModel::removeIndex(int index)
{
    if (index < 0)
        return false;

    auto fi = dynamic_cast<PlaylistItem*>(m_list.at(index));
    if (!fi) {
        qWarning() << "Dynamic cast is null";
        return false;
    }

    bool active_removed = false;
    if (fi->active())
        active_removed = true;

    bool ok = removeRow(index);

    if (ok) {
        if (m_activeItemIndex > -1) {
            if (index == m_activeItemIndex)
                setActiveItemIndex(-1);
            else if (index < m_activeItemIndex)
                setActiveItemIndex(m_activeItemIndex - 1);
        }

        if (Settings::instance()->getRememberPlaylist())
            save();

        emit itemRemoved();

        if(active_removed)
            emit activeItemChanged();
    }

    return ok;
}

bool PlayListModel::remove(const QString &id)
{
    return removeIndex(indexFromId(id));
}

QString PlayListModel::nextActiveId() const
{
    if (m_activeItemIndex < 0)
        return QString();

    int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return m_list.at(m_activeItemIndex)->id();

    int nextIndex = m_activeItemIndex + 1;

    if (nextIndex < l) {
        return m_list.at(nextIndex)->id();
    } else if (m_playMode == PM_RepeatAll)
        return m_list.first()->id();

    return QString();
}

QString PlayListModel::prevActiveId() const
{
    if (m_activeItemIndex < 0)
        return QString();

    const int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return m_list.at(m_activeItemIndex)->id();

    int prevIndex = m_activeItemIndex - 1;

    if (prevIndex < 0) {
        if (m_playMode == PM_RepeatAll)
            prevIndex = l - 1;
        else
            return QString();
    }

    return m_list.at(prevIndex)->id();
}


QString PlayListModel::nextId(const QString &id) const
{
    const int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return id;

    bool nextFound = false;

    for (auto li : m_list) {
        if (nextFound)
            return li->id();
        if(li->id() == id)
            nextFound = true;
    }

    if (nextFound && m_playMode == PM_RepeatAll)
        return m_list.first()->id();

    return QString();
}

// -----

PlaylistItem::PlaylistItem(const QUrl &id,
                           const QString &name,
                           const QUrl &url,
                           ContentServer::Type type,
                           const QString &title,
                           const QString &artist,
                           const QString &album,
                           const QString &date,
                           const int duration,
                           const qint64 size,
#ifdef DESKTOP
                           const QIcon &icon,
#else
                           const QUrl &icon,
#endif
                           bool active,
                           bool toBeActive,
                           QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_name(name),
    m_url(url),
    m_type(type),
    m_title(title),
    m_artist(artist),
    m_album(album),
    m_date(date),
    m_duration(duration),
    m_size(size),
    m_icon(icon),
    m_active(active),
    m_tobeactive(toBeActive)
{}

QHash<int, QByteArray> PlaylistItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[UrlRole] = "url";
    names[TypeRole] = "type";
    names[TitleRole] = "title";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[DateRole] = "date";
    names[DurationRole] = "duration";
    names[SizeRole] = "size";
    names[IconRole] = "icon";
    names[ActiveRole] = "active";
    names[ToBeActiveRole] = "toBeActive";
    return names;
}

QString PlaylistItem::path() const
{
    if (m_url.isLocalFile())
        return m_url.toLocalFile();
    return QString();
}

QVariant PlaylistItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case UrlRole:
        return url();
    case TypeRole:
        return type();
    case TitleRole:
        return title();
    case ArtistRole:
        return artist();
    case AlbumRole:
        return album();
    case DateRole:
        return date();
    case DurationRole:
        return duration();
    case SizeRole:
        return size();
    case IconRole:
        return icon();
    case ActiveRole:
        return active();
    case ToBeActiveRole:
        return toBeActive();
#ifdef DESKTOP
    case ForegroundRole:
        return foreground();
#endif
    default:
        return QVariant();
    }
}

void PlaylistItem::setActive(bool value)
{
    setToBeActive(false);

    if (m_active != value) {
        m_active = value;
        emit dataChanged();
    }
}

void PlaylistItem::setToBeActive(bool value)
{
    if (m_tobeactive != value) {
        m_tobeactive = value;
        emit dataChanged();
    }
}

#ifdef DESKTOP
QBrush PlaylistItem::foreground() const
{
    auto p = QApplication::palette();
    return m_tobeactive ? p.brush(QPalette::Inactive, QPalette::Highlight) :
                          m_active ? p.brush(QPalette::Active, QPalette::Highlight) :
                          p.brush(QPalette::WindowText);
}
#endif
