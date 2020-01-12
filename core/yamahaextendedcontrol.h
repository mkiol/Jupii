/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef YAMAHAEXTENDEDCONTROL_H
#define YAMAHAEXTENDEDCONTROL_H

#include <QObject>
#include <QDebug>
#include <QString>
#include <QNetworkReply>
#include <QJsonObject>

#include <memory>

class YamahaXC : public QObject
{
    Q_OBJECT

public:
    struct Data
    {
        bool valid = false;
        QString urlBase;
        QString controlUrl;
    };
    struct Status
    {
        bool power;
        int volume;
    };

    YamahaXC(const QString &deviceId, const QString& desc, QObject *parent = nullptr);

    bool valid();
    void powerToggle();
    void getStatus();

private slots:
    void handleFinished();

private:
    enum Action {
        UNKNOWN,
        POWER_TOGGLE,
        GET_STATUS
    };
    static const QString urlBase_tag;
    static const QString controlUrl_tag;
    Data data;
    QString deviceId;
    QNetworkReply *reply = nullptr;
    Action action = UNKNOWN;

    void apiCall(Action action, const QString& method);
    bool parse(const QString& desc);

    void handleGetStatus(const QJsonObject& obj);
};

#endif // YAMAHAEXTENDEDCONTROL_H
