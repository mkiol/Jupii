/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QThreadPool>
#include <QDebug>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QByteArray>
#include <QFile>

#include <string>

#include <libupnpp/control/description.hxx>

#include "settings.h"
#include "utils.h"
#include "directory.h"

Directory* Directory::m_instance = nullptr;

Directory::Directory(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent)
{
    init();
}

void Directory::init()
{
    Utils* u = Utils::instance();

    QString ifname, addr;

    if (!u->getNetworkIf(ifname, addr)) {
        qWarning() << "Can't find valid network interface";
        setInited(false);
        emit error(1);
        return;
    }

    m_lib = UPnPP::LibUPnP::getLibUPnP(
        false, 0, ifname.toStdString(), addr.toStdString(), 0
    );

    if (m_lib == 0) {
        qWarning() << "Can't initialize UPnPP lib";
        setInited(false);
        emit error(2);
        return;
    }

    // Delete old libupnpp log files
    // TODO: Remove before release
    Utils::removeFile("/home/nemo/IUpnpErrFile.txt");
    Utils::removeFile("/home/nemo/IUpnpInfoFile.txt");

    m_lib->setLogFileName("", UPnPP::LibUPnP::LogLevelError);

    m_directory = UPnPClient::UPnPDeviceDirectory::getTheDir(4);

    if (m_directory == 0) {
        qWarning() << "Can't initialize UPnPP directory";
        setInited(false);
        emit error(3);
        return;
    }

    setInited(true);
}

Directory* Directory::instance(QObject *parent)
{
    if (Directory::m_instance == nullptr) {
        Directory::m_instance = new Directory(parent);
    }

    return Directory::m_instance;
}

void Directory::clearLists()
{
    this->m_devsdesc.clear();
    this->m_servsdesc.clear();
}

void Directory::discover(const QString& ssdpIp)
{
    if (!m_inited) {
        qWarning() << "Directory not inited.";
        return;
    }

    if (!Utils::instance()->checkNetworkIf()) {
        qWarning() << "Can't find valid network interface";
        setInited(false);
        return;
    }

    if (taskActive()) {
        qWarning() << "Task is active. Skipping adding new task";
        return;
    }

    setBusy(true);

    startTask([this, ssdpIp](){

        clearLists();

        if (ssdpIp.isEmpty())
            m_directory->resetSsdpIP();
        else
            m_directory->setSsdpIP(ssdpIp.toStdString());

        bool found = false;
        auto traverseFun = [this, &found](const UPnPClient::UPnPDeviceDesc &ddesc,
                const UPnPClient::UPnPServiceDesc &sdesc) {
            /*qDebug() << "==> Visitor";
            qDebug() << " Device";
            qDebug() << "  friendlyName:" << QString::fromStdString(ddesc.friendlyName);
            qDebug() << "  deviceType:" << QString::fromStdString(ddesc.deviceType);
            qDebug() << "  UDN:" << QString::fromStdString(ddesc.UDN);
            qDebug() << "  modelName:" << QString::fromStdString(ddesc.modelName);
            qDebug() << "  URLBase:" << QString::fromStdString(ddesc.URLBase);
            qDebug() << " Service";
            qDebug() << "  controlURL:" << QString::fromStdString(sdesc.controlURL);
            qDebug() << "  eventSubURL:" << QString::fromStdString(sdesc.eventSubURL);
            qDebug() << "  SCPDURL:" << QString::fromStdString(sdesc.SCPDURL);
            qDebug() << "  serviceId:" << QString::fromStdString(sdesc.serviceId);
            qDebug() << "  serviceType:" << QString::fromStdString(sdesc.serviceType);*/

            auto did = QString::fromStdString(ddesc.UDN);
            auto sid = QString::fromStdString(sdesc.serviceId);

            this->m_devsdesc.insert(did, ddesc);
            this->m_servsdesc.insert(did + sid, sdesc);

            found = true;

            return true;
        };

        for (int i = 0; i < 3; ++i) {
            m_directory->traverse(traverseFun);
            qDebug() << "traverse found:" << found;
            if (found)
                break;
        }

        qDebug() << "traverse end";

        m_directory->resetSsdpIP();

        emit discoveryReady();

        setBusy(false);
    });
}

void Directory::discover()
{
    discover(QString());
}

void Directory::discoverFavs()
{
    if (!m_inited) {
        qWarning() << "Directory not inited.";
        return;
    }

    setBusy(true);

    startTask([this](){
        auto s = Settings::instance();

        clearLists();

        auto favs = s->getFavDevices();
        for (auto it = favs.begin(); it != favs.end(); ++it) {

            qDebug() << it.key() << it.value().toString();

            QString id = it.key();
            QString url = it.value().toString();
            QByteArray xml;

            if (!s->readDeviceXML(id, xml))
                return;

            UPnPClient::UPnPDeviceDesc ddesc(url.toStdString(), xml.toStdString());

            auto did = QString::fromStdString(ddesc.UDN);

            for (auto& sdesc : ddesc.services) {

                auto sid = QString::fromStdString(sdesc.serviceId);

                this->m_servsdesc.insert(did + sid, sdesc);
            }

            this->m_devsdesc.insert(did, ddesc);
        }

        emit discoveryReady();

        setBusy(false);

        // Empty traverse to init directory
        m_directory->traverse([this](const UPnPClient::UPnPDeviceDesc &ddesc,
                                     const UPnPClient::UPnPServiceDesc &sdesc) {
            Q_UNUSED(ddesc)
            Q_UNUSED(sdesc)
            return true;
        });

    });
}

const QHash<QString,UPnPClient::UPnPDeviceDesc>& Directory::getDeviceDescs()
{
    return m_devsdesc;
}

bool Directory::getServiceDesc(const QString& deviceId, const QString& serviceId,
                                   UPnPClient::UPnPServiceDesc& sdesc)
{
    const auto it = m_servsdesc.find(deviceId + serviceId);
    if (it != m_servsdesc.end()) {
        sdesc = it.value();
        return true;
    }

    qWarning() << "Can't find device" << deviceId << "with service" << serviceId;

    return false;
}

bool Directory::getDeviceDesc(const QString& deviceId, UPnPClient::UPnPDeviceDesc& ddesc)
{
    const auto it = m_devsdesc.find(deviceId);
    if (it != m_devsdesc.end()) {
        ddesc = it.value();
        return true;
    }

    qWarning() << "Can't find device" << deviceId;

    return false;
}

bool Directory::getBusy()
{
    return m_busy;
}

bool Directory::getInited()
{
    return m_inited;
}

void Directory::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void Directory::setInited(bool inited)
{
    if (inited != m_inited) {
        m_inited = inited;
        emit initedChanged();
    }
}

QUrl Directory::getDeviceIconUrl(const UPnPClient::UPnPDeviceDesc& ddesc)
{
    if (ddesc.iconList.empty())
        return QUrl();

    // Finding largest icon
    int max_size = 0; std::string max_url;
    for (auto icon : ddesc.iconList) {
        if (icon.mimeType == "image/jpeg" || icon.mimeType == "image/png") {
            int size = icon.width + icon.height;
            if (size > max_size) {
                max_url = icon.url;
            }
        }
    }

    QUrl url(QString::fromStdString(ddesc.URLBase));
    return url.resolved(QUrl(QString::fromStdString(max_url)));
}
