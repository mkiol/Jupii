/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>

#include <algorithm>

#include "devicemodel.h"
#include "directory.h"
#include "utils.h"
#include "settings.h"


DeviceModel::DeviceModel(QObject *parent) :
    ListModel(new DeviceItem, parent)
{
    QObject::connect(Directory::instance(),
                     &Directory::discoveryReady,
                     this,
                     &DeviceModel::updateModel);
}

DeviceModel::~DeviceModel()
{
}

void DeviceModel::updateModel()
{
    auto d = Directory::instance();
    auto s = Settings::instance();

    auto ddescs = d->getDeviceDescs();
    bool showAll = s->getShowAllDevices();

    clear();

    for (auto ddesc : ddescs) {
        /*bool supported = ddesc.deviceType == "urn:schemas-upnp-org:device:MediaRenderer:1" ||
                         ddesc.deviceType == "urn:schemas-upnp-org:device:MediaServer:1";*/
        bool supported = ddesc.deviceType == "urn:schemas-upnp-org:device:MediaRenderer:1";

        if (supported || showAll) {
            appendRow(new DeviceItem(QString::fromStdString(ddesc.UDN),
                                     QString::fromStdString(ddesc.friendlyName),
                                     QString::fromStdString(ddesc.deviceType),
                                     QString::fromStdString(ddesc.modelName),
                                     d->getDeviceIconUrl(ddesc),
                                     supported
                                     ));
        }
    }
}

void DeviceModel::clear()
{
    if(rowCount()>0) removeRows(0,rowCount());
}

DeviceItem::DeviceItem(const QString &id,
                   const QString &title,
                   const QString &type,
                   const QString &model,
                   const QUrl &icon,
                   const bool &supported,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_title(title),
    m_type(type),
    m_model(model),
    m_icon(icon),
    m_supported(supported)
{

    QObject::connect(Settings::instance(), &Settings::favDevicesChanged,
                     [this](){ emit dataChanged(); });
}

QHash<int, QByteArray> DeviceItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[TypeRole] = "type";
    names[ModelRole] = "modelName";
    names[FavRole] = "fav";
    names[IconRole] = "icon";
    names[SupportedRole] = "supported";
    return names;
}

QVariant DeviceItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case TitleRole:
        return title();
    case TypeRole:
        return type();
    case ModelRole:
        return model();
    case FavRole:
        return isFav();
    case IconRole:
        return icon();
    case SupportedRole:
        return supported();
    default:
        return QVariant();
    }
}

bool DeviceItem::isFav() const
{
    auto fd = Settings::instance()->getFavDevices();
    return fd.contains(m_id);
}
