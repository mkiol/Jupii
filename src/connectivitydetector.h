/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONNECTIVITYDETECTOR_H
#define CONNECTIVITYDETECTOR_H

#include <QNetworkConfigurationManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "singleton.h"

class ConnectivityDetector : public QObject,
                             public Singleton<ConnectivityDetector> {
    Q_OBJECT

    Q_PROPERTY(
        bool networkConnected READ networkConnected NOTIFY networkStateChanged)

   public:
    ConnectivityDetector(QObject *parent = nullptr);
    bool networkConnected() const;
    bool selectNetworkIf(QString &ifname, QString &address) const;

   public slots:
    void update();

   signals:
    void networkStateChanged();

   private:
    std::unique_ptr<QNetworkConfigurationManager> ncm;
    QString ifname;

    void handleNetworkConfChanged(const QNetworkConfiguration &conf);
    static void selectIfnameCandidates(QStringList &eth, QStringList &wlan);
    static QString fixAddress(const QString &address);
};

#endif  // CONNECTIVITYDETECTOR_H
