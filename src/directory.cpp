/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "directory.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QList>
#include <QMap>
#include <QNetworkConfiguration>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QStringList>
#include <QThreadPool>
#include <algorithm>
#include <string>

#include "connectivitydetector.h"
#include "device.h"
#include "libupnpp/control/description.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/upnpplib.hxx"
#include "settings.h"
#include "xc.h"

#ifndef USE_SFOS_HARBOUR
#include "mpdtools.hpp"
#endif

Directory::Directory(QObject* parent) : QObject{parent}, TaskExecutor{parent} {
    connect(this, &Directory::busyChanged, this, &Directory::handleBusyChanged);
    connect(this, &Directory::initedChanged, this,
            &Directory::handleInitedChanged, Qt::QueuedConnection);
    connect(Settings::instance(), &Settings::contentDirSupportedChanged, this,
            &Directory::restartMediaServer, Qt::QueuedConnection);
    connect(ConnectivityDetector::instance(),
            &ConnectivityDetector::networkStateChanged, this, &Directory::init,
            Qt::QueuedConnection);
}

Directory::~Directory() { UPnPClient::UPnPDeviceDirectory::terminate(); }

void Directory::handleBusyChanged() {
    if (!m_busy) refreshXC();
}

void Directory::refreshXC() {
    qDebug() << "Refreshing status for XC devices";

    auto i = m_xcs.begin();
    while (i != m_xcs.end()) {
        i.value()->getStatus();
        ++i;
    }
}

void Directory::handleInitedChanged() { restartMediaServer(); }

void Directory::restartMediaServer() {
    if (m_inited && Settings::instance()->getContentDirSupported()) {
        msdev = std::make_unique<MediaServerDevice>();
    } else {
        msdev.reset();
    }
}

void Directory::init() {
    qDebug() << "Directory init";

    setBusy(true);

    QString ifname, addr;
    if (!ConnectivityDetector::instance()->selectNetworkIf(ifname, addr)) {
        qWarning() << "Cannot find valid network interface";
        setInited(false);
        setBusy(false);
        emit error(1);
        return;
    }

    qDebug() << "LibUPnP init:" << ifname << addr;

    m_lib = UPnPP::LibUPnP::getLibUPnP(false, nullptr, "*");

    if (!m_lib) {
        qWarning() << "Cannot initialize UPnPP lib (lib == nullptr)";
        setInited(false);
        setBusy(false);
        emit error(2);
        return;
    }

    if (!m_lib->ok()) {
        qWarning() << "Cannot initialize UPnPP lib (lib != ok)";
        setInited(false);
        setBusy(false);
        emit error(2);
        return;
    }

    UPnPP::LibUPnP::setLogLevel(UPnPP::LibUPnP::LogLevelError);

    m_directory = UPnPClient::UPnPDeviceDirectory::getTheDir(5);

    if (!m_directory) {
        qWarning() << "Cannot initialize UPnPP directory (dir == nullptr)";
        setInited(false);
        setBusy(false);
        emit error(3);
        return;
    }

    if (!m_directory->ok()) {
        qWarning() << "Cannot initialize UPnPP directory (dir != ok)";
        setInited(false);
        setBusy(false);
        m_directory = nullptr;
        emit error(2);
        return;
    }

    setInited(true);

    discover();
}

bool Directory::visitorCallback(const UPnPClient::UPnPDeviceDesc& ddesc,
                                const UPnPClient::UPnPServiceDesc& sdesc) {
    const auto did = QString::fromStdString(ddesc.UDN);
    const auto sid = did + QString::fromStdString(sdesc.serviceId);

    if (!this->m_servsdesc.contains(sid)) {
        qDebug() << "==> new service found";
        qDebug() << "Device| type:" << QString::fromStdString(ddesc.deviceType)
                 << "friendly name:"
                 << QString::fromStdString(ddesc.friendlyName);
#ifdef QT_DEBUG
        qDebug() << "  UDN:" << did;
        qDebug() << "  model name:" << QString::fromStdString(ddesc.modelName);
        qDebug() << "  URL base:" << QString::fromStdString(ddesc.URLBase);
#endif
        qDebug() << "Service| type:"
                 << QString::fromStdString(sdesc.serviceType);
#ifdef QT_DEBUG
        qDebug() << "  controlURL:" << QString::fromStdString(sdesc.controlURL);
        qDebug() << "  eventSubURL:"
                 << QString::fromStdString(sdesc.eventSubURL);
        qDebug() << "  SCPDURL:" << QString::fromStdString(sdesc.SCPDURL);
        qDebug() << "  serviceId:" << QString::fromStdString(sdesc.serviceId);
        qDebug() << "  ddesc.XMLText:" << QString::fromStdString(ddesc.XMLText);
#endif
        this->m_servsdesc.insert(sid, sdesc);

        if (!this->m_devsdesc.contains(did)) {
            this->m_devsdesc.insert(did, ddesc);
            checkXcs(ddesc);
            emit deviceFound(did);
        }
    }

    return true;
}

