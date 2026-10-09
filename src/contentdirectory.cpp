/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "contentdirectory.h"

#include <QDebug>
#include <QGuiApplication>
#include <QThread>
#include <algorithm>
#include <set>

ContentDirectory::ContentDirectory(QObject *parent) : Service(parent) {}

void ContentDirectory::changed(const QString &name, const QVariant &value) {
    Q_UNUSED(name)
    Q_UNUSED(value)
}

UPnPClient::Service *ContentDirectory::createUpnpService(
    const UPnPClient::UPnPDeviceDesc &ddesc,
    const UPnPClient::UPnPServiceDesc &sdesc) {
    return new UPnPClient::ContentDirectory(ddesc, sdesc);
}

void ContentDirectory::postInit() {}

void ContentDirectory::reset() {
    m_searchCaps.clear();
    m_searchCapsLoaded = false;
}

std::string ContentDirectory::type() const {
    return "urn:schemas-upnp-org:service:ContentDirectory:1";
}

UPnPClient::ContentDirectory *ContentDirectory::s() {
    if (m_ser == nullptr) {
        qWarning() << "content-directory is not inited!";
        // emit error(E_NotInited);
        return nullptr;
    }

    return static_cast<UPnPClient::ContentDirectory *>(m_ser);
}

bool ContentDirectory::readItems(const QString &id,
                                 UPnPClient::UPnPDirContent &content) {
    int ret = 0;
    {
        QMutexLocker lock{&m_mtx};
        auto srv = s();
        if (!getInited() || !srv) {
            qWarning() << "content-directory service is not inited";
            return false;
        }

        ret = srv->readDir(id.toStdString(), content);
    }

    if (!handleError(ret)) {
        return false;
    }

#ifdef QT_DEBUG
    qDebug() << "containers:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_containers) {
        qDebug() << QString::fromStdString(dir.dump());
    }
    qDebug() << "items:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_items) {
        qDebug() << QString::fromStdString(dir.dump());
    }
#endif
    return true;
}

bool ContentDirectory::readItem(const QString &id, const QString &pid,
                                UPnPClient::UPnPDirContent &content) {
    int ret = 0;
    {
        QMutexLocker lock{&m_mtx};
        auto srv = s();
        if (!getInited() || !srv) {
            qWarning() << "content-directory service is not inited";
            return false;
        }

        ret = srv->getMetadata(id.toStdString(), content);
    }

    if (!handleError(ret)) return false;

#ifdef QT_DEBUG
    qDebug() << "id:" << id;
    qDebug() << "pid:" << pid;
    qDebug() << "containers:";
    for (const UPnPClient::UPnPDirObject &obj : content.m_containers)
        qDebug() << QString::fromStdString(obj.dump());
    qDebug() << "items:";
    for (const UPnPClient::UPnPDirObject &obj : content.m_items)
        qDebug() << QString::fromStdString(obj.dump());
#endif

    if (content.m_containers.empty() ||
        content.m_containers[0].m_resources.empty() ||
        content.m_containers[0].m_resources[0].m_uri.empty()) {
        qDebug() << "upnp item url is missing, trying search with pid";

        UPnPClient::UPnPDirContent parentContent;

        if (!readItems(pid, parentContent)) return false;

        auto it = std::find_if(
            parentContent.m_items.begin(), parentContent.m_items.end(),
            [objid = id.toStdString()](const UPnPClient::UPnPDirObject &obj) {
                return obj.m_id == objid;
            });

        if (it == parentContent.m_items.end()) return false;

        qDebug() << "found item:" << QString::fromStdString(it->dump());

        content.clear();
        content.m_items.push_back(std::move(*it));
    }

    return true;
}

bool ContentDirectory::search(const QString &criteria,
                              UPnPClient::UPnPDirContent &content) {
    int ret = 0;
    {
        QMutexLocker lock{&m_mtx};
        auto srv = s();
        if (!getInited() || !srv) {
            qWarning() << "content-directory service is not inited";
            return false;
        }

        ret = srv->search("0", criteria.toStdString(), content);
    }

    if (!handleError(ret)) {
        return false;
    }

#ifdef QT_DEBUG
    qDebug() << "search containers:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_containers) {
        qDebug() << QString::fromStdString(dir.dump());
    }
    qDebug() << "search items:";
    for (const UPnPClient::UPnPDirObject &dir : content.m_items) {
        qDebug() << QString::fromStdString(dir.dump());
    }
#endif
    return true;
}

QStringList ContentDirectory::searchCapabilities() {
    QMutexLocker lock{&m_mtx};
    auto srv = s();
    if (!getInited() || !srv) {
        qWarning() << "content-directory service is not inited";
        return {};
    }

    if (m_searchCapsLoaded) {
        return m_searchCaps;
    }

    std::set<std::string> caps;
    int ret = srv->getSearchCapabilities(caps);
    if (!handleError(ret)) {
        // Do not cache a failure: allow retrying on next call.
        return {};
    }

    m_searchCaps.clear();
    for (const auto &cap : caps) {
        m_searchCaps.append(QString::fromStdString(cap));
    }
    m_searchCapsLoaded = true;
    qDebug() << "search capabilities:" << m_searchCaps;
    return m_searchCaps;
}
