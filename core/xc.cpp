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
#include <QUrlQuery>
#include <QTimer>

#include "utils.h"
#include "devicemodel.h"
#include "yamahaxc.h"
#include "frontiersiliconxc.h"
#include "directory.h"

const int XC::TIMEOUT = 1000;

XC::XC(const QString &deviceId, const QString &address, QObject *parent)
    : QObject(parent),
      deviceId(deviceId),
      address(address)
{
}

bool XC::valid() const
{
    return ok;
}

bool XC::possible(const QString &deviceType)
{
    return deviceType.contains("MediaRenderer");
}

QUrl XC::makeApiCallUrl(const QString& path, const std::vector<param>& params) const
{
    if (params.empty())
        return QUrl(urlBase + path);

    QUrlQuery query;
    for (const auto& [key, value] : params)
        query.addQueryItem(key, value);

    QUrl url(urlBase + path);
    url.setQuery(query);

    return url;
}

void XC::apiCall(XC::Action action, const QString& path, const std::vector<param>& params, const QVariant& userData, int timeout)
{
    auto url = makeApiCallUrl(path, params);
    qDebug() << "XC API call:" << url.toString();

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::Attribute::FollowRedirectsAttribute, true);

    auto reply = Utils::instance()->nam->get(request);
    reply->setProperty("action", action);
    reply->setProperty("userData", userData);

    QTimer::singleShot(timeout, reply, &QNetworkReply::abort);

    connect(reply, &QNetworkReply::finished, this, &XC::handleActionFinished);
}

void XC::handleActionFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    auto action = reply->property("action").value<XC::Action>();

    qDebug() << "XC API call finished:" << reply->url().toString();

    if (reply->error() == QNetworkReply::NetworkError::NoError) {
        auto d = reply->readAll();
        if (!d.isEmpty()) {
            qDebug() << "XC API call response:" << d;
            switch (action) {
            case Action::ACTION_POWER_ON:
                handleActionPowerOn(d, reply->property("userData"));
                break;
            case Action::ACTION_POWER_OFF:
                handleActionPowerOff(d, reply->property("userData"));
                break;
            case Action::ACTION_POWER_TOGGLE:
                handleActionPowerToggle(d, reply->property("userData"));
                break;
            case Action::ACTION_GET_STATUS:
                DeviceModel::instance()->
                        updatePower(deviceId, handleActionGetStatus(d, reply->property("userData")) == Status::STATUS_POWER_ON);
                break;
            case Action::ACTION_CREATE_SESSION:
                handleActionCreateSession(d, reply->property("userData"));
                break;
            default:
                break;
            }
        } else {
            qWarning() << "Empty data received for XC call";
        }
    } else {
        qWarning() << "Network error in XC call:" << reply->error() << reply->errorString();
        handleActionError(action, reply->error(), reply->property("userData"));
    }

    reply->deleteLater();
    reply = nullptr;
}

XC::Status XC::handleActionGetStatus(const QByteArray&, const QVariant&)
{
    return Status::STATUS_UNKNOWN;
}

void XC::handleActionPowerToggle(const QByteArray&, const QVariant&)
{
}

void XC::handleActionCreateSession(const QByteArray&, const QVariant&)
{
}

void XC::handleActionPowerOn(const QByteArray&, const QVariant&)
{
}

void XC::handleActionPowerOff(const QByteArray&, const QVariant&)
{
}

void XC::handleActionError(XC::Action, QNetworkReply::NetworkError, const QVariant&)
{
}

std::shared_ptr<XC> XC::make_shared(const QString& deviceId, const QString &address, const QString& desc)
{
    if (std::shared_ptr<XC> yxc = std::make_shared<YamahaXC>(deviceId, desc);
            yxc->valid())
        return yxc;

    if (std::shared_ptr<XC> fsapi = std::make_shared<FrontierSiliconXC>(deviceId, address);
            fsapi->valid())
        return fsapi;

    return {};
}
