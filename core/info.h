/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
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
static constexpr const char* APP_VERSION = "2.7.3 (debug)";
#else
static constexpr const char* APP_VERSION = "2.7.3";
#endif
static constexpr const char* AUTHOR = "Michal Kosciesza";
static constexpr const char* COPYRIGHT_YEAR = "2018-2020";
static constexpr const char* SUPPORT_EMAIL = "jupii@mkiol.net";
static constexpr const char* PAGE = "https://github.com/mkiol/Jupii";
static constexpr const char* LICENSE = "Mozilla Public License 2.0";
static constexpr const char* LICENSE_URL = "http://mozilla.org/MPL/2.0/";
}

#endif // INFO_H
