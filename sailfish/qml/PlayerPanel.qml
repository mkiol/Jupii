/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.PlayListModel 1.0
import harbour.jupii.ContentServer 1.0

DockedPanel_ {
    id: root

    property bool full: false
    property bool controlable: true
    property int playMode: PlayListModel.PM_Normal
    property alias nextEnabled: nextButton.enabled
    property alias prevEnabled: prevButton.enabled
    property alias forwardEnabled: forwardButton.enabled
    property alias backwardEnabled: backwardButton.enabled
    property alias playmodeEnabled: playmodeButton.enabled
    property alias recordEnabled: recordButton.enabled
    property bool recordActive: false
    property bool isPortrait: pageStack.currentPage.isPortrait
    property bool isLandscape: pageStack.currentPage.isLandscape
    property int itemType

    property string title: ""
    property string subtitle: ""

    signal labelClicked
    signal nextClicked
    signal prevClicked
    signal forwardClicked
    signal backwardClicked
    signal togglePlayClicked
    signal repeatClicked
    signal recordClicked

    width: parent.width
    height: column.height
    animationDuration: 200

    dock: Dock.Bottom

    onOpenChanged: {
        if (open)
            full = false
    }

    onRequestCompact: {
        full = false
    }

    onRequestFull: {
        full = true
    }

    function update() {
        if (rc.inited)
            volumeSlider.updateValue(rc.volume)
        else
            volumeSlider.updateValue(0)
    }

    Connections {
        target: rc
        onVolumeChanged: update()
        onInitedChanged: update()
    }

    Column {
        id: column

        width: parent.width

        Column {
            width: parent.width

            Label {
                // Time position when track duration is unknown
                anchors {
                    left: parent.left
                    right: parent.right
                    bottomMargin: Theme.paddingMedium + Theme.paddingSmall
                    topMargin: Theme.paddingMedium + Theme.paddingSmall
                }
                font.pixelSize: root.isPortrait ? Theme.fontSizeHuge : Theme.fontSizeExtraLarge
                horizontalAlignment: Text.AlignHCenter
                visible: av.currentType !== AVTransport.T_Image &&
                         root.full &&
                         av.currentTrackDuration === 0
                text: utils.secToStr(av.relativeTimePosition)
            }

            Slider {
                id: trackPositionSlider

                visible: root.full && av.currentTrackDuration > 0

                property bool blockValueChangedSignal: false

                width: parent.width
                minimumValue: 0
                maximumValue: av.currentTrackDuration
                stepSize: 1
                handleVisible: av.seekSupported
                enabled: av.seekSupported && root.controlable
                valueText: utils.secToStr(value > 0 ? value : 0)

                onValueChanged: {
                    if (!blockValueChangedSignal)
                        av.seek(value)
                }

                Component.onCompleted: {
                    trackPositionSlider.updateValue(av.relativeTimePosition)
                }

                Connections {
                    target: av
                    onRelativeTimePositionChanged: {
                        trackPositionSlider.updateValue(av.relativeTimePosition)
                    }
                }

                function updateValue(_value) {
                    blockValueChangedSignal = true
                    value = _value
                    blockValueChangedSignal = false
                }
            }

            Item {
                id: lbi
                property int imgSize: root.isPortrait ? Theme.itemSizeLarge : 0.8 * Theme.itemSizeLarge
                width: parent.width
                height: imgSize + 2 *
                        (root.full ? (root.isPortrait ? Theme.paddingMedium : 0) : Theme.paddingMedium)

                Image {
                    id: image
                    visible: _image.status !== Image.Ready
                    width: lbi.imgSize
                    height: lbi.imgSize
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                    }
                    source: "image://theme/graphic-grid-playlist?" + Theme.primaryColor
                }

                Image {
                    id: _image
                    width: lbi.imgSize
                    height: lbi.imgSize
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                    }
                    sourceSize.width: lbi.imgSize
                    sourceSize.height: lbi.imgSize
                    fillMode: Image.PreserveAspectCrop
                    source: av.currentAlbumArtURI
                }

                Rectangle {
                    visible: _aicon.status === Image.Ready
                    anchors.right: _image.right
                    anchors.bottom: _image.bottom
                    width: Theme.iconSizeSmall
                    height: Theme.iconSizeSmall
                    color: Theme.rgba((Theme.colorScheme === Theme.LightOnDark ?
                             Theme.darkPrimaryColor : Theme.lightPrimaryColor), 0.5)
                    Image {
                        id: _aicon
                        anchors.fill: parent
                        source: itemType === ContentServer.ItemType_Url ?
                                    ("image://icons/icon-s-browser?" + Theme.primaryColor) :
                                itemType === ContentServer.ItemType_Upnp ?
                                    ("image://icons/icon-s-device?" + Theme.primaryColor) : ""
                    }
                }

                Image {
                    anchors {
                        right: _image.right
                        bottom: _image.bottom
                    }
                    source: itemType === ContentServer.ItemType_Url ?
                                ("image://icons/icon-s-browser?" + Theme.primaryColor) :
                            itemType === ContentServer.ItemType_Upnp ?
                                ("image://icons/icon-s-device?" + Theme.primaryColor) : ""
                }

                Column {
                    id: lcol

                    spacing: Theme.paddingSmall
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: _image.status !== Image.Ready ? image.right : _image.right
                        leftMargin: Theme.paddingLarge
                        right: playButton.visible ? playButton.left : parent.right
                        rightMargin: Theme.paddingLarge
                    }

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        color: lbi.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                        elide: Text.ElideNone
                        horizontalAlignment: Text.AlignLeft
                        text: root.title
                    }

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        color: lbi.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                        elide: Text.ElideNone
                        horizontalAlignment: Text.AlignLeft
                        opacity: text.length > 0 ? 1.0 : 0.0
                        visible: opacity > 0.0
                        text: root.subtitle
                    }
                }

                IconButton {
                    id: playButton

                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: Theme.horizontalPageMargin
                    }

                    icon.source: av.transportState !== AVTransport.Playing ?
                                     "image://theme/icon-l-play" :
                                     "image://theme/icon-l-pause"
                    enabled: root.controlable
                    onClicked: togglePlayClicked()

                    ProgressCircle {
                        anchors.fill: parent
                        value: av.currentTrackDuration > 0 ?
                                   av.relativeTimePosition / av.currentTrackDuration :
                                   0
                        progressColor: Theme.highlightColor
                        backgroundColor: "transparent"
                        enabled: root.controlable && !root.full && av.currentTrackDuration > 0
                        opacity: enabled ? 0.4 : 0.0
                        visible: opacity > 0.0
                    }
                }
            }
        }

        Row {
            visible: root.full

            property real size: Theme.itemSizeSmall

            height: size + (root.isPortrait ? Theme.paddingMedium : Theme.paddingSmall)
            spacing: {
                var count = 0
                if (prevButton.visible)
                    count++
                if (backwardButton.visible)
                    count++
                if (forwardButton.visible)
                    count++
                if (nextButton.visible)
                    count++
                if (recordButton.visible)
                    count++
                if (playmodeButton.visible)
                    count++
                return count > 1 ? (parent.width - count * size - 2 * Theme.horizontalPageMargin)/(count-1) : 0
            }
            anchors.horizontalCenter: parent.horizontalCenter

            IconButton {
                id: prevButton
                width: parent.size; height: parent.size
                anchors.bottom: parent.bottom
                icon.source: "image://theme/icon-m-previous"
                onClicked: prevClicked()
            }

            IconButton {
                id: backwardButton
                width: parent.size; height: parent.size
                anchors.bottom: parent.bottom
                icon.source: "image://icons/icon-m-backward"
                onClicked: backwardClicked()
            }

            IconButton {
                id: forwardButton
                width: parent.size; height: parent.size
                anchors.bottom: parent.bottom
                icon.source: "image://icons/icon-m-forward"
                onClicked: forwardClicked()
            }

            IconButton {
                id: nextButton
                width: parent.size; height: parent.size
                anchors.bottom: parent.bottom
                icon.source: "image://theme/icon-m-next"
                onClicked: nextClicked()
            }

            IconButton {
                id: recordButton
                width: parent.size; height: parent.size
                anchors.bottom: parent.bottom
                icon.source: recordActive ? "image://icons/icon-m-record-active" : "image://icons/icon-m-record"
                onClicked: recordClicked()
                visible: enabled
            }

            IconButton {
                id: playmodeButton
                width: parent.size; height: parent.size
                anchors.bottom: parent.bottom
                icon.source: root.playMode === PlayListModel.PM_RepeatAll ? "image://theme/icon-m-repeat" :
                             root.playMode === PlayListModel.PM_RepeatOne ? "image://theme/icon-m-repeat-single" :
                                                                            "image://icons/icon-m-norepeat"
                onClicked: repeatClicked()
            }
        }

        Item {
            visible: root.full && rc.inited && !rc.busy
            width: parent.width
            height: volumeSlider.height + Theme.paddingSmall

            Slider {
                id: volumeSlider

                property bool blockValueChangedSignal: false

                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    rightMargin: muteButt.width/1.5
                    right: parent.right
                }

                minimumValue: 0
                maximumValue: 100
                stepSize: settings.volStep
                valueText: value
                opacity: rc.mute ? 0.7 : 1.0

                onValueChanged: {
                    if (!blockValueChangedSignal) {
                        rc.volume = value
                        rc.setMute(false)
                    }
                }

                function updateValue(_value) {
                    blockValueChangedSignal = true
                    value = _value
                    blockValueChangedSignal = false
                }
            }

            IconButton {
                id: muteButt
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                }
                icon.source: rc.mute ? "image://theme/icon-m-speaker-mute":
                                       "image://theme/icon-m-speaker"
                onClicked: rc.setMute(!rc.mute)
            }
        }
    }
}
