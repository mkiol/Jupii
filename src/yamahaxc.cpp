/* Copyright (C) 2018-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "yamahaxc.h"

#include <QXmlStreamReader>
#include <QString>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>

const QString YamahaXC::urlBase_tag = "yamaha:X_URLBase";
const QString YamahaXC::controlUrl_tag = "yamaha:X_yxcControlURL";

bool YamahaXC::init(const QString &desc)
{
    QXmlStreamReader xml{desc};
    QString* elm = nullptr;
    QString urlBase;
    QString controlUrl;

    while (!xml.atEnd()) {
        auto type = xml.readNext();
        if (type == QXmlStreamReader::TokenType::StartElement) {
            if (xml.qualifiedName() == urlBase_tag)
                elm = &urlBase;
            else if (xml.qualifiedName() == controlUrl_tag)
                elm = &controlUrl;
        } else if (elm && type == QXmlStreamReader::TokenType::Characters)
            *elm = xml.text().toString();
        else if (type == QXmlStreamReader::TokenType::EndElement)
             elm = nullptr;
    }

    if (xml.hasError() || urlBase.isEmpty() || controlUrl.isEmpty()) {
        //qWarning() << "XML parsing error:" << desc;
        return false;
    }

    if (urlBase.endsWith('/'))
        urlBase = urlBase.left(urlBase.length() - 1);

    this->urlBase = urlBase + controlUrl;

    return true;
}

YamahaXC::YamahaXC(const QString &deviceId, const QString& desc, QObject *parent) :
    XC(deviceId, QString(), parent)
{
    ok = init(desc);
}

QString YamahaXC::name() const
{
    return "Yamaha Extended Control";
}

void YamahaXC::powerOn()
{
    qDebug() << "Power on";

    if (valid())
        apiCall(Action::ACTION_POWER_ON, "main/setPower?power=on");
}

void YamahaXC::powerOff()
{
    qDebug() << "Power off";

    if (valid())
        apiCall(Action::ACTION_POWER_OFF, "main/setPower?power=standby");
}

void YamahaXC::powerToggle()
{
    qDebug() << "Power toggle";

    if (valid())
        apiCall(Action::ACTION_POWER_TOGGLE, "main/setPower?power=toggle");
}

void YamahaXC::getStatus()
{
    qDebug() << "Get status";

    if (valid())
        apiCall(Action::ACTION_GET_STATUS, "main/getStatus");
}

XC::Status YamahaXC::handleActionGetStatus(const QByteArray& data, const QVariant&)
{
    QJsonParseError err;

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::ParseError::NoError || !json.isObject()) {
        qWarning() << "Error parsing json:" << err.errorString();
        return Status::STATUS_UNKNOWN;
    }

    return json.object()["power"].toString() == "on" ?
            Status::STATUS_POWER_ON : Status::STATUS_POWER_OFF;
}
