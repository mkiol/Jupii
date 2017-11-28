/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: root

    property var musicPickerPage
    property var videoPickerPage
    property var imagePickerPage
    property var filePickerPage

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }

            PageHeader {
                title: qsTr("Add item")
            }

            SimpleListItem {
                name: qsTr("Music")
                onClicked: {
                    pageStack.replace(musicPickerPage)
                }
            }

            SimpleListItem {
                name: qsTr("Video")
                onClicked: {
                    pageStack.replace(videoPickerPage)
                }
            }

            SimpleListItem {
                name: qsTr("Image")
                visible: settings.imageSupported
                onClicked: {
                    pageStack.replace(imagePickerPage)
                }
            }

            SimpleListItem {
                name: qsTr("File")
                onClicked: {
                    pageStack.replace(filePickerPage)
                }
            }
        }
    }

    VerticalScrollDecorator {
        flickable: flick
    }
}
