/* Copyright (C) 2019-2020 Michal Kosciesza <michal@mkiol.net>
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
#include "filedownloader.h"
#include "contentserver.h"
#include "service.h"
#include "services.h"
#include "avtransport.h"
#include "renderingcontrol.h"
#include "contentdirectory.h"

#include "libupnpp/control/description.hxx"

DeviceModel* DeviceModel::m_instance = nullptr;

DeviceModel* DeviceModel::instance(QObject *parent)
{
    if (DeviceModel::m_instance == nullptr) {
        DeviceModel::m_instance = new DeviceModel(parent);
    }

    return DeviceModel::m_instance;
}

DeviceModel::DeviceModel(QObject *parent) :
    ListModel(new DeviceItem, parent)
{
    connect(Directory::instance(), &Directory::discoveryReady,
            this, &DeviceModel::updateModel);
    connect(Directory::instance(), &Directory::initedChanged,
            this, &DeviceModel::updateModel);
    connect(Directory::instance(), &Directory::discoveryFavReady,
            this, &DeviceModel::updateModel);
    connect(Services::instance()->avTransport.get(), &Service::initedChanged,
            this, &DeviceModel::serviceInitedHandler);
    connect(Services::instance()->renderingControl.get(), &Service::initedChanged,
            this, &DeviceModel::serviceInitedHandler);
    connect(Services::instance()->contentDir.get(), &Service::initedChanged,
            this, &DeviceModel::serviceInitedHandler);
    connect(Settings::instance(), &Settings::favDeviceChanged,
            this, &ListModel::handleItemChangeById);
}

DeviceModel::DeviceType DeviceModel::getDeviceType()
{
    return m_deviceType;
}

void DeviceModel::setDeviceType(DeviceModel::DeviceType value)
{
    if (m_deviceType != value) {
        m_deviceType = value;
        emit deviceTypeChanged();
        updateModel();
    }
}

void DeviceModel::serviceInitedHandler()
{
    auto service = dynamic_cast<Service*>(sender());
    if (service) {
        int l = rowCount();
        for (int i = 0; i < l; ++i) {
            auto item = dynamic_cast<DeviceItem*>(readRow(i));
            if (item)
                item->setActive(item->id() == service->getDeviceId());
        }
    }
}

void DeviceModel::updateModel()
{
    auto d = Directory::instance();
    auto s = Settings::instance();
    auto av = Services::instance()->avTransport;

    auto& ddescs = d->getDeviceDescs();

    bool showAll = m_deviceType == MediaRendererType && s->getShowAllDevices();

    QString typeStr = showAll || m_deviceType == MediaRendererType ?
                "urn:schemas-upnp-org:device:MediaRenderer" :
                "urn:schemas-upnp-org:device:MediaServer";

    QString ownMediaServerId = "uuid:" + s->mediaServerDevUuid();

    clear();

    QList<ListItem*> items;

    for (auto it = ddescs.begin(); it != ddescs.end(); ++it) {
        auto& ddesc = it.value();
        auto type = QString::fromStdString(ddesc.deviceType);
        auto devid = QString::fromStdString(ddesc.UDN);
        bool supported = type.contains(typeStr, Qt::CaseInsensitive);
        if ((supported && devid != ownMediaServerId) || showAll) {
            auto id = QString::fromStdString(ddesc.UDN);
            auto iconUrl = d->getDeviceIconUrl(ddesc);
            auto iconPath = Utils::deviceIconFilePath(id);
            bool active = av && av->getDeviceId() == id;
            bool xc = d->xcExists(it.key());
            items << new DeviceItem(id,
                             QString::fromStdString(ddesc.friendlyName),
                             type,
                             QString::fromStdString(ddesc.modelName),
                             iconPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(iconPath),
                             supported,
                             active,
                             xc
                         );

            if (iconPath.isEmpty() && !iconUrl.isEmpty()) {
                auto downloader = new FileDownloader(iconUrl, this);
                connect(downloader, &FileDownloader::downloaded, this,
                        [this, id, downloader](int error){
                    if (!error) {
                        auto item = qobject_cast<DeviceItem*>(this->find(id));
                        if (item) {
                            auto data = downloader->downloadedData();
                            auto ext = ContentServer::extFromMime(downloader->contentType());

                            if (!data.isEmpty() && !ext.isEmpty()) {
                                Utils::instance()->writeToCacheFile(
                                            Utils::deviceIconFileName(id, ext), data, true);
                                item->setIcon(QUrl::fromLocalFile(Utils::deviceIconFilePath(id)));
                            }
                        }
                    } else {
                        qWarning() << "Cannot download device icon:" << id;
                    }

                    downloader->deleteLater();
                });
            }
        }
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<DeviceItem*>(a);
        auto bb = dynamic_cast<DeviceItem*>(b);
        if (aa->isFav()) {
            if (!bb->isFav())
                return true;
        } else {
            if (bb->isFav())
                return false;
        }
        return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
    });

    appendRows(items);
}

void DeviceModel::clear()
{
    if(rowCount()>0) removeRows(0,rowCount());
}

void DeviceModel::updatePower(const QString &id, bool value)
{
    auto item = find(id);
    if (item)
        dynamic_cast<DeviceItem*>(item)->setPower(value);
}

DeviceItem::DeviceItem(const QString &id,
                   const QString &title,
                   const QString &type,
                   const QString &model,
                   const QUrl &icon,
                   bool supported,
                   bool active,
                   bool jxc,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_title(title),
    m_type(type),
    m_model(model),
    m_icon(icon),
    m_active(active),
    m_supported(supported),
    m_xc(jxc)
{
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
    names[XcRole] = "xc";
    names[PowerRole] = "power";
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
    case XcRole:
        return xc();
    case PowerRole:
        return power();
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

void DeviceItem::setPower(bool value)
{
    if (m_power != value) {
        m_power = value;
        emit dataChanged();
    }
}

void DeviceItem::setIcon(const QUrl& icon)
{
    m_icon = icon;
    emit dataChanged();
}
