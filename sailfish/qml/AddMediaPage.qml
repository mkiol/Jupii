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

    property var musicPickerDialog
    property var videoPickerDialog
    property var audioFromVideoPickerDialog
    property var imagePickerDialog
    property var albumPickerPage
    property var artistPickerPage
    property var playlistPickerPage
    property var filePickerPage
    property var urlPickerPage
    property var somafmPickerPage
    property var gpodderPickerPage

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
                title.text: qsTr("Music")
                icon.source: "image://theme/icon-m-file-audio?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(musicPickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("Album")
                icon.source: "image://theme/icon-m-media-albums?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(albumPickerPage)
                }
            }

            SimpleListItem {
                title.text: qsTr("Artist")
                icon.source: "image://theme/icon-m-media-artists?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(artistPickerPage)
                }
            }

            SimpleListItem {
                title.text: qsTr("Playlist")
                icon.source: "image://theme/icon-m-media-playlists?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(playlistPickerPage)
                }
            }

            SimpleListItem {
                title.text: qsTr("Audio from Video")
                icon.source: "image://theme/icon-m-file-audio?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(audioFromVideoPickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("Video")
                icon.source: "image://theme/icon-m-file-video?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(videoPickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("Image")
                icon.source: "image://theme/icon-m-file-image?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                visible: settings.imageSupported
                onClicked: {
                    pageStack.replace(imagePickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("File")
                icon.source: "image://theme/icon-m-file-other?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(filePickerPage)
                }
            }

            SimpleListItem {
                title.text: qsTr("URL")
                icon.source: "image://icons/icon-m-browser?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.replace(urlPickerPage)
                }
            }

            SectionHeader {
                text: qsTr("Apps & services")
            }

            SimpleListItem {
                visible: utils.isGpodderAvailable();
                title.text: qsTr("gPodder")
                icon.source: "image://icons/icon-m-gpodder"
                onClicked: {
                    pageStack.replace(gpodderPickerPage)
                }
            }

            SimpleListItem {
                title.text: qsTr("SomaFM")
                icon.source: "somafm.jpg"
                onClicked: {
                    pageStack.replace(somafmPickerPage)
                }
            }
        }
    }

    VerticalScrollDecorator {
        flickable: flick
    }
}
