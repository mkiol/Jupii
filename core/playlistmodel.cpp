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
#include <utility>

#include "playlistmodel.h"
#include "utils.h"
#include "filemetadata.h"
#include "settings.h"
#include "services.h"

#ifdef DESKTOP
//#include <QPixmap>
//#include <QImage>
#include <QPalette>
#include <QApplication>
//#include "filedownloader.h"
#endif

PlaylistModel* PlaylistModel::m_instance = nullptr;

PlaylistWorker::PlaylistWorker(const QList<UrlItem> &&urls,
                               bool asAudio,
                               bool urlIsId,
                               QObject *parent) :
    QThread(parent),
    urls(urls),
    asAudio(asAudio),
    urlIsId(urlIsId)
{
}

void PlaylistWorker::run()
{
    QList<QUrl> ids;

    if (urlIsId) {
        for (const auto &purl : urls)
            ids << purl.url;
    } else {
        for (const auto &purl : urls) {
            const auto &url = purl.url;
            const auto &name = purl.name;
            const auto &icon = purl.icon;
            const auto &desc = purl.desc;

            if (Utils::isUrlValid(url)) {
                QUrl id(url);
                QUrlQuery q(url);

                // cookie
                if (q.hasQueryItem(Utils::cookieKey))
                    q.removeQueryItem(Utils::cookieKey);
                q.addQueryItem(Utils::cookieKey, Utils::randString());

                // name
                if (!name.isEmpty()) {
                    if (q.hasQueryItem(Utils::nameKey))
                        q.removeQueryItem(Utils::nameKey);
                        q.addQueryItem(Utils::nameKey, name);
                }

                // type
                if (asAudio) {
                    if (q.hasQueryItem(Utils::typeKey))
                        q.removeQueryItem(Utils::typeKey);
                    q.addQueryItem(Utils::typeKey, QString::number(ContentServer::TypeMusic));
                }

                // icon
                if (!icon.isEmpty()) {
                    if (q.hasQueryItem(Utils::iconKey))
                        q.removeQueryItem(Utils::iconKey);
                    q.addQueryItem(Utils::iconKey, icon.toString());
                }

                // desc
                if (!desc.isEmpty()) {
                    if (q.hasQueryItem(Utils::descKey))
                        q.removeQueryItem(Utils::descKey);
                    q.addQueryItem(Utils::descKey, desc);
                }

                id.setQuery(q);

                ids << id;
            }
        }
    }

    auto pl = PlaylistModel::instance();
    for (auto &id : ids) {
        auto item = pl->makeItem(id);
        if (item)
            items << item;
    }
}

PlaylistModel* PlaylistModel::instance(QObject *parent)
{
    if (PlaylistModel::m_instance == nullptr) {
        PlaylistModel::m_instance = new PlaylistModel(parent);
    }

    return PlaylistModel::m_instance;
}

PlaylistModel::PlaylistModel(QObject *parent) :
    ListModel(new PlaylistItem, parent)
{
    auto s = Settings::instance();
    auto services = Services::instance();
    auto rc = services->renderingControl;
    auto av = services->avTransport;

    connect(av.get(), &AVTransport::currentURIChanged,
            this, &PlaylistModel::onAvCurrentURIChanged);
    connect(av.get(), &AVTransport::nextURIChanged,
            this, &PlaylistModel::onAvNextURIChanged);
    connect(av.get(), &AVTransport::trackEnded,
            this, &PlaylistModel::onAvTrackEnded);
    connect(av.get(), &AVTransport::transportStateChanged,
            this, &PlaylistModel::onAvStateChanged);
    connect(av.get(), &AVTransport::initedChanged,
            this, &PlaylistModel::onAvInitedChanged);
    connect(av.get(), &AVTransport::busyChanged,
            this, &PlaylistModel::onAvInitedChanged);
    connect(av.get(), &AVTransport::transportActionsChanged,
            this, &PlaylistModel::onSupportedChanged);
    connect(av.get(), &AVTransport::relativeTimePositionChanged,
            this, &PlaylistModel::onSupportedChanged);
    connect(this, &PlaylistModel::itemsAdded,
            this, &PlaylistModel::onSupportedChanged);
    connect(this, &PlaylistModel::itemsLoaded,
            this, &PlaylistModel::onSupportedChanged);
    connect(this, &PlaylistModel::itemsRemoved,
            this, &PlaylistModel::onSupportedChanged);
    connect(this, &PlaylistModel::playModeChanged,
            this, &PlaylistModel::onSupportedChanged);
    connect(this, &PlaylistModel::itemsAdded,
            this, &PlaylistModel::onItemsAdded);
    connect(this, &PlaylistModel::itemsLoaded,
            this, &PlaylistModel::onItemsLoaded);
    connect(this, &PlaylistModel::itemsRemoved,
            this, &PlaylistModel::onItemsRemoved);

    if (s->getRememberPlaylist())
        load();
}

