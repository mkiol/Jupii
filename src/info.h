/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INFO_H
#define INFO_H

namespace Jupii {
static constexpr const char* APP_NAME = "Jupii";
#ifdef QT_DEBUG
static constexpr const char* APP_VERSION = "2.13.4 (debug)";
#else
#ifdef HARBOUR
static constexpr const char* APP_VERSION = "2.13.4 (harbour)";
#else
static constexpr const char* APP_VERSION = "2.13.4";
#endif  // HARBOUR
#endif  // QT_DEBUG
#ifdef FLATPAK
static constexpr const char* APP_ID = "org.mkiol.Jupii";
static constexpr const char* APP_BINARY_ID = "jupii";
#else
#ifdef SAILFISH
static constexpr const char* APP_ID = "jupii";
static constexpr const char* APP_BINARY_ID = "harbour-jupii";
#else
static constexpr const char* APP_ID = "jupii";
static constexpr const char* APP_BINARY_ID = "jupii";
#endif
#endif  // FLATPAK
static constexpr const char* ORG = "org.mkiol";
static constexpr const char* AUTHOR = "Michal Kosciesza";
static constexpr const char* AUTHOR_EMAIL = "michal@mkiol.net";
static constexpr const char* COPYRIGHT_YEAR = "2018-2022";
static constexpr const char* SUPPORT_EMAIL = "jupii@mkiol.net";
static constexpr const char* PAGE = "https://github.com/mkiol/Jupii";
static constexpr const char* LICENSE = "Mozilla Public License 2.0";
static constexpr const char* LICENSE_URL = "http://mozilla.org/MPL/2.0/";
static constexpr const char* LICENSE_SPDX = "MPL-2.0";
static constexpr const char* DBUS_SERVICE = "org.mkiol.jupii";
static constexpr const char* DBUS_INTERFACE = "org.mkiol.jupii";
}  // namespace Jupii

#endif  // INFO_H
