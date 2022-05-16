/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FRONTIERSILICONXC_H
#define FRONTIERSILICONXC_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QDomDocument>

#include "xc.h"

class FrontierSiliconXC : public XC
{
    Q_OBJECT
public:
    FrontierSiliconXC(const QString &deviceId, const QString &address, QObject *parent = nullptr);

    void powerOn();
    void powerOff();
    void powerToggle();
    void getStatus();
    QString name() const;

private:
    static const QString URL;
    static const int timeout;
    QString sid;

    static QDomDocument parse(const QByteArray& data);
    Status handleActionGetStatus(const QByteArray& data, const QVariant& userData = {});
    void handleActionCreateSession(const QByteArray &data, const QVariant& userData);
    void handleActionError(XC::Action action, QNetworkReply::NetworkError error, const QVariant& userData = {});
    bool init();
    bool parseDeviceDescription(const QByteArray& data);
    void createSession(Action action);
};

#endif // FRONTIERSILICONXC_H
