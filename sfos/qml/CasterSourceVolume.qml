/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Slider {
    id: root

    property double volume

    opacity: enabled ? 1.0 : Theme.opacityLow

    width: parent.width
    minimumValue: -50
    maximumValue: 50
    stepSize: 1
    handleVisible: true
    value: volume
    valueText: root.volume
    onValueChanged: root.volume = value
}
