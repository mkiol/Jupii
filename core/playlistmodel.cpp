/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDataStream>
#include <QUrlQuery>
#include <QTimer>
#include <QTextStream>
#include <utility>

#include "playlistmodel.h"
#include "utils.h"
#include "filemetadata.h"
#include "settings.h"
#include "services.h"
#include "directory.h"

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
        QList<UrlItem> nurls;

        // check for playlists
        for (const auto& url : urls) {
            bool isPlaylist = false;

            if (url.url.isLocalFile()) {
                auto type = ContentServer::getContentTypeByExtension(url.url.path());
                if (type == ContentServer::TypePlaylist) {
                    isPlaylist = true;
                    auto path = url.url.toLocalFile();
                    qDebug() << "File" << path << "is a playlist";

                    QFile f(path);
                    if (f.exists()) {
                        if (f.open(QIODevice::ReadOnly)) {
                            auto data = f.readAll();
                            f.close();
                            if (!data.isEmpty()) {
                                QFileInfo fi(f); auto dir = fi.absoluteDir().path();
                                auto ptype = ContentServer::playlistTypeFromExtension(path);
                                auto items = ptype == ContentServer::PlaylistPLS ?
                                             ContentServer::parsePls(data, dir) :
                                                ptype == ContentServer::PlaylistXSPF ?
                                                ContentServer::parseXspf(data, dir) :
                                                    ContentServer::parseM3u(data, dir);

                                if (items.isEmpty()) {
                                    qWarning() << "Playlist doesn't contain any valid items";
                                } else {
                                    for (const auto& item : items) {
                                        // TODO: Consider playlist item title as well
                                        UrlItem ui; ui.url = item.url;
                                        nurls << ui;
                                    }
                                }
                            } else {
                                qWarning() << "Playlist content is empty";
                            }
                        } else {
                            qWarning() << "Cannot open file" << f.fileName() <<
                                          "for reading (" + f.errorString() + ")";
                        }
                    } else {
                        qWarning() << "File" << f.fileName() << "doesn't exist";
                    }
                }
            }

            if (!isPlaylist)
                nurls << url;
        }

        urls = nurls;

        for (const auto &purl : urls) {
            const auto &url = purl.url;
            const auto &name = purl.name;
            const auto &author = purl.author;
            const auto &icon = purl.icon;
            const auto &desc = purl.desc;
            bool play = purl.play;

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

                // author
                if (!author.isEmpty()) {
                    if (q.hasQueryItem(Utils::authorKey))
                        q.removeQueryItem(Utils::authorKey);
                    q.addQueryItem(Utils::authorKey, author);
                }

                // play
                if (play) {
                    if (q.hasQueryItem(Utils::playKey))
                        q.removeQueryItem(Utils::playKey);
                    q.addQueryItem(Utils::playKey, "true");
                }

                id.setQuery(q);

                ids << id;
            }
        }
    }

    auto pl = PlaylistModel::instance();
    for (const auto &id : ids) {
        qDebug() << "Calling makeItem:" << QThread::currentThreadId();
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
    auto av = services->avTransport;
    auto dir = Directory::instance();

    m_playMode = s->getPlayMode();

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
    connect(dir, &Directory::initedChanged, this,
            &PlaylistModel::load, Qt::QueuedConnection);

#ifdef SAILFISH
    m_backgroundActivity = new BackgroundActivity(this);
    m_backgroundActivity->setWakeupFrequency(BackgroundActivity::ThirtySeconds);
    connect(m_backgroundActivity, &BackgroundActivity::stateChanged, this,
            &PlaylistModel::handleBackgroundActivityStateChange);
#endif
    load();
}

#ifdef SAILFISH
void PlaylistModel::handleBackgroundActivityStateChange()
{
    qDebug() << "Background activity state:" << m_backgroundActivity->state();
}
void PlaylistModel::updateBackgroundActivity()
{
    auto av = Services::instance()->avTransport;
    bool playing = av->getInited() &&
            av->getTransportState() == AVTransport::Playing;

    qDebug() << "updateBackgroundActivity:" << playing;

    if (playing) {
        if (!m_backgroundActivity->isRunning())
            m_backgroundActivity->run();
    } else {
        if (m_backgroundActivity->isRunning())
            m_backgroundActivity->stop();
    }
}
#endif

