/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QModelIndex>
#include <QUrl>
#include <QVariant>
#include <QThread>
#include <QPair>
#include <QVariantList>
#include <QTimer>
#include <memory>

#ifdef WIDGETS
#include <QIcon>
#include <QBrush>
#endif

#ifdef SAILFISH
#include <keepalive/backgroundactivity.h>
#endif

#include "contentserver.h"
#include "listmodel.h"

class PlaylistItem;
class PlaylistModel;

struct UrlItem {
    QUrl url;
    QUrl origUrl;
    QString name;
    QString author;
    QUrl icon;
    QString desc;
    QString app;
    bool play = false;
};

class PlaylistWorker :
        public QThread
{
    Q_OBJECT

friend class PlaylistModel;

public:
    QList<ListItem*> items;
    PlaylistWorker(const QList<UrlItem> &urls,
                   bool asAudio = false,
                   QObject *parent = nullptr);
    PlaylistWorker(QList<UrlItem> &&urls,
                   bool asAudio = false,
                   QObject *parent = nullptr);
    PlaylistWorker(QList<QUrl> &&ids, QObject *parent = nullptr);
    inline QString origId(ListItem* item) { return itemToOrigId.value(item).toString(); }
    ~PlaylistWorker();
    void cancel();

signals:
    void progress(int value, int total);

private:
    QList<UrlItem> urls;
    QList<QUrl> ids;
    QHash<ListItem*, QUrl> itemToOrigId;
    bool asAudio;
    bool urlIsId;
    void run();
};

