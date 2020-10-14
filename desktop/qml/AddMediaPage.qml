/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
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

Kirigami.ScrollablePage {
    id: root

    property var rightPage: app.getRightPage(root)

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    Kirigami.Theme.colorSet: Kirigami.Theme.Window

    title: qsTr("Add items")

    onBackRequested: {
        fileDialog.close()
        urlDialog.close()
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a file")
        selectMultiple: true
        selectFolder: false
        selectExisting: true
        folder: shortcuts.music
        onAccepted: {
            playlist.addItemFileUrls(fileDialog.fileUrls)
            pageStack.flickBack()
        }
    }

    AddUrlDialog {
        id: urlDialog
        parent: pageStack.columnView
        anchors.centerIn: parent
        width: parent.width - 8 * Kirigami.Units.largeSpacing
        onAccepted: {
            if (ok)
                playlist.addItemUrl(url, name)
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
                fileDialog.open()
            }
        }

        DoubleListItem {
            Layout.fillWidth: true
            visible: settings.rec
            label: qsTr("Recordings")
            iconSource: "media-record"
            highlighted: rightPage ? rightPage.objectName === "rec" : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("RecPage.qml"))
            }
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
            onClicked: {
                urlDialog.open()
            }
        }

        DoubleListItem {
            Layout.fillWidth: true
            label: qsTr("Media Server")
            iconSource: "network-server"
            next: true
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
            label: qsTr("Audio capture")
            icon: "player-volume"
            onClicked: {
                playlist.addItemUrl("jupii://pulse")
                pageStack.flickBack()
            }
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            visible: settings.screenSupported
            label: qsTr("Screen capture")
            icon: "computer"
            onClicked: {
                playlist.addItemUrl("jupii://screen")
                pageStack.flickBack()
            }
        }

        Kirigami.BasicListItem {
            Layout.fillWidth: true
            label: qsTr("Microphone")
            icon: "audio-input-microphone"
            onClicked: {
                playlist.addItemUrl("jupii://mic")
                pageStack.flickBack()
            }
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Apps & services")
        }

        DoubleListItem {
            Layout.fillWidth: true
            label: "Bandcamp"
            iconSource: "qrc:/images/bandcamp.svg"
            highlighted: rightPage ? rightPage.objectName === "bc" : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("BcPage.qml"))
            }
        }

        DoubleListItem {
            Layout.fillWidth: true
            label: "FOSDEM"
            iconSource: "qrc:/images/fosdem.svg"
            highlighted: rightPage ? rightPage.objectName === "fosdem" : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("FosdemYearsPage.qml"))
            }
        }

        DoubleListItem {
            Layout.fillWidth: true
            visible: utils.isGpodderAvailable()
            label: "gPodder"
            iconSource: "qrc:/images/gpodder.svg"
            highlighted: rightPage ? rightPage.objectName === "gpodder" : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("GpodderPage.qml"))
            }
        }

        DoubleListItem {
            Layout.fillWidth: true
            text: "Icecast"
            iconSource: "qrc:/images/icecast.svg"
            highlighted: rightPage ? rightPage.objectName === "icecast" : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("IcecastPage.qml"))
            }
        }

        DoubleListItem {
            Layout.fillWidth: true
            label: "SomaFM"
            iconSource: "qrc:/images/somafm.svg"
            highlighted: rightPage ? rightPage.objectName === "somafm" : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("SomafmPage.qml"))
            }
        }
    }
}
