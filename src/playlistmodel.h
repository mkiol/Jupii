/* Copyright (C) 2017-2025 Michal Kosciesza <michal@mkiol.net>
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

#include "contentserver.h"
#include "itemmodel.h"
#include "listmodel.h"
#include "singleton.h"

#ifdef USE_SFOS
class BackgroundActivity;
#endif
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
    ~PlaylistWorker() override;
    void cancel();

   signals:
    void progress(int value, int total);

   private:
    QList<UrlItem> urls;
    QList<QUrl> ids;
    QHash<ListItem *, QUrl> itemToOrigId;
    ContentServer::Type type = ContentServer::Type::Type_Unknown;
    static void processPlaylist(const UrlItem &url, QList<UrlItem> &urls);
    bool urlIsId;
    void run() override;
};

class PlaylistModel : public ListModel, public Singleton<PlaylistModel> {
    Q_OBJECT
    Q_PROPERTY(int activeItemIndex READ getActiveItemIndex NOTIFY
                   activeItemIndexChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeItemIndexChanged)
    Q_PROPERTY(bool live READ isLive NOTIFY activeItemIndexChanged)
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
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

    friend class PlaylistWorker;

   public:
    enum ErrorType {
        E_Unknown,
        E_FileExists,
        E_SomeItemsNotAdded,
        E_ItemNotAdded,
        E_AllItemsNotAdded,
        E_ProxyError,
        E_CasterError,
        E_CasterError_NoFiles
    };
    Q_ENUM(ErrorType)

    enum PlayMode { PM_Normal = 0, PM_RepeatAll, PM_RepeatOne };
    Q_ENUM(PlayMode)

    PlaylistModel(QObject *parent = nullptr);
    void clear(bool save = true, bool deleteItems = true);
    Q_INVOKABLE QString firstId() const;
    Q_INVOKABLE QString secondId() const;
    Q_INVOKABLE void removeSelectedItems();
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool removeIndex(int index);
    Q_INVOKABLE QString activeId() const;
    QString activeCookie() const;
    Q_INVOKABLE QString nextActiveId() const;
    Q_INVOKABLE QString prevActiveId() const;
    Q_INVOKABLE QString nextId(const QString &id) const;
    Q_INVOKABLE void saveSelectedToFile(const QString &title);
    Q_INVOKABLE void saveSelectedToUrl(const QUrl &path);
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
    inline bool isActive() const { return m_activeItemIndex > -1; }
    bool isLive() const;
    int selectedCount() const;
    Q_INVOKABLE void setSelected(int index, bool value);
    Q_INVOKABLE void setAllSelected(bool value);
    Q_INVOKABLE void clearSelection();

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
    void selectedCountChanged();

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
#ifdef USE_SFOS
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
    int m_selectedCount = 0;

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
#ifdef USE_SFOS
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
    QByteArray makePlsDataFromSelectedItems(const QString &name = {}) const;
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
    PlaylistItem *itemFromId(const QString &id) const;
    void casterErrorHandler(ContentServer::CasterError error);
    QStringList selectedItems() const;
#ifdef USE_SFOS
    void updateBackgroundActivity();
#endif
};

class PlaylistItem : public SelectableItem {
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
        DescRole,
        LiveRole,
        VideoSourceRole,
        AudioSourceRole,
        VideoOrientationRole,
        AudioSourceMutedRole,
        SelectedRole
    };

   public:
    explicit PlaylistItem(QObject *parent = nullptr) : SelectableItem{parent} {}
    PlaylistItem(const QUrl &id, const QString &name, const QUrl &url,
                 const QUrl &origUrl, ContentServer::Type type,
                 const QString &ctype, const QString &artist,
                 const QString &album, const QString &date, int duration,
                 qint64 size, const QUrl &icon, bool ytdl, bool live,
                 bool play,  // auto play after adding
                 const QString &desc, const QDateTime &recDate,
                 const QString &recUrl, ContentServer::ItemType itemType,
                 const QString &devId, const QString &videoSource,
                 const QString &audioSource, const QString &videoOrientation,
                 bool audioSourceMuted, QObject *parent = nullptr);
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
    inline auto live() const { return m_live; }
    inline auto active() const { return m_active; }
    inline auto toBeActive() const { return m_tobeactive; }
    inline auto play() const { return m_play; }
    inline auto desc() const { return m_desc; }
    inline auto recUrl() const { return m_recUrl; }
    inline auto itemType() const { return m_item_type; }
    inline auto devId() const { return m_devid; }
    inline auto videoSource() const { return m_videoSource; }
    inline auto audioSource() const { return m_audioSource; }
    inline auto videoOrientation() const { return m_videoOrientation; }
    inline auto audioSourceMuted() const { return m_audioSourceMuted; }
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
    bool m_live = false;
    QString m_desc;
    QDateTime m_recDate;
    QString m_recUrl;
    ContentServer::ItemType m_item_type = ContentServer::ItemType_Unknown;
    QString m_devid;
    QString m_videoSource;
    QString m_audioSource;
    QString m_videoOrientation;
    bool m_audioSourceMuted = false;
};

#endif  // PLAYLISTMODEL_H