std::pair<int, QString> PlaylistModel::getDidlList(int max, const QString& didlId)
{
    std::pair<int, QString> ret;

    QTextStream m(&ret.second);
    m << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" ";
    m << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    m << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" ";
    m << "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">";

    auto cs = ContentServer::instance();

    if (didlId.isEmpty()) {
        int count = 0;
        for (auto li : m_list) {
            if (max > 0 && count > max) {
                break;
            }
            auto fi = dynamic_cast<PlaylistItem*>(li);
            if (fi->itemType() != ContentServer::ItemType_Upnp &&
                    cs->getContentMetaItem(fi->id(), ret.second)) {
                ++count;
            }
        }
        ret.first = count;
    } else {
        cs->getContentMetaItemByDidlId(didlId, ret.second);
        ret.first = 1;
    }

    m << "</DIDL-Lite>";

    return ret;
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
        qWarning() << "Playlist is empty so cannot do next()";
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
#ifdef SAILFISH
    updateBackgroundActivity();
#endif
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

#ifdef SAILFISH
    updateBackgroundActivity();
#endif
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
    qDebug() << "Saving current playlist";

    QStringList p_ids;
    QStringList d_ids;

    for (auto item : m_list) {
        p_ids << item->id();

        auto pitem = dynamic_cast<PlaylistItem*>(item);
        if (pitem->itemType() == ContentServer::ItemType_Upnp &&
                !pitem->devId().isEmpty()) {
            d_ids << pitem->devId();
        }
    }

    auto s = Settings::instance();
    s->setLastPlaylist(p_ids);
    s->addLastDevices(d_ids);
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
        if (name.isEmpty()) {
            if (pitem->artist().isEmpty())
                name = pitem->title();
            else
                name = pitem->artist() + " - " + pitem->title();
        }
        sdata << "File" << i + 1 << "=" << url << endl;
        if (!name.isEmpty())
            sdata << "Title" << i + 1 << "=" << name << endl;
    }

    return data;
}

void PlaylistModel::load()
{
    qDebug() << "Playlist load";

    auto dir = Directory::instance();
    if (!dir->getInited()) {
        qWarning() << "Unloading playlist because directory is not inited";
        clear(false);
        return;
    }

    setBusy(true);

    auto ids = Settings::instance()->getLastPlaylist();

    QList<UrlItem> urls;
    for (const auto &id : ids) {
        UrlItem ui; ui.url = id;
        urls << ui;
    }

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
        auto s = Settings::instance();
        s->setPlayMode(m_playMode);
        emit playModeChanged();
        update();
    }
}

void PlaylistModel::togglePlayMode()
{
    // PM_Normal has some bug therefore will be disabled
    switch(m_playMode) {
    /*case PlaylistModel::PM_Normal:
        setPlayMode(PlaylistModel::PM_RepeatAll);
        break;*/
    case PlaylistModel::PM_RepeatAll:
        setPlayMode(PlaylistModel::PM_RepeatOne);
        break;
    /*case PlaylistModel::PM_RepeatOne:
        setPlayMode(PlaylistModel::PM_Normal);
        break;*/
    default:
        setPlayMode(PlaylistModel::PM_RepeatAll);
    }
}

int PlaylistModel::getActiveItemIndex() const
{
    return m_activeItemIndex;
}

void PlaylistModel::addItemPaths(const QStringList& paths)
{
    QList<UrlItem> urls;
    for (auto& path : paths) {
        UrlItem ui; ui.url = QUrl::fromLocalFile(path);
        urls << ui;
    }
    addItems(urls, false);
}

void PlaylistModel::addItemUrls(const QList<UrlItem> &urls)
{
    addItems(urls, false);
}

void PlaylistModel::addItemUrls(const QVariantList &urls)
{
    qDebug() << urls;
    QList<UrlItem> items;
    for (const QVariant &item : urls) {
        if (item.canConvert(QMetaType::QVariantMap)) {
            auto m = item.toMap();
            UrlItem ui;
            ui.url = m.value("url").toUrl();
            ui.name = m.value("name").toString();
            ui.author = m.value("author").toString();
            ui.icon = m.value("icon").toUrl();
            ui.desc = m.value("desc").toString();
            items << ui;
        } else if (item.canConvert(QMetaType::QUrl)) {
            UrlItem ui; ui.url = item.toUrl();
            items << ui;
        }
    }

    addItems(items, false);
}

