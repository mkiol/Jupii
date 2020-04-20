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
#include "youtubedl.h"

#ifdef DESKTOP
//#include <QPixmap>
//#include <QImage>
#include <QPalette>
#include <QApplication>
//#include "filedownloader.h"
#endif

PlaylistModel* PlaylistModel::m_instance = nullptr;

PlaylistWorker::PlaylistWorker(const QList<UrlItem> &urls,
                               bool asAudio,
                               QObject *parent) :
    QThread(parent),
    urls(urls),
    asAudio(asAudio),
    urlIsId(false)
{
}

PlaylistWorker::PlaylistWorker(QList<UrlItem> &&urls,
                               bool asAudio,
                               QObject *parent) :
    QThread(parent),
    urls(urls),
    asAudio(asAudio),
    urlIsId(false)
{
}

PlaylistWorker::PlaylistWorker(QList<QUrl> &&ids, QObject *parent) :
    QThread(parent),
    ids(ids),
    urlIsId(true)
{
}

void PlaylistWorker::cancel()
{
    if (isRunning()) {
        qDebug() << "Playlist worker cancel";
        requestInterruption();
        quit();
        wait();
        qDebug() << "Playlist worker cancel ended";
    }
}

PlaylistWorker::~PlaylistWorker()
{
    cancel();
}

void PlaylistWorker::run()
{
    if (ids.isEmpty()) {
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
                q.addQueryItem(Utils::cookieKey, Utils::instance()->randString(10));

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
    for (int i = 0; i < ids.size(); ++i) {
        if (isInterruptionRequested()) {
            qDebug() << "Playlist worker interruption requested";
            return;
        }
        qDebug() << "Calling makeItem:" << QThread::currentThreadId();
        emit progress(i, ids.size());
        auto item = pl->makeItem(ids.at(i));
        if (item) {
            itemToOrigId.insert(item, ids.at(i));
            items << item;
        }
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
    auto cs = ContentServer::instance();

    m_playMode = s->getPlayMode();

    connect(av.get(), &AVTransport::currentURIChanged,
            this, &PlaylistModel::onAvCurrentURIChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::nextURIChanged,
            this, &PlaylistModel::onAvNextURIChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::trackEnded,
            this, &PlaylistModel::doOnTrackEnded, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::transportStateChanged,
            this, &PlaylistModel::onAvStateChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::initedChanged,
            this, &PlaylistModel::onAvInitedChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::busyChanged,
            this, &PlaylistModel::onAvInitedChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::transportActionsChanged,
            this, &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::relativeTimePositionChanged,
            this, &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsAdded,
            this, &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsLoaded,
            this, &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsRemoved,
            this, &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::playModeChanged,
            this, &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsAdded,
            this, &PlaylistModel::onItemsAdded, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsLoaded,
            this, &PlaylistModel::onItemsLoaded, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsRemoved,
            this, &PlaylistModel::onItemsRemoved, Qt::QueuedConnection);
    connect(dir, &Directory::initedChanged, this,
            &PlaylistModel::load, Qt::QueuedConnection);
    connect(cs, &ContentServer::fullHashesUpdated, this,
            &PlaylistModel::updateActiveId, Qt::QueuedConnection);

#ifdef SAILFISH
    m_backgroundActivity = new BackgroundActivity(this);
    m_backgroundActivity->setWakeupFrequency(BackgroundActivity::ThirtySeconds);
    connect(m_backgroundActivity, &BackgroundActivity::stateChanged, this,
            &PlaylistModel::handleBackgroundActivityStateChange);
#endif

    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(1000);
    connect(&m_updateTimer, &QTimer::timeout, this,
            &PlaylistModel::update, Qt::QueuedConnection);
    m_updateActiveTimer.setSingleShot(true);
    m_updateTimer.setInterval(1000);
    connect(&m_updateActiveTimer, &QTimer::timeout, this,
            &PlaylistModel::updateActiveId, Qt::QueuedConnection);
    m_trackEndedTimer.setSingleShot(true);
    m_trackEndedTimer.setInterval(1000);
    connect(&m_trackEndedTimer, &QTimer::timeout, this,
            &PlaylistModel::onTrackEnded, Qt::QueuedConnection);
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
                    cs->getContentMetaItem(fi->id(), ret.second, false)) {
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
    if (rowCount() > 0) {
        auto av = Services::instance()->avTransport;
        if (av->updating() || isBusy() || isRefreshing()) {
            qDebug() << "Playlist busy/refreshing or AV update is in progress, next skipped";
            return;
        }
        auto fid = firstId();
        auto aid = activeId();
        auto nid = nextActiveId();
        if (aid.isEmpty()) {
            refreshAndSetContent(fid, nid, true);
        } else {
            refreshAndSetContent(nid, "", true);
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
        if (av->updating() || isBusy() || isRefreshing()) {
            qDebug() << "Playlist busy/refreshing or AV update is in progress, prev skipped";
            return;
        }
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
            refreshAndSetContent(pid, aid, true);
        }
    } else {
        if (seekable)
            av->seek(0);
    }
}

void PlaylistModel::onAvCurrentURIChanged()
{
    updateActiveId();
    doUpdate();
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
        updateActiveId();
        doUpdate();
    }
}

void PlaylistModel::doOnTrackEnded()
{
    qDebug() << "doOnTrackEnded";
    m_trackEndedTimer.start();
}

void PlaylistModel::onTrackEnded()
{
    qDebug() << "onTrackEnded";

    auto av = Services::instance()->avTransport;

    if (av->getInited()) {
        if (av->updating() || isBusy() || isRefreshing()) {
            qDebug() << "Playlist busy/refreshing or AV update is in progress, "
                        "so delaying track end handling";
            doOnTrackEnded();
            return;
        }

        if (!m_list.isEmpty() && (!av->getNextURISupported() ||
            m_playMode == PlaylistModel::PM_RepeatOne ||
            (m_playMode == PlaylistModel::PM_RepeatAll && m_list.size() == 1)))
        {
            auto aid = activeId();
            if (!aid.isEmpty()) {
                auto nid = nextActiveId();
                if (!nid.isEmpty()) {
                    refreshAndSetContent(nid, "");
                }
            }
        }
    }
}

void PlaylistModel::onAvStateChanged()
{
    qDebug() << "onAvStateChanged";
    doUpdate();
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
        updateActiveId();
        doUpdate();
    }

#ifdef SAILFISH
    updateBackgroundActivity();
#endif
}

