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

    Image {
        id: bg
        anchors.fill: parent
        source: root.image
        fillMode: Image.PreserveAspectCrop
    }

    OpacityRampEffect {
        sourceItem: bg
        direction: OpacityRamp.BottomToTop
        offset: 0.4
        slope: 2
    }

    Image {
        anchors.fill: parent
        source: "image://icons/icon-c-cover?" + Theme.primaryColor
        opacity: bg.status === Image.Ready ? 0.5 : 1.0
    }

    Column {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            leftMargin: Theme.paddingSmall
            rightMargin: Theme.paddingSmall
            topMargin: Theme.paddingMedium
        }
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            color: Theme.primaryColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            text: av.controlable ? root.title : APP_NAME
            font.pixelSize: av.controlable ?
                                Theme.fontSizeMedium :
                                Theme.fontSizeLarge
        }

        Label {
            width: parent.width
            color: Theme.secondaryColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            text: root.author
            visible: av.controlable
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
