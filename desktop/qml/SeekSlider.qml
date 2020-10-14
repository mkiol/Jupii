/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.7
import QtQuick.Controls 2.2 as Controls
import org.kde.kirigami 2.14 as Kirigami

MouseArea {
    id: root

    property alias from: slider.from
    property alias to: slider.to
    property alias value: slider.value
    property alias stepSize: slider.stepSize
    property int weelStep: 10

    signal seekRequested(int seekValue)

    height: 2 * Kirigami.Units.gridUnit
    acceptedButtons: Qt.NoButton

    onWheel: {
        if (wheel.angleDelta.y > 0) {
            seekRequested(value + weelStep)
        } else {
            seekRequested(value - weelStep)
        }
    }

    Controls.Slider {
        id: slider

        property bool seekStarted: false
        property int seekValue

        anchors.fill: parent
        enabled: root.enabled
        live: true
        onValueChanged: {
            if (seekStarted) {
                seekValue = value
            }
        }

        onPressedChanged: {
            if (pressed) {
                seekStarted = true
                seekValue = value
            } else {
                root.seekRequested(seekValue)
                seekStarted = false
            }
        }

        background: Rectangle {
            anchors.fill: parent
            implicitWidth: root.width
            implicitHeight: root.height
            color: "transparent"

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: slider.handle.x + radius
                height: 6
                color: Kirigami.Theme.highlightColor
                radius: 3
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: slider.handle.x
                width: slider.availableWidth - slider.handle.x - radius
                height: 6
                color: Kirigami.Theme.backgroundColor
                radius: 3
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: Kirigami.Theme.backgroundColor
            border.color: slider.hovered ?
                              Kirigami.Theme.highlightColor :
                              Kirigami.Theme.disabledTextColor
            border.width: 1
        }
    }
}
