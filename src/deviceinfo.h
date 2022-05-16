/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <libupnpp/control/description.hxx>


class DeviceInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY (QString udn READ getUdn WRITE setUdn NOTIFY dataChanged)
    Q_PROPERTY (QString friendlyName READ getFriendlyName NOTIFY dataChanged)
    Q_PROPERTY (QString deviceType READ getDeviceType NOTIFY dataChanged)
    Q_PROPERTY (QString modelName READ getModelName NOTIFY dataChanged)
    Q_PROPERTY (QString urlBase READ getUrlBase NOTIFY dataChanged)
    Q_PROPERTY (QString manufacturer READ getManufacturer NOTIFY dataChanged)
    Q_PROPERTY (QUrl icon READ getIcon NOTIFY dataChanged)
    Q_PROPERTY (QStringList services READ getServices NOTIFY dataChanged)
    Q_PROPERTY (QString xc READ getXc NOTIFY dataChanged)

public:
    explicit DeviceInfo(QObject *parent = nullptr);

    Q_INVOKABLE QString getXML() const;

    void setUdn(const QString& value);
    QString getUdn() const;
    QString getFriendlyName() const;
    QString getDeviceType() const;
    QString getModelName() const;
    QString getUrlBase() const;
    QString getManufacturer() const;
    QUrl getIcon() const;
    QStringList getServices() const;
    QString getXc() const;

signals:
    void dataChanged();

private:
    QString m_udn;
    UPnPClient::UPnPDeviceDesc m_ddesc;
};

#endif // DEVICEINFO_H
