/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "devicemodel.h"

#include <QDebug>
#include <QHash>
#include <algorithm>
#include <memory>
#include <utility>

#include "avtransport.h"
#include "contentdirectory.h"
#include "contentserver.h"
#include "directory.h"
#include "filedownloader.h"
#include "libupnpp/control/description.hxx"
#include "mpdtools.hpp"
#include "notifications.h"
#include "renderingcontrol.h"
#include "service.h"
#include "services.h"
#include "settings.h"
#include "utils.h"

DeviceModel::DeviceModel(QObject *parent) : ListModel(new DeviceItem, parent) {
    connect(Directory::instance(), &Directory::deviceFound, this,
            &DeviceModel::updateModel);
    connect(Directory::instance(), &Directory::initedChanged, this,
            &DeviceModel::updateModel);
    connect(Directory::instance(), &Directory::discoveryFavReady, this,
            &DeviceModel::updateModel);
    connect(Services::instance()->avTransport.get(), &Service::initedChanged,
            this, &DeviceModel::serviceInitedHandler);
    connect(Services::instance()->renderingControl.get(),
            &Service::initedChanged, this, &DeviceModel::serviceInitedHandler);
    connect(Services::instance()->contentDir.get(), &Service::initedChanged,
            this, &DeviceModel::serviceInitedHandler);
    connect(Settings::instance(), &Settings::favDeviceChanged, this,
            &ListModel::handleItemChangeById);
}

void DeviceModel::setDeviceType(DeviceModel::DeviceType value) {
    if (m_deviceType != value) {
        m_deviceType = value;
        emit deviceTypeChanged();
        updateModel();
    }
}

void DeviceModel::serviceInitedHandler() {
    const auto *service = qobject_cast<Service *>(sender());
    for (int i = 0; i < rowCount(); ++i) {
        if (auto *item = qobject_cast<DeviceItem *>(readRow(i)); item)
            item->setActive(item->id() == service->getDeviceId());
    }
}

void DeviceModel::handleXcError(XC::ErrorType code) {
    if (code == XC::ErrorType::ERROR_INVALID_PIN) {
        if (const auto *item = qobject_cast<DeviceItem *>(
                find(qobject_cast<XC *>(sender())->deviceId));
            item) {
            Notifications::instance()->show(
                tr("Invalid PIN for %1").arg(item->title()), {},
                item->icon().toLocalFile());
        }
    }
}

void DeviceModel::updateModel() {
    auto *d = Directory::instance();
    auto *s = Settings::instance();
    auto av = Services::instance()->avTransport;

    const auto &ddescs = d->getDeviceDescs();

    bool showAll = m_deviceType == MediaRendererType && s->getShowAllDevices();

    QString typeStr = showAll || m_deviceType == MediaRendererType
                          ? "urn:schemas-upnp-org:device:MediaRenderer"
                          : "urn:schemas-upnp-org:device:MediaServer";

    QString ownMediaServerId = "uuid:" + s->mediaServerDevUuid();

    clear();

    QList<ListItem *> items;

    for (auto it = ddescs.cbegin(); it != ddescs.cend(); ++it) {
        const auto &ddesc = it.value();
        auto type = QString::fromStdString(ddesc.deviceType);
        bool supported = type.contains(typeStr, Qt::CaseInsensitive);
        if ((supported &&
             QString::fromStdString(ddesc.UDN) != ownMediaServerId) ||
            showAll) {
            auto id = QString::fromStdString(ddesc.UDN);
            auto iconUrl = Directory::getDeviceIconUrl(ddesc);
            auto iconPath = Utils::deviceIconFilePath(id);
            auto model = QString::fromStdString(ddesc.modelName);
            auto xc = d->xc(it.key());

            bool active = av && av->getDeviceId() == id;
            bool upmdp = mpdtools::upmpdcliDev(
                model, QUrl{QString::fromStdString(ddesc.URLBase)});

            uint32_t flags = 0;
            if (xc) flags |= DeviceItem::Flags::Xc;
            if (supported) flags |= DeviceItem::Flags::Supported;
            if (upmdp) flags |= DeviceItem::Flags::Upmpd;

            auto *item = new DeviceItem{
                /*id=*/id,
                /*title=*/QString::fromStdString(ddesc.friendlyName),
                /*type=*/type,
                /*model=*/model,
                /*icon=*/upmdp || iconPath.isEmpty()
                    ? QUrl{}
                    : QUrl::fromLocalFile(iconPath),
                /*active=*/active,
                /*flags=*/flags};
            if (xc) {
                connect(xc->get().get(), &XC::error, this,
                        &DeviceModel::handleXcError, Qt::QueuedConnection);
            }

            items.push_back(item);

            if (iconPath.isEmpty() && !iconUrl.isEmpty()) {
                auto *downloader = new FileDownloader{iconUrl, this};
                connect(downloader, &FileDownloader::downloaded, this,
                        [this, id, downloader](int error) {
                            if (!error) {
                                auto *item =
                                    qobject_cast<DeviceItem *>(this->find(id));
                                if (item) {
                                    auto data = downloader->downloadedData();
                                    auto ext = ContentServer::extFromMime(
                                        downloader->contentType());

                                    if (!data.isEmpty() && !ext.isEmpty()) {
                                        Utils::writeToCacheFile(
                                            Utils::deviceIconFileName(id, ext),
                                            data, true);
                                        item->setIcon(QUrl::fromLocalFile(
                                            Utils::deviceIconFilePath(id)));
                                    }
                                }
                            } else {
                                qWarning()
                                    << "Cannot download device icon:" << id;
                            }

                            downloader->deleteLater();
                        });
            }
        }
    }

    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto *aa = qobject_cast<DeviceItem *>(a);
        auto *bb = qobject_cast<DeviceItem *>(b);
        if (aa->isFav()) {
            if (!bb->isFav()) return true;
        } else {
            if (bb->isFav()) return false;
        }
        return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
    });

    appendRows(items);
}

void DeviceModel::clear() {
    if (rowCount() > 0) removeRows(0, rowCount());
}

void DeviceModel::updatePower(const QString &id, bool value) {
    auto *item = find(id);
    if (item) qobject_cast<DeviceItem *>(item)->setPower(value);
}

DeviceItem::DeviceItem(const QString &id, const QString &title,
                       const QString &type, const QString &model,
                       const QUrl &icon, bool active, uint32_t flags,
                       QObject *parent)
    : ListItem{parent}, m_id{id}, m_title{title}, m_type{type}, m_model{model},
      m_icon{icon}, m_active{active}, m_flags{flags} {}

QHash<int, QByteArray> DeviceItem::roleNames() const {
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
    names[UpmpdRole] = "upmpd";
    return names;
}

QVariant DeviceItem::data(int role) const {
    switch (role) {
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
        case UpmpdRole:
            return upmpd();
        default:
            return {};
    }
}

bool DeviceItem::isFav() const {
    return upmpd() ? false
                   : Settings::instance()->getFavDevices().contains(m_id);
}

void DeviceItem::setActive(bool value) {
    if (m_active != value) {
        m_active = value;
        emit dataChanged();
    }
}

void DeviceItem::setPower(bool value) {
    if (m_power != value) {
        m_power = value;
        emit dataChanged();
    }
}

void DeviceItem::setIcon(const QUrl &icon) {
    m_icon = icon;
    emit dataChanged();
}
