/* Copyright (C) 2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

Loader {
    id: root

    property int hintType: -1 // Settings::HintType
    property string text: ""
    property string subtext: ""
    property bool closeOnClick: true

    anchors.fill: parent ? parent : undefined
    sourceComponent: hintType >= 0 && settings.hintEnabled(hintType) ? cmp : null
    enabled: status === Loader.Ready

    Component {
        id: cmp

        InteractionHintLabel_ {
            anchors.bottom: parent ? parent.bottom : undefined
            text: root.text
            subtext: root.subtext
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (root.closeOnClick) {
                        settings.disableHint(root.hintType)
                        root.sourceComponent = null
                    }
                }
            }
        }
    }
}
