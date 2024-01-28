/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.7
import QtQuick.Controls 2.2 as Controls
import org.kde.kirigami 2.14 as Kirigami

import harbour.jupii.AVTransport 1.0

Controls.RoundButton {
    id: root

    property alias progressEnabled: progressBar.visible
    property int size: Kirigami.Units.iconSizes.huge

    Controls.ToolTip.visible: hovered
    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
    Controls.ToolTip.text: av.transportState !== AVTransport.Playing ?
                      qsTr("Play") : qsTr("Pause")

    activeFocusOnTab: true

    Keys.onReturnPressed: flatButtonWithToolTip.clicked()
    Accessible.onPressAction: flatButtonWithToolTip.clicked()

    flat: true

    width: size
    height: size
    radius: size / 2

    icon.width: Kirigami.Units.iconSizes.smallMedium
    icon.height: Kirigami.Units.iconSizes.smallMedium
    icon.name: av.transportState !== AVTransport.Playing ?
                   "media-playback-start" : "media-playback-pause"

    background: Rectangle {
        implicitWidth: root.size - 8
        implicitHeight: root.size - 8
        border.color: root.hovered ? Kirigami.Theme.hoverColor : "transparent"
        border.width: 1
        opacity: 1
        radius: root.size / 2
        color: root.down ? Kirigami.Theme.hoverColor : "transparent"
    }

    CircularProgressBar {
        id: progressBar

        anchors.centerIn: parent
        size: root.size
        lineWidth: 5
        color: Kirigami.Theme.highlightColor
        animationDuration: 100
        value: av.currentTrackDuration > 0 ?
               av.relativeTimePosition / av.currentTrackDuration : 0
    }
}