void PlaylistModel::addItemUrl(const QUrl& url,
                               const QString& name,
                               const QString& author,
                               const QUrl &icon,
                               const QString& desc,
                               bool play)
{
    QList<UrlItem> urls;
    UrlItem ui;
    ui.url = url;
    ui.name = name;
    ui.author = author;
    ui.icon = icon;
    ui.desc = desc;
    ui.play = play;
    urls << ui;
    addItems(urls, false);
}

void PlaylistModel::addItemPath(const QString& path,
                                const QString& name,
                                bool play)
{
    QList<UrlItem> urls;
    UrlItem ui;
    ui.url = QUrl::fromLocalFile(path);
    ui.name = name;
    ui.play = play;
    urls << ui;
    addItems(urls, false);
}

bool PlaylistModel::pathExists(const QString& path)
{
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
        if (fi->path() == path)
            return true;
    }

    return false;
}

bool PlaylistModel::playPath(const QString& path)
{
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
        if (fi->path() == path) {
            // path exists, so playing it
            fi->setPlay(true);
            autoPlay();
            return true;
        }
    }

    return false;
}

bool PlaylistModel::urlExists(const QUrl& url)
{
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
        if (fi->url() == url || fi->origUrl() == url)
            return true;
    }

    return false;
}

bool PlaylistModel::playUrl(const QUrl& url)
{
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
        if (fi->url() == url || fi->origUrl() == url) {
            // url exists, so playing it
            fi->setPlay(true);
            autoPlay();
            return true;
        }
    }

    return false;
}

void PlaylistModel::addItemPathsAsAudio(const QStringList& paths)
{
    QList<UrlItem> urls;
    for (auto& path : paths) {
        UrlItem ui; ui.url = QUrl::fromLocalFile(path);
        urls << ui;
    }
    addItems(urls, true);
}

void PlaylistModel::addItems(const QList<UrlItem>& urls, bool asAudio)
{
    qDebug() << "addItems:" << QThread::currentThreadId();
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
    for (const auto &url : urls) {
        UrlItem ui; ui.url = url;
        purls << ui;
    }

    addItems(purls, asAudio);
}

void PlaylistModel::workerDone()
{
    qDebug() << "workerDone:" << QThread::currentThreadId();

    if (m_worker) {
        if (m_worker->items.length() != m_worker->urls.length()) {
            qWarning() << "Some urls are invalid and cannot be added to the playlist";
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

            // saving last play queue
            save();

            // auto playing
            autoPlay();
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
    qDebug() << "makeItem:" << id << QThread::currentThreadId();

    int t = 0; QString name, cookie, author; QUrl ficon, origUrl; bool play = false;
    if (!Utils::pathTypeNameCookieIconFromId(id, nullptr, &t,
                &name, &cookie, &ficon, nullptr, &author, &origUrl, &play) ||
            cookie.isEmpty()) {
        qWarning() << "Invalid Id";
        return nullptr;
    }

    if (play)
        qDebug() << "Auto play is enabled";

    QUrl url = Utils::urlFromId(id);

    const ContentServer::ItemMeta *meta;
    meta = ContentServer::instance()->getMeta(url, true, origUrl);
    if (!meta) {
        qWarning() << "No meta item found";
        return nullptr;
    }

    auto forcedType = static_cast<ContentServer::Type>(t);
    auto type = forcedType == ContentServer::TypeUnknown ?
                meta->type : forcedType;

    auto finalId = id;

    if (name.isEmpty()) {
        name = ContentServer::bestName(*meta);
        if (meta->ytdl) { // add discovered name to url for youtube-dl content
            QUrlQuery q(finalId);
            if (q.hasQueryItem(Utils::nameKey))
                q.removeQueryItem(Utils::nameKey);
            q.addQueryItem(Utils::nameKey, name);
            finalId.setQuery(q);
        }
    }

    if (origUrl.isEmpty()) {
        if (meta->origUrl != meta->url) {
            QUrlQuery q(finalId);
            if (q.hasQueryItem(Utils::origUrlKey))
                q.removeQueryItem(Utils::origUrlKey);
            q.addQueryItem(Utils::origUrlKey, meta->origUrl.toString());
            finalId.setQuery(q);
        }
    } else {
        url = origUrl;
    }

    QString iconUrl = ficon.isEmpty() ?
                meta->albumArt.isEmpty() ? type == ContentServer::TypeImage ?
                meta->url.toString() : "" : meta->albumArt : ficon.toString();
#ifndef SAILFISH
    QIcon icon;
    if (iconUrl.isEmpty()) {
        if (Utils::isUrlMic(url)) {
            icon = QIcon::fromTheme("audio-input-microphone");
        } else {
            switch (type) {
            case ContentServer::TypeMusic:
                icon = QIcon::fromTheme("audio-x-generic");
                break;
            case ContentServer::TypeVideo:
                icon = QIcon::fromTheme("video-x-generic");
                break;
            case ContentServer::TypeImage:
                icon = QIcon::fromTheme("image-x-generic");
                break;
            default:
                icon = QIcon::fromTheme("unknown");
                break;
            }
        }
    } else {
        icon = QIcon(iconUrl);
    }
#endif
    finalId = meta->itemType == ContentServer::ItemType_Upnp ||
            meta->url == url ? Utils::cleanId(finalId) : Utils::swapUrlInId(meta->url, finalId);
    qDebug() << "final id:" << finalId;
    auto item = new PlaylistItem(finalId, // id
                               name, // name
                               meta->url, // url
                               url, // orig url
                               type, // type
                               meta->title, // title
                               author.isEmpty() ? meta->artist : author, // artist
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
                               false, // to be active
                               play, // play
                               meta->itemType,
                               meta->upnpDevId
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
        fi->setToBeActive(false);
    }
}

void PlaylistModel::setToBeActiveIndex(int index)
{
    const int len = m_list.length();
    for (int i = 0; i < len; ++i) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(i));
        fi->setToBeActive(i == index);
    }
}

