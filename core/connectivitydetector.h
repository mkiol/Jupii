/* Copyright (C) 2017-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONNECTIVITYDETECTOR_H
#define CONNECTIVITYDETECTOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkConfigurationManager>
#include <memory>

class ConnectivityDetector : public QObject
{
    Q_OBJECT

    Q_PROPERTY (bool networkConnected READ networkConnected NOTIFY networkStateChanged)

public:
    static ConnectivityDetector* instance(QObject *parent = nullptr);
    ConnectivityDetector(const ConnectivityDetector&) = delete;
    ConnectivityDetector& operator= (const ConnectivityDetector&) = delete;
    bool networkConnected() const;
    bool selectNetworkIf(QString &ifname, QString &address) const;

public slots:
    void update();

signals:
    void networkStateChanged();

private:
    static ConnectivityDetector* m_instance;
    std::unique_ptr<QNetworkConfigurationManager> ncm;
    QString ifname;

    explicit ConnectivityDetector(QObject *parent = nullptr);
    void handleNetworkConfChanged(const QNetworkConfiguration &conf);
    static void selectIfnameCandidates(QStringList& eth, QStringList& wlan);
};

#endif // CONNECTIVITYDETECTOR_H
