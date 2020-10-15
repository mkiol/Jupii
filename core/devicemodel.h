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
        TitleRole = Qt::DisplayRole,
        TypeRole = Qt::ToolTipRole,
        IconRole = Qt::DecorationRole,
        ForegroundRole = Qt::ForegroundRole,
        IdRole = Qt::UserRole,
        ActiveRole,
        ModelRole,
        FavRole,
        SupportedRole,
        XcRole,
        PowerRole
    };

    DeviceItem(QObject *parent = nullptr): ListItem(parent) {}
    explicit DeviceItem(const QString &id,
                      const QString &title,
                      const QString &type,
                      const QString &model,
                      const QUrl &icon,
                      bool supported,
                      bool active,
                      bool jxc,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString type() const { return m_type; }
    inline QString model() const { return m_model; }
    inline QUrl icon() const { return m_icon; }
    inline bool supported() const { return m_supported; }
    inline bool active() const { return m_active; }
    inline bool xc() const { return m_xc; }
    inline bool power() const { return m_power; }
    bool isFav() const;
    void setActive(bool value);
    void setPower(bool value);
    void setIcon(const QUrl &icon);

private:
    QString m_id;
    QString m_title;
    QString m_type;
    QString m_model;
    QUrl m_icon;
    bool m_active = false;
    bool m_supported = false;
    bool m_xc = false;
    bool m_power = false;
};

class DeviceModel : public ListModel
{
    Q_OBJECT
    Q_PROPERTY (DeviceType deviceType READ getDeviceType WRITE setDeviceType NOTIFY deviceTypeChanged)
public:
    enum DeviceType {
        AnyType,
        MediaRendererType,
        MediaServerType
    };
    Q_ENUM(DeviceType)

    static DeviceModel* instance(QObject *parent = nullptr);
    void clear();
    void updatePower(const QString &id, bool value);
    DeviceType getDeviceType();
    void setDeviceType(DeviceType value);

signals:
    void deviceTypeChanged();

public slots:
    void updateModel();
    void serviceInitedHandler();

private:
    static DeviceModel* m_instance;
    DeviceType m_deviceType = AnyType;
    explicit DeviceModel(QObject *parent = nullptr);
};

#endif // DEVICEMODEL_H
