/* Copyright (C) 2018-2021 Michal Kosciesza <michal@mkiol.net>
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

#include "xc.h"

class YamahaXC : public XC
{
    Q_OBJECT
public:
    YamahaXC(const QString &deviceId, const QString& desc, QObject *parent = nullptr);

    void powerToggle();
    void getStatus();
    QString name() const;

private:
    static const QString urlBase_tag;
    static const QString controlUrl_tag;

    bool parse(const QString& desc);
    Status handleGetStatus(const QByteArray& data);
};

#endif // YAMAHAEXTENDEDCONTROL_H
