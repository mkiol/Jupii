/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "filemetadata.h"

FileMetaData::FileMetaData()
{
    clear();
}

void FileMetaData::clear() {
    path.clear();
    filename.clear();
    size = 0;
    date.clear();
    icon.clear();
    type = ContentServer::TypeUnknown;
}

bool FileMetaData::empty() const
{
    return path.isEmpty();
}
