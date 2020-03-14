/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
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
#include <QNetworkSession>
#include <memory>
#include <functional>

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/control/description.hxx"

#include "taskexecutor.h"
#include "yamahaextendedcontrol.h"
#include "device.h"

class Directory :
        public QObject,
        public TaskExecutor
{
    Q_OBJECT
    Q_PROPERTY (bool busy READ getBusy NOTIFY busyChanged)
    Q_PROPERTY (bool inited READ getInited NOTIFY initedChanged)
    Q_PROPERTY (bool networkConnected READ isNetworkConnected NOTIFY networkStateChanged)

public:
    QNetworkConfigurationManager* ncm;
    QNetworkSession* nsession;
    std::unique_ptr<MediaServerDevice> msdev;

    static Directory* instance(QObject *parent = nullptr);

    bool getBusy();
    bool getInited();
    bool getServiceDesc(const QString& deviceId, const QString& serviceId, UPnPClient::UPnPServiceDesc& sdesc);
    bool getDeviceDesc(const QString& deviceId, UPnPClient::UPnPDeviceDesc& ddesc);
    QString deviceNameFromId(const QString& deviceId);
    YamahaXC* deviceXC(const QString& deviceId);
    bool xcExists(const QString& deviceId);
    const QHash<QString,UPnPClient::UPnPDeviceDesc>& getDeviceDescs();
    QUrl getDeviceIconUrl(const UPnPClient::UPnPDeviceDesc& ddesc);
    Q_INVOKABLE void discover();
    bool isNetworkConnected();
    bool getNetworkIf(QString &ifname, QString &address);

    // Extended control API
    Q_INVOKABLE void xcTogglePower(const QString &deviceId);
    Q_INVOKABLE void xcGetStatus(const QString& deviceId);

signals:
    void discoveryReady();
    void discoveryFavReady();
    void discoveryLastReady();
    void busyChanged();
    void initedChanged();
    void networkStateChanged();
    void error(int code);

public slots:
    void init();

private slots:
    void handleBusyChanged();
    void handleInitedChanged();
    void handleNetworkConfChanged(const QNetworkConfiguration &conf);

private:
    static Directory* m_instance;
    bool m_busy = false;
    bool m_inited = false;
    UPnPP::LibUPnP* m_lib = 0;
    UPnPClient::UPnPDeviceDirectory* m_directory;
    QHash<QString,UPnPClient::UPnPServiceDesc> m_servsdesc;
    QHash<QString,UPnPClient::UPnPDeviceDesc> m_devsdesc;
    QHash<QString,UPnPClient::UPnPDeviceDesc> m_last_devsdesc;
    QHash<QString,YamahaXC*> m_xcs;
    QString m_ifname;
    explicit Directory(QObject *parent = nullptr);
    void setBusy(bool busy);
    void setInited(bool inited);
    bool handleError(int ret);
    void clearLists(bool all);
    void updateNetworkConf();
};

#endif // DIRECTORY_H
