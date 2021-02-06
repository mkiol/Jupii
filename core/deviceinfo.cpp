/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "deviceinfo.h"

#include <QDebug>
#include <algorithm>

#include "directory.h"
#include "utils.h"


DeviceInfo::DeviceInfo(QObject *parent) : QObject(parent)
{
}

void DeviceInfo::setUdn(const QString& value)
{
    if (value != m_udn) {
        if (Directory::instance()->getDeviceDesc(value, m_ddesc)) {
            m_udn = value;
            emit dataChanged();
        } else {
            qWarning() << "Cannot find device description";
        }
    }
}

QString DeviceInfo::getUdn() const
{
    return m_udn;
}

QString DeviceInfo::getFriendlyName() const
{
    return QString::fromStdString(m_ddesc.friendlyName);
}

QString DeviceInfo::getDeviceType() const
{
    return QString::fromStdString(m_ddesc.deviceType);
}

QString DeviceInfo::getModelName() const
{
    return QString::fromStdString(m_ddesc.modelName);
}

QString DeviceInfo::getUrlBase() const
{
    return QString::fromStdString(m_ddesc.URLBase);
}

QString DeviceInfo::getManufacturer() const
{
    return QString::fromStdString(m_ddesc.manufacturer);
}

QUrl DeviceInfo::getIcon() const
{
    auto iconPath = Utils::deviceIconFilePath(getUdn());
    return iconPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(iconPath);
}

QString DeviceInfo::getXML() const
{
    return QString::fromStdString(m_ddesc.XMLText);
}

QStringList DeviceInfo::getServices() const
{
    QStringList list;

    std::transform(m_ddesc.services.cbegin(), m_ddesc.services.cend(), std::back_inserter(list),
                   [](const auto& service) {
        return QString::fromStdString(service.serviceType);
    });

    return list;
}
