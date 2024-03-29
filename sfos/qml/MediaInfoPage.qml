/* Copyright (C) 2017-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.ContentServer 1.0
import harbour.jupii.Settings 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    property bool imgOk: imagep.status === Image.Ready || imagel.status === Image.Ready
    property int itemType: utils.itemTypeFromUrl(av.currentId)
    property bool isShout: app.streamTitle.length !== 0
    property var urlInfo: cserver.parseUrl(av.currentId)

    onStatusChanged: {
        if (status === PageStatus.Active) {
            settings.disableHint(Settings.Hint_MediaInfoSwipeLeft)
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Image {
            id: imagel
            visible: root.isLandscape
            width: {
               if (root.width/2 > root.height)
                   return root.height
               return root.width/2
            }
            x: root.width - width
            height: {
                if (status !== Image.Ready)
                    return 1

                var w = width
                var ratio = w / implicitWidth
                var h = implicitHeight * ratio
                return h > w ? w : h
            }

            fillMode: Image.PreserveAspectCrop
            source: av.currentAlbumArtURI
        }

        OpacityRampEffect {
            sourceItem: imagel
            direction: OpacityRamp.BottomToTop
            offset: root.imgOk ? 0.5 : -0.5
            slope: 1.8
        }

        Column {
            id: column

            width: root.width

            PullDownMenu {
                visible: itemType === ContentServer.ItemType_LocalFile ||
                         itemType === ContentServer.ItemType_Url

                MenuItem {
                    visible: av.currentYtdl
                    text: qsTr("Open URL")
                    onClicked: Qt.openUrlExternally(av.currentOrigURL)
                }

                MenuItem {
                    text: itemType === ContentServer.ItemType_LocalFile ? qsTr("Copy path") :
                                                                          qsTr("Copy URL")
                    onClicked: Clipboard.text =
                               itemType === ContentServer.ItemType_LocalFile ? av.currentPath :
                                                                               av.currentOrigURL
                }

                MenuItem {
                    text: qsTr("Copy current title")
                    visible: itemType === ContentServer.ItemType_Url && isShout
                    onClicked: Clipboard.text = app.streamTitle
                }
            }

            Item {
                visible: root.isPortrait
                width: parent.width
                height: Math.max(imagep.height, header.height)

                Image {
                    id: imagep
                    width: parent.width
                    height: {
                        if (status !== Image.Ready)
                            return 1

                        var w = width
                        var ratio = w / implicitWidth
                        var h = implicitHeight * ratio
                        return h > w ? w : h
                    }

                    fillMode: Image.PreserveAspectCrop
                    source: av.currentAlbumArtURI
                }

                OpacityRampEffect {
                    sourceItem: imagep
                    direction: OpacityRamp.BottomToTop
                    offset: root.imgOk ? 0.5 : -0.5
                    slope: 1.8
                }

                PageHeader {
                    id: header
                    title: av.currentTitle
                }
            }

            PageHeader {
                visible: root.isLandscape
                title: av.currentTitle
            }

            Spacer {}

            Column {
                width: root.isLandscape && root.imgOk ? root.width/2 : root.width
                anchors.left: parent.left
                spacing: Theme.paddingMedium

                DetailItem {
                    visible: value.length > 0
                    label: qsTr("Item type")
                    value: {
                        switch(itemType) {
                        case ContentServer.ItemType_LocalFile:
                            return qsTr("Local file")
                        case ContentServer.ItemType_Url:
                            return qsTr("URL")
                        case ContentServer.ItemType_Upnp:
                            return qsTr("Media Server")
                        case ContentServer.ItemType_ScreenCapture:
                            return qsTr("Screen Capture")
                        case ContentServer.ItemType_PlaybackCapture:
                            return qsTr("Audio Capture")
                        case ContentServer.ItemType_Mic:
                            return qsTr("Microphone")
                        case ContentServer.ItemType_Cam:
                            return qsTr("Camera")
                        default:
                            return ""
                        }
                    }
                }

                DetailItem {
                    id: videoLabel
                    label: qsTr("Video source")
                    value: urlInfo.videoName
                    visible: value.length > 0 &&
                             itemType === ContentServer.ItemType_Cam
                }

                DetailItem {
                    id: audioLabel
                    label: qsTr("Audio source")
                    value: urlInfo.audioName
                    visible: value.length > 0 &&
                             (itemType === ContentServer.ItemType_Mic ||
                              itemType === ContentServer.ItemType_Cam ||
                              itemType === ContentServer.ItemType_ScreenCapture)
                }

                DetailItem {
                    label: qsTr("Video orientation")
                    value: settings.videoOrientationStr(urlInfo.videoOrientation)

                    visible: itemType === ContentServer.ItemType_Cam ||
                             itemType === ContentServer.ItemType_ScreenCapture
                }

                DetailItem {
                    label: isShout ? qsTr("Station name") : qsTr("Title")
                    value: av.currentTitle
                    visible: value.length !== 0 &&
                             itemType !== ContentServer.ItemType_Mic &&
                             itemType !== ContentServer.ItemType_PlaybackCapture &&
                             itemType !== ContentServer.ItemType_ScreenCapture &&
                             itemType !== ContentServer.ItemType_Cam
                }

                DetailItem {
                    label: itemType === ContentServer.ItemType_PlaybackCapture ||
                           itemType === ContentServer.ItemType_ScreenCapture ?
                               qsTr("Captured application") : qsTr("Current title")
                    value: app.streamTitle.length === 0 &&
                           (itemType === ContentServer.ItemType_PlaybackCapture ||
                            itemType === ContentServer.ItemType_ScreenCapture) ?
                               qsTr("None") : app.streamTitle
                    visible: (itemType === ContentServer.ItemType_PlaybackCapture ||
                             itemType === ContentServer.ItemType_ScreenCapture ||
                             itemType === ContentServer.ItemType_Url) &&
                             value.length !== 0 && av.currentType !== AVTransport.T_Image
                }

                DetailItem {
                    label: qsTr("Author")
                    value: av.currentAuthor
                    visible: itemType !== ContentServer.ItemType_Mic &&
                             itemType !== ContentServer.ItemType_Cam &&
                             itemType !== ContentServer.ItemType_PlaybackCapture &&
                             itemType !== ContentServer.ItemType_ScreenCapture &&
                             av.currentType !== AVTransport.T_Image &&
                             value.length !== 0
                }

                DetailItem {
                    label: qsTr("Album")
                    value: av.currentAlbum
                    visible: itemType !== ContentServer.ItemType_Mic &&
                             itemType !== ContentServer.ItemType_Cam &&
                             itemType !== ContentServer.ItemType_PlaybackCapture &&
                             itemType !== ContentServer.ItemType_ScreenCapture &&
                             av.currentType !== AVTransport.T_Image &&
                             value.length !== 0
                }

                DetailItem {
                    label: qsTr("Duration")
                    value: utils.secToStr(av.currentTrackDuration)
                    visible: av.currentType !== AVTransport.T_Image &&
                             av.currentTrackDuration !== 0 &&
                             itemType !== ContentServer.ItemType_Mic &&
                             itemType !== ContentServer.ItemType_Cam &&
                             itemType !== ContentServer.ItemType_PlaybackCapture &&
                             itemType !== ContentServer.ItemType_ScreenCapture
                }

                DetailItem {
                    label: qsTr("Recording date")
                    value: av.currentRecDate
                    visible: value.length !== 0
                }

                DetailItem {
                    label: qsTr("Media Server")
                    value: utils.devNameFromUpnpId(av.currentId)
                    visible: itemType === ContentServer.ItemType_Upnp &&
                             value.length !== 0
                }

                DetailItem {
                    label: qsTr("Content type")
                    value: av.currentContentType
                    visible: value.length > 0
                }

                DetailItem {
                    label: qsTr("Live")
                    value: playlist.live ? qsTr("Yes") : qsTr("No")
                    visible: playlist.active && (itemType === ContentServer.ItemType_Mic ||
                                                 itemType === ContentServer.ItemType_Cam ||
                                                 itemType === ContentServer.ItemType_PlaybackCapture ||
                                                 itemType === ContentServer.ItemType_ScreenCapture ||
                                                 itemType === ContentServer.ItemType_Url)

                }

                DetailItem {
                    label: qsTr("Cached")
                    value: cserver.idCached(av.currentId) ? qsTr("Yes") : qsTr("No")
                    visible: settings.isDebug() && itemType == ContentServer.ItemType_Url

                }
            }

            Column {
                width: root.isLandscape && root.imgOk ? imagel.x : root.width
                anchors.left: parent.left

                SectionHeader {
                    text: qsTr("Description")
                    visible: itemType !== ContentServer.ItemType_Mic &&
                             itemType !== ContentServer.ItemType_PlaybackCapture &&
                             itemType !== ContentServer.ItemType_ScreenCapture &&
                             itemType !== ContentServer.ItemType_Cam &&
                             av.currentDescription.length !== 0
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: av.currentDescription
                    visible: av.currentDescription.length !== 0
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }

                SectionHeader {
                    text: av.currentPath.length !== 0 ? qsTr("Path") : qsTr("URL")
                    visible: itemType === ContentServer.ItemType_LocalFile ||
                             itemType === ContentServer.ItemType_Url
                }

                PaddedLabel {
                    text: av.currentPath.length !== 0 ? av.currentPath : av.currentOrigURL
                    visible: itemType === ContentServer.ItemType_LocalFile ||
                             itemType === ContentServer.ItemType_Url
                }

                SectionHeader {
                    text: qsTr("Volume boost")
                    visible: mic.visible || playback.visible
                }

                CasterSourceVolume {
                    id: mic
                    visible: urlInfo.audioName.length !== 0 &&
                             (itemType === ContentServer.ItemType_Mic ||
                             itemType === ContentServer.ItemType_Cam)
                    volume: settings.casterMicVolume
                    onVolumeChanged: settings.casterMicVolume = volume
                }

                CasterSourceVolume {
                    id: playback
                    visible: urlInfo.audioName.length !== 0 &&
                             (itemType === ContentServer.ItemType_ScreenCapture ||
                             itemType === ContentServer.ItemType_PlaybackCapture)
                    volume: settings.casterPlaybackVolume
                    onVolumeChanged: settings.casterPlaybackVolume = volume
                }

                SectionHeader {
                    text: qsTr("Tracks history")
                    visible: app.streamTitleHistory.length > 0
                }

                Repeater {
                    id: _titles_repeater
                    visible: app.streamTitleHistory.length > 0
                    model: app.streamTitleHistory
                    width: parent.width
                    delegate: Row {
                        layoutDirection: Qt.LeftToRight
                        x: Theme.horizontalPageMargin
                        height: Math.max(_note_label.height, _title_label.height)
                        spacing: Theme.paddingSmall
                        Label {
                            id: _note_label
                            color: modelData === app.streamTitle ? Theme.highlightColor : "transparent"
                            font.pixelSize: Theme.fontSizeSmall
                            text: "🎵"
                        }
                        Label {
                            id: _title_label
                            width: _titles_repeater.width - 2*Theme.horizontalPageMargin -
                                   _note_label.width - Theme.paddingSmall
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.NoWrap
                            truncationMode: TruncationMode.Fade
                            text: modelData
                        }
                    }
                }
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
