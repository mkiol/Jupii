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
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QModelIndex>
#include <QUrl>
#include <QFileInfo>
#include <QVariant>
#include <QThread>
#include <QPair>
#include <memory>

#ifdef DESKTOP
#include <QIcon>
#include <QBrush>
#endif

#include "contentserver.h"
#include "listmodel.h"

class PlaylistModel;

struct UrlItem {
    QUrl url;
    QString name;
    QUrl icon;
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
        DateRole,
        TypeRole,
        SizeRole,
        DurationRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        ToBeActiveRole
    };

public:
    PlaylistItem(QObject *parent = nullptr): ListItem(parent) {}
    explicit PlaylistItem(const QUrl &id,
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
                      bool active = false,
                      bool toBeActive = false,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    QString path() const;
    inline QString id() const { return m_id.toString(); }
    //inline QUrl idUrl() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QUrl url() const { return m_url; }
    inline ContentServer::Type type() const { return m_type; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QString date() const { return m_date; }
    inline int duration() const { return m_duration; }
    inline qint64 size() const { return m_size; }
#ifdef SAILFISH
    inline QUrl icon() const { return m_icon; }
#else
    inline QIcon icon() const { return m_icon; }
#endif
    inline bool active() const { return m_active; }
    inline bool toBeActive() const { return m_tobeactive; }
    void setActive(bool value);
    void setToBeActive(bool value);
#ifdef DESKTOP
    QBrush foreground() const;
#endif

private:
    QUrl m_id;
    QString m_name;
    QUrl m_url;
    ContentServer::Type m_type;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_date;
    int m_duration;
    qint64 m_size;
#ifdef SAILFISH
    QUrl m_icon;
#else
    QIcon m_icon;
#endif
    bool m_active;
    bool m_tobeactive;
};

class PlaylistWorker :
        public QThread
{
    Q_OBJECT

friend class PlaylistModel;

public:
    QList<ListItem*> items;
    PlaylistWorker(const QList<UrlItem> &&urls,
                   bool asAudio = false,
                   bool urlIsId = false,
                   QObject *parent = nullptr);
private:
    QList<UrlItem> urls;
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
    Q_PROPERTY (bool nextSupported READ isNextSupported NOTIFY nextSupportedChanged)
    Q_PROPERTY (bool prevSupported READ isPrevSupported NOTIFY prevSupportedChanged)

friend class PlaylistWorker;

public:
    enum ErrorType {
        E_Unknown,
        E_FileExists
    };
    Q_ENUM(ErrorType)

    enum PlayMode {
        PM_Normal,
        PM_RepeatAll,
        PM_RepeatOne
    };
    Q_ENUM(PlayMode)

    static PlaylistModel* instance(QObject *parent = nullptr);

    Q_INVOKABLE void clear();
    Q_INVOKABLE QString firstId() const;
    Q_INVOKABLE QString secondId() const;
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool removeIndex(int index);
    Q_INVOKABLE QString activeId() const;
    Q_INVOKABLE QString nextActiveId() const;
    Q_INVOKABLE QString prevActiveId() const;
    Q_INVOKABLE QString nextId(const QString &id) const;
    Q_INVOKABLE void load();
    Q_INVOKABLE bool saveToFile(const QString& title);
    Q_INVOKABLE void update(bool play = false);
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    int getActiveItemIndex() const;
    int getPlayMode() const;
    void setPlayMode(int value);
    bool isNextSupported();
    bool isPrevSupported();

signals:
    void itemsRemoved();
    void itemsAdded();
    void itemsLoaded();
    void error(ErrorType code);
    void activeItemChanged();
    void activeItemIndexChanged();
    void playModeChanged();
    void busyChanged();
    void nextSupportedChanged();
    void prevSupportedChanged();

public slots:
    void addItemPaths(const QStringList& paths);
    void addItemUrls(const QList<QUrl>& urls);
    void addItemUrls(const QList<UrlItem>& urls);
    void addItemUrl(const QUrl& url,
                    const QString& name = QString(),
                    const QUrl& icon = QUrl());
    void addItemPathsAsAudio(const QStringList& paths);
    void setActiveId(const QString &id);
    void setActiveUrl(const QUrl &url);
    void setToBeActiveIndex(int index);
    void setToBeActiveId(const QString& id);
    void resetToBeActive();
    void togglePlayMode();
    bool isBusy();

private slots:
    void workerDone();
    void onItemsAdded();
    void onItemsLoaded();
    void onItemsRemoved();
    void onAvCurrentURIChanged();
    void onAvNextURIChanged();
    void onAvTrackEnded();
    void onAvStateChanged();
    void onAvInitedChanged();
    void onSupportedChanged();

private:
    static PlaylistModel* m_instance;

    std::unique_ptr<PlaylistWorker> m_worker;
    bool m_busy = false;
    int m_activeItemIndex = -1;
    int m_playMode = PM_RepeatAll;
    bool m_prevSupported = false;
    bool m_nextSupported = false;

    PlaylistModel(QObject *parent = nullptr);
    void addItems(const QList<QUrl>& urls, bool asAudio);
    void addItems(const QList<UrlItem> &urls, bool asAudio);
    void setActiveItemIndex(int index);
    //bool addId(const QString& id, ContentServer::Type type = ContentServer::TypeUnknown);
    bool addId(const QUrl& id);
    PlaylistItem* makeItem(const QUrl &id);
    void save();
    QByteArray makePlsData(const QString& name);
    void setBusy(bool busy);
    void updateNextSupported();
    void updatePrevSupported();
};

#endif // PLAYLISTMODEL_H
