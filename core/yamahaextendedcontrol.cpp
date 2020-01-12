/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QXmlStreamReader>
#include <QStringRef>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>

#include "yamahaextendedcontrol.h"
#include "directory.h"
#include "devicemodel.h"

const QString YamahaXC::urlBase_tag = "yamaha:X_URLBase";
const QString YamahaXC::controlUrl_tag = "yamaha:X_yxcControlURL";

bool YamahaXC::parse(const QString &desc)
{
    QXmlStreamReader xml(desc);
    QString* elm = nullptr;

    while (!xml.atEnd()) {
        auto type = xml.readNext();
        if (type == QXmlStreamReader::StartElement) {
            if (xml.qualifiedName() == urlBase_tag)
                elm = &data.urlBase;
            else if (xml.qualifiedName() == controlUrl_tag)
                elm = &data.controlUrl;
        } else if (elm && type == QXmlStreamReader::Characters)
            *elm = xml.text().toString();
        else if (type == QXmlStreamReader::EndElement)
             elm = nullptr;
    }

    if (xml.hasError()) {
        qWarning() << "XML parsing error";
        return false;
    }

    //qDebug() << "YXC:" << urlBase << controlUrl;

    data.valid = !data.urlBase.isEmpty() && !data.controlUrl.isEmpty();
    if (data.urlBase.endsWith('/'))
        data.urlBase = data.urlBase.left(data.urlBase.length() - 1);

    return true;
}

YamahaXC::YamahaXC(const QString &deviceId, const QString& desc, QObject *parent) :
    QObject(parent), deviceId(deviceId)
{
    parse(desc);
}

bool YamahaXC::valid()
{
    return !data.urlBase.isEmpty() && !data.controlUrl.isEmpty();
}

void YamahaXC::apiCall(YamahaXC::Action action, const QString& method)
{
    if (reply && reply->isRunning()) {
        qDebug() << "Previous reply is running, so aborting";
        reply->abort();
    }

    QUrl url(data.urlBase + data.controlUrl + method);
    qDebug() << "YamahaXC API call:" << url.toString() << action;
    this->action = action;
    reply = Directory::instance()->nm->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, &YamahaXC::handleFinished);
}

void YamahaXC::powerToggle()
{
    qDebug() << "Power toggle";

    if (valid()) {
        apiCall(POWER_TOGGLE, "main/setPower?power=toggle");
    }
}

void YamahaXC::handleFinished()
{
    qDebug() << "XC API call finished:" << deviceId << reply->error();

    if (reply->error() == QNetworkReply::NoError) {
        auto d = reply->readAll();
        if (!d.isEmpty()) {
            QJsonParseError err;
            auto json = QJsonDocument::fromJson(d, &err);
            if (err.error == QJsonParseError::NoError) {
                if (json.isObject()) {
                    auto obj = json.object();
                    if (action == GET_STATUS) {
                        handleGetStatus(obj);
                    }
                } else {
                    qWarning() << "Json is not a object";
                }
            } else {
                qWarning() << "Error parsing json:" << err.errorString();
            }
        } else {
            qWarning() << "Empty data received for XC call";
        }
    } else {
        qWarning() << "Network error in XC call:" << reply->errorString();
    }

    reply->deleteLater();
    reply = nullptr;
}

void YamahaXC::handleGetStatus(const QJsonObject &obj)
{
    bool power = obj["power"].toString() == "on";
    qDebug() << "Power status for" << deviceId << "is" << power;
    DeviceModel::instance()->updatePower(deviceId, power);
}

void YamahaXC::getStatus()
{
    qDebug() << "Get status";

    if (valid())
        apiCall(GET_STATUS, "main/getStatus");
}
