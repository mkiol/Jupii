/* Copyright (C) 2017-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <QObject>
#include <QHash>
#include <QMap>
#include <QList>
#include <QString>
#include <QUrl>
#include <QNetworkConfigurationManager>
#include <memory>
#include <functional>
#include <optional>

#include "taskexecutor.h"

class XC;

namespace UPnPP {
    class LibUPnP;
}

namespace UPnPClient {
    class UPnPDeviceDirectory;
    class UPnPServiceDesc;
    class UPnPDeviceDesc;
}

class MediaServerDevice;

class Directory :
        public QObject,
        public TaskExecutor
{
    Q_OBJECT

    Q_PROPERTY (bool busy READ getBusy NOTIFY busyChanged)
    Q_PROPERTY (bool inited READ getInited NOTIFY initedChanged)

public:
    std::unique_ptr<MediaServerDevice> msdev;

    static Directory* instance(QObject *parent = nullptr);
    Directory(const Directory&) = delete;
    Directory& operator= (const Directory&) = delete;
    ~Directory();

    bool getBusy();
    bool getInited();
    bool getServiceDesc(const QString& deviceId, const QString& serviceId, UPnPClient::UPnPServiceDesc& sdesc);
    bool getDeviceDesc(const QString& deviceId, UPnPClient::UPnPDeviceDesc& ddesc);
    QString deviceNameFromId(const QString& deviceId);
    bool xcExists(const QString& deviceId);
    std::optional<std::reference_wrapper<const std::shared_ptr<XC>>> xc(const QString& deviceId);
    const QHash<QString,UPnPClient::UPnPDeviceDesc>& getDeviceDescs();
    QUrl getDeviceIconUrl(const UPnPClient::UPnPDeviceDesc& ddesc);
    Q_INVOKABLE void discover();

    // Extended control API
    Q_INVOKABLE void xcTogglePower(const QString &deviceId);
    Q_INVOKABLE void xcGetStatus(const QString& deviceId);
    Q_INVOKABLE void xcPowerOn(const QString& deviceId);

signals:
    void discoveryReady();
    void deviceFound(const QString &did);
    void discoveryFavReady();
    void discoveryLastReady();
    void busyChanged();
    void initedChanged();
    void error(int code);

public slots:
    void init();

private slots:
    void handleBusyChanged();
    void handleInitedChanged();
    void restartMediaServer();

private:
    static Directory* m_instance;
    bool m_busy = false;
    bool m_inited = false;
    unsigned int m_visitorCallbackId = 0;
    UPnPP::LibUPnP* m_lib = nullptr;
    UPnPClient::UPnPDeviceDirectory* m_directory;
    QHash<QString,UPnPClient::UPnPServiceDesc> m_servsdesc;
    QHash<QString,UPnPClient::UPnPDeviceDesc> m_devsdesc;
    QHash<QString,UPnPClient::UPnPDeviceDesc> m_last_devsdesc;
    QHash<QString,std::shared_ptr<XC>> m_xcs;
    QHash<QString,bool> m_xcs_status;

    explicit Directory(QObject *parent = nullptr);
    void setBusy(bool busy);
    void setInited(bool inited);
    bool handleError(int ret);
    void clearLists(bool all);
    void refreshXC();
    void discoverStatic(const QHash<QString,QVariant> &devs, QHash<QString, UPnPClient::UPnPDeviceDesc> &map);
    void checkXcs(const UPnPClient::UPnPDeviceDesc &ddesc);
    bool visitorCallback(const UPnPClient::UPnPDeviceDesc &ddesc, const UPnPClient::UPnPServiceDesc &sdesc);
};

#endif // DIRECTORY_H