void PlaylistModel::onSupportedChanged()
{
    updateNextSupported();
    updatePrevSupported();
}

void PlaylistModel::updatePrevSupported()
{
    auto av = Services::instance()->avTransport;
    bool value = (av->getSeekSupported() &&
                av->getTransportState() == AVTransport::Playing &&
                av->getRelativeTimePosition() > 5) ||
               (m_playMode != PlaylistModel::PM_RepeatOne &&
                m_activeItemIndex > -1);

    if (m_prevSupported != value) {
        m_prevSupported = value;
        emit prevSupportedChanged();
    }
}

void PlaylistModel::updateNextSupported()
{
    /*auto av = Services::instance()->avTransport;
    bool value =  m_playMode != PlaylistModel::PM_RepeatOne &&
                        av->getNextSupported() && rowCount() > 0;*/
    bool value =  m_playMode != PlaylistModel::PM_RepeatOne &&
                        rowCount() > 0;

    if (m_nextSupported != value) {
        m_nextSupported = value;
        emit nextSupportedChanged();
    }
}

bool PlaylistModel::isNextSupported()
{
    return m_nextSupported;
}

bool PlaylistModel::isPrevSupported()
{
    return m_prevSupported;
}

void PlaylistModel::next()
{
    auto av = Services::instance()->avTransport;

    if (rowCount() > 0) {
        auto fid = firstId();
        auto aid = activeId();
        auto nid = nextActiveId();
        if (aid.isEmpty()) {
            setToBeActiveId(fid);
            av->setLocalContent(fid, nid);
        } else {
            setToBeActiveId(nid);
            av->setLocalContent(nid, "");
        }
    } else {
        qWarning() << "Playlist is empty so can't do next()";
    }
}

void PlaylistModel::prev()
{
    auto av = Services::instance()->avTransport;
    bool seekable = av->getSeekSupported();

    if (rowCount() > 0) {
        auto pid = prevActiveId();
        auto aid = activeId();

        if (aid.isEmpty()) {
            if (seekable)
                av->seek(0);
            return;
        }

        if (pid.isEmpty()) {
            if (seekable)
                av->seek(0);
            return;
        }

        if (seekable && av->getRelativeTimePosition() > 5) {
            av->seek(0);
        } else {
            setToBeActiveId(pid);
            av->setLocalContent(pid, aid);
        }
    } else {
        if (seekable)
            av->seek(0);
    }
}

void PlaylistModel::onAvCurrentURIChanged()
{
    auto av = Services::instance()->avTransport;
    qDebug() << "onAvCurrentURIChanged:" << av->getCurrentURI();

    setActiveUrl(av->getCurrentURI());
    bool play = av->getTransportState() != AVTransport::Playing && rowCount() == 1;
    update(play);
}

void PlaylistModel::onAvNextURIChanged()
{
    auto av = Services::instance()->avTransport;
    qDebug() << "onAvNextURIChanged:" << av->getNextURI();

    auto nextURI = av->getNextURI();
    auto currentURI = av->getCurrentURI();
    bool normalPlayMode = getPlayMode() == PlaylistModel::PM_Normal;

    if (nextURI.isEmpty() && !currentURI.isEmpty() && normalPlayMode) {
        qDebug() << "AVT switches to nextURI without currentURIChanged";
        setActiveUrl(currentURI);

        bool play = av->getTransportState() != AVTransport::Playing && rowCount() == 1;
        update(play);
    }
}

