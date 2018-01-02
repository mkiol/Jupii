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

#include "contentserver.h"
#include "listmodel.h"

class FileItem : public ListItem
{
    Q_OBJECT
public:
    enum Roles {
        IdRole,
        NameRole,
        PathRole,
        DateRole,
        TypeRole,
        SizeRole,
        TitleRole,
        IconRole,
        ActiveRole
    };

public:
    FileItem(QObject *parent = 0): ListItem(parent) {}
    explicit FileItem(const QString &id,
                      const QString &name,
                      const QString &path,
                      const QString &date,
                      ContentServer::Type type,
                      const qint64 size,
                      const QString &title,
                      const QString &icon,
                      bool active = false,
                      QObject *parent = 0);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString path() const { return m_path; }
    inline QString date() const { return m_date; }
    inline ContentServer::Type type() const { return m_type; }
    inline qint64 size() const { return m_size; }
    inline QString title() const { return m_title; }
    inline QString icon() const { return m_icon; }
    inline bool active() const { return m_active; }
    void setActive(bool value);

private:
    QString m_id;
    QString m_name;
    QString m_path;
    QString m_date;
    ContentServer::Type m_type;
    qint64 m_size;
    QString m_title;
    QString m_icon;
    bool m_active;
};

class PlayListModel : public ListModel
{
    Q_OBJECT
    Q_PROPERTY (int activeItemIndex READ getActiveItemIndex NOTIFY activeItemChanged)

public:
    enum ErrorType {
        E_Unknown,
        E_FileExists
    };

    Q_ENUM(ErrorType)

    explicit PlayListModel(QObject *parent = 0);
    ~PlayListModel();

    Q_INVOKABLE void clear();
    Q_INVOKABLE QString firstId() const;
    Q_INVOKABLE QString secondId() const;
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE QString activeId() const;
    Q_INVOKABLE QString nextActiveId() const;
    Q_INVOKABLE QString prevActiveId() const;
    Q_INVOKABLE QString nextId(const QString &id) const;
    Q_INVOKABLE void load();
    int getActiveItemIndex() const;

signals:
    void itemsAdded(const QStringList& ids);
    void itemsLoaded(const QStringList& ids);
    void error(ErrorType code);
    int activeItemChanged();

public slots:
    void addItems(const QStringList& paths);
    void setActiveId(const QString &id);
    void setActiveUrl(const QUrl &url);

private:
    int m_activeItemIndex = -1;
    void setActiveItemIndex(int index);
    bool addId(const QString& id);
    void save();
};

#endif // PLAYLISTMODEL_H
