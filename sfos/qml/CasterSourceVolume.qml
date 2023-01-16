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
    minimumValue: 0
    maximumValue: 10
    stepSize: 0.1
    handleVisible: true
    value: volume
    valueText: {
        var v = round(root.volume).toString()
        return v.indexOf(".") === -1 ? (v + ".0") : v
    }

    onValueChanged: {
        root.volume = round(value)
    }

    function round(num) {
        var exp = Math.pow(10, 1);
        return Math.round(num * exp) / exp
    }
}
