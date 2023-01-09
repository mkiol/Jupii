/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0

Slider {
    id: root

    property bool _block: false

    minimumValue: 0
    maximumValue: av.currentTrackDuration
    stepSize: 1
    handleVisible: av.seekSupported
    valueText: utils.secToStr(value > 0 ? value : 0)

    onValueChanged: {
        if (!_block) av.seek(value)
    }

    Component.onCompleted: updateValue(av.relativeTimePosition)

    Connections {
        target: av
        onRelativeTimePositionChanged: root.updateValue(av.relativeTimePosition)
    }

    function updateValue(_value) {
        _block = true
        value = _value
        _block = false
    }
}