void Directory::checkXcs(const UPnPClient::UPnPDeviceDesc& ddesc) {
    const auto did = QString::fromStdString(ddesc.UDN);
    if (!m_xcs_status.contains(did) &&
        XC::possible(QString::fromStdString(ddesc.deviceType))) {
        m_xcs_status[did] = true;

        auto xc = XC::make_shared(
            did, QUrl{QString::fromStdString(ddesc.URLBase)}.host(),
            QString::fromStdString(ddesc.XMLText));
        if (xc) {
            qDebug() << "Valid" << xc->name() << "for" << did;
            xc->moveToThread(QCoreApplication::instance()->thread());
            this->m_xcs.insert(did, xc);
        }
    }
}

void Directory::clearLists(bool all) {
    m_devsdesc.clear();
    m_servsdesc.clear();
    if (all) m_last_devsdesc.clear();
}

void Directory::discoverStatic(const QHash<QString, QVariant>& devs,
                               QHash<QString, UPnPClient::UPnPDeviceDesc>& map,
                               bool xcs) {
    for (auto it = devs.cbegin(); it != devs.cend(); ++it) {
        const auto& id = it.key();

        QByteArray xml;
        if (!Settings::readDeviceXML(id, xml)) continue;

        UPnPClient::UPnPDeviceDesc ddesc{it.value().toString().toStdString(),
                                         xml.toStdString()};

        const auto did = QString::fromStdString(ddesc.UDN);
        for (const auto& sdesc : ddesc.services) {
            this->m_servsdesc.insert(
                did + QString::fromStdString(sdesc.serviceId), sdesc);
        }

        map.insert(did, ddesc);
        if (xcs) checkXcs(ddesc);
    }
}

void Directory::discover() {
    qDebug() << "Discover";

    if (!m_inited) {
        qWarning() << "Directory not inited.";
        setBusy(false);
        return;
    }

    if (!ConnectivityDetector::instance()->networkConnected()) {
        qWarning() << "Cannot find valid network interface";
        setInited(false);
        setBusy(false);
        return;
    }

    if (taskActive()) {
        qWarning() << "Task is active. Skipping adding new task";
        setBusy(false);
        return;
    }

    if (!m_directory) {
        qWarning() << "Directory not initialized";
        setInited(false);
        setBusy(false);
        emit error(3);
        return;
    }

    setBusy(true);

    UPnPClient::UPnPDeviceDirectory::delCallback(m_visitorCallbackId);

    discoverStatic(Settings::instance()->getLastDevices(),
                   this->m_last_devsdesc, false);
    emit discoveryLastReady();

    startTask([this]() {
        clearLists(false);

        discoverStatic(Settings::instance()->getFavDevices(), this->m_devsdesc,
                       true);
        emit discoveryFavReady();

#ifndef USE_SFOS_HARBOUR
        if (Settings::instance()->controlMpdService()) mpdtools::start();
#endif
        // discovery

        m_directory->traverse(std::bind(&Directory::visitorCallback, this,
                                        std::placeholders::_1,
                                        std::placeholders::_2));

        qDebug() << "traverse end:" << m_devsdesc.size();

        emit discoveryReady();

        setBusy(false);

        m_visitorCallbackId = UPnPClient::UPnPDeviceDirectory::addCallback(
            std::bind(&Directory::visitorCallback, this, std::placeholders::_1,
                      std::placeholders::_2));
    });
}

const QHash<QString, UPnPClient::UPnPDeviceDesc>& Directory::getDeviceDescs() {
    return m_devsdesc;
}

