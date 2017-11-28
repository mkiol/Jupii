/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef RENDERINGCONTROL_H
#define RENDERINGCONTROL_H

#include <QTimer>

#include <libupnpp/control/renderingcontrol.hxx>
#include <libupnpp/control/service.hxx>

#include "service.h"

class RenderingControl : public Service
{
    Q_OBJECT
    Q_PROPERTY (int volume READ getVolume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY (bool mute READ getMute NOTIFY muteChanged)

public:
    explicit RenderingControl(QObject* parent = nullptr);

    int getVolume();
    void setVolume(int value);
    bool getMute();
    Q_INVOKABLE void setMute(bool value);

signals:
    void volumeChanged();
    void muteChanged();

public slots:
    void asyncUpdate();

private slots:
    void volumeTimeout();
    void handleApplicationStateChanged(Qt::ApplicationState state);

private:
    int m_volume = 50;
    bool m_mute = false;

    QTimer m_volumeTimer;
    int m_futureVolume = 0;

    void changed(const QString &name, const QVariant &value);
    UPnPClient::Service* createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                           const UPnPClient::UPnPServiceDesc &sdesc);
    void postInit();
    void postDeInit();
    std::string type() const;

    UPnPClient::RenderingControl* s();

    void update();
    void updateMute();
    void updateVolume();
    void asyncUpdateMute();
    void asyncUpdateVolume();
};

#endif // RENDERINGCONTROL_H
