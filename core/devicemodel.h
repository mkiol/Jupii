/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QList>
#include <QMap>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QModelIndex>
#include <QUrl>

#ifdef DESKTOP
#include <QIcon>
#include <QBrush>
#endif

#include "listmodel.h"

class DeviceItem : public ListItem
{
    Q_OBJECT
public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        TypeRole = Qt::ToolTipRole,
        IconRole = Qt::DecorationRole,
        ForegroundRole = Qt::ForegroundRole,
        IdRole = Qt::UserRole,
        ActiveRole,
        ModelRole,
        FavRole,
        SupportedRole
    };

public:
    DeviceItem(QObject *parent = nullptr): ListItem(parent) {}
    explicit DeviceItem(const QString &id,
                      const QString &title,
                      const QString &type,
                      const QString &model,
#ifdef DESKTOP
                      const QIcon &icon,
#else
                      const QUrl &icon,
#endif
                      bool supported,
                      bool active,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString type() const { return m_type; }
    inline QString model() const { return m_model; }
#ifdef DESKTOP
    inline QIcon icon() const { return m_icon; }
#else
    inline QUrl icon() const { return m_icon; }
#endif
    inline bool supported() const { return m_supported; }
    inline bool active() const { return m_active; }
    bool isFav() const;
    void setActive(bool value);
#ifdef DESKTOP
    void setIcon(const QIcon &icon);
    QBrush foreground() const;
#endif

private:
    QString m_id;
    QString m_title;
    QString m_type;
    QString m_model;
#ifdef DESKTOP
    QIcon m_icon;
#else
    QUrl m_icon;
#endif
    bool m_active = false;
    bool m_supported = false;
};

class DeviceModel : public ListModel
{
    Q_OBJECT

public:
    explicit DeviceModel(QObject *parent = nullptr);
    ~DeviceModel();
    void clear();

public slots:
    void updateModel();
    void setActiveIndex(int index);
};

#endif // DEVICEMODEL_H
