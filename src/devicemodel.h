/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QList>
#include <QMap>
#include <QModelIndex>
#include <QString>
#include <QUrl>
#include <cstdint>

#include "listmodel.h"
#include "singleton.h"
#include "xc.h"

class DeviceItem : public ListItem {
    Q_OBJECT
   public:
    enum Flags { Upmpd = 1 << 1, Xc = 1 << 2, Supported = 1 << 3 };

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
        PowerRole,
        UpmpdRole
    };

    DeviceItem(QObject *parent = nullptr) : ListItem(parent) {}
    DeviceItem(const QString &id, const QString &title, const QString &type,
               const QString &model, const QUrl &icon, bool active,
               uint32_t flags, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto title() const { return m_title; }
    inline auto type() const { return m_type; }
    inline auto model() const { return m_model; }
    inline auto icon() const { return m_icon; }
    inline bool supported() const { return m_flags & Flags::Supported; }
    inline auto active() const { return m_active; }
    inline bool xc() const { return m_flags & Flags::Xc; }
    inline auto power() const { return m_power; }
    inline bool upmpd() const { return m_flags & Flags::Upmpd; }
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
    bool m_power = false;
    uint32_t m_flags = 0;
};

class DeviceModel : public ListModel, public Singleton<DeviceModel> {
    Q_OBJECT
    Q_PROPERTY(DeviceType deviceType READ getDeviceType WRITE setDeviceType
                   NOTIFY deviceTypeChanged)
   public:
    enum DeviceType { AnyType, MediaRendererType, MediaServerType };
    Q_ENUM(DeviceType)

    DeviceModel(QObject *parent = nullptr);
    void clear();
    void updatePower(const QString &id, bool value);
    inline auto getDeviceType() const { return m_deviceType; }
    void setDeviceType(DeviceType value);

   signals:
    void deviceTypeChanged();

   public slots:
    void updateModel();
    void serviceInitedHandler();

   private:
    DeviceType m_deviceType = AnyType;
    void handleXcError(XC::ErrorType code);
};

#endif  // DEVICEMODEL_H
