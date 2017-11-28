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

DockedPanel {
    id: root

    property var av
    property var rc

    property alias nextEnabled: nextButton.enabled
    property alias prevEnabled: prevButton.enabled

    property alias trackPositionSlider: trackPositionSlider
    property bool imgOk: image.status === Image.Ready

    signal labelClicked
    signal nextClicked
    signal prevClicked
    signal forwardClicked
    signal backwardClicked
    signal togglePlayClicked

    width: parent.width
    height: column.height + Theme.paddingLarge
    dock: Dock.Bottom

    Column {
        id: column

        width: parent.width
        spacing: Theme.paddingMedium

        Column {
            width: parent.width

            Slider {
                id: trackPositionSlider

                property bool blockValueChangedSignal: false

                width: parent.width
                minimumValue: 0
                maximumValue: av.currentTrackDuration
                stepSize: 1
                handleVisible: av.seekSupported
                value: av.relativeTimePosition
                enabled: av.seekSupported
                valueText: utils.secToStr(value > 0 ? value : 0)

                onValueChanged: {
                    if (!blockValueChangedSignal)
                        av.seek(value)
                }

                function updateValue(_value) {
                    blockValueChangedSignal = true
                    value = _value
                    blockValueChangedSignal = false
                }
            }

            ListItem {
                id: lbi

                width: parent.width
                contentHeight: Theme.itemSizeLarge + 2 * Theme.paddingMedium

                onClicked: root.labelClicked()

                Image {
                    id: image

                    width: Theme.itemSizeLarge
                    height: Theme.itemSizeLarge
                    sourceSize.width: width
                    sourceSize.height: height
                    fillMode: Image.PreserveAspectCrop
                    visible: root.imgOk
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                    }

                    source: av.currentAlbumArtURI
                }

                Column {
                    id: lcol

                    spacing: Theme.paddingSmall
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: root.imgOk ? image.right : parent.left
                        leftMargin: root.imgOk ? Theme.paddingLarge : Theme.horizontalPageMargin
                        right: parent.right
                        rightMargin: Theme.horizontalPageMargin
                    }

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        color: lbi.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                        elide: root.imgOk ? Text.ElideNone : Text.ElideRight
                        horizontalAlignment: root.imgOk || truncated ? Text.AlignLeft : Text.AlignHCenter
                        text: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
                    }

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        color: lbi.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                        elide: root.imgOk ? Text.ElideNone : Text.ElideRight
                        horizontalAlignment: root.imgOk || truncated ? Text.AlignLeft : Text.AlignHCenter
                        opacity: text.length > 0 ? 1.0 : 0.0
                        visible: opacity > 0.0
                        Behavior on opacity { FadeAnimation {} }

                        text: av.currentAuthor
                    }
                }
            }
        }

        Row {
            height: Theme.iconSizeExtraLarge
            spacing: Theme.paddingLarge
            anchors.horizontalCenter: parent.horizontalCenter

            IconButton {
                id: prevButton
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://theme/icon-m-previous"
                onClicked: prevClicked()
            }

            IconButton {
                id: backwardButton
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://icons/icon-m-backward"
                enabled: av.seekSupported && av.transportState === AVTransport.Playing
                onClicked: backwardClicked()
            }

            IconButton {
                id: playButton
                anchors.verticalCenter: parent.verticalCenter
                icon.source: av.transportState !== AVTransport.Playing ? "image://theme/icon-l-play" : "image://theme/icon-l-pause"
                enabled: (av.playSupported && av.transportState !== AVTransport.Playing) ||
                         (av.pauseSupported && av.transportState === AVTransport.Playing) ||
                         (av.stopSupported && av.transportState === AVTransport.Stopped)
                onClicked: togglePlayClicked()
            }

            IconButton {
                id: forwardButton
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://icons/icon-m-forward"
                enabled: av.seekSupported && av.transportState === AVTransport.Playing
                onClicked: forwardClicked()
            }

            IconButton {
                id: nextButton
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://theme/icon-m-next"
                onClicked: nextClicked()
            }
        }
    }
}
