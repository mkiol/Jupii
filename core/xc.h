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
#include <memory>

class QNetworkReply;

class XC : public QObject
{
    Q_OBJECT
public:
    enum Action {
        ACTION_UNKNOWN,
        ACTION_POWER_TOGGLE,
        ACTION_GET_STATUS
    };
    enum Status {
        STATUS_UNKNOWN,
        STATUS_POWER_ON,
        STATUS_POWER_OFF
    };

    static std::shared_ptr<XC> make(const QString& deviceId, const QString& desc);

    explicit XC(const QString& deviceId, QObject *parent = nullptr);

    virtual bool valid() const;
    virtual void powerToggle() = 0;
    virtual void getStatus() = 0;
    virtual QString name() const = 0;

protected:
    bool ok = true;
    QString deviceId;
    QString urlBase;

    virtual QUrl makeApiCallUrl(const QString& path) const;
    virtual Status handleGetStatus(const QByteArray& data);
    virtual void handlePowerToggle(const QByteArray& data);

    void apiCall(Action action, const QString& path);

private slots:
    void handleActionFinished();

private:
    QNetworkReply* reply = nullptr;
};

Q_DECLARE_METATYPE(XC::Action)

#endif // XC_H
