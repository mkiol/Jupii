/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QList>
#include <QMap>
#include <QModelIndex>
#include <QMutex>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <memory>
#include <optional>

#ifdef SAILFISH
#include <keepalive/backgroundactivity.h>
#endif

#include "contentserver.h"
#include "listmodel.h"
#include "singleton.h"

class PlaylistItem;
class PlaylistModel;

struct UrlItem {
    QUrl url;
    QUrl origUrl;
    QString name;
    QString author;
    QString album;
    QUrl icon;
    QString desc;
    QString app;
    int duration = 0;
    bool play = false;
};

class PlaylistWorker : public QThread {
    Q_OBJECT

    friend class PlaylistModel;

   public:
    QList<ListItem *> items;

    template <typename Urls>
    PlaylistWorker(Urls &&urls, ContentServer::Type type,
                   QObject *parent = nullptr)
        : QThread{parent},
          urls(std::forward<Urls>(urls)),
          type{type},
          urlIsId{false} {}
    explicit PlaylistWorker(QList<QUrl> &&ids, QObject *parent = nullptr)
        : QThread{parent}, ids(std::move(ids)), urlIsId{true} {}
    inline QString origId(ListItem *item) {
        return itemToOrigId.value(item).toString();
    }
    ~PlaylistWorker();
    void cancel();

   signals:
    void progress(int value, int total);

   private:
    QList<UrlItem> urls;
    QList<QUrl> ids;
    QHash<ListItem *, QUrl> itemToOrigId;
    ContentServer::Type type = ContentServer::Type::Type_Unknown;
    bool urlIsId;
    void run() override;
};

