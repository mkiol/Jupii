/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>

#include <algorithm>
#include <memory>
#include <utility>

#include "devicemodel.h"
#include "directory.h"
#include "utils.h"
#include "settings.h"
#include "services.h"

#ifdef DESKTOP
#include <QPixmap>
#include <QImage>
#include <QPalette>
#include <QApplication>
#include "filedownloader.h"
const int icon_size = 64;
#endif

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
    auto av = Services::instance()->avTransport;

    auto ddescs = d->getDeviceDescs();
    bool showAll = s->getShowAllDevices();

    clear();

    for (auto ddesc : ddescs) {
        /*bool supported = ddesc.deviceType == "urn:schemas-upnp-org:device:MediaRenderer:1" ||
                         ddesc.deviceType == "urn:schemas-upnp-org:device:MediaServer:1";*/
        bool supported = ddesc.deviceType == "urn:schemas-upnp-org:device:MediaRenderer:1";

        if (supported || showAll) {
            QString id = QString::fromStdString(ddesc.UDN);
            QUrl iconUrl = d->getDeviceIconUrl(ddesc);
            bool active = av && av->getDeviceId() == id;

            appendRow(new DeviceItem(id,
                                     QString::fromStdString(ddesc.friendlyName),
                                     QString::fromStdString(ddesc.deviceType),
                                     QString::fromStdString(ddesc.modelName),
#ifdef DESKTOP
                                     QIcon(),
#else
                                     iconUrl,
#endif
                                     supported,
                                     active
                                     ));

#ifdef DESKTOP
            if (!iconUrl.isEmpty()) {
                auto downloader = new FileDownloader(iconUrl, this);
                connect(downloader, &FileDownloader::downloaded,
                        [this, id, downloader](int error){
                    //qDebug() << "Icon downloaded for:" << id;
                    if (error == 0) {
                        auto item = dynamic_cast<DeviceItem*>(this->find(id));
                        if (item) {
                            auto img = QImage::fromData(downloader->downloadedData());
                            img = img.scaled(QSize(icon_size,icon_size));
                            auto pix = QPixmap::fromImage(img);
                            item->setIcon(QIcon(pix));
                        }
                    } else {
                        qWarning() << "Icon downloading error";
                    }

                    downloader->deleteLater();
                });
            }
#endif
        }
    }
}

void DeviceModel::clear()
{
    if(rowCount()>0) removeRows(0,rowCount());
}

void DeviceModel::setActiveIndex(int index)
{
    int l = rowCount();
    for (int i = 0; i < l; ++i) {
        auto item = dynamic_cast<DeviceItem*>(readRow(i));
        if (item)
            item->setActive(i == index);
    }
}

DeviceItem::DeviceItem(const QString &id,
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
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_title(title),
    m_type(type),
    m_model(model),
    m_icon(icon),
    m_active(active),
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
    names[ActiveRole] = "active";
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
    case ActiveRole:
        return active();
#ifdef DESKTOP
    case ForegroundRole:
        return foreground();
#endif
    default:
        return QVariant();
    }
}

bool DeviceItem::isFav() const
{
    auto fd = Settings::instance()->getFavDevices();
    return fd.contains(m_id);
}

void DeviceItem::setActive(bool value)
{
    if (m_active != value) {
        m_active = value;
        emit dataChanged();
    }
}

#ifdef DESKTOP
void DeviceItem::setIcon(const QIcon& icon)
{
    m_icon = icon;
    emit dataChanged();
}

QBrush DeviceItem::foreground() const
{
    auto p = QApplication::palette();
    return m_supported ? m_active ? p.brush(QPalette::Highlight) :
                                    p.brush(QPalette::WindowText) :
                         p.brush(QPalette::Disabled, QPalette::WindowText);
}
#endif
