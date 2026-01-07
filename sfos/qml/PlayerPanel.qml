/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
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
    property int playMode: PlayListModel.PM_Normal
    property bool nextEnabled: false
    property bool prevEnabled: false
    property bool forwardEnabled: false
    property bool backwardEnabled: false
    property bool playmodeEnabled: false
    property bool recordEnabled: false
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
        if (open) full = false
    }

    onRequestCompact: {
        full = false
    }

    onRequestFull: {
        full = true
    }

    function update() {
        if (rc.inited) volumeSlider.updateValue(rc.volume)
        else volumeSlider.updateValue(0)
    }

    Connections {
        target: rc
        onVolumeChanged: update()
        onInitedChanged: update()
    }

    Handle {
        id: handle
        width: Theme.itemSizeLarge
        enabled: true
        highlighted: root.pressed || root.dragging
    }

    Column {
        id: column

        width: parent.width
        y: trackSlider.visible ? -Theme.paddingSmall : 0
        spacing: root.isPortrait ? Theme.paddingSmall : 0

        Column {
            width: parent.width

            TrackSlider {
                id: trackSlider
                visible: root.isPortrait && root.full && av.currentTrackDuration > 0 && !playlist.live
                enabled: av.controlable && av.seekSupported
                width: parent.width
            }

            Item {
                id: lbi
                width: parent.width
                height: Theme.itemSizeLarge

                Image {
                    id: image
                    visible: _image.status !== Image.Ready
                    width: Theme.itemSizeLarge
                    height: Theme.itemSizeLarge
                    anchors.left: parent.left
                    source: utils.noresIcon("icon-itemart")
                }

                Image {
                    id: _image
                    anchors.fill: image
                    sourceSize: Qt.size(Theme.itemSizeLarge, Theme.itemSizeLarge)
                    source: av.currentAlbumArtURI
                }

                Column {
                    width: Theme.iconSizeSmall
                    anchors.right: _image.right
                    anchors.bottom: _image.bottom

                    AttachedIcon {
                        source: {
                            switch(root.itemType) {
                            case ContentServer.ItemType_Url:
                                return "image://icons/icon-s-browser?" + Theme.primaryColor
                            case ContentServer.ItemType_Upnp:
                                return "image://icons/icon-s-device?" + Theme.primaryColor
                            case ContentServer.ItemType_Slides:
                                return "image://icons/icon-s-slidesitem?" + Theme.primaryColor
                            }
                            return ""
                        }
                    }

                    AttachedIcon {
                        source: {
                            switch (av.currentType) {
                            case AVTransport.T_Image:
                                return "image://theme/icon-m-file-image?" + Theme.primaryColor
                            case AVTransport.T_Audio:
                                return "image://theme/icon-m-file-audio?" + Theme.primaryColor
                            case AVTransport.T_Video:
                                return "image://theme/icon-m-file-video?" + Theme.primaryColor
                            default:
                                return "image://theme/icon-m-file-other?" + Theme.primaryColor
                            }
                        }
                    }
                }

                Column {
                    id: lcol

                    spacing: Theme.paddingSmall
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: _image.status !== Image.Ready ? image.right : _image.right
                        leftMargin: Theme.paddingLarge
                        right: playerButtons.visible ? playerButtons.left : playButton.visible ? playButton.left : parent.right
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

                PlayerButtons {
                    id: playerButtons

                    visible: root.full && root.isLandscape
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: playButton.left
                        rightMargin: Theme.paddingMedium
                    }

                    prevEnabled: root.prevEnabled
                    backwardEnabled: root.backwardEnabled
                    forwardEnabled: root.forwardEnabled
                    nextEnabled: root.nextEnabled
                    recordEnabled: root.recordEnabled
                    playMode: root.playMode

                    onPrevClicked: root.prevClicked()
                    onBackwardClicked: root.backwardClicked()
                    onForwardClicked: root.forwardClicked()
                    onNextClicked: root.nextClicked()
                    onRecordClicked: root.recordClicked()
                    onRepeatClicked: root.repeatClicked()
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
                    enabled: av.controlable
                    onClicked: togglePlayClicked()

                    ProgressCircle {
                        anchors.fill: parent
                        value: av.currentTrackDuration > 0 ?
                                   av.relativeTimePosition / av.currentTrackDuration :
                                   0
                        progressColor: Theme.highlightColor
                        backgroundColor: "transparent"
                        enabled: av.controlable && !root.full && av.currentTrackDuration > 0 && !playlist.live
                        opacity: enabled ? 0.4 : 0.0
                        visible: opacity > 0.0
                    }
                }
            }
        }

        PlayerButtons {
            visible: root.full && root.isPortrait
            anchors.horizontalCenter: parent.horizontalCenter

            prevEnabled: root.prevEnabled
            backwardEnabled: root.backwardEnabled
            forwardEnabled: root.forwardEnabled
            nextEnabled: root.nextEnabled
            recordEnabled: root.recordEnabled
            playMode: root.playMode

            onPrevClicked: root.prevClicked()
            onBackwardClicked: root.backwardClicked()
            onForwardClicked: root.forwardClicked()
            onNextClicked: root.nextClicked()
            onRecordClicked: root.recordClicked()
            onRepeatClicked: root.repeatClicked()
        }

        Item {
            visible: root.full && rc.inited && !rc.busy
            width: parent.width
            height: volumeSlider.height -Theme.paddingMedium

            TrackSlider {
                id: trackSliderL
                y: -Theme.paddingLarge
                x: Theme.itemSizeExtraSmall
                visible: root.isLandscape && av.currentTrackDuration > 0
                enabled: av.controlable && av.seekSupported
                width: parent.width * 0.60
            }

            Slider {
                id: volumeSlider
                y: -Theme.paddingLarge

                property bool blockValueChangedSignal: false

                anchors {
                    left: trackSliderL.visible ? trackSliderL.right : parent.left
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
