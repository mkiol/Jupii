/* Copyright (C) 2017-2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "service.h"

#include <QDebug>
#include <QGuiApplication>
#include <QMutexLocker>
#include <QThreadPool>

#include "devicemodel.h"
#include "directory.h"
#include "libupnpp/control/description.hxx"
#include "taskexecutor.h"
#include "utils.h"

Service::Service(QObject *parent)
    : QObject(parent), TaskExecutor(parent, 5), m_timer(parent) {
    QObject::connect(&m_timer, &QTimer::timeout, this, &Service::timerEvent);
    QObject::connect(this, &Service::needTimer, this, &Service::timer);

#ifdef USE_SFOS
    auto app = static_cast<QGuiApplication *>(QGuiApplication::instance());
    QObject::connect(app, &QGuiApplication::applicationStateChanged, this,
                     &Service::handleApplicationStateChanged);
#endif

    auto dir = Directory::instance();
    connect(dir, &Directory::initedChanged, this, &Service::deInit);
}

Service::~Service() {
    {
        QMutexLocker lock{&m_mtx};
        if (m_ser) {
            m_ser->installReporter(nullptr);
            delete m_ser;
            m_ser = nullptr;
            qDebug() << "Service deleted";
        }
    }

    setInited(false);

    waitForDone();
}

void Service::changed(const char *nm, const char *value) {
    qDebug() << "changed char*:" << nm << value;
    changed(QString(nm), QVariant::fromValue(QString(value)));
}

void Service::changed(const char *nm, int value) {
    qDebug() << "changed int:" << nm << value;
    changed(QString(nm), QVariant::fromValue(value));
}

bool Service::getInited() { return m_inited; }

bool Service::isInitedOrIniting() { return m_inited || m_initing; }

bool Service::getBusy() { return m_busy; }

void Service::setBusy(bool value) {
    if (m_busy != value) {
        m_busy = value;
        emit busyChanged();
    }
}

void Service::setInited(bool value) {
    bool doEmit = false;

    {
        QMutexLocker lock{&m_mtx};
        if (m_ser) {
            if (m_inited != value) {
                m_inited = value;
                doEmit = true;
            }
        } else {
            if (m_inited != false) {
                m_inited = false;
                doEmit = true;
            }
        }
    }

    if (doEmit) {
        emit initedChanged();
    }
}

QString Service::getDeviceId() const {
    QMutexLocker lock{&m_mtx};
    auto id = m_ser && m_inited ? QString::fromStdString(m_ser->getDeviceId())
                                : QString();
    // qDebug() << "device id:" << id;
    return id;
}

QString Service::getDeviceIconPath() const {
    return Utils::deviceIconFilePath(getDeviceId());
}

QUrl Service::getDeviceUrl() const {
    QMutexLocker lock{&m_mtx};
    if (m_ser && m_inited)
        return QUrl{QString::fromStdString(m_ser->getActionURL())};
    return {};
}

QString Service::getDeviceFriendlyName() const {
    QMutexLocker lock{&m_mtx};
    auto name = m_ser && m_inited
                    ? QString::fromStdString(m_ser->getFriendlyName())
                    : QString();
    return name;
}

QString Service::getDeviceModel() const {
    QMutexLocker lock{&m_mtx};
    auto name = m_ser && m_inited
                    ? QString::fromStdString(m_ser->getModelName())
                    : QString();
    return name;
}

void Service::handleApplicationStateChanged(Qt::ApplicationState state) {
    qDebug() << "State changed:" << state;
}

void Service::deInit() {
    qDebug() << "Deiniting";

    setInited(false);
    m_initing = false;
    timer(false);
    reset();

    {
        QMutexLocker lock{&m_mtx};
        if (m_ser) {
            delete m_ser;
            m_ser = nullptr;
        }
    }

    setBusy(false);
}

bool Service::init(const QString &deviceId) {
    qDebug() << "Initing";

    if (deviceId.isEmpty()) {
        qWarning() << "To init deviceId must not be empty!";
        return false;
    }

    auto d = Directory::instance();

    UPnPClient::UPnPDeviceDesc ddesc;
    if (!d->getDeviceDesc(deviceId, ddesc)) {
        qWarning() << "Cannot find device description!";
        return false;
    }

    UPnPClient::UPnPServiceDesc sdesc;
    for (auto &_sdesc : ddesc.services) {
        if (_sdesc.serviceType == type()) {
            sdesc = _sdesc;
            break;
        }
    }

    if (sdesc.serviceId.empty()) {
        qWarning() << "Cannot find" << QString::fromStdString(type())
                   << "service description for" << deviceId << "device";
        return false;
    }

    {
        QMutexLocker lock{&m_mtx};
        if (m_inited && m_ser) {
            if (m_ser->getDeviceId() == ddesc.UDN &&
                m_ser->getServiceType() == sdesc.serviceType) {
                qDebug() << "Same service, init not needed";
                return true;
            }
        }
    }

    timer(false);
    setInited(false);

    reset();

    setBusy(true);

    startTask([this, ddesc, sdesc] {
        qDebug() << "Initing started";
        m_initing = true;

        {
            QMutexLocker lock{&m_mtx};

            if (m_ser) {
                delete m_ser;
                m_ser = nullptr;
            }

            m_ser = createUpnpService(ddesc, sdesc);

            if (!m_ser) {
                qWarning() << "Unable to connect to UPnP service";
                m_initing = false;
            }
        }
        if (!m_initing) {
            emit error(E_NotInited);
        }

        postInit();

        bool successfulInit = false;
        {
            QMutexLocker lock{&m_mtx};

            if (m_initing && m_ser) {
                m_ser->installReporter(this);
                successfulInit = true;
                m_initing = false;
            } else {
                qWarning() << "Cannot init service";
                if (m_ser) {
                    delete m_ser;
                    m_ser = nullptr;
                }
            }
        }
        if (successfulInit) {
            setInited(true);
        }

        setBusy(false);
    });

    return true;
}

void Service::reset() {}

void Service::postInit() {}

void Service::postDeInit() {}

void Service::timerEvent() {}

void Service::timer(bool start) {
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

bool Service::handleError(int ret) {
    // qDebug() << "handleError:" << ret;

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
            case -113:  // libnpupnp on *any curl error* generates
                        // UPNP_E_BAD_RESPONSE :(
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
