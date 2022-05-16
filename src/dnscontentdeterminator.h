/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DNSCONTENTDETERMINATOR_H
#define DNSCONTENTDETERMINATOR_H

#include <QUrl>
#include <QString>

class DnsDeterminator
{
public:
    enum class Type {
        Unknown, Bc
    };

    static Type type(const QUrl &url);

private:
    static QString hostName(const QString &host);

};

#endif // DNSCONTENTDETERMINATOR_H
