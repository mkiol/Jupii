/* Copyright (C) 2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Column {
    id: root

    property alias label: _slider.label
    property alias description: _descLabel.text
    property alias value: _slider.value
    property alias valueText: _slider.valueText
    property alias minimumValue: _slider.minimumValue
    property alias maximumValue: _slider.maximumValue
    property alias stepSize: _slider.stepSize


    Slider {
        id: _slider

        opacity: enabled ? 1.0 : Theme.opacityLow
        width: root.width
        handleVisible: true
        valueText: value
        stepSize: 1
    }

    Label {
        id: _descLabel

        opacity: enabled ? 1.0 : Theme.opacityLow
        x: _slider.leftMargin - Theme.paddingMedium
        width: Math.max(0, _slider.width - _slider.leftMargin - _slider.rightMargin) + 2 * Theme.paddingMedium
        height: text.length ? (implicitHeight + Theme.paddingMedium) : 0
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: _slider.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
    }
}
