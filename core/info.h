/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INFO_H
#define INFO_H

namespace Jupii {
static const char* APP_NAME = "Jupii";
#ifdef SAILFISH
static const char* APP_VERSION = "2.6.0 (alpha)";
#elif DESKTOP
static const char* APP_VERSION = "2.6.0 (alpha)";
#endif
static const char* AUTHOR = "Michal Kosciesza";
static const char* COPYRIGHT_YEAR = "2018-2020";
static const char* SUPPORT_EMAIL = "jupii@mkiol.net";
static const char* PAGE = "https://github.com/mkiol/Jupii";
static const char* LICENSE = "Mozilla Public License 2.0";
static const char* LICENSE_URL = "http://mozilla.org/MPL/2.0/";
}

#endif // INFO_H
