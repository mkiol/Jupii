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
static const char* APP_VERSION = "1.9.3";
#elif DESKTOP
static const char* APP_VERSION = "1.9.3";
#else
static const char* APP_VERSION = "0.0.0";
#endif
static const char* AUTHOR = "Michal Kosciesza";
static const char* COPYRIGHT_YEAR = "2018";
static const char* SUPPORT_EMAIL = "jupii@mkiol.net";
static const char* PAGE = "https://github.com/mkiol/Jupii";
static const char* LICENSE = "Mozilla Public License 2.0";
static const char* LICENSE_URL = "http://mozilla.org/MPL/2.0/";
}

#endif // INFO_H