void PlaylistModel::onAvTrackEnded()
{
    qDebug() << "onAvTrackEnded";

    auto av = Services::instance()->avTransport;

    if (rowCount() > 0 && (!av->getNextURISupported() ||
        m_playMode == PlaylistModel::PM_RepeatOne))
    {
        auto aid = activeId();
        if (!aid.isEmpty()) {
            auto nid = nextActiveId();
            if (!nid.isEmpty())
                av->setLocalContent(nid,"");
        }
    }
}

void PlaylistModel::onAvStateChanged()
{
    qDebug() << "onAvStateChanged";
    update();
}

void PlaylistModel::onAvInitedChanged()
{
    auto av = Services::instance()->avTransport;
    bool inited = av->getInited();
    bool busy = av->getBusy();

    qDebug() << "onAvInitedChanged:" << inited << busy;

    if (inited && !busy) {
        setActiveUrl(av->getCurrentURI());
        update(false);
    }
}

void PlaylistModel::onItemsAdded()
{
    qDebug() << "onItemsAdded";
    auto av = Services::instance()->avTransport;
    setActiveUrl(av->getCurrentURI());
    update(av->getTransportState() != AVTransport::Playing && rowCount() == 1);
}

void PlaylistModel::onItemsLoaded()
{
    qDebug() << "onItemsLoaded";
    auto av = Services::instance()->avTransport;
    setActiveUrl(av->getCurrentURI());
    update();
}

void PlaylistModel::onItemsRemoved()
{
    update();
}

void PlaylistModel::save()
{
    QStringList ids;

    for (auto item : m_list) {
        ids << item->id();
    }

    Settings::instance()->setLastPlaylist(ids);
}

void PlaylistModel::update(bool play)
{
    qDebug() << "Update playlist:" << play;

    if (rowCount() > 0) {
        auto av = Services::instance()->avTransport;
        if (av->getInited()) {
            auto aid = activeId();
            if (aid.isEmpty()) {
                auto fid = firstId();
                if (play) {
                    auto sid = secondId();
                    setToBeActiveId(fid);
                    av->setLocalContent(fid, sid);
                } else {
                    if (!fid.isEmpty())
                        av->setLocalContent("", fid);
                }
            } else {
                auto nid = nextActiveId();
                av->setLocalContent("", nid);
            }
        }
    }
}

bool PlaylistModel::isBusy()
{
    return m_busy;
}

void PlaylistModel::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool PlaylistModel::saveToFile(const QString& title)
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

QByteArray PlaylistModel::makePlsData(const QString& name)
{
    QByteArray data;
    QTextStream sdata(&data);

    sdata << "[playlist]" << endl;
    sdata << "X-GNOME-Title=" << name << endl;
    sdata << "NumberOfEntries=" << m_list.size() << endl;

    int l = m_list.size();
    for (int i = 0; i < l; ++i) {
        auto pitem = dynamic_cast<PlaylistItem*>(m_list.at(i));
        auto url = Utils::urlWithTypeFromId(pitem->id()).toString();
        auto name = Utils::nameFromId(pitem->id());
        sdata << "File" << i + 1 << "=" << url << endl;
        if (!name.isEmpty())
            sdata << "Title" << i + 1 << "=" << name << endl;
    }

    return data;
}

void PlaylistModel::load()
{
    setBusy(true);

    auto ids = Settings::instance()->getLastPlaylist();

    QList<UrlItem> urls;
    for (const auto &id : ids)
        urls << UrlItem{id};

    m_worker = std::unique_ptr<PlaylistWorker>(new PlaylistWorker(std::move(urls), false, true, this));
    connect(m_worker.get(), &PlaylistWorker::finished, this, &PlaylistModel::workerDone);
    m_worker->start();
}

int PlaylistModel::getPlayMode() const
{
    return m_playMode;
}

void PlaylistModel::setPlayMode(int value)
{
    if (value != m_playMode) {
        m_playMode = value;
        emit playModeChanged();
        update();
    }
}

