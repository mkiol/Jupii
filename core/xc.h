/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef XC_H
#define XC_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QVariant>
#include <QNetworkReply>
#include <memory>
#include <vector>
#include <utility>

class QNetworkReply;

class XC : public QObject
{
    Q_OBJECT
public:
    enum Action {
        ACTION_UNKNOWN,
        ACTION_POWER_ON,
        ACTION_POWER_OFF,
        ACTION_POWER_TOGGLE,
        ACTION_GET_STATUS,
        ACTION_CREATE_SESSION
    };
    enum Status {
        STATUS_UNKNOWN,
        STATUS_POWER_ON,
        STATUS_POWER_OFF
    };

    using param = std::pair<QString,QString>;

    static std::shared_ptr<XC> make_shared(const QString& deviceId, const QString &address, const QString& desc);

    explicit XC(const QString& deviceId, const QString& address = QString(), QObject *parent = nullptr);

    virtual bool valid() const;
    virtual void powerToggle() = 0;
    virtual void getStatus() = 0;
    virtual void powerOn() = 0;
    virtual void powerOff() = 0;
    virtual QString name() const = 0;

protected:
    bool ok = true;
    QString deviceId;
    QString urlBase;
    QString address;

    virtual QUrl makeApiCallUrl(const QString& path, const std::vector<param>& params) const;
    virtual Status handleActionGetStatus(const QByteArray& data, const QVariant& userData = {});
    virtual void handleActionPowerToggle(const QByteArray& data, const QVariant& userData = {});
    virtual void handleActionPowerOn(const QByteArray& data, const QVariant& userData = {});
    virtual void handleActionPowerOff(const QByteArray& data, const QVariant& userData = {});
    virtual void handleActionCreateSession(const QByteArray& data, const QVariant& userData = {});
    virtual void handleActionError(Action action, QNetworkReply::NetworkError error, const QVariant& userData = {});

    void apiCall(Action action, const QString& path,
                 const std::vector<param>& params = {},
                 const QVariant& userData = {}, int timeout = TIMEOUT);

private slots:
    void handleActionFinished();

private:
    static const int TIMEOUT;
};

Q_DECLARE_METATYPE(XC::Action)

#endif // XC_H
