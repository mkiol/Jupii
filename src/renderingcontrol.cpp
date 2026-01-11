/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "renderingcontrol.h"

#include <QDebug>
#include <QGuiApplication>
#include <QThread>

#include "devicemodel.h"
#include "notifications.h"
#include "settings.h"
#include "utils.h"

RenderingControl::RenderingControl(QObject *parent)
    : Service(parent), m_volumeTimer(parent) {
    m_volumeTimer.setInterval(500);
    m_volumeTimer.setSingleShot(true);
    QObject::connect(&m_volumeTimer, &QTimer::timeout, this,
                     &RenderingControl::volumeTimeout);

#ifdef USE_SFOS
    m_volumeUpTimer.setInterval(500);
    m_volumeUpTimer.setSingleShot(true);
    QObject::connect(&m_volumeUpTimer, &QTimer::timeout, this,
                     &RenderingControl::volumeUpTimeout);
#endif
}

void RenderingControl::changed(const QString &name, const QVariant &_value) {
    if (!isInitedOrIniting()) {
        qWarning() << "RenderingControl service is not inited";
        return;
    }

    if (_value.canConvert(QVariant::Int)) {
        int value = _value.toInt();

        if (name == "Volume") {
            if (m_volume != value) {
                m_volume = value;
                emit volumeChanged();
            }
        } else if (name == "Mute") {
            if (m_mute != value) {
                m_mute = value;
                emit muteChanged();
            }
        }
    }
}

UPnPClient::Service *RenderingControl::createUpnpService(
    const UPnPClient::UPnPDeviceDesc &ddesc,
    const UPnPClient::UPnPServiceDesc &sdesc) {
    return new UPnPClient::RenderingControl(ddesc, sdesc);
}

void RenderingControl::postInit() { update(); }

void RenderingControl::reset() {
    m_volume = 0;
    m_futureVolume = 0;
    m_mute = false;
    emit volumeChanged();
    emit muteChanged();
}

void RenderingControl::update() {
    auto app = static_cast<QGuiApplication *>(QGuiApplication::instance());

    if (app->applicationState() == Qt::ApplicationActive) {
        updateVolume();
        updateMute();
    }
}

void RenderingControl::asyncUpdate() {
    if (!isInitedOrIniting()) {
        qWarning() << "RenderingControl service is not inited";
        return;
    }

    startTask([this]() { update(); });
}

std::string RenderingControl::type() const {
    return "urn:schemas-upnp-org:service:RenderingControl:1";
}

UPnPClient::RenderingControl *RenderingControl::s() {
    if (m_ser == nullptr) {
        qWarning() << "RenderingConrol is not inited!";
        // emit error(E_NotInited);
        return nullptr;
    }

    return static_cast<UPnPClient::RenderingControl *>(m_ser);
}

int RenderingControl::getVolume() { return m_volume; }

void RenderingControl::setVolume(int value) {
    m_futureVolume = value;
    m_volumeTimer.start();
}

void RenderingControl::volumeTimeout() {
    if (!getInited()) {
        qWarning() << "RenderingControl service is not inited";
        return;
    }

    if (m_volume != m_futureVolume) {
        startTask([this]() {
            int ret = 0;
            {
                QMutexLocker lock{&m_mtx};
                auto srv = s();

                if (!getInited() || !srv) {
                    qWarning() << "RenderingControl service is not inited";
                    return;
                }

                ret = srv->setVolume(m_futureVolume);
            }

            if (handleError(ret)) {
                m_volume = m_futureVolume;

                tsleep();
                updateVolume();

                emit volumeChanged();
            }

            m_futureVolume = 0;
        });
    }
}

bool RenderingControl::getMute() { return m_mute; }

void RenderingControl::setMute(bool value) {
    if (m_mute != value) {
        startTask([this, value]() {
            int ret = 0;
            {
                QMutexLocker lock{&m_mtx};
                auto srv = s();

                if (!getInited() || !srv) {
                    qWarning() << "RenderingControl service is not inited";
                    return;
                }

                ret = srv->setMute(value);
            }

            if (handleError(ret)) {
                m_mute = value;
                emit muteChanged();
            }
        });
    }
}

void RenderingControl::handleApplicationStateChanged(Qt::ApplicationState) {
    update();
}

void RenderingControl::asyncUpdateVolume() {
    if (!isInitedOrIniting()) {
        qWarning() << "RenderingControl service is not inited";
        return;
    }

    startTask([this]() { updateVolume(); });
}

void RenderingControl::updateVolume() {
    int ret = 0;
    {
        QMutexLocker lock{&m_mtx};
        auto srv = s();

        if (!isInitedOrIniting() || !srv) {
            qWarning() << "RenderingControl service is not inited";
            return;
        }

        ret = srv->getVolume();
    }

    if (handleError(ret)) {
        // qDebug() << "Volume:" << ret;
        if (m_volume != ret) {
            m_volume = ret;
            emit volumeChanged();
        }
    } else {
        qWarning() << "Error response for getVolume()";
    }
}

void RenderingControl::asyncUpdateMute() {
    if (!getInited()) {
        qWarning() << "RenderingControl service is not inited";
        return;
    }

    startTask([this]() { updateMute(); });
}

void RenderingControl::updateMute() {
    bool m = false;
    {
        QMutexLocker lock{&m_mtx};

        auto srv = s();

        if (!isInitedOrIniting() || !srv) {
            qWarning() << "RenderingControl service is not inited";
            return;
        }

        m = srv->getMute();
    }

    if (m_mute != m) {
        m_mute = m;
        emit muteChanged();
    }
}

#ifdef USE_SFOS
void RenderingControl::showVolNofification() const {
    auto name = getDeviceFriendlyName().isEmpty() ? tr("Remote device")
                                                  : getDeviceFriendlyName();
    Notifications::instance()->show(
        tr("Volume level of %1 is %2").arg(name).arg(m_volume), {},
        m_volume > 0 ? "icon-system-volume" : "icon-system-volume-mute");
}

void RenderingControl::volumeUpTimeout() {
    if (m_volUpMutex.tryLock()) {
        startTask([this]() {
            int ret = 0;
            {
                QMutexLocker lock{&m_mtx};

                auto srv = s();

                if (!getInited() || !srv) {
                    qWarning() << "RenderingControl service is not inited";
                    m_volUpMutex.unlock();
                    return;
                }

                ret = srv->setVolume(m_futureVolume);
            }

            if (handleError(ret)) {
                m_volume = m_futureVolume;
                emit volumeChanged();
                showVolNofification();
            }

            m_futureVolume = 0;
            m_volUpMutex.unlock();
        });
    }
}

void RenderingControl::volUpPressed() {
    qDebug() << "Volume up pressed";

    if (m_volUpMutex.tryLock()) {
        m_volUpMutex.unlock();
        auto step = Settings::instance()->getVolStep();
        auto vol = step + (m_futureVolume > 0 ? m_futureVolume : getVolume());
        vol -= vol % step;
        m_futureVolume = vol > 100 ? 100 : vol;
        m_volumeUpTimer.start();
    }
}

void RenderingControl::volDownPressed() {
    qDebug() << "Volume down pressed";

    if (m_volUpMutex.tryLock()) {
        m_volUpMutex.unlock();
        auto step = Settings::instance()->getVolStep();
        auto vol = (m_futureVolume > 0 ? m_futureVolume : getVolume()) - step;
        vol += vol % step;
        m_futureVolume = vol < 0 ? 0 : vol;
        m_volumeUpTimer.start();
    }
}
#endif
