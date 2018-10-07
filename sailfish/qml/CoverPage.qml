/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0

CoverBackground {
    id: root

    property string image: av.transportStatus === AVTransport.TPS_Ok ?
                               av.currentAlbumArtURI : ""
    //property string title: av.currentTitle.length > 0 ? av.currentTitle : qsTr("Unknown")
    //property string author: av.currentAuthor.length > 0 ? av.currentAuthor : ""
    property string title: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
    property string author: app.streamTitle.length === 0 ? av.currentAuthor : app.streamTitle

    function togglePlay() {
        if (av.transportState !== AVTransport.Playing) {
            av.speed = 1
            av.play()
        } else {
            if (av.pauseSupported)
                av.pause()
            else
                av.stop()
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
                text: av.controlable ? root.title : APP_NAME
            }

            Label {
                width: parent.width
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
                //fontSizeMode: Text.Fit
                text: root.author
                visible: av.controlable
            }
        }
    }

    CoverActionList {
        enabled: av.controlable
        iconBackground: bg.status === Image.Ready

        CoverAction {
            iconSource: av.playable ? "image://theme/icon-cover-play" :
                                       "image://theme/icon-cover-pause"
            onTriggered: root.togglePlay();
        }
    }
}
