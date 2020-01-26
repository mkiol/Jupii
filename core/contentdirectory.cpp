/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QThread>
#include <QGuiApplication>

#include "contentdirectory.h"

ContentDirectory::ContentDirectory(QObject *parent) : Service(parent)
{
}

void ContentDirectory::changed(const QString &name, const QVariant &_value)
{
}

UPnPClient::Service* ContentDirectory::createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                                         const UPnPClient::UPnPServiceDesc &sdesc)
{
    return new UPnPClient::ContentDirectory(ddesc, sdesc);
}

void ContentDirectory::postInit()
{
}

void ContentDirectory::reset()
{
}

std::string ContentDirectory::type() const
{
    return "urn:schemas-upnp-org:service:ContentDirectory:1";
}

UPnPClient::ContentDirectory* ContentDirectory::s()
{
    if (m_ser == nullptr) {
        qWarning() << "ContentDirectory is not inited!";
        //emit error(E_NotInited);
        return nullptr;
    }

    return static_cast<UPnPClient::ContentDirectory*>(m_ser);
}

bool ContentDirectory::readItems(const QString &id, UPnPClient::UPnPDirContent &content)
{
    auto srv = s();

    if (!getInited() || !srv) {
        qWarning() << "ContentDirectory service is not inited";
        return false;
    }

    if (!handleError(srv->readDir(id.toStdString(), content))) {
        return false;
    }

    qDebug() << "containers:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_containers) {
        qDebug() << QString::fromStdString(dir.dump());
    }
    qDebug() << "items:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_items) {
        qDebug() << QString::fromStdString(dir.dump());
    }

    return true;
}

bool ContentDirectory::readItem(const QString &id, UPnPClient::UPnPDirContent &content)
{
    auto srv = s();

    if (!getInited() || !srv) {
        qWarning() << "ContentDirectory service is not inited";
        return false;
    }

    if (!handleError(srv->getMetadata(id.toStdString(), content))) {
        return false;
    }

    qDebug() << "containers:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_containers) {
        qDebug() << QString::fromStdString(dir.dump());
    }
    qDebug() << "items:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_items) {
        qDebug() << QString::fromStdString(dir.dump());
    }

    return true;
}