void PlaylistModel::togglePlayMode()
{
    switch(m_playMode) {
    case PlaylistModel::PM_Normal:
        setPlayMode(PlaylistModel::PM_RepeatAll);
        break;
    case PlaylistModel::PM_RepeatAll:
        setPlayMode(PlaylistModel::PM_RepeatOne);
        break;
    case PlaylistModel::PM_RepeatOne:
        setPlayMode(PlaylistModel::PM_Normal);
        break;
    default:
        setPlayMode(PlaylistModel::PM_Normal);
    }
}

int PlaylistModel::getActiveItemIndex() const
{
    return m_activeItemIndex;
}

void PlaylistModel::addItemPaths(const QStringList& paths)
{
    QList<UrlItem> urls;
    for (auto& path : paths)
        urls << UrlItem{QUrl::fromLocalFile(path)};
    addItems(urls, false);
}

void PlaylistModel::addItemUrls(const QList<QUrl>& urls)
{
    addItems(urls, false);
}

void PlaylistModel::addItemUrls(const QList<UrlItem> &urls)
{
    addItems(urls, false);
}

void PlaylistModel::addItemUrl(const QUrl& url,
                               const QString& name,
                               const QUrl &icon,
                               const QString& desc)
{
    QList<UrlItem> urls;
    urls << UrlItem{url, name, icon, desc};
    addItems(urls, false);
}

void PlaylistModel::addItemPath(const QString& path,
                                const QString& name)
{
    QList<UrlItem> urls;
    urls << UrlItem{QUrl::fromLocalFile(path), name};
    addItems(urls, false);
}

void PlaylistModel::addItemPathsAsAudio(const QStringList& paths)
{
    QList<UrlItem> urls;
    for (auto& path : paths)
        urls << UrlItem{QUrl::fromLocalFile(path)};
    addItems(urls, true);
}

void PlaylistModel::addItems(const QList<UrlItem>& urls, bool asAudio)
{
    if (urls.isEmpty())
        return;

    setBusy(true);

    m_worker = std::unique_ptr<PlaylistWorker>(new PlaylistWorker(std::move(urls), asAudio, false, this));
    connect(m_worker.get(), &PlaylistWorker::finished, this, &PlaylistModel::workerDone);
    m_worker->start();
}

void PlaylistModel::addItems(const QList<QUrl>& urls, bool asAudio)
{
    if (urls.isEmpty())
        return;

    QList<UrlItem> purls;
    for (const auto &url : urls)
        purls << UrlItem{url};

    addItems(purls, asAudio);
}

void PlaylistModel::workerDone()
{
    qDebug() << "workerDone";

    if (m_worker) {
        if (m_worker->items.length() != m_worker->urls.length()) {
            qWarning() << "Some urls are invalid and can't be added to the playlist";
            if (m_worker->urls.length() == 1)
                emit error(E_ItemNotAdded);
            else if (m_worker->items.length() == 0)
                emit error(E_AllItemsNotAdded);
            else
                emit error(E_SomeItemsNotAdded);
        }

        if (!m_worker->items.isEmpty()) {
            appendRows(m_worker->items);
            if (m_worker->urlIsId)
                emit itemsAdded();
            else
                emit itemsLoaded();
            if (Settings::instance()->getRememberPlaylist())
                save();
        } else {
            qWarning() << "No items to add to playlist";
        }
    } else {
        qWarning() << "Worker done signal but worker is null";
    }

    setBusy(false);
}