class PlaylistModel : public ListModel, public Singleton<PlaylistModel> {
    Q_OBJECT
    Q_PROPERTY(int activeItemIndex READ getActiveItemIndex NOTIFY
                   activeItemIndexChanged)
    Q_PROPERTY(
        int playMode READ getPlayMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(bool refreshing READ isRefreshing NOTIFY refreshingChanged)
    Q_PROPERTY(
        bool nextSupported READ isNextSupported NOTIFY nextSupportedChanged)
    Q_PROPERTY(
        bool prevSupported READ isPrevSupported NOTIFY prevSupportedChanged)
    Q_PROPERTY(int progressValue READ getProgressValue NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ getProgressTotal NOTIFY progressChanged)
    Q_PROPERTY(bool refreshable READ isRefreshable NOTIFY refreshableChanged)

    friend class PlaylistWorker;

   public:
    enum ErrorType {
        E_Unknown,
        E_FileExists,
        E_SomeItemsNotAdded,
        E_ItemNotAdded,
        E_AllItemsNotAdded,
        E_ProxyError
    };
    Q_ENUM(ErrorType)

    enum PlayMode { PM_Normal = 0, PM_RepeatAll, PM_RepeatOne };
    Q_ENUM(PlayMode)

    PlaylistModel(QObject *parent = nullptr);
    Q_INVOKABLE void clear(bool save = true, bool deleteItems = true);
    Q_INVOKABLE QString firstId() const;
    Q_INVOKABLE QString secondId() const;
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool removeIndex(int index);
    Q_INVOKABLE QString activeId() const;
    QString activeCookie() const;
    Q_INVOKABLE QString nextActiveId() const;
    Q_INVOKABLE QString prevActiveId() const;
    Q_INVOKABLE QString nextId(const QString &id) const;
    Q_INVOKABLE bool saveToFile(const QString &title);
    Q_INVOKABLE bool saveToUrl(const QUrl &path);
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    Q_INVOKABLE bool play(const QString &id);
    void play();
    void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void cancelRefresh();
    Q_INVOKABLE void cancelAdd();

    int getActiveItemIndex() const;
    const PlaylistItem *getActiveItem() const;
    int getPlayMode() const;
    void setPlayMode(int value);
    inline auto isNextSupported() const { return m_nextSupported; }
    inline auto isPrevSupported() const { return m_prevSupported; }
    bool pathExists(const QString &path);
    bool playPath(const QString &path);
    bool urlExists(const QUrl &url);
    bool playUrl(const QUrl &url);
    std::pair<int, QString> getDidlList(int max = 0,
                                        const QString &didlId = {});
    inline int getProgressValue() const { return m_progressValue; }
    inline int getProgressTotal() const { return m_progressTotal; }
    inline bool isRefreshable() const { return m_refreshable_count > 0; }
    inline bool isBusy() const { return m_busy; }
    inline bool isRefreshing() const {
        return static_cast<bool>(m_refresh_worker);
    }

   signals:
    void itemsRemoved();
    void itemsAdded();
    void itemsLoaded();
    void itemsRefreshed();
    void error(PlaylistModel::ErrorType code);
    void activeItemChanged();
    void activeItemIndexChanged();
    void playModeChanged();
    void busyChanged();
    void refreshingChanged();
    void nextSupportedChanged();
    void prevSupportedChanged();
    void progressChanged();
    void refreshableChanged();
    void bcMainUrlAdded();
    void bcAlbumUrlAdded(const QUrl &url);
    void bcArtistUrlAdded(const QUrl &url);
    void soundcloudMainUrlAdded();
    void soundcloudAlbumUrlAdded(const QUrl &url);
    void soundcloudArtistUrlAdded(const QUrl &url);
    void unknownTypeUrlAdded(const QUrl &url, const QString &name);

   public slots:
    void load();
    void addItemPaths(
        const QStringList &paths,
        ContentServer::Type type = ContentServer::Type::Type_Unknown);
    void addItemPath(
        const QString &path,
        ContentServer::Type type = ContentServer::Type::Type_Unknown,
        const QString &name = {}, bool autoPlay = false);
    void addItemFileUrls(
        const QList<QUrl> &urls,
        ContentServer::Type type = ContentServer::Type::Type_Unknown);
    void addItemUrls(
        const QVariantList &urls,
        ContentServer::Type type = ContentServer::Type::Type_Unknown);
    void addItemUrl(
        QUrl url, ContentServer::Type type = ContentServer::Type::Type_Unknown,
        const QString &name = {}, const QUrl &origUrl = {},
        const QString &author = {}, const QUrl &icon = {},
        const QString &desc = {}, QString app = {}, bool autoPlay = false);
    void addItemUrlSkipUrlDialog(
        QUrl url, ContentServer::Type type = ContentServer::Type::Type_Unknown,
        const QString &name = {});
    void setActiveId(const QString &id);
    void setActiveUrl(const QUrl &url);
    void setToBeActiveIndex(int index);
    void setToBeActiveId(const QString &id);
    void resetToBeActive();
    void togglePlayMode();

   private:
    enum class UrlType {
        Unknown,
        BcMain,
        BcTrack,
        BcAlbum,
        BcArtist,
        SoundcloudMain,
        SoundcloudTrack,
        SoundcloudAlbum,
        SoundcloudArtist,
        File,
        Jupii
    };

    const static int refreshTimer = 30000;  // 30s
#ifdef SAILFISH
    BackgroundActivity *m_backgroundActivity;
#endif
    std::unique_ptr<PlaylistWorker> m_add_worker;
    std::unique_ptr<PlaylistWorker> m_refresh_worker;
    bool m_busy = false;
    int m_activeItemIndex = -1;
    int m_playMode;
    bool m_prevSupported = false;
    bool m_nextSupported = false;
    bool m_has_ytdl = false;
    QTimer m_updateTimer;
    QTimer m_updateActiveTimer;
    QTimer m_trackEndedTimer;
    QTimer m_refreshTimer;
    QMutex m_refresh_mutex;
    int m_progressValue = 0;
    int m_progressTotal = 0;
    int m_refreshable_count = 0;
    QHash<QString, QUrl> cookieToUrl;  // use for mapping: cookie => url => meta

    void addWorkerDone();
    void progressUpdate(int value, int total);
    void onItemsAdded();
    void onItemsLoaded();
    void onItemsRemoved();
    void onAvCurrentURIChanged();
    void onAvNextURIChanged();
    void onTrackEnded();
    void onAvStateChanged();
    void onAvInitedChanged();
    void onSupportedChanged();
    void update();
    void updateActiveId();
    void doOnTrackEnded();
    void handleRefreshTimer();
#ifdef SAILFISH
    void handleBackgroundActivityStateChange();
#endif
    void addItems(const QList<QUrl> &urls,
                  ContentServer::Type type = ContentServer::Type::Type_Unknown);
    void addItems(const QList<UrlItem> &urls,
                  ContentServer::Type type = ContentServer::Type::Type_Unknown);
    void setActiveItemIndex(int index);
    bool addId(const QUrl &id);
    PlaylistItem *makeItem(const QUrl &id);
    void save();
    QByteArray makePlsData(const QString &name = {});
    void setBusy(bool busy);
    void updateNextSupported();
    void updatePrevSupported();
    bool autoPlay();
    void refreshAndSetContent(const QString &id1, const QString &id2,
                              bool toBeActive = false,
                              bool setIfNotChanged = true);
    void setContent(const QString &id1, const QString &id2);
    QList<ListItem *> handleRefreshWorker();
    void doUpdate();
    void doUpdateActiveId();
    std::optional<int> nextActiveIndex() const;
    void refresh(QList<QUrl> &&ids);
    void updateRefreshTimer();
    static UrlType determineUrlType(QUrl *url);
    void addItemUrlWithTypeCheck(bool doTypeCheck, QUrl &&url,
                                 ContentServer::Type type, const QString &name,
                                 const QUrl &origUrl, const QString &author,
                                 const QUrl &icon, const QString &desc,
                                 QString &&app, bool autoPlay);
#ifdef SAILFISH
    void updateBackgroundActivity();
#endif
};

class PlaylistItem : public ListItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        ForegroundRole = Qt::ForegroundRole,
        IconRole = Qt::DecorationRole,
        IdRole = Qt::UserRole,
        ActiveRole,
        UrlRole,
        OrigUrlRole,
        DateRole,
        TypeRole,
        SizeRole,
        DurationRole,
        ContentTypeRole,
        ArtistRole,
        AlbumRole,
        ToBeActiveRole,
        ItemTypeRole,
        DevIdRole,
        YtdlRole,
        RecDateRole,
        RecUrlRole,
        DescRole
    };

   public:
    PlaylistItem(QObject *parent = nullptr) : ListItem{parent} {}
    PlaylistItem(const QUrl &id, const QString &name, const QUrl &url,
                 const QUrl &origUrl, ContentServer::Type type,
                 const QString &ctype, const QString &artist,
                 const QString &album, const QString &date, int duration,
                 qint64 size, const QUrl &icon, bool ytdl,
                 bool play,  // auto play after adding
                 const QString &desc, const QDateTime &recDate,
                 const QString &recUrl, ContentServer::ItemType itemType,
                 const QString &devId, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString path() const;
    inline QString id() const override { return m_id.toString(); }
    inline auto cookie() const { return m_cookie; }
    inline auto name() const { return m_name; }
    inline auto url() const { return m_url; }
    inline auto origUrl() const {
        return m_origUrl.isEmpty() ? m_url : m_origUrl;
    }
    inline auto type() const { return m_type; }
    QString ctype() const;
    inline auto artist() const { return m_artist; }
    inline auto album() const { return m_album; }
    inline auto date() const { return m_date; }
    inline auto duration() const { return m_duration; }
    inline auto size() const { return m_size; }
    inline auto icon() const { return m_icon; }
    inline auto ytdl() const { return m_ytdl; }
    inline auto active() const { return m_active; }
    inline auto toBeActive() const { return m_tobeactive; }
    inline auto play() const { return m_play; }
    inline auto desc() const { return m_desc; }
    inline auto recUrl() const { return m_recUrl; }
    inline auto itemType() const { return m_item_type; }
    inline auto devId() const { return m_devid; }
    void setActive(bool value);
    void setToBeActive(bool value);
    void setPlay(bool value);
    bool refreshable() const;
    QString friendlyRecDate() const;

   private:
    QUrl m_id;
    QString m_name;
    QUrl m_url;
    QUrl m_origUrl;
    ContentServer::Type m_type{};
    QString m_ctype;
    QString m_artist;
    QString m_album;
    QString m_date;
    QString m_cookie;
    int m_duration = 0;
    qint64 m_size = 0;
    QUrl m_icon;
    bool m_active = false;
    bool m_tobeactive = false;
    bool m_play = false;
    bool m_ytdl = false;
    QString m_desc;
    QDateTime m_recDate;
    QString m_recUrl;
    ContentServer::ItemType m_item_type = ContentServer::ItemType_Unknown;
    QString m_devid;
};

#endif  // PLAYLISTMODEL_H
