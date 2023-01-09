/* Copyright (C) 2019-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONNECTIONMANAGERSERVICE_H
#define CONNECTIONMANAGERSERVICE_H

#include <libupnpp/device/service.hxx>
#include <string>
#include <vector>

namespace UPnPProvider {
class UpnpDevice;
}

class ConnectionManagerService : public UPnPProvider::UpnpService {
   public:
    ConnectionManagerService(const std::string& stp, const std::string& sid,
                             UPnPProvider::UpnpDevice* dev);
    ConnectionManagerService(const ConnectionManagerService&) = delete;
    void operator=(const ConnectionManagerService&) = delete;
    bool getEventData(bool all, std::vector<std::string>& names,
                      std::vector<std::string>& values) override;
};

#endif  // CONNECTIONMANAGERSERVICE_H
