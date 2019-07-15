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
    TaskExecutor(parent),
    nm(new QNetworkAccessManager())
{
    init();
}

void Directory::init()
{
    auto u = Utils::instance();

    QString ifname, addr;

    if (!u->getNetworkIf(ifname, addr)) {
        qWarning() << "Cannot find valid network interface";
        setInited(false);
        emit error(1);
        return;
    }

    m_lib = UPnPP::LibUPnP::getLibUPnP(
        false, 0, ifname.toStdString(), addr.toStdString(), 0
    );

    if (m_lib == 0) {
        qWarning() << "Cannot initialize UPnPP lib";
        setInited(false);
        emit error(2);
        return;
    }

    m_lib->setLogFileName("", UPnPP::LibUPnP::LogLevelError);

    m_directory = UPnPClient::UPnPDeviceDirectory::getTheDir(4);

    if (m_directory == 0) {
        qWarning() << "Cannot initialize UPnPP directory";
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

void Directory::discover()
{
    if (!m_inited) {
        qWarning() << "Directory not inited.";
        return;
    }

    if (!Utils::instance()->checkNetworkIf()) {
        qWarning() << "Cannot find valid network interface";
        setInited(false);
        return;
    }

    if (taskActive()) {
        qWarning() << "Task is active. Skipping adding new task";
        return;
    }

    setBusy(true);

    startTask([this](){

        clearLists();

        if (m_directory == 0) {
            qWarning() << "Directory not initialized";
            setInited(false);
            emit error(3);
            return;
        }

        bool found = false;
        QHash<QString,bool> xcs;

        auto traverseFun = [this, &found, &xcs](const UPnPClient::UPnPDeviceDesc &ddesc,
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
            //qDebug() << "  ddesc.XMLText:" << QString::fromStdString(ddesc.XMLText);

            auto did = QString::fromStdString(ddesc.UDN);
            auto sid = QString::fromStdString(sdesc.serviceId);

            this->m_devsdesc.insert(did, ddesc);
            this->m_servsdesc.insert(did + sid, sdesc);

            if (!xcs.contains(did)) {
                xcs[did] = true;
                auto data = XCParser::parse(QString::fromStdString(ddesc.XMLText));
                if (data.valid) {
                    auto xc = new YamahaXC(); xc->data = data;
                    this->m_xcs.insert(did, xc);
                }
            }

            found = true;

            return true;
        };

        for (int i = 0; i < 5; ++i) {
            if (m_directory == 0) {
                qWarning() << "Directory not initialized";
                setInited(false);
                emit error(3);
                return;
            }
            qDebug() << "traverse:" << i;
            m_directory->traverse(traverseFun);
            if (found)
                break;
        }

        //qDebug() << "traverse end";

        emit discoveryReady();

        setBusy(false);
    });
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

    qWarning() << "Cannot find device" << deviceId << "with service" << serviceId;

    return false;
}

bool Directory::getDeviceDesc(const QString& deviceId, UPnPClient::UPnPDeviceDesc& ddesc)
{
    const auto it = m_devsdesc.find(deviceId);
    if (it != m_devsdesc.end()) {
        ddesc = it.value();
        return true;
    }

    qWarning() << "Cannot find device" << deviceId;

    return false;
}

YamahaXC* Directory::deviceXC(const QString& deviceId)
{
    auto it = m_xcs.find(deviceId);
    if (it != m_xcs.end())
        return it.value();
    return nullptr;
}

bool Directory::xcExists(const QString& deviceId)
{
    return m_xcs.contains(deviceId);
}

bool Directory::getBusy()
{
    return m_busy;
}

void Directory::xcTogglePower(const QString& deviceId)
{
    auto xc = deviceXC(deviceId);
    if (xc) {
        xc->powerToggle();
    } else {
        qWarning() << "Device doesn't have XC API";
    }
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
