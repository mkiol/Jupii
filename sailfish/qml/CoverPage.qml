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
    property string title: ""
    property string author: ""

    property bool controlable: playable || stopable

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
                root.title = Qt.binding(function() {
                    return app.player.title
                })
                root.author = Qt.binding(function() {
                    return app.player.author
                })
            }
        } else if (status === Cover.Deactivating) {
            root.playable = false
            root.stopable = false
            root.image = ""
            root.title = ""
            root.author = ""
        }
    }

    Rectangle {
        anchors.fill: parent
        opacity: 0.3
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#8200dd" }
            GradientStop { position: 1.0; color: "#5a00f7" }
        }
    }

    Image {
        id: bg
        anchors.fill: parent
        source: root.image
        opacity: 0.5
        fillMode: Image.PreserveAspectCrop
    }

    Item {
        anchors.fill: parent
        width: Theme.iconSizeLarge
        height: Theme.iconSizeLarge

        Image {
            id: image
            y: Theme.paddingLarge
            anchors.horizontalCenter: parent.horizontalCenter
            opacity: 0.4
            source: "image://icons/icon-a-jupii"
            visible: bg.status !== Image.Ready
        }

        Column {
            anchors.centerIn: parent
            spacing: Theme.paddingSmall
            width: parent.width - (screen.sizeCategory > Screen.Medium
                                   ? 2*Theme.paddingMedium : 2*Theme.paddingLarge)

            Label {
                width: parent.width
                color: Theme.primaryColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
                text: root.controlable ? root.title : APP_NAME
            }

            Label {
                width: parent.width
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
                //fontSizeMode: Text.Fit
                text: root.author
                visible: root.controlable
            }
        }
    }

    CoverActionList {
        enabled: root.controlable
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