void PlaylistModel::onItemsAdded()
{
    qDebug() << "onItemsAdded";
    updateActiveId();
    doUpdate();
}

void PlaylistModel::onItemsLoaded()
{
    qDebug() << "onItemsLoaded";
    updateActiveId();
    doUpdate();
}

void PlaylistModel::onItemsRemoved()
{
    doUpdate();
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

void PlaylistModel::doUpdate()
{
    qDebug() << "doUpdate";
    m_updateTimer.start();
}

void PlaylistModel::update()
{
    qDebug() << "Update playlist";

    if (rowCount() > 0) {
        auto av = Services::instance()->avTransport;
        if (av->getInited()) {
            if (isRefreshing()) {
                qDebug() << "Playlist refreshing, skipping update";
                return;
            }
            if (av->updating() || isBusy()) {
                qDebug() << "Playlist busy or AV update is in progress, so delaying playlist update";
                doUpdate();
                return;
            }

            if (autoPlay()) {
                qDebug() << "Auto playing item";
                return;
            }

            auto aid = activeId();
            qDebug() << "aid:" << aid;
            if (!aid.isEmpty()) {
                auto av = Services::instance()->avTransport;
                bool updateCurrent = av->getCurrentId() != aid &&
                        av->getTransportState() == AVTransport::Playing;
                refreshAndSetContent(updateCurrent ? aid : "", nextActiveId());
            }
        }
    }
}

bool PlaylistModel::isBusy()
{
    return m_busy;
}

bool PlaylistModel::isRefreshing()
{
    return m_refresh_worker ? true : false;
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
        auto url = Utils::bestUrlFromId(QUrl(pitem->id())).toString();
        auto name = Utils::nameFromId(pitem->id());
        if (name.isEmpty()) {
            if (pitem->artist().isEmpty())
                name = pitem->name();
            else
                name = pitem->artist() + " - " + pitem->name();
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

    auto ids = QUrl::fromStringList(Settings::instance()->getLastPlaylist());
    for (auto& id : ids) {
        Utils::fixUrl(id);
    }

    m_add_worker = std::unique_ptr<PlaylistWorker>(new PlaylistWorker(std::move(ids)));
    connect(m_add_worker.get(), &PlaylistWorker::finished, this, &PlaylistModel::addWorkerDone);
    connect(m_add_worker.get(), &PlaylistWorker::progress, this, &PlaylistModel::progressUpdate);
    progressUpdate(0, 0);
    m_add_worker->start();
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
        doUpdate();
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

const PlaylistItem *PlaylistModel::getActiveItem() const
{
    return m_activeItemIndex > -1 && m_list.size() > m_activeItemIndex ?
                dynamic_cast<PlaylistItem*>(m_list.at(m_activeItemIndex)) :
                nullptr;
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
            Utils::fixUrl(ui.url);
            ui.name = m.value("name").toString();
            ui.author = m.value("author").toString();
            ui.icon = m.value("icon").toUrl();
            ui.desc = m.value("desc").toString();
            items << ui;
        } else if (item.canConvert(QMetaType::QUrl)) {
            UrlItem ui; ui.url = item.toUrl();
            Utils::fixUrl(ui.url);
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
    Utils::fixUrl(ui.url);
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
            play(fi->id());
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
            play(fi->id());
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
    if (urls.isEmpty() || isBusy()) {
        qWarning() << "Cannot add items";
        return;
    }

    setBusy(true);

    m_add_worker = std::unique_ptr<PlaylistWorker>(new PlaylistWorker(urls, asAudio));
    connect(m_add_worker.get(), &PlaylistWorker::finished, this, &PlaylistModel::addWorkerDone);
    connect(m_add_worker.get(), &PlaylistWorker::progress, this, &PlaylistModel::progressUpdate);
    progressUpdate(0, 0);
    m_add_worker->start();
}

void PlaylistModel::addItems(const QList<QUrl>& urls, bool asAudio)
{
    if (urls.isEmpty())
        return;

    QList<UrlItem> purls;
    for (const auto &url : urls) {
        UrlItem ui; ui.url = url;
        Utils::fixUrl(ui.url);
        purls << ui;
    }

    addItems(purls, asAudio);
}

void PlaylistModel::progressUpdate(int value, int total)
{
    bool changed = false;
    if (m_progressValue != value) {
        m_progressValue = value;
        changed = true;
    }
    if (m_progressTotal != total) {
        m_progressTotal = total;
        changed = true;
    }
    if (changed) {
        emit progressChanged();
    }
}

void PlaylistModel::addWorkerDone()
{
    qDebug() << "addWorkerDone";

    if (m_add_worker) {
        if (m_add_worker->items.length() != m_add_worker->ids.length()) {
            qWarning() << "Some urls are invalid and cannot be added to the playlist";
            if (m_add_worker->ids.length() == 1)
                emit error(E_ItemNotAdded);
            else if (m_add_worker->items.length() == 0)
                emit error(E_AllItemsNotAdded);
            else
                emit error(E_SomeItemsNotAdded);
        }

        if (!m_add_worker->items.isEmpty()) {
            int old_refreshable = m_refreshable_count;
            for (auto item : m_add_worker->items) {
                if (dynamic_cast<PlaylistItem*>(item)->refreshable())
                    ++m_refreshable_count;
            }

            appendRows(m_add_worker->items);

            if (!m_add_worker->urlIsId)
                emit itemsAdded();
            else
                emit itemsLoaded();

            // saving last play queue
            save();

            if (old_refreshable == 0 && m_refreshable_count > 0)
                emit refreshableChanged();
        } else {
            qWarning() << "No items to add to playlist";
        }
    } else {
        qWarning() << "Worker done signal but worker is null";
    }

    setBusy(false);
    progressUpdate(0, 0);
}

PlaylistItem* PlaylistModel::makeItem(const QUrl &id)
{
    qDebug() << "makeItem:" << id;

    int t = 0;
    QString name, cookie, author;
    QUrl ficon, origUrl;
    bool play = false;
    bool ytdl = false;
    if (!Utils::pathTypeNameCookieIconFromId(id, nullptr, &t,
                &name, &cookie, &ficon, nullptr, &author, &origUrl, &ytdl, &play) ||
            cookie.isEmpty()) {
        qWarning() << "Invalid Id";
        return nullptr;
    }

    if (play)
        qDebug() << "Auto play is enabled";

    QUrl url = Utils::urlFromId(id);

    const ContentServer::ItemMeta *meta;
    if (!origUrl.isEmpty() && origUrl != url) {
        meta = ContentServer::instance()->getMeta(url, false);
        if (meta && meta->expired()) {
            qDebug() << "Meta exipred";
            meta = ContentServer::instance()->getMeta(url, true, origUrl, ytdl, false, true);
        } else {
            meta = ContentServer::instance()->getMeta(url, true, origUrl, ytdl);
        }
    } else {
        meta = ContentServer::instance()->getMeta(url, true, origUrl, ytdl);
    }
    if (!meta) {
        qWarning() << "No meta item found";
        return nullptr;
    }

    auto finalId = id;

    if (!ytdl) {
        if (meta->ytdl) { // add ytdl to url for youtube-dl content
            QUrlQuery q(finalId);
            if (q.hasQueryItem(Utils::ytdlKey))
                q.removeQueryItem(Utils::ytdlKey);
            q.addQueryItem(Utils::ytdlKey, "true");
            finalId.setQuery(q);
            ytdl = true;
        }
    }

    auto type = static_cast<ContentServer::Type>(t);
    if (type == ContentServer::TypeUnknown) {
        if (meta->ytdl) { // add type to url for youtube-dl content
            QUrlQuery q(finalId);
            if (q.hasQueryItem(Utils::typeKey))
                q.removeQueryItem(Utils::typeKey);
            q.addQueryItem(Utils::typeKey, QString::number(meta->type));
            finalId.setQuery(q);
        }
        type = meta->type;
    }

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
        if (!meta->origUrl.isEmpty() && meta->origUrl != meta->url) {
            QUrlQuery q(finalId);
            if (q.hasQueryItem(Utils::origUrlKey))
                q.removeQueryItem(Utils::origUrlKey);
            q.addQueryItem(Utils::origUrlKey, meta->origUrl.toString());
            finalId.setQuery(q);
            origUrl = meta->origUrl;
        }
    }

    QUrl iconUrl = ficon.isEmpty() ?
                meta->albumArt.isEmpty() ? type == ContentServer::TypeImage ?
                meta->url : QUrl() : QFileInfo::exists(meta->albumArt) ?
                QUrl::fromLocalFile(meta->albumArt) : QUrl(meta->albumArt) : ficon;
    ContentServer::instance()->getMetaForImg(iconUrl, true); // create meta for albumArt

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
            url == meta->url ? Utils::cleanId(finalId) :
                               Utils::swapUrlInId(meta->url, finalId);
    qDebug() << "final id:" << finalId;
    auto item = new PlaylistItem(finalId, // id
                               name, // name
                               meta->url, // url
                               origUrl, // orig url
                               type, // type
                               meta->mime, // ctype
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
                               ytdl, // ytdl
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
    qDebug() << "setActiveId" << id;
    auto cookie = Utils::cookieFromId(id);
    auto meta = ContentServer::instance()->getMetaForId(id, false);

    const int len = m_list.length();
    bool active_found = false;
    bool new_active_found = false;
    for (int i = 0; i < len; ++i) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(i));
        bool new_active = meta && fi->cookie() == cookie;
        if (new_active)
            active_found = true;
        if (fi->active() != new_active) {
            fi->setActive(new_active);
            if(new_active) {
                setActiveItemIndex(i);
                new_active_found = true;
            }
            emit activeItemChanged();
        }
    }

    if (new_active_found)
        resetToBeActive();

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
    if (id.isEmpty()) {
        for (auto li : m_list) {
            auto fi = dynamic_cast<PlaylistItem*>(li);
            fi->setToBeActive(false);
        }
    } else {
        for (auto li : m_list) {
            auto fi = dynamic_cast<PlaylistItem*>(li);
            fi->setToBeActive(fi->id() == id);
        }
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

void PlaylistModel::clear(bool save, bool deleteItems)
{
    bool active_removed = false;
    if (m_activeItemIndex > -1) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(m_activeItemIndex));
        if (fi->active())
            active_removed = true;
    }

    if(rowCount() > 0) {
        if (deleteItems) {
            removeRows(0, rowCount());
        } else {
            removeRowsNoDeleteItems(0, rowCount());
        }
        emit itemsRemoved();
    }

    if (deleteItems) {
        setActiveItemIndex(-1);
        if (isRefreshable()) {
            m_refreshable_count = 0;
            emit refreshableChanged();
        }
    }

    if (save) // saving last play queue
        this->save();

    if (active_removed)
        emit activeItemChanged();

    doUpdate();
}

QString PlaylistModel::activeId() const
{
    if (m_activeItemIndex > -1) {
        auto fi = m_list.at(m_activeItemIndex);
        return fi->id();
    }

    return QString();
}

QString PlaylistModel::activeCookie() const
{
    if (m_activeItemIndex > -1) {
        auto fi = dynamic_cast<PlaylistItem*>(m_list.at(m_activeItemIndex));
        if (fi)
            return fi->cookie();
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
    qDebug() << "setActiveItemIndex:" << m_activeItemIndex << index;
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemIndexChanged();
    }
}

bool PlaylistModel::autoPlay()
{
    auto av = Services::instance()->avTransport;

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
                play(fi->id());
            }
            done = true;
        }
        fi->setPlay(false);
    }

    return done;
}

