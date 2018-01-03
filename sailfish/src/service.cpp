/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QThreadPool>
#include <QGuiApplication>

#include "service.h"
#include "directory.h"
#include "devicemodel.h"
#include "taskexecutor.h"

Service::Service(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent, 5),
    m_timer(parent)
{
    QObject::connect(&m_timer, &QTimer::timeout, this, &Service::timerEvent);
    QObject::connect(this, &Service::needTimer, this, &Service::timer);

    auto app = static_cast<QGuiApplication*>(QGuiApplication::instance());
    QObject::connect(app, &QGuiApplication::applicationStateChanged,
                     this, &Service::handleApplicationStateChanged);
}

Service::~Service()
{
    deInit();
    waitForDone();
}

void Service::tsleep(int ms)
{
    if (ms > 0)
        QThread::currentThread()->msleep(ms);
}

void Service::changed(const char *nm, const char *value)
{
    qDebug() << "changed char*:" << nm << value;
    changed(QString(nm), QVariant::fromValue(QString(value)));
}

void Service::changed(const char *nm, int value)
{
    qDebug() << "changed int:" << nm << value;
    changed(QString(nm), QVariant::fromValue(value));
}

bool Service::getInited()
{
    return m_inited;
}

bool Service::getBusy()
{
    return m_busy;
}

void Service::setBusy(bool value)
{
    if (m_busy != value) {
        m_busy = value;
        emit busyChanged();
    }
}

void Service::setInited(bool value)
{
    if (m_ser != nullptr) {
        if (m_inited != value) {
            m_inited = value;
            emit initedChanged();
        }
    } else {
        if (m_inited != false) {
            m_inited = false;
            emit initedChanged();
        }
    }
}

void Service::handleApplicationStateChanged(Qt::ApplicationState state)
{
    qDebug() << "State changed:" << state;
}

bool Service::deInit()
{
    m_initing = false;

    timer(false);

    postDeInit();

    if (m_ser != nullptr) {
        m_ser->installReporter(nullptr);
        delete m_ser;
        m_ser = nullptr;
        setInited(false);

        return true;
    }

    setInited(false);
    setBusy(false);

    return false;
}

bool Service::init(const QString &deviceId)
{
    if (deviceId.isEmpty()) {
        qWarning() << "To init deviceId must not be empty!";
        return false;
    }

    auto d = Directory::instance();

    UPnPClient::UPnPDeviceDesc ddesc;
    if (!d->getDeviceDesc(deviceId, ddesc)) {
        qWarning() << "Can't find device description!";
        return false;
    }

    UPnPClient::UPnPServiceDesc sdesc;
    for (auto& _sdesc : ddesc.services) {
        if (_sdesc.serviceType == type()) {
            sdesc = _sdesc;
            break;
        }
    }

    if (sdesc.serviceId.empty()) {
        qWarning() << "Can't find" << QString::fromStdString(type())
                   << "service description for" << deviceId << "device";
        return false;
    }

    setBusy(true);
    startTask([this, ddesc, sdesc](){
        if (m_ser != nullptr)
            delete m_ser;

        m_initing = true;

        m_ser = createUpnpService(ddesc, sdesc);

        if (m_ser == nullptr) {
            qWarning() << "Unable to create UPnP service";
        } else {
            postInit();

            if (m_ser == nullptr) {
                qWarning() << "Unable to connect to UPnP service";
                return;
            }

            m_ser->installReporter(this);

            setInited(true);

            m_initing = false;
        }

        setBusy(false);
    });

    return true;
}

void Service::postInit()
{
}

void Service::postDeInit()
{
}

void Service::timerEvent()
{
}

void Service::timer(bool start)
{
    /*qDebug() << "Timer request";
    qDebug() << "  isActive:" << m_timer.isActive();
    qDebug() << "  reqest start:" << start;*/

    if (start && (m_inited || m_initing)) {
        if (!m_timer.isActive()) {
            m_timer.start(1000);
        }
    } else {
        if (m_timer.isActive()) {
            m_timer.stop();
        }
    }
}

bool Service::handleError(int ret)
{
    //qDebug() << "handleError:" << ret;

    if (ret < 0) {
        qWarning() << "Upnp request error:" << ret;

        switch (ret) {
        case -200:
        case -201:
        case -202:
        case -203:
        case -204:
        case -205:
        case -206:
        case -207:
        case -208:
            emit error(E_LostConnection);
            deInit();
            break;
        default:
            emit error(E_ServerError);
        }

        return false;
    }

    return true;
}
