/* Copyright (C) 2019-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "contentdirectoryservice.h"

ContentDirectoryService::ContentDirectoryService(const std::string& stp,
                                                 const std::string& sid,
                                                 UPnPProvider::UpnpDevice* dev)
    : UPnPProvider::UpnpService(stp, sid, "CD", dev) {}

std::string ContentDirectoryService::systemUpdateId() const {
    return std::to_string(updateCounter);
}

void ContentDirectoryService::update() {
    ++updateCounter;
    updateNeeded = true;
}

bool ContentDirectoryService::getEventData([[maybe_unused]] bool all,
                                           std::vector<std::string>& names,
                                           std::vector<std::string>& values) {
    if (updateNeeded) {
        names.push_back("SystemUpdateID");
        values.push_back(systemUpdateId());
        updateNeeded = false;
    }
    return true;
}