bool PlaylistModel::removeIndex(int index)
{
    if (index < 0)
        return false;

    auto fi = dynamic_cast<PlaylistItem*>(m_list.at(index));

    bool active_removed = false;
    if (fi->active())
        active_removed = true;

    bool refreshable = fi->refreshable();

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

        if (active_removed)
            emit activeItemChanged();

        if (refreshable) {
            m_refreshable_count--;
            if (m_refreshable_count == 0)
                emit refreshableChanged();
        }
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

bool PlaylistModel::play(const QString &id)
{
    auto dir = Directory::instance();
    auto av = Services::instance()->avTransport;

    if (m_list.isEmpty() || !dir->getInited() || !av->getInited() ||
            !find(id)) {
        qWarning() << "Cannot play";
        return false;
    }

    if (activeId() == id) {
        qDebug() << "Id is active id";
        if (av->getTransportState() != AVTransport::Playing) {
            av->setSpeed(1);
            av->play();
        }
        return true;
    }

    if (isBusy() || isRefreshing()) {
        qWarning() << "Cannot play";
        return false;
    }

    refreshAndSetContent(id, nextId(id), true);
    return true;
}

void PlaylistModel::togglePlay() {
    auto dir = Directory::instance();
    auto av = Services::instance()->avTransport;

    if (!dir->getInited() || !av->getInited()) {
        qWarning() << "Cannot play";
        return;
    }

    if (av->getTransportState() != AVTransport::Playing) {
        auto aid = activeId();
        if (aid.isEmpty() || av->getRelativeTimePosition() > 0) {
            av->setSpeed(1);
            av->play();
        } else {
            play(aid);
        }
    } else {
        if (av->getPauseSupported())
            av->pause();
        else
            av->stop();
    }
}

QList<ListItem*> PlaylistModel::handleRefreshWorker()
{
    QList<ListItem*> items;

    if (m_refresh_worker) {
        bool refreshed = false;
        //bool refreshable = isRefreshable();
        for (auto& item : m_refresh_worker->items) {
            const auto id = m_refresh_worker->origId(item);
            qDebug() << "Refresh done for:" << id;
            auto oldItem = dynamic_cast<PlaylistItem*>(find(id));
            if (oldItem) {
                auto newItem = dynamic_cast<PlaylistItem*>(item);
                if (newItem->id() == id) {
                    qDebug() << "No need to update item";
                    delete newItem;
                } else {
                    auto list = m_list;
                    for (int i = 0; i < list.size(); ++i) {
                        if (id == list.at(i)->id()) {
                            qDebug() << "Replace item with new id:" << i << newItem->id();
                            newItem->setToBeActive(oldItem->toBeActive());
                            list.replace(i, newItem);
                            if (m_activeItemIndex == i)
                                setActiveItemIndex(-1);
                            clear(false, false);
                            appendRows(list);
                            /*if (oldItem->refreshable() != newItem->refreshable()) {
                                if (newItem->refreshable())
                                    m_refreshable_count++;
                                else
                                    m_refreshable_count--;
                            }*/
                            delete oldItem;
                            items << newItem;
                            refreshed = true;
                        }
                    }
                }
            } else {
                qWarning() << "Orig item doesn't exist";
                delete item;
            }
        }

        if (refreshed) {
            save();
            emit itemsRefreshed();
        }

        /*if (refreshable != isRefreshable()) {
            emit refreshableChanged();
        }*/
    }

    return items;
}

void PlaylistModel::updateActiveId()
{
    qDebug() << "updateActiveId";
    auto av = Services::instance()->avTransport;
    setActiveUrl(av->getCurrentURI());
}

void PlaylistModel::doUpdateActiveId()
{
    qDebug() << "doUpdateActiveId";
    m_updateActiveTimer.start();
}

void PlaylistModel::cancelRefresh()
{
    if (!isRefreshing()) {
        return;
    }

    qDebug() << "Cancel refresh";
    m_refresh_worker->cancel();
    qDebug() << "Cancel refresh ended";
}

void PlaylistModel::cancelAdd()
{
    qDebug() << "Cancel add";
    m_add_worker->cancel();
    qDebug() << "Cancel add ended";
}

void PlaylistModel::refresh()
{
    QList<QUrl> ids;
    for (const auto item : m_list) {
        auto pitem = dynamic_cast<PlaylistItem*>(item);
        if (pitem && pitem->refreshable())
            ids << pitem->id();
    }

    if (ids.isEmpty()) {
        qWarning() << "No ids to refresh";
        return;
    }

    if (!m_refresh_mutex.tryLock()) {
        qDebug() << "Other refreshing is ongoing";
        return;
    }

    m_refresh_worker = std::unique_ptr<PlaylistWorker>(new PlaylistWorker(std::move(ids)));

    connect(m_refresh_worker.get(), &PlaylistWorker::finished, this, [this] {
        qDebug() << "refresh_worker finished";
        handleRefreshWorker();

        m_refresh_worker.reset();
        m_refresh_mutex.unlock();
        emit refreshingChanged();
    }, Qt::QueuedConnection);

    connect(m_refresh_worker.get(), &PlaylistWorker::progress, this, &PlaylistModel::progressUpdate);
    progressUpdate(0, 0);

    m_refresh_worker->start();

    emit refreshingChanged();

    qDebug() << "Refreshing started";
}

void PlaylistModel::refreshAndSetContent(const QString &id1, const QString &id2,
                                         bool toBeActive)
{
    if (!m_refresh_mutex.tryLock()) {
        qDebug() << "Other refreshing is ongoing";
        return;
    }

    qDebug() << "refreshAndSetContent id1,id2:" << id1 << id2;

    QList<QUrl> ids; // ids to refresh

    auto item1 = dynamic_cast<PlaylistItem*>(find(id1));
    if (item1) {
        if (item1->refreshable())
            ids << QUrl(id1);
        if (toBeActive)
            setToBeActiveId(id1);
    } else if (id1 == id2) {
        qWarning() << "Cannot find:" << id1;
        m_refresh_mutex.unlock();
        return;
    }

    auto item2 = item1;
    if (id1 != id2) {
        item2 = dynamic_cast<PlaylistItem*>(find(id2));
        if (item2 && item2->refreshable())
            ids << QUrl(id2);
    }

    if (!item1 && !item2) {
        qWarning() << "Cannot find both items:" << id1 << id2;
        m_refresh_mutex.unlock();
        return;
    }

    if (ids.isEmpty()) { // no ids to refresh
        auto av = Services::instance()->avTransport;
        av->setLocalContent(id1, id2);
        m_refresh_mutex.unlock();
    } else {
        m_refresh_worker = std::unique_ptr<PlaylistWorker>(
                    new PlaylistWorker(std::move(ids)));

        connect(m_refresh_worker.get(), &PlaylistWorker::finished, this, [this, id1, id2, toBeActive] {
            qDebug() << "refresh_worker finished";

            auto item1 = dynamic_cast<PlaylistItem*>(find(id1));
            auto item2 = id1 == id2 ? item1 : dynamic_cast<PlaylistItem*>(find(id2));

            bool clearActive = false;

            if (item1 || item2) {
                if (m_refresh_worker) {
                    auto newItems = handleRefreshWorker();

                    auto av = Services::instance()->avTransport;

                    if (newItems.size() == 2) { // both item refreshed
                        av->setLocalContent(newItems.at(0)->id(), newItems.at(1)->id());
                    } else if (newItems.size() == 1) {
                        if (m_refresh_worker->origId(newItems.at(0)) == id1)
                            av->setLocalContent(newItems.at(0)->id(), id2);
                        else if (m_refresh_worker->origId(newItems.at(0)) == id2)
                            av->setLocalContent(id1, newItems.at(0)->id());
                        else {
                            qWarning() << "Refreshed item doesn't match";
                            clearActive = true;
                        }
                    } else { // none items refreshed
                        av->setLocalContent(id1, id2);
                    }
                } else {
                    clearActive = true;
                }
            } else {
                clearActive = true;
            }

            if (clearActive && item1 && toBeActive) {
                setToBeActiveId("");
            }

            m_refresh_mutex.unlock();
            if (m_refresh_worker) {
                m_refresh_worker.reset();
                emit refreshingChanged();
            }
        }, Qt::QueuedConnection);

        connect(m_refresh_worker.get(), &PlaylistWorker::progress, this, &PlaylistModel::progressUpdate);
        progressUpdate(0, 0);

        m_refresh_worker->start();
        emit refreshingChanged();
    }
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
                           const QString &ctype,
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
                           bool ytdl,
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
    m_ctype(ctype),
    m_artist(artist),
    m_album(album),
    m_date(date),
    m_duration(duration),
    m_size(size),
    m_icon(icon),
    m_ytdl(ytdl),
    m_play(play),
    m_item_type(itemType),
    m_devid(devId),
    m_cookie(Utils::cookieFromId(id))
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
    names[ContentTypeRole] = "ctype";
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
    names[YtdlRole] = "ytdl";
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
    case ContentTypeRole:
        return ctype();
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
    case YtdlRole:
        return ytdl();
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
