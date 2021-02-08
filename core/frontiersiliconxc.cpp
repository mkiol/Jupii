/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "frontiersiliconxc.h"

#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>

#include "settings.h"

const QString FrontierSiliconXC::URL("http://%1:80/device"); // TODO: port & path should be provided via UI
const int FrontierSiliconXC::TIMEOUT = 1000;

FrontierSiliconXC::FrontierSiliconXC(const QString &deviceId, const QString &address, QObject *parent) :
    XC(deviceId, address, parent)
{
    qDebug() << "FrontierSiliconXC:" << deviceId << address;
    ok = init();
}

QString FrontierSiliconXC::name() const
{
    return "Frontier Silicon";
}

void FrontierSiliconXC::powerOn()
{
    qDebug() << "Power on";

    if (!valid())
        return;

    if (sid.isEmpty())
        createSession(Action::ACTION_POWER_ON);
    else
        apiCall(Action::ACTION_POWER_ON, "/SET/netRemote.sys.power",
                {{"pin", Settings::instance()->fsapiPin()}, {"sid", sid}, {"value", "1"}});
}

void FrontierSiliconXC::powerOff()
{
    qDebug() << "Power off";

    if (!valid())
        return;

    if (sid.isEmpty())
        createSession(Action::ACTION_POWER_OFF);
    else
        apiCall(Action::ACTION_POWER_OFF, "/SET/netRemote.sys.power",
                {{"pin", Settings::instance()->fsapiPin()}, {"sid", sid}, {"value", "0"}});
}

void FrontierSiliconXC::powerToggle()
{
    qDebug() << "Power toggle";

    if (!valid())
        return;

    if (sid.isEmpty())
        createSession(Action::ACTION_POWER_TOGGLE);
    else
        apiCall(Action::ACTION_GET_STATUS, "/GET/netRemote.sys.power",
                {{"pin", Settings::instance()->fsapiPin()}, {"sid", sid}}, Action::ACTION_POWER_TOGGLE);
}

void FrontierSiliconXC::getStatus()
{
    qDebug() << "Get status";

    if (!valid())
        return;

    if (sid.isEmpty())
        createSession(Action::ACTION_GET_STATUS);
    else
        apiCall(Action::ACTION_GET_STATUS, "/GET/netRemote.sys.power",
                {{"pin", Settings::instance()->fsapiPin()}, {"sid", sid}});
}

void FrontierSiliconXC::createSession(Action action)
{
    qDebug() << "Create session";

    apiCall(ACTION_CREATE_SESSION, "/CREATE_SESSION", {{"pin", Settings::instance()->fsapiPin()}}, action);
}

bool FrontierSiliconXC::downloadDeviceDescription(QByteArray& data)
{
    QNetworkRequest request;
    request.setUrl(QUrl(URL.arg(address)));
    request.setAttribute(QNetworkRequest::Attribute::FollowRedirectsAttribute, true);

    QNetworkAccessManager nam;
    auto reply = nam.get(request);

    QTimer::singleShot(TIMEOUT, reply, &QNetworkReply::abort);
    QEventLoop loop;

    connect(reply, &QNetworkReply::finished, this, [&loop, reply, &data] {
        auto err = reply->error();
        qDebug() << "FrontierSiliconXC finished";
        if (err == QNetworkReply::NetworkError::NoError) {
            data = reply->readAll();
            reply->deleteLater();
            loop.quit();
        } else {
            qWarning() << "FrontierSiliconXC error:" << err;
            reply->deleteLater();
            loop.exit(1);
        }
    });

    return loop.exec() == 0;
}

QDomDocument FrontierSiliconXC::parse(const QByteArray& data)
{
    QDomDocument doc;

    QString error;
    if (!doc.setContent(data, false, &error))
        qWarning() << "Parse error:" << error;

    return doc;
}

bool FrontierSiliconXC::parseDeviceDescription(const QByteArray& data)
{
    if (data.isEmpty())
        return false;

    auto doc = parse(data);

    if (doc.isNull())
        return false;

    urlBase = doc.elementsByTagName("webfsapi").item(0).toElement().text();

    if (urlBase.endsWith('/'))
        urlBase = urlBase.left(urlBase.length() - 1);

    qDebug() << "FrontierSiliconXC webfsapi:" << urlBase;

    return !urlBase.isEmpty();
}

bool FrontierSiliconXC::init()
{
    QByteArray data;

    if (!downloadDeviceDescription(data)) {
        qWarning() << "Cannot download device description";
        return false;
    }

    qDebug() << "FrontierSiliconXC data:" << data;

    if (!parseDeviceDescription(data)) {
        qWarning() << "Cannot parse device description:" << data;
        return false;
    }

    return true;
}

void FrontierSiliconXC::handleActionCreateSession(const QByteArray& data, const QVariant& userData)
{
    auto doc = parse(data);

    if (doc.isNull()) {
        qWarning() << "Cannot parse response:" << data;
        return;
    }

    sid = doc.elementsByTagName("sessionId").item(0).toElement().text();

    qDebug() << "FrontierSiliconXC sessionId:" << sid;

    if (sid.isEmpty()) {
        qWarning() << "Empty session id";
        return;
    }

    if (!userData.isNull()) {
        switch (userData.value<XC::Action>()) {
        case Action::ACTION_POWER_ON:
            powerOn();
            break;
        case Action::ACTION_POWER_OFF:
            powerOff();
            break;
        case Action::ACTION_POWER_TOGGLE:
            powerToggle();
            break;
        case Action::ACTION_GET_STATUS:
            getStatus();
            break;
        default:
            break;
        }
    }
}

void FrontierSiliconXC::handleActionError(XC::Action, QNetworkReply::NetworkError error, const QVariant&)
{
    if (error == QNetworkReply::NetworkError::ContentNotFoundError) {
        qWarning() << "Received 404, so most likely sid is invalid";
    } else if (error == QNetworkReply::NetworkError::ContentAccessDenied ||
               error == QNetworkReply::NetworkError::ContentOperationNotPermittedError) {
        qWarning() << "Received 403, so most likely pin is invalid";
        emit XC::error(XC::ErrorType::ERROR_INVALID_PIN);
    }

    sid.clear();
}

XC::Status FrontierSiliconXC::handleActionGetStatus(const QByteArray& data, const QVariant& userData)
{
    const auto doc = parse(data);

    if (doc.isNull()) {
        qWarning() << "Cannot parse response:" << data;
        return Status::STATUS_UNKNOWN;
    }

    const auto statusStr = doc.elementsByTagName("value").item(0).toElement()
            .elementsByTagName("u8").item(0).toElement().text();

    if (statusStr.isEmpty()) {
        qWarning() << "Empty power status";
        return Status::STATUS_UNKNOWN;
    }

    const auto status = statusStr == "0" ? Status::STATUS_POWER_OFF :
                        statusStr == "1" ? Status::STATUS_POWER_ON :
                                     Status::STATUS_UNKNOWN;

    if (!userData.isNull() &&
            userData.value<XC::Action>() == Action::ACTION_POWER_TOGGLE &&
            status != Status::STATUS_UNKNOWN) {
        apiCall(Action::ACTION_POWER_TOGGLE, "/SET/netRemote.sys.power",
                {{"pin", Settings::instance()->fsapiPin()}, {"sid", sid}, {"value", status == Status::STATUS_POWER_OFF ? "1" : "0"}});
    }

    return status;
}