void PlaylistModel::setToBeActiveId(const QString &id)
{
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
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
        //setActiveCookie(cs->cookieFromUrl(url));
        setActiveId(cs->idFromUrl(url));
    }
}

void PlaylistModel::clear(bool save)
{
    bool active_removed = false;
    if (m_activeItemIndex > -1) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(m_activeItemIndex));
        if (fi->active())
            active_removed = true;
    }

    if(rowCount() > 0) {
        removeRows(0, rowCount());
        emit itemsRemoved();
    }

    setActiveItemIndex(-1);

    if (save) // saving last play queue
        this->save();

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

void PlaylistModel::autoPlay()
{
    auto av = Services::instance()->avTransport;
    if (!av) {
        qWarning() << "AV not connected";
        return;
    }

    auto ai = activeId();
    bool playing = static_cast<AVTransport::TransportState>
            (av->getTransportState()) == AVTransport::Playing;

    // start playing fist item with play flag
    bool done = false;
    for (auto li : m_list) {
        auto fi = dynamic_cast<PlaylistItem*>(li);
        if (!done && fi->play()) {
            if (ai != fi->id() || !playing) {
                qDebug() << "Playing item:" << fi->id();
                //fi->setToBeActive(true);
                auto nid = nextId(fi->id());
                av->setLocalContent(fi->id(), nid);
            }
            done = true;
        }
        fi->setPlay(false);
    }
}

bool PlaylistModel::removeIndex(int index)
{
    if (index < 0)
        return false;

    auto fi = dynamic_cast<PlaylistItem*>(m_list.at(index));

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

        // saving last play queue
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
                           const QUrl &origUrl,
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
                           bool play,
                           ContentServer::ItemType itemType,
                           const QString &devId,
                           QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_name(name),
    m_url(url),
    m_origUrl(origUrl),
    m_type(type),
    m_title(title),
    m_artist(artist),
    m_album(album),
    m_date(date),
    m_duration(duration),
    m_size(size),
    m_icon(icon),
    m_active(active),
    m_play(play),
    m_item_type(itemType),
    m_devid(devId)
{
}

QHash<int, QByteArray> PlaylistItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[UrlRole] = "url";
    names[OrigUrlRole] = "origUrl";
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
    names[ItemTypeRole] = "itemType";
    names[DevIdRole] = "devid";
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
    case OrigUrlRole:
        return origUrl();
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
    case ItemTypeRole:
        return itemType();
    case DevIdRole:
        return devId();
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

void PlaylistItem::setPlay(bool value)
{
    m_play = value;
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
