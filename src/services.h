/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SERVICES_H
#define SERVICES_H

#include <QObject>
#include <memory>

#include "singleton.h"

class RenderingControl;
class AVTransport;
class ContentDirectory;

class Services : public QObject, public Singleton<Services> {
    Q_OBJECT
   public:
    Services(QObject* parent = nullptr);
    std::shared_ptr<RenderingControl> renderingControl;
    std::shared_ptr<AVTransport> avTransport;
    std::shared_ptr<ContentDirectory> contentDir;
};

#endif  // SERVICES_H
