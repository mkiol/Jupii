/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    id: root

    property bool playable: false
    property bool stopable: false
    property string image: ""
    property string label: ""

    onStatusChanged: {
        if (status === Cover.Activating) {
            if (app.player) {
                root.playable = Qt.binding(function() {
                    return app.player.playable
                })
                root.stopable = Qt.binding(function() {
                    return app.player.stopable
                })
                root.image = Qt.binding(function() {
                    return app.player.image
                })
                root.label = Qt.binding(function() {
                    return app.player.label
                })
            }
        } else if (status === Cover.Deactivating) {
            root.playable = false
            root.stopable = false
            root.image = ""
            root.label = ""
        }
    }

    Image {
        id: bg
        anchors.fill: parent
        source: root.image
        opacity: 0.5
        fillMode: Image.PreserveAspectCrop
    }

    CoverPlaceholder {
        width: Theme.iconSizeLarge
        height: Theme.iconSizeLarge
        icon.source: "image://icons/icon-a-jupii"
        icon.visible: bg.status !== Image.Ready
        text: root.label === "" ? APP_NAME : root.label
    }

    CoverActionList {
        enabled: root.playable || root.stopable
        iconBackground: bg.status === Image.Ready

        CoverAction {
            iconSource: root.playable ? "image://theme/icon-cover-play" :
                                       "image://theme/icon-cover-pause"
            onTriggered: {
                if (app.player)
                    app.player.togglePlay()
            }
        }
    }
}
