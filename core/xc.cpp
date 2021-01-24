/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "xc.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include "utils.h"
#include "devicemodel.h"
#include "yamahaxc.h"

XC::XC(const QString &deviceId, QObject *parent)
    : QObject(parent),
      deviceId(deviceId)
{
}

bool XC::valid() const
{
    return ok;
}

QUrl XC::makeApiCallUrl(const QString& path) const
{
    return QUrl(urlBase + path);
}

void XC::apiCall(XC::Action action, const QString& path)
{
    if (reply && reply->isRunning()) {
        qDebug() << "Previous reply is running, so aborting";
        reply->abort();
    }

    auto url = makeApiCallUrl(path);
    qDebug() << "XC API call:" << url.toString();

    auto reply = Utils::instance()->nam->get(QNetworkRequest(url));
    reply->setProperty("action", action);

    connect(reply, &QNetworkReply::finished, this, &XC::handleActionFinished);
}

void XC::handleActionFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    auto action = reply->property("action").value<XC::Action>();

    qDebug() << "XC API call finished:" << reply->url().toString();

    if (reply->error() == QNetworkReply::NoError) {
        auto d = reply->readAll();
        if (!d.isEmpty()) {
            switch (action) {
            case ACTION_POWER_TOGGLE:
                handleGetStatus(d);
                break;
            case ACTION_GET_STATUS:
                DeviceModel::instance()->updatePower(deviceId, handleGetStatus(d) == STATUS_POWER_ON);
                break;
            default:
                break;
            }
        } else {
            qWarning() << "Empty data received for XC call";
        }
    } else {
        qWarning() << "Network error in XC call:" << reply->error() << reply->errorString();
    }

    reply->deleteLater();
    reply = nullptr;
}

XC::Status XC::handleGetStatus(const QByteArray& data)
{
    Q_UNUSED(data)
    return STATUS_UNKNOWN;
}

void XC::handlePowerToggle(const QByteArray& data)
{
    Q_UNUSED(data)
}

std::shared_ptr<XC> XC::make(const QString& deviceId, const QString& desc)
{
    return std::make_shared<YamahaXC>(deviceId, desc);
}