bool Directory::getServiceDesc(const QString& deviceId,
                               const QString& serviceId,
                               UPnPClient::UPnPServiceDesc& sdesc) {
    const auto it = m_servsdesc.find(deviceId + serviceId);
    if (it != m_servsdesc.end()) {
        sdesc = it.value();
        return true;
    }

    qWarning() << "Cannot find device" << deviceId << "with service"
               << serviceId;

    return false;
}

bool Directory::getDeviceDesc(const QString& deviceId,
                              UPnPClient::UPnPDeviceDesc& ddesc) {
    const auto it = m_devsdesc.find(deviceId);
    if (it != m_devsdesc.end()) {
        ddesc = it.value();
        return true;
    }

    const auto lit = m_last_devsdesc.find(deviceId);
    if (lit != m_last_devsdesc.end()) {
        ddesc = lit.value();
        // qDebug() << "Found last device:" << deviceId;
        return true;
    }

    qWarning() << "Cannot find device:" << deviceId;

    return false;
}

QString Directory::deviceNameFromId(const QString& deviceId) {
    const auto it = m_devsdesc.find(deviceId);
    if (it != m_devsdesc.end()) {
        return QString::fromStdString(it.value().friendlyName);
    }

    const auto lit = m_last_devsdesc.find(deviceId);
    if (lit != m_last_devsdesc.end()) {
        return QString::fromStdString(lit.value().friendlyName);
    }

    qWarning() << "Cannot find device name:" << deviceId;

    return {};
}

bool Directory::xcExists(const QString& deviceId) {
    const auto it = m_xcs.constFind(deviceId);
    return it != m_xcs.cend() && (*it)->valid();
}

std::optional<std::reference_wrapper<const std::shared_ptr<XC>>> Directory::xc(
    const QString& deviceId) {
    const auto it = m_xcs.constFind(deviceId);
    if (it == m_xcs.cend()) return std::nullopt;
    return m_xcs.constFind(deviceId).value();
}

void Directory::xcTogglePower(const QString& deviceId) {
    auto it = m_xcs.find(deviceId);
    if (it == m_xcs.end()) return;
    (*it)->powerToggle();
}

void Directory::xcGetStatus(const QString& deviceId) {
    auto it = m_xcs.find(deviceId);
    if (it == m_xcs.end()) return;
    (*it)->getStatus();
}

void Directory::xcPowerOn(const QString& deviceId) {
    auto it = m_xcs.find(deviceId);
    if (it == m_xcs.end()) return;
    (*it)->powerOn();
}

void Directory::setBusy(bool busy) {
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void Directory::setInited(bool inited) {
    if (inited != m_inited) {
        m_inited = inited;
        qDebug() << "Directory inited:" << m_inited;
        if (!m_inited) clearLists(true);
        emit initedChanged();
    }
}

QUrl Directory::getDeviceIconUrl(const UPnPClient::UPnPDeviceDesc& ddesc) {
    QDomDocument doc;
    QString error;
    if (!doc.setContent(QString::fromStdString(ddesc.XMLText), false, &error)) {
        qWarning() << "Parse error:" << error;
        return {};
    }

    const auto icons = doc.elementsByTagName("icon");

    // Find largest icon
    int max_size = 0;
    QString max_url;
    for (int i = 0; i < icons.length(); ++i) {
        const auto icon = icons.item(i).toElement();
        const auto mime =
            icon.elementsByTagName("mimetype").item(0).toElement().text();

        if (mime == "image/jpeg" || mime == "image/png") {
            const int size = icon.elementsByTagName("width")
                                 .item(0)
                                 .toElement()
                                 .text()
                                 .toInt() +
                             icon.elementsByTagName("height")
                                 .item(0)
                                 .toElement()
                                 .text()
                                 .toInt();
            if (size > max_size) {
                max_size = size;
                max_url =
                    icon.elementsByTagName("url").item(0).toElement().text();
            }
        }
    }

    if (max_url.isEmpty()) {
        qWarning() << "No icon for device:"
                   << QString::fromStdString(ddesc.friendlyName);
        return {};
    }

    return QUrl{QString::fromStdString(ddesc.URLBase)}.resolved(QUrl(max_url));
}
