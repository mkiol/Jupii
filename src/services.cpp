/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "services.h"

#include "avtransport.h"
#include "contentdirectory.h"
#include "renderingcontrol.h"

Services::Services(QObject* parent)
    : QObject{parent},
      renderingControl{std::make_shared<RenderingControl>(parent)},
      avTransport{std::make_shared<AVTransport>(parent)},
      contentDir{std::make_shared<ContentDirectory>(parent)} {}
