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

    signal labelClicked
    signal nextClicked
    signal prevClicked
    signal forwardClicked
    signal backwardClicked
    signal togglePlayClicked
    signal repeatClicked

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

    Column {
        id: column

        width: parent.width
        spacing: Theme.paddingMedium

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
                font.pixelSize: Theme.fontSizeHuge
                horizontalAlignment: Text.AlignHCenter
                visible: root.full && av.currentTrackDuration === 0
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
                width: parent.width
                height: Theme.itemSizeLarge + 2 * Theme.paddingMedium

                Image {
                    id: image

                    width: Theme.itemSizeLarge
                    height: Theme.itemSizeLarge

                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                    }

                    source: "image://theme/graphic-grid-playlist"

                    Image {
                        id: _image

                        anchors.fill: parent
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectCrop
                        source: av.currentAlbumArtURI
                    }
                }

                Column {
                    id: lcol

                    spacing: Theme.paddingSmall
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: image.right
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
                        text: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
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
                        text: av.currentAuthor
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

            height: Theme.iconSizeExtraLarge
            spacing: parent.width/4 - size - 2 * Theme.horizontalPageMargin
            anchors.horizontalCenter: parent.horizontalCenter

            IconButton {
                id: prevButton
                width: parent.size; height: parent.size
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://theme/icon-m-previous"
                onClicked: prevClicked()
            }

            IconButton {
                id: backwardButton
                width: parent.size; height: parent.size
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://icons/icon-m-backward"
                onClicked: backwardClicked()
            }

            IconButton {
                id: forwardButton
                width: parent.size; height: parent.size
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://icons/icon-m-forward"
                onClicked: forwardClicked()
            }

            IconButton {
                id: nextButton
                width: parent.size; height: parent.size
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://theme/icon-m-next"
                onClicked: nextClicked()
            }

            IconButton {
                id: playmodeButton
                width: parent.size; height: parent.size
                anchors.verticalCenter: parent.verticalCenter
                icon.source: root.playMode === PlayListModel.PM_RepeatAll ? "image://theme/icon-m-repeat" :
                             root.playMode === PlayListModel.PM_RepeatOne ? "image://theme/icon-m-repeat-single" :
                                                                            "image://icons/icon-m-norepeat"
                onClicked: repeatClicked()
            }
        }
    }
}