class PlaylistModel :
        public ListModel
{
    Q_OBJECT
    Q_PROPERTY (int activeItemIndex READ getActiveItemIndex NOTIFY activeItemIndexChanged)
    Q_PROPERTY (int playMode READ getPlayMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY (bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY (bool refreshing READ isRefreshing NOTIFY refreshingChanged)
    Q_PROPERTY (bool nextSupported READ isNextSupported NOTIFY nextSupportedChanged)
    Q_PROPERTY (bool prevSupported READ isPrevSupported NOTIFY prevSupportedChanged)
    Q_PROPERTY (int progressValue READ getProgressValue NOTIFY progressChanged)
    Q_PROPERTY (int progressTotal READ getProgressTotal NOTIFY progressChanged)
    Q_PROPERTY (bool refreshable READ isRefreshable NOTIFY refreshableChanged)

friend class PlaylistWorker;

public:
    enum ErrorType {
        E_Unknown,
        E_FileExists,
        E_SomeItemsNotAdded,
        E_ItemNotAdded,
        E_AllItemsNotAdded
    };
    Q_ENUM(ErrorType)

    enum PlayMode {
        PM_Normal = 0,
        PM_RepeatAll,
        PM_RepeatOne
    };
    Q_ENUM(PlayMode)

    static PlaylistModel* instance(QObject *parent = nullptr);

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
    Q_INVOKABLE bool saveToFile(const QString& title);
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
    const PlaylistItem* getActiveItem() const;
    int getPlayMode() const;
    void setPlayMode(int value);
    bool isNextSupported();
    bool isPrevSupported();
    bool pathExists(const QString& path);
    bool playPath(const QString& path);
    bool urlExists(const QUrl& url);
    bool playUrl(const QUrl& url);
    std::pair<int,QString> getDidlList(int max = 0, const QString &didlId = QString());
    inline int getProgressValue() { return m_progressValue; }
    inline int getProgressTotal() { return m_progressTotal; }
    inline bool isRefreshable() { return m_refreshable_count > 0; }

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

public slots:
    void load();
    void addItemPaths(const QStringList& paths);
    void addItemPath(const QString& path,
                     const QString &name = QString(),
                     bool autoPlay = false);
    void addItemUrls(const QList<UrlItem>& urls);
    void addItemFileUrls(const QList<QUrl>& urls);
    void addItemUrls(const QVariantList& urls);
    void addItemUrl(const QUrl& url,
                    const QString& name = QString(),
                    const QUrl& origUrl = QUrl(),
                    const QString &author = QString(),
                    const QUrl& icon = QUrl(),
                    const QString& desc = QString(),
                    const QString& app = QString(),
                    bool autoPlay = false);
    void addItemPathsAsAudio(const QStringList& paths);
    void setActiveId(const QString &id);
    void setActiveUrl(const QUrl &url);
    void setToBeActiveIndex(int index);
    void setToBeActiveId(const QString& id);
    void resetToBeActive();
    void togglePlayMode();
    bool isBusy();
    bool isRefreshing();

private slots:
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
#ifdef SAILFISH
    void handleBackgroundActivityStateChange();
#endif

private:
    static PlaylistModel* m_instance;
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
    QMutex m_refresh_mutex;
    int m_progressValue = 0;
    int m_progressTotal = 0;
    int m_refreshable_count = 0;

    PlaylistModel(QObject *parent = nullptr);
    void addItems(const QList<QUrl>& urls, bool asAudio);
    void addItems(const QList<UrlItem> &urls, bool asAudio);
    void setActiveItemIndex(int index);
    bool addId(const QUrl& id);
    PlaylistItem* makeItem(const QUrl &id);
    void save();
    QByteArray makePlsData(const QString& name = QString());
    void setBusy(bool busy);
    void updateNextSupported();
    void updatePrevSupported();
    bool autoPlay();
    void refreshAndSetContent(const QString &id1, const QString &id2, bool toBeActive = false);
    QList<ListItem*> handleRefreshWorker();
    void doUpdate();
    void doUpdateActiveId();
#ifdef SAILFISH
    void updateBackgroundActivity();
#endif
};

class PlaylistItem :
        public ListItem
{
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
    PlaylistItem(QObject *parent = nullptr): ListItem(parent) {}
    explicit PlaylistItem(const QUrl &id,
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
#ifdef WIDGETS
                      const QIcon &icon,
#else
                      const QUrl &icon,
#endif
                      bool ytdl,
                      bool play, // auto play after adding
                      const QString &desc,
                      const QDateTime &recDate,
                      const QString &recUrl,
                      ContentServer::ItemType itemType,
                      const QString &devId,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    QString path() const;
    inline QString id() const { return m_id.toString(); }
    inline QString cookie() const { return m_cookie; }
    inline QString name() const { return m_name; }
    inline QUrl url() const { return m_url; }
    inline QUrl origUrl() const { return m_origUrl.isEmpty() ? m_url : m_origUrl; }
    inline ContentServer::Type type() const { return m_type; }
    inline QString ctype() const { return m_ctype; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QString date() const { return m_date; }
    inline int duration() const { return m_duration; }
    inline qint64 size() const { return m_size; }
#ifdef WIDGETS
    inline QIcon icon() const { return m_icon; }
#else
    inline QUrl icon() const { return m_icon; }
#endif
    inline bool ytdl() const { return m_ytdl; }
    inline bool active() const { return m_active; }
    inline bool toBeActive() const { return m_tobeactive; }
    inline bool play() const { return m_play; }
    inline QString desc() const { return m_desc; }
    inline QString recUrl() const { return m_recUrl; }
    inline ContentServer::ItemType itemType() const { return m_item_type; }
    inline QString devId() const { return m_devid; }
    void setActive(bool value);
    void setToBeActive(bool value);
    void setPlay(bool value);
    inline bool refreshable() const { return m_ytdl; }
    QString friendlyRecDate() const;
#ifdef WIDGETS
    QBrush foreground() const;
#endif

private:
    QUrl m_id;
    QString m_name;
    QUrl m_url;
    QUrl m_origUrl;
    ContentServer::Type m_type;
    QString m_ctype;
    QString m_artist;
    QString m_album;
    QString m_date;
    QString m_cookie;
    int m_duration = 0;
    qint64 m_size = 0;
#ifdef WIDGETS
    QIcon m_icon;
#else
    QUrl m_icon;
#endif
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

#endif // PLAYLISTMODEL_H
