/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "playlistmodel.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTextStream>
#include <QTimer>
#include <QUrlQuery>
#include <utility>

#include "avtransport.h"
#include "bcapi.h"
#include "directory.h"
#include "dnscontentdeterminator.h"
#include "filemetadata.h"
#include "playlistparser.h"
#include "renderingcontrol.h"
#include "services.h"
#include "settings.h"
#include "soundcloudapi.h"
#include "utils.h"

void PlaylistWorker::cancel() {
    if (isRunning()) {
        qDebug() << "playlist worker cancel";
        requestInterruption();
        quit();
        wait();
        qDebug() << "playlist worker cancel ended";
    }
}

PlaylistWorker::~PlaylistWorker() { cancel(); }

void PlaylistWorker::run() {
    if (ids.isEmpty()) {
        QList<UrlItem> nurls;

        // check for playlists
        foreach (const auto &url, urls) {
            bool isPlaylist = false;

            if (url.url.isLocalFile()) {
                auto type =
                    ContentServer::getContentTypeByExtension(url.url.path());
                if (type == ContentServer::Type::Type_Playlist) {
                    isPlaylist = true;
                    auto path = url.url.toLocalFile();
                    qDebug() << "file is a playlist:" << path;

                    QFile f{path};
                    if (f.exists()) {
                        if (f.open(QIODevice::ReadOnly)) {
                            auto data = f.readAll();
                            f.close();
                            if (!data.isEmpty()) {
                                QFileInfo fi(f);
                                auto dir = fi.absoluteDir().path();
                                auto ptype =
                                    ContentServer::playlistTypeFromExtension(
                                        path);
                                auto items =
                                    ptype == ContentServer::PlaylistType::PLS
                                        ? parsePls(data, dir)
                                    : ptype == ContentServer::PlaylistType::XSPF
                                        ? parseXspf(data, dir)
                                        : parseM3u(data, dir);

                                if (items.isEmpty()) {
                                    qWarning() << "playlist doesn't contain "
                                                  "any valid items";
                                } else {
                                    foreach (const auto &item, items) {
                                        // TODO: Consider playlist item title as
                                        // well
                                        UrlItem ui;
                                        ui.url = item.url;
                                        nurls << ui;
                                    }
                                }
                            } else {
                                qWarning() << "playlist content is empty";
                            }
                        } else {
                            qWarning() << "cannot open file" << f.fileName()
                                       << f.errorString();
                        }
                    } else {
                        qWarning() << "file doesn't exist:" << f.fileName();
                    }
                }
            }

            if (!isPlaylist) nurls << url;
        }

        urls = nurls;

        foreach (const auto &purl, urls) {
            const auto &url = purl.url;
            const auto &name = purl.name.trimmed();
            const auto &author = purl.author.trimmed();
            const auto &album = purl.album.trimmed();
            const auto &icon = purl.icon;
            const auto &desc = purl.desc.trimmed();
            const auto &origUrl = purl.origUrl;
            const auto &app = purl.app;
            const int duration = purl.duration;
            bool play = purl.play;

            if (Utils::isUrlValid(url)) {
                QUrl id{url};
                QUrlQuery q{url};

                // cookie
                if (q.hasQueryItem(Utils::cookieKey))
                    q.removeQueryItem(Utils::cookieKey);
                q.addQueryItem(Utils::cookieKey,
                               Utils::instance()->randString(10));

                // origUrl
                bool origUrlExists =
                    !origUrl.isEmpty() && Utils::isUrlValid(origUrl);
                if (origUrlExists) {
                    if (q.hasQueryItem(Utils::origUrlKey))
                        q.removeQueryItem(Utils::origUrlKey);
                    q.addQueryItem(Utils::origUrlKey, origUrl.toString());
                }

                // name
                if (!name.isEmpty()) {
                    if (q.hasQueryItem(Utils::nameKey))
                        q.removeQueryItem(Utils::nameKey);
                    q.addQueryItem(Utils::nameKey, name);
                }

                // type
                if (type != ContentServer::Type::Type_Unknown) {
                    if (q.hasQueryItem(Utils::typeKey))
                        q.removeQueryItem(Utils::typeKey);
                    q.addQueryItem(Utils::typeKey,
                                   QString::number(static_cast<int>(type)));
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

                // album
                if (!album.isEmpty()) {
                    if (q.hasQueryItem(Utils::albumKey))
                        q.removeQueryItem(Utils::albumKey);
                    q.addQueryItem(Utils::albumKey, album);
                }

                // app
                if (!app.isEmpty()) {
                    if (q.hasQueryItem(Utils::appKey))
                        q.removeQueryItem(Utils::appKey);
                    q.addQueryItem(Utils::appKey, app);

                    // refresh url with ytdl
                    if (origUrlExists) {
                        if (q.hasQueryItem(Utils::ytdlKey))
                            q.removeQueryItem(Utils::ytdlKey);
                        q.addQueryItem(Utils::ytdlKey, QStringLiteral("true"));
                    }
                }

                // play
                if (play) {
                    if (q.hasQueryItem(Utils::playKey))
                        q.removeQueryItem(Utils::playKey);
                    q.addQueryItem(Utils::playKey, QStringLiteral("true"));
                }

                // duration
                if (duration > 0) {
                    if (q.hasQueryItem(Utils::durKey))
                        q.removeQueryItem(Utils::durKey);
                    q.addQueryItem(Utils::durKey, QString::number(duration));
                }

                id.setQuery(q);

                ids << id;
            }
        }
    } else {
        for (auto &id : ids) Utils::fixUrl(id);
    }

    auto *pl = PlaylistModel::instance();
    for (int i = 0; i < ids.size(); ++i) {
        if (isInterruptionRequested()) {
            qDebug() << "playlist worker interruption requested";
            return;
        }
        emit progress(i, ids.size());
        if (auto *item = pl->makeItem(ids.at(i)); item) {
            itemToOrigId.insert(item, ids.at(i));
            items << item;
        }
    }
}

PlaylistModel::PlaylistModel(QObject *parent)
    : ListModel{new PlaylistItem, parent} {
    auto *s = Settings::instance();
    auto *services = Services::instance();
    auto av = services->avTransport;
    auto *dir = Directory::instance();
    auto *cs = ContentServer::instance();

    m_playMode = s->getPlayMode();

    connect(av.get(), &AVTransport::currentURIChanged, this,
            &PlaylistModel::onAvCurrentURIChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::nextURIChanged, this,
            &PlaylistModel::onAvNextURIChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::trackEnded, this,
            &PlaylistModel::doOnTrackEnded, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::transportStateChanged, this,
            &PlaylistModel::onAvStateChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::initedChanged, this,
            &PlaylistModel::onAvInitedChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::busyChanged, this,
            &PlaylistModel::onAvInitedChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::transportActionsChanged, this,
            &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(av.get(), &AVTransport::relativeTimePositionChanged, this,
            &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsAdded, this,
            &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsLoaded, this,
            &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsRemoved, this,
            &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::playModeChanged, this,
            &PlaylistModel::onSupportedChanged, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsAdded, this,
            &PlaylistModel::onItemsAdded, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsLoaded, this,
            &PlaylistModel::onItemsLoaded, Qt::QueuedConnection);
    connect(this, &PlaylistModel::itemsRemoved, this,
            &PlaylistModel::onItemsRemoved, Qt::QueuedConnection);
    connect(dir, &Directory::initedChanged, this, &PlaylistModel::load,
            Qt::QueuedConnection);
    connect(cs, &ContentServer::fullHashesUpdated, this,
            &PlaylistModel::updateActiveId, Qt::QueuedConnection);

#ifdef SAILFISH
    m_backgroundActivity = new BackgroundActivity(this);
    m_backgroundActivity->setWakeupFrequency(BackgroundActivity::ThirtySeconds);
    connect(m_backgroundActivity, &BackgroundActivity::stateChanged, this,
            &PlaylistModel::handleBackgroundActivityStateChange);
#endif  // SAILFISH

    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(1000);
    connect(&m_updateTimer, &QTimer::timeout, this, &PlaylistModel::update,
            Qt::QueuedConnection);
    m_updateActiveTimer.setSingleShot(true);
    m_updateTimer.setInterval(1000);
    connect(&m_updateActiveTimer, &QTimer::timeout, this,
            &PlaylistModel::updateActiveId, Qt::QueuedConnection);
    m_trackEndedTimer.setSingleShot(true);
    m_trackEndedTimer.setInterval(1000);
    connect(&m_trackEndedTimer, &QTimer::timeout, this,
            &PlaylistModel::onTrackEnded, Qt::QueuedConnection);
    m_refreshTimer.setTimerType(Qt::VeryCoarseTimer);
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(refreshTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this,
            &PlaylistModel::handleRefreshTimer, Qt::QueuedConnection);
    connect(this, &PlaylistModel::refreshableChanged, this,
            &PlaylistModel::updateRefreshTimer, Qt::QueuedConnection);
}

#ifdef SAILFISH
void PlaylistModel::handleBackgroundActivityStateChange() {
    qDebug() << "Background activity state:" << m_backgroundActivity->state();
}
void PlaylistModel::updateBackgroundActivity() {
    auto av = Services::instance()->avTransport;
    bool playing =
        av->getInited() && av->getTransportState() == AVTransport::Playing;

    qDebug() << "updateBackgroundActivity:" << playing;

    if (playing) {
        if (!m_backgroundActivity->isRunning()) m_backgroundActivity->run();
    } else {
        if (m_backgroundActivity->isRunning()) m_backgroundActivity->stop();
    }
}
#endif  // SAILFISH

std::pair<int, QString> PlaylistModel::getDidlList(int max,
                                                   const QString &didlId) {
    std::pair<int, QString> ret;

    QTextStream m{&ret.second};
    m << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" ";
    m << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    m << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" ";
    m << "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">";

    auto *cs = ContentServer::instance();

    if (didlId.isEmpty()) {
        int count = 0;
        foreach (const auto li, m_list) {
            if (max > 0 && count > max) {
                break;
            }
            auto *fi = qobject_cast<PlaylistItem *>(li);
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

void PlaylistModel::onSupportedChanged() {
    updateNextSupported();
    updatePrevSupported();
}

void PlaylistModel::updatePrevSupported() {
    auto av = Services::instance()->avTransport;
    bool value =
        (av->getSeekSupported() &&
         av->getTransportState() == AVTransport::Playing &&
         av->getRelativeTimePosition() > 5) ||
        (m_playMode != PlaylistModel::PM_RepeatOne && m_activeItemIndex > -1);

    if (m_prevSupported != value) {
        m_prevSupported = value;
        emit prevSupportedChanged();
    }
}

void PlaylistModel::updateNextSupported() {
    bool value = m_playMode != PlaylistModel::PM_RepeatOne && rowCount() > 0;

    if (m_nextSupported != value) {
        m_nextSupported = value;
        emit nextSupportedChanged();
    }
}

void PlaylistModel::next() {
    if (rowCount() > 0) {
        if (Services::instance()->avTransport->updating() || isBusy() ||
            isRefreshing()) {
            qDebug() << "playlist busy/refreshing or av update is in progress, "
                        "next skipped";
            return;
        }

        if (activeId().isEmpty())
            refreshAndSetContent(firstId(), nextActiveId(), true);
        else
            refreshAndSetContent(nextActiveId(), "", true);
    } else {
        qWarning() << "playlist is empty so cannot do next";
    }
}

void PlaylistModel::prev() {
    auto av = Services::instance()->avTransport;
    bool seekable = av->getSeekSupported();

    if (rowCount() > 0) {
        if (av->updating() || isBusy() || isRefreshing()) {
            qDebug() << "playlist busy/refreshing or av update is in progress, "
                        "prev skipped";
            return;
        }

        const auto aid = activeId();

        if (aid.isEmpty()) {
            if (seekable) av->seek(0);
            return;
        }

        const auto pid = prevActiveId();

        if (pid.isEmpty()) {
            if (seekable) av->seek(0);
            return;
        }

        if (seekable && av->getRelativeTimePosition() > 5) {
            av->seek(0);
        } else {
            refreshAndSetContent(pid, aid, true);
        }
    } else {
        if (seekable) av->seek(0);
    }
}

void PlaylistModel::onAvCurrentURIChanged() {
    updateActiveId();
    doUpdate();
}

void PlaylistModel::onAvNextURIChanged() {
    if (getPlayMode() != PlaylistModel::PM_Normal) {
        return;
    }

    auto av = Services::instance()->avTransport;

    if (av->getNextURI().isEmpty() && !av->getCurrentURI().isEmpty()) {
        qDebug() << "avt switches to nextURI without currentURIChanged";
        updateActiveId();
        doUpdate();
    }
}

void PlaylistModel::doOnTrackEnded() {
    m_trackEndedTimer.start();
}

void PlaylistModel::onTrackEnded() {
    qDebug() << "on track ended";

    auto av = Services::instance()->avTransport;

    if (av->getInited()) {
        if (av->updating() || isBusy() || isRefreshing()) {
            qDebug() << "playlist busy/refreshing or av update is in progress, "
                        "so delaying track end handling";
            doOnTrackEnded();
            return;
        }

        if (!m_list.isEmpty() && (!av->getNextURISupported() ||
                                  m_playMode == PlaylistModel::PM_RepeatOne ||
                                  (m_playMode == PlaylistModel::PM_RepeatAll &&
                                   m_list.size() == 1))) {
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

void PlaylistModel::updateRefreshTimer() {
    if (isRefreshable()) {
        if (Services::instance()->avTransport->getTransportState() ==
            AVTransport::Playing) {
            m_refreshTimer.start();
        } else {
            m_refreshTimer.stop();
        }
    }
}

void PlaylistModel::onAvStateChanged() {
    if (Services::instance()->avTransport->getTransportState() ==
        AVTransport::Playing) {
        resetToBeActive();
    }
    updateRefreshTimer();
    doUpdate();
#ifdef SAILFISH
    updateBackgroundActivity();
#endif  // SAILFISH
}

void PlaylistModel::onAvInitedChanged() {
    auto av = Services::instance()->avTransport;
    bool inited = av->getInited();
    bool busy = av->getBusy();

    resetToBeActive();

    if (inited) {
        if (!busy) {
            updateActiveId();
            doUpdate();
        }
    }

#ifdef SAILFISH
    updateBackgroundActivity();
#endif
}

void PlaylistModel::onItemsAdded() {
    updateActiveId();
    doUpdate();
}

void PlaylistModel::onItemsLoaded() {
    updateActiveId();
    doUpdate();
}

void PlaylistModel::onItemsRemoved() { doUpdate(); }

void PlaylistModel::save() {
    qDebug() << "saving current playlist";

    QStringList p_ids;
    QStringList d_ids;

    foreach (const auto item, m_list) {
        p_ids << item->id();

        const auto *pitem = qobject_cast<PlaylistItem *>(item);
        if (pitem->itemType() == ContentServer::ItemType_Upnp &&
            !pitem->devId().isEmpty()) {
            d_ids << pitem->devId();
        }
    }

    auto *s = Settings::instance();
    s->setLastPlaylist(p_ids);
    s->addLastDevices(d_ids);
}

void PlaylistModel::doUpdate() {
    qDebug() << "doing update";
    m_updateTimer.start();
}

void PlaylistModel::update() {
    qDebug() << "update playlist";

    if (rowCount() > 0) {
        auto av = Services::instance()->avTransport;
        if (av->getInited()) {
            if (isRefreshing()) {
                qDebug() << "playlist refreshing, skipping update";
                return;
            }
            if (av->updating() || isBusy()) {
                qDebug() << "playlist busy or av update is in progress, so "
                            "delaying playlist update";
                doUpdate();
                return;
            }

            if (autoPlay()) {
                qDebug() << "auto playing item";
                return;
            }

            auto aid = activeId();
            if (!aid.isEmpty()) {
                bool updateCurrent =
                    Utils::cookieFromId(av->getCurrentId()) != activeCookie() &&
                    av->getTransportState() == AVTransport::Playing;
                refreshAndSetContent(updateCurrent ? aid : "", nextActiveId());
            }
        }
    }
}

void PlaylistModel::setBusy(bool busy) {
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool PlaylistModel::saveToFile(const QString &title) {
    if (m_list.isEmpty()) {
        qWarning() << "current playlist is empty";
        return false;
    }

    const auto dir = Settings::instance()->getPlaylistDir();

    if (dir.isEmpty()) return false;

    QString name{title.trimmed()};
    if (name.isEmpty()) {
        name = tr("Playlist");
    }

    const QString oname{name};

    QString path;
    for (int i = 0; i < 10; ++i) {
        name = oname + (i > 0 ? " " + QString::number(i) : "");
        path = dir + "/" + name + ".pls";
        if (!QFileInfo::exists(path)) break;
    }

    Utils::writeToFile(path, makePlsData(name));

    return true;
}

bool PlaylistModel::saveToUrl(const QUrl &path) {
    if (m_list.isEmpty()) {
        qWarning() << "current playlist is empty";
        return false;
    }

    Utils::writeToFile(path.toLocalFile(), makePlsData());

    return true;
}

QByteArray PlaylistModel::makePlsData(const QString &name) {
    QByteArray data;
    QTextStream sdata{&data};

    sdata << "[playlist]\n";
    if (!name.isEmpty()) sdata << "X-GNOME-Title=" << name << "\n";
    sdata << "NumberOfEntries=" << m_list.size() << "\n";

    int l = m_list.size();
    for (int i = 0; i < l; ++i) {
        const auto *pitem = qobject_cast<PlaylistItem *>(m_list.at(i));
        const auto url = Utils::bestUrlFromId(QUrl(pitem->id())).toString();
        auto name = Utils::nameFromId(pitem->id());
        if (name.isEmpty()) {
            if (pitem->artist().isEmpty())
                name = pitem->name();
            else
                name = pitem->artist() + " - " + pitem->name();
        }
        sdata << "File" << i + 1 << "=" << url << "\n";
        if (!name.isEmpty()) sdata << "Title" << i + 1 << "=" << name << "\n";
    }

    return data;
}

void PlaylistModel::load() {
    qDebug() << "playlist load";

    if (!Directory::instance()->getInited()) {
        qWarning() << "directory is not inited, skipping playlist load";
        return;
    }

    if (!m_list.isEmpty()) return;

    setBusy(true);

    auto ids = QUrl::fromStringList(Settings::instance()->getLastPlaylist());

    progressUpdate(0, 0);

    if (ids.isEmpty()) {
        setBusy(false);
        emit itemsLoaded();
        return;
    }

    m_add_worker = std::make_unique<PlaylistWorker>(std::move(ids));
    connect(m_add_worker.get(), &PlaylistWorker::finished, this,
            &PlaylistModel::addWorkerDone, Qt::QueuedConnection);
    connect(m_add_worker.get(), &PlaylistWorker::progress, this,
            &PlaylistModel::progressUpdate, Qt::QueuedConnection);
    m_add_worker->start();
}

int PlaylistModel::getPlayMode() const { return m_playMode; }

void PlaylistModel::setPlayMode(int value) {
    if (value != m_playMode) {
        m_playMode = value;
        Settings::instance()->setPlayMode(m_playMode);
        emit playModeChanged();
        doUpdate();
    }
}

void PlaylistModel::togglePlayMode() {
    // PM_Normal has some bug therefore will be disabled
    switch (m_playMode) {
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

int PlaylistModel::getActiveItemIndex() const { return m_activeItemIndex; }

const PlaylistItem *PlaylistModel::getActiveItem() const {
    return m_activeItemIndex > -1 && m_list.size() > m_activeItemIndex
               ? qobject_cast<PlaylistItem *>(m_list.at(m_activeItemIndex))
               : nullptr;
}

void PlaylistModel::addItemPaths(const QStringList &paths,
                                 ContentServer::Type type) {
    QList<UrlItem> urls;
    foreach (const auto &path, paths) {
        UrlItem ui;
        ui.url = QUrl::fromLocalFile(path);
        urls << ui;
    }
    addItems(urls, type);
}

void PlaylistModel::addItemFileUrls(const QList<QUrl> &urls,
                                    ContentServer::Type type) {
    QList<UrlItem> items;
    foreach (const auto &url, urls) {
        UrlItem ui;
        ui.url = url;
        items << ui;
    }
    addItems(items, type);
}

void PlaylistModel::addItemUrls(const QVariantList &urls,
                                ContentServer::Type type) {
    QList<UrlItem> items;

    foreach (const QVariant &item, urls) {
        if (item.canConvert(QMetaType::QVariantMap)) {
            auto m = item.toMap();
            UrlItem ui;
            ui.url = m.value("url").toUrl();
            Utils::fixUrl(ui.url);
            ui.name = m.value("name").toString();
            ui.author = m.value("author").toString();
            ui.album = m.value("album").toString();
            ui.icon = m.value("icon").toUrl();
            ui.desc = m.value("desc").toString();
            ui.origUrl = m.value("origUrl").toUrl();
            ui.app = m.value("app").toString();
            ui.duration = m.value("duration").toInt();
            items << ui;
        } else if (item.canConvert(QMetaType::QUrl)) {
            UrlItem ui;
            ui.url = item.toUrl();
            Utils::fixUrl(ui.url);
            items << ui;
        }
    }

    addItems(items, type);
}

PlaylistModel::UrlType PlaylistModel::determineUrlType(QUrl *url) {
    const auto host = url->host();
    const auto path = url->path();

    if (url->isLocalFile()) return UrlType::File;

    if (url->scheme().compare("jupii", Qt::CaseInsensitive) == 0)
        return UrlType::Jupii;

    auto cleanUrl = [url, &host]() {
        if (host.size() > 2 && host.startsWith("m.")) {
            url->setHost(host.mid(2));
        }
        url->setQuery(QString{});
        url->setFragment(QString{});
    };

    if (host.compare("soundcloud.com", Qt::CaseInsensitive) == 0) {
        if (path.isEmpty() || path == "/") {
            cleanUrl();
            return UrlType::SoundcloudMain;
        }
    }

    if (host.contains("soundcloud.com", Qt::CaseInsensitive)) {
        cleanUrl();
        auto ps = path.split('/');
        if (ps.size() == 3) {
            const auto &last = ps.last();
            if (last.compare("popular-tracks", Qt::CaseInsensitive) == 0 ||
                last.compare("tracks", Qt::CaseInsensitive) == 0 ||
                last.compare("albums", Qt::CaseInsensitive) == 0 ||
                last.compare("playlist", Qt::CaseInsensitive) == 0 ||
                last.compare("reposts", Qt::CaseInsensitive) == 0) {
                url->setPath("/" + ps.at(1));
                return UrlType::SoundcloudArtist;
            }
            return UrlType::SoundcloudTrack;
        }
        if (path.contains("/sets/", Qt::CaseInsensitive))
            return UrlType::SoundcloudAlbum;
        if (ps.size() == 2 && !ps.last().isEmpty())
            return UrlType::SoundcloudArtist;

        *url = QUrl{"https://soundcloud.com"};
        return UrlType::SoundcloudMain;
    }

    if (host.compare("bandcamp.com", Qt::CaseInsensitive) == 0) {
        cleanUrl();
        if (path.isEmpty() || path == "/") return UrlType::BcMain;
    }

    if (host.contains("bandcamp.com", Qt::CaseInsensitive) ||
        DnsDeterminator::type(*url) == DnsDeterminator::Type::Bc) {
        cleanUrl();
        auto ps = path.split('/');
        if (path.contains("/album/", Qt::CaseInsensitive))
            return UrlType::BcAlbum;
        if (path.contains("/track/", Qt::CaseInsensitive))
            return UrlType::BcTrack;

        if (path.isEmpty() || path == "/" ||
            path.contains("/music/", Qt::CaseInsensitive) ||
            path.contains("/merch/", Qt::CaseInsensitive) ||
            path.contains("/community/", Qt::CaseInsensitive) ||
            path.contains("/concerts/", Qt::CaseInsensitive)) {
            url->setPath("");
            return UrlType::BcArtist;
        }

        if (host.compare("bandcamp.com", Qt::CaseInsensitive) == 0) {
            *url = QUrl{"https://bandcamp.com"};
            return UrlType::BcMain;
        }

        url->setPath("");
        return UrlType::BcArtist;
    }

    return UrlType::Unknown;
}

void PlaylistModel::addItemUrlWithTypeCheck(
    bool doTypeCheck, QUrl &&url, ContentServer::Type type, const QString &name,
    const QUrl &origUrl, const QString &author, const QUrl &icon,
    const QString &desc, QString &&app, bool play) {
    if (app.isEmpty() && origUrl.isEmpty()) {
        auto urlType = determineUrlType(&url);
        if (urlType == UrlType::BcTrack) {
            qDebug() << "url type: BcTrack";
            app = "bc";
        } else if (urlType == UrlType::SoundcloudTrack) {
            qDebug() << "url type: SoundcloudTrack";
            app = "soundcloud";
        } else if (urlType == UrlType::BcMain) {
            qDebug() << "url type: BcMain";
            emit bcMainUrlAdded();
            return;
        } else if (urlType == UrlType::BcAlbum) {
            qDebug() << "url type: BcAlbum";
            emit bcAlbumUrlAdded(url);
            return;
        } else if (urlType == UrlType::BcArtist) {
            qDebug() << "url type: BcArtist";
            emit bcArtistUrlAdded(url);
            return;
        } else if (urlType == UrlType::SoundcloudMain) {
            qDebug() << "url type: SoundcloudMain";
            emit soundcloudMainUrlAdded();
            return;
        } else if (urlType == UrlType::SoundcloudAlbum) {
            qDebug() << "url type: SoundcloudAlbum";
            emit soundcloudAlbumUrlAdded(url);
            return;
        } else if (urlType == UrlType::SoundcloudArtist) {
            qDebug() << "url type: SoundcloudArtist";
            emit soundcloudArtistUrlAdded(url);
            return;
        } else if (doTypeCheck && urlType == UrlType::Unknown &&
                   type == ContentServer::Type::Type_Unknown) {
            qDebug() << "unknown type url";
            emit unknownTypeUrlAdded(url, name);
            return;
        }
    }

    QList<UrlItem> urls;
    UrlItem ui;
    ui.url = std::move(url);
    Utils::fixUrl(ui.url);
    ui.origUrl = origUrl;
    ui.name = name;
    ui.author = author;
    ui.icon = icon;
    ui.desc = desc;
    ui.app = std::move(app);
    ui.play = play;
    urls << ui;
    addItems(urls, type);
}

void PlaylistModel::addItemUrlSkipUrlDialog(QUrl url, ContentServer::Type type,
                                            const QString &name) {
    return addItemUrlWithTypeCheck(false, std::move(url), type, name, {}, {},
                                   {}, {}, {}, false);
}

void PlaylistModel::addItemUrl(QUrl url, ContentServer::Type type,
                               const QString &name, const QUrl &origUrl,
                               const QString &author, const QUrl &icon,
                               const QString &desc, QString app, bool play) {
    return addItemUrlWithTypeCheck(true, std::move(url), type, name, origUrl,
                                   author, icon, desc, std::move(app), play);
}

void PlaylistModel::addItemPath(const QString &path, ContentServer::Type type,
                                const QString &name, bool play) {
    QList<UrlItem> urls;
    UrlItem ui;
    ui.url = QUrl::fromLocalFile(path);
    ui.name = name;
    ui.play = play;
    urls << ui;
    addItems(urls, type);
}

bool PlaylistModel::pathExists(const QString &path) {
    foreach (const auto li, m_list) {
        if (qobject_cast<PlaylistItem *>(li)->path() == path) return true;
    }

    return false;
}

bool PlaylistModel::playPath(const QString &path) {
    foreach (const auto li, m_list) {
        const auto *fi = qobject_cast<PlaylistItem *>(li);
        if (fi->path() == path) {
            // path exists, so playing it
            play(fi->id());
            return true;
        }
    }

    return false;
}

bool PlaylistModel::urlExists(const QUrl &url) {
    foreach (const auto li, m_list) {
        const auto *fi = qobject_cast<PlaylistItem *>(li);
        if (fi->url() == url || fi->origUrl() == url) return true;
    }

    return false;
}

bool PlaylistModel::playUrl(const QUrl &url) {
    foreach (const auto li, m_list) {
        const auto fi = qobject_cast<PlaylistItem *>(li);
        if (fi->url() == url || fi->origUrl() == url) {
            // url exists, so playing it
            play(fi->id());
            return true;
        }
    }

    return false;
}

void PlaylistModel::addItems(const QList<UrlItem> &urls,
                             ContentServer::Type type) {
    if (urls.isEmpty() || isBusy()) {
        qWarning() << "cannot add items";
        return;
    }

    setBusy(true);

    m_add_worker = std::make_unique<PlaylistWorker>(urls, type);
    connect(m_add_worker.get(), &PlaylistWorker::finished, this,
            &PlaylistModel::addWorkerDone);
    connect(m_add_worker.get(), &PlaylistWorker::progress, this,
            &PlaylistModel::progressUpdate);
    progressUpdate(0, 0);
    m_add_worker->start();
}

void PlaylistModel::addItems(const QList<QUrl> &urls,
                             ContentServer::Type type) {
    if (urls.isEmpty()) return;

    QList<UrlItem> purls;
    foreach (const auto &url, urls) {
        UrlItem ui;
        ui.url = url;
        Utils::fixUrl(ui.url);
        purls << ui;
    }

    addItems(purls, type);
}

void PlaylistModel::progressUpdate(int value, int total) {
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

void PlaylistModel::addWorkerDone() {
    qDebug() << "add worker done";

    if (m_add_worker) {
        if (m_add_worker->items.length() != m_add_worker->ids.length()) {
            qWarning()
                << "some urls are invalid and cannot be added to the playlist";
            if (m_add_worker->ids.length() == 1)
                emit error(E_ItemNotAdded);
            else if (m_add_worker->items.length() == 0)
                emit error(E_AllItemsNotAdded);
            else
                emit error(E_SomeItemsNotAdded);
        }

        if (!m_add_worker->items.isEmpty()) {
            int old_refreshable = m_refreshable_count;
            foreach (const auto item, m_add_worker->items) {
                if (qobject_cast<PlaylistItem *>(item)->refreshable())
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
            qWarning() << "no items to add to playlist";
        }
    } else {
        qWarning() << "worker done signal but worker is null";
    }

    setBusy(false);
    progressUpdate(0, 0);
}

PlaylistItem *PlaylistModel::makeItem(const QUrl &id) {
    qDebug() << "making item:" << Utils::anonymizedId(id);

    int t = 0, duration = 0;
    QString name, cookie, author, album, app;
    QUrl ficon, origUrl;
    bool play = false, ytdl = false;
    if (!Utils::pathTypeNameCookieIconFromId(
            id, nullptr, &t, &name, &cookie, &ficon, nullptr, &author, &album,
            &origUrl, &ytdl, &play, &app, &duration) ||
        cookie.isEmpty()) {
        qWarning() << "invalid id";
        return nullptr;
    }

    if (play) qDebug() << "auto play is enabled";

    auto url = Utils::urlFromId(id);
    auto type = static_cast<ContentServer::Type>(t);

    ContentServer::ItemMeta *meta;
    if (!origUrl.isEmpty() && origUrl != url) {
        meta = ContentServer::instance()->getMeta(
            url, false, origUrl, /*app=*/{}, /*ytdl=*/false, /*img=*/false,
            /*refresh=*/false, /*type=*/type);
        if (meta && meta->expired()) {
            qDebug() << "meta exipred";
            meta = ContentServer::instance()->getMeta(
                url, true, origUrl, /*app=*/app, /*ytdl=*/ytdl, /*img=*/false,
                /*refresh=*/true, /*type=*/type);
        } else {
            meta = ContentServer::instance()->getMeta(
                url, true, origUrl, /*app=*/app, /*ytdl=*/ytdl, /*img=*/false,
                /*refresh=*/false, /*type=*/type);
        }
    } else {
        meta = ContentServer::instance()->getMeta(
            url, true, origUrl, /*app=*/app, /*ytdl=*/ytdl, /*img=*/false,
            /*refresh=*/false, /*type=*/type);
    }
    if (!meta) {
        qWarning() << "no meta found";
        return nullptr;
    }

    cookieToUrl.insert(cookie, meta->url);

    auto finalId = id;

    if (!ytdl) {
        if (meta->flagSet(ContentServer::MetaFlag::YtDl)) {  // add ytdl to url
            QUrlQuery q{finalId};
            if (q.hasQueryItem(Utils::ytdlKey))
                q.removeQueryItem(Utils::ytdlKey);
            q.addQueryItem(Utils::ytdlKey, QStringLiteral("true"));
            finalId.setQuery(q);
            ytdl = true;
        }
    }

    if (app.isEmpty()) {
        if (!meta->app.isEmpty()) {  // add app to url
            QUrlQuery q{finalId};
            if (q.hasQueryItem(Utils::appKey)) q.removeQueryItem(Utils::appKey);
            q.addQueryItem(Utils::appKey, meta->app);
            finalId.setQuery(q);
            app = meta->app;
        }
    }

    if (type == ContentServer::Type::Type_Unknown) {
        if (meta->flagSet(ContentServer::MetaFlag::YtDl)) {  // add type to url
                                                             // for ytdl content
            QUrlQuery q{finalId};
            if (q.hasQueryItem(Utils::typeKey))
                q.removeQueryItem(Utils::typeKey);
            q.addQueryItem(Utils::typeKey,
                           QString::number(static_cast<int>(meta->type)));
            finalId.setQuery(q);
        }
        type = meta->type;
    }

    if (name.isEmpty()) {
        name = ContentServer::bestName(*meta);
        if (meta->flagSet(
                ContentServer::MetaFlag::YtDl)) {  // add discovered name to url
                                                   // for ytdl content
            QUrlQuery q{finalId};
            if (q.hasQueryItem(Utils::nameKey))
                q.removeQueryItem(Utils::nameKey);
            q.addQueryItem(Utils::nameKey, name);
            finalId.setQuery(q);
        }
    } else {
        meta->title = name;
    }

    if (origUrl.isEmpty()) {
        if (!meta->origUrl.isEmpty() && meta->origUrl != meta->url) {
            QUrlQuery q{finalId};
            if (q.hasQueryItem(Utils::origUrlKey))
                q.removeQueryItem(Utils::origUrlKey);
            q.addQueryItem(Utils::origUrlKey, meta->origUrl.toString());
            finalId.setQuery(q);
            origUrl = meta->origUrl;
        }
    } else {
        meta->origUrl = origUrl;
    }

    QUrl iconUrl;
    if (ficon.isEmpty() ||
        meta->flagSet(ContentServer::MetaFlag::MadeFromCache)) {
        if (meta->flagSet(ContentServer::MetaFlag::NiceAlbumArt) &&
            !meta->albumArt.isEmpty()) {
            iconUrl = QUrl{meta->albumArt};

            // NiceAlbumArt => adding icon to url
            QUrlQuery q{finalId};
            q.addQueryItem(Utils::iconKey, meta->albumArt);
            finalId.setQuery(q);
        } else if (!meta->albumArt.isEmpty()) {
            auto artPath = ContentServer::localArtPathIfExists(meta->albumArt);
            if (artPath.isEmpty()) {
                iconUrl = type == ContentServer::Type::Type_Image
                              ? meta->url
                              : ContentServer::artUrl(meta->albumArt);
            } else {
                iconUrl = ContentServer::artUrl(artPath);
            }
        }
    } else {
        iconUrl = ficon;
    }

    if (!iconUrl.isEmpty()) {
        auto *imgMeta = ContentServer::instance()->getMetaForImg(iconUrl, true);
        if (imgMeta && !imgMeta->path.isEmpty()) {
            iconUrl = QUrl::fromLocalFile(imgMeta->path);
        }
    }

    if (ytdl && duration == 0 && meta->duration > 0) {
        QUrlQuery q{finalId};
        if (q.hasQueryItem(Utils::durKey)) q.removeQueryItem(Utils::durKey);
        q.addQueryItem(Utils::durKey, QString::number(meta->duration));
        finalId.setQuery(q);
    }

    if (duration == 0) {
        duration = meta->duration;
    }

    if (!author.isEmpty()) {
        meta->artist = author;
    }

    if (!album.isEmpty()) {
        meta->album = album;
    }

    // qDebug() << "meta:" << meta->title << meta->artist << meta->album;

    finalId = meta->itemType == ContentServer::ItemType_Upnp || url == meta->url
                  ? Utils::cleanId(finalId)
                  : Utils::swapUrlInId(meta->url, finalId);

    qDebug() << "final id:" << Utils::anonymizedId(finalId);

    return new PlaylistItem(finalId,       // id
                            name,          // name
                            meta->url,     // url
                            origUrl,       // orig url
                            type,          // type
                            meta->mime,    // ctype
                            meta->artist,  // artist
                            meta->album,   // album
                            {},            // date
                            duration, meta->size, iconUrl, ytdl, play,
                            meta->comment, meta->recDate, meta->recUrl,
                            meta->itemType, meta->upnpDevId);
}

bool PlaylistModel::addId(const QUrl &id) {
    if (auto *item = makeItem(id); item) {
        appendRow(item);
        return true;
    }

    return false;
}

void PlaylistModel::setActiveId(const QString &id) {
    qDebug() << "set active id:" << Utils::anonymizedId(id);
    const auto cookie = Utils::cookieFromId(id);
    auto metaExists =
        ContentServer::instance()->metaExists(Utils::urlFromId(id));
    if (!metaExists && cookieToUrl.contains(
                           cookie)) {  // edge case: url was updated by refresh
        metaExists =
            ContentServer::instance()->metaExists(cookieToUrl.value(cookie));
    }

    const auto len = m_list.length();
    bool active_found = false;
    for (int i = 0; i < len; ++i) {
        auto *fi = qobject_cast<PlaylistItem *>(m_list.at(i));
        const auto new_active = metaExists && fi->cookie() == cookie;
        if (new_active) active_found = true;
        if (fi->active() != new_active) {
            fi->setActive(new_active);
            if (new_active) setActiveItemIndex(i);
            emit activeItemChanged();
        }
    }

    resetToBeActive();

    if (!active_found) setActiveItemIndex(-1);
}

void PlaylistModel::resetToBeActive() {
    for (int i = 0; i < m_list.length(); ++i) {
        qobject_cast<PlaylistItem *>(m_list.at(i))->setToBeActive(false);
    }
}

void PlaylistModel::setToBeActiveIndex(int index) {
    for (int i = 0; i < m_list.length(); ++i) {
        qobject_cast<PlaylistItem *>(m_list.at(i))->setToBeActive(i == index);
    }
}

void PlaylistModel::setToBeActiveId(const QString &id) {
    if (id.isEmpty()) {
        foreach (auto li, m_list) {
            qobject_cast<PlaylistItem *>(li)->setToBeActive(false);
        }
    } else {
        foreach (auto li, m_list) {
            qobject_cast<PlaylistItem *>(li)->setToBeActive(li->id() == id);
        }
    }
}

void PlaylistModel::setActiveUrl(const QUrl &url) {
    if (url.isEmpty()) {
        qDebug() << "set empty active url";
        setActiveId({});
    } else {
        setActiveId(ContentServer::instance()->idFromUrl(url));
    }
}

void PlaylistModel::clear(bool save, bool deleteItems) {
    bool active_removed = false;
    if (m_activeItemIndex > -1) {
        if (qobject_cast<PlaylistItem *>(m_list.at(m_activeItemIndex))
                ->active())
            active_removed = true;
    }

    if (rowCount() > 0) {
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

    if (save)  // saving last play queue
        this->save();

    if (active_removed) emit activeItemChanged();

    doUpdate();
}

QString PlaylistModel::activeId() const {
    if (m_activeItemIndex > -1) {
        return m_list.at(m_activeItemIndex)->id();
    }

    return {};
}

QString PlaylistModel::activeCookie() const {
    if (m_activeItemIndex > -1) {
        if (const auto *fi =
                qobject_cast<PlaylistItem *>(m_list.at(m_activeItemIndex));
            fi)
            return fi->cookie();
    }

    return {};
}

QString PlaylistModel::firstId() const {
    if (m_list.length() > 0) return m_list.first()->id();

    return {};
}

QString PlaylistModel::secondId() const {
    if (m_list.length() > 1) return m_list.at(1)->id();

    return {};
}

void PlaylistModel::setActiveItemIndex(int index) {
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemIndexChanged();
    }
}

bool PlaylistModel::autoPlay() {
    auto &av = Services::instance()->avTransport;

    const auto ai = activeId();
    bool playing = static_cast<AVTransport::TransportState>(
                       av->getTransportState()) == AVTransport::Playing;

    // start playing fist item with play flag
    bool done = false;
    foreach (auto li, m_list) {
        auto *fi = qobject_cast<PlaylistItem *>(li);
        if (!done && fi->play()) {
            if (ai != fi->id() || !playing) {
                qDebug() << "playing item:" << fi->id();
                play(fi->id());
            }
            done = true;
        }
        fi->setPlay(false);
    }

    return done;
}

bool PlaylistModel::removeIndex(int index) {
    if (index < 0) return false;

    auto *fi = qobject_cast<PlaylistItem *>(m_list.at(index));

    bool active_removed = false;
    if (fi->active()) active_removed = true;

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

        if (active_removed) emit activeItemChanged();

        if (refreshable) {
            m_refreshable_count--;
            if (m_refreshable_count == 0) emit refreshableChanged();
        }
    }

    return ok;
}

bool PlaylistModel::remove(const QString &id) {
    return removeIndex(indexFromId(id));
}

QString PlaylistModel::nextActiveId() const {
    if (m_activeItemIndex < 0) return {};

    int l = m_list.length();
    if (l == 0) return {};

    if (m_playMode == PM_RepeatOne) return m_list.at(m_activeItemIndex)->id();

    int nextIndex = m_activeItemIndex + 1;

    if (nextIndex < l) return m_list.at(nextIndex)->id();
    if (m_playMode == PM_RepeatAll) return m_list.first()->id();

    return {};
}

std::optional<int> PlaylistModel::nextActiveIndex() const {
    if (m_activeItemIndex >= 0 && m_list.length() > 0) {
        if (m_playMode == PM_RepeatOne) return m_activeItemIndex;
        if (m_activeItemIndex + 1 < m_list.length())
            return m_activeItemIndex + 1;
        if (m_playMode == PM_RepeatAll) return 0;
    }

    return std::nullopt;
}

QString PlaylistModel::prevActiveId() const {
    if (m_activeItemIndex < 0) return {};

    const int l = m_list.length();
    if (l == 0) return {};

    if (m_playMode == PM_RepeatOne) return m_list.at(m_activeItemIndex)->id();

    int prevIndex = m_activeItemIndex - 1;

    if (prevIndex < 0) {
        if (m_playMode == PM_RepeatAll)
            prevIndex = l - 1;
        else
            return {};
    }

    return m_list.at(prevIndex)->id();
}

bool PlaylistModel::play(const QString &id) {
    auto *dir = Directory::instance();
    auto &av = Services::instance()->avTransport;

    if (m_list.isEmpty() || !dir->getInited() || !av->getInited() ||
        !find(id)) {
        qWarning() << "cannot play";
        return false;
    }

    if (activeId() == id &&
        av->getCurrentId().toString() ==
            id) {  // both checks are needed due to url change in refresh
        qDebug() << "id is active id";
        if (av->getTransportState() != AVTransport::Playing) {
            if (av->getCurrentTrackDuration() > 0 &&
                av->getRelativeTimePosition() > 0) {
                dir->xcPowerOn(av->getDeviceId());
                av->setSpeed(1);
                av->play();
            } else {
                av->stop();
            }
        } else {
            return true;
        }
    }

    if (isBusy() || isRefreshing()) {
        qWarning() << "cannot play";
        return false;
    }

    dir->xcPowerOn(av->getDeviceId());

    // delaying refreshAndSetContent for xcPowerOn finish
    QTimer::singleShot(
        200, this, [this, id] { refreshAndSetContent(id, nextId(id), true); });

    return true;
}

void PlaylistModel::togglePlay() {
    auto *dir = Directory::instance();
    auto av = Services::instance()->avTransport;

    if (!dir->getInited() || !av->getInited()) {
        qWarning() << "cannot play";
        return;
    }

    if (av->getTransportState() != AVTransport::Playing) {
        auto aid = activeId();
        if (aid.isEmpty() || (av->getCurrentTrackDuration() > 0 &&
                              av->getRelativeTimePosition() > 0)) {
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

void PlaylistModel::play() {
    auto *dir = Directory::instance();
    auto av = Services::instance()->avTransport;

    if (!dir->getInited() || !av->getInited()) {
        qWarning() << "cannot play";
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
    }
}

void PlaylistModel::pause() {
    auto *dir = Directory::instance();
    auto av = Services::instance()->avTransport;

    if (!dir->getInited() || !av->getInited()) {
        qWarning() << "cannot pause";
        return;
    }

    if (av->getTransportState() == AVTransport::Playing) {
        if (av->getPauseSupported())
            av->pause();
        else
            av->stop();
    }
}

QList<ListItem *> PlaylistModel::handleRefreshWorker() {
    QList<ListItem *> items;

    if (m_refresh_worker) {
        bool refreshed = false;
        decltype(m_list) tmpList;
        for (auto &item : m_refresh_worker->items) {
            const auto id = m_refresh_worker->origId(item);
            qDebug() << "refresh done for:" << Utils::anonymizedId(id);
            if (auto *oldItem = qobject_cast<PlaylistItem *>(find(id))) {
                auto *newItem = qobject_cast<PlaylistItem *>(item);
                if (newItem->id() == id) {
                    qDebug() << "no need to update item";
                    delete newItem;
                } else {
                    tmpList = m_list;
                    for (int i = 0; i < tmpList.size(); ++i) {
                        if (id == tmpList.at(i)->id()) {
                            qDebug() << "replace item with new id:" << i
                                     << Utils::anonymizedId(newItem->id());
                            newItem->setToBeActive(oldItem->toBeActive());
                            tmpList.replace(i, newItem);
                            if (m_activeItemIndex == i) setActiveItemIndex(-1);
                            clear(false, false);
                            appendRows(tmpList);
                            delete oldItem;
                            ;
                            items << newItem;
                            refreshed = true;
                            break;
                        }
                    }
                }
            } else {
                qWarning() << "orig item doesn't exist";
                delete item;
            }
        }

        if (refreshed) {
            save();
            emit itemsRefreshed();
        }
    }

    return items;
}

void PlaylistModel::updateActiveId() {
    setActiveUrl(QUrl{Services::instance()->avTransport->getCurrentURI()});
}

void PlaylistModel::doUpdateActiveId() { m_updateActiveTimer.start(); }

void PlaylistModel::cancelRefresh() {
    ContentServer::instance()->cancelCaching();

    if (!isRefreshing()) {
        return;
    }

    m_refresh_worker->cancel();
}

void PlaylistModel::cancelAdd() { m_add_worker->cancel(); }

void PlaylistModel::handleRefreshTimer() {
    if (!isRefreshing()) {
        if (auto idx = nextActiveIndex(); idx) {
            if (auto *item =
                    qobject_cast<PlaylistItem *>(m_list.at(idx.value()));
                item->refreshable()) {
                refreshAndSetContent({}, item->id(), false, false);
            }
        }
    }

    updateRefreshTimer();
}

void PlaylistModel::refresh(QList<QUrl> &&ids) {
    if (ids.isEmpty()) {
        qWarning() << "no ids to refresh";
        return;
    }

    if (!m_refresh_mutex.tryLock()) {
        qDebug() << "other refreshing is ongoing";
        return;
    }

    m_refresh_worker = std::make_unique<PlaylistWorker>(std::move(ids));

    connect(
        m_refresh_worker.get(), &PlaylistWorker::finished, this,
        [this] {
            handleRefreshWorker();
            m_refresh_worker.reset();
            m_refresh_mutex.unlock();
            emit refreshingChanged();
        },
        Qt::QueuedConnection);

    connect(m_refresh_worker.get(), &PlaylistWorker::progress, this,
            &PlaylistModel::progressUpdate);
    progressUpdate(0, 0);

    m_refresh_worker->start();

    emit refreshingChanged();

    qDebug() << "refreshing started";
}

void PlaylistModel::refresh() {
    QList<QUrl> ids;
    foreach (const auto item, m_list) {
        if (qobject_cast<PlaylistItem *>(item)->refreshable())
            ids << QUrl{item->id()};
    }

    refresh(std::move(ids));
}

void PlaylistModel::setContent(const QString &id1, const QString &id2) {
    auto cachableItem = [this](const QString &id) -> PlaylistItem * {
        if (id.isEmpty()) return nullptr;
        auto item = qobject_cast<PlaylistItem *>(find(id));
        if (item && item->itemType() == ContentServer::ItemType_Url) {
            return item;
        }
        return nullptr;
    };

    auto *item1 = cachableItem(id1);
    auto *item2 = cachableItem(id2);

    std::pair<ContentServer::CachingResult, ContentServer::CachingResult>
        result;

    if (item1 && item2)
        result = ContentServer::instance()->makeCache(QUrl{id1}, QUrl{id2});
    else if (item1)
        result = ContentServer::instance()->makeCache(QUrl{id1}, {});
    else if (item2)
        result = ContentServer::instance()->makeCache(QUrl{id2}, {});

    if (result.first == ContentServer::CachingResult::NotCachedCanceled ||
        result.second == ContentServer::CachingResult::NotCachedCanceled) {
        qWarning() << "item caching canceled";
        if (item1) item1->setToBeActive(false);
        return;
    }

    if (result.first == ContentServer::CachingResult::NotCachedError ||
        result.second == ContentServer::CachingResult::NotCachedError) {
        qWarning() << "item caching error";
        emit error(PlaylistModel::ErrorType::E_ProxyError);
        if (item1) item1->setToBeActive(false);
        return;
    }

    if (item1 && result.first == ContentServer::CachingResult::NotCached) {
        auto err = ContentServer::instance()->startProxy(QUrl{id1});
        if (err != ContentServer::ProxyError::NoError) {
            if (err == ContentServer::ProxyError::Canceled) {
                qWarning() << "proxy did not start => canceled";
            } else {
                qWarning() << "proxy did not start => error";
                emit error(PlaylistModel::ErrorType::E_ProxyError);
            }
            item1->setToBeActive(false);
            return;
        }
    }

    Services::instance()->avTransport->setLocalContent(id1, id2);
}

void PlaylistModel::refreshAndSetContent(const QString &id1, const QString &id2,
                                         bool toBeActive,
                                         bool setIfNotChanged) {
    if (!m_refresh_mutex.tryLock()) {
        qDebug() << "other refreshing is ongoing";
        return;
    }

    qDebug() << "refreshing:" << Utils::anonymizedId(id1)
             << Utils::anonymizedId(id2);

    QList<QUrl> idsToRefresh;

    auto *item1 = qobject_cast<PlaylistItem *>(find(id1));
    if (item1) {
        if (item1->itemType() == ContentServer::ItemType_LocalFile &&
            !QFileInfo::exists(item1->path())) {
            qWarning() << "first file doesn't exist:" << item1->path();
            m_refresh_mutex.unlock();
            return;
        }
        if (item1->refreshable()) idsToRefresh << QUrl{id1};
        if (toBeActive) setToBeActiveId(id1);
    } else if (id1 == id2) {
        qWarning() << "cannot find:" << Utils::anonymizedId(id1);
        m_refresh_mutex.unlock();
        return;
    }

    auto *item2 = item1;
    if (id1 != id2) {
        item2 = qobject_cast<PlaylistItem *>(find(id2));
        if (item2 && item2->itemType() == ContentServer::ItemType_LocalFile &&
            !QFileInfo::exists(item2->path())) {
            qWarning() << "second file doesn't exist:" << item2->path();
            item2 = nullptr;
        }
        if (item2 && item2->refreshable()) idsToRefresh << QUrl{id2};
    }

    if (!item1 && !item2) {
        qWarning() << "cannot find both items:" << Utils::anonymizedId(id1)
                   << Utils::anonymizedId(id2);
        m_refresh_mutex.unlock();
        return;
    }

    if (idsToRefresh.isEmpty()) {
        qDebug() << "no ids to refresh";
        if (setIfNotChanged) setContent(id1, id2);
        m_refresh_mutex.unlock();
    } else {
        m_refresh_worker =
            std::make_unique<PlaylistWorker>(std::move(idsToRefresh));

        connect(
            m_refresh_worker.get(), &PlaylistWorker::finished, this,
            [this, id1, id2, toBeActive, setIfNotChanged] {
                auto *item1 = qobject_cast<PlaylistItem *>(find(id1));
                auto *item2 = id1 == id2
                                  ? item1
                                  : qobject_cast<PlaylistItem *>(find(id2));

                bool clearActive = false;

                if (item1 || item2) {
                    if (m_refresh_worker) {
                        auto newItems = handleRefreshWorker();
                        if (newItems.size() == 2) {  // both item refreshed
                            setContent(newItems.at(0)->id(),
                                       newItems.at(1)->id());
                        } else if (newItems.size() == 1) {
                            if (id1 == id2)
                                setContent(newItems.at(0)->id(),
                                           newItems.at(0)->id());
                            else if (m_refresh_worker->origId(newItems.at(0)) ==
                                     id1)
                                setContent(newItems.at(0)->id(), id2);
                            else if (m_refresh_worker->origId(newItems.at(0)) ==
                                     id2)
                                setContent(id1, newItems.at(0)->id());
                            else {
                                qWarning() << "refreshed item doesn't match";
                                clearActive = true;
                            }
                        } else {  // none items refreshed
                            if (setIfNotChanged) setContent(id1, id2);
                        }
                    } else
                        clearActive = true;
                } else
                    clearActive = true;

                if (clearActive && item1 && toBeActive) setToBeActiveId({});

                m_refresh_mutex.unlock();
                if (m_refresh_worker) {
                    m_refresh_worker.reset();
                    emit refreshingChanged();
                }
            },
            Qt::QueuedConnection);

        connect(m_refresh_worker.get(), &PlaylistWorker::progress, this,
                &PlaylistModel::progressUpdate);
        progressUpdate(0, 0);

        m_refresh_worker->start();
        emit refreshingChanged();
    }
}

QString PlaylistModel::nextId(const QString &id) const {
    if (m_list.isEmpty()) return {};

    if (m_playMode == PM_RepeatOne) return id;

    bool nextFound = false;
    for (auto *li : m_list) {
        if (nextFound) return li->id();
        if (li->id() == id) nextFound = true;
    }

    if (nextFound && m_playMode == PM_RepeatAll) return m_list.first()->id();

    return {};
}

PlaylistItem::PlaylistItem(
    const QUrl &id, const QString &name, const QUrl &url, const QUrl &origUrl,
    ContentServer::Type type, const QString &ctype, const QString &artist,
    const QString &album, const QString &date, const int duration,
    const qint64 size, const QUrl &icon, bool ytdl, bool play,
    const QString &desc, const QDateTime &recDate, const QString &recUrl,
    ContentServer::ItemType itemType, const QString &devId, QObject *parent)
    : ListItem(parent),
      m_id(id),
      m_name(name),
      m_url(url),
      m_origUrl(origUrl),
      m_type(type),
      m_ctype(ctype),
      m_artist(artist),
      m_album(album),
      m_date(date),
      m_cookie(Utils::cookieFromId(id)),
      m_duration(duration),
      m_size(size),
      m_icon(icon),
      m_play(play),
      m_ytdl(ytdl),
      m_desc(desc),
      m_recDate(recDate),
      m_recUrl(recUrl),
      m_item_type(itemType),
      m_devid(devId) {}

QHash<int, QByteArray> PlaylistItem::roleNames() const {
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
    names[DescRole] = "desc";
    names[RecDateRole] = "recDate";
    names[RecUrlRole] = "recUrl";
    return names;
}

QString PlaylistItem::path() const {
    if (m_url.isLocalFile()) return m_url.toLocalFile();
    return {};
}

QVariant PlaylistItem::data(int role) const {
    switch (role) {
        case IdRole:
            return id();
        case NameRole:
            return name();
        case UrlRole:
            return url();
        case OrigUrlRole:
            return origUrl();
        case TypeRole:
            return static_cast<int>(type());
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
        case RecDateRole:
            return friendlyRecDate();
        case RecUrlRole:
            return recUrl();
        case DescRole:
            return desc();
        default:
            return {};
    }
}

QString PlaylistItem::friendlyRecDate() const {
    return Utils::friendlyDate(m_recDate);
}

void PlaylistItem::setActive(bool value) {
    setToBeActive(false);

    if (m_active != value) {
        m_active = value;
        emit dataChanged();
    }
}

void PlaylistItem::setToBeActive(bool value) {
    if (m_tobeactive != value) {
        m_tobeactive = value;
        emit dataChanged();
    }
}

void PlaylistItem::setPlay(bool value) { m_play = value; }

QString PlaylistItem::ctype() const {
    if (m_type == ContentServer::Type::Type_Music) {
        auto *meta = ContentServer::instance()->getMetaForId(m_id, false);
        if (meta && meta->audioAvMeta) {
            return meta->audioAvMeta->mime;
        }
    }
    return m_ctype;
}

bool PlaylistItem::refreshable() const {
    if (!m_ytdl) return false;
    auto *meta = ContentServer::instance()->getMetaForId(m_id, false);
    return !meta ||
           !static_cast<bool>(ContentServer::pathToCachedContent(*meta, true));
}
