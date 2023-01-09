/* Copyright (C) 2019-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "connectionmanagerservice.h"

ConnectionManagerService::ConnectionManagerService(
    const std::string& stp, const std::string& sid,
    UPnPProvider::UpnpDevice* dev)
    : UPnPProvider::UpnpService(stp, sid, "CM", dev) {}

bool ConnectionManagerService::getEventData(
    [[maybe_unused]] bool all, [[maybe_unused]] std::vector<std::string>& names,
    [[maybe_unused]] std::vector<std::string>& values) {
    return true;
}
