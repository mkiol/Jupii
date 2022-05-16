/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "dnscontentdeterminator.h"

#include <QDebug>
#include <QHostInfo>
#include <QHostAddress>
#include <QThread>

QString DnsDeterminator::hostName(const QString &host)
{
    QString resolvedName;

    auto info = QHostInfo::fromName(host);
    if ( info.error() != QHostInfo::NoError) {
        qWarning() << "Lookup A failed:" <<  info.errorString();
        return resolvedName;
    }

    const auto addresses = info.addresses();
    if (addresses.isEmpty()) {
        qWarning() << "No addresses";
        return resolvedName;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return resolvedName;

    info = QHostInfo::fromName(addresses.first().toString());
    if (info.error() != QHostInfo::NoError) {
        qWarning() << "Lookup PTR failed:" << info.errorString();
        return resolvedName;
    }

    resolvedName = info.hostName();

    return resolvedName;
}

DnsDeterminator::Type DnsDeterminator::type(const QUrl &url)
{
    auto name = hostName(url.host());

    qDebug() << "host name:" << name;

    if (name.contains(".bc.", Qt::CaseInsensitive)) return Type::Bc;
    return Type::Unknown;
}
