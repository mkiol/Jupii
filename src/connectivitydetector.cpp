/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "connectivitydetector.h"

#include <QDebug>
#include <QNetworkConfiguration>
#include <QNetworkInterface>

#include "settings.h"
#include "utils.h"

ConnectivityDetector::ConnectivityDetector(QObject *parent)
    : QObject{parent},
      ncm{std::make_unique<QNetworkConfigurationManager>(parent)},
      ifname{"invalid"} {
    connect(ncm.get(), &QNetworkConfigurationManager::configurationAdded, this,
            &ConnectivityDetector::handleNetworkConfChanged);
    connect(ncm.get(), &QNetworkConfigurationManager::configurationRemoved,
            this, &ConnectivityDetector::handleNetworkConfChanged);
    connect(ncm.get(), &QNetworkConfigurationManager::configurationChanged,
            this, &ConnectivityDetector::handleNetworkConfChanged);
}

bool ConnectivityDetector::networkConnected() const {
    return !ifname.isEmpty();
}

void ConnectivityDetector::selectIfnameCandidates(
    QStringList &ethCandidates, QStringList &wlanCandidates,
    QStringList &localCandidates) {
    foreach (const auto &interface, QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !interface.addressEntries().isEmpty()) {
            if (Utils::ethNetworkInf(interface)) {
                // qDebug() << "eth interface:" << interface.name();
                ethCandidates << interface.name();
            } else if (Utils::wlanNetworkInf(interface)) {
                // qDebug() << "wlan interface:" << interface.name();
                wlanCandidates << interface.name();
            } else if (Utils::localNetworkInf(interface)) {
                // qDebug() << "local interface:" << interface.name();
                localCandidates << interface.name();
            }
        }
    }
}

void ConnectivityDetector::handleNetworkConfChanged(
    const QNetworkConfiguration &conf) {
    if (conf.state() & QNetworkConfiguration::Defined &&
        (conf.bearerType() == QNetworkConfiguration::BearerWLAN ||
         conf.bearerType() == QNetworkConfiguration::BearerEthernet)) {
        update();
    }
}

void ConnectivityDetector::update() {
    QStringList ethCandidates;
    QStringList wlanCandidates;
    QStringList localCandidates;

    selectIfnameCandidates(ethCandidates, wlanCandidates, localCandidates);

    QString newIfname;

    if (ethCandidates.isEmpty() && wlanCandidates.isEmpty() &&
        localCandidates.isEmpty()) {
        qWarning() << "no connected network interface found";
    } else {
        QString prefIfname = Settings::instance()->getPrefNetInf();
#ifdef QT_DEBUG
        qDebug() << "preferred network interface:" << prefIfname;
#endif

        if (!prefIfname.isEmpty() && (ethCandidates.contains(prefIfname) ||
                                      wlanCandidates.contains(prefIfname) ||
                                      localCandidates.contains(prefIfname))) {
            qDebug() << "preferred network interface found";
            newIfname = prefIfname;
        } else {
#ifdef USE_SFOS
            // preferred WLAN
            if (wlanCandidates.contains("tether")) {
                newIfname = "tether";
            } else if (!wlanCandidates.isEmpty()) {
                newIfname = wlanCandidates.first();
            } else if (!ethCandidates.isEmpty()) {
                newIfname = ethCandidates.first();
            } else {
                newIfname = localCandidates.first();
            }
#else
            // preferred Ethernet
            if (!ethCandidates.isEmpty()) {
                newIfname = ethCandidates.first();
            } else if (!wlanCandidates.isEmpty()) {
                newIfname = wlanCandidates.first();
            } else {
                newIfname = localCandidates.first();
            }
#endif
        }
    }

    if (ifname != newIfname) {
        qDebug() << "connected network interface changed:" << newIfname;
        ifname = newIfname;
        emit networkStateChanged();
    }
}

QString ConnectivityDetector::fixAddress(const QString &address) {
    if (const auto idx = address.indexOf('%'); idx != -1) {
        return address.left(idx);
    }

    return address;
}

bool ConnectivityDetector::selectNetworkIf(QString &ifname,
                                           QString &address) const {
    if (networkConnected()) {
        auto interface = QNetworkInterface::interfaceFromName(this->ifname);
        if (interface.isValid() &&
            interface.flags().testFlag(QNetworkInterface::IsUp) &&
            interface.flags().testFlag(QNetworkInterface::IsRunning)) {
            ifname = this->ifname;

            auto addra = interface.addressEntries();
            for (const auto &a : addra) {
                auto ha = a.ip();
                if (ha.protocol() == QAbstractSocket::IPv4Protocol ||
                    ha.protocol() == QAbstractSocket::IPv6Protocol) {
                    address = fixAddress(ha.toString());

                    return true;
                }
            }

            qWarning() << "cannot find valid ip addr for interface:"
                       << this->ifname;
        }
    }

    return false;
}