PlaylistItem* PlaylistModel::makeItem(const QUrl &id)
{
    qDebug() << "makeItem:" << id;

    /*if (find(id.toString())) {
        qWarning() << "Id" << id << "already added";
        emit error(E_FileExists);
        return nullptr;
    }*/

    int t = 0;
    QString name, cookie;
    QUrl ficon;
    if (!Utils::pathTypeNameCookieIconFromId(id, nullptr, &t,
                                             &name, &cookie, &ficon) ||
            cookie.isEmpty()) {
        qWarning() << "Invalid Id";
        return nullptr;
    }

    QUrl url = Utils::urlFromId(id);

    const ContentServer::ItemMeta *meta;
    meta = ContentServer::instance()->getMeta(url);
    if (!meta) {
        qWarning() << "No meta item found";
        return nullptr;
    }

    auto forcedType = static_cast<ContentServer::Type>(t);
    auto type = forcedType == ContentServer::TypeUnknown ?
                meta->type : forcedType;

    if (name.isEmpty())
        name = ContentServer::bestName(*meta);

    QString iconUrl = ficon.isEmpty() ? meta->albumArt : ficon.toString();
#ifndef SAILFISH
    QIcon icon;
    if (iconUrl.isEmpty()) {
        switch (type) {
        case ContentServer::TypeMusic:
            icon = QIcon::fromTheme("audio-mp3");
            break;
        case ContentServer::TypeVideo:
            icon = QIcon::fromTheme("video-mp4");
            break;
        case ContentServer::TypeImage:
            icon = QIcon::fromTheme("image-jpeg");
            break;
        default:
            icon = QIcon::fromTheme("unknown");
            break;
        }
    } else {
        icon = QIcon(iconUrl);
    }
#endif

    auto item = new PlaylistItem(meta->url == url ?
                                   id : Utils::swapUrlInId(meta->url, id), // id
                               name, // name
                               meta->url, // url
                               type, // type
                               meta->title, // title
                               meta->artist, // artist
                               meta->album, // album
                               "", // date
                               meta->duration, // duration
                               meta->size, // size
#ifdef SAILFISH
                               iconUrl, // icon
#else
                               icon, // icon
#endif
                               false, // active
                               false // to be active
                               );

    return item;
}

bool PlaylistModel::addId(const QUrl &id)
{
    auto item = makeItem(id);
    if (item)
        appendRow(item);
    else
        return false;

    return true;
}

void PlaylistModel::setActiveId(const QString &id)
{
    //qDebug() << "setActiveId";
    //qDebug() << "id:" << id;
    //qDebug() << "current active id:" << activeId();

    if (id == activeId())
        return;

    if (id.isEmpty())
        setActiveItemIndex(-1);

    const int len = m_list.length();
    bool active_found = false;
    for (int i = 0; i < len; ++i) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(i));
        if (!fi) {
            qWarning() << "Dynamic cast is null";
            return;
        }
        bool new_active = fi->id() == id;

        if (new_active)
            active_found = true;

        if (fi->active() != new_active) {
            fi->setActive(new_active);
            if(new_active)
                setActiveItemIndex(i);
            emit activeItemChanged();
        }

        fi->setToBeActive(false);
    }

    if (!active_found)
        setActiveItemIndex(-1);
}

void PlaylistModel::resetToBeActive()
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

void PlaylistModel::setToBeActiveIndex(int index)
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

void PlaylistModel::setToBeActiveId(const QString &id)
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

void PlaylistModel::setActiveUrl(const QUrl &url)
{
    qDebug() << "Setting active URL:" << url;
    if (url.isEmpty()) {
        setActiveId(QString());
    } else {
        auto cs = ContentServer::instance();
        setActiveId(cs->idFromUrl(url));
    }
}

void PlaylistModel::clear()
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
        emit itemsRemoved();
    }

    setActiveItemIndex(-1);

    if (Settings::instance()->getRememberPlaylist())
        save();

    if(active_removed)
        emit activeItemChanged();

    update();
}

QString PlaylistModel::activeId() const
{
    if (m_activeItemIndex > -1) {
        auto fi = m_list.at(m_activeItemIndex);
        return fi->id();
    }

    return QString();
}

QString PlaylistModel::firstId() const
{
    if (m_list.length() > 0)
        return m_list.first()->id();

    return QString();
}

QString PlaylistModel::secondId() const
{
    if (m_list.length() > 1)
        return m_list.at(1)->id();

    return QString();
}

void PlaylistModel::setActiveItemIndex(int index)
{
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemIndexChanged();
    }
}

bool PlaylistModel::removeIndex(int index)
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

        emit itemsRemoved();

        if(active_removed)
            emit activeItemChanged();
    }

    return ok;
}

bool PlaylistModel::remove(const QString &id)
{
    return removeIndex(indexFromId(id));
}

QString PlaylistModel::nextActiveId() const
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

QString PlaylistModel::prevActiveId() const
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


QString PlaylistModel::nextId(const QString &id) const
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
#ifdef SAILFISH
                           const QUrl &icon,
#else
                           const QIcon &icon,
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
