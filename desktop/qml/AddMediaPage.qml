/* Copyright (C) 2020-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami
import QtQuick.Dialogs 1.1

import org.mkiol.jupii.ContentServer 1.0

Kirigami.ScrollablePage {
    id: root

    property var rightPage: app.rightPage(root)
    property bool openUrlDialog: false
    property alias url: urlDialog.url
    property alias name: urlDialog.name

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    Kirigami.Theme.colorSet: Kirigami.Theme.Window

    title: qsTr("Add")

    Component.onCompleted: {
        if (openUrlDialog) {
            urlDialog.open()
            openUrlDialog = false
        }
    }

    onBackRequested: {
        fileDialog.close()
        urlDialog.close()
    }

    FileDialog {
        id: fileDialog
        property bool asAudio: false
        property bool asVideo: false
        title: qsTr("Choose a file")
        selectMultiple: true
        selectFolder: false
        selectExisting: true
        folder: shortcuts.music
        onAccepted: {
            playlist.addItemFileUrls(fileDialog.fileUrls,
                                     asAudio ? ContentServer.Type_Music :
                                               asVideo ? ContentServer.Type_Video :
                                                         ContentServer.Type_Unknown)
            pageStack.flickBack()
        }
    }

    FileDialog {
        id: folderDialog
        property bool asAudio: false
        property bool asVideo: false
        title: qsTr("Choose a folder")
        selectMultiple: false
        selectFolder: true
        selectExisting: true
        folder: shortcuts.music
        onAccepted: {
            playlist.addItemFileUrls(folderDialog.fileUrls,
                                     asAudio ? ContentServer.Type_Music :
                                               asVideo ? ContentServer.Type_Video : ContentServer.Type_Unknown)
            pageStack.flickBack()
        }
    }

    UrlDialog {
        id: urlDialog
        parent: pageStack.columnView
        anchors.centerIn: parent
        width: parent.width - 8 * Kirigami.Units.largeSpacing
        onAccepted: {
            if (ok) {
                playlist.addItemUrlSkipUrlDialog(url, asAudio ? ContentServer.Type_Music :
                                                   ContentServer.Type_Unknown, name)
            }
            url = ""
            name = ""
            pageStack.flickBack()
        }
    }

    CamDialog {
        id: camDialog
        parent: pageStack.columnView
        anchors.centerIn: parent
        width: parent.width - 8 * Kirigami.Units.largeSpacing
        onAccepted: {
            playlist.addItemUrl("jupii://cam")
            pageStack.flickBack()
        }
    }

    MicDialog {
        id: micDialog
        parent: pageStack.columnView
        anchors.centerIn: parent
        width: parent.width - 8 * Kirigami.Units.largeSpacing
        onAccepted: {
            playlist.addItemUrl("jupii://mic")
            pageStack.flickBack()
        }
    }

    ScreenDialog {
        id: screenDialog
        parent: pageStack.columnView
        anchors.centerIn: parent
        width: parent.width - 8 * Kirigami.Units.largeSpacing
        onAccepted: {
            playlist.addItemUrl("jupii://screen")
            pageStack.flickBack()
        }
    }

    PlaybackDialog {
        id: playbackDialog
        parent: pageStack.columnView
        anchors.centerIn: parent
        width: parent.width - 8 * Kirigami.Units.largeSpacing
        onAccepted: {
            playlist.addItemUrl("jupii://playback")
            pageStack.flickBack()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 0

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Local")
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("File")
            icon: "folder-open"
            highlighted: fileDialog.visible
            onClicked: {
                fileDialog.asAudio = false
                fileDialog.asVideo = false
                fileDialog.open()
            }
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("Folder")
            icon: "folder-open"
            highlighted: folderDialog.visible
            onClicked: {
                folderDialog.asAudio = false
                fileDialog.asVideo = false
                folderDialog.open()
            }

            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            Controls.ToolTip.text: qsTr("Add all files in the folder and subfolders")
            hoverEnabled: true
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("Audio from video")
            icon: "folder-open"
            highlighted: fileDialog.visible
            onClicked: {
                fileDialog.asAudio = true
                fileDialog.asVideo = false
                fileDialog.open()
            }

            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            Controls.ToolTip.text: qsTr("Add audio extracted from a video file.")
            hoverEnabled: true
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("Image as video")
            icon: "folder-open"
            highlighted: fileDialog.visible
            onClicked: {
                folderDialog.asAudio = false
                fileDialog.asVideo = true
                fileDialog.open()
            }

            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            Controls.ToolTip.text: qsTr("Add an image as a video.") + " " +
                                   qsTr("The image will be converted into a low frame rate video stream.") + " " +
                                   qsTr("Use %1 option to set how long the image will displayed.").arg("<i>" + qsTr("Image display time") + "</i>")
            hoverEnabled: true
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: qsTr("Recording")
            icon: "media-record"
            highlighted: rightPage ? rightPage.objectName === "rec" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("RecPage.qml"))
            }

            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            Controls.ToolTip.text: qsTr("Add audio previously recorded using the record button.")
            hoverEnabled: true
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Remote")
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("URL")
            icon: "folder-remote"
            onClicked: urlDialog.open()
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: qsTr("Media Server")
            icon: "network-server"
            highlighted: rightPage ? rightPage.objectName === "cdir" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("UpnpCDirDevicesPage.qml"))
            }
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Capture")
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            visible: !settings.casterDontUsePipeWire || !settings.casterHasPipeWire()
            label: qsTr("Audio capture")
            icon: "player-volume"
            onClicked: playbackDialog.open()
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            visible: settings.isXcb()
            label: qsTr("Screen capture")
            icon: "computer"
            onClicked: screenDialog.open()
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            visible: !settings.casterDontUsePipeWire || !settings.casterHasPipeWire()
            label: qsTr("Microphone")
            icon: "audio-input-microphone"
            onClicked: micDialog.open()
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("Camera")
            icon: "camera-web"
            onClicked: camDialog.open()
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Apps & services")
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: "Bandcamp"
            icon: "qrc:/images/bandcamp.svg"
            highlighted: rightPage ? rightPage.objectName === "bc" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("BcPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: "FOSDEM"
            icon: "qrc:/images/fosdem.svg"
            highlighted: rightPage ? rightPage.objectName === "fosdem" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("FosdemYearsPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            visible: utils.isGpodderAvailable()
            label: "gPodder"
            icon: "qrc:/images/gpodder.svg"
            highlighted: rightPage ? rightPage.objectName === "gpodder" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("GpodderPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            text: "Icecast"
            icon: "qrc:/images/icecast.svg"
            highlighted: rightPage ? rightPage.objectName === "icecast" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("IcecastPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: "radio.net"
            icon: "qrc:/images/radionet.svg"
            highlighted: rightPage ? rightPage.objectName === "radionet" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("RadionetPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: "SomaFM"
            icon: "qrc:/images/somafm.svg"
            highlighted: rightPage ? rightPage.objectName === "somafm" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("SomafmPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: "SoundCloud"
            icon: "qrc:/images/soundcloud.svg"
            highlighted: rightPage ? rightPage.objectName === "soundcloud" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"))
            }
        }

        BasicListItemWithArrow {
            Layout.fillWidth: true
            label: "TuneIn"
            icon: "qrc:/images/tunein.svg"
            highlighted: rightPage ? rightPage.objectName === "tunein" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("TuneinPage.qml"))
            }
        }

        BasicListItemWithArrow {
            visible: settings.isPy()
            Layout.fillWidth: true
            label: "YouTube Music"
            icon: "qrc:/images/youtube.svg"
            highlighted: rightPage ? rightPage.objectName === "yt" : false
            onClicked: {
                pageStack.push(Qt.resolvedUrl("YtPage.qml"))
            }
        }
    }
}
