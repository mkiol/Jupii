/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QThread>
#include <QGuiApplication>

#include "renderingcontrol.h"
#include "directory.h"
#include "devicemodel.h"

RenderingControl::RenderingControl(QObject *parent) : Service(parent),
    m_volumeTimer(parent)
{
    m_volumeTimer.setInterval(500);
    m_volumeTimer.setSingleShot(true);
    QObject::connect(&m_volumeTimer, &QTimer::timeout, this, &RenderingControl::volumeTimeout);
}

void RenderingControl::changed(const QString &name, const QVariant &_value)
{
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

UPnPClient::Service* RenderingControl::createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                                         const UPnPClient::UPnPServiceDesc &sdesc)
{
    return new UPnPClient::RenderingControl(ddesc, sdesc);
}

void RenderingControl::postInit()
{
    update();
}

void RenderingControl::postDeInit()
{
    m_volume = 0;
    m_mute = false;
    emit volumeChanged();
    emit muteChanged();
}

void RenderingControl::update()
{
    auto app = static_cast<QGuiApplication*>(QGuiApplication::instance());

    if (app->applicationState() == Qt::ApplicationActive) {
        updateVolume();
        updateMute();
    }
}

void RenderingControl::asyncUpdate()
{
    startTask([this](){
        update();
    });
}

std::string RenderingControl::type() const
{
    return "urn:schemas-upnp-org:service:RenderingControl:1";
}

UPnPClient::RenderingControl* RenderingControl::s()
{
    if (m_ser == nullptr) {
        qWarning() << "RenderingConrol is not inited!";
        //emit error(E_NotInited);
        return nullptr;
    }

    return static_cast<UPnPClient::RenderingControl*>(m_ser);
}


int RenderingControl::getVolume()
{
    return m_volume;
}

void RenderingControl::setVolume(int value)
{
    m_futureVolume = value;
    m_volumeTimer.start();
}

void RenderingControl::volumeTimeout()
{
    if (m_volume != m_futureVolume) {
        startTask([this](){
            auto srv = s();
            if (srv == nullptr)
                return;

            if (handleError(srv->setVolume(m_futureVolume))) {
                m_volume = m_futureVolume;

                tsleep();
                updateVolume();

                emit volumeChanged();
            }
        });
    }
}

bool RenderingControl::getMute()
{
    return m_mute;
}

void RenderingControl::setMute(bool value)
{
    if (m_mute != value) {
        startTask([this, value](){
            auto srv = s();
            if (srv == nullptr)
                return;

            if (handleError(srv->setMute(value))) {
                m_mute = value;
                emit muteChanged();
            }
        });
    }
}

void RenderingControl::handleApplicationStateChanged(Qt::ApplicationState state)
{
    Q_UNUSED(state)
    update();
}

void RenderingControl::asyncUpdateVolume()
{
    startTask([this](){
        updateVolume();
    });
}

void RenderingControl::updateVolume()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    int v = srv->getVolume();

    if (handleError(v)) {
        qDebug() << "Volume:" << v;
        if (m_volume != v) {
            m_volume = v;
            emit volumeChanged();
        }
    } else {
        qWarning() << "Error response for getVolume()";
    }
}

void RenderingControl::asyncUpdateMute()
{
    startTask([this](){
        updateMute();
    });
}

void RenderingControl::updateMute()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    bool m = srv->getMute();

    if (m_mute != m) {
        m_mute = m;
        emit muteChanged();
    }
}
