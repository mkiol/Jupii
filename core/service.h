/* Copyright (C) 2017-2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SERVICE_H
#define SERVICE_H

#include <QObject>
#include <QRunnable>
#include <QTimer>
#include <QString>
#include <QVariant>
#include <functional>

#include <libupnpp/control/avtransport.hxx>
#include <libupnpp/control/service.hxx>

#include "taskexecutor.h"

class Service :
        public QObject,
        public UPnPClient::VarEventReporter,
        public TaskExecutor
{
    Q_OBJECT
    Q_PROPERTY (bool inited READ getInited NOTIFY initedChanged)
    Q_PROPERTY (bool busy READ getBusy NOTIFY busyChanged)
    Q_PROPERTY (QString deviceId READ getDeviceId NOTIFY initedChanged)
    Q_PROPERTY (QString deviceFriendlyName READ getDeviceFriendlyName NOTIFY initedChanged)

public:
    enum ErrorType {
        E_Unknown,
        E_NotInited,
        E_ServerError,
        E_InvalidPath,
        E_LostConnection
    };

    Q_ENUM(ErrorType)

    explicit Service(QObject *parent = nullptr);
    ~Service();

    Q_INVOKABLE bool init(const QString &deviceId);
    bool getInited();
    bool isInitedOrIniting();
    bool getBusy();
    QString getDeviceId() const;
    QString getDeviceFriendlyName() const;

signals:
    void initedChanged();
    void busyChanged();
    void error(ErrorType code);
    void needTimer(bool start);

public slots:
    void deInit();

protected slots:
    virtual void timerEvent();
    void timer(bool start);

protected:
    UPnPClient::Service* m_ser = nullptr;
    QTimer m_timer;
    bool m_initing = false;

    void setBusy(bool busy);
    void setInited(bool inited);

    virtual void changed(const QString &name, const QVariant &value) = 0;
    virtual UPnPClient::Service* createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                                   const UPnPClient::UPnPServiceDesc &sdesc) = 0;
    virtual void postInit();
    virtual void postDeInit();
    virtual void reset();
    virtual std::string type() const = 0;

    virtual void handleApplicationStateChanged(Qt::ApplicationState state);

    bool handleError(int ret);

private:
    bool m_busy = false;
    bool m_inited = false;
    void changed(const char *nm, int value);
    void changed(const char *nm, const char *value);
};

#endif // SERVICE_H
