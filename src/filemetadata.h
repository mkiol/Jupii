/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FILEMETADATA_H
#define FILEMETADATA_H

#include <QString>

#include "contentserver.h"

struct FileMetaData {
    QString path;
    QString filename;
    qint64 size = 0;
    QString date;
    QString icon;
    ContentServer::Type type;

    FileMetaData();
    void clear();
    [[nodiscard]] bool empty() const;
};

#endif // FILEMETADATA_H
