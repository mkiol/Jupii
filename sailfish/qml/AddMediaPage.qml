/* Copyright (C) 2017-2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    Component {
        id: filePickerPage
        FilePickerPage {
            allowedOrientations: Orientation.All
            nameFilters: cserver.getExtensions(263)
            onSelectedContentPropertiesChanged: {
                playlist.addItemPath(selectedContentProperties.filePath)
                app.popToQueue()
            }
        }
    }

    Component {
        id: musicPickerDialog
        MultiMusicPickerDialog {
            allowedOrientations: Orientation.All
            onDone: {
                if (result === DialogResult.Accepted) {
                    var paths = [];
                    for (var i = 0; i < selectedContent.count; ++i)
                        paths.push(selectedContent.get(i).filePath)
                    playlist.addItemPaths(paths)
                    app.popToQueue()
                }
            }
        }
    }

    Component {
        id: videoPickerDialog
        MultiVideoPickerDialog {
            allowedOrientations: Orientation.All
            onDone: {
                if (result === DialogResult.Accepted) {
                    var paths = [];
                    for (var i = 0; i < selectedContent.count; ++i)
                        paths.push(selectedContent.get(i).filePath)
                    playlist.addItemPaths(paths)
                    app.popToQueue()
                }
            }
        }
    }

    Component {
        id: audioFromVideoPickerDialog
        MultiVideoPickerDialog {
            allowedOrientations: Orientation.All
            onDone: {
                if (result === DialogResult.Accepted) {
                    var paths = [];
                    for (var i = 0; i < selectedContent.count; ++i)
                        paths.push(selectedContent.get(i).filePath)
                    playlist.addItemPathsAsAudio(paths)
                    app.popToQueue()
                }
            }
        }
    }

    Component {
        id: imagePickerDialog
        MultiImagePickerDialog {
            allowedOrientations: Orientation.All
            onDone: {
                if (result === DialogResult.Accepted) {
                    var paths = [];
                    for (var i = 0; i < selectedContent.count; ++i)
                        paths.push(selectedContent.get(i).filePath)
                    playlist.addItemPaths(paths)
                    app.popToQueue()
                }
            }
        }
    }

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

            SectionHeader {
                text: qsTr("Local")
            }

            SimpleListItem {
                title.text: qsTr("Music")
                icon.source: "image://theme/icon-m-file-audio?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(musicPickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("Album")
                icon.source: "image://theme/icon-m-media-albums?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("AlbumsPage.qml"));
                }
            }

            SimpleListItem {
                title.text: qsTr("Artist")
                icon.source: "image://theme/icon-m-media-artists?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("ArtistPage.qml"));
                }
            }

            SimpleListItem {
                title.text: qsTr("Playlist")
                icon.source: "image://theme/icon-m-media-playlists?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("PlaylistPage.qml"));
                }
            }

            SimpleListItem {
                title.text: qsTr("Audio from Video")
                icon.source: "image://theme/icon-m-file-audio?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(audioFromVideoPickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("Video")
                icon.source: "image://theme/icon-m-file-video?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(videoPickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("Image")
                icon.source: "image://theme/icon-m-file-image?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(imagePickerDialog)
                }
            }

            SimpleListItem {
                title.text: qsTr("File")
                icon.source: "image://theme/icon-m-file-other?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(filePickerPage)
                }
            }

            SimpleListItem {
                visible: settings.rec
                title.text: qsTr("Recordings")
                icon.source: "image://icons/icon-m-record?"  + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("RecPage.qml"));
                }
            }

            SectionHeader {
                text: qsTr("Remote")
            }

            SimpleListItem {
                title.text: qsTr("URL")
                icon.source: "image://icons/icon-m-browser?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("AddUrlPage.qml"));
                }
            }

            SimpleListItem {
                title.text: qsTr("Media Server")
                icon.source: "image://icons/icon-m-device?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("UpnpCDirDevicesPage.qml"));
                }
            }

            SectionHeader {
                text: qsTr("Capture")
            }

            SimpleListItem {
                title.text: qsTr("Audio capture")
                icon.source: "image://theme/icon-m-speaker?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)

                onClicked: {
                    playlist.addItemUrl("jupii://pulse")
                    app.popToQueue()
                }
            }

            SimpleListItem {
                visible: settings.screenSupported
                title.text: qsTr("Screen capture")
                icon.source: "image://theme/icon-m-display?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)

                onClicked: {
                    playlist.addItemUrl("jupii://screen")
                    app.popToQueue()
                }
            }

            SimpleListItem {
                title.text: qsTr("Microphone")
                icon.source: "image://theme/icon-m-mic?" + (highlighted ?
                             Theme.highlightColor : Theme.primaryColor)

                onClicked: {
                    playlist.addItemUrl("jupii://mic")
                    app.popToQueue()
                }
            }

            SectionHeader {
                text: qsTr("Apps & services")
            }

            SimpleListItem {
                title.text: "Bandcamp"
                icon.source: "image://icons/icon-m-bandcamp"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("BcPage.qml"));
                }
            }

            SimpleListItem {
                title.text: "FOSDEM"
                icon.source: "image://icons/icon-m-fosdem"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("FosdemYearsPage.qml"));
                }
            }

            SimpleListItem {
                visible: utils.isGpodderAvailable()
                title.text: "gPodder"
                icon.source: "image://icons/icon-m-gpodder"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("GpodderEpisodesPage.qml"));
                }
            }

            SimpleListItem {
                title.text: "Icecast"
                icon.source: "image://icons/icon-m-icecast"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("IcecastPage.qml"));
                }
            }

            SimpleListItem {
                title.text: "SomaFM"
                icon.source: "image://icons/icon-m-somafm"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("SomafmPage.qml"));
                }
            }

            SimpleListItem {
                title.text: "SoundCloud"
                icon.source: "image://icons/icon-m-soundcloud"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"));
                }
            }

            SimpleListItem {
                title.text: "TuneIn"
                icon.source: "image://icons/icon-m-tunein"
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("TuneinPage.qml"));
                }
            }
        }
    }

    VerticalScrollDecorator {
        flickable: flick
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
