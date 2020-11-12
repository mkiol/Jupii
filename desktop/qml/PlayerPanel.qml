/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.7
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami
import QtQuick.Dialogs 1.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.PlayListModel 1.0
import harbour.jupii.ContentServer 1.0

Rectangle {
    id: root

    property bool full: true
    property bool open: false
    property bool inited: false

    property bool controlable: true
    property int playMode: PlayListModel.PM_Normal
    property alias nextEnabled: nextButton.enabled
    property alias prevEnabled: prevButton.enabled
    property alias forwardEnabled: forwardButton.enabled
    property alias backwardEnabled: backwardButton.enabled
    property alias playmodeEnabled: playmodeButton.enabled
    property alias recordEnabled: recordButton.enabled
    property bool recordActive: false
    property int itemType
    readonly property bool narrowMode: root.width < Kirigami.Units.gridUnit * 25

    property string title: ""
    property string subtitle: ""

    signal nextClicked
    signal prevClicked
    signal forwardClicked
    signal backwardClicked
    signal togglePlayClicked
    signal repeatClicked
    signal recordClicked
    signal clicked
    signal iconClicked

    Kirigami.Theme.colorSet: Kirigami.Theme.Button

    /*opacity: open || !inited ? 1.0 : 0.0
    visible: opacity > 0
    height: visible ? column.height : 0
    Behavior on opacity {
        NumberAnimation {
            duration: Kirigami.Units.shortDuration; easing.type: Easing.OutQuad
        }
    }*/
    visible: open || !inited
    height: visible ? column.height : 0

    color: root.inited ? Kirigami.Theme.buttonBackgroundColor : "transparent"

    onOpenChanged: {
        if (open)
            full = true
    }

    Kirigami.Separator {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.top
        }
        visible: root.inited
    }

    MouseArea {
        enabled: root.inited
        hoverEnabled: true
        anchors.fill: column

        onClicked: {
            root.clicked()
        }

        Rectangle {
            x: Kirigami.Units.smallSpacing
            y: Kirigami.Units.smallSpacing
            width: parent.width - 2 * Kirigami.Units.smallSpacing
            height: parent.height - 2 * Kirigami.Units.smallSpacing
            border.width: 1
            border.color: Kirigami.Theme.hoverColor
            color: "transparent"
            radius: 3
            visible: parent.containsMouse
        }
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Separator {}

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: !root.inited && !root.open
            icon.source: "network-offline"
            text: qsTr("Not connected") + ". " + qsTr("Connect to a device to control playback using %1.").arg(APP_NAME)
            + (settings.contentDirSupported ? " " + qsTr("Without connection, all items in play queue are still accessible on other devices in your local network.") : "")
            actions: [
                Kirigami.Action {
                    text: qsTr("Devices")
                    icon.name: "computer"
                    onTriggered: app.devicesAction.trigger()
                }
            ]
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            visible: root.full && root.inited

            TextMetrics {
                id: durationTextMetrics
                text: "0:00:00"
            }

            LabelWithToolTip {
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: Kirigami.Units.smallSpacing
                Layout.preferredWidth: (durationTextMetrics.boundingRect.width -
                                        durationTextMetrics.boundingRect.x) +
                                       Kirigami.Units.smallSpacing

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight

                text: utils.secToStr(av.relativeTimePosition)
            }

            SeekSlider {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true

                enabled: av.seekSupported && root.controlable
                from: 0
                to: av.currentTrackDuration
                value: av.relativeTimePosition
                weelStep: 10

                onSeekRequested: {
                    av.seek(seekValue)
                }
            }

            LabelWithToolTip {
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.preferredWidth: durationTextMetrics.boundingRect.width -
                                       durationTextMetrics.boundingRect.x
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft

                visible: av.currentTrackDuration > 0

                text: utils.secToStr(av.currentTrackDuration)
            }
        }

        RowLayout {
            visible: root.inited
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing

            MouseArea {
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                width: Kirigami.Units.iconSizes.huge
                height: Kirigami.Units.iconSizes.huge
                enabled: root.inited
                hoverEnabled: true

                onClicked: root.iconClicked()

                ImageWithFallback {
                    anchors.fill: parent
                    sourceSize.width: Kirigami.Units.iconSizes.huge
                    sourceSize.height: Kirigami.Units.iconSizes.huge
                    fillMode: Image.PreserveAspectCrop

                    source: {
                        if (itemType === ContentServer.ItemType_Mic)
                            return "image://icon/audio-input-microphone"
                        else if (itemType === ContentServer.ItemType_AudioCapture)
                            return "image://icon/player-volume"
                        else if (itemType === ContentServer.ItemType_ScreenCapture)
                            return "image://icon/computer"
                        return av.currentAlbumArtURI
                    }
                    fallback: {
                        switch (av.currentType) {
                        case AVTransport.T_Image:
                            return "image://icon/image-x-generic"
                        case AVTransport.T_Audio:
                            return "image://icon/audio-x-generic"
                        case AVTransport.T_Video:
                            return "image://icon/video-x-generic"
                        default:
                            return "image://icon/view-media-album-cover"
                        }
                    }

                    Item { // attached icon
                        visible: itemType === ContentServer.ItemType_Url ||
                                 itemType === ContentServer.ItemType_Upnp
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: Kirigami.Units.iconSizes.smallMedium
                        height: Kirigami.Units.iconSizes.smallMedium
                        Rectangle {
                            anchors.fill: parent
                            color: Kirigami.Theme.backgroundColor
                            opacity: 0.7
                        }
                        Kirigami.Icon {
                            anchors.fill: parent
                            source: itemType === ContentServer.ItemType_Url ?
                                        "folder-remote" :
                                    itemType === ContentServer.ItemType_Upnp ?
                                        "network-server" : ""
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    border.width: 1
                    border.color: Kirigami.Theme.hoverColor
                    color: "transparent"
                    radius: 3
                    visible: parent.containsMouse
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                spacing: Kirigami.Units.largeSpacing

                Controls.Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    text: root.title
                }

                Controls.Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    visible: text.length > 0
                    text: root.subtitle
                }
            }

            PlayButton {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: Kirigami.Units.largeSpacing

                enabled: root.controlable
                progressEnabled: !root.full
                onClicked: togglePlayClicked()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.alignment: Qt.AlignCenter | Qt.AlignVCenter

            visible: root.full && root.inited

            spacing: Kirigami.Units.largeSpacing

            FlatButtonWithToolTip {
                id: prevButton

                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                icon.name: "media-skip-backward"
                text: qsTr("Skip Backward")
                onClicked: prevClicked()
            }

            FlatButtonWithToolTip {
                id: backwardButton

                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                icon.name: "media-seek-backward"
                text: qsTr("Seek Backward")
                onClicked: backwardClicked()
            }

            FlatButtonWithToolTip {
                id: forwardButton

                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                icon.name: "media-seek-forward"
                text: qsTr("Seek Forward")
                onClicked: forwardClicked()
            }

            FlatButtonWithToolTip {
                id: nextButton

                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                icon.name: "media-skip-forward"
                text: qsTr("Skip Forward")
                onClicked: nextClicked()
            }

            FlatButtonWithToolTip {
                id: recordButton

                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                checkable: true
                checked: recordActive
                icon.name: "media-record"
                text: qsTr("Toggle Record")
                onClicked: recordClicked()
                visible: enabled
            }

            FlatButtonWithToolTip {
                id: playmodeButton

                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                icon.name:  root.playMode === PlayListModel.PM_RepeatAll ? "media-playlist-repeat" :
                            root.playMode === PlayListModel.PM_RepeatOne ? "media-playlist-repeat" :
                            "media-playlist-normal"

                text: qsTr("Toggle Repeat One")
                onClicked: repeatClicked()

                Controls.Label {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Kirigami.Units.smallSpacing
                    anchors.right: parent.right
                    anchors.rightMargin: Kirigami.Units.smallSpacing
                    visible: root.playMode === PlayListModel.PM_RepeatOne
                    font: Kirigami.Theme.smallFont
                    text: "1"
                }
            }

            SeekSlider {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                visible: !root.narrowMode

                enabled: rc.inited && !rc.busy && !rc.mute
                from: 0
                to: 100
                value: rc.volume
                stepSize: settings.volStep
                weelStep: settings.volStep

                onSeekRequested: {
                    rc.volume = seekValue
                }
            }

            FlatButtonWithToolTip {
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                visible: !root.narrowMode

                enabled: rc.inited && !rc.busy
                checkable: true
                checked: rc.mute
                icon.name: "audio-volume-muted"
                text: qsTr("Toggle Mute")
                onClicked: rc.setMute(!rc.mute)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

            visible: root.full && root.inited && root.narrowMode

            spacing: Kirigami.Units.largeSpacing

            SeekSlider {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true

                enabled: rc.inited && !rc.busy && !rc.mute
                from: 0
                to: 100
                value: rc.volume
                stepSize: settings.volStep
                weelStep: settings.volStep

                onSeekRequested: {
                    rc.volume = seekValue
                }
            }

            FlatButtonWithToolTip {
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                enabled: rc.inited && !rc.busy
                checkable: true
                checked: rc.mute
                icon.name: "audio-volume-muted"
                text: qsTr("Toggle Mute")
                onClicked: rc.setMute(!rc.mute)
            }
        }

        Kirigami.Separator {}
    }
}
