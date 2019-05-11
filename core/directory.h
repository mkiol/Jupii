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
#include <QNetworkAccessManager>
#include <memory>
#include <functional>

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/control/description.hxx"

#include "taskexecutor.h"
#include "yamahaextendedcontrol.h"

class Directory :
        public QObject,
        public TaskExecutor
{
    Q_OBJECT
    Q_PROPERTY (bool busy READ getBusy NOTIFY busyChanged)
    Q_PROPERTY (bool inited READ getInited NOTIFY initedChanged)

public:
    std::unique_ptr<QNetworkAccessManager> nm;

    static Directory* instance(QObject *parent = nullptr);

    bool getBusy();
    bool getInited();
    bool getServiceDesc(const QString& deviceId, const QString& serviceId, UPnPClient::UPnPServiceDesc& sdesc);
    bool getDeviceDesc(const QString& deviceId, UPnPClient::UPnPDeviceDesc& ddesc);
    YamahaXC* deviceXC(const QString& deviceId);
    bool xcExists(const QString& deviceId);
    const QHash<QString,UPnPClient::UPnPDeviceDesc>& getDeviceDescs();
    QUrl getDeviceIconUrl(const UPnPClient::UPnPDeviceDesc& ddesc);
    Q_INVOKABLE void init();
    Q_INVOKABLE void discover();
    Q_INVOKABLE void discoverFavs();

    // Extended control API
    Q_INVOKABLE void xcTogglePower(const QString& deviceId);

signals:
    void discoveryReady();
    void busyChanged();
    void initedChanged();
    void error(int code);

private:
    static Directory* m_instance;
    bool m_busy = false;
    bool m_inited = false;
    UPnPP::LibUPnP* m_lib = 0;
    UPnPClient::UPnPDeviceDirectory* m_directory;
    QHash<QString,UPnPClient::UPnPServiceDesc> m_servsdesc;
    QHash<QString,UPnPClient::UPnPDeviceDesc> m_devsdesc;
    QHash<QString,YamahaXC*> m_xcs;
    explicit Directory(QObject *parent = nullptr);
    void setBusy(bool busy);
    void setInited(bool inited);
    bool handleError(int ret);
    void clearLists();
};

#endif // DIRECTORY_H
