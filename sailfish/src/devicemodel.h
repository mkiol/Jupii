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

#include "listmodel.h"

class DeviceItem : public ListItem
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole,
        TitleRole = Qt::DisplayRole,
        TypeRole,
        ModelRole,
        FavRole,
        IconRole,
        SupportedRole
    };

public:
    DeviceItem(QObject *parent = 0): ListItem(parent) {}
    explicit DeviceItem(const QString &id,
                      const QString &title,
                      const QString &type,
                      const QString &model,
                      const QUrl &icon,
                      const bool &supported,
                      QObject *parent = 0);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString type() const { return m_type; }
    inline QString model() const { return m_model; }
    inline QUrl icon() const { return m_icon; }
    inline bool supported() const { return m_supported; }
    bool isFav() const;

private:
    QString m_id;
    QString m_title;
    QString m_type;
    QString m_model;
    QUrl m_icon;
    bool m_supported;
};

class DeviceModel : public ListModel
{
    Q_OBJECT

public:
    explicit DeviceModel(QObject *parent = 0);
    ~DeviceModel();
    void clear();

public slots:
    void updateModel();
};

#endif // DEVICEMODEL_H
